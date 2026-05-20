/**
 * **Feature: meld-lsp-server, Property 30: Homoiconic analysis correctness**
 *
 * For any homoiconic Meld code, the LSP server should understand the
 * relationship between AST representation and runtime values.
 *
 * **Validates: Requirements 7.1**
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

rc::Gen<std::string> genExpr() {
    return rc::gen::element(
        std::string("1 + 2"),
        std::string("x * y"),
        std::string("fnc foo() { return 1 }"),
        std::string("let a = 42"),
        std::string("if x > 0 { x } else { 0 }"));
}

/// Well-formed quote block
rc::Gen<std::string> genValidQuote() {
    return rc::gen::map(
        genExpr(),
        [](const std::string& expr) {
            return "let ast = quote { " + expr + " }";
        }
    );
}

/// Well-formed quote with unquote inside
rc::Gen<std::string> genQuoteWithUnquote() {
    return rc::gen::map(
        rc::gen::element(std::string("x"), std::string("val"), std::string("expr")),
        [](const std::string& var) {
            return "let ast = quote { 1 + unquote(" + var + ") }";
        }
    );
}

/// Unquote outside of quote (error)
rc::Gen<std::string> genOrphanUnquote() {
    return rc::gen::map(
        rc::gen::element(std::string("x"), std::string("val"), std::string("42")),
        [](const std::string& var) {
            return "let result = unquote(" + var + ")";
        }
    );
}

/// Quote with unmatched brace (error)
rc::Gen<std::string> genUnmatchedQuote() {
    return rc::gen::map(
        genExpr(),
        [](const std::string& expr) {
            return "let ast = quote { " + expr;
        }
    );
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

TEST(HomoiconicAnalysisPropertyTest, ValidQuoteProducesNoErrors) {
    rc::check("Well-formed quote blocks must produce no errors",
        []() {
            auto source = *genValidQuote();
            AnalysisEngine engine;
            auto result = engine.validate_homoiconic_constructs("file:///test.meld", source);
            RC_ASSERT(!result.has_errors());
            RC_ASSERT(result.quote_count >= 1);
        }
    );
}

TEST(HomoiconicAnalysisPropertyTest, QuoteWithUnquoteIsValid) {
    rc::check("Quote blocks containing unquote must be valid",
        []() {
            auto source = *genQuoteWithUnquote();
            AnalysisEngine engine;
            auto result = engine.validate_homoiconic_constructs("file:///test.meld", source);
            RC_ASSERT(!result.has_errors());
            RC_ASSERT(result.quote_count >= 1);
            RC_ASSERT(result.unquote_count >= 1);
        }
    );
}

TEST(HomoiconicAnalysisPropertyTest, OrphanUnquoteProducesError) {
    rc::check("Unquote outside of quote must produce an error",
        []() {
            auto source = *genOrphanUnquote();
            AnalysisEngine engine;
            auto result = engine.validate_homoiconic_constructs("file:///test.meld", source);
            RC_ASSERT(result.has_errors());
            RC_ASSERT(result.errors[0].construct == "unquote");
            RC_ASSERT(result.errors[0].message.find("outside") != std::string::npos);
        }
    );
}

TEST(HomoiconicAnalysisPropertyTest, UnmatchedQuoteProducesError) {
    rc::check("Quote with unmatched brace must produce an error",
        []() {
            auto source = *genUnmatchedQuote();
            AnalysisEngine engine;
            auto result = engine.validate_homoiconic_constructs("file:///test.meld", source);
            RC_ASSERT(result.has_errors());
            RC_ASSERT(result.errors[0].construct == "quote");
        }
    );
}

TEST(HomoiconicAnalysisPropertyTest, EmptyContentProducesNoErrors) {
    rc::check("Empty content must produce no homoiconic errors",
        []() {
            AnalysisEngine engine;
            auto result = engine.validate_homoiconic_constructs("file:///test.meld", "");
            RC_ASSERT(!result.has_errors());
            RC_ASSERT(result.quote_count == 0);
            RC_ASSERT(result.unquote_count == 0);
        }
    );
}
