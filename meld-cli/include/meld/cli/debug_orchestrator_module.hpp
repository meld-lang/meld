#pragma once

#include "command_handler.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace meld::cli {

/**
 * Unified options for `meld debug`.
 * Consolidates --run and --attach into a single orchestrator.
 */
struct DebugOptions {
    /// Mode: run a binary or attach to a running process.
    enum class Mode { Run, Attach, None };
    Mode mode = Mode::None;

    /// --run <binary>: path to the binary to launch under the debugger.
    std::string run_binary;

    /// --attach <pid>: PID of a running process to attach to.
    uint32_t attach_pid = 0;

    /// --debugger <lldb|gdb>: override auto-detection.
    enum class Debugger { Auto, LLDB, GDB };
    Debugger debugger = Debugger::Auto;

    /// --break <file:line>: initial breakpoint (optional).
    std::string breakpoint;

    /// --sandbox: opt-in SRT sandbox during debug session.
    bool sandbox = false;

    /// --dap: start a DAP server instead of interactive debugger.
    bool dap_mode = false;

    /// --port <port>: DAP server port (default 4711).
    uint16_t dap_port = 4711;
};

/**
 * Handles the unified `meld debug` subcommand (Req 20).
 *
 * Supersedes the old DebugAttachModule. Supports:
 *   meld debug --run <binary> [--debugger lldb|gdb] [--break file:line]
 *              [--sandbox] [--dap] [--port N]
 *   meld debug --attach <pid> [--debugger lldb|gdb] [--dap] [--port N]
 *
 * Automatically resolves .mdebug sidecars via the daemon's
 * DebugSidecarResolver when available.
 */
class DebugOrchestratorModule : public BaseCommandHandler {
public:
    DebugOrchestratorModule();
    ~DebugOrchestratorModule() = default;

    CommandResult execute(const CommandArgs& args) override;
    std::string get_help() const override;
    std::string get_usage() const override;
    std::vector<std::string> get_completions(const std::string& partial) const override;
    bool validate_args(const CommandArgs& args, std::string& error_message) const override;

private:
    /// Parse CommandArgs into DebugOptions.
    DebugOptions parse_options(const CommandArgs& args) const;

    /// Orchestrate a --run session: launch binary under debugger.
    CommandResult run_session(const DebugOptions& options);

    /// Orchestrate an --attach session: attach debugger to PID.
    CommandResult attach_session(const DebugOptions& options);

    /// Auto-detect the appropriate debugger.
    DebugOptions::Debugger detect_debugger() const;

    /// Spawn the debugger process with the given options.
    CommandResult spawn_debugger(const DebugOptions& options);

    /// Start a DAP server wrapping the debugger.
    CommandResult start_dap_server(const DebugOptions& options);

    /// Resolve the .mdebug sidecar for a binary (via daemon or co-located).
    std::filesystem::path resolve_mdebug(const std::filesystem::path& binary) const;

    /// Resolve the Meld data formatter script path.
    std::filesystem::path formatter_script_path() const;

    /// Check whether a binary has DWARF debug sections.
    bool has_debug_symbols(const std::filesystem::path& binary) const;
};

}  // namespace meld::cli
