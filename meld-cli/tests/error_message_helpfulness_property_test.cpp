#include <gtest/gtest.h>
#include "meld/cli/error_handler.hpp"
#include "meld/testing/property_test.hpp"
#include <memory>
#include <vector>
#include <string>
#include <random>
#include <iostream>
#include <sstream>
#include <iomanip>

using namespace meld::cli;
using namespace meld::testing;

/**
 * **Feature: meld-cli, Property 22: Error Message Helpfulness**
 * **Validates: Requirements 7.6**
 * 
 * Property: For any error condition, the CLI should provide error messages 
 * that include context, specific error locations, and actionable suggestions
 */

// Generator for error types
std::function<CliErrorType()> error_type_generator() {
    static std::vector<CliErrorType> types = {
        CliErrorType::CommandNotFound,
        CliErrorType::InvalidArguments,
        CliErrorType::CompilationFailed,
        CliErrorType::RuntimeError,
        CliErrorType::ConfigurationError,
        CliErrorType::NetworkError,
        CliErrorType::InternalError
    };
    
    return []() {
        static std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<size_t> dist(0, types.size() - 1);
        return types[dist(gen)];
    };
}

// Generator for error messages
std::function<std::string()> error_message_generator() {
    static std::vector<std::string> messages = {
        "Command not found",
        "Invalid argument provided",
        "Compilation failed with syntax error",
        "Runtime exception occurred",
        "Configuration file is malformed",
        "Network connection failed",
        "Internal error in command processing"
    };
    
    return []() {
        static std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<size_t> dist(0, messages.size() - 1);
        return messages[dist(gen)];
    };
}

// Generator for error codes
std::function<std::string()> error_code_generator() {
    return []() {
        static std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<int> code_dist(1, 999);
        
        std::ostringstream oss;
        oss << "CLI" << std::setfill('0') << std::setw(3) << code_dist(gen);
        return oss.str();
    };
}

// Generator for command names (for command not found errors)
std::function<std::string()> command_name_generator() {
    static std::vector<std::string> commands = {
        "compil", "rn", "tst", "bild", "formatt", "lin", "instal", "hlp"
    };
    
    return []() {
        static std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<size_t> dist(0, commands.size() - 1);
        return commands[dist(gen)];
    };
}

// Generator for valid commands (for suggestions)
std::function<std::vector<std::string>()> suggestion_generator() {
    static std::vector<std::vector<std::string>> suggestion_sets = {
        {"compile", "complete"},
        {"run", "rerun"},
        {"test", "rest"},
        {"build", "rebuild"},
        {"format", "reform"},
        {"lint", "hint"},
        {"install", "uninstall"},
        {"help", "helm"}
    };
    
    return []() {
        static std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<size_t> dist(0, suggestion_sets.size() - 1);
        return suggestion_sets[dist(gen)];
    };
}

TEST(ErrorMessageHelpfulnessPropertyTest, ErrorReportStructure) {
    ErrorHandler error_handler;
    
    // Property: For any error report, it should contain all required fields
    bool property_holds = PropertyTest::forall(
        error_type_generator(),
        error_code_generator(),
        error_message_generator(),
        [&](CliErrorType type, const std::string& code, const std::string& message) {
            
            ErrorReport error = error_handler.create_error_report(type, code, message);
            
            // Error should have the correct type
            if (error.type != type) {
                std::cerr << "Error type mismatch" << std::endl;
                return false;
            }
            
            // Error should have the provided code
            if (error.code != code) {
                std::cerr << "Error code mismatch" << std::endl;
                return false;
            }
            
            // Error should have the provided message
            if (error.message != message) {
                std::cerr << "Error message mismatch" << std::endl;
                return false;
            }
            
            // Error should have a help URL
            if (!error.help_url.has_value() || error.help_url.value().empty()) {
                std::cerr << "Error missing help URL" << std::endl;
                return false;
            }
            
            // Help URL should be a valid URL format
            std::string url = error.help_url.value();
            if (url.find("https://") != 0 && url.find("http://") != 0) {
                std::cerr << "Invalid help URL format: " << url << std::endl;
                return false;
            }
            
            return true;
        },
        100
    );
    
    EXPECT_TRUE(property_holds);
}

TEST(ErrorMessageHelpfulnessPropertyTest, CommandNotFoundErrorHelpfulness) {
    ErrorHandler error_handler;
    
    // Property: Command not found errors should provide helpful suggestions
    bool property_holds = PropertyTest::forall(
        command_name_generator(),
        suggestion_generator(),
        [&](const std::string& invalid_command, const std::vector<std::string>& suggestions) {
            
            // Test command not found error reporting
            error_handler.report_command_not_found(invalid_command, suggestions);
            
            // Create error report to test structure
            ErrorReport error = error_handler.create_error_report(
                CliErrorType::CommandNotFound,
                "CLI001",
                "Command '" + invalid_command + "' not found"
            );
            error.suggestions = suggestions;
            
            // Format the error message
            std::string formatted = error_handler.format_error(error);
            
            // Error message should contain the invalid command
            if (formatted.find(invalid_command) == std::string::npos) {
                std::cerr << "Error message doesn't contain invalid command" << std::endl;
                return false;
            }
            
            // Error message should contain error code
            if (formatted.find("CLI001") == std::string::npos) {
                std::cerr << "Error message doesn't contain error code" << std::endl;
                return false;
            }
            
            // If suggestions are provided, they should appear in the formatted message
            if (!suggestions.empty()) {
                bool found_suggestions = false;
                for (const auto& suggestion : suggestions) {
                    if (formatted.find(suggestion) != std::string::npos) {
                        found_suggestions = true;
                        break;
                    }
                }
                if (!found_suggestions) {
                    std::cerr << "Error message doesn't contain suggestions" << std::endl;
                    return false;
                }
            }
            
            // Error message should contain "Suggestions:" section if suggestions exist
            if (!suggestions.empty() && formatted.find("Suggestions:") == std::string::npos) {
                std::cerr << "Error message missing suggestions section" << std::endl;
                return false;
            }
            
            return true;
        },
        100
    );
    
    EXPECT_TRUE(property_holds);
}

