/**
 * **Feature: meld-lsp-server, Property 6: Context-aware completions**
 *
 * For any cursor position in a Meld file, completion suggestions should be
 * appropriate for the current scope and context.
 *
 * **Validates: Requirements 2.1**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "meld/daemon/language_service.hpp"

#include <string>
#include <vector>
#include <algorithm>
#include <sstream>

namespace {

using namespace meld::lsp::services;

/// Generate a random Meld identifier (lowercase, 2-8 chars)
rc::Gen<std::string> genIdentifier() {
    return rc::gen::map(
        rc::gen::inRange(2, 9),
        [](int len) {
            std::string result;
            result.reserve(len);
            for (int i = 0; i < len; ++i) {
                result += static_cast<char>('a' + (i % 26));
            }
            return result;
        }
    );
}

/// Generate a top-level Meld source with cursor at top-level (line after all declarations)
rc::Gen<std::string> genTopLevelSource() {
    return rc::gen::map(
        rc::gen::inRange(0, 4),
        [](int num_funcs) {
            std::ostringstream oss;
            for (int i = 0; i < num_funcs; ++i) {
                oss << "fnc func_" << i << "() {\n    let x = 1\n}\n\n";
            }
            // Leave a blank line at the end for cursor placement at top-level
            oss << "\n";
            return oss.str();
        }
    );
}

/// Generate a Meld source with a function body and cursor inside it
/// Returns {source, line_inside_body}
rc::Gen<std::pair<std::string, int>> genFunctionBodySource() {
    return rc::gen::map(
        rc::gen::tuple(genIdentifier(), rc::gen::inRange(0, 4)),
        [](const std::tuple<std::string, int>& t) {
            auto [name, num_vars] = t;
            std::ostringstream oss;
            oss << "fnc " << name << "() {\n";
            int body_line = 1;
            for (int i = 0; i < num_vars; ++i) {
                oss << "    let var_" << i << " = " << i << "\n";
                body_line++;
            }
            // Cursor will be placed on this line (inside the body)
            oss << "    \n";
            int cursor_line = body_line;
            oss << "}\n";
            return std::make_pair(oss.str(), cursor_line);
        }
    );
}

/// Generate a Meld source with a type annotation position
/// Returns {source, line, character} where cursor is after ':'
rc::Gen<std::tuple<std::string, int, int>> genTypePositionSource() {
    return rc::gen::map(
        genIdentifier(),
        [](const std::string& name) {
            std::string source = "fnc " + name + "() {\n    let x: \n}\n";
            // Cursor is on line 1, after ": " (character 11)
            return std::make_tuple(source, 1, 11);
        }
    );
}

/// Generate a Meld source with an annotation position (after @)
rc::Gen<std::pair<std::string, int>> genAnnotationSource() {
    return rc::gen::map(
        genIdentifier(),
        [](const std::string& name) {
            std::string source = "@\nfnc " + name + "() {\n}\n";
            // Cursor is on line 0, after '@' (character 1)
            return std::make_pair(source, 0);
        }
    );
}

} // anonymous namespace

/**
 * Property 6: Top-level completions include top-level keywords
 *
 * For any cursor at top-level scope (brace depth 0), completions
 * must include top-level keywords like fnc, struct, enum, trait, etc.
 */
TEST(ContextAwareCompletionsPropertyTest, TopLevelCompletionsIncludeTopLevelKeywords) {
    rc::check("Top-level cursor positions must produce top-level keyword completions",
        []() {
            auto source = *genTopLevelSource();

            // Count lines to find the last line (top-level)
            int line_count = 0;
            for (char c : source) {
                if (c == '\n') line_count++;
            }
            int cursor_line = std::max(0, line_count - 1);

            LanguageService service;
            auto completions = service.get_completions("file:///test.meld", source, cursor_line, 0);

            // Property: completions must not be empty at top-level
            RC_ASSERT(!completions.items.empty());

            // Property: must include key top-level keywords
            auto has_label = [&](const std::string& label) {
                return std::any_of(completions.items.begin(), completions.items.end(),
                    [&](const CompletionItem& item) { return item.label == label; });
            };

            RC_ASSERT(has_label("fnc"));
            RC_ASSERT(has_label("struct"));
            RC_ASSERT(has_label("enum"));
            RC_ASSERT(has_label("trait"));
            RC_ASSERT(has_label("import"));
            RC_ASSERT(has_label("effect"));

            // Property: all items at top-level should be keywords
            for (const auto& item : completions.items) {
                RC_ASSERT(item.kind == CompletionItemKind::Keyword);
            }
        }
    );
}

