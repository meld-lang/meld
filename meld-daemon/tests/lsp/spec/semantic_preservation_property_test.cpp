/**
 * **Feature: meld-lsp-server, Property 25: Semantic preservation during formatting**
 *
 * For any code formatting operation, the semantic meaning should be
 * preserved while improving readability.
 *
 * **Validates: Requirements 5.5**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "meld/daemon/language_service.hpp"

#include <string>
#include <sstream>
#include <vector>
#include <regex>
#include <algorithm>

namespace {

using namespace meld::lsp::services;

/// Helper: apply edits to content
std::string apply_edits(const std::string& content,
                        const std::vector<LanguageService::TextEdit>& edits) {
    if (edits.empty()) return content;
    if (edits.size() == 1 && edits[0].start_line == 0 && edits[0].start_character == 0) {
        return edits[0].new_text;
    }
    return content;
}

/// Extract all identifiers from Meld source (ignoring whitespace differences)
std::vector<std::string> extract_identifiers(const std::string& content) {
    std::vector<std::string> ids;
    std::regex id_re(R"(\b([a-zA-Z_][a-zA-Z0-9_]*)\b)");
    auto begin = std::sregex_iterator(content.begin(), content.end(), id_re);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        ids.push_back((*it)[1].str());
    }
    return ids;
}

/// Extract all non-whitespace tokens from source
std::vector<std::string> extract_tokens(const std::string& content) {
    std::vector<std::string> tokens;
    std::regex token_re(R"(\S+)");
    auto begin = std::sregex_iterator(content.begin(), content.end(), token_re);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        tokens.push_back((*it)[0].str());
    }
    return tokens;
}

} // anonymous namespace

/**
 * Property 25.1: Formatting preserves all identifiers in the same order.
 */
TEST(SemanticPreservationPropertyTest, IdentifiersPreserved) {
    rc::check("Formatting preserves all identifiers in order",
        []() {
            int num_funcs = *rc::gen::inRange(1, 4);

            std::ostringstream oss;
            for (int i = 0; i < num_funcs; ++i) {
                int bad_indent = *rc::gen::inRange(0, 8);
                std::string spaces(bad_indent, ' ');
                oss << spaces << "fnc calc_" << i << "(val: Int) -> Int {\n";
                int body_indent = *rc::gen::inRange(0, 12);
                std::string body_spaces(body_indent, ' ');
                oss << body_spaces << "let result = val\n";
                oss << body_spaces << "return result\n";
                int close_indent = *rc::gen::inRange(0, 6);
                std::string close_spaces(close_indent, ' ');
                oss << close_spaces << "}\n\n";
            }

            std::string source = oss.str();
            auto original_ids = extract_identifiers(source);

            LanguageService service;
            auto edits = service.format_document("file:///test.meld", source);
            std::string formatted = apply_edits(source, edits);

            auto formatted_ids = extract_identifiers(formatted);

            // All identifiers must be preserved in the same order
            RC_ASSERT(original_ids == formatted_ids);
        }
    );
}

/**
 * Property 25.2: Formatting preserves all non-whitespace tokens.
 */
TEST(SemanticPreservationPropertyTest, TokensPreserved) {
    rc::check("Formatting preserves all non-whitespace tokens",
        []() {
            int num_vars = *rc::gen::inRange(1, 5);

            std::ostringstream oss;
            oss << "fnc process() {\n";
            for (int i = 0; i < num_vars; ++i) {
                int bad_indent = *rc::gen::inRange(0, 10);
                std::string spaces(bad_indent, ' ');
                oss << spaces << "let x_" << i << " = " << (i * 10) << "\n";
            }
            oss << "}\n";

            std::string source = oss.str();
            auto original_tokens = extract_tokens(source);

            LanguageService service;
            auto edits = service.format_document("file:///test.meld", source);
            std::string formatted = apply_edits(source, edits);

            auto formatted_tokens = extract_tokens(formatted);

            RC_ASSERT(original_tokens == formatted_tokens);
        }
    );
}

/**
 * Property 25.3: Formatting preserves the number of function declarations.
 */
TEST(SemanticPreservationPropertyTest, FunctionCountPreserved) {
    rc::check("Formatting preserves the number of function declarations",
        []() {
            int num_funcs = *rc::gen::inRange(1, 6);

            std::ostringstream oss;
            for (int i = 0; i < num_funcs; ++i) {
                int indent = *rc::gen::inRange(0, 8);
                std::string spaces(indent, ' ');
                oss << spaces << "fnc op_" << i << "() {\n";
                oss << spaces << "  return " << i << "\n";
                oss << spaces << "}\n";
                // Random number of blank lines between functions
                int blanks = *rc::gen::inRange(0, 5);
                for (int b = 0; b < blanks; ++b) oss << "\n";
            }

            std::string source = oss.str();

            // Count "fnc" keywords before formatting
            std::regex fnc_re(R"(\bfnc\b)");
            auto orig_begin = std::sregex_iterator(source.begin(), source.end(), fnc_re);
            auto orig_end = std::sregex_iterator();
            int orig_count = static_cast<int>(std::distance(orig_begin, orig_end));

            LanguageService service;
            auto edits = service.format_document("file:///test.meld", source);
            std::string formatted = apply_edits(source, edits);

            auto fmt_begin = std::sregex_iterator(formatted.begin(), formatted.end(), fnc_re);
            auto fmt_end = std::sregex_iterator();
            int fmt_count = static_cast<int>(std::distance(fmt_begin, fmt_end));

            RC_ASSERT(orig_count == fmt_count);
        }
    );
}

/**
 * Property 25.4: Formatting preserves string literal contents exactly.
 */
TEST(SemanticPreservationPropertyTest, StringLiteralsPreserved) {
    rc::check("Formatting preserves string literal contents",
        []() {
            int num_strings = *rc::gen::inRange(1, 4);

            std::ostringstream oss;
            oss << "fnc strings() {\n";
            for (int i = 0; i < num_strings; ++i) {
                int indent = *rc::gen::inRange(0, 10);
                std::string spaces(indent, ' ');
                oss << spaces << "let s_" << i << " = " << '"' << "hello_" << i << '"' << "\n";
            }
            oss << "}\n";

            std::string source = oss.str();

            // Extract string literals
            std::regex str_re(R"re("([^"]*)")re");
            std::vector<std::string> orig_strings;
            {
                auto begin = std::sregex_iterator(source.begin(), source.end(), str_re);
                auto end = std::sregex_iterator();
                for (auto it = begin; it != end; ++it) {
                    orig_strings.push_back((*it)[0].str());
                }
            }

            LanguageService service;
            auto edits = service.format_document("file:///test.meld", source);
            std::string formatted = apply_edits(source, edits);

            std::vector<std::string> fmt_strings;
            {
                auto begin = std::sregex_iterator(formatted.begin(), formatted.end(), str_re);
                auto end = std::sregex_iterator();
                for (auto it = begin; it != end; ++it) {
                    fmt_strings.push_back((*it)[0].str());
                }
            }

            RC_ASSERT(orig_strings == fmt_strings);
        }
    );
}
