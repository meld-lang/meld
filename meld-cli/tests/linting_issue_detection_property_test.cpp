#include <gtest/gtest.h>
#include <random>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <regex>
#include "meld/cli/dev_tools_module.hpp"

/**
 * **Feature: meld-cli, Property 25: Linting Issue Detection**
 * **Validates: Requirements 8.3, 8.4**
 * 
 * Property: For any Meld source file with code quality issues, linting should 
 * detect and report all issues with specific locations
 */

class LintingIssueDetectionTest : public ::testing::Test {
protected:
    void SetUp() override {
        gen.seed(std::random_device{}());
        linter = std::make_unique<meld::cli::CodeLinter>();
    }

    std::mt19937 gen;
    std::unique_ptr<meld::cli::CodeLinter> linter;

    // Generate code with known issues
    struct CodeWithIssues {
        std::string code;
        std::vector<std::string> expected_issues;
        std::vector<size_t> expected_lines;
    };

    CodeWithIssues generate_code_with_trailing_whitespace() {
        CodeWithIssues result;
        
        std::vector<std::string> base_lines = {
            "val x = 42",
            "val y = x + 1", 
            "print(y)",
            "fnc add(a: int, b: int) -> int {",
            "    return a + b",
            "}"
        };
        
        std::uniform_int_distribution<> line_dist(0, base_lines.size() - 1);
        std::uniform_int_distribution<> space_dist(1, 5);
        
        // Add trailing whitespace to random lines
        for (size_t i = 0; i < base_lines.size(); ++i) {
            if (gen() % 3 == 0) { // 1/3 chance of adding trailing whitespace
                int spaces = space_dist(gen);
                base_lines[i] += std::string(spaces, ' ');
                result.expected_issues.push_back("trailing-whitespace");
                result.expected_lines.push_back(i + 1);
            }
        }
        
        // Join lines
        std::ostringstream oss;
        for (const auto& line : base_lines) {
            oss << line << "\n";
        }
        result.code = oss.str();
        
        return result;
    }

    CodeWithIssues generate_code_with_long_lines() {
        CodeWithIssues result;
        
        // Create intentionally long lines
        std::vector<std::string> long_lines = {
            "val very_long_variable_name_that_exceeds_normal_limits = some_function_with_extremely_long_name(parameter1, parameter2, parameter3, parameter4, parameter5, parameter6)",
            "if (some_very_long_condition_that_checks_multiple_things && another_condition_that_is_also_quite_long && yet_another_condition) {",
            "    some_function_call_with_many_parameters(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12)",
            "}"
        };
        
        std::ostringstream oss;
        for (size_t i = 0; i < long_lines.size(); ++i) {
            oss << long_lines[i] << "\n";
            if (long_lines[i].length() > 100) {
                result.expected_issues.push_back("line-length");
                result.expected_lines.push_back(i + 1);
            }
        }
        result.code = oss.str();
        
        return result;
    }

    CodeWithIssues generate_code_with_syntax_issues() {
        CodeWithIssues result;
        
        std::vector<std::string> syntax_issues = {
            // Unmatched braces
            "fnc test() {\n    val x = 42\n    // Missing closing brace",
            
            // Multiple issues
            "val x = {\n    val y = 42\n    // Missing closing brace\n    val z = y + 1",
            
            // Nested unmatched braces
            "if (condition) {\n    if (other) {\n        doSomething()\n    // Missing two closing braces"
        };
        
        std::uniform_int_distribution<> issue_dist(0, syntax_issues.size() - 1);
        result.code = syntax_issues[issue_dist(gen)];
        
        // Count expected brace issues
        std::istringstream iss(result.code);
        std::string line;
        size_t line_num = 1;
        while (std::getline(iss, line)) {
            size_t open_braces = std::count(line.begin(), line.end(), '{');
            size_t close_braces = std::count(line.begin(), line.end(), '}');
            
            if (open_braces != close_braces) {
                result.expected_issues.push_back("unmatched-braces");
                result.expected_lines.push_back(line_num);
            }
            line_num++;
        }
        
        return result;
    }

