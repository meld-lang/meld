#include "meld/cli/daemon_module.hpp"
#include "meld/cli/daemon_bridge.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>

#ifndef _WIN32
#include <csignal>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace meld::cli {

DaemonModule::DaemonModule()
    : BaseCommandHandler("daemon", "Manage the meldd daemon lifecycle") {
}

CommandResult DaemonModule::execute(const CommandArgs& args) {
    workspace_root_ = resolve_workspace(args);
    bool json_output = args.flags.count("json") > 0;

    // Determine sub-subcommand from first positional arg.
    std::string subcmd;
    if (!args.positional.empty()) {
        subcmd = args.positional[0];
    }

    if (subcmd.empty()) {
        std::cout << get_help() << std::endl;
        return CommandResult::Success;
    }
    if (subcmd == "status") {
        return handle_status(json_output);
    }
    if (subcmd == "start") {
        DaemonStartOptions opts;
        opts.workspace = workspace_root_;
        opts.foreground = args.flags.count("foreground") > 0;
        if (args.options.count("timeout")) {
            try {
                opts.timeout_secs = static_cast<uint32_t>(std::stoul(args.options.at("timeout")));
            } catch (...) {}
        }
        return handle_start(opts, json_output);
    }
    if (subcmd == "stop") {
        return handle_stop(json_output);
    }
    if (subcmd == "restart") {
        DaemonStartOptions opts;
        opts.workspace = workspace_root_;
        opts.foreground = args.flags.count("foreground") > 0;
        return handle_restart(opts, json_output);
    }
    if (subcmd == "logs") {
        DaemonLogOptions lopts;
        lopts.follow = args.flags.count("no-follow") == 0;
        if (args.options.count("lines")) {
            try {
                lopts.lines = static_cast<uint32_t>(std::stoul(args.options.at("lines")));
            } catch (...) {}
        }
        return handle_logs(lopts);
    }

    std::cerr << "error: unknown daemon subcommand: " << subcmd << std::endl;
    std::cerr << "usage: " << get_usage() << std::endl;
    return CommandResult::InvalidArguments;
}

// ---------------------------------------------------------------------------
// Start
// ---------------------------------------------------------------------------

CommandResult DaemonModule::handle_start(const DaemonStartOptions& opts,
                                         bool json_output) {
    auto existing = find_running_daemon();
    if (existing) {
        if (json_output) {
            std::cout << "{\"status\":\"already_running\",\"pid\":" << *existing
                      << ",\"workspace\":\"" << workspace_root_.string() << "\"}" << std::endl;
        } else {
            std::cout << "Daemon already running (PID " << *existing << ") for workspace: "
                      << workspace_root_.string() << std::endl;
        }
        return CommandResult::Success;
    }

    // Ensure .meld/ directory exists.
    auto meld_dir = workspace_root_ / ".meld";
    std::filesystem::create_directories(meld_dir);

    // Resolve meldd binary path.
    auto meldd_path = DaemonBridge::find_meldd();
    if (!meldd_path) {
        std::cerr << "error: meldd binary not found. Build with: bazel build //meld-daemon:meldd" << std::endl;
        return CommandResult::Error;
    }
    std::string cmd = meldd_path->string() + " --workspace=" + workspace_root_.string();
    if (opts.timeout_secs) {
        cmd += " --timeout=" + std::to_string(*opts.timeout_secs);
    }

#ifndef _WIN32
    if (opts.foreground) {
        // Run in foreground — exec replaces this process.
        if (json_output) {
            std::cout << "{\"status\":\"starting\",\"mode\":\"foreground\",\"workspace\":\""
                      << workspace_root_.string() << "\"}" << std::endl;
        } else {
            std::cout << "Starting daemon in foreground for: " << workspace_root_.string() << std::endl;
        }
        execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
        // If exec returns, it failed.
        std::cerr << "error: failed to exec meldd" << std::endl;
        return CommandResult::Error;
    }

    // Daemonize: fork and exec meldd in background.
    pid_t child = fork();
    if (child < 0) {
        std::cerr << "error: fork failed" << std::endl;
        return CommandResult::Error;
    }
    if (child == 0) {
        // Child: fully detach from parent.
        setsid();
        // Redirect stdin/stdout/stderr to /dev/null so the daemon
        // doesn't hold the parent's file descriptors open.
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            if (devnull > STDERR_FILENO) close(devnull);
        }
        // Redirect daemon stderr to log file.
        auto log = log_file_path();
        int log_fd = open(log.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (log_fd >= 0) {
            dup2(log_fd, STDERR_FILENO);
            close(log_fd);
        }
        execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
        _exit(1);
    }
#endif

    // Parent: wait for PID file to appear (up to 5s).
    auto pid_path = pid_file_path();
    for (int i = 0; i < 50; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (std::filesystem::exists(pid_path)) {
            auto running = find_running_daemon();
            if (running) {
                if (json_output) {
                    std::cout << "{\"status\":\"started\",\"pid\":" << *running
                              << ",\"workspace\":\"" << workspace_root_.string() << "\"}" << std::endl;
                } else {
                    std::cout << "Daemon started (PID " << *running << ") for workspace: "
                              << workspace_root_.string() << std::endl;
                }
                return CommandResult::Success;
            }
        }
    }

    std::cerr << "warning: daemon may not have started — PID file not found after 5s" << std::endl;
    return CommandResult::Error;
}

