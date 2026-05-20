#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace meld::cli {

/// Result of a binary freshness check against the daemon.
struct FreshnessResult {
    bool is_current{false};
    std::vector<std::string> stale_sources;
    bool rebuild_triggered{false};
    std::string build_error;  // non-empty if rebuild failed
};

/// Lightweight IPC client for communicating with the meldd daemon.
/// Used by `meld run` for the daemon handshake (Req 2.14) and
/// debug sidecar resolution (Req 2.15).
class DaemonClient {
public:
    explicit DaemonClient(const std::filesystem::path& workspace);

    /// Check if the daemon is reachable.
    bool is_daemon_running() const;

    /// Check binary freshness; triggers rebuild if stale (Req 2.14).
    /// Returns nullopt if daemon is not running (graceful fallback).
    std::optional<FreshnessResult> check_binary_freshness(
        const std::filesystem::path& binary_path) const;

    /// Resolve a debug_id to the .mdebug sidecar path (Req 2.15).
    /// Returns nullopt if daemon is not running or sidecar not found.
    std::optional<std::filesystem::path> resolve_debug_sidecar(
        const std::string& debug_id) const;

private:
    std::filesystem::path workspace_;
    std::filesystem::path pid_file_path() const;
    std::optional<int> read_daemon_pid() const;
};

} // namespace meld::cli
