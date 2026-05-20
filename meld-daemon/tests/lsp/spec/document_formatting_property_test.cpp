/**
 * **Feature: meld-lsp-server, Property 21: Document formatting consistency**
 *
 * For any Meld document, formatting should produce output that conforms
 * to standard style conventions.
 *
 * **Validates: Requirements 5.1**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "meld/daemon/language_service.hpp"

#include <string>
#include <sstream>
#include <algorithm>

namespace {

using namespace meld::lsp::services;

/// Helper: apply edits to content to get formatted result
std::string apply_edits(const std::string& content,
                        const std::vector<LanguageService::TextEdit>& edits) {
    if (edits.empty()) return content;
    // For whole-document edits (single edit replacing everything), just return new_text
    if (edits.size() == 1 && edits[0].start_line == 0 && edits[0].start_character == 0) {
        return edits[0].new_text;
    }
    return content;
}

/// Helper: count leading spaces on a line
int count_leading_spaces(const std::string& line) {
    int count = 0;
    for (char c : line) {
        if (c == ' ') count++;
        else break;
    }
    return count;
}

/// Helper: split string into lines
std::vector<std::string> split_lines(const std::string& s) {
    std::vector<std::string> lines;
    std::istringstream stream(s);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    return lines;
}

} // anonymous namespace

/**
 * Property 21.1: Formatting produces consistent 4-space indentation
 * based on brace nesting depth.
 */
TEST(DocumentFormattingPropertyTest, ConsistentIndentation) {
    rc::check("Formatted code uses 4-space indentation per nesting level",
        []() {
            int num_funcs = *rc::gen::inRange(1, 5);

            std::ostringstream oss;
            for (int i = 0; i < num_funcs; ++i) {
                // Generate with inconsistent indentation
                int bad_indent = *rc::gen::inRange(0, 8);
                std::string spaces(bad_indent, ' ');
                oss << spaces << "fnc func_" << i << "(x: Int) {\n";
                int body_indent = *rc::gen::inRange(0, 12);
                std::string body_spaces(body_indent, ' ');
                oss << body_spaces << "let val_" << i << " = x\n";
                int close_indent = *rc::gen::inRange(0, 8);
                std::string close_spaces(close_indent, ' ');
                oss << close_spaces << "}\n\n";
            }

            std::string source = oss.str();
            LanguageService service;
            auto edits = service.format_document("file:///test.meld", source);
            std::string formatted = apply_edits(source, edits);

            // Verify: all non-blank lines have indentation that is a multiple of 4
            auto lines = split_lines(formatted);
            for (const auto& line : lines) {
                if (line.find_first_not_of(" \t\r") == std::string::npos) continue;
                int spaces = count_leading_spaces(line);
                RC_ASSERT(spaces % 4 == 0);
            }
        }
    );
}

/**
 * Property 21.2: Formatting trims trailing whitespace from all lines.
 */
TEST(DocumentFormattingPropertyTest, TrailingWhitespaceTrimmed) {
    rc::check("Formatted code has no trailing whitespace",
        []() {
            int num_lines = *rc::gen::inRange(1, 6);

            std::ostringstream oss;
            oss << "fnc example() {\n";
            for (int i = 0; i < num_lines; ++i) {
                int trailing = *rc::gen::inRange(0, 10);
                std::string trail(trailing, ' ');
                oss << "    let x_" << i << " = " << i << trail << "\n";
            }
            oss << "}\n";

            std::string source = oss.str();
            LanguageService service;
            auto edits = service.format_document("file:///test.meld", source);
            std::string formatted = apply_edits(source, edits);

            auto lines = split_lines(formatted);
            for (const auto& line : lines) {
                if (line.empty()) continue;
                RC_ASSERT(line.back() != ' ');
                RC_ASSERT(line.back() != '\t');
            }
        }
    );
}

/**
 * Property 21.3: Formatting normalizes multiple consecutive blank lines
 * to at most one blank line.
 */
TEST(DocumentFormattingPropertyTest, NormalizedBlankLines) {
    rc::check("Formatted code has at most one consecutive blank line",
        []() {
            std::ostringstream oss;
            oss << "fnc first() {\n    return 1\n}\n";

            int blanks = *rc::gen::inRange(2, 8);
            for (int i = 0; i < blanks; ++i) oss << "\n";

            oss << "fnc second() {\n    return 2\n}\n";

            std::string source = oss.str();
            LanguageService service;
            auto edits = service.format_document("file:///test.meld", source);
            std::string formatted = apply_edits(source, edits);

            // Check no more than one consecutive blank line
            auto lines = split_lines(formatted);
            int consecutive_blanks = 0;
            for (const auto& line : lines) {
                bool is_blank = (line.find_first_not_of(" \t\r") == std::string::npos);
                if (is_blank) {
                    consecutive_blanks++;
                    RC_ASSERT(consecutive_blanks <= 1);
                } else {
                    consecutive_blanks = 0;
                }
            }
        }
    );
}

/**
 * Property 21.4: Formatting is idempotent — formatting already-formatted
 * code produces the same result.
 */
TEST(DocumentFormattingPropertyTest, FormattingIsIdempotent) {
    rc::check("Formatting already-formatted code produces identical output",
        []() {
            int num_funcs = *rc::gen::inRange(1, 4);

            std::ostringstream oss;
            for (int i = 0; i < num_funcs; ++i) {
                oss << "fnc task_" << i << "(n: Int) {\n";
                oss << "    let result = n\n";
                oss << "    return result\n";
                oss << "}\n\n";
            }

            std::string source = oss.str();
            LanguageService service;

            // Format once
            auto edits1 = service.format_document("file:///test.meld", source);
            std::string formatted1 = apply_edits(source, edits1);

            // Format again
            auto edits2 = service.format_document("file:///test.meld", formatted1);
            std::string formatted2 = apply_edits(formatted1, edits2);

            RC_ASSERT(formatted1 == formatted2);
        }
    );
}
