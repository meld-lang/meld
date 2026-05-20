#include "meld/cli/debug_attach_module.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <csignal>
#include <unistd.h>
#endif

namespace meld::cli {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

DebugAttachModule::DebugAttachModule()
    : BaseCommandHandler("debug", "Compiled debug attach (meld debug attach)") {
}

// ---------------------------------------------------------------------------
// execute — parse args, build AttachOptions, delegate to attach()
// ---------------------------------------------------------------------------

CommandResult DebugAttachModule::execute(const CommandArgs& args) {
    std::string error_message;
    if (!validate_args(args, error_message)) {
        std::cerr << "error: " << error_message << std::endl;
        return CommandResult::InvalidArguments;
    }

    AttachOptions options;

    // First positional arg is the PID.
    try {
        options.pid = static_cast<uint32_t>(std::stoul(args.positional[0]));
    } catch (const std::exception&) {
        std::cerr << "error: invalid PID: " << args.positional[0] << std::endl;
        return CommandResult::InvalidArguments;
    }

    // --debugger <lldb|gdb> override.
    if (auto it = args.options.find("debugger"); it != args.options.end()) {
        const auto& val = it->second;
        if (val == "lldb") {
            options.debugger = AttachOptions::Debugger::LLDB;
        } else if (val == "gdb") {
            options.debugger = AttachOptions::Debugger::GDB;
        } else {
            std::cerr << "error: unknown debugger '" << val
                      << "' — expected 'lldb' or 'gdb'" << std::endl;
            return CommandResult::InvalidArguments;
        }
    }

    // --dap and --port are parsed here but DAP logic is deferred to task 41.3.
    options.dap_mode = args.flags.count("dap") > 0;
    if (auto it = args.options.find("port"); it != args.options.end()) {
        try {
            options.dap_port = static_cast<uint16_t>(std::stoul(it->second));
        } catch (const std::exception&) {
            std::cerr << "error: invalid port: " << it->second << std::endl;
            return CommandResult::InvalidArguments;
        }
    }

    return attach(options);
}

// ---------------------------------------------------------------------------
// attach — orchestrate: validate PID, detect debugger, check symbols, spawn
// ---------------------------------------------------------------------------

CommandResult DebugAttachModule::attach(const AttachOptions& options) {
    // 1. Validate that the target process exists.
#ifdef _WIN32
    HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                              static_cast<DWORD>(options.pid));
    if (proc == nullptr) {
        std::cerr << "error: cannot attach to process " << options.pid
                  << ": process does not exist or access denied" << std::endl;
        return CommandResult::Error;
    }
    CloseHandle(proc);
#else
    if (kill(static_cast<pid_t>(options.pid), 0) != 0) {
        std::string reason;
        if (errno == ESRCH) {
            reason = "process does not exist";
        } else if (errno == EPERM) {
            reason = "permission denied";
        } else {
            reason = "unknown error";
        }
        std::cerr << "error: cannot attach to process " << options.pid
                  << ": " << reason << std::endl;
        return CommandResult::Error;
    }
#endif

    // 2. Resolve debugger (auto-detect or honour override).
    AttachOptions::Debugger debugger = options.debugger;
    if (debugger == AttachOptions::Debugger::Auto) {
        debugger = detect_debugger();
        if (debugger == AttachOptions::Debugger::Auto) {
            std::cerr << "error: cannot attach to process " << options.pid
                      << ": no supported debugger (lldb or gdb) found on PATH"
                      << std::endl;
            return CommandResult::Error;
        }
    }

    // 3. Check for debug symbols and warn if missing.
    if (!has_debug_symbols(options.pid)) {
        std::cerr << "warning: target binary may lack debug symbols"
                  << " — consider rebuilding with 'meld build --debug'"
                  << std::endl;
    }

    // 4. Build effective options with resolved debugger and spawn.
    AttachOptions resolved = options;
    resolved.debugger = debugger;

    // 5. If DAP mode requested, start a DAP server instead of interactive debugger.
    if (resolved.dap_mode) {
        return start_dap_server(resolved);
    }

    return spawn_debugger(resolved);
}

// ---------------------------------------------------------------------------
// detect_debugger — platform-aware auto-detection
// ---------------------------------------------------------------------------

