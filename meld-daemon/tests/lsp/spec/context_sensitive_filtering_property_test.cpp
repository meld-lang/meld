/**
 * **Feature: meld-lsp-server, Property 10: Context-sensitive filtering**
 *
 * For any syntactic position, completion suggestions should be filtered
 * to exclude inappropriate options for that context.
 *
 * **Validates: Requirements 2.5**
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

/// Helper: check if any completion item has the given label
bool has_label(const CompletionList& completions, const std::string& label) {
    return std::any_of(completions.items.begin(), completions.items.end(),
        [&](const CompletionItem& item) { return item.label == label; });
}

/// Generate a random Meld identifier (lowercase, 2-6 chars)
rc::Gen<std::string> genIdentifier() {
    return rc::gen::map(
        rc::gen::inRange(2, 7),
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

/// Generate a Meld source with cursor in type position (after ':')
/// Returns {source, line, character}
rc::Gen<std::tuple<std::string, int, int>> genTypePositionSource() {
    return rc::gen::map(
        rc::gen::tuple(genIdentifier(), genIdentifier()),
        [](const std::tuple<std::string, std::string>& t) {
            auto [func_name, var_name] = t;
            std::string source = "fnc " + func_name + "() {\n"
                                 "    let " + var_name + ": \n"
                                 "}\n";
            // Cursor is on line 1, after ": "
            int col = 4 + 4 + static_cast<int>(var_name.size()) + 2; // "    let " + var_name + ": "
            return std::make_tuple(source, 1, col);
        }
    );
}

/// Generate a Meld source with cursor in return type position (after '->')
/// Returns {source, line, character}
rc::Gen<std::tuple<std::string, int, int>> genReturnTypePositionSource() {
    return rc::gen::map(
        genIdentifier(),
        [](const std::string& func_name) {
            std::string source = "fnc " + func_name + "() -> \n{\n}\n";
            // Cursor is on line 0, after "-> "
            int col = 4 + static_cast<int>(func_name.size()) + 6; // "fnc " + name + "() -> "
            return std::make_tuple(source, 0, col);
        }
    );
}

/// Generate a top-level Meld source with some functions defined
/// Returns {source, cursor_line} where cursor_line is at top-level
rc::Gen<std::pair<std::string, int>> genTopLevelSource() {
    return rc::gen::map(
        rc::gen::inRange(0, 4),
        [](int num_funcs) {
            std::ostringstream oss;
            for (int i = 0; i < num_funcs; ++i) {
                oss << "fnc func_" << i << "() {\n    let x = 1\n}\n\n";
            }
            oss << "\n";
            int line_count = 0;
            std::string s = oss.str();
            for (char c : s) {
                if (c == '\n') line_count++;
            }
            return std::make_pair(s, std::max(0, line_count - 1));
        }
    );
}

/// Generate a Meld source with cursor after '@' (annotation position)
/// Returns {source, line, character}
rc::Gen<std::tuple<std::string, int, int>> genAnnotationPositionSource() {
    return rc::gen::map(
        genIdentifier(),
        [](const std::string& func_name) {
            std::string source = "@\nfnc " + func_name + "() {\n}\n";
            // Cursor is on line 0, character 1 (right after '@')
            return std::make_tuple(source, 0, 1);
        }
    );
}

/// Generate a Meld source with cursor inside a function body
/// Returns {source, cursor_line}
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
            oss << "    \n";
            int cursor_line = body_line;
            oss << "}\n";
            return std::make_pair(oss.str(), cursor_line);
        }
    );
}

} // anonymous namespace


/**
 * Property 10.1: Type position excludes non-type items
 *
 * In type position (after ':' or '->'), completions should NOT include
 * keywords like let, var, if, return, or function names.
 */
TEST(ContextSensitiveFilteringPropertyTest, TypePositionExcludesNonTypeItems) {
    rc::check("Type position completions must exclude non-type items",
        []() {
            auto [source, cursor_line, cursor_char] = *genTypePositionSource();

            LanguageService service;
            auto completions = service.get_completions("file:///test.meld", source, cursor_line, cursor_char);

            // Property: type position must not include body/control-flow keywords
            RC_ASSERT(!has_label(completions, "let"));
            RC_ASSERT(!has_label(completions, "var"));
            RC_ASSERT(!has_label(completions, "if"));
            RC_ASSERT(!has_label(completions, "else"));
            RC_ASSERT(!has_label(completions, "return"));
            RC_ASSERT(!has_label(completions, "for"));
            RC_ASSERT(!has_label(completions, "while"));
            RC_ASSERT(!has_label(completions, "match"));

            // Property: type position must not include top-level-only keywords
            RC_ASSERT(!has_label(completions, "fnc"));
            RC_ASSERT(!has_label(completions, "struct"));
            RC_ASSERT(!has_label(completions, "enum"));
            RC_ASSERT(!has_label(completions, "trait"));
            RC_ASSERT(!has_label(completions, "import"));

            // Property: all items should be of Type kind
            for (const auto& item : completions.items) {
                RC_ASSERT(item.kind == CompletionItemKind::Type);
            }
        }
    );
}

/**
 * Property 10.1b: Return type position also excludes non-type items
 *
 * After '->', completions should only contain type names.
 */