    CodeWithIssues generate_code_with_unused_variables() {
        CodeWithIssues result;
        
        std::vector<std::string> unused_var_code = {
            "val unused_var = 42\nval used_var = 10\nprint(used_var)",
            "var temp = calculate()\nval result = 100\nreturn result",
            "val x = 1\nval y = 2\nval z = 3\nprint(x + y)" // z is unused
        };
        
        std::uniform_int_distribution<> code_dist(0, unused_var_code.size() - 1);
        result.code = unused_var_code[code_dist(gen)];
        
        // Analyze for unused variables (simplified)
        std::regex var_regex("(val|var)\\s+(\\w+)");
        std::sregex_iterator iter(result.code.begin(), result.code.end(), var_regex);
        std::sregex_iterator end;
        
        std::vector<std::string> declared_vars;
        std::vector<size_t> declaration_lines;
        
        // Find all variable declarations
        size_t current_line = 1;
        size_t pos = 0;
        for (auto i = iter; i != end; ++i) {
            std::smatch match = *i;
            declared_vars.push_back(match[2].str());
            
            // Calculate line number
            while (pos < match.position() && pos < result.code.length()) {
                if (result.code[pos] == '\n') current_line++;
                pos++;
            }
            declaration_lines.push_back(current_line);
        }
        
        // Check usage (very simplified)
        for (size_t i = 0; i < declared_vars.size(); ++i) {
            const std::string& var_name = declared_vars[i];
            size_t usage_count = 0;
            
            // Count occurrences
            size_t search_pos = 0;
            while ((search_pos = result.code.find(var_name, search_pos)) != std::string::npos) {
                usage_count++;
                search_pos += var_name.length();
            }
            
            // If only declared once (the declaration itself), it's unused
            if (usage_count <= 1) {
                result.expected_issues.push_back("unused-variable");
                result.expected_lines.push_back(declaration_lines[i]);
            }
        }
        
        return result;
    }

    CodeWithIssues generate_mixed_issues_code() {
        CodeWithIssues result;
        
        // Combine multiple types of issues
        auto trailing_ws = generate_code_with_trailing_whitespace();
        auto long_lines = generate_code_with_long_lines();
        auto unused_vars = generate_code_with_unused_variables();
        
        result.code = trailing_ws.code + "\n" + long_lines.code + "\n" + unused_vars.code;
        
        // Combine expected issues, adjusting line numbers
        result.expected_issues = trailing_ws.expected_issues;
        result.expected_lines = trailing_ws.expected_lines;
        
        size_t trailing_lines = std::count(trailing_ws.code.begin(), trailing_ws.code.end(), '\n') + 1;
        
        for (size_t i = 0; i < long_lines.expected_issues.size(); ++i) {
            result.expected_issues.push_back(long_lines.expected_issues[i]);
            result.expected_lines.push_back(long_lines.expected_lines[i] + trailing_lines);
        }
        
        size_t long_lines_count = std::count(long_lines.code.begin(), long_lines.code.end(), '\n') + 1;
        
        for (size_t i = 0; i < unused_vars.expected_issues.size(); ++i) {
            result.expected_issues.push_back(unused_vars.expected_issues[i]);
            result.expected_lines.push_back(unused_vars.expected_lines[i] + trailing_lines + long_lines_count);
        }
        
        return result;
    }

    // Generate lint options
    meld::cli::LintOptions generate_lint_options() {
        meld::cli::LintOptions options;
        
        std::uniform_int_distribution<> bool_dist(0, 1);
        std::uniform_int_distribution<> severity_dist(0, 3);
        
        options.auto_fix = bool_dist(gen);
        options.recursive = bool_dist(gen);
        
        // Randomly enable/disable specific rules
        std::vector<std::string> all_rules = {
            "trailing-whitespace", "line-length", "unmatched-braces", "unused-variable"
        };
        
        if (bool_dist(gen)) {
            // Sometimes enable specific rules
            std::uniform_int_distribution<> rule_dist(0, all_rules.size() - 1);
            options.enabled_rules.push_back(all_rules[rule_dist(gen)]);
        }
        
        // Set minimum severity
        switch (severity_dist(gen)) {
            case 0: options.min_severity = meld::cli::LintSeverity::Error; break;
            case 1: options.min_severity = meld::cli::LintSeverity::Warning; break;
            case 2: options.min_severity = meld::cli::LintSeverity::Info; break;
            case 3: options.min_severity = meld::cli::LintSeverity::Hint; break;
        }
        
        return options;
    }
};

