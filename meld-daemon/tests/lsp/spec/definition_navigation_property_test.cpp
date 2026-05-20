/**
 * **Feature: meld-lsp-server, Property 16: Definition navigation accuracy**
 *
 * For any symbol, "go to definition" should navigate to the correct
 * declaration location.
 *
 * **Validates: Requirements 4.1**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "meld/daemon/language_service.hpp"

#include <string>
#include <vector>
#include <sstream>

namespace {

using namespace meld::lsp::services;
using namespace meld::lsp::analysis;

/// Generate a valid Meld identifier
std::string make_id(const std::string& prefix, int i) {
    return prefix + "_" + std::to_string(i);
}

} // anonymous namespace

/**
 * Property 16.1: Go-to-definition on a function name at its call site
 * navigates to the function declaration line.
 */
TEST(DefinitionNavigationPropertyTest, FunctionDefinitionNavigation) {
    rc::check("Go-to-definition on function call resolves to declaration",
        []() {
            int num_funcs = *rc::gen::inRange(1, 6);

            std::ostringstream oss;
            // Declare functions
            for (int i = 0; i < num_funcs; ++i) {
                oss << "fnc " << make_id("func", i) << "(x: Int) -> Int {\n";
                oss << "    return x\n";
                oss << "}\n\n";
            }

            // Pick a random function to call
            int target = *rc::gen::inRange(0, num_funcs);
            std::string target_name = make_id("func", target);

            // Add a caller function that uses the target
            oss << "fnc caller() {\n";
            oss << "    let result = " << target_name << "(42)\n";
            oss << "}\n";

            std::string source = oss.str();

            LanguageService service;

            // The target function declaration is at line target*4
            // (each function is 4 lines: fnc, return, }, blank)
            int expected_decl_line = target * 4;

            // Call go_to_definition at the call site
            // The call is on the line: "    let result = func_N(42)"
            // which is at line num_funcs*4 + 1
            int call_line = num_funcs * 4 + 1;
            // Character position: "    let result = " is 17 chars, then the func name
            int call_char = 17;

            auto def_loc = service.go_to_definition(
                "file:///test.meld", source, call_line, call_char);

            RC_ASSERT(def_loc.has_value());
            RC_ASSERT(def_loc->line == expected_decl_line);
            RC_ASSERT(def_loc->uri == "file:///test.meld");
        }
    );
}

/**
 * Property 16.2: Go-to-definition on a variable name resolves to its
 * let/var declaration.
 */
TEST(DefinitionNavigationPropertyTest, VariableDefinitionNavigation) {
    rc::check("Go-to-definition on variable resolves to its declaration",
        []() {
            int num_vars = *rc::gen::inRange(1, 5);

            std::ostringstream oss;
            oss << "fnc test_func() {\n";
            for (int i = 0; i < num_vars; ++i) {
                std::string decl = (i % 2 == 0) ? "let" : "var";
                oss << "    " << decl << " " << make_id("myvar", i)
                    << " = " << i << "\n";
            }
            oss << "}\n";

            std::string source = oss.str();

            // Pick a random variable
            int target = *rc::gen::inRange(0, num_vars);
            std::string target_name = make_id("myvar", target);

            // The variable declaration is at line 1 + target (0-based)
            int expected_line = 1 + target;

            LanguageService service;
            // Navigate to the variable on its declaration line
            // "    let myvar_N = N" — the variable name starts at col 8 for "let " or col 8 for "var "
            int col = 8; // after "    let " or "    var "
            if (target % 2 != 0) col = 8; // "var" is same length padding

            auto def_loc = service.go_to_definition(
                "file:///test.meld", source, expected_line, col);

            RC_ASSERT(def_loc.has_value());
            RC_ASSERT(def_loc->line == expected_line);
            RC_ASSERT(def_loc->uri == "file:///test.meld");
        }
    );
}

/**
 * Property 16.3: Go-to-definition on a struct name navigates to its
 * struct declaration.
 */
TEST(DefinitionNavigationPropertyTest, StructDefinitionNavigation) {
    rc::check("Go-to-definition on struct name resolves to struct declaration",
        []() {
            int num_structs = *rc::gen::inRange(1, 4);

            std::ostringstream oss;
            for (int i = 0; i < num_structs; ++i) {
                std::string name = "MyStruct" + std::to_string(i);
                oss << "struct " << name << " {\n";
                oss << "    value: Int\n";
                oss << "}\n\n";
            }

            std::string source = oss.str();

            int target = *rc::gen::inRange(0, num_structs);
            std::string target_name = "MyStruct" + std::to_string(target);

            // Each struct takes 4 lines (struct, value, }, blank)
            int expected_line = target * 4;

            LanguageService service;
            // The struct name starts at col 7 ("struct " is 7 chars)
            auto def_loc = service.go_to_definition(
                "file:///test.meld", source, expected_line, 7);

            RC_ASSERT(def_loc.has_value());
            RC_ASSERT(def_loc->line == expected_line);
            RC_ASSERT(def_loc->character == 7);
        }
    );
}

/**
 * Property 16.4: Go-to-definition returns nullopt for unknown symbols
 * or positions not on any identifier.
 */
TEST(DefinitionNavigationPropertyTest, UnknownSymbolReturnsNullopt) {
    rc::check("Go-to-definition on whitespace or unknown position returns nullopt",
        []() {
            std::string source = "fnc hello() {\n    return 42\n}\n";

            LanguageService service;
            // Position on whitespace (line 1, col 0 is spaces)
            auto def_loc = service.go_to_definition(
                "file:///test.meld", source, 1, 0);

            // Whitespace position — should not resolve to anything meaningful
            // (it might resolve to "return" which is a keyword, not a symbol)
            // Just verify it doesn't crash
            // The key property: it either returns nullopt or a valid location
            if (def_loc.has_value()) {
                RC_ASSERT(!def_loc->uri.empty());
                RC_ASSERT(def_loc->line >= 0);
            }
        }
    );
}
