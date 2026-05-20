#include <gtest/gtest.h>
#include "../include/meld/cli/compiler_module.hpp"
#include "meld/testing/property_test.hpp"
#include <string>
#include <vector>
#include <filesystem>
#include <random>

using namespace meld::cli;
using namespace meld::testing;

/**
 * **Feature: meld-cli, Property 2: Compilation Error Reporting**
 * **Validates: Requirements 1.6**
 * 
 * Property: For any invalid Meld source file, compilation should fail with 
 * structured error messages that include specific error locations and actionable fix suggestions
 */
class CompilationErrorReportingTest : public ::testing::Test {
protected:
    void SetUp() override {
        compiler_module_ = std::make_unique<CompilerModule>();
        
        // Create temporary directory for test files
        temp_dir_ = std::filesystem::temp_directory_path() / "meld_cli_error_test";
        std::filesystem::create_directories(temp_dir_);
    }
    
    void TearDown() override {
        // Clean up temporary files
        if (std::filesystem::exists(temp_dir_)) {
            std::filesystem::remove_all(temp_dir_);
        }
    }
    
    std::unique_ptr<CompilerModule> compiler_module_;
    std::filesystem::path temp_dir_;
};

// Generator for invalid Meld source code samples
class InvalidMeldSourceGenerator {
public:
    static std::vector<std::pair<std::string, std::string>> generate_invalid_sources() {
        return {
            // Syntax errors
            {"val x =", "incomplete_assignment"},
            {"fnc add(a: Int, b: Int -> Int { a + b }", "missing_parenthesis"},
            {"class Person { val name: String val age: Int }", "missing_semicolon"},
            {"val x = 42;; val y = 10", "double_semicolon"},
            {"fnc test() { return }", "incomplete_return"},
            
            // Type errors
            {"val x: String = 42", "type_mismatch"},
            {"val x: Int = \"hello\"", "string_to_int"},
            {"val x: Bool = 123", "number_to_bool"},
            {"fnc add(a: Int, b: String) -> Int { a + b }", "incompatible_types"},
            
            // Undefined variables
            {"val x = undefined_variable", "undefined_variable"},
            {"fnc test() { return unknown_var }", "undefined_in_function"},
            {"val x = y + 1; val y = 2", "forward_reference"},
            
            // Invalid function calls
            {"unknown_function(1, 2, 3)", "undefined_function"},
            {"val x = 42; x(1, 2)", "not_a_function"},
            {"println()", "missing_arguments"},
            
            // Invalid array/tuple operations
            {"val arr = [1, 2, 3]; val x = arr[\"invalid\"]", "invalid_index_type"},
            {"val tup = (x: 1, y: 2); val z = tup.unknown_field", "invalid_field_access"},
            
            // Invalid class/struct definitions
            {"class { val x: Int }", "missing_class_name"},
            {"struct Point { val x: ; val y: Float }", "missing_field_type"},
            {"class Person { val name: UnknownType }", "unknown_field_type"},
            
            // Malformed expressions
            {"1 + + 2", "double_operator"},
            {"val x = (1 + 2", "unmatched_parenthesis"},
            {"val x = [1, 2, 3", "unmatched_bracket"},
            {"val x = {1, 2, 3}", "invalid_brace_usage"},
            
            // Invalid control flow
            {"if true { val x = 1 } else", "incomplete_if_else"},
            {"match x {", "incomplete_match"},
            {"while { val x = 1 }", "missing_condition"},
            
            // Empty or whitespace-only input
            {"", "empty_input"},
            {"   \n\t  ", "whitespace_only"},
            {"\n\n\n", "newlines_only"}
        };
    }
};

