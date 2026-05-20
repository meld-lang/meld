#include "meld/cli/error_handler.hpp"
#include <iostream>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace meld::cli {

void ErrorHandler::report_error(const ErrorReport& error) {
    display_error(error);
    log_error(error);
}

void ErrorHandler::report_command_not_found(const std::string& command, const std::vector<std::string>& suggestions) {
    ErrorReport error = create_error_report(
        CliErrorType::CommandNotFound,
        "CLI001",
        "Command '" + command + "' not found"
    );
    error.suggestions = suggestions;
    report_error(error);
}

void ErrorHandler::report_invalid_arguments(const std::string& message, const std::string& usage) {
    ErrorReport error = create_error_report(
        CliErrorType::InvalidArguments,
        "CLI002",
        message
    );
    error.suggestions.push_back("Usage: " + usage);
    report_error(error);
}

void ErrorHandler::report_configuration_error(const std::string& message) {
    ErrorReport error = create_error_report(
        CliErrorType::ConfigurationError,
        "CLI003",
        message
    );
    report_error(error);
}

void ErrorHandler::report_internal_error(const std::string& message) {
    ErrorReport error = create_error_report(
        CliErrorType::InternalError,
        "CLI999",
        "Internal error: " + message
    );
    error.suggestions.push_back("This is likely a bug. Please report it to the developers.");
    report_error(error);
}

ErrorReport ErrorHandler::create_error_report(CliErrorType type, const std::string& code, const std::string& message) {
    ErrorReport error;
    error.type = type;
    error.code = code;
    error.message = message;
    error.help_url = get_help_url(type);
    error.context = global_context_;
    return error;
}

std::vector<std::string> ErrorHandler::generate_command_suggestions(const std::string& invalid_command, const std::vector<std::string>& valid_commands) {
    std::vector<std::pair<std::string, double>> scored_commands;
    
    for (const auto& command : valid_commands) {
        double similarity = calculate_string_similarity(invalid_command, command);
        
        // Boost score for commands that start with the same letter
        if (!invalid_command.empty() && !command.empty() && 
            std::tolower(invalid_command[0]) == std::tolower(command[0])) {
            similarity += 0.1;
        }
        
        // Boost score for commands that contain the invalid command as substring
        if (command.find(invalid_command) != std::string::npos) {
            similarity += 0.2;
        }
        
        // Only suggest commands with reasonable similarity
        if (similarity > 0.3) {
            scored_commands.emplace_back(command, similarity);
        }
    }
    
    // Sort by similarity score (descending)
    std::sort(scored_commands.begin(), scored_commands.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    // Return top 3 suggestions
    std::vector<std::string> suggestions;
    for (size_t i = 0; i < std::min(size_t(3), scored_commands.size()); ++i) {
        suggestions.push_back(scored_commands[i].first);
    }
    
    return suggestions;
}

std::string ErrorHandler::format_error(const ErrorReport& error) {
    std::ostringstream oss;
    
    // Error header
    oss << "Error [" << error.code << "]: " << error.message << std::endl;
    
    // Location if available
    if (error.location.has_value()) {
        oss << "  at " << error.location.value() << std::endl;
    }
    
    // Suggestions
    if (!error.suggestions.empty()) {
        oss << std::endl << "Suggestions:" << std::endl;
        for (const auto& suggestion : error.suggestions) {
            oss << "  • " << suggestion << std::endl;
        }
    }
    
    // Help URL
    if (error.help_url.has_value()) {
        oss << std::endl << "For more help: " << error.help_url.value() << std::endl;
    }
    
    return oss.str();
}

void ErrorHandler::display_error(const ErrorReport& error) {
    std::cerr << format_error(error);
}

void ErrorHandler::add_context(const std::string& key, const std::string& value) {
    global_context_[key] = value;
}

void ErrorHandler::clear_context() {
    global_context_.clear();
}

std::string ErrorHandler::get_help_url(CliErrorType type) {
    switch (type) {
        case CliErrorType::CommandNotFound:
            return "https://meld-lang.org/docs/cli/commands";
        case CliErrorType::InvalidArguments:
            return "https://meld-lang.org/docs/cli/usage";
        case CliErrorType::ConfigurationError:
            return "https://meld-lang.org/docs/cli/configuration";
        default:
            return "https://meld-lang.org/docs/cli/troubleshooting";
    }
}

bool ErrorHandler::should_suggest_fix(CliErrorType type) {
    return type == CliErrorType::CommandNotFound || 
           type == CliErrorType::InvalidArguments ||
           type == CliErrorType::ConfigurationError;
}

void ErrorHandler::log_error(const ErrorReport& error) {
    // TODO: Implement proper logging when logging system is available
    // For now, just output to stderr
}

double ErrorHandler::calculate_string_similarity(const std::string& a, const std::string& b) {
    // Simple Levenshtein distance-based similarity
    if (a.empty() || b.empty()) {
        return 0.0;
    }
    
    const size_t len_a = a.length();
    const size_t len_b = b.length();
    
    std::vector<std::vector<size_t>> matrix(len_a + 1, std::vector<size_t>(len_b + 1));
    
    for (size_t i = 0; i <= len_a; ++i) {
        matrix[i][0] = i;
    }
    for (size_t j = 0; j <= len_b; ++j) {
        matrix[0][j] = j;
    }
    
    for (size_t i = 1; i <= len_a; ++i) {
        for (size_t j = 1; j <= len_b; ++j) {
            size_t cost = (a[i-1] == b[j-1]) ? 0 : 1;
            matrix[i][j] = std::min({
                matrix[i-1][j] + 1,      // deletion
                matrix[i][j-1] + 1,      // insertion
                matrix[i-1][j-1] + cost  // substitution
            });
        }
    }
    
    size_t distance = matrix[len_a][len_b];
    size_t max_len = std::max(len_a, len_b);
    
    return 1.0 - (static_cast<double>(distance) / max_len);
}

} // namespace meld::cli