// ---------------------------------------------------------------------------
// Stop
// ---------------------------------------------------------------------------

CommandResult DaemonModule::handle_stop(bool json_output) {
    auto pid = find_running_daemon();
    if (!pid) {
        if (json_output) {
            std::cout << "{\"status\":\"not_running\",\"workspace\":\""
                      << workspace_root_.string() << "\"}" << std::endl;
        } else {
            std::cout << "info: no daemon running for workspace: "
                      << workspace_root_.string() << std::endl;
        }
        return CommandResult::Success;
    }

#ifndef _WIN32
    // Send SIGTERM for graceful shutdown.
    kill(*pid, SIGTERM);

    // Wait up to 10s for exit.
    for (int i = 0; i < 100; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (!is_process_alive(*pid)) {
            remove_stale_pid_file();
            // Kill any orphaned meldd processes for this workspace.
            kill_orphaned_daemons();
            if (json_output) {
                std::cout << "{\"status\":\"stopped\",\"pid\":" << *pid << "}" << std::endl;
            } else {
                std::cout << "Daemon stopped (PID " << *pid << ")" << std::endl;
            }
            return CommandResult::Success;
        }
    }

    // Timeout — force kill.
    kill(*pid, SIGKILL);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    remove_stale_pid_file();

    if (json_output) {
        std::cout << "{\"status\":\"killed\",\"pid\":" << *pid << "}" << std::endl;
    } else {
        std::cerr << "warning: daemon did not stop gracefully — killed (PID " << *pid << ")" << std::endl;
    }
#endif

    // Kill any orphaned meldd processes for this workspace.
    kill_orphaned_daemons();

    return CommandResult::Success;
}

// ---------------------------------------------------------------------------
// Status
// ---------------------------------------------------------------------------

CommandResult DaemonModule::handle_status(bool json_output) {
    auto pid = find_running_daemon();

    DaemonStatus status;
    status.workspace = workspace_root_;

    if (!pid) {
        status.running = false;
        if (json_output) {
            std::cout << "{\"running\":false,\"workspace\":\""
                      << workspace_root_.string() << "\"}" << std::endl;
        } else {
            std::cout << "Daemon: stopped" << std::endl;
            std::cout << "  Workspace: " << workspace_root_.string() << std::endl;
        }
        return CommandResult::Success;
    }

    status.running = true;
    status.pid = *pid;
    // TODO: Query daemon via IPC for detailed status (uptime, clients, indexing progress).
    // For now report what we can determine from the PID file.

    if (json_output) {
        std::cout << "{\"running\":true,\"pid\":" << status.pid
                  << ",\"workspace\":\"" << workspace_root_.string() << "\"}" << std::endl;
    } else {
        std::cout << "Daemon: running" << std::endl;
        std::cout << "  PID: " << status.pid << std::endl;
        std::cout << "  Workspace: " << workspace_root_.string() << std::endl;
    }
    return CommandResult::Success;
}

// ---------------------------------------------------------------------------
// Restart
// ---------------------------------------------------------------------------

CommandResult DaemonModule::handle_restart(const DaemonStartOptions& opts,
                                           bool json_output) {
    handle_stop(false);  // quiet stop
    return handle_start(opts, json_output);
}

// ---------------------------------------------------------------------------
// Logs
// ---------------------------------------------------------------------------

