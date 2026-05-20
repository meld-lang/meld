#include "meld/cli/interpreter_module.hpp"
#include "meld/cli/daemon_client.hpp"
#include "meld/cli/crash_trace.hpp"
#include <meld/effects/agent_test_mode.hpp>
#include <meld/interpreter/ast_interpreter.hpp>
#include <meld/daemon/dap_channel.hpp>
#include <meld/interpreter/file_watcher.hpp>
#include <meld/manifest/tombstone.hpp>
#include <meld/manifest/mdebug_sidecar.hpp>
#include <meld/parser/parser.hpp>
#include <linenoise/linenoise.h>
#include <boost/variant/get.hpp>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <thread>
#include <regex>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace meld::cli {

// RuntimeError implementation
std::string RuntimeError::format() const {
    std::ostringstream oss;
    if (!file.empty()) {
        oss << file << ":";
        if (line > 0) {
            oss << line << ":" << column << ": ";
        } else {
            oss << " ";
        }
    }
    oss << "error: " << message;
    
    if (!context.empty()) {
        oss << "\n  " << context;
    }
    
    if (!stack_trace.empty()) {
        oss << "\nStack trace:";
        for (const auto& frame : stack_trace) {
            oss << "\n  at " << frame;
        }
    }
    
    return oss.str();
}

// ReplSession implementation
ReplSession::ReplSession()
    : interp_env_(std::make_shared<interpreter::Environment>()),
      interp_(std::make_unique<interpreter::AstInterpreter>(interp_env_))
{
    environment_["MELD_REPL"] = "1";
    environment_["MELD_VERSION"] = "0.1.0";
}

ReplResult ReplSession::evaluate_line(const std::string& input) {
    ReplResult result;
    
    // Skip empty input
    if (input.empty() || std::all_of(input.begin(), input.end(), ::isspace)) {
        result.success = true;
        return result;
    }
    
    // Add to history
    add_to_history(input);
    
    // Parse the input as an expression (or declaration)
    parser::Parser p;
    parser::ast::expression expr;
    if (!p.parse_expression(input, expr)) {
        result.success = false;
        result.errors.emplace_back(p.error_message());
        return result;
    }
    
    // Evaluate using the persistent interpreter/environment
    try {
        auto val = interp_->evaluate(expr);
        result.success = true;
        result.value = format_value(val);
        result.type = value_type_name(val);
    } catch (const interpreter::InterpreterError& e) {
        result.success = false;
        result.errors.emplace_back(e.what(),
                                   e.location().file,
                                   e.location().line,
                                   e.location().column);
    } catch (const std::exception& e) {
        result.success = false;
        result.errors.emplace_back(e.what());
    }
    
    return result;
}

bool ReplSession::load_module(const std::string& module_name) {
    // Check if already loaded
    if (std::find(loaded_modules_.begin(), loaded_modules_.end(), module_name) != loaded_modules_.end()) {
        return true;
    }
    
    // Read the file
    std::ifstream file(module_name);
    if (!file.is_open()) {
        return false;
    }
    std::ostringstream buf;
    buf << file.rdbuf();
    std::string source = buf.str();
    
    // Parse the file
    parser::Parser p;
    std::vector<parser::ast::expression> ast;
    if (!p.parse_file(source, ast)) {
        return false;
    }
    
    // Evaluate in the session's persistent environment
    try {
        interp_->evaluate_program(ast);
        loaded_modules_.push_back(module_name);
        return true;
    } catch (const interpreter::InterpreterError&) {
        return false;
    } catch (const std::exception&) {
        return false;
    }
}

std::vector<std::string> ReplSession::get_completions(const std::string& partial) const {
    std::vector<std::string> completions;
    
    // Add common Meld keywords
    std::vector<std::string> keywords = {
        "val", "var", "fnc", "class", "struct", "enum", "trait",
        "if", "else", "while", "for", "match", "return", "break", "continue",
        "imp"
    };
    
    for (const auto& keyword : keywords) {
        if (keyword.starts_with(partial)) {
            completions.push_back(keyword);
        }
    }
    
    return completions;
}

void ReplSession::add_to_history(const std::string& command) {
    // Avoid duplicate consecutive entries
    if (!history_.empty() && history_.back() == command) {
        return;
    }
    
    history_.push_back(command);
    
    // Limit history size
    if (history_.size() > max_history_size_) {
        history_.erase(history_.begin());
    }
}

bool ReplSession::save_history(const std::filesystem::path& history_file) const {
    try {
        std::ofstream file(history_file);
        if (!file.is_open()) {
            return false;
        }
        
        for (const auto& command : history_) {
            file << command << "\n";
        }
        
        return true;
    } catch (...) {
        return false;
    }
}

bool ReplSession::load_history(const std::filesystem::path& history_file) {
    try {
        std::ifstream file(history_file);
        if (!file.is_open()) {
            return false;
        }
        
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty()) {
                history_.push_back(line);
            }
        }
        
        return true;
    } catch (...) {
        return false;
    }
}

void ReplSession::clear() {
    // Reset interpreter state — fresh environment and interpreter
    interp_env_ = std::make_shared<interpreter::Environment>();
    interp_ = std::make_unique<interpreter::AstInterpreter>(interp_env_);
    loaded_modules_.clear();
    // Keep history and environment metadata
}

void ReplSession::set_environment(const std::string& name, const std::string& value) {
    environment_[name] = value;
}

bool ReplSession::is_complete_expression(const std::string& input) const {
    // Simple heuristic - check for balanced braces/parentheses
    int brace_count = 0;
    int paren_count = 0;
    
    for (char c : input) {
        switch (c) {
            case '{': brace_count++; break;
            case '}': brace_count--; break;
            case '(': paren_count++; break;
            case ')': paren_count--; break;
        }
    }
    
    return brace_count == 0 && paren_count == 0;
}