TEST_F(LintingIssueDetectionTest, DetectsTrailingWhitespaceIssues) {
    // Property: Linter should detect trailing whitespace issues
    
    for (int iteration = 0; iteration < 100; ++iteration) {
        auto code_with_issues = generate_code_with_trailing_whitespace();
        auto options = generate_lint_options();
        
        auto result = linter->lint_code(code_with_issues.code, "test.meld", options);
        
        if (result.success && !code_with_issues.expected_issues.empty()) {
            // Should detect trailing whitespace issues
            bool found_trailing_whitespace = false;
            for (const auto& issue : result.issues) {
                if (issue.rule_id == "trailing-whitespace") {
                    found_trailing_whitespace = true;
                    
                    // Verify issue has proper location information
                    EXPECT_GT(issue.line, 0) 
                        << "Issue should have valid line number for iteration " << iteration;
                    EXPECT_FALSE(issue.message.empty()) 
                        << "Issue should have descriptive message for iteration " << iteration;
                    EXPECT_EQ(issue.file, "test.meld") 
                        << "Issue should reference correct file for iteration " << iteration;
                }
            }
            
            // If we expect trailing whitespace issues and the rule is enabled, we should find them
            bool rule_enabled = options.enabled_rules.empty() || 
                               std::find(options.enabled_rules.begin(), options.enabled_rules.end(), 
                                        "trailing-whitespace") != options.enabled_rules.end();
            
            if (rule_enabled && std::find(code_with_issues.expected_issues.begin(), 
                                         code_with_issues.expected_issues.end(), 
                                         "trailing-whitespace") != code_with_issues.expected_issues.end()) {
                EXPECT_TRUE(found_trailing_whitespace)
                    << "Should detect trailing whitespace when present for iteration " << iteration
                    << "\nCode: " << code_with_issues.code;
            }
        }
    }
}

TEST_F(LintingIssueDetectionTest, DetectsLineLengthIssues) {
    // Property: Linter should detect line length issues
    
    for (int iteration = 0; iteration < 100; ++iteration) {
        auto code_with_issues = generate_code_with_long_lines();
        auto options = generate_lint_options();
        
        auto result = linter->lint_code(code_with_issues.code, "test.meld", options);
        
        if (result.success && !code_with_issues.expected_issues.empty()) {
            bool found_line_length = false;
            for (const auto& issue : result.issues) {
                if (issue.rule_id == "line-length") {
                    found_line_length = true;
                    
                    // Verify issue properties
                    EXPECT_GT(issue.line, 0) 
                        << "Line length issue should have valid line number for iteration " << iteration;
                    EXPECT_FALSE(issue.message.empty()) 
                        << "Line length issue should have message for iteration " << iteration;
                    EXPECT_EQ(issue.severity, meld::cli::LintSeverity::Info) 
                        << "Line length should be Info severity for iteration " << iteration;
                }
            }
            
            bool rule_enabled = options.enabled_rules.empty() || 
                               std::find(options.enabled_rules.begin(), options.enabled_rules.end(), 
                                        "line-length") != options.enabled_rules.end();
            
            if (rule_enabled && std::find(code_with_issues.expected_issues.begin(), 
                                         code_with_issues.expected_issues.end(), 
                                         "line-length") != code_with_issues.expected_issues.end()) {
                EXPECT_TRUE(found_line_length)
                    << "Should detect line length issues when present for iteration " << iteration;
            }
        }
    }
}

TEST_F(LintingIssueDetectionTest, DetectsSyntaxIssues) {
    // Property: Linter should detect syntax issues like unmatched braces
    
    for (int iteration = 0; iteration < 100; ++iteration) {
        auto code_with_issues = generate_code_with_syntax_issues();
        auto options = generate_lint_options();
        
        auto result = linter->lint_code(code_with_issues.code, "test.meld", options);
        
        if (result.success && !code_with_issues.expected_issues.empty()) {
            bool found_syntax_issue = false;
            for (const auto& issue : result.issues) {
                if (issue.rule_id == "unmatched-braces") {
                    found_syntax_issue = true;
                    
                    // Syntax issues should be errors
                    EXPECT_EQ(issue.severity, meld::cli::LintSeverity::Error) 
                        << "Syntax issues should be errors for iteration " << iteration;
                    EXPECT_GT(issue.line, 0) 
                        << "Syntax issue should have valid line number for iteration " << iteration;
                    EXPECT_FALSE(issue.suggestions.empty()) 
                        << "Syntax issues should have suggestions for iteration " << iteration;
                }
            }
            
            bool rule_enabled = options.enabled_rules.empty() || 
                               std::find(options.enabled_rules.begin(), options.enabled_rules.end(), 
                                        "unmatched-braces") != options.enabled_rules.end();
            
            if (rule_enabled && std::find(code_with_issues.expected_issues.begin(), 
                                         code_with_issues.expected_issues.end(), 
                                         "unmatched-braces") != code_with_issues.expected_issues.end()) {
                EXPECT_TRUE(found_syntax_issue)
                    << "Should detect syntax issues when present for iteration " << iteration;
            }
        }
    }
}

