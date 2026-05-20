#pragma once

#include "command_handler.hpp"
#include "error_handler.hpp"
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <expected>
#include <chrono>

namespace meld::cli {

/**
 * Code formatting options
 */
struct FormatOptions {
    bool check_only = false;        // Don't modify files, just check formatting
    bool recursive = false;         // Process directories recursively
    int indent_size = 4;           // Number of spaces for indentation
    int max_line_length = 100;     // Maximum line length
    bool use_tabs = false;         // Use tabs instead of spaces
    bool preserve_newlines = true; // Preserve existing newlines where possible
};

/**
 * Linting severity levels
 */
enum class LintSeverity {
    Error,
    Warning,
    Info,
    Hint
};

/**
 * Linting issue information
 */
struct LintIssue {
    std::string message;
    std::string file;
    size_t line = 0;
    size_t column = 0;
    LintSeverity severity = LintSeverity::Warning;
    std::string rule_id;
    std::vector<std::string> suggestions;
    bool auto_fixable = false;
    
    LintIssue(std::string msg, std::string f = "", size_t l = 0, size_t c = 0, 
              LintSeverity sev = LintSeverity::Warning)
        : message(std::move(msg)), file(std::move(f)), line(l), column(c), severity(sev) {}
    
    std::string format() const;
    std::string severity_string() const;
};

/**
 * Linting options
 */
struct LintOptions {
    bool auto_fix = false;          // Apply automatic fixes
    bool recursive = false;         // Process directories recursively
    std::vector<std::string> enabled_rules;  // Specific rules to enable
    std::vector<std::string> disabled_rules; // Specific rules to disable
    LintSeverity min_severity = LintSeverity::Info; // Minimum severity to report
};

/**
 * Linting result
 */
struct LintResult {
    bool success = true;
    std::vector<LintIssue> issues;
    size_t files_processed = 0;
    size_t fixes_applied = 0;
    
    bool has_errors() const;
    bool has_warnings() const;
    size_t error_count() const;
    size_t warning_count() const;
};

/**
 * Formatting result
 */
struct FormatResult {
    bool success = true;
    bool needs_formatting = false;  // True if file needs formatting (check mode)
    std::string formatted_code;     // Formatted code (format mode)
    std::vector<std::string> errors; // Any formatting errors
    size_t files_processed = 0;
    size_t files_changed = 0;
};

/**
 * Debug breakpoint information
 */
struct Breakpoint {
    std::filesystem::path file;
    size_t line;
    std::string condition;  // Optional condition for conditional breakpoints
    bool enabled = true;
    
    Breakpoint(std::filesystem::path f, size_t l, std::string cond = "")
        : file(std::move(f)), line(l), condition(std::move(cond)) {}
};

/**
 * Debug session state
 */
enum class DebugState {
    NotStarted,
    Running,
    Paused,
    Stopped,
    Error
};

/**
 * Debug options for dev tools
 */
struct DevToolsDebugOptions {
    std::vector<Breakpoint> breakpoints;
    bool step_mode = false;         // Start in step mode
    bool trace_execution = false;   // Enable execution tracing
    std::filesystem::path log_file; // Debug log file
};

/**
 * Profiling options
 */
struct ProfileOptions {
    bool cpu_profiling = true;      // Enable CPU profiling
    bool memory_profiling = true;   // Enable memory profiling
    bool trace_calls = false;       // Trace function calls
    std::chrono::seconds duration = std::chrono::seconds(30); // Profiling duration
    std::filesystem::path output_file; // Profile output file
};

/**
 * Performance metrics for dev tools profiling
 */
struct DevToolsPerformanceMetrics {
    std::chrono::nanoseconds execution_time;
    size_t peak_memory_usage = 0;   // Peak memory usage in bytes
    size_t total_allocations = 0;   // Total number of allocations
    size_t function_calls = 0;      // Total function calls
    std::map<std::string, std::chrono::nanoseconds> function_times; // Per-function timing
    std::map<std::string, size_t> allocation_sites; // Allocation hotspots
};

/**
 * Profiling result
 */
struct ProfileResult {
    bool success = true;
    DevToolsPerformanceMetrics metrics;
    std::string report;             // Human-readable performance report
    std::filesystem::path report_file; // Generated report file
    std::vector<std::string> hotspots; // Performance hotspots
    std::vector<std::string> recommendations; // Optimization recommendations
};

/**
 * Code formatter - handles Meld code formatting
 */
class CodeFormatter {
public:
    /**
     * Format Meld source code
     */
    FormatResult format_code(const std::string& source_code, const FormatOptions& options);
    
    /**
     * Format a source file
     */
    FormatResult format_file(const std::filesystem::path& file_path, const FormatOptions& options);
    
    /**
     * Format all files in a directory (optionally recursive)
     */
    FormatResult format_directory(const std::filesystem::path& dir_path, const FormatOptions& options);
    
    /**
     * Check if code needs formatting without modifying it
     */
    bool needs_formatting(const std::string& source_code, const FormatOptions& options);

private:
    std::string apply_indentation(const std::string& code, const FormatOptions& options);
    std::string apply_line_length_limits(const std::string& code, const FormatOptions& options);
    std::string normalize_whitespace(const std::string& code, const FormatOptions& options);
};

/**
 * Code linter - analyzes Meld code for issues
 */
class CodeLinter {
public:
    /**
     * Lint Meld source code
     */
    LintResult lint_code(const std::string& source_code, const std::string& file_path, 
                        const LintOptions& options);
    
