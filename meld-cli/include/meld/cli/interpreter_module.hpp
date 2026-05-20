#pragma once

#include "meld/cli/command_handler.hpp"
#include "meld/cli/error_handler.hpp"
#include "meld/cli/vfs_bridge.hpp"
#include "meld/interpreter/ast_interpreter.hpp"
#include "meld/daemon/dap_channel.hpp"
#include "meld/interpreter/file_watcher.hpp"
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <expected>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>

namespace meld::cli {

/**
 * Runtime error information
 */
struct RuntimeError {
    std::string message;
    std::string file;
    size_t line = 0;
    size_t column = 0;
    std::vector<std::string> stack_trace;
    std::string context;
    
    RuntimeError(std::string msg, std::string f = "", size_t l = 0, size_t c = 0)
        : message(std::move(msg)), file(std::move(f)), line(l), column(c) {}
    
    std::string format() const;
};

/**
 * Execution result
 */
struct ExecutionResult {
    bool success = false;
    int exit_code = 0;
    std::string output;
    std::string error_output;
    std::vector<RuntimeError> errors;
    
    bool has_errors() const { return !errors.empty(); }
};

/**
 * REPL evaluation result
 */
struct ReplResult {
    bool success = false;
    std::string value;
    std::string type;
    std::string output;
    std::vector<RuntimeError> errors;
    
    bool has_errors() const { return !errors.empty(); }
};

/**
 * REPL session state
 */
class ReplSession {
public:
    ReplSession();
    ~ReplSession() = default;
    
    /**
     * Evaluate a line of input in the REPL
     */
    ReplResult evaluate_line(const std::string& input);
    
    /**
     * Load a module into the REPL environment
     */
    bool load_module(const std::string& module_name);
    
    /**
     * Get available completions for partial input
     */
    std::vector<std::string> get_completions(const std::string& partial) const;
    
    /**
     * Get command history
     */
    const std::vector<std::string>& get_history() const { return history_; }
    
    /**
     * Add command to history
     */
    void add_to_history(const std::string& command);
    
    /**
     * Save history to file
     */
    bool save_history(const std::filesystem::path& history_file) const;
    
    /**
     * Load history from file
     */
    bool load_history(const std::filesystem::path& history_file);
    
    /**
     * Clear the session state
     */
    void clear();
    
    /**
     * Get current environment variables
     */
    const std::map<std::string, std::string>& get_environment() const { return environment_; }
    
    /**
     * Set environment variable
     */
    void set_environment(const std::string& name, const std::string& value);

    // Helper methods
    bool is_complete_expression(const std::string& input) const;

private:
    std::map<std::string, std::string> environment_;
    std::vector<std::string> loaded_modules_;
    std::vector<std::string> history_;
    size_t max_history_size_ = 1000;
    
    // Interpreter state — persistent across REPL lines
    std::shared_ptr<interpreter::Environment> interp_env_;
    std::unique_ptr<interpreter::AstInterpreter> interp_;
    
    std::string format_value(const kernel::Value& value) const;
    std::string value_type_name(const kernel::Value& value) const;
};

/**
 * File watcher for hot reload functionality
 */
class FileWatcher {
public:
    using ChangeCallback = std::function<void(const std::filesystem::path&)>;
    
    FileWatcher();
    ~FileWatcher();
    
    /**
     * Start watching a file for changes
     */
    bool watch_file(const std::filesystem::path& file_path, ChangeCallback callback);
    
    /**
     * Stop watching a file
     */
    void stop_watching(const std::filesystem::path& file_path);
    
    /**
     * Stop watching all files
     */
    void stop_all();
    
    /**
     * Check if currently watching any files
     */
    bool is_watching() const { return !watched_files_.empty(); }

private:
    struct WatchedFile {
        std::filesystem::path path;
        std::filesystem::file_time_type last_write_time;
        ChangeCallback callback;
    };
    
    std::map<std::filesystem::path, WatchedFile> watched_files_;
    std::unique_ptr<std::thread> watcher_thread_;
    std::atomic<bool> should_stop_{false};
    std::mutex files_mutex_;
    
    void watch_loop();
};

/**
 * Meld runtime environment
 */
class MeldRuntime {
public:
    MeldRuntime();
    ~MeldRuntime() = default;
    
