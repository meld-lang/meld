#include <gtest/gtest.h>
#include <random>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include "meld/cli/dev_tools_module.hpp"

/**
 * **Feature: meld-cli, Property 23: Code Formatting Consistency**
 * **Validates: Requirements 8.1**
 * 
 * Property: For any valid Meld source file, formatting should produce output 
 * that conforms to standard conventions and is idempotent (formatting twice 
 * produces the same result)
 */

class CodeFormattingConsistencyTest : public ::testing::Test {
protected:
    void SetUp() override {
        gen.seed(std::random_device{}());
        formatter = std::make_unique<meld::cli::CodeFormatter>();
    }

    std::mt19937 gen;
    std::unique_ptr<meld::cli::CodeFormatter> formatter;

    // Generator for valid Meld code snippets
    std::string generate_meld_code() {
        std::vector<std::string> code_templates = {
            "val x = 42\nval y = x + 1\nprint(y)",
            "fnc add(a: int, b: int) -> int {\n    return a + b\n}",
            "class Point {\n    val x: int\n    val y: int\n}",
            "if (condition) {\n    doSomething()\n} else {\n    doOther()\n}",
            "for (i in 0..10) {\n    print(i)\n}",
            "val list = [1, 2, 3, 4, 5]\nval filtered = list.filter { it > 2 }",
            "match (value) {\n    case 1 -> print(\"one\")\n    case 2 -> print(\"two\")\n    default -> print(\"other\")\n}",
            "val result = try {\n    riskyOperation()\n} catch (e: Exception) {\n    handleError(e)\n}"
        };
        
        std::uniform_int_distribution<> template_dist(0, code_templates.size() - 1);
        return code_templates[template_dist(gen)];
    }

    // Add random formatting variations to code
    std::string add_formatting_variations(const std::string& code) {
        std::string varied = code;
        
        // Add random extra spaces
        std::uniform_int_distribution<> space_dist(0, 10);
        if (space_dist(gen) < 3) {
            size_t pos = varied.find(' ');
            if (pos != std::string::npos) {
                varied.insert(pos, "  "); // Add extra spaces
            }
        }
        
        // Add random extra newlines
        if (space_dist(gen) < 3) {
            size_t pos = varied.find('\n');
            if (pos != std::string::npos) {
                varied.insert(pos, "\n"); // Add extra newline
            }
        }
        
        // Add trailing whitespace
        if (space_dist(gen) < 3) {
            std::istringstream iss(varied);
            std::ostringstream oss;
            std::string line;
            while (std::getline(iss, line)) {
                oss << line;
                if (space_dist(gen) < 5) {
                    oss << "  "; // Add trailing spaces
                }
                oss << "\n";
            }
            varied = oss.str();
        }
        
        return varied;
    }

    // Generate format options
    meld::cli::FormatOptions generate_format_options() {
        meld::cli::FormatOptions options;
        
        std::uniform_int_distribution<> bool_dist(0, 1);
        std::uniform_int_distribution<> indent_dist(2, 8);
        std::uniform_int_distribution<> length_dist(80, 120);
        
        options.check_only = false; // We want to actually format
        options.recursive = bool_dist(gen);
        options.indent_size = indent_dist(gen);
        options.max_line_length = length_dist(gen);
        options.use_tabs = bool_dist(gen);
        options.preserve_newlines = bool_dist(gen);
        
        return options;
    }
};

TEST_F(CodeFormattingConsistencyTest, FormattingIsIdempotent) {
    // Property: Formatting the same code twice should produce identical results
    
    for (int iteration = 0; iteration < 100; ++iteration) {
        // Generate test input
        std::string original_code = generate_meld_code();
        std::string varied_code = add_formatting_variations(original_code);
        meld::cli::FormatOptions options = generate_format_options();
        
        // First formatting pass
        auto first_result = formatter->format_code(varied_code, options);
        ASSERT_TRUE(first_result.success) 
            << "First formatting should succeed for iteration " << iteration;
        
        // Second formatting pass on the result
        auto second_result = formatter->format_code(first_result.formatted_code, options);
        ASSERT_TRUE(second_result.success) 
            << "Second formatting should succeed for iteration " << iteration;
        
        // Property: Idempotence - formatting twice should produce the same result
        EXPECT_EQ(first_result.formatted_code, second_result.formatted_code)
            << "Formatting should be idempotent for iteration " << iteration
            << "\nOriginal: " << varied_code
            << "\nFirst format: " << first_result.formatted_code
            << "\nSecond format: " << second_result.formatted_code;
        
        // Property: Second formatting should not indicate changes needed
        EXPECT_FALSE(second_result.needs_formatting)
            << "Already formatted code should not need further formatting for iteration " << iteration;
    }
}