    /**
     * Lint a source file
     */
    LintResult lint_file(const std::filesystem::path& file_path, const LintOptions& options);
    
    /**
     * Lint all files in a directory (optionally recursive)
     */
    LintResult lint_directory(const std::filesystem::path& dir_path, const LintOptions& options);
    
    /**
     * Apply automatic fixes to code
     */
    std::string apply_fixes(const std::string& source_code, const std::vector<LintIssue>& issues);

private:
    std::vector<LintIssue> check_syntax_issues(const std::string& code, const std::string& file_path);
    std::vector<LintIssue> check_style_issues(const std::string& code, const std::string& file_path);
    std::vector<LintIssue> check_semantic_issues(const std::string& code, const std::string& file_path);
    bool is_rule_enabled(const std::string& rule_id, const LintOptions& options);
};

/**
 * Debugger - provides debugging support for Meld programs
 */
class Debugger {
public:
    /**
     * Start debugging a Meld program
     */
    bool start_debug_session(const std::filesystem::path& program_path, 
                           const std::vector<std::string>& args,
                           const DevToolsDebugOptions& options);
    
    /**
     * Stop the current debug session
     */
    void stop_debug_session();
    
    /**
     * Add a breakpoint
     */
    bool add_breakpoint(const Breakpoint& breakpoint);
    
    /**
     * Remove a breakpoint
     */
    bool remove_breakpoint(const std::filesystem::path& file, size_t line);
    
    /**
     * Step to next line
     */
    bool step_next();
    
    /**
     * Step into function
     */
    bool step_into();
    
    /**
     * Step out of function
     */
    bool step_out();
    
    /**
     * Continue execution
     */
    bool continue_execution();
    
    /**
     * Get current debug state
     */
    DebugState get_state() const { return state_; }
    
    /**
     * Get variable values at current location
     */
    std::map<std::string, std::string> get_variables();
    
    /**
     * Evaluate expression in current context
     */
    std::string evaluate_expression(const std::string& expression);

private:
    DebugState state_ = DebugState::NotStarted;
    std::vector<Breakpoint> breakpoints_;
    std::filesystem::path current_file_;
    size_t current_line_ = 0;
};

/**
 * Profiler - provides performance profiling for Meld programs
 */
class Profiler {
public:
    /**
     * Start profiling a Meld program
     */
    ProfileResult profile_program(const std::filesystem::path& program_path,
                                const std::vector<std::string>& args,
                                const ProfileOptions& options);
    
    /**
     * Generate performance report from metrics
     */
    std::string generate_report(const DevToolsPerformanceMetrics& metrics);
    
    /**
     * Analyze performance hotspots
     */
    std::vector<std::string> analyze_hotspots(const DevToolsPerformanceMetrics& metrics);
    
    /**
     * Generate optimization recommendations
     */
    std::vector<std::string> generate_recommendations(const DevToolsPerformanceMetrics& metrics);

private:
    DevToolsPerformanceMetrics collect_metrics(const std::filesystem::path& program_path,
                                     const std::vector<std::string>& args,
                                     const ProfileOptions& options);
    void write_profile_data(const DevToolsPerformanceMetrics& metrics, 
                           const std::filesystem::path& output_file);
};

/**
 * Development tools module - handles formatting, linting, debugging, and profiling
 */
class DevToolsModule : public BaseCommandHandler {
public:
    DevToolsModule();
    ~DevToolsModule() override = default;
    
    // CommandHandler interface
    CommandResult execute(const CommandArgs& args) override;
    std::string get_help() const override;
    std::string get_usage() const override;
    std::vector<std::string> get_completions(const std::string& partial) const override;
    bool validate_args(const CommandArgs& args, std::string& error_message) const override;
    
    /**
     * Handle format command
     */
    CommandResult handle_format_command(const CommandArgs& args);
    
    /**
     * Handle lint command
     */
    CommandResult handle_lint_command(const CommandArgs& args);
    
    /**
     * Handle debug command
     */
    CommandResult handle_debug_command(const CommandArgs& args);
    
    /**
     * Handle profile command
     */
    CommandResult handle_profile_command(const CommandArgs& args);

private:
    std::unique_ptr<CodeFormatter> formatter_;
    std::unique_ptr<CodeLinter> linter_;
    std::unique_ptr<Debugger> debugger_;
    std::unique_ptr<Profiler> profiler_;
    
    // Helper methods
    FormatOptions parse_format_options(const CommandArgs& args) const;
    LintOptions parse_lint_options(const CommandArgs& args) const;
    DevToolsDebugOptions parse_debug_options(const CommandArgs& args) const;
    ProfileOptions parse_profile_options(const CommandArgs& args) const;
    
    void print_format_result(const FormatResult& result) const;
    void print_lint_result(const LintResult& result) const;
    void print_profile_result(const ProfileResult& result) const;
    
    std::vector<std::filesystem::path> collect_meld_files(const std::filesystem::path& path, 
                                                         bool recursive) const;
    bool is_meld_file(const std::filesystem::path& file) const;
};

} // namespace meld::cli