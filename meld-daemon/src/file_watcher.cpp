#include "meld/daemon/file_watcher.hpp"

#include <algorithm>

namespace meld::daemon {

FileWatcher::FileWatcher(const std::filesystem::path& root,
                         std::chrono::milliseconds debounce)
    : root_(root), debounce_(debounce) {}

FileWatcher::~FileWatcher() {
    stop();
}

void FileWatcher::start(ChangeCallback callback) {
    if (watching_) return;
    callback_ = std::move(callback);
    watching_ = true;
    watch_thread_ = std::thread([this] { watch_loop(); });
}

void FileWatcher::stop() {
    if (!watching_) return;
    watching_ = false;
    if (watch_thread_.joinable()) {
        watch_thread_.join();
    }
}

void FileWatcher::inject_event(FileChangeEvent event) {
    if (!should_watch(event.path)) return;
    std::lock_guard lock(pending_mutex_);
    pending_events_.push_back(std::move(event));
    last_event_time_ = std::chrono::steady_clock::now();
}

bool FileWatcher::should_watch(const std::filesystem::path& path) {
    auto ext = path.extension().string();
    auto filename = path.filename().string();
    return ext == ".meld" || filename == "meld.toml";
}

void FileWatcher::watch_loop() {
    // Platform-specific implementation:
    // - macOS: FSEvents via FSEventStreamCreate
    // - Linux: inotify via inotify_init1 + inotify_add_watch
    //
    // For now, we use a polling fallback that checks the pending_events_
    // queue (populated via inject_event for testing, or by platform hooks).

    while (watching_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        auto now = std::chrono::steady_clock::now();
        bool should_flush = false;

        {
            std::lock_guard lock(pending_mutex_);
            if (!pending_events_.empty()) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - last_event_time_);
                if (elapsed >= debounce_) {
                    should_flush = true;
                }
            }
        }

        if (should_flush) {
            flush_pending();
        }
    }

    // Flush any remaining events on shutdown
    flush_pending();
}

void FileWatcher::flush_pending() {
    std::vector<FileChangeEvent> events;
    {
        std::lock_guard lock(pending_mutex_);
        if (pending_events_.empty()) return;
        events = std::move(pending_events_);
        pending_events_.clear();
    }

    // Deduplicate: keep only the last event per path
    std::unordered_map<std::string, FileChangeEvent> deduped;
    for (auto& ev : events) {
        deduped[ev.path.string()] = std::move(ev);
    }

    std::vector<FileChangeEvent> final_events;
    final_events.reserve(deduped.size());
    for (auto& [_, ev] : deduped) {
        final_events.push_back(std::move(ev));
    }

    if (callback_ && !final_events.empty()) {
        callback_(final_events);
    }
}

}  // namespace meld::daemon
