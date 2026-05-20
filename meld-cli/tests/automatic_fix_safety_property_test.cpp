#include <gtest/gtest.h>
#include <random>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <regex>
#include <set>
#include "meld/cli/dev_tools_module.hpp"

/**
 * **Feature: meld-cli, Property 26: Automatic Fix Safety**
 * **Validates: Requirements 8.5**
 * 
 * Property: For any code issue that can be automatically fixed, applying the fix 
 * should preserve program semantics
 */

class AutomaticFixSafetyTest : public ::testing::Test {
protected:
    void SetUp() override {
        gen.seed(std::random_device{}());
        linter = std::make_unique<meld::cli::CodeLinter>();
    }

    std::mt19937 gen;
    std::unique_ptr<meld::cli::CodeLinter> linter;

    // Generate code with fixable issues
    struct FixableCode {
        std::string original;
        std::vector<std::string> expected_fixable_rules;
        std::string description;
    };

    FixableCode generate_trailing_whitespace_code() {
        FixableCode result;
        result.description = "Code with trailing whitespace";
        
        std::vector<std::string> base_lines = {
            "val x = 42",
            "val y = x + 1", 
            "print(y)",
            "fnc add(a: int, b: int) -> int {",
            "    return a + b",
            "}",
            "class Point {",
            "    val x: int",
            "    val y: int",
            "}"
        };
        
        std::uniform_int_distribution<> space_dist(1, 5);
        
        // Add trailing whitespace to some lines
        std::ostringstream oss;
        for (const auto& line : base_lines) {
            oss << line;
            if (gen() % 3 == 0) { // 1/3 chance of adding trailing whitespace
                int spaces = space_dist(gen);
                oss << std::string(spaces, ' ');
            }
            oss << "\n";
        }
        
        result.original = oss.str();
        result.expected_fixable_rules.push_back("trailing-whitespace");
        
        return result;
    }

    FixableCode generate_mixed_fixable_code() {
        FixableCode result;
        result.description = "Code with multiple fixable issues";
        
        // Code with trailing whitespace and other fixable formatting issues
        result.original = R"(val x = 42   
val y = x + 1  
print(y)	
fnc calculate(a: int, b: int) -> int {  
    return a + b  
}  
)";
        
        result.expected_fixable_rules.push_back("trailing-whitespace");
        
        return result;
    }

    FixableCode generate_complex_semantic_code() {
        FixableCode result;
        result.description = "Complex code that should preserve semantics";
        
        // More complex code to test semantic preservation
        result.original = R"(class Calculator {   
    val precision: int = 10   
    
    fnc add(a: double, b: double) -> double {   
        return a + b   
    }   
    
    fnc multiply(a: double, b: double) -> double {   
        val result = a * b   
        return result   
    }   
}   

val calc = Calculator()   
val sum = calc.add(3.14, 2.86)   
val product = calc.multiply(sum, 2.0)   
print("Result: ${product}")   
)";
        
        result.expected_fixable_rules.push_back("trailing-whitespace");
        
        return result;
    }

    // Extract semantic tokens from code
    std::vector<std::string> extract_semantic_tokens(const std::string& code) {
        std::vector<std::string> tokens;
        
        // Simple tokenization - in a real implementation this would be more sophisticated
        std::regex token_regex(R"(\b\w+\b|[+\-*/=<>!]+|[{}()\[\];,.]|\d+\.?\d*)");
        std::sregex_iterator iter(code.begin(), code.end(), token_regex);
        std::sregex_iterator end;
        
        for (auto i = iter; i != end; ++i) {
            std::string token = i->str();
            // Skip pure whitespace tokens
            if (!token.empty() && token.find_first_not_of(" \t\n\r") != std::string::npos) {
                tokens.push_back(token);
            }
        }
        
        return tokens;
    }

    // Extract identifiers and their relationships
    struct SemanticStructure {
        std::set<std::string> identifiers;
        std::vector<std::pair<std::string, std::string>> assignments; // var -> value
        std::vector<std::string> function_calls;
        std::vector<std::string> class_definitions;
        int brace_balance = 0;
        int paren_balance = 0;
    };

    SemanticStructure analyze_semantic_structure(const std::string& code) {
        SemanticStructure structure;
        
        // Extract identifiers
        std::regex identifier_regex(R"(\b[a-zA-Z_][a-zA-Z0-9_]*\b)");
        std::sregex_iterator iter(code.begin(), code.end(), identifier_regex);
        std::sregex_iterator end;
        
        for (auto i = iter; i != end; ++i) {
            std::string identifier = i->str();
            // Skip keywords
            if (identifier != "val" && identifier != "var" && identifier != "fnc" && 
                identifier != "class" && identifier != "if" && identifier != "else" &&
                identifier != "return" && identifier != "print") {
                structure.identifiers.insert(identifier);
            }
        }
        
        // Extract assignments
        std::regex assignment_regex(R"((?:val|var)\s+(\w+)\s*=\s*([^;\n]+))");
        std::sregex_iterator assign_iter(code.begin(), code.end(), assignment_regex);
        
        for (auto i = assign_iter; i != std::sregex_iterator(); ++i) {
            std::smatch match = *i;
            structure.assignments.emplace_back(match[1].str(), match[2].str());
        }
        
        // Extract function calls
        std::regex call_regex(R"((\w+)\s*\()");
        std::sregex_iterator call_iter(code.begin(), code.end(), call_regex);
        
        for (auto i = call_iter; i != std::sregex_iterator(); ++i) {
            std::smatch match = *i;
            structure.function_calls.push_back(match[1].str());
        }
        
        // Count braces and parentheses
        for (char c : code) {
            if (c == '{') structure.brace_balance++;
            else if (c == '}') structure.brace_balance--;
            else if (c == '(') structure.paren_balance++;
            else if (c == ')') structure.paren_balance--;
        }
        
        return structure;
    }

    // Generate lint options that enable auto-fixing
    meld::cli::LintOptions generate_autofix_options() {
        meld::cli::LintOptions options;
        
        options.auto_fix = true; // Enable auto-fixing
        options.recursive = false;
        
        // Enable all fixable rules
        options.enabled_rules = {"trailing-whitespace"};
        options.min_severity = meld::cli::LintSeverity::Hint;
        
        return options;
    }
};

