#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace meld::interpreter {

/// Platform-aware file watcher with debouncing.
///
/// Uses inotify (Linux), kqueue (macOS), ReadDirectoryChangesW (Windows),
/// or a polling fallback on unsupported platforms.  Rapid saves within
/// the debounce window (default 100 ms) are coalesced into a single
/// callback invocation.
///
/// Usage:
///   FileWatcher fw;
///   fw.watch("/path/to/file.meld", [](const std::filesystem::path& p) {
///       std::cout << p << " changed\n";
///   });
///   // ... later ...
///   fw.stop();
class FileWatcher {
public:
    using ChangeCallback = std::function<void(const std::filesystem::path&)>;

    /// Construct a file watcher.
    /// @param debounce  Minimum interval between successive callbacks
    ///                  for the same file (default 100 ms).
    explicit FileWatcher(std::chrono::milliseconds debounce = std::chrono::milliseconds(100));

    ~FileWatcher();

    // Non-copyable, non-movable
    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;
    FileWatcher(FileWatcher&&) = delete;
    FileWatcher& operator=(FileWatcher&&) = delete;

    /// Start watching a file for modifications.
    /// @param path      Absolute or relative path to the file.
    /// @param callback  Invoked (on the watcher thread) when the file changes.
    /// @return true if the watch was set up successfully.
    bool watch(const std::filesystem::path& path, ChangeCallback callback);

    /// Stop watching and release all resources.
    void stop();

    /// Check whether the watcher is currently active.
    bool is_active() const;

private:
    std::chrono::milliseconds debounce_;
    std::atomic<bool> running_{false};

    // Watched target
    std::filesystem::path watched_path_;
    ChangeCallback callback_;
    std::mutex mutex_;

    // Background thread
    std::unique_ptr<std::thread> thread_;

    // Debounce bookkeeping
    std::chrono::steady_clock::time_point last_callback_time_;

    // Platform-specific descriptor (-1 when unused)
    int platform_fd_ = -1;
    int watch_fd_ = -1;

    // The main watch loop dispatched on the background thread.
    void watch_loop();

    // Platform-specific setup; returns true on success.
    bool platform_init();

    // Platform-specific teardown.
    void platform_cleanup();

    // Platform-specific single wait iteration.
    // Returns true if a change was detected.
    bool platform_poll_once();

    // Polling fallback used when no native API is available.
    bool poll_fallback();

    // Last known modification time (for polling fallback).
    std::filesystem::file_time_type last_write_time_;
};

} // namespace meld::interpreter
