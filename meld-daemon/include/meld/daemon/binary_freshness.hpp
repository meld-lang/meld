#pragma once

#include "meld/daemon/semantic_model.hpp"

#include <chrono>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace meld::daemon {

// Forward declaration
class BazelWorker;

/// Result of a binary freshness check (Req 12.1).
struct FreshnessResult {
    bool is_current{false};
    std::vector<std::filesystem::path> stale_sources;
    bool rebuild_triggered{false};
    std::optional<std::string> build_diagnostics;  // On rebuild failure
};

/// Checks whether a compiled binary is up-to-date with respect to its
/// source dependencies, and triggers automatic rebuilds when stale (Req 12).
class BinaryFreshnessChecker {
public:
    BinaryFreshnessChecker(const SemanticModel& model,
                           BazelWorker* worker = nullptr);

    /// Check if binary is current; trigger rebuild if stale (blocks until complete).
    /// Accessible via both LSP and MCP channels (Req 12.1).
    FreshnessResult check(const std::filesystem::path& binary_path);

    /// Invalidate cached results for binaries depending on changed file (Req 12.7).
    void invalidate(const std::filesystem::path& changed_source);

    /// Invalidate all cached results.
    void invalidate_all();

    /// Get the debounce window.
    std::chrono::milliseconds debounce_window() const { return debounce_window_; }

    /// Set the debounce window.
    void set_debounce_window(std::chrono::milliseconds ms) { debounce_window_ = ms; }

private:
    const SemanticModel& model_;
    BazelWorker* worker_;  // May be null in tests

    mutable std::mutex cache_mutex_;
    struct CachedResult {
        FreshnessResult result;
        std::chrono::steady_clock::time_point timestamp;
    };
    std::unordered_map<std::string, CachedResult> cache_;
    std::chrono::milliseconds debounce_window_{50};

    /// Compare binary build timestamp against source mtimes (Req 12.2).
    bool is_stale(const std::filesystem::path& binary_path,
                  std::vector<std::filesystem::path>& stale_sources) const;

    /// Find the Bazel target for a binary.
    std::string resolve_target(const std::filesystem::path& binary_path) const;

    /// Get transitive source dependencies for a binary.
    std::unordered_set<std::string> get_transitive_deps(
        const std::filesystem::path& binary_path) const;

    /// Trigger a rebuild via BazelWorker (Req 12.3).
    bool trigger_rebuild(const std::string& target,
                         std::string& diagnostics);
};

}  // namespace meld::daemon