/**
 * Property 6: Function body completions include body-level keywords
 *
 * For any cursor inside a function body (brace depth > 0), completions
 * must include body-level keywords like let, var, if, return, etc.
 */
TEST(ContextAwareCompletionsPropertyTest, FunctionBodyCompletionsIncludeBodyKeywords) {
    rc::check("Cursor inside function body must produce body-level keyword completions",
        []() {
            auto [source, cursor_line] = *genFunctionBodySource();

            LanguageService service;
            auto completions = service.get_completions("file:///test.meld", source, cursor_line, 4);

            // Property: completions must not be empty inside function body
            RC_ASSERT(!completions.items.empty());

            // Property: must include body-level keywords
            auto has_label = [&](const std::string& label) {
                return std::any_of(completions.items.begin(), completions.items.end(),
                    [&](const CompletionItem& item) { return item.label == label; });
            };

            RC_ASSERT(has_label("let"));
            RC_ASSERT(has_label("var"));
            RC_ASSERT(has_label("if"));
            RC_ASSERT(has_label("return"));
            RC_ASSERT(has_label("for"));
            RC_ASSERT(has_label("while"));
            RC_ASSERT(has_label("match"));
        }
    );
}

/**
 * Property 6: Type position completions include type names
 *
 * For any cursor in a type annotation position (after ':' or '->'),
 * completions must include built-in type names.
 */
TEST(ContextAwareCompletionsPropertyTest, TypePositionCompletionsIncludeTypeNames) {
    rc::check("Cursor in type position must produce type name completions",
        []() {
            auto [source, cursor_line, cursor_char] = *genTypePositionSource();

            LanguageService service;
            auto completions = service.get_completions("file:///test.meld", source, cursor_line, cursor_char);

            // Property: completions must not be empty in type position
            RC_ASSERT(!completions.items.empty());

            // Property: must include built-in types
            auto has_label = [&](const std::string& label) {
                return std::any_of(completions.items.begin(), completions.items.end(),
                    [&](const CompletionItem& item) { return item.label == label; });
            };

            RC_ASSERT(has_label("Int"));
            RC_ASSERT(has_label("Float"));
            RC_ASSERT(has_label("String"));
            RC_ASSERT(has_label("Bool"));

            // Property: all items in type position should be Type kind
            for (const auto& item : completions.items) {
                RC_ASSERT(item.kind == CompletionItemKind::Type);
            }
        }
    );
}

/**
 * Property 6: Completions are never empty for valid cursor positions
 *
 * For any valid cursor position in a Meld file, the completion list
 * should never be empty — there's always something contextually relevant.
 */
TEST(ContextAwareCompletionsPropertyTest, CompletionsNeverEmptyForValidPositions) {
    rc::check("Completions should never be empty for valid cursor positions in Meld code",
        []() {
            // Generate a variety of Meld sources
            auto num_funcs = *rc::gen::inRange(1, 4);
            std::ostringstream oss;
            for (int i = 0; i < num_funcs; ++i) {
                oss << "fnc func_" << i << "(x: Int) {\n";
                oss << "    let val_" << i << " = x\n";
                oss << "    return val_" << i << "\n";
                oss << "}\n\n";
            }
            std::string source = oss.str();

            // Pick a random line that is not empty and not a closing brace only
            std::vector<int> valid_lines;
            std::istringstream stream(source);
            std::string line_str;
            int ln = 0;
            while (std::getline(stream, line_str)) {
                // Skip empty lines and lines that are just '}'
                std::string trimmed = line_str;
                trimmed.erase(0, trimmed.find_first_not_of(" \t"));
                if (!trimmed.empty() && trimmed != "}") {
                    valid_lines.push_back(ln);
                }
                ln++;
            }
            RC_PRE(!valid_lines.empty());

            int line_idx = *rc::gen::elementOf(valid_lines);

            LanguageService service;
            auto completions = service.get_completions("file:///test.meld", source, line_idx, 0);

            // Property: completions should never be empty
            RC_ASSERT(!completions.items.empty());
        }
    );
}