std::string ReplSession::format_value(const kernel::Value& value) const {
    return value.to_string();
}

std::string ReplSession::value_type_name(const kernel::Value& value) const {
    if (value.is<kernel::Integer>()) return "Integer";
    if (value.is<kernel::String>()) return "String";
    if (value.is<kernel::Boolean>()) return "Boolean";
    if (value.is<kernel::Function>()) return "Function";
    if (value.is<kernel::Vec>()) return "Vec";
    if (value.is<kernel::Symbol>()) return "Symbol";
    if (value.is<kernel::Empty>()) return "Empty";
    if (value.is<kernel::Cons>()) return "Cons";
    return "Unknown";
}

// FileWatcher implementation
FileWatcher::FileWatcher() = default;

FileWatcher::~FileWatcher() {
    stop_all();
}

bool FileWatcher::watch_file(const std::filesystem::path& file_path, ChangeCallback callback) {
    std::lock_guard<std::mutex> lock(files_mutex_);
    
    try {
        if (!std::filesystem::exists(file_path)) {
            return false;
        }
        
        auto last_write_time = std::filesystem::last_write_time(file_path);
        
        watched_files_[file_path] = WatchedFile{
            file_path,
            last_write_time,
            std::move(callback)
        };
        
        // Start watcher thread if not already running
        if (!watcher_thread_) {
            should_stop_ = false;
            watcher_thread_ = std::make_unique<std::thread>(&FileWatcher::watch_loop, this);
        }
        
        return true;
    } catch (...) {
        return false;
    }
}

void FileWatcher::stop_watching(const std::filesystem::path& file_path) {
    std::lock_guard<std::mutex> lock(files_mutex_);
    watched_files_.erase(file_path);
}

void FileWatcher::stop_all() {
    should_stop_ = true;
    
    if (watcher_thread_ && watcher_thread_->joinable()) {
        watcher_thread_->join();
        watcher_thread_.reset();
    }
    
    std::lock_guard<std::mutex> lock(files_mutex_);
    watched_files_.clear();
}