TEST_F(AutomaticFixSafetyTest, FixesPreserveSemanticTokens) {
    // Property: Automatic fixes should preserve all semantic tokens in the same order
    
    for (int iteration = 0; iteration < 100; ++iteration) {
        std::vector<FixableCode> test_cases = {
            generate_trailing_whitespace_code(),
            generate_mixed_fixable_code(),
            generate_complex_semantic_code()
        };
        
        std::uniform_int_distribution<> case_dist(0, test_cases.size() - 1);
        auto test_case = test_cases[case_dist(gen)];
        
        auto options = generate_autofix_options();
        
        // Get original semantic tokens
        auto original_tokens = extract_semantic_tokens(test_case.original);
        
        // Run linting with auto-fix
        auto result = linter->lint_code(test_case.original, "test.meld", options);
        
        if (result.success && result.fixes_applied > 0) {
            // Apply fixes to get the fixed code
            auto fixed_code = linter->apply_fixes(test_case.original, result.issues);
            
            // Get fixed semantic tokens
            auto fixed_tokens = extract_semantic_tokens(fixed_code);
            
            // Property: Semantic tokens should be preserved
            EXPECT_EQ(original_tokens, fixed_tokens)
                << "Semantic tokens should be preserved after auto-fix for iteration " << iteration
                << "\nTest case: " << test_case.description
                << "\nOriginal code: " << test_case.original
                << "\nFixed code: " << fixed_code
                << "\nOriginal tokens: " << original_tokens.size()
                << "\nFixed tokens: " << fixed_tokens.size();
        }
    }
}

TEST_F(AutomaticFixSafetyTest, FixesPreserveSemanticStructure) {
    // Property: Automatic fixes should preserve semantic structure (identifiers, assignments, calls)
    
    for (int iteration = 0; iteration < 100; ++iteration) {
        auto test_case = generate_complex_semantic_code();
        auto options = generate_autofix_options();
        
        // Analyze original semantic structure
        auto original_structure = analyze_semantic_structure(test_case.original);
        
        // Run linting with auto-fix
        auto result = linter->lint_code(test_case.original, "test.meld", options);
        
        if (result.success && result.fixes_applied > 0) {
            auto fixed_code = linter->apply_fixes(test_case.original, result.issues);
            auto fixed_structure = analyze_semantic_structure(fixed_code);
            
            // Property: Identifiers should be preserved
            EXPECT_EQ(original_structure.identifiers, fixed_structure.identifiers)
                << "Identifiers should be preserved after auto-fix for iteration " << iteration
                << "\nOriginal code: " << test_case.original
                << "\nFixed code: " << fixed_code;
            
            // Property: Assignments should be preserved
            EXPECT_EQ(original_structure.assignments.size(), fixed_structure.assignments.size())
                << "Number of assignments should be preserved for iteration " << iteration;
            
            for (size_t i = 0; i < std::min(original_structure.assignments.size(), 
                                           fixed_structure.assignments.size()); ++i) {
                EXPECT_EQ(original_structure.assignments[i].first, 
                         fixed_structure.assignments[i].first)
                    << "Assignment variable should be preserved for iteration " << iteration;
            }
            
            // Property: Function calls should be preserved
            EXPECT_EQ(original_structure.function_calls, fixed_structure.function_calls)
                << "Function calls should be preserved after auto-fix for iteration " << iteration;
            
            // Property: Structural balance should be preserved
            EXPECT_EQ(original_structure.brace_balance, fixed_structure.brace_balance)
                << "Brace balance should be preserved for iteration " << iteration;
            EXPECT_EQ(original_structure.paren_balance, fixed_structure.paren_balance)
                << "Parentheses balance should be preserved for iteration " << iteration;
        }
    }
}