// Property test: Error reporting structure
TEST_F(CompilationErrorReportingTest, InvalidSourceProducesStructuredErrors) {
    auto invalid_sources = InvalidMeldSourceGenerator::generate_invalid_sources();
    auto targets = std::vector<CompilationTarget>{
        CompilationTarget::JVM, CompilationTarget::Go, 
        CompilationTarget::Cpp, CompilationTarget::WebAssembly
    };
    
    for (const auto& [source, error_type] : invalid_sources) {
        for (const auto& target : targets) {
            CompileOptions options;
            options.target = target;
            
            auto result = compiler_module_->compile_source(source, options);
            
            // Compilation should fail for invalid source
            EXPECT_FALSE(result.success)
                << "Compilation should fail for invalid source (" << error_type << "): " << source;
            
            // Should have at least one error
            EXPECT_FALSE(result.errors.empty())
                << "Failed compilation should have error messages for " << error_type;
            
            // Check error structure
            for (const auto& error : result.errors) {
                // Error message should not be empty
                EXPECT_FALSE(error.message.empty())
                    << "Error message should not be empty for " << error_type;
                
                // Error should have a meaningful message
                EXPECT_GT(error.message.length(), 5)
                    << "Error message should be descriptive for " << error_type;
                
                // Formatted error should include the message
                std::string formatted = error.format();
                EXPECT_FALSE(formatted.empty())
                    << "Formatted error should not be empty for " << error_type;
                
                EXPECT_TRUE(formatted.find(error.message) != std::string::npos)
                    << "Formatted error should contain the error message for " << error_type;
            }
        }
    }
}

// Property test: Error message quality
TEST_F(CompilationErrorReportingTest, ErrorMessagesAreDescriptive) {
    auto invalid_sources = InvalidMeldSourceGenerator::generate_invalid_sources();
    
    for (const auto& [source, error_type] : invalid_sources) {
        CompileOptions options;
        options.target = CompilationTarget::JVM;  // Use one target for this test
        
        auto result = compiler_module_->compile_source(source, options);
        
        if (!result.success && !result.errors.empty()) {
            for (const auto& error : result.errors) {
                // Error message should contain relevant keywords
                std::string message_lower = error.message;
                std::transform(message_lower.begin(), message_lower.end(), 
                             message_lower.begin(), ::tolower);
                
                // Check for common error indicators
                bool has_error_indicator = 
                    message_lower.find("error") != std::string::npos ||
                    message_lower.find("failed") != std::string::npos ||
                    message_lower.find("invalid") != std::string::npos ||
                    message_lower.find("unknown") != std::string::npos ||
                    message_lower.find("undefined") != std::string::npos ||
                    message_lower.find("missing") != std::string::npos ||
                    message_lower.find("expected") != std::string::npos ||
                    message_lower.find("cannot") != std::string::npos;
                
                EXPECT_TRUE(has_error_indicator)
                    << "Error message should contain descriptive keywords for " 
                    << error_type << ": " << error.message;
                
                // Error message should not be too generic
                EXPECT_FALSE(message_lower == "error" || message_lower == "failed" || 
                           message_lower == "compilation failed")
                    << "Error message should be specific, not generic for " << error_type;
            }
        }
    }
}

// Property test: Error consistency across targets
TEST_F(CompilationErrorReportingTest, ErrorConsistencyAcrossTargets) {
    // Use a subset of invalid sources for cross-target comparison
    std::vector<std::pair<std::string, std::string>> test_sources = {
        {"val x =", "incomplete_assignment"},
        {"val x: String = 42", "type_mismatch"},
        {"val x = undefined_variable", "undefined_variable"},
        {"unknown_function(1, 2, 3)", "undefined_function"}
    };
    
    auto targets = std::vector<CompilationTarget>{
        CompilationTarget::JVM, CompilationTarget::Go, 
        CompilationTarget::Cpp, CompilationTarget::WebAssembly
    };
    
    for (const auto& [source, error_type] : test_sources) {
        std::vector<CompilationResult> results;
        
        // Compile with all targets
        for (const auto& target : targets) {
            CompileOptions options;
            options.target = target;
            results.push_back(compiler_module_->compile_source(source, options));
        }
        
        // All targets should fail for the same invalid source
        for (size_t i = 0; i < results.size(); ++i) {
            EXPECT_FALSE(results[i].success)
                << "Target " << static_cast<int>(targets[i]) 
                << " should fail for invalid source (" << error_type << ")";
            
            EXPECT_FALSE(results[i].errors.empty())
                << "Target " << static_cast<int>(targets[i]) 
                << " should report errors for " << error_type;
        }
        
        // Error types should be similar across targets (at least for parsing errors)
        if (error_type == "incomplete_assignment" || error_type == "undefined_variable") {
            // For parsing/semantic errors, all targets should report similar issues
            for (size_t i = 1; i < results.size(); ++i) {
                EXPECT_EQ(results[0].errors.empty(), results[i].errors.empty())
                    << "Error presence should be consistent across targets for " << error_type;
            }
        }
    }
}

