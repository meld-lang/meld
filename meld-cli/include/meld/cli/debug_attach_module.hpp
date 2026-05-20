#pragma once

#include "command_handler.hpp"
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace meld::cli {

/**
 * Options for attaching a debugger to a running process.
 */
struct AttachOptions {
    /// Target process ID to attach to.
    uint32_t pid = 0;

    /// Debugger selection.
    enum class Debugger { Auto, LLDB, GDB };
    Debugger debugger = Debugger::Auto;

    /// Whether to start a DAP server wrapping the debugger session.
    bool dap_mode = false;

    /// DAP server port (only used when dap_mode is true).
    uint16_t dap_port = 4711;
};

/**
 * Handles the `meld debug attach` subcommand.
 * Attaches a standard OS debugger (LLDB/GDB) to a running native binary
 * compiled with `meld build --debug`, with Meld data formatters auto-loaded.
 *
 * Requirements: 18.1, 18.2, 18.3
 */
class DebugAttachModule : public BaseCommandHandler {
public:
    DebugAttachModule();
    ~DebugAttachModule() = default;

    // CommandHandler interface
    CommandResult execute(const CommandArgs& args) override;
    std::string get_help() const override;
    std::string get_usage() const override;
    std::vector<std::string> get_completions(const std::string& partial) const override;
    bool validate_args(const CommandArgs& args, std::string& error_message) const override;

private:
    /// Main attach logic — orchestrates detection, symbol check, and spawning.
    CommandResult attach(const AttachOptions& options);

    /// Auto-detect the appropriate debugger based on host platform and availability.
    AttachOptions::Debugger detect_debugger() const;

    /// Fork/exec the selected debugger, attaching to the target process.
    CommandResult spawn_debugger(const AttachOptions& options);

    /// Start a DAP server wrapping LLDB-DAP or GDB/MI for IDE graphical debugging.
    CommandResult start_dap_server(const AttachOptions& options);

    /// Resolve the path to meld_formatters.py relative to the CLI binary.
    std::filesystem::path formatter_script_path() const;

    /// Check whether the target process binary contains DWARF debug sections.
    bool has_debug_symbols(uint32_t pid) const;
};

} // namespace meld::cli