void FileWatcher::watch_loop() {
    while (!should_stop_) {
        {
            std::lock_guard<std::mutex> lock(files_mutex_);
            
            for (auto& [path, watched_file] : watched_files_) {
                try {
                    if (std::filesystem::exists(path)) {
                        auto current_write_time = std::filesystem::last_write_time(path);
                        if (current_write_time != watched_file.last_write_time) {
                            watched_file.last_write_time = current_write_time;
                            watched_file.callback(path);
                        }
                    }
                } catch (...) {
                    // Ignore errors for individual files
                }
            }
        }
        
        // Check every 100ms
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// MeldRuntime implementation
MeldRuntime::MeldRuntime() {
    working_dir_ = std::filesystem::current_path();
}

ExecutionResult MeldRuntime::execute_file(const std::filesystem::path& file_path,
                                        const std::vector<std::string>& args) {
    try {
        std::string source_code = read_source_file(file_path);
        return execute_internal(source_code, args, file_path);
    } catch (const std::exception& e) {
        ExecutionResult result;
        result.success = false;
        result.exit_code = 1;
        result.errors.emplace_back("Failed to read file: " + std::string(e.what()), 
                                  file_path.string());
        return result;
    }
}

ExecutionResult MeldRuntime::execute_source(const std::string& source_code,
                                          const std::vector<std::string>& args) {
    return execute_internal(source_code, args);
}

bool MeldRuntime::has_main_function(const std::string& source_code) const {
    // Parse and check for a top-level function named "main"
    parser::Parser p;
    std::vector<parser::ast::expression> ast;
    if (!p.parse_file(source_code, ast)) {
        return false;
    }
    for (const auto& expr : ast) {
        auto* fn_ptr = boost::get<boost::spirit::x3::forward_ast<parser::ast::function_definition>>(&expr);
        if (fn_ptr && fn_ptr->get().name.name == "main") {
            return true;
        }
    }
    return false;
}

std::expected<std::string, RuntimeError> MeldRuntime::parse_source(const std::string& source_code) const {
    if (source_code.empty()) {
        return std::unexpected(RuntimeError("Empty source code"));
    }
    
    // Use the real Meld parser
    parser::Parser p;
    std::vector<parser::ast::expression> ast;
    if (!p.parse_file(source_code, ast)) {
        return std::unexpected(RuntimeError(p.error_message()));
    }
    
    return "parsed";
}

std::string MeldRuntime::read_source_file(const std::filesystem::path& file_path) const {
    if (!std::filesystem::exists(file_path)) {
        throw std::runtime_error("error: file not found: " + file_path.string());
    }
    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw std::runtime_error("error: cannot open file: " + file_path.string());
    }
    
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

ExecutionResult MeldRuntime::execute_internal(const std::string& source_code,
                                            const std::vector<std::string>& args,
                                            const std::filesystem::path& source_file) const {
    ExecutionResult result;
    
    // Parse source code using the real Meld parser
    parser::Parser p;
    std::vector<parser::ast::expression> ast;
    if (!p.parse_file(source_code, ast)) {
        result.success = false;
        result.exit_code = 1;
        auto err = RuntimeError(p.error_message(), source_file.string());
        result.errors.push_back(std::move(err));
        return result;
    }
    
    // Set up interpreter with a fresh environment
    auto env = std::make_shared<interpreter::Environment>();
    
    // Bind program arguments as a built-in `args` variable (kernel::Vec of kernel::String)
    std::vector<kernel::Value> arg_values;
    arg_values.reserve(args.size());
    for (const auto& a : args) {
        arg_values.push_back(kernel::Value(std::make_shared<kernel::String>(a)));
    }
    env->define("args", kernel::Value(std::make_shared<kernel::Vec>(std::move(arg_values))),
                /*is_mutable=*/false);
    
    interpreter::AstInterpreter interp(env);
    interp.set_source_file(source_file.string());
    
    try {
        auto val = interp.evaluate_program(ast);
        result.success = true;
        result.exit_code = 0;
        // If main() returned an integer, use it as the exit code
        if (val.is<kernel::Integer>()) {
            result.exit_code = static_cast<int>(val.as<kernel::Integer>()->value());
            result.success = (result.exit_code == 0);
        }
    } catch (const interpreter::InterpreterError& e) {
        result.success = false;
        result.exit_code = 1;
        auto err = RuntimeError(e.what(),
                                e.location().file,
                                e.location().line,
                                e.location().column);
        // Capture stack trace
        for (const auto& frame : e.stack_trace()) {
            std::string frame_str = "at " + frame.function_name;
            if (!frame.location.file.empty()) {
                frame_str += " in " + frame.location.file;
            }
            err.stack_trace.push_back(std::move(frame_str));
        }
        result.errors.push_back(std::move(err));
    } catch (const std::exception& e) {
        result.success = false;
        result.exit_code = 1;
        result.errors.emplace_back(e.what(), source_file.string());
    }
    
    return result;
}

std::vector<std::string> MeldRuntime::generate_stack_trace(const std::string& source_code,
                                                         const std::string& error_location) const {
    std::vector<std::string> stack_trace;
    
    // Simplified stack trace generation
    // In real implementation, this would track function calls during execution
    stack_trace.push_back("at main() in " + error_location);
    
    return stack_trace;
}

// InterpreterModule implementation
InterpreterModule::InterpreterModule() 
    : BaseCommandHandler("run", "Execute Meld programs directly or start REPL"),
      runtime_(std::make_unique<MeldRuntime>()),
      file_watcher_(std::make_unique<interpreter::FileWatcher>()) {
}

CommandResult InterpreterModule::execute(const CommandArgs& args) {
    // -i / --interactive with no file → start REPL
    bool interactive = args.flags.count("interactive") > 0 || args.flags.count("i") > 0 ||
                       args.options.count("interactive") > 0;
    if (args.positional.empty() && interactive) {
        start_repl();
        return CommandResult::Success;
    }

    // No file and no -i → show help
    if (args.positional.empty()) {
        std::cout << get_help() << std::endl;
        return CommandResult::Success;
    }

    // Handle run command
    return handle_run_command(args);
}

std::string InterpreterModule::get_help() const {
    return R"(Execute Meld programs via the AST interpreter (Tier 1).

USAGE:
    meld run <file.meld> [args...]     Execute a Meld program
    meld run -i                        Start interactive REPL
    meld run -i <file.meld>            Load file then drop into REPL

OPTIONS:
    --agent-test                       Enable deterministic mode (virtual time, seeded random)
    --debug                            Start DAP server for IDE debugging
    -i, --interactive                  Start REPL (optionally after running a file)
    --isolation=<backend>              Override isolation backend: srt, finch, microvm
    --port <port>                      DAP server port (default 4711)
    --strict                           Delegate to melds production supervisor
    --vfs                              Execute with memory-only VFS (ephemeral writes)
    --vfs-output <path>                Copy VFS output to real filesystem after execution
    --wait                             Halt before first instruction until IDE attaches
    --watch                            Watch file for changes and re-execute

EXAMPLES:
    meld run hello.meld                Execute hello.meld
    meld run hello.meld arg1 arg2      Execute with arguments
    meld run -i                        Start interactive REPL
    meld run -i prelude.meld           Load file then drop into REPL
    meld run --watch server.meld       Re-execute on file saves
    meld run --debug app.meld          Debug with DAP (VS Code/Neovim)
    meld run --debug --wait app.meld   Wait for debugger before starting
    meld run --strict app.bin          Production-grade verification
    meld run --vfs app.meld            Dry run with ephemeral filesystem
    meld run --isolation=microvm app.bin  Run in Firecracker MicroVM)";
}

std::string InterpreterModule::get_usage() const {
    return "meld run <file.meld> [args...]";
}

std::vector<std::string> InterpreterModule::get_completions(const std::string& partial) const {
    std::vector<std::string> completions;
    
    // Add subcommands
    std::vector<std::string> subcommands = {"run", "repl"};
    for (const auto& cmd : subcommands) {
        if (cmd.starts_with(partial)) {
            completions.push_back(cmd);
        }
    }
    
    // Add common flags
    std::vector<std::string> flags = {"--agent-test", "--debug", "-i", "--interactive", "--isolation", "--port", "--strict", "--trace", "--vfs", "--vfs-output", "--wait", "--watch"};
    for (const auto& flag : flags) {
        if (flag.starts_with(partial)) {
            completions.push_back(flag);
        }
    }
    
    return completions;
}

bool InterpreterModule::validate_args(const CommandArgs& args, std::string& error_message) const {
    // Empty positionals is valid — execute() shows help in that case
    if (!args.positional.empty()) {
        std::filesystem::path file_path(args.positional[0]);
        if (!std::filesystem::exists(file_path)) {
            error_message = "error: file not found: " + file_path.string();
            return false;
        }
        
        if (file_path.extension() != ".meld") {
            std::cerr << "warning: file does not have .meld extension: " << file_path.string() << std::endl;
        }
    }
    
    return true;
}

ExecutionResult InterpreterModule::run_file(const std::filesystem::path& file_path,
                                          const std::vector<std::string>& args,
                                          bool watch_mode) {
    // Set up hot reload if requested
    if (watch_mode) {
        setup_hot_reload(file_path, args);
    }
    
    // Execute the file
    return runtime_->execute_file(file_path, args);
}

int InterpreterModule::start_repl() {
    ReplSession session;
    
    // Load history
    auto history_file = get_history_file_path();
    session.load_history(history_file);
    
    print_repl_welcome();
    run_repl_loop(session);
    
    // Save history
    session.save_history(history_file);
    
    return 0;
}

ExecutionResult InterpreterModule::run_source(const std::string& source_code,
                                            const std::vector<std::string>& args) {
    return runtime_->execute_source(source_code, args);
}

CommandResult InterpreterModule::handle_run_command(const CommandArgs& args) {
    std::filesystem::path file_path(args.positional[0]);
    
    // Extract program arguments (skip the file path)
    std::vector<std::string> program_args;
    if (args.positional.size() > 1) {
        program_args.assign(args.positional.begin() + 1, args.positional.end());
    }
    
    // ─── Isolation backend resolution (Req 26.1–26.3) ──────────────
    std::string isolation_backend = resolve_isolation_backend(args);
    if (!is_valid_isolation_backend(isolation_backend)) {
        std::cerr << "error: unrecognized isolation backend '" << isolation_backend
                  << "' — valid backends are: srt, finch, microvm" << std::endl;
        return CommandResult::Error;
    }

    // ─── Strict mode: delegate to melds supervisor (Req 2.17) ──────
    bool strict_mode = args.flags.find("strict") != args.flags.end();
    bool vfs_mode = args.flags.find("vfs") != args.flags.end();

    // ─── Isolation delegation (Req 26.4–26.8) ──────────────────────
    // For finch/microvm backends, always delegate to melds.
    // For srt with --strict, delegate to melds with SRT enforcement.
    // The --isolation flag composes with --strict and --vfs.
    if (isolation_backend == "finch" || isolation_backend == "microvm") {
        return handle_isolation_delegation(isolation_backend, file_path,
                                          program_args, strict_mode, vfs_mode);
    }

    // srt backend: use existing strict/vfs delegation paths when applicable
    if (strict_mode && vfs_mode) {
        // Req 22.7: --vfs + --strict → delegate to melds with VFS enforcement
        return handle_strict_mode_vfs(file_path, program_args);
    }
    if (strict_mode) {
        return handle_strict_mode(file_path, program_args);
    }

    // Check for flags
    bool watch_mode = args.flags.find("watch") != args.flags.end();
    bool debug_mode = args.flags.find("debug") != args.flags.end();
    bool debug_wait = args.flags.find("wait") != args.flags.end() ||
                      args.options.count("wait") > 0;
    bool trace_mode = args.flags.find("trace") != args.flags.end();
    bool agent_test = args.flags.find("agent-test") != args.flags.end();
    bool text_mode = args.flags.count("text") > 0 || args.options.count("text") > 0;
    // Legacy: --json is accepted but is now the default (no-op)
    (void)args.flags.count("json");
    
    // Parse --vfs-output flag (Req 22.4)
    std::string vfs_output_path;
    auto vfs_output_it = args.flags.find("vfs-output");
    if (vfs_output_it != args.flags.end() && !vfs_output_it->second.empty()) {
        vfs_output_path = vfs_output_it->second;
    }

    // ─── VFS Bridge Mode (Req 22.1–22.4) ───────────────────────────
    std::optional<VfsBridge> vfs_bridge;
    std::filesystem::path original_cwd;
    if (vfs_mode) {
        auto bridge = VfsBridge::create();
        if (!bridge) {
            std::cerr << "error: failed to create VFS bridge" << std::endl;
            return CommandResult::Error;
        }
        original_cwd = std::filesystem::current_path();
        runtime_->set_working_directory(bridge->mount_point());
        vfs_bridge = std::move(*bridge);
        std::cerr << "[vfs] Memory-only working directory: "
                  << vfs_bridge->mount_point().string() << std::endl;
    }
    
    // Parse --port flag (default 4711)
    uint16_t dap_port = 4711;
    auto port_it = args.flags.find("port");
    if (port_it != args.flags.end() && !port_it->second.empty()) {
        try {
            dap_port = static_cast<uint16_t>(std::stoi(port_it->second));
        } catch (...) {
            std::cerr << "warning: invalid --port value, using default 4711" << std::endl;
        }
    } else {
        auto opt_it = args.options.find("port");
        if (opt_it != args.options.end() && !opt_it->second.empty()) {
            try {
                dap_port = static_cast<uint16_t>(std::stoi(opt_it->second));
            } catch (...) {
                std::cerr << "warning: invalid --port value, using default 4711" << std::endl;
            }
        }
    }
    
    // Configure runtime
    runtime_->set_debug_mode(debug_mode);
    runtime_->set_trace_mode(trace_mode);
    
    // ─── Agent-Test mode (AI_DX Req 19) ─────────────────────────────
    if (agent_test) {
        effects::AgentTestMode::enable();
        std::cerr << "[agent-test] Deterministic mode active: "
                     "time=virtual, random=seed(0), scheduling=deterministic"
                  << std::endl;
    }
    
    // ─── Daemon handshake for compiled binaries (Req 2.14–2.16) ─────
    // If the target is a compiled binary (not .meld source), perform
    // freshness check and shadow symbol resolution via the daemon.
    std::optional<manifest::MdebugSidecar> loaded_sidecar;
    bool is_compiled_binary = file_path.extension() != ".meld"
                              && std::filesystem::exists(file_path);
    
    if (is_compiled_binary) {
        auto workspace = std::filesystem::current_path();
        DaemonClient daemon(workspace);
        
        // Req 2.14: Check binary freshness — trigger rebuild if stale.
        auto freshness = daemon.check_binary_freshness(file_path);
        if (freshness) {
            if (freshness->rebuild_triggered && !freshness->is_current) {
                std::cerr << "error: rebuild failed for " << file_path.string() << std::endl;
                if (!freshness->build_error.empty()) {
                    std::cerr << freshness->build_error << std::endl;
                }
                return CommandResult::Error;
            }
            if (freshness->rebuild_triggered && freshness->is_current) {
                std::cerr << "[daemon] Binary rebuilt successfully" << std::endl;
            }
        }
        // If daemon not running, skip freshness check silently.
        
        // Req 2.15: Resolve shadow symbols via .mdebug sidecar.
        auto ts = manifest::read_tombstone(file_path);
        if (ts && !ts->debug_id.empty()) {
            auto sidecar_path = daemon.resolve_debug_sidecar(ts->debug_id);
            if (sidecar_path) {
                auto sidecar = manifest::read_mdebug_file(*sidecar_path);
                if (sidecar) {
                    loaded_sidecar = std::move(*sidecar);
                }
            }
            // Also check co-located .mdebug if daemon didn't find it.
            if (!loaded_sidecar) {
                auto colocated = file_path;
                colocated.replace_extension(".mdebug");
                if (std::filesystem::exists(colocated)) {
                    auto sidecar = manifest::read_mdebug_file(colocated);
                    if (sidecar) {
                        loaded_sidecar = std::move(*sidecar);
                    }
                }
            }
        }
    }
    
    // ─── DAP debug mode (Req 2.11, 2.12, 12.2, 12.3, 12.4) ────────
    if (debug_mode) {
        // Read and parse the source file
        std::string source_code;
        try {
            std::ifstream file(file_path);
            if (!file.is_open()) {
                std::cerr << "error: file not found: " << file_path.string() << std::endl;
                return CommandResult::Error;
            }
            std::ostringstream buf;
            buf << file.rdbuf();
            source_code = buf.str();
        } catch (const std::exception& e) {
            std::cerr << "error: " << e.what() << std::endl;
            return CommandResult::Error;
        }
        
        parser::Parser p;
        std::vector<parser::ast::expression> ast;
        if (!p.parse_file(source_code, ast)) {
            std::cerr << file_path.string() << ": error: " << p.error_message() << std::endl;
            return CommandResult::Error;
        }
        
        // Set up interpreter with environment
        auto env = std::make_shared<interpreter::Environment>();
        std::vector<kernel::Value> arg_values;
        for (const auto& a : program_args) {
            arg_values.push_back(kernel::Value(std::make_shared<kernel::String>(a)));
        }
        env->define("args", kernel::Value(std::make_shared<kernel::Vec>(std::move(arg_values))),
                    /*is_mutable=*/false);
        
        interpreter::AstInterpreter interp(env);
        interp.set_source_file(file_path.string());
        
        // Create and start DAP server
        daemon::DapServer dap(interp, dap_port);
        try {
            // wait_for_attach=true blocks until IDE connects (--debug-wait),
            // wait_for_attach=false accepts in background
            dap.start(debug_wait);
        } catch (const std::exception& e) {
            std::cerr << "error: DAP server failed to start: " << e.what() << std::endl;
            return CommandResult::Error;
        }
        
        // Execute the program with debug hooks active
        try {
            auto val = interp.evaluate_program(ast);
            auto str_repr = val.to_string();
            if (!str_repr.empty() && str_repr != "empty") {
                std::cout << str_repr << "\n";
            }
        } catch (const interpreter::InterpreterError& e) {
            std::cerr << e.location().file << ":" << e.location().line
                      << ":" << e.location().column << ": error: " << e.what() << std::endl;
            for (const auto& frame : e.stack_trace()) {
                std::cerr << "  at " << frame.function_name;
                if (!frame.location.file.empty()) {
                    std::cerr << " in " << frame.location.file;
                }
                std::cerr << std::endl;
            }
            dap.stop();
            return CommandResult::Error;
        }
        
        dap.stop();
        return CommandResult::Success;
    }
    
    // ─── Normal (non-debug) execution ───────────────────────────────
    // Capture stdout for JSON envelope
    std::streambuf* old_cout = nullptr;
    std::ostringstream captured_output;
    if (!text_mode) {
        old_cout = std::cout.rdbuf(captured_output.rdbuf());
    }

    ExecutionResult result = run_file(file_path, program_args, watch_mode);

    if (old_cout) std::cout.rdbuf(old_cout);

    // ─── Output formatting ──────────────────────────────────────────
    if (text_mode) {
        // --text: plain output (program already printed to stdout)
        if (!result.success) {
            print_execution_errors(result);
            return CommandResult::Error;
        }
        if (!result.output.empty()) std::cout << result.output;
    } else {
        // Default: JSON envelope
        std::string output_str = captured_output.str();
        auto json_escape = [](const std::string& s) {
            std::string r;
            for (char c : s) {
                if (c == '"') r += "\\\"";
                else if (c == '\\') r += "\\\\";
                else if (c == '\n') r += "\\n";
                else if (c == '\r') r += "\\r";
                else if (c == '\t') r += "\\t";
                else r += c;
            }
            return r;
        };
        std::cout << "{\"success\": " << (result.success ? "true" : "false");
        if (!output_str.empty()) {
            std::cout << ", \"output\": \"" << json_escape(output_str) << "\"";
        }
        if (!result.success && !result.errors.empty()) {
            std::cout << ", \"errors\": [";
            for (size_t i = 0; i < result.errors.size(); ++i) {
                if (i > 0) std::cout << ", ";
                auto& err = result.errors[i];
                std::cout << "{\"message\": \"" << json_escape(err.message) << "\"";
                if (!err.file.empty()) std::cout << ", \"file\": \"" << json_escape(err.file) << "\"";
                if (err.line > 0) std::cout << ", \"line\": " << err.line;
                if (!err.stack_trace.empty()) {
                    std::cout << ", \"stack\": [";
                    for (size_t j = 0; j < err.stack_trace.size(); ++j) {
                        if (j > 0) std::cout << ", ";
                        std::cout << "\"" << json_escape(err.stack_trace[j]) << "\"";
                    }
                    std::cout << "]";
                }
                std::cout << "}";
            }
            std::cout << "]";
        }
        std::cout << ", \"exit_code\": " << result.exit_code << "}" << std::endl;
        if (!result.success) return CommandResult::Error;
    }
    
    // ─── Agent-Test trace output (AI_DX Req 19.4, 19.5) ────────────
    if (agent_test) {
        auto trace_json = effects::AgentTestMode::trace_to_json();
        if (!text_mode) {
            // Write trace to stdout (JSON is default)
            std::cout << trace_json << std::endl;
        } else {
            // Write trace to .meld/traces/run.trace.json
            auto trace_dir = std::filesystem::current_path() / ".meld" / "traces";
            try {
                std::filesystem::create_directories(trace_dir);
                auto trace_path = trace_dir / "run.trace.json";
                std::ofstream trace_file(trace_path);
                if (trace_file) {
                    trace_file << trace_json;
                    std::cerr << "[agent-test] Trace written to "
                              << trace_path.string() << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "[agent-test] Warning: could not write trace: "
                          << e.what() << std::endl;
            }
        }
        effects::AgentTestMode::disable();
    }
    
    if (watch_mode) {
        std::cout << "Watching " << file_path << " for changes. Press Ctrl+C to stop." << std::endl;
        
        // Keep the program running in watch mode
        while (file_watcher_->is_active()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    // ─── VFS output extraction and cleanup (Req 22.4) ──────────────
    if (vfs_bridge) {
        if (!vfs_output_path.empty()) {
            // Copy specified output from tmpfs to real filesystem
            auto dest = original_cwd / vfs_output_path;
            if (vfs_bridge->extract_output(".", dest)) {
                std::cerr << "[vfs] Output extracted to "
                          << dest.string() << std::endl;
            } else {
                std::cerr << "[vfs] Warning: failed to extract output" << std::endl;
            }
        }
        // VfsBridge destructor handles tmpfs cleanup (RAII)
        std::cerr << "[vfs] Discarding ephemeral filesystem" << std::endl;
        vfs_bridge.reset();
    }

    return CommandResult::Success;
}

CommandResult InterpreterModule::handle_strict_mode(
    const std::filesystem::path& binary_path,
    const std::vector<std::string>& extra_args) {

    // Locate the melds binary:
    // 1. Check $PATH via standard lookup
    // 2. Check relative to the meld binary's installation directory
    std::string melds_path;

    // Strategy 1: Search $PATH
    const char* path_env = std::getenv("PATH");
    if (path_env) {
        std::istringstream path_stream(path_env);
        std::string dir;
        while (std::getline(path_stream, dir, ':')) {
            auto candidate = std::filesystem::path(dir) / "melds";
            if (std::filesystem::exists(candidate)) {
                melds_path = candidate.string();
                break;
            }
        }
    }

    // Strategy 2: Check relative to the meld binary (../libexec/melds, same dir)
    if (melds_path.empty()) {
        // /proc/self/exe on Linux, or argv[0] heuristic
        std::error_code ec;
        auto self = std::filesystem::read_symlink("/proc/self/exe", ec);
        if (!ec) {
            auto bin_dir = self.parent_path();
            // Same directory as meld
            auto candidate = bin_dir / "melds";
            if (std::filesystem::exists(candidate)) {
                melds_path = candidate.string();
            }
            // One level up in libexec
            if (melds_path.empty()) {
                candidate = bin_dir.parent_path() / "libexec" / "melds";
                if (std::filesystem::exists(candidate)) {
                    melds_path = candidate.string();
                }
            }
        }
    }

    if (melds_path.empty()) {
        std::cerr << "error: melds binary not found — install the production "
                     "supervisor or add it to $PATH" << std::endl;
        return CommandResult::Error;
    }

    // Build argv: melds <binary_path> [extra_args...]
    std::vector<const char*> exec_argv;
    exec_argv.push_back(melds_path.c_str());
    std::string binary_str = binary_path.string();
    exec_argv.push_back(binary_str.c_str());
    for (const auto& arg : extra_args) {
        exec_argv.push_back(arg.c_str());
    }
    exec_argv.push_back(nullptr);

    // Replace current process with melds
    execvp(melds_path.c_str(), const_cast<char* const*>(exec_argv.data()));

    // If execvp returns, it failed
    std::cerr << "error: failed to execute melds: " << std::strerror(errno) << std::endl;
    return CommandResult::Error;
}

CommandResult InterpreterModule::handle_strict_mode_vfs(
    const std::filesystem::path& binary_path,
    const std::vector<std::string>& extra_args) {

    // Req 22.7: --vfs + --strict → delegate to melds with --vfs flag
    // Locate the melds binary (same logic as handle_strict_mode)
    std::string melds_path;

    const char* path_env = std::getenv("PATH");
    if (path_env) {
        std::istringstream path_stream(path_env);
        std::string dir;
        while (std::getline(path_stream, dir, ':')) {
            auto candidate = std::filesystem::path(dir) / "melds";
            if (std::filesystem::exists(candidate)) {
                melds_path = candidate.string();
                break;
            }
        }
    }

    if (melds_path.empty()) {
        std::error_code ec;
        auto self = std::filesystem::read_symlink("/proc/self/exe", ec);
        if (!ec) {
            auto bin_dir = self.parent_path();
            auto candidate = bin_dir / "melds";
            if (std::filesystem::exists(candidate)) {
                melds_path = candidate.string();
            }
            if (melds_path.empty()) {
                candidate = bin_dir.parent_path() / "libexec" / "melds";
                if (std::filesystem::exists(candidate)) {
                    melds_path = candidate.string();
                }
            }
        }
    }

    if (melds_path.empty()) {
        std::cerr << "error: melds binary not found — install the production "
                     "supervisor or add it to $PATH" << std::endl;
        return CommandResult::Error;
    }

    // Build argv: melds --vfs <binary_path> [extra_args...]
    std::vector<const char*> exec_argv;
    exec_argv.push_back(melds_path.c_str());
    static const char* vfs_flag = "--vfs";
    exec_argv.push_back(vfs_flag);
    std::string binary_str = binary_path.string();
    exec_argv.push_back(binary_str.c_str());
    for (const auto& arg : extra_args) {
        exec_argv.push_back(arg.c_str());
    }
    exec_argv.push_back(nullptr);

    // Replace current process with melds (with VFS enforcement)
    execvp(melds_path.c_str(), const_cast<char* const*>(exec_argv.data()));

    std::cerr << "error: failed to execute melds: " << std::strerror(errno) << std::endl;
    return CommandResult::Error;
}

CommandResult InterpreterModule::handle_repl_command(const CommandArgs& args) {
    start_repl();
    return CommandResult::Success;
}

void InterpreterModule::run_repl_loop(ReplSession& session) {
    std::string multi_line_input;
    
    // Enable linenoise multi-line mode for brace-delimited blocks
    linenoiseSetMultiLine(1);
    
    // Load history into linenoise
    auto history_file = get_history_file_path();
    linenoiseHistoryLoad(history_file.c_str());
    
    while (true) {
        const char* prompt = multi_line_input.empty() ? "meld> " : "... > ";
        char* raw = linenoise(prompt);
        
        if (raw == nullptr) {
            // EOF (Ctrl+D) or error
            std::cout << "\nGoodbye!" << std::endl;
            break;
        }
        
        std::string input(raw);
        linenoiseFree(raw);
        
        // Handle special commands
        if (multi_line_input.empty() && handle_repl_special_command(input, session)) {
            if (!input.empty()) {
                linenoiseHistoryAdd(input.c_str());
            }
            continue;
        }
        
        // Handle multi-line input
        if (!multi_line_input.empty()) {
            multi_line_input += "\n" + input;
            if (session.is_complete_expression(multi_line_input)) {
                input = multi_line_input;
                multi_line_input.clear();
            } else {
                continue;
            }
        } else if (!session.is_complete_expression(input)) {
            multi_line_input = input;
            continue;
        }
        
        // Add to linenoise history
        if (!input.empty()) {
            linenoiseHistoryAdd(input.c_str());
        }
        
        // Evaluate the input
        ReplResult result = session.evaluate_line(input);
        print_repl_result(result);
    }
    
    // Save history
    linenoiseHistorySave(history_file.c_str());
}

void InterpreterModule::print_repl_welcome() const {
    std::cout << "Meld REPL v0.1.0" << std::endl;
    std::cout << "Type :help for help, :quit to exit" << std::endl;
    std::cout << std::endl;
}

void InterpreterModule::print_repl_help() const {
    std::cout << R"(REPL Commands:
  :help                 Show this help
  :quit, :exit          Exit the REPL
  :clear                Clear the session state
  :history              Show command history
  :load <module>        Load a module
  :env                  Show environment variables
  :set <name> <value>   Set environment variable

Meld Language Help:
  val x = 42            Define an immutable variable
  var y = 10            Define a mutable variable
  fnc add(a, b) = a + b Define a function
  x + 10                Evaluate an expression
)" << std::endl;
}

std::string InterpreterModule::get_repl_prompt() const {
    return "meld> ";
}

bool InterpreterModule::handle_repl_special_command(const std::string& input, ReplSession& session) {
    if (input.empty()) {
        return true;
    }
    
    if (input[0] != ':') {
        return false;
    }
    
    std::istringstream iss(input.substr(1));
    std::string command;
    iss >> command;
    
    if (command == "help" || command == "h") {
        print_repl_help();
        return true;
    }
    
    if (command == "quit" || command == "exit" || command == "q") {
        std::cout << "Goodbye!" << std::endl;
        std::exit(0);
    }
    
    if (command == "clear") {
        session.clear();
        std::cout << "Session cleared." << std::endl;
        return true;
    }
    
    if (command == "history") {
        const auto& history = session.get_history();
        for (size_t i = 0; i < history.size(); ++i) {
            std::cout << "  " << (i + 1) << ": " << history[i] << std::endl;
        }
        return true;
    }
    
    if (command == "load") {
        std::string file_path;
        iss >> file_path;
        if (file_path.empty()) {
            std::cout << "Usage: :load <file.meld>" << std::endl;
        } else {
            if (session.load_module(file_path)) {
                std::cout << "Loaded '" << file_path << "'" << std::endl;
            } else {
                std::cout << "Failed to load '" << file_path << "'" << std::endl;
            }
        }
        return true;
    }
    
    if (command == "env") {
        const auto& env = session.get_environment();
        for (const auto& [name, value] : env) {
            std::cout << "  " << name << " = " << value << std::endl;
        }
        return true;
    }
    
    if (command == "set") {
        std::string name, value;
        iss >> name >> value;
        if (name.empty() || value.empty()) {
            std::cout << "Usage: :set <name> <value>" << std::endl;
        } else {
            session.set_environment(name, value);
            std::cout << "Set " << name << " = " << value << std::endl;
        }
        return true;
    }
    
    std::cout << "Unknown command: " << command << std::endl;
    std::cout << "Type :help for available commands." << std::endl;
    return true;
}

void InterpreterModule::setup_hot_reload(const std::filesystem::path& file_path,
                                       const std::vector<std::string>& args) {
    file_watcher_->watch(file_path, [this, file_path, args](const std::filesystem::path&) {
        std::cout << "[watch] reloading " << file_path.string() << "..." << std::endl;
        
        ExecutionResult result = runtime_->execute_file(file_path, args);
        
        if (!result.success) {
            // Print error and continue watching (don't exit)
            print_execution_errors(result);
        } else if (!result.output.empty()) {
            std::cout << result.output;
        }
    });
}

void InterpreterModule::print_execution_errors(const ExecutionResult& result) const {
    for (const auto& error : result.errors) {
        std::cerr << error.format() << std::endl;
    }
}

void InterpreterModule::print_repl_result(const ReplResult& result) const {
    if (!result.success) {
        for (const auto& error : result.errors) {
            std::cerr << "Error: " << error.message << std::endl;
        }
        return;
    }
    
    // Print any output from side effects
    if (!result.output.empty()) {
        std::cout << result.output;
    }
    
    // Print the result value
    if (!result.value.empty()) {
        std::cout << result.value;
        if (!result.type.empty()) {
            std::cout << " : " << result.type;
        }
        std::cout << std::endl;
    }
}

std::filesystem::path InterpreterModule::get_history_file_path() const {
    // Try to use XDG config directory, fall back to home directory
    std::filesystem::path config_dir;
    
    const char* xdg_config = std::getenv("XDG_CONFIG_HOME");
    if (xdg_config) {
        config_dir = std::filesystem::path(xdg_config) / "meld";
    } else {
        const char* home = std::getenv("HOME");
        if (home) {
            config_dir = std::filesystem::path(home) / ".config" / "meld";
        } else {
            config_dir = std::filesystem::current_path();
        }
    }
    
    // Create directory if it doesn't exist
    try {
        std::filesystem::create_directories(config_dir);
    } catch (...) {
        // Fall back to current directory
        config_dir = std::filesystem::current_path();
    }
    
    return config_dir / "repl_history";
}

// ─── Isolation backend helpers (Req 26.1–26.8) ─────────────────────

std::string InterpreterModule::resolve_isolation_backend(const CommandArgs& args) const {
    // Check for --isolation flag (supports both --isolation=<val> and --isolation <val>)
    auto it = args.flags.find("isolation");
    if (it != args.flags.end() && !it->second.empty()) {
        return it->second;
    }
    // Fall back to meld.toml [isolation].local, default: srt
    return read_isolation_from_config();
}

bool InterpreterModule::is_valid_isolation_backend(const std::string& backend) const {
    return backend == "srt" || backend == "finch" || backend == "microvm";
}

std::string InterpreterModule::read_isolation_from_config() const {
    // Read [isolation].local from meld.toml in the current working directory
    auto toml_path = std::filesystem::current_path() / "meld.toml";
    if (!std::filesystem::exists(toml_path)) {
        return "srt";  // default when no config file
    }

    std::ifstream file(toml_path);
    if (!file.is_open()) {
        return "srt";
    }

    // Simple TOML parser for [isolation] section
    std::string line;
    bool in_isolation_section = false;
    while (std::getline(file, line)) {
        // Trim leading whitespace
        auto start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        line = line.substr(start);

        // Check for section headers
        if (line.starts_with("[")) {
            in_isolation_section = (line.find("[isolation]") != std::string::npos);
            continue;
        }

        if (in_isolation_section && line.starts_with("local")) {
            auto eq_pos = line.find('=');
            if (eq_pos != std::string::npos) {
                std::string value = line.substr(eq_pos + 1);
                // Trim whitespace and quotes
                auto vstart = value.find_first_not_of(" \t\"'");
                auto vend = value.find_last_not_of(" \t\"'");
                if (vstart != std::string::npos && vend != std::string::npos) {
                    return value.substr(vstart, vend - vstart + 1);
                }
            }
        }
    }

    return "srt";  // default
}

CommandResult InterpreterModule::handle_isolation_delegation(
    const std::string& backend,
    const std::filesystem::path& binary_path,
    const std::vector<std::string>& extra_args,
    bool strict_mode,
    bool vfs_mode) {

    // Locate the melds binary (same strategy as handle_strict_mode)
    std::string melds_path;

    const char* path_env = std::getenv("PATH");
    if (path_env) {
        std::istringstream path_stream(path_env);
        std::string dir;
        while (std::getline(path_stream, dir, ':')) {
            auto candidate = std::filesystem::path(dir) / "melds";
            if (std::filesystem::exists(candidate)) {
                melds_path = candidate.string();
                break;
            }
        }
    }

    if (melds_path.empty()) {
        std::error_code ec;
        auto self = std::filesystem::read_symlink("/proc/self/exe", ec);
        if (!ec) {
            auto bin_dir = self.parent_path();
            auto candidate = bin_dir / "melds";
            if (std::filesystem::exists(candidate)) {
                melds_path = candidate.string();
            }
            if (melds_path.empty()) {
                candidate = bin_dir.parent_path() / "libexec" / "melds";
                if (std::filesystem::exists(candidate)) {
                    melds_path = candidate.string();
                }
            }
        }
    }

    if (melds_path.empty()) {
        std::cerr << "error: melds binary not found — install the production "
                     "supervisor or add it to $PATH" << std::endl;
        return CommandResult::Error;
    }

    // Build argv: melds [--isolation=<backend>] [--vfs] <binary_path> [extra_args...]
    std::vector<std::string> arg_strings;
    arg_strings.push_back(melds_path);

    // Pass isolation backend to melds
    arg_strings.push_back("--isolation=" + backend);

    // Req 26.8: When combined with --strict, melds applies Sigstore verification
    // (melds always verifies by default; --strict is implicit when delegating)

    // Req 26.7/22.7: When combined with --vfs, pass vfs_mode to the sandbox provider
    if (vfs_mode) {
        arg_strings.push_back("--vfs");
    }

    arg_strings.push_back(binary_path.string());
    for (const auto& arg : extra_args) {
        arg_strings.push_back(arg);
    }

    // Build C-style argv for execvp
    std::vector<const char*> exec_argv;
    for (const auto& s : arg_strings) {
        exec_argv.push_back(s.c_str());
    }
    exec_argv.push_back(nullptr);

    // Replace current process with melds
    execvp(melds_path.c_str(), const_cast<char* const*>(exec_argv.data()));

    // If execvp returns, it failed
    std::cerr << "error: failed to execute melds: " << std::strerror(errno) << std::endl;
    return CommandResult::Error;
}

} // namespace meld::cli