CommandResult DaemonModule::handle_logs(const DaemonLogOptions& opts) {
    auto log_path = log_file_path();
    if (!std::filesystem::exists(log_path)) {
        std::cerr << "error: daemon log not found: " << log_path.string() << std::endl;
        return CommandResult::Error;
    }

    // Read last N lines.
    std::ifstream file(log_path);
    if (!file.is_open()) {
        std::cerr << "error: cannot open log file: " << log_path.string() << std::endl;
        return CommandResult::Error;
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }

    // Print last N lines.
    size_t start = lines.size() > opts.lines ? lines.size() - opts.lines : 0;
    for (size_t i = start; i < lines.size(); ++i) {
        std::cout << lines[i] << std::endl;
    }

    if (!opts.follow) {
        return CommandResult::Success;
    }

    // Tail mode: watch for new lines.
    file.clear();
    file.seekg(0, std::ios::end);
    while (true) {
        if (std::getline(file, line)) {
            std::cout << line << std::endl;
        } else {
            file.clear();
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    }

    return CommandResult::Success;  // unreachable in follow mode
}

// ---------------------------------------------------------------------------
// Process discovery helpers
// ---------------------------------------------------------------------------

std::optional<pid_t> DaemonModule::find_running_daemon() const {
    auto path = pid_file_path();
    if (!std::filesystem::exists(path)) {
        return std::nullopt;
    }

    std::ifstream file(path);
    pid_t pid = 0;
    file >> pid;
    if (pid <= 0) {
        return std::nullopt;
    }

    if (!is_process_alive(pid)) {
        // Stale PID file — process is dead.
        const_cast<DaemonModule*>(this)->remove_stale_pid_file();
        return std::nullopt;
    }

    return pid;
}

bool DaemonModule::is_process_alive(pid_t pid) const {
#ifndef _WIN32
    return kill(pid, 0) == 0;
#else
    return false;  // TODO: Windows implementation
#endif
}

void DaemonModule::remove_stale_pid_file() const {
    auto path = pid_file_path();
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

std::filesystem::path DaemonModule::pid_file_path() const {
    return workspace_root_ / ".meld" / "meldd.pid";
}

std::filesystem::path DaemonModule::log_file_path() const {
    return workspace_root_ / ".meld" / "meldd.log";
}

std::filesystem::path DaemonModule::resolve_workspace(const CommandArgs& args) const {
    if (args.options.count("workspace")) {
        return args.options.at("workspace");
    }
    return std::filesystem::current_path();
}

void DaemonModule::kill_orphaned_daemons() const {
#ifndef _WIN32
    auto ws_arg = "--workspace=" + workspace_root_.string();
    // Use pgrep to find all meldd processes with our workspace arg
    std::string cmd = "pgrep -f 'meldd.*" + ws_arg + "' 2>/dev/null";
    FILE* fp = popen(cmd.c_str(), "r");
    if (!fp) return;
    char buf[32];
    while (fgets(buf, sizeof(buf), fp)) {
        pid_t pid = std::atoi(buf);
        if (pid > 0) kill(pid, SIGKILL);
    }
    pclose(fp);
#endif
}

// ---------------------------------------------------------------------------
// Help / completions
// ---------------------------------------------------------------------------

std::string DaemonModule::get_help() const {
    return R"(Daemon management commands:

USAGE:
    meld daemon <subcommand> [OPTIONS]

SUBCOMMANDS:
    start       Start the meldd daemon (or connect to existing)
    stop        Gracefully shut down the daemon
    status      Show daemon status (default if no subcommand)
    restart     Restart the daemon
    logs        Tail daemon log output

OPTIONS:
    --workspace=<path>    Project root directory (default: cwd)
    --foreground          Run daemon in foreground (start only)
    --timeout=<seconds>   Auto-exit after idle time (start only)
    --lines=<N>           Number of log lines to show (logs only, default: 50)
    --no-follow           Print log and exit without tailing (logs only)
    --json                Output in machine-readable JSON format

EXAMPLES:
    meld daemon start
    meld daemon start --foreground --timeout=300
    meld daemon stop
    meld daemon status --json
    meld daemon restart
    meld daemon logs --lines=100
    meld daemon logs --no-follow)";
}

std::string DaemonModule::get_usage() const {
    return "meld daemon <start|stop|status|restart|logs> [OPTIONS]";
}

std::vector<std::string> DaemonModule::get_completions(const std::string& partial) const {
    std::vector<std::string> all = {
        "start", "stop", "status", "restart", "logs",
        "--workspace", "--foreground", "--timeout",
        "--lines", "--no-follow", "--json"
    };
    std::vector<std::string> result;
    for (const auto& c : all) {
        if (c.find(partial) == 0) {
            result.push_back(c);
        }
    }
    return result;
}

bool DaemonModule::validate_args(const CommandArgs& /*args*/,
                                  std::string& /*error_message*/) const {
    return true;
}

} // namespace meld::cli
