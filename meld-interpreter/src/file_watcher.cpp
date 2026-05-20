#include <meld/interpreter/file_watcher.hpp>

#include <chrono>
#include <thread>

// ─── Platform headers ───────────────────────────────────────────────
#if defined(__linux__)
#  include <sys/inotify.h>
#  include <unistd.h>
#  include <poll.h>
#  include <cerrno>
#  include <cstring>
#elif defined(__APPLE__)
#  include <sys/event.h>
#  include <sys/time.h>
#  include <fcntl.h>
#  include <unistd.h>
#elif defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

namespace meld::interpreter {

// ─── Construction / destruction ─────────────────────────────────────

FileWatcher::FileWatcher(std::chrono::milliseconds debounce)
    : debounce_(debounce) {}

FileWatcher::~FileWatcher() {
    stop();
}

// ─── Public API ─────────────────────────────────────────────────────

bool FileWatcher::watch(const std::filesystem::path& path, ChangeCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (running_) {
        return false;  // Already watching
    }

    if (!std::filesystem::exists(path)) {
        return false;
    }

    watched_path_ = std::filesystem::canonical(path);
    callback_ = std::move(callback);
    last_callback_time_ = std::chrono::steady_clock::time_point{};

    // Record initial modification time (used by polling fallback)
    try {
        last_write_time_ = std::filesystem::last_write_time(watched_path_);
    } catch (...) {
        return false;
    }

    running_ = true;
    thread_ = std::make_unique<std::thread>(&FileWatcher::watch_loop, this);
    return true;
}

void FileWatcher::stop() {
    running_ = false;

    if (thread_ && thread_->joinable()) {
        thread_->join();
        thread_.reset();
    }

    platform_cleanup();
}

bool FileWatcher::is_active() const {
    return running_;
}

// ─── Watch loop ─────────────────────────────────────────────────────

void FileWatcher::watch_loop() {
    bool use_native = platform_init();

    while (running_) {
        bool changed = false;

        if (use_native) {
            changed = platform_poll_once();
        } else {
            changed = poll_fallback();
        }

        if (changed && running_) {
            auto now = std::chrono::steady_clock::now();
            bool should_fire = false;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (now - last_callback_time_ >= debounce_) {
                    last_callback_time_ = now;
                    should_fire = true;
                }
            }
            if (should_fire && callback_) {
                callback_(watched_path_);
            }
        }
    }
}

// ─── Polling fallback ───────────────────────────────────────────────

bool FileWatcher::poll_fallback() {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    try {
        if (!std::filesystem::exists(watched_path_)) {
            // File deleted — keep watching for reappearance
            return false;
        }
        auto current = std::filesystem::last_write_time(watched_path_);
        if (current != last_write_time_) {
            last_write_time_ = current;
            return true;
        }
    } catch (...) {
        // Transient error (e.g. file being rewritten) — ignore
    }
    return false;
}

// ═══════════════════════════════════════════════════════════════════
// Platform-specific implementations
// ═══════════════════════════════════════════════════════════════════

#if defined(__linux__)
// ─── Linux: inotify ─────────────────────────────────────────────────

bool FileWatcher::platform_init() {
    platform_fd_ = inotify_init1(IN_NONBLOCK);
    if (platform_fd_ < 0) {
        return false;
    }

    // Watch the parent directory so we also catch delete+recreate
    auto parent = watched_path_.parent_path();
    watch_fd_ = inotify_add_watch(platform_fd_, parent.c_str(),
                                  IN_MODIFY | IN_CREATE | IN_MOVED_TO | IN_CLOSE_WRITE);
    if (watch_fd_ < 0) {
        ::close(platform_fd_);
        platform_fd_ = -1;
        return false;
    }
    return true;
}

void FileWatcher::platform_cleanup() {
    if (watch_fd_ >= 0) {
        inotify_rm_watch(platform_fd_, watch_fd_);
        watch_fd_ = -1;
    }
    if (platform_fd_ >= 0) {
        ::close(platform_fd_);
        platform_fd_ = -1;
    }
}

bool FileWatcher::platform_poll_once() {
    struct pollfd pfd{};
    pfd.fd = platform_fd_;
    pfd.events = POLLIN;

    // Wait up to 200 ms so we can check running_ periodically
    int ret = ::poll(&pfd, 1, 200);
    if (ret <= 0) {
        return false;
    }

    // Drain the inotify buffer
    alignas(struct inotify_event) char buf[4096];
    bool detected = false;

    while (true) {
        auto len = ::read(platform_fd_, buf, sizeof(buf));
        if (len <= 0) break;

        for (char* ptr = buf; ptr < buf + len; ) {
            auto* event = reinterpret_cast<struct inotify_event*>(ptr);
            if (event->len > 0) {
                std::string name(event->name);
                if (name == watched_path_.filename().string()) {
                    detected = true;
                }
            } else {
                // Event on the watched file itself
                detected = true;
            }
            ptr += sizeof(struct inotify_event) + event->len;
        }
    }
    return detected;
}

#elif defined(__APPLE__)
// ─── macOS: kqueue ──────────────────────────────────────────────────