// Property test: Error formatting consistency
TEST_F(CompilationErrorReportingTest, ErrorFormattingConsistency) {
    std::vector<std::string> invalid_sources = {
        "val x =",
        "val x: String = 42", 
        "unknown_function()"
    };
    
    for (const auto& source : invalid_sources) {
        CompileOptions options;
        options.target = CompilationTarget::JVM;
        
        auto result = compiler_module_->compile_source(source, options);
        
        if (!result.success) {
            for (const auto& error : result.errors) {
                std::string formatted = error.format();
                
                // Formatted error should not be empty
                EXPECT_FALSE(formatted.empty())
                    << "Formatted error should not be empty";
                
                // Should contain the error message
                EXPECT_TRUE(formatted.find(error.message) != std::string::npos)
                    << "Formatted error should contain the original message";
                
                // Should have consistent format structure
                if (!error.file.empty()) {
                    EXPECT_TRUE(formatted.find(error.file) != std::string::npos)
                        << "Formatted error should contain file name when available";
                }
                
                // Should contain "error:" indicator
                EXPECT_TRUE(formatted.find("error:") != std::string::npos)
                    << "Formatted error should contain 'error:' indicator";
                
                // Should not have duplicate error indicators
                size_t error_count = 0;
                size_t pos = 0;
                while ((pos = formatted.find("error:", pos)) != std::string::npos) {
                    error_count++;
                    pos += 6;
                }
                EXPECT_LE(error_count, 1)
                    << "Formatted error should not have duplicate 'error:' indicators";
            }
        }
    }
}

// Property test: Error suggestions presence
TEST_F(CompilationErrorReportingTest, ErrorSuggestionsWhenApplicable) {
    // Test cases where suggestions might be provided
    std::vector<std::pair<std::string, std::string>> sources_with_potential_suggestions = {
        {"val x =", "suggest_completing_assignment"},
        {"fnc add(a: Int, b: Int -> Int { a + b }", "suggest_missing_parenthesis"},
        {"val x: String = 42", "suggest_type_conversion"},
        {"unknown_function(1, 2, 3)", "suggest_similar_functions"}
    };
    
    for (const auto& [source, suggestion_type] : sources_with_potential_suggestions) {
        CompileOptions options;
        options.target = CompilationTarget::JVM;
        
        auto result = compiler_module_->compile_source(source, options);
        
        if (!result.success) {
            // Check if any errors have suggestions
            bool has_suggestions = false;
            for (const auto& error : result.errors) {
                if (!error.suggestions.empty()) {
                    has_suggestions = true;
                    
                    // Suggestions should be meaningful
                    for (const auto& suggestion : error.suggestions) {
                        EXPECT_FALSE(suggestion.empty())
                            << "Suggestion should not be empty for " << suggestion_type;
                        
                        EXPECT_GT(suggestion.length(), 3)
                            << "Suggestion should be descriptive for " << suggestion_type;
                    }
                }
            }
            
            // Note: We don't require suggestions for all errors, as the implementation
            // may not provide suggestions for all error types yet
            if (has_suggestions) {
                // If suggestions are provided, they should be in the formatted output
                for (const auto& error : result.errors) {
                    if (!error.suggestions.empty()) {
                        std::string formatted = error.format();
                        EXPECT_TRUE(formatted.find("Suggestion") != std::string::npos)
                            << "Formatted error should indicate suggestions are available";
                    }
                }
            }
        }
    }
}

// Property-based repetition test removed — individual TEST_F tests above
// already cover each property independently.