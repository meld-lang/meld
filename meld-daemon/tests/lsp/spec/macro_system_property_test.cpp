/**
 * **Feature: meld-lsp-server, Property 31: Macro system support**
 *
 * For any macro definition, the LSP server should provide appropriate
 * language service support for meta-macro system constructs.
 *
 * **Validates: Requirements 7.2**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "meld/daemon/analysis_engine.hpp"

#include <string>
#include <vector>

namespace {

using namespace meld::lsp::analysis;

// ---------------------------------------------------------------------------
// Generators
// ---------------------------------------------------------------------------

rc::Gen<std::string> genMacroName() {
    return rc::gen::map(
        rc::gen::inRange(1, 6),
        [](int len) {
            std::string s;
            for (int i = 0; i < len; ++i) {
                s += static_cast<char>('a' + ((i * 11 + 1) % 26));
            }
            return s;
        }
    );
}

rc::Gen<std::string> genMacroBody() {
    return rc::gen::element(
        std::string("quote { 1 + 2 }"),
        std::string("let x = 42"),
        std::string("return ast"),
        std::string("fnc inner() { return 1 }"));
}

/// Well-formed macro with body
rc::Gen<std::string> genValidMacro() {
    return rc::gen::map(
        rc::gen::tuple(genMacroName(), genMacroBody()),
        [](const std::tuple<std::string, std::string>& t) {
            auto [name, body] = t;
            return "macro " + name + " {\n    " + body + "\n}";
        }
    );
}

/// Well-formed macro with parameters
rc::Gen<std::string> genValidMacroWithParams() {
    return rc::gen::map(
        rc::gen::tuple(genMacroName(), genMacroBody()),
        [](const std::tuple<std::string, std::string>& t) {
            auto [name, body] = t;
            return "macro " + name + "(expr) {\n    " + body + "\n}";
        }
    );
}

/// Macro with empty body (error)
rc::Gen<std::string> genEmptyMacro() {
    return rc::gen::map(
        genMacroName(),
        [](const std::string& name) {
            return "macro " + name + " {\n}";
        }
    );
}

/// Macro with unmatched brace (error)
rc::Gen<std::string> genUnmatchedMacro() {
    return rc::gen::map(
        rc::gen::tuple(genMacroName(), genMacroBody()),
        [](const std::tuple<std::string, std::string>& t) {
            auto [name, body] = t;
            return "macro " + name + " {\n    " + body;
        }
    );
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

TEST(MacroSystemPropertyTest, ValidMacroProducesNoErrors) {
    rc::check("Well-formed macro definitions must produce no errors",
        []() {
            auto source = *genValidMacro();
            AnalysisEngine engine;
            auto result = engine.validate_macro_definitions("file:///test.meld", source);
            RC_ASSERT(!result.has_errors());
            RC_ASSERT(result.macro_count >= 1);
        }
    );
}

TEST(MacroSystemPropertyTest, MacroWithParamsProducesNoErrors) {
    rc::check("Macro with parameters must produce no errors",
        []() {
            auto source = *genValidMacroWithParams();
            AnalysisEngine engine;
            auto result = engine.validate_macro_definitions("file:///test.meld", source);
            RC_ASSERT(!result.has_errors());
            RC_ASSERT(result.macro_count >= 1);
        }
    );
}

TEST(MacroSystemPropertyTest, EmptyMacroBodyProducesError) {
    rc::check("Macro with empty body must produce an error",
        []() {
            auto source = *genEmptyMacro();
            AnalysisEngine engine;
            auto result = engine.validate_macro_definitions("file:///test.meld", source);
            RC_ASSERT(result.has_errors());
            RC_ASSERT(result.errors[0].message.find("empty") != std::string::npos);
        }
    );
}

TEST(MacroSystemPropertyTest, UnmatchedBraceProducesError) {
    rc::check("Macro with unmatched brace must produce an error",
        []() {
            auto source = *genUnmatchedMacro();
            AnalysisEngine engine;
            auto result = engine.validate_macro_definitions("file:///test.meld", source);
            RC_ASSERT(result.has_errors());
            RC_ASSERT(result.errors[0].message.find("unmatched") != std::string::npos);
        }
    );
}

TEST(MacroSystemPropertyTest, NoMacrosProducesNoErrors) {
    rc::check("Code without macros must produce no macro errors",
        []() {
            std::string source = "fnc foo(x: Int) -> Int { return x }";
            AnalysisEngine engine;
            auto result = engine.validate_macro_definitions("file:///test.meld", source);
            RC_ASSERT(!result.has_errors());
            RC_ASSERT(result.macro_count == 0);
        }
    );
}
