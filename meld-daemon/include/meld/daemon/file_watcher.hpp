#pragma once

#include <chrono>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace meld::daemon {

/// Type of file change event
enum class FileChangeType { Created, Modified, Deleted };

/// A single file change event
struct FileChangeEvent {
    std::filesystem::path path;
    FileChangeType type;
};

/// Filesystem watcher that monitors .meld files and meld.toml for changes.
/// Uses FSEvents on macOS, inotify on Linux. Debounces rapid events.
class FileWatcher {
public:
    using ChangeCallback = std::function<void(const std::vector<FileChangeEvent>&)>;

    explicit FileWatcher(const std::filesystem::path& root,
                         std::chrono::milliseconds debounce = std::chrono::milliseconds(50));
    ~FileWatcher();

    // Non-copyable
    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;

    /// Start watching (spawns background thread)
    void start(ChangeCallback callback);

    /// Stop watching
    void stop();

    /// Check if watching
    bool is_watching() const { return watching_; }

    /// Get the debounce window
    std::chrono::milliseconds debounce_window() const { return debounce_; }

    /// Set the debounce window
    void set_debounce_window(std::chrono::milliseconds ms) { debounce_ = ms; }

    /// Manually inject an event (for testing)
    void inject_event(FileChangeEvent event);

private:
    /// Check if a path should be watched (.meld files and meld.toml)
    static bool should_watch(const std::filesystem::path& path);

    /// Platform-specific watch loop
    void watch_loop();

    /// Process debounced events
    void flush_pending();

    std::filesystem::path root_;
    std::chrono::milliseconds debounce_;
    ChangeCallback callback_;
    bool watching_{false};
    std::thread watch_thread_;

    std::mutex pending_mutex_;
    std::vector<FileChangeEvent> pending_events_;
    std::chrono::steady_clock::time_point last_event_time_;
};

}  // namespace meld::daemon