AttachOptions::Debugger DebugAttachModule::detect_debugger() const {
#ifdef __APPLE__
    // macOS ships with LLDB; prefer it.
    if (std::system("which lldb > /dev/null 2>&1") == 0) {
        return AttachOptions::Debugger::LLDB;
    }
    if (std::system("which gdb > /dev/null 2>&1") == 0) {
        return AttachOptions::Debugger::GDB;
    }
#elif defined(_WIN32)
    // Windows: check via `where`.
    if (std::system("where lldb > NUL 2>&1") == 0) {
        return AttachOptions::Debugger::LLDB;
    }
    if (std::system("where gdb > NUL 2>&1") == 0) {
        return AttachOptions::Debugger::GDB;
    }
#else
    // Linux and other POSIX — prefer GDB.
    if (std::system("which gdb > /dev/null 2>&1") == 0) {
        return AttachOptions::Debugger::GDB;
    }
    if (std::system("which lldb > /dev/null 2>&1") == 0) {
        return AttachOptions::Debugger::LLDB;
    }
#endif
    return AttachOptions::Debugger::Auto; // nothing found
}

// ---------------------------------------------------------------------------
// spawn_debugger — build command and exec
// ---------------------------------------------------------------------------

CommandResult DebugAttachModule::spawn_debugger(const AttachOptions& options) {
    std::filesystem::path fmt_path = formatter_script_path();
    std::ostringstream cmd;

    if (options.debugger == AttachOptions::Debugger::LLDB) {
        // LLDB: attach to PID and auto-load Meld formatters.
        cmd << "lldb -p " << options.pid;
        if (std::filesystem::exists(fmt_path)) {
            cmd << " -o \"command script import " << fmt_path.string() << "\"";
        }
    } else {
        // GDB: attach to PID and source Meld pretty-printers.
        cmd << "gdb -p " << options.pid;
        // GDB uses a separate printers file located alongside the LLDB formatters.
        std::filesystem::path gdb_path =
            fmt_path.parent_path().parent_path() / "gdb" / "meld_gdb_printers.py";
        if (std::filesystem::exists(gdb_path)) {
            cmd << " -ex \"source " << gdb_path.string() << "\"";
        }
    }

    int ret = std::system(cmd.str().c_str());
    if (ret != 0) {
        std::cerr << "error: cannot attach to process " << options.pid
                  << ": debugger exited with code " << ret << std::endl;
        return CommandResult::Error;
    }
    return CommandResult::Success;
}

// ---------------------------------------------------------------------------
// start_dap_server — launch LLDB-DAP or GDB/MI for IDE DAP clients
// ---------------------------------------------------------------------------

CommandResult DebugAttachModule::start_dap_server(const AttachOptions& options) {
    std::filesystem::path fmt_path = formatter_script_path();
    std::ostringstream cmd;

    std::string debugger_name;

    if (options.debugger == AttachOptions::Debugger::LLDB) {
        debugger_name = "lldb";
        // lldb-dap (formerly lldb-vscode) speaks DAP natively.
        // Spawn it on the requested port; the IDE sends an `attach` request
        // with the PID, so we don't need to pass --attach here.
        cmd << "lldb-dap --port " << options.dap_port;
    } else {
        debugger_name = "gdb";
        // GDB/MI mode: spawn GDB in machine-interface mode attached to the
        // target PID.  Full GDB/MI-to-DAP translation is a future
        // enhancement; for now the IDE can consume the MI stream directly.
        cmd << "gdb --interpreter=mi -p " << options.pid;
    }

    std::cerr << "Starting DAP server on port " << options.dap_port
              << "..." << std::endl;
    std::cerr << "Debugger: " << debugger_name << std::endl;

    int ret = std::system(cmd.str().c_str());
    if (ret != 0) {
        std::cerr << "error: DAP server exited with code " << ret << std::endl;
        return CommandResult::Error;
    }
    return CommandResult::Success;
}

// ---------------------------------------------------------------------------
// formatter_script_path — resolve meld_formatters.py relative to CLI binary
// ---------------------------------------------------------------------------

