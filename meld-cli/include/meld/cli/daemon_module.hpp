#pragma once

#include "command_handler.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#ifndef _WIN32
#include <sys/types.h>  // pid_t
#endif

namespace meld::cli {

/// Options for `meld daemon start`.
struct DaemonStartOptions {
    std::filesystem::path workspace;            // --workspace=<path>
    bool foreground = false;                    // --foreground
    std::optional<uint32_t> timeout_secs;       // --timeout=<seconds>
};

/// Options for `meld daemon logs`.
struct DaemonLogOptions {
    uint32_t lines = 50;    // --lines=<N>
    bool follow = true;     // default: tail mode; --no-follow disables
};

/// Status information returned by the daemon.
struct DaemonStatus {
    bool running = false;
    pid_t pid = 0;
    std::filesystem::path workspace;
    std::chrono::seconds uptime{0};
    uint32_t lsp_clients = 0;
    uint32_t mcp_clients = 0;
    float indexing_progress = 0.0f;  // 0.0–1.0
    uint32_t active_sandboxed_processes = 0;
};

/**
 * Handles the `meld daemon` subcommand.
 * Manages the meldd daemon lifecycle: start, stop, status, restart, logs.
 *
 * Requirements: 24.1–24.16
 */
class DaemonModule : public BaseCommandHandler {
public:
    DaemonModule();
    ~DaemonModule() override = default;

    CommandResult execute(const CommandArgs& args) override;
    std::string get_help() const override;
    std::string get_usage() const override;
    std::vector<std::string> get_completions(const std::string& partial) const override;
    bool validate_args(const CommandArgs& args, std::string& error_message) const override;

private:
    CommandResult handle_start(const DaemonStartOptions& opts, bool json_output);
    CommandResult handle_stop(bool json_output);
    CommandResult handle_status(bool json_output);
    CommandResult handle_restart(const DaemonStartOptions& opts, bool json_output);
    CommandResult handle_logs(const DaemonLogOptions& opts);

    /// Find a running daemon via PID file. Returns empty if none running.
    std::optional<pid_t> find_running_daemon() const;

    /// Check if a PID is alive.
    bool is_process_alive(pid_t pid) const;

    /// Remove stale PID file.
    void remove_stale_pid_file() const;

    /// Kill all orphaned meldd processes for this workspace.
    void kill_orphaned_daemons() const;

    std::filesystem::path pid_file_path() const;
    std::filesystem::path log_file_path() const;

    /// Resolve workspace root from CLI args or cwd.
    std::filesystem::path resolve_workspace(const CommandArgs& args) const;

    std::filesystem::path workspace_root_;
};

} // namespace meld::cli
