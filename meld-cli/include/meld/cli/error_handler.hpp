#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>

namespace meld::cli {

/**
 * Error categories for CLI operations
 */
enum class CliErrorType {
    CommandNotFound,
    InvalidArguments,
    CompilationFailed,
    RuntimeError,
    ConfigurationError,
    NetworkError,
    InternalError
};

/**
 * Structured error information
 */
struct ErrorReport {
    CliErrorType type;
    std::string code;           // Unique error identifier
    std::string message;        // Human-readable description
    std::optional<std::string> location;  // File/line information if applicable
    std::vector<std::string> suggestions;  // Actionable fix suggestions
    std::optional<std::string> help_url;   // Link to documentation
    std::map<std::string, std::string> context;  // Additional debugging info
};

/**
 * Command not found error with suggestions
 */
struct CommandNotFoundError {
    std::string command;
    std::vector<std::string> suggestions;
};

/**
 * Invalid arguments error with usage information
 */
struct InvalidArgumentsError {
    std::string message;
    std::string usage;
};

/**
 * CLI error handling and reporting system
 */
class ErrorHandler {
public:
    ErrorHandler() = default;
    ~ErrorHandler() = default;

    // Error reporting
    void report_error(const ErrorReport& error);
    void report_command_not_found(const std::string& command, const std::vector<std::string>& suggestions);
    void report_invalid_arguments(const std::string& message, const std::string& usage);
    void report_configuration_error(const std::string& message);
    void report_internal_error(const std::string& message);

    // Error creation helpers
    ErrorReport create_error_report(CliErrorType type, const std::string& code, const std::string& message);
    std::vector<std::string> generate_command_suggestions(const std::string& invalid_command, const std::vector<std::string>& valid_commands);

    // Error formatting
    std::string format_error(const ErrorReport& error);
    void display_error(const ErrorReport& error);

    // Context management
    void add_context(const std::string& key, const std::string& value);
    void clear_context();

private:
    std::map<std::string, std::string> global_context_;
    
    // Helper methods
    std::string get_help_url(CliErrorType type);
    bool should_suggest_fix(CliErrorType type);
    void log_error(const ErrorReport& error);
    double calculate_string_similarity(const std::string& a, const std::string& b);
};

} // namespace meld::cli