TEST_F(AutomaticFixSafetyTest, FixesOnlyModifyWhitespace) {
    // Property: Safe automatic fixes should only modify whitespace, not code content
    
    for (int iteration = 0; iteration < 100; ++iteration) {
        auto test_case = generate_trailing_whitespace_code();
        auto options = generate_autofix_options();
        
        // Run linting with auto-fix
        auto result = linter->lint_code(test_case.original, "test.meld", options);
        
        if (result.success && result.fixes_applied > 0) {
            auto fixed_code = linter->apply_fixes(test_case.original, result.issues);
            
            // Property: Non-whitespace characters should be identical
            auto remove_whitespace = [](const std::string& str) {
                std::string result;
                for (char c : str) {
                    if (!std::isspace(c)) {
                        result += c;
                    }
                }
                return result;
            };
            
            std::string original_no_ws = remove_whitespace(test_case.original);
            std::string fixed_no_ws = remove_whitespace(fixed_code);
            
            EXPECT_EQ(original_no_ws, fixed_no_ws)
                << "Non-whitespace content should be identical after auto-fix for iteration " << iteration
                << "\nOriginal: " << test_case.original
                << "\nFixed: " << fixed_code;
            
            // Property: Fixed code should have less or equal whitespace
            size_t original_ws_count = test_case.original.length() - original_no_ws.length();
            size_t fixed_ws_count = fixed_code.length() - fixed_no_ws.length();
            
            EXPECT_LE(fixed_ws_count, original_ws_count)
                << "Fixed code should not add unnecessary whitespace for iteration " << iteration
                << "\nOriginal whitespace: " << original_ws_count
                << "\nFixed whitespace: " << fixed_ws_count;
        }
    }
}

TEST_F(AutomaticFixSafetyTest, FixesAreIdempotent) {
    // Property: Applying fixes multiple times should produce the same result
    
    for (int iteration = 0; iteration < 100; ++iteration) {
        auto test_case = generate_mixed_fixable_code();
        auto options = generate_autofix_options();
        
        // First fix application
        auto result1 = linter->lint_code(test_case.original, "test.meld", options);
        
        if (result1.success && result1.fixes_applied > 0) {
            auto fixed_code1 = linter->apply_fixes(test_case.original, result1.issues);
            
            // Second fix application on already fixed code
            auto result2 = linter->lint_code(fixed_code1, "test.meld", options);
            
            if (result2.success) {
                auto fixed_code2 = linter->apply_fixes(fixed_code1, result2.issues);
                
                // Property: Second application should not change anything
                EXPECT_EQ(fixed_code1, fixed_code2)
                    << "Fixes should be idempotent for iteration " << iteration
                    << "\nOriginal: " << test_case.original
                    << "\nFirst fix: " << fixed_code1
                    << "\nSecond fix: " << fixed_code2;
                
                // Property: No more fixes should be needed
                EXPECT_EQ(result2.fixes_applied, 0)
                    << "No additional fixes should be applied on already fixed code for iteration " << iteration;
            }
        }
    }
}