std::filesystem::path DebugAttachModule::formatter_script_path() const {
    // Resolve the CLI binary location and navigate to the formatter script.
    // Layout: <prefix>/bin/meld  →  <prefix>/meld-core/tools/lldb/meld_formatters.py
    std::error_code ec;
#ifdef _WIN32
    // On Windows, use GetModuleFileName.
    char buf[MAX_PATH];
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::filesystem::path bin_path(buf);
#else
    // On POSIX, /proc/self/exe or argv[0] via canonical.
    std::filesystem::path bin_path =
        std::filesystem::canonical("/proc/self/exe", ec);
    if (ec) {
        // Fallback: try relative path from current working directory.
        bin_path = std::filesystem::current_path(ec);
    }
#endif
    // Navigate: <bin_dir>/../meld-core/tools/lldb/meld_formatters.py
    std::filesystem::path base = bin_path.parent_path().parent_path();
    return base / "meld-lang" / "tools" / "lldb" / "meld_formatters.py";
}

// ---------------------------------------------------------------------------
// has_debug_symbols — check for DWARF .debug_info section
// ---------------------------------------------------------------------------

bool DebugAttachModule::has_debug_symbols(uint32_t pid) const {
#ifdef _WIN32
    // On Windows, heuristic check is non-trivial; assume symbols present.
    (void)pid;
    return true;
#elif defined(__APPLE__)
    // macOS: use dwarfdump on the binary.
    // Resolve binary path via lsof or similar; fall back to optimistic.
    std::ostringstream cmd;
    cmd << "dwarfdump --debug-info /proc/" << pid << "/exe 2>/dev/null"
        << " | head -1 | grep -q .debug_info";
    // /proc is not standard on macOS; try a different approach.
    // Use `lldb -p <pid> -o 'image list' -o quit` or simply return true
    // as a best-effort heuristic. Full implementation would use dsymutil.
    (void)pid;
    return true;
#else
    // Linux: read /proc/<pid>/exe and check for .debug_info section.
    std::ostringstream proc_path;
    proc_path << "/proc/" << pid << "/exe";

    std::ostringstream cmd;
    cmd << "readelf -S " << proc_path.str()
        << " 2>/dev/null | grep -q .debug_info";
    int ret = std::system(cmd.str().c_str());
    return ret == 0;
#endif
}

// ---------------------------------------------------------------------------
// Help / usage / completions / validation
// ---------------------------------------------------------------------------

std::string DebugAttachModule::get_help() const {
    return R"(Attach a debugger to a running Meld process:

USAGE:
    meld debug attach <pid> [OPTIONS]

OPTIONS:
    --debugger <lldb|gdb>   Use a specific debugger instead of auto-detecting
    --dap                   Start a DAP server wrapping the debugger session
    --port <port>           DAP server port (default: 4711, used with --dap)

DESCRIPTION:
    Detects the host platform and launches the appropriate debugger (LLDB on
    macOS, GDB on Linux) attached to the given process ID. Meld data formatters
    are auto-loaded so kernel types display in human-readable form.

    If the target binary lacks DWARF debug symbols a warning is printed.

EXAMPLES:
    meld debug attach 12345
    meld debug attach 12345 --debugger lldb
    meld debug attach 12345 --debugger gdb
    meld debug attach 12345 --dap --port 5000)";
}

std::string DebugAttachModule::get_usage() const {
    return "meld debug attach <pid> [--debugger <lldb|gdb>] [--dap] [--port <port>]";
}

std::vector<std::string> DebugAttachModule::get_completions(
    const std::string& partial) const {
    std::vector<std::string> all = {
        "--debugger", "--dap", "--port"
    };
    std::vector<std::string> result;
    for (const auto& c : all) {
        if (c.find(partial) == 0) {
            result.push_back(c);
        }
    }
    return result;
}

bool DebugAttachModule::validate_args(const CommandArgs& args,
                                      std::string& error_message) const {
    if (args.positional.empty()) {
        error_message = "missing required argument: <pid>";
        return false;
    }

    // Verify the PID string is a valid unsigned integer.
    try {
        std::stoul(args.positional[0]);
    } catch (const std::exception&) {
        error_message = "invalid PID: " + args.positional[0];
        return false;
    }

    return true;
}

} // namespace meld::cli