TEST(ContextSensitiveFilteringPropertyTest, ReturnTypePositionExcludesNonTypeItems) {
    rc::check("Return type position completions must exclude non-type items",
        []() {
            auto [source, cursor_line, cursor_char] = *genReturnTypePositionSource();

            LanguageService service;
            auto completions = service.get_completions("file:///test.meld", source, cursor_line, cursor_char);

            // Property: return type position must not include keywords
            RC_ASSERT(!has_label(completions, "let"));
            RC_ASSERT(!has_label(completions, "var"));
            RC_ASSERT(!has_label(completions, "if"));
            RC_ASSERT(!has_label(completions, "return"));
            RC_ASSERT(!has_label(completions, "fnc"));
            RC_ASSERT(!has_label(completions, "import"));

            // Property: all items should be of Type kind
            for (const auto& item : completions.items) {
                RC_ASSERT(item.kind == CompletionItemKind::Type);
            }
        }
    );
}

/**
 * Property 10.2: Top-level excludes body-only keywords
 *
 * At top-level (brace depth 0), completions should NOT include
 * body-only keywords like if, else, match, return, for, while.
 */
TEST(ContextSensitiveFilteringPropertyTest, TopLevelExcludesBodyOnlyKeywords) {
    rc::check("Top-level completions must exclude body-only keywords",
        []() {
            auto [source, cursor_line] = *genTopLevelSource();

            LanguageService service;
            auto completions = service.get_completions("file:///test.meld", source, cursor_line, 0);

            // Property: top-level must not include body-only keywords
            RC_ASSERT(!has_label(completions, "if"));
            RC_ASSERT(!has_label(completions, "else"));
            RC_ASSERT(!has_label(completions, "match"));
            RC_ASSERT(!has_label(completions, "return"));
            RC_ASSERT(!has_label(completions, "for"));
            RC_ASSERT(!has_label(completions, "while"));

            // Property: top-level must include top-level keywords
            RC_ASSERT(has_label(completions, "fnc"));
            RC_ASSERT(has_label(completions, "struct"));
            RC_ASSERT(has_label(completions, "enum"));
            RC_ASSERT(has_label(completions, "trait"));
            RC_ASSERT(has_label(completions, "import"));
        }
    );
}

/**
 * Property 10.3: Annotation position excludes non-annotations
 *
 * After '@', completions should NOT include regular keywords or type names.
 * Only annotation names (uses, effect, pure) should appear.
 */
TEST(ContextSensitiveFilteringPropertyTest, AnnotationPositionExcludesNonAnnotations) {
    rc::check("Annotation position completions must exclude non-annotation items",
        []() {
            auto [source, cursor_line, cursor_char] = *genAnnotationPositionSource();

            LanguageService service;
            auto completions = service.get_completions("file:///test.meld", source, cursor_line, cursor_char);

            // Property: annotation position must not include regular keywords
            RC_ASSERT(!has_label(completions, "fnc"));
            RC_ASSERT(!has_label(completions, "let"));
            RC_ASSERT(!has_label(completions, "var"));
            RC_ASSERT(!has_label(completions, "if"));
            RC_ASSERT(!has_label(completions, "return"));
            RC_ASSERT(!has_label(completions, "struct"));
            RC_ASSERT(!has_label(completions, "enum"));
            RC_ASSERT(!has_label(completions, "trait"));
            RC_ASSERT(!has_label(completions, "import"));

            // Property: annotation position must not include type names
            RC_ASSERT(!has_label(completions, "Int"));
            RC_ASSERT(!has_label(completions, "Float"));
            RC_ASSERT(!has_label(completions, "String"));
            RC_ASSERT(!has_label(completions, "Bool"));

            // Property: must include annotation names
            RC_ASSERT(has_label(completions, "uses"));
            RC_ASSERT(has_label(completions, "pure"));

            // Property: all items should be Effect kind (annotations)
            for (const auto& item : completions.items) {
                RC_ASSERT(item.kind == CompletionItemKind::Effect);
            }
        }
    );
}

/**
 * Property 10.4: Function body excludes top-level-only keywords
 *
 * Inside a function body (brace depth > 0), completions should NOT include
 * top-level-only keywords like struct, enum, trait, import.
 */
TEST(ContextSensitiveFilteringPropertyTest, FunctionBodyExcludesTopLevelOnlyKeywords) {
    rc::check("Function body completions must exclude top-level-only keywords",
        []() {
            auto [source, cursor_line] = *genFunctionBodySource();

            LanguageService service;
            auto completions = service.get_completions("file:///test.meld", source, cursor_line, 4);

            // Property: function body must not include top-level-only keywords
            RC_ASSERT(!has_label(completions, "struct"));
            RC_ASSERT(!has_label(completions, "enum"));
            RC_ASSERT(!has_label(completions, "trait"));
            RC_ASSERT(!has_label(completions, "import"));
            RC_ASSERT(!has_label(completions, "effect"));

            // Property: function body must include body-level keywords
            RC_ASSERT(has_label(completions, "let"));
            RC_ASSERT(has_label(completions, "var"));
            RC_ASSERT(has_label(completions, "if"));
            RC_ASSERT(has_label(completions, "return"));
        }
    );
}