TEST(ErrorMessageHelpfulnessPropertyTest, InvalidArgumentsErrorHelpfulness) {
    ErrorHandler error_handler;
    
    // Generator for usage strings
    std::function<std::string()> usage_generator = []() {
        static std::vector<std::string> usages = {
            "meld compile [options] <file>",
            "meld run [options] <file>",
            "meld new <project-name>",
            "meld build [options]",
            "meld test [options]"
        };
        
        static std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<size_t> dist(0, usages.size() - 1);
        return usages[dist(gen)];
    };
    
    // Property: Invalid arguments errors should provide usage information
    bool property_holds = PropertyTest::forall(
        error_message_generator(),
        usage_generator,
        [&](const std::string& message, const std::string& usage) {
            
            // Test invalid arguments error reporting
            error_handler.report_invalid_arguments(message, usage);
            
            // Create error report to test structure
            ErrorReport error = error_handler.create_error_report(
                CliErrorType::InvalidArguments,
                "CLI002",
                message
            );
            error.suggestions.push_back("Usage: " + usage);
            
            // Format the error message
            std::string formatted = error_handler.format_error(error);
            
            // Error message should contain the original message
            if (formatted.find(message) == std::string::npos) {
                std::cerr << "Error message doesn't contain original message" << std::endl;
                return false;
            }
            
            // Error message should contain usage information
            if (formatted.find(usage) == std::string::npos) {
                std::cerr << "Error message doesn't contain usage information" << std::endl;
                return false;
            }
            
            // Error message should contain "Usage:" prefix
            if (formatted.find("Usage:") == std::string::npos) {
                std::cerr << "Error message missing usage prefix" << std::endl;
                return false;
            }
            
            return true;
        },
        100
    );
    
    EXPECT_TRUE(property_holds);
}

TEST(ErrorMessageHelpfulnessPropertyTest, ErrorMessageFormatConsistency) {
    ErrorHandler error_handler;
    
    // Property: All error messages should follow consistent formatting
    bool property_holds = PropertyTest::forall(
        error_type_generator(),
        error_code_generator(),
        error_message_generator(),
        [&](CliErrorType type, const std::string& code, const std::string& message) {
            
            ErrorReport error = error_handler.create_error_report(type, code, message);
            std::string formatted = error_handler.format_error(error);
            
            // Should start with "Error [CODE]:"
            std::string expected_prefix = "Error [" + code + "]:";
            if (formatted.find(expected_prefix) != 0) {
                std::cerr << "Error message doesn't start with expected prefix" << std::endl;
                return false;
            }
            
            // Should contain the message
            if (formatted.find(message) == std::string::npos) {
                std::cerr << "Error message doesn't contain the message" << std::endl;
                return false;
            }
            
            // Should end with newline
            if (formatted.empty() || formatted.back() != '\n') {
                std::cerr << "Error message doesn't end with newline" << std::endl;
                return false;
            }
            
            // Should not be empty
            if (formatted.empty()) {
                std::cerr << "Error message is empty" << std::endl;
                return false;
            }
            
            return true;
        },
        100
    );
    
    EXPECT_TRUE(property_holds);
}

TEST(ErrorMessageHelpfulnessPropertyTest, ContextInformation) {
    ErrorHandler error_handler;
    
    // Add some context
    error_handler.add_context("app_name", "meld");
    error_handler.add_context("version", "0.1.0");
    error_handler.add_context("command", "compile");
    
    // Property: Error reports should include context information
    bool property_holds = PropertyTest::forall(
        error_type_generator(),
        error_code_generator(),
        error_message_generator(),
        [&](CliErrorType type, const std::string& code, const std::string& message) {
            
            ErrorReport error = error_handler.create_error_report(type, code, message);
            
            // Error should include context
            if (error.context.empty()) {
                std::cerr << "Error report missing context" << std::endl;
                return false;
            }
            
            // Should include the context we added
            if (error.context.find("app_name") == error.context.end()) {
                std::cerr << "Error report missing app_name context" << std::endl;
                return false;
            }
            
            if (error.context.find("version") == error.context.end()) {
                std::cerr << "Error report missing version context" << std::endl;
                return false;
            }
            
            return true;
        },
        100
    );
    
    EXPECT_TRUE(property_holds);
}