    /**
     * Execute a Meld program from file
     */
    ExecutionResult execute_file(const std::filesystem::path& file_path,
                                const std::vector<std::string>& args = {});
    
    /**
     * Execute Meld source code directly
     */
    ExecutionResult execute_source(const std::string& source_code,
                                  const std::vector<std::string>& args = {});
    
    /**
     * Find and validate main function in source
     */
    bool has_main_function(const std::string& source_code) const;
    
    /**
     * Parse Meld source code
     */
    std::expected<std::string, RuntimeError> parse_source(const std::string& source_code) const;
    
    /**
     * Set runtime options
     */
    void set_debug_mode(bool enabled) { debug_mode_ = enabled; }
    void set_trace_mode(bool enabled) { trace_mode_ = enabled; }
    void set_working_directory(const std::filesystem::path& path) { working_dir_ = path; }
    
    /**
     * Get runtime information
     */
    bool is_debug_mode() const { return debug_mode_; }
    bool is_trace_mode() const { return trace_mode_; }
    const std::filesystem::path& get_working_directory() const { return working_dir_; }

private:
    bool debug_mode_ = false;
    bool trace_mode_ = false;
    std::filesystem::path working_dir_;
    
    // Helper methods
    std::string read_source_file(const std::filesystem::path& file_path) const;
    ExecutionResult execute_internal(const std::string& source_code,
                                   const std::vector<std::string>& args,
                                   const std::filesystem::path& source_file = {}) const;
    std::vector<std::string> generate_stack_trace(const std::string& source_code,
                                                 const std::string& error_location) const;
};

/**
 * Interpreter module - handles direct execution and REPL
 */
class InterpreterModule : public BaseCommandHandler {
public:
    InterpreterModule();
    ~InterpreterModule() override = default;
    
    // CommandHandler interface
    CommandResult execute(const CommandArgs& args) override;
    std::string get_help() const override;
    std::string get_usage() const override;
    std::vector<std::string> get_completions(const std::string& partial) const override;
    bool validate_args(const CommandArgs& args, std::string& error_message) const override;
    
    /**
     * Execute a Meld file directly
     */
    ExecutionResult run_file(const std::filesystem::path& file_path,
                           const std::vector<std::string>& args = {},
                           bool watch_mode = false);
    
    /**
     * Start an interactive REPL session
     */
    int start_repl();
    
    /**
     * Execute source code directly
     */
    ExecutionResult run_source(const std::string& source_code,
                             const std::vector<std::string>& args = {});

private:
    std::unique_ptr<MeldRuntime> runtime_;
    std::unique_ptr<interpreter::FileWatcher> file_watcher_;
    
    // Command handlers
    CommandResult handle_run_command(const CommandArgs& args);
    CommandResult handle_strict_mode(const std::filesystem::path& binary_path,
                                    const std::vector<std::string>& extra_args);
    CommandResult handle_strict_mode_vfs(const std::filesystem::path& binary_path,
                                        const std::vector<std::string>& extra_args);
    CommandResult handle_isolation_delegation(const std::string& backend,
                                             const std::filesystem::path& binary_path,
                                             const std::vector<std::string>& extra_args,
                                             bool strict_mode,
                                             bool vfs_mode);
    CommandResult handle_repl_command(const CommandArgs& args);
    
    // Isolation backend helpers
    std::string resolve_isolation_backend(const CommandArgs& args) const;
    bool is_valid_isolation_backend(const std::string& backend) const;
    std::string read_isolation_from_config() const;
    
    // REPL implementation
    void run_repl_loop(ReplSession& session);
    void print_repl_welcome() const;
    void print_repl_help() const;
    std::string get_repl_prompt() const;
    bool handle_repl_special_command(const std::string& input, ReplSession& session);
    
    // Hot reload implementation
    void setup_hot_reload(const std::filesystem::path& file_path,
                         const std::vector<std::string>& args);
    
    // Helper methods
    void print_execution_errors(const ExecutionResult& result) const;
    void print_repl_result(const ReplResult& result) const;
    std::filesystem::path get_history_file_path() const;
};

} // namespace meld::cli