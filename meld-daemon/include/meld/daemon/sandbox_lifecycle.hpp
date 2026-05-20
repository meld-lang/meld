#pragma once

#include "meld/daemon/semantic_model.hpp"

#include <chrono>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace meld::daemon {

/// Sandbox configuration derived from a .meld manifest
struct SandboxConfig {
    std::filesystem::path binary_path;
    std::filesystem::path manifest_path;
    std::vector<std::string> allowed_effects;  // e.g., ["file_system", "network"]
    std::filesystem::path policy_file;         // Ephemeral srt-settings.json path
    std::chrono::seconds timeout{300};         // Default 5min for test, 0 for run
};

/// Tracks an active sandboxed process
struct SandboxedProcess {
    int pid{0};
    SandboxConfig config;
    std::chrono::steady_clock::time_point started;
};

/// Manages sandbox policy lifecycle: generation, cleanup, and process tracking.
/// Delegates to the SandboxProvider interface from meld-manifest.
class SandboxLifecycleManager {
public:
    SandboxLifecycleManager() = default;
    ~SandboxLifecycleManager();

    /// Generate an ephemeral srt-settings.json from a manifest and launch the process.
    /// Returns the PID of the sandboxed process, or nullopt on failure.
    std::optional<int> launch(SandboxConfig config);

    /// Called when a sandboxed process exits — cleans up the policy file.
    void on_process_exit(int pid);

    /// Terminate all active sandboxed processes.
    void terminate_all();

    /// Terminate processes that have exceeded their timeout.
    void enforce_timeouts();

    /// Scan temp directory for stale policy files and delete them (Req 5.4).
    static void cleanup_stale_policies(const std::filesystem::path& tmp_dir);

    /// Get the number of active sandboxed processes.
    size_t active_count() const;

    /// Get info about an active process.
    std::optional<SandboxedProcess> get_process(int pid) const;

private:
    /// Generate the srt-settings.json content from a SandboxConfig
    std::string generate_policy_json(const SandboxConfig& config) const;

    mutable std::mutex mutex_;
    std::unordered_map<int, SandboxedProcess> active_processes_;
};

}  // namespace meld::daemon
