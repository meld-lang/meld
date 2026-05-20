/**
 * **Feature: meld-daemon, Property 23: Syntax Error Diagnostics**
 *
 * For any Meld code containing syntax errors, the LspChannel SHALL provide
 * diagnostic information with precise error locations.
 *
 * **Validates: Requirements 15.3**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "meld/daemon/semantic_token_provider.hpp"

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

namespace meld::daemon {
namespace {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

int count_lines(const std::string& src) {
    if (src.empty()) return 0;
    int lines = 1;
    for (char c : src) {
        if (c == '\n') lines++;
    }
    return lines;
}

// ---------------------------------------------------------------------------
// Generators
// ---------------------------------------------------------------------------

rc::Gen<std::string> genId() {
    return rc::gen::map(
        rc::gen::inRange(1, 8),
        [](int len) {
            std::string s;
            for (int i = 0; i < len; ++i)
                s += static_cast<char>('a' + ((i * 7 + 3) % 26));
            return s;
        }
    );
}

rc::Gen<std::string> genUnclosedString() {
    return rc::gen::map(genId(), [](const std::string& name) {
        return "val " + name + " = \"hello world\nval y = 42";
    });
}

rc::Gen<std::string> genMissingCloseBrace() {
    return rc::gen::map(genId(), [](const std::string& name) {
        return "fnc " + name + "() {\n    val x = 10\n    val y = 20";
    });
}

rc::Gen<std::string> genMissingCloseParen() {
    return rc::gen::map(genId(), [](const std::string& name) {
        return "fnc " + name + "(x, y {\n    return x\n}";
    });
}

rc::Gen<std::string> genInvalidToken() {
    return rc::gen::map(
        rc::gen::tuple(genId(), rc::gen::elementOf(
            std::vector<char>{'`', '#'})),
        [](const std::tuple<std::string, char>& t) {
            auto [name, bad_char] = t;
            return "val " + name + " = 42\n" +
                   std::string(1, bad_char) + " invalid\nval z = 10";
        }
    );
}

rc::Gen<std::string> genMultipleErrors() {
    return rc::gen::map(
        rc::gen::tuple(genId(), genId()),
        [](const std::tuple<std::string, std::string>& t) {
            auto [n1, n2] = t;
            std::ostringstream oss;
            oss << "val " << n1 << " = \"unclosed\n";
            oss << "fnc " << n2 << "() {\n";
            oss << "    val x = 10\n";
            return oss.str();
        }
    );
}

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

/**
 * Property 23a: Code with unclosed strings produces non-empty diagnostics.
 */
RC_GTEST_PROP(SyntaxErrorDiagnosticsProperty,
              UnclosedStringProducesDiagnostics,
              ()) {
    auto source = *genUnclosedString();

    SemanticTokenProvider provider;
    auto result = provider.parse(source);

    RC_ASSERT(!result.errors.empty());
    RC_ASSERT(!result.diagnostics.empty());

    bool has_string_error = std::any_of(
        result.diagnostics.begin(), result.diagnostics.end(),
        [](const ParseDiagnostic& d) {
            return d.message.find("Unterminated string") != std::string::npos;
        });
    RC_ASSERT(has_string_error);
}

/**
 * Property 23b: Code with missing closing braces produces diagnostics.
 */
RC_GTEST_PROP(SyntaxErrorDiagnosticsProperty,
              MissingBraceProducesDiagnostics,
              ()) {
    auto source = *genMissingCloseBrace();

    SemanticTokenProvider provider;
    auto result = provider.parse(source);

    RC_ASSERT(!result.errors.empty());
    RC_ASSERT(!result.diagnostics.empty());

    bool has_brace_error = std::any_of(
        result.diagnostics.begin(), result.diagnostics.end(),
        [](const ParseDiagnostic& d) {
            return d.message.find("brace") != std::string::npos ||
                   d.message.find("Unbalanced") != std::string::npos;
        });
    RC_ASSERT(has_brace_error);
}

/**
 * Property 23c: Code with invalid tokens produces diagnostics with locations.
 */
RC_GTEST_PROP(SyntaxErrorDiagnosticsProperty,
              InvalidTokenProducesDiagnostics,
              ()) {
    auto source = *genInvalidToken();

    SemanticTokenProvider provider;
    auto result = provider.parse(source);

    RC_ASSERT(!result.diagnostics.empty());

    bool has_unexpected = std::any_of(
        result.diagnostics.begin(), result.diagnostics.end(),
        [](const ParseDiagnostic& d) {
            return d.message.find("Unexpected") != std::string::npos;
        });
    RC_ASSERT(has_unexpected);
}

/**
 * Property 23d: Diagnostic locations are within source bounds.
 */
RC_GTEST_PROP(SyntaxErrorDiagnosticsProperty,
              DiagnosticLocationsWithinBounds,
              ()) {
    auto source = *rc::gen::oneOf(
        genUnclosedString(),
        genMissingCloseBrace(),
        genMissingCloseParen(),
        genInvalidToken(),
        genMultipleErrors()
    );

    SemanticTokenProvider provider;
    auto result = provider.parse(source);

    int total_lines = count_lines(source);

    for (const auto& d : result.diagnostics) {
        RC_ASSERT(d.line >= 0);
        RC_ASSERT(d.line < total_lines);
        RC_ASSERT(d.column >= 0);
        RC_ASSERT(d.end_line >= 0);
        RC_ASSERT(d.end_line < total_lines);
        RC_ASSERT(d.end_column >= 0);
    }
}

/**
 * Property 23e: All diagnostics have source set to "meld".
 */
RC_GTEST_PROP(SyntaxErrorDiagnosticsProperty,
              DiagnosticsHaveMeldSource,
              ()) {
    auto source = *rc::gen::oneOf(
        genUnclosedString(),
        genMissingCloseBrace(),
        genInvalidToken()
    );

    SemanticTokenProvider provider;
    auto result = provider.parse(source);

    for (const auto& d : result.diagnostics) {
        RC_ASSERT(d.source == "meld");
    }
}

/**
 * Property 23f: Multiple errors produce multiple diagnostics.
 */
RC_GTEST_PROP(SyntaxErrorDiagnosticsProperty,
              MultipleErrorsProduceMultipleDiagnostics,
              ()) {
    auto source = *genMultipleErrors();

    SemanticTokenProvider provider;
    auto result = provider.parse(source);

    RC_ASSERT(result.diagnostics.size() >= 2);
}

}  // namespace
}  // namespace meld::daemon
