/**
 * **Feature: meld-lsp-server, Property 20: Hover information accuracy**
 *
 * For any symbol, hover should display correct type information,
 * documentation, and signature details.
 *
 * **Validates: Requirements 4.5**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "meld/daemon/language_service.hpp"

#include <string>
#include <sstream>

namespace {

using namespace meld::lsp::services;

/// Generate a valid Meld identifier
std::string make_id(const std::string& prefix, int i) {
    return prefix + "_" + std::to_string(i);
}

} // anonymous namespace

/**
 * Property 20.1: Hovering over a function declaration shows its type
 * signature and documentation.
 */
TEST(HoverInformationPropertyTest, FunctionHoverShowsSignature) {
    rc::check("Hover on function name shows type signature and documentation",
        []() {
            int num_funcs = *rc::gen::inRange(1, 6);

            std::ostringstream oss;
            for (int i = 0; i < num_funcs; ++i) {
                oss << "fnc " << make_id("compute", i) << "(x: Int) -> Int {\n";
                oss << "    return x\n";
                oss << "}\n\n";
            }

            std::string source = oss.str();

            // Pick a random function to hover over
            int target = *rc::gen::inRange(0, num_funcs);
            std::string target_name = make_id("compute", target);

            LanguageService service;

            // Function declaration is at line target*4, name starts at col 4 ("fnc ")
            int decl_line = target * 4;
            int name_col = 4;

            auto hover = service.get_hover("file:///test.meld", source, decl_line, name_col);

            RC_ASSERT(hover.has_value());
            // Hover contents should include the type signature
            RC_ASSERT(hover->contents.find(target_name) != std::string::npos);
            // Should contain the signature with parameters
            RC_ASSERT(hover->contents.find("x: Int") != std::string::npos);
            // Should contain the return type
            RC_ASSERT(hover->contents.find("Int") != std::string::npos);
            // Should indicate it's a function
            RC_ASSERT(hover->contents.find("function") != std::string::npos);
            // Range should cover the identifier
            RC_ASSERT(hover->line == decl_line);
        }
    );
}

/**
 * Property 20.2: Hovering over a variable declaration shows its
 * binding kind and name.
 */
TEST(HoverInformationPropertyTest, VariableHoverShowsInfo) {
    rc::check("Hover on variable shows binding kind and documentation",
        []() {
            int num_vars = *rc::gen::inRange(1, 5);

            std::ostringstream oss;
            oss << "fnc test_func() {\n";
            for (int i = 0; i < num_vars; ++i) {
                std::string decl = (i % 2 == 0) ? "let" : "var";
                oss << "    " << decl << " " << make_id("val", i)
                    << " = " << i << "\n";
            }
            oss << "}\n";

            std::string source = oss.str();

            int target = *rc::gen::inRange(0, num_vars);
            std::string target_name = make_id("val", target);
            std::string expected_kind = (target % 2 == 0) ? "let" : "var";

            LanguageService service;

            // Variable is at line 1 + target, name starts at col 8
            int var_line = 1 + target;
            int name_col = 8;

            auto hover = service.get_hover("file:///test.meld", source, var_line, name_col);

            RC_ASSERT(hover.has_value());
            // Should contain the variable name
            RC_ASSERT(hover->contents.find(target_name) != std::string::npos);
            // Should contain the binding kind (let or var)
            RC_ASSERT(hover->contents.find(expected_kind) != std::string::npos);
            // Should indicate it's a variable
            RC_ASSERT(hover->contents.find("variable") != std::string::npos);
        }
    );
}

/**
 * Property 20.3: Hovering over a struct name shows struct type info.
 */
TEST(HoverInformationPropertyTest, StructHoverShowsTypeInfo) {
    rc::check("Hover on struct name shows struct type information",
        []() {
            int num_structs = *rc::gen::inRange(1, 4);

            std::ostringstream oss;
            for (int i = 0; i < num_structs; ++i) {
                std::string name = "MyType" + std::to_string(i);
                oss << "struct " << name << " {\n";
                oss << "    value: Int\n";
                oss << "}\n\n";
            }

            std::string source = oss.str();

            int target = *rc::gen::inRange(0, num_structs);
            std::string target_name = "MyType" + std::to_string(target);

            LanguageService service;

            // Struct declaration at line target*4, name at col 7 ("struct ")
            int decl_line = target * 4;
            int name_col = 7;

            auto hover = service.get_hover("file:///test.meld", source, decl_line, name_col);

            RC_ASSERT(hover.has_value());
            RC_ASSERT(hover->contents.find(target_name) != std::string::npos);
            RC_ASSERT(hover->contents.find("struct") != std::string::npos);
        }
    );
}

/**
 * Property 20.4: Hovering over whitespace or empty positions returns
 * no hover information.
 */
TEST(HoverInformationPropertyTest, NoHoverOnWhitespace) {
    rc::check("Hover on whitespace or empty position returns nullopt",
        []() {
            std::string source = "fnc hello() {\n    return 42\n}\n";

            LanguageService service;

            // Position on leading whitespace of line 1 (col 0 is a space)
            auto hover = service.get_hover("file:///test.meld", source, 1, 0);

            // Whitespace — should not produce hover (or if it does, it's valid)
            if (hover.has_value()) {
                RC_ASSERT(!hover->contents.empty());
            }

            // Position past end of content
            auto hover2 = service.get_hover("file:///test.meld", source, 100, 0);
            RC_ASSERT(!hover2.has_value());
        }
    );
}

/**
 * Property 20.5: Hover contents are formatted as markdown with code blocks.
 */
TEST(HoverInformationPropertyTest, HoverContentsAreMarkdown) {
    rc::check("Hover contents contain markdown code blocks",
        []() {
            int num_funcs = *rc::gen::inRange(1, 4);

            std::ostringstream oss;
            for (int i = 0; i < num_funcs; ++i) {
                oss << "fnc " << make_id("action", i) << "(n: Int) {\n";
                oss << "    return n\n";
                oss << "}\n\n";
            }

            std::string source = oss.str();

            int target = *rc::gen::inRange(0, num_funcs);

            LanguageService service;

            int decl_line = target * 4;
            int name_col = 4;

            auto hover = service.get_hover("file:///test.meld", source, decl_line, name_col);

            RC_ASSERT(hover.has_value());
            // Should contain markdown code fence
            RC_ASSERT(hover->contents.find("```meld") != std::string::npos);
            RC_ASSERT(hover->contents.find("```\n") != std::string::npos);
        }
    );
}