TEST_F(AutomaticFixSafetyTest, FixesPreserveLineStructure) {
    // Property: Fixes should preserve logical line structure and relationships
    
    for (int iteration = 0; iteration < 100; ++iteration) {
        auto test_case = generate_complex_semantic_code();
        auto options = generate_autofix_options();
        
        // Count logical lines (non-empty lines)
        auto count_logical_lines = [](const std::string& code) {
            std::istringstream iss(code);
            std::string line;
            int count = 0;
            while (std::getline(iss, line)) {
                // Trim whitespace
                line.erase(0, line.find_first_not_of(" \t"));
                line.erase(line.find_last_not_of(" \t") + 1);
                if (!line.empty()) {
                    count++;
                }
            }
            return count;
        };
        
        int original_logical_lines = count_logical_lines(test_case.original);
        
        auto result = linter->lint_code(test_case.original, "test.meld", options);
        
        if (result.success && result.fixes_applied > 0) {
            auto fixed_code = linter->apply_fixes(test_case.original, result.issues);
            int fixed_logical_lines = count_logical_lines(fixed_code);
            
            // Property: Number of logical lines should be preserved
            EXPECT_EQ(original_logical_lines, fixed_logical_lines)
                << "Number of logical lines should be preserved for iteration " << iteration
                << "\nOriginal lines: " << original_logical_lines
                << "\nFixed lines: " << fixed_logical_lines;
            
            // Property: Line order should be preserved
            std::vector<std::string> original_lines, fixed_lines;
            
            std::istringstream orig_iss(test_case.original);
            std::string line;
            while (std::getline(orig_iss, line)) {
                line.erase(0, line.find_first_not_of(" \t"));
                line.erase(line.find_last_not_of(" \t") + 1);
                if (!line.empty()) {
                    original_lines.push_back(line);
                }
            }
            
            std::istringstream fixed_iss(fixed_code);
            while (std::getline(fixed_iss, line)) {
                line.erase(0, line.find_first_not_of(" \t"));
                line.erase(line.find_last_not_of(" \t") + 1);
                if (!line.empty()) {
                    fixed_lines.push_back(line);
                }
            }
            
            EXPECT_EQ(original_lines, fixed_lines)
                << "Logical line content and order should be preserved for iteration " << iteration;
        }
    }
}

TEST_F(AutomaticFixSafetyTest, OnlyFixableIssuesAreFixed) {
    // Property: Only issues marked as auto-fixable should be automatically fixed
    
    for (int iteration = 0; iteration < 100; ++iteration) {
        auto test_case = generate_mixed_fixable_code();
        auto options = generate_autofix_options();
        
        auto result = linter->lint_code(test_case.original, "test.meld", options);
        
        if (result.success) {
            // Property: Only auto-fixable issues should contribute to fixes_applied count
            size_t auto_fixable_count = 0;
            for (const auto& issue : result.issues) {
                if (issue.auto_fixable) {
                    auto_fixable_count++;
                }
            }
            
            // The number of fixes applied should not exceed the number of auto-fixable issues
            EXPECT_LE(result.fixes_applied, auto_fixable_count)
                << "Fixes applied should not exceed auto-fixable issues for iteration " << iteration
                << "\nFixes applied: " << result.fixes_applied
                << "\nAuto-fixable issues: " << auto_fixable_count;
            
            // Property: All auto-fixable issues should have suggestions
            for (const auto& issue : result.issues) {
                if (issue.auto_fixable) {
                    EXPECT_FALSE(issue.suggestions.empty())
                        << "Auto-fixable issues must have suggestions for iteration " << iteration
                        << "\nIssue: " << issue.message;
                }
            }
        }
    }
}

TEST_F(AutomaticFixSafetyTest, FixesDoNotIntroduceNewIssues) {
    // Property: Applying fixes should not introduce new linting issues
    
    for (int iteration = 0; iteration < 50; ++iteration) {
        auto test_case = generate_trailing_whitespace_code();
        auto options = generate_autofix_options();
        
        // Get original issues
        auto original_result = linter->lint_code(test_case.original, "test.meld", options);
        
        if (original_result.success && original_result.fixes_applied > 0) {
            auto fixed_code = linter->apply_fixes(test_case.original, original_result.issues);
            
            // Check for issues in fixed code
            auto fixed_result = linter->lint_code(fixed_code, "test.meld", options);
            
            if (fixed_result.success) {
                // Property: Fixed code should have fewer or equal issues
                EXPECT_LE(fixed_result.issues.size(), original_result.issues.size())
                    << "Fixed code should not have more issues than original for iteration " << iteration
                    << "\nOriginal issues: " << original_result.issues.size()
                    << "\nFixed issues: " << fixed_result.issues.size();
                
                // Property: No new error-level issues should be introduced
                size_t original_errors = 0, fixed_errors = 0;
                for (const auto& issue : original_result.issues) {
                    if (issue.severity == meld::cli::LintSeverity::Error) {
                        original_errors++;
                    }
                }
                for (const auto& issue : fixed_result.issues) {
                    if (issue.severity == meld::cli::LintSeverity::Error) {
                        fixed_errors++;
                    }
                }
                
                EXPECT_LE(fixed_errors, original_errors)
                    << "Fixes should not introduce new errors for iteration " << iteration
                    << "\nOriginal errors: " << original_errors
                    << "\nFixed errors: " << fixed_errors;
            }
        }
    }
}

// Run the property-based tests
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}