bool FileWatcher::platform_init() {
    platform_fd_ = kqueue();
    if (platform_fd_ < 0) {
        return false;
    }

    watch_fd_ = ::open(watched_path_.c_str(), O_RDONLY);
    if (watch_fd_ < 0) {
        ::close(platform_fd_);
        platform_fd_ = -1;
        return false;
    }

    struct kevent change{};
    EV_SET(&change, watch_fd_, EVFILT_VNODE,
           EV_ADD | EV_ENABLE | EV_CLEAR,
           NOTE_WRITE | NOTE_DELETE | NOTE_RENAME | NOTE_ATTRIB,
           0, nullptr);

    if (kevent(platform_fd_, &change, 1, nullptr, 0, nullptr) < 0) {
        ::close(watch_fd_);
        ::close(platform_fd_);
        watch_fd_ = -1;
        platform_fd_ = -1;
        return false;
    }
    return true;
}

void FileWatcher::platform_cleanup() {
    if (watch_fd_ >= 0) {
        ::close(watch_fd_);
        watch_fd_ = -1;
    }
    if (platform_fd_ >= 0) {
        ::close(platform_fd_);
        platform_fd_ = -1;
    }
}

bool FileWatcher::platform_poll_once() {
    struct kevent event{};
    struct timespec timeout{};
    timeout.tv_sec = 0;
    timeout.tv_nsec = 200'000'000;  // 200 ms

    int n = kevent(platform_fd_, nullptr, 0, &event, 1, &timeout);
    if (n <= 0) {
        return false;
    }

    // If the file was deleted or renamed, try to re-open it
    if (event.fflags & (NOTE_DELETE | NOTE_RENAME)) {
        ::close(watch_fd_);
        watch_fd_ = -1;

        // Wait briefly for the file to reappear (editor save pattern)
        for (int i = 0; i < 10 && running_; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            if (std::filesystem::exists(watched_path_)) {
                watch_fd_ = ::open(watched_path_.c_str(), O_RDONLY);
                if (watch_fd_ >= 0) {
                    struct kevent change{};
                    EV_SET(&change, watch_fd_, EVFILT_VNODE,
                           EV_ADD | EV_ENABLE | EV_CLEAR,
                           NOTE_WRITE | NOTE_DELETE | NOTE_RENAME | NOTE_ATTRIB,
                           0, nullptr);
                    kevent(platform_fd_, &change, 1, nullptr, 0, nullptr);
                    return true;
                }
            }
        }
        return false;
    }

    return true;  // NOTE_WRITE or NOTE_ATTRIB
}

#elif defined(_WIN32)
// ─── Windows: ReadDirectoryChangesW ─────────────────────────────────

bool FileWatcher::platform_init() {
    auto parent = watched_path_.parent_path();
    HANDLE h = CreateFileW(
        parent.wstring().c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        nullptr);

    if (h == INVALID_HANDLE_VALUE) {
        return false;
    }
    // Store the HANDLE as an integer (platform_fd_ is int, but on Windows
    // we need the full HANDLE).  For a real production build you'd use a
    // void* member; here we cast for simplicity.
    platform_fd_ = static_cast<int>(reinterpret_cast<intptr_t>(h));
    return true;
}

void FileWatcher::platform_cleanup() {
    if (platform_fd_ != -1) {
        CloseHandle(reinterpret_cast<HANDLE>(static_cast<intptr_t>(platform_fd_)));
        platform_fd_ = -1;
    }
}

bool FileWatcher::platform_poll_once() {
    HANDLE h = reinterpret_cast<HANDLE>(static_cast<intptr_t>(platform_fd_));
    alignas(DWORD) char buf[4096];
    DWORD bytes_returned = 0;
    OVERLAPPED overlapped{};
    overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

    BOOL ok = ReadDirectoryChangesW(
        h, buf, sizeof(buf), FALSE,
        FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME,
        &bytes_returned, &overlapped, nullptr);

    if (!ok) {
        CloseHandle(overlapped.hEvent);
        return false;
    }

    // Wait up to 200 ms
    DWORD wait = WaitForSingleObject(overlapped.hEvent, 200);
    if (wait != WAIT_OBJECT_0) {
        CancelIo(h);
        CloseHandle(overlapped.hEvent);
        return false;
    }

    GetOverlappedResult(h, &overlapped, &bytes_returned, FALSE);
    CloseHandle(overlapped.hEvent);

    if (bytes_returned == 0) {
        return false;
    }

    // Scan notifications for our target filename
    auto* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buf);
    auto target_name = watched_path_.filename().wstring();
    bool detected = false;

    while (true) {
        std::wstring name(info->FileName, info->FileNameLength / sizeof(WCHAR));
        if (name == target_name) {
            detected = true;
            break;
        }
        if (info->NextEntryOffset == 0) break;
        info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(
            reinterpret_cast<char*>(info) + info->NextEntryOffset);
    }
    return detected;
}

#else
// ─── Unsupported platform: polling only ─────────────────────────────

bool FileWatcher::platform_init() {
    return false;  // Fall back to polling
}

void FileWatcher::platform_cleanup() {
    // Nothing to clean up
}

bool FileWatcher::platform_poll_once() {
    return poll_fallback();
}

#endif

} // namespace meld::interpreter
