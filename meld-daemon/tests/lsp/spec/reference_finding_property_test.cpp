/**
 * **Feature: meld-lsp-server, Property 17: Reference finding completeness**
 *
 * For any symbol, "find references" should locate all usages across the
 * workspace without missing any.
 *
 * **Validates: Requirements 4.2**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "meld/daemon/language_service.hpp"

#include <string>
#include <vector>
#include <sstream>
#include <algorithm>

namespace {

using namespace meld::lsp::services;
using namespace meld::lsp::analysis;

/// Generate a valid Meld identifier
std::string make_id(const std::string& prefix, int i) {
    return prefix + "_" + std::to_string(i);
}

/// Count occurrences of a whole-word identifier in source
int count_occurrences(const std::string& content, const std::string& name) {
    int count = 0;
    size_t pos = 0;
    while ((pos = content.find(name, pos)) != std::string::npos) {
        // Check word boundaries
        bool left_ok = (pos == 0) ||
            !(std::isalnum(static_cast<unsigned char>(content[pos - 1])) ||
              content[pos - 1] == '_');
        size_t end = pos + name.size();
        bool right_ok = (end >= content.size()) ||
            !(std::isalnum(static_cast<unsigned char>(content[end])) ||
              content[end] == '_');
        if (left_ok && right_ok) count++;
        pos = end;
    }
    return count;
}

} // anonymous namespace

/**
 * Property 17.1: Find references locates all usages of a function name
 * including its declaration and all call sites.
 */
TEST(ReferenceFindingPropertyTest, AllFunctionReferencesFound) {
    rc::check("Find references returns all occurrences of a function name",
        []() {
            int num_calls = *rc::gen::inRange(1, 6);

            std::ostringstream oss;
            oss << "fnc target_fn(x: Int) -> Int {\n";
            oss << "    return x\n";
            oss << "}\n\n";

            oss << "fnc caller() {\n";
            for (int i = 0; i < num_calls; ++i) {
                oss << "    let r" << i << " = target_fn(" << i << ")\n";
            }
            oss << "}\n";

            std::string source = oss.str();
            std::string uri = "file:///test.meld";

            LanguageService service;
            // Find references at the declaration (line 0, col 4 = "target_fn")
            auto refs = service.find_references(uri, source, 0, 4);

            // Expected: 1 declaration + num_calls call sites
            int expected = count_occurrences(source, "target_fn");
            RC_ASSERT(static_cast<int>(refs.size()) == expected);

            // All references should be in the same URI
            for (const auto& ref : refs) {
                RC_ASSERT(ref.uri == uri);
            }
        }
    );
}

/**
 * Property 17.2: Find references for a variable finds its declaration
 * and all usage sites.
 */
TEST(ReferenceFindingPropertyTest, AllVariableReferencesFound) {
    rc::check("Find references returns all occurrences of a variable",
        []() {
            int num_uses = *rc::gen::inRange(1, 5);

            std::ostringstream oss;
            oss << "fnc test_func() {\n";
            oss << "    let my_value = 10\n";
            for (int i = 0; i < num_uses; ++i) {
                oss << "    let use_" << i << " = my_value\n";
            }
            oss << "}\n";

            std::string source = oss.str();
            std::string uri = "file:///test.meld";

            LanguageService service;
            // Find references at the declaration line (line 1, col 8 = "my_value")
            auto refs = service.find_references(uri, source, 1, 8);

            // Expected: 1 declaration + num_uses references
            int expected = count_occurrences(source, "my_value");
            RC_ASSERT(expected == 1 + num_uses);
            RC_ASSERT(static_cast<int>(refs.size()) == expected);
        }
    );
}

/**
 * Property 17.3: Find references returns correct line/column positions
 * for each reference location.
 */
TEST(ReferenceFindingPropertyTest, ReferencePositionsAreAccurate) {
    rc::check("Each reference location has correct line and column",
        []() {
            std::string source =
                "fnc alpha() {\n"       // line 0: alpha at col 4
                "    return 1\n"         // line 1
                "}\n"                    // line 2
                "\n"                     // line 3
                "fnc beta() {\n"         // line 4
                "    let x = alpha()\n"  // line 5: alpha at col 13
                "}\n";                   // line 6

            std::string uri = "file:///test.meld";

            LanguageService service;
            auto refs = service.find_references(uri, source, 0, 4);

            // Should find exactly 2 references to "alpha"
            RC_ASSERT(refs.size() == 2u);

            // Sort by line for deterministic checking
            auto sorted = refs;
            std::sort(sorted.begin(), sorted.end(),
                [](const Location& a, const Location& b) {
                    return a.line < b.line || (a.line == b.line && a.character < b.character);
                });

            // First: declaration at line 0, col 4
            RC_ASSERT(sorted[0].line == 0);
            RC_ASSERT(sorted[0].character == 4);

            // Second: usage at line 5, col 12
            RC_ASSERT(sorted[1].line == 5);
            RC_ASSERT(sorted[1].character == 12);
        }
    );
}

/**
 * Property 17.4: Find references for a symbol that appears zero times
 * at the cursor returns empty results.
 */
TEST(ReferenceFindingPropertyTest, NoReferencesForUnknownSymbol) {
    rc::check("Find references on whitespace returns empty",
        []() {
            std::string source = "fnc hello() {\n    return 42\n}\n";
            std::string uri = "file:///test.meld";

            LanguageService service;
            // Position on empty line or out of range
            auto refs = service.find_references(uri, source, 2, 0);

            // '}' is not an identifier, so should return empty
            RC_ASSERT(refs.empty());
        }
    );
}
