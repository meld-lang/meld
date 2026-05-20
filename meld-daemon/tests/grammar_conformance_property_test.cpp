/**
 * **Feature: meld-daemon, Property 31: Grammar Conformance Validation**
 *
 * For any Meld code, syntax validation SHALL conform exactly to the Meld
 * grammar specification.
 *
 * **Validates: Requirements 17.1**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "meld/daemon/diagnostics_provider.hpp"

#include <string>

namespace meld::daemon {
namespace {

// ---------------------------------------------------------------------------
// Generators
// ---------------------------------------------------------------------------

/// Generate a well-formed Meld source snippet with balanced delimiters.
rc::Gen<std::string> genBalancedSource() {
    return rc::gen::map(
        rc::gen::inRange(0, 5),
        [](int depth) {
            std::string src;
            for (int i = 0; i < depth; ++i) {
                src += "fnc f" + std::to_string(i) + "() {\n";
            }
            src += "  val x = 42\n";
            for (int i = 0; i < depth; ++i) {
                src += "}\n";
            }
            return src;
        }
    );
}

/// Generate source with an unbalanced opening delimiter.
rc::Gen<std::pair<std::string, char>> genUnbalancedOpen() {
    return rc::gen::map(
        rc::gen::elementOf(std::vector<char>{'{', '(', '['}),
        [](char open) {
            std::string src = "fnc test() ";
            src += open;
            src += "\n  val x = 1\n";
            // Missing closing delimiter
            return std::make_pair(src, open);
        }
    );
}

/// Generate source with an extra closing delimiter.
rc::Gen<std::pair<std::string, char>> genUnbalancedClose() {
    return rc::gen::map(
        rc::gen::elementOf(std::vector<char>{'}', ')', ']'}),
        [](char close) {
            std::string src = "val x = 1\n";
            src += close;
            src += "\n";
            return std::make_pair(src, close);
        }
    );
}

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

/**
 * Property 31a: Well-formed source with balanced delimiters always conforms.
 */
RC_GTEST_PROP(GrammarConformanceProperty,
              BalancedDelimitersConform,
              ()) {
    auto source = *genBalancedSource();

    SemanticModel model;
    DiagnosticsProvider provider(model);
    auto result = provider.check_grammar("test.meld", source);

    RC_ASSERT(result.conforms);
    RC_ASSERT(result.diagnostics.empty());
}

/**
 * Property 31b: Source with unbalanced opening delimiters is non-conforming.
 */
RC_GTEST_PROP(GrammarConformanceProperty,
              UnbalancedOpenIsNonConforming,
              ()) {
    auto [source, open_char] = *genUnbalancedOpen();

    SemanticModel model;
    DiagnosticsProvider provider(model);
    auto result = provider.check_grammar("test.meld", source);

    RC_ASSERT(!result.conforms);
    RC_ASSERT(!result.diagnostics.empty());
    // All diagnostics should have error code E1000
    for (const auto& diag : result.diagnostics) {
        RC_ASSERT(diag.code == "E1000");
        RC_ASSERT(!diag.suggestion.empty());
    }
}

/**
 * Property 31c: Source with extra closing delimiters is non-conforming.
 */
RC_GTEST_PROP(GrammarConformanceProperty,
              ExtraCloseIsNonConforming,
              ()) {
    auto [source, close_char] = *genUnbalancedClose();

    SemanticModel model;
    DiagnosticsProvider provider(model);
    auto result = provider.check_grammar("test.meld", source);

    RC_ASSERT(!result.conforms);
    RC_ASSERT(!result.diagnostics.empty());
}

/**
 * Property 31d: Empty source always conforms.
 */
RC_GTEST_PROP(GrammarConformanceProperty,
              EmptySourceConforms,
              ()) {
    SemanticModel model;
    DiagnosticsProvider provider(model);
    auto result = provider.check_grammar("test.meld", "");

    RC_ASSERT(result.conforms);
    RC_ASSERT(result.diagnostics.empty());
}

}  // namespace
}  // namespace meld::daemon