TEST_F(CodeFormattingConsistencyTest, FormattingProducesValidStructure) {
    // Property: Formatted code should have consistent indentation and structure
    
    for (int iteration = 0; iteration < 100; ++iteration) {
        std::string original_code = generate_meld_code();
        std::string varied_code = add_formatting_variations(original_code);
        meld::cli::FormatOptions options = generate_format_options();
        
        auto result = formatter->format_code(varied_code, options);
        ASSERT_TRUE(result.success) 
            << "Formatting should succeed for iteration " << iteration;
        
        std::string formatted = result.formatted_code;
        
        // Property: No trailing whitespace on any line
        std::istringstream iss(formatted);
        std::string line;
        int line_num = 1;
        while (std::getline(iss, line)) {
            if (!line.empty()) {
                EXPECT_NE(line.back(), ' ') 
                    << "Line " << line_num << " should not have trailing spaces in iteration " << iteration
                    << "\nLine content: '" << line << "'";
                EXPECT_NE(line.back(), '\t') 
                    << "Line " << line_num << " should not have trailing tabs in iteration " << iteration
                    << "\nLine content: '" << line << "'";
            }
            line_num++;
        }
        
        // Property: Consistent indentation
        iss.clear();
        iss.str(formatted);
        line_num = 1;
        while (std::getline(iss, line)) {
            if (!line.empty() && (line[0] == ' ' || line[0] == '\t')) {
                // Check that indentation is consistent with options
                size_t indent_count = 0;
                for (char c : line) {
                    if (c == ' ' || c == '\t') {
                        indent_count++;
                    } else {
                        break;
                    }
                }
                
                if (options.use_tabs) {
                    // Should use tabs for indentation
                    EXPECT_TRUE(line[0] == '\t' || line[0] != ' ')
                        << "Line " << line_num << " should use tabs for indentation in iteration " << iteration
                        << "\nLine content: '" << line << "'";
                } else {
                    // Should use spaces and be multiple of indent_size
                    if (indent_count > 0) {
                        EXPECT_EQ(indent_count % options.indent_size, 0)
                            << "Line " << line_num << " indentation should be multiple of " << options.indent_size 
                            << " in iteration " << iteration
                            << "\nIndent count: " << indent_count
                            << "\nLine content: '" << line << "'";
                    }
                }
            }
            line_num++;
        }
    }
}

TEST_F(CodeFormattingConsistencyTest, FormattingRespectsLineLength) {
    // Property: Formatted code should respect maximum line length settings
    
    for (int iteration = 0; iteration < 100; ++iteration) {
        std::string original_code = generate_meld_code();
        meld::cli::FormatOptions options = generate_format_options();
        
        // Create a long line to test line length handling
        std::string long_line = "val very_long_variable_name = some_function_with_long_name(parameter1, parameter2, parameter3, parameter4, parameter5)";
        std::string test_code = original_code + "\n" + long_line;
        
        auto result = formatter->format_code(test_code, options);
        ASSERT_TRUE(result.success) 
            << "Formatting should succeed for iteration " << iteration;
        
        // Property: No line should exceed max_line_length (with some tolerance for unbreakable tokens)
        std::istringstream iss(result.formatted_code);
        std::string line;
        int line_num = 1;
        while (std::getline(iss, line)) {
            // Allow some tolerance for lines that can't be broken
            if (line.length() > static_cast<size_t>(options.max_line_length + 10)) {
                // Check if the line contains unbreakable tokens
                bool has_long_token = false;
                std::istringstream token_stream(line);
                std::string token;
                while (token_stream >> token) {
                    if (token.length() > static_cast<size_t>(options.max_line_length)) {
                        has_long_token = true;
                        break;
                    }
                }
                
                if (!has_long_token) {
                    EXPECT_LE(line.length(), static_cast<size_t>(options.max_line_length + 10))
                        << "Line " << line_num << " exceeds maximum length in iteration " << iteration
                        << "\nMax length: " << options.max_line_length
                        << "\nActual length: " << line.length()
                        << "\nLine content: '" << line << "'";
                }
            }
            line_num++;
        }
    }
}

TEST_F(CodeFormattingConsistencyTest, FormattingPreservesSemantics) {
    // Property: Formatting should not change the semantic meaning of code
    
    for (int iteration = 0; iteration < 100; ++iteration) {
        std::string original_code = generate_meld_code();
        meld::cli::FormatOptions options = generate_format_options();
        
        auto result = formatter->format_code(original_code, options);
        ASSERT_TRUE(result.success) 
            << "Formatting should succeed for iteration " << iteration;
        
        // Property: Number of non-whitespace characters should be preserved
        auto count_non_whitespace = [](const std::string& str) {
            return std::count_if(str.begin(), str.end(), 
                [](char c) { return !std::isspace(c); });
        };
        
        size_t original_chars = count_non_whitespace(original_code);
        size_t formatted_chars = count_non_whitespace(result.formatted_code);
        
        EXPECT_EQ(original_chars, formatted_chars)
            << "Formatting should preserve non-whitespace characters in iteration " << iteration
            << "\nOriginal: " << original_code
            << "\nFormatted: " << result.formatted_code;
        
        // Property: Key tokens should be preserved in order
        auto extract_tokens = [](const std::string& str) {
            std::vector<std::string> tokens;
            std::istringstream iss(str);
            std::string token;
            while (iss >> token) {
                tokens.push_back(token);
            }
            return tokens;
        };
        
        auto original_tokens = extract_tokens(original_code);
        auto formatted_tokens = extract_tokens(result.formatted_code);
        
        EXPECT_EQ(original_tokens, formatted_tokens)
            << "Formatting should preserve token order in iteration " << iteration
            << "\nOriginal tokens size: " << original_tokens.size()
            << "\nFormatted tokens size: " << formatted_tokens.size();
    }
}

// Run the property-based tests
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}