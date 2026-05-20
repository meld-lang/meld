#pragma once

#include "meld/manifest/sandbox_provider.hpp"

#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>

namespace meld::manifest {

/// Manages ephemeral srt-settings.json lifecycle (Req 12).
class PolicyLifecycleManager {
public:
    PolicyLifecycleManager() = default;
    ~PolicyLifecycleManager();

    /// Generate a unique policy file and return its path
    std::filesystem::path generate_policy(const SandboxConfig& config);

    /// Delete a policy file after process exit
    void cleanup_policy(const std::filesystem::path& policy_path);

    /// Clean up stale policy files from previous sessions
    static void cleanup_stale(const std::filesystem::path& tmp_dir);

    /// Get the number of active policy files
    size_t active_count() const;

private:
    /// Generate a UUID-based filename
    static std::string generate_uuid();

    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::filesystem::path> active_policies_;
};

}  // namespace meld::manifest