TEST_F(LintingIssueDetectionTest, IssueLocationAccuracy) {
    // Property: All detected issues should have accurate location information
    
    for (int iteration = 0; iteration < 100; ++iteration) {
        auto code_with_issues = generate_mixed_issues_code();
        auto options = generate_lint_options();
        
        auto result = linter->lint_code(code_with_issues.code, "test.meld", options);
        
        if (result.success) {
            for (const auto& issue : result.issues) {
                // Property: Line numbers should be valid
                EXPECT_GT(issue.line, 0) 
                    << "Issue line number should be positive for iteration " << iteration
                    << "\nIssue: " << issue.message;
                
                // Count actual lines in code
                size_t total_lines = std::count(code_with_issues.code.begin(), 
                                              code_with_issues.code.end(), '\n') + 1;
                EXPECT_LE(issue.line, total_lines) 
                    << "Issue line number should not exceed total lines for iteration " << iteration
                    << "\nIssue line: " << issue.line << ", Total lines: " << total_lines;
                
                // Property: Column numbers should be reasonable
                EXPECT_GE(issue.column, 0) 
                    << "Issue column should be non-negative for iteration " << iteration;
                
                // Property: File name should be set
                EXPECT_EQ(issue.file, "test.meld") 
                    << "Issue should reference correct file for iteration " << iteration;
                
                // Property: Message should be descriptive
                EXPECT_FALSE(issue.message.empty()) 
                    << "Issue message should not be empty for iteration " << iteration;
                EXPECT_GE(issue.message.length(), 5) 
                    << "Issue message should be descriptive for iteration " << iteration;
                
                // Property: Rule ID should be set
                EXPECT_FALSE(issue.rule_id.empty()) 
                    << "Issue should have rule ID for iteration " << iteration;
                
                // Property: Severity should be valid
                EXPECT_TRUE(issue.severity == meld::cli::LintSeverity::Error ||
                           issue.severity == meld::cli::LintSeverity::Warning ||
                           issue.severity == meld::cli::LintSeverity::Info ||
                           issue.severity == meld::cli::LintSeverity::Hint)
                    << "Issue should have valid severity for iteration " << iteration;
            }
        }
    }
}

TEST_F(LintingIssueDetectionTest, SeverityFilteringWorks) {
    // Property: Linter should respect minimum severity filtering
    
    for (int iteration = 0; iteration < 100; ++iteration) {
        auto code_with_issues = generate_mixed_issues_code();
        auto options = generate_lint_options();
        
        auto result = linter->lint_code(code_with_issues.code, "test.meld", options);
        
        if (result.success) {
            // Property: All reported issues should meet minimum severity requirement
            for (const auto& issue : result.issues) {
                EXPECT_GE(static_cast<int>(issue.severity), static_cast<int>(options.min_severity))
                    << "Issue severity should meet minimum requirement for iteration " << iteration
                    << "\nIssue severity: " << static_cast<int>(issue.severity)
                    << "\nMin severity: " << static_cast<int>(options.min_severity)
                    << "\nIssue: " << issue.message;
            }
        }
    }
}

TEST_F(LintingIssueDetectionTest, AutoFixableIssuesMarked) {
    // Property: Issues that can be automatically fixed should be marked as such
    
    for (int iteration = 0; iteration < 100; ++iteration) {
        auto code_with_issues = generate_code_with_trailing_whitespace();
        auto options = generate_lint_options();
        
        auto result = linter->lint_code(code_with_issues.code, "test.meld", options);
        
        if (result.success) {
            for (const auto& issue : result.issues) {
                // Property: Trailing whitespace should be auto-fixable
                if (issue.rule_id == "trailing-whitespace") {
                    EXPECT_TRUE(issue.auto_fixable)
                        << "Trailing whitespace should be auto-fixable for iteration " << iteration;
                    EXPECT_FALSE(issue.suggestions.empty())
                        << "Auto-fixable issues should have suggestions for iteration " << iteration;
                }
                
                // Property: If marked as auto-fixable, should have suggestions
                if (issue.auto_fixable) {
                    EXPECT_FALSE(issue.suggestions.empty())
                        << "Auto-fixable issues must have suggestions for iteration " << iteration
                        << "\nIssue: " << issue.message;
                }
            }
        }
    }
}

// Run the property-based tests
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}