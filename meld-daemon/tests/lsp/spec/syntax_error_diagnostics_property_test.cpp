/**
 * **Feature: meld-lsp-server, Property 3: Syntax error diagnostics**
 *
 * For any Meld code containing syntax errors, the LSP server should provide
 * diagnostic information with precise error locations.
 *
 * **Validates: Requirements 1.3**
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

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Count lines in source (0-based count of newlines + 1)
int count_lines(const std::string& src) {
    if (src.empty()) return 0;
    int lines = 1;
    for (char c : src) {
        if (c == '\n') lines++;
    }
    return lines;
}

/// Get the length of a specific 0-based line
int line_length(const std::string& src, int target_line) {
    int line = 0;
    int len = 0;
    for (size_t i = 0; i < src.size(); ++i) {
        if (src[i] == '\n') {
            if (line == target_line) return len;
            line++;
            len = 0;
        } else {
            len++;
        }
    }
    if (line == target_line) return len;
    return 0;
}

// ---------------------------------------------------------------------------
// Generators
// ---------------------------------------------------------------------------

/// Generate a simple identifier
rc::Gen<std::string> genId() {
    return rc::gen::map(
        rc::gen::inRange(1, 8),
        [](int len) {
            std::string s;
            s.reserve(len);
            for (int i = 0; i < len; ++i) {
                s += static_cast<char>('a' + ((i * 7 + 3) % 26));
            }
            return s;
        }
    );
}

/// Generate code with an unclosed string literal
rc::Gen<std::string> genUnclosedString() {
    return rc::gen::map(
        genId(),
        [](const std::string& name) {
            return "let " + name + " = \"hello world\nlet y = 42";
        }
    );
}

/// Generate code with missing closing brace
rc::Gen<std::string> genMissingCloseBrace() {
    return rc::gen::map(
        genId(),
        [](const std::string& name) {
            return "fnc " + name + "() {\n    let x = 10\n    let y = 20";
        }
    );
}

/// Generate code with missing closing paren
rc::Gen<std::string> genMissingCloseParen() {
    return rc::gen::map(
        genId(),
        [](const std::string& name) {
            return "fnc " + name + "(x: Int, y: Int {\n    return x\n}";
        }
    );
}

/// Generate code with an invalid token character
rc::Gen<std::string> genInvalidToken() {
    return rc::gen::map(
        rc::gen::tuple(genId(), rc::gen::elementOf(
            std::vector<char>{'`', '$'})),
        [](const std::tuple<std::string, char>& t) {
            auto [name, bad_char] = t;
            return "let " + name + " = 42\n" +
                   std::string(1, bad_char) + " invalid\nlet z = 10";
        }
    );
}

/// Generate valid Meld code (should produce no diagnostics)
rc::Gen<std::string> genValidCode() {
    return rc::gen::map(
        rc::gen::tuple(genId(), rc::gen::inRange(0, 999)),
        [](const std::tuple<std::string, int>& t) {
            auto [name, val] = t;
            return "let " + name + " = " + std::to_string(val) + "\n" +
                   "fnc add(a: Int, b: Int) {\n    return a\n}\n";
        }
    );
}

/// Generate code with multiple different syntax errors
rc::Gen<std::string> genMultipleErrors() {
    return rc::gen::map(
        rc::gen::tuple(genId(), genId()),
        [](const std::tuple<std::string, std::string>& t) {
            auto [n1, n2] = t;
            std::ostringstream oss;
            oss << "let " << n1 << " = \"unclosed\n";
            oss << "fnc " << n2 << "() {\n";
            oss << "    let x = 10\n";
            // missing closing brace
            return oss.str();
        }
    );
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

/**
 * Property 3a: Code with unclosed strings produces non-empty diagnostics.
 */
TEST(SyntaxErrorDiagnosticsPropertyTest, UnclosedStringProducesDiagnostics) {
    rc::check("Unclosed string literals must produce diagnostics",
        []() {
            auto source = *genUnclosedString();

            LanguageService service;
            auto diags = service.get_diagnostics("file:///test.meld", source);

            RC_ASSERT(!diags.empty());

            // All diagnostics should have Error severity
            for (const auto& d : diags) {
                RC_ASSERT(d.severity == DiagnosticSeverity::Error);
            }
        }
    );
}

/**
 * Property 3b: Code with missing closing braces produces non-empty diagnostics.
 */
TEST(SyntaxErrorDiagnosticsPropertyTest, MissingBraceProducesDiagnostics) {
    rc::check("Missing closing braces must produce diagnostics",
        []() {
            auto source = *genMissingCloseBrace();

            LanguageService service;
            auto diags = service.get_diagnostics("file:///test.meld", source);

            RC_ASSERT(!diags.empty());

            // Should mention unclosed brace
            bool has_brace_error = std::any_of(
                diags.begin(), diags.end(),
                [](const Diagnostic& d) {
                    return d.message.find("Unclosed") != std::string::npos ||
                           d.message.find("brace") != std::string::npos ||
                           d.message.find("'{'") != std::string::npos;
                });
            RC_ASSERT(has_brace_error);
        }
    );
}

/**
 * Property 3c: Code with invalid tokens produces non-empty diagnostics.
 */
TEST(SyntaxErrorDiagnosticsPropertyTest, InvalidTokenProducesDiagnostics) {
    rc::check("Invalid token characters must produce diagnostics",
        []() {
            auto source = *genInvalidToken();

            LanguageService service;
            auto diags = service.get_diagnostics("file:///test.meld", source);

            RC_ASSERT(!diags.empty());

            bool has_invalid = std::any_of(
                diags.begin(), diags.end(),
                [](const Diagnostic& d) {
                    return d.message.find("Invalid") != std::string::npos;
                });
            RC_ASSERT(has_invalid);
        }
    );
}

/**
 * Property 3d: Diagnostic locations are within source code bounds.
 *
 * For any code with syntax errors, every diagnostic's line and character
 * positions must be within the bounds of the source text.
 */
TEST(SyntaxErrorDiagnosticsPropertyTest, DiagnosticLocationsWithinBounds) {
    rc::check("Diagnostic locations must be within source bounds",
        []() {
            auto source = *rc::gen::oneOf(
                genUnclosedString(),
                genMissingCloseBrace(),
                genMissingCloseParen(),
                genInvalidToken(),
                genMultipleErrors()
            );

            LanguageService service;
            auto diags = service.get_diagnostics("file:///test.meld", source);

            int total = count_lines(source);

            for (const auto& d : diags) {
                // Line must be within [0, total_lines - 1]
                RC_ASSERT(d.line >= 0);
                RC_ASSERT(d.line < total);
                RC_ASSERT(d.end_line >= 0);
                RC_ASSERT(d.end_line < total);

                // Character must be within [0, line_length]
                RC_ASSERT(d.character >= 0);
                RC_ASSERT(d.character <= line_length(source, d.line));
                RC_ASSERT(d.end_character >= 0);
                RC_ASSERT(d.end_character <= line_length(source, d.end_line));
            }
        }
    );
}

/**
 * Property 3e: Valid code produces no diagnostics.
 *
 * For any syntactically valid Meld code, get_diagnostics must return
 * an empty vector.
 */
TEST(SyntaxErrorDiagnosticsPropertyTest, ValidCodeProducesNoDiagnostics) {
    rc::check("Valid Meld code must produce no diagnostics",
        []() {
            auto source = *genValidCode();

            LanguageService service;
            auto diags = service.get_diagnostics("file:///test.meld", source);

            RC_ASSERT(diags.empty());
        }
    );
}

/**
 * Property 3f: All diagnostics have source set to "meld".
 */
TEST(SyntaxErrorDiagnosticsPropertyTest, DiagnosticsHaveMeldSource) {
    rc::check("All diagnostics must have source 'meld'",
        []() {
            auto source = *rc::gen::oneOf(
                genUnclosedString(),
                genMissingCloseBrace(),
                genInvalidToken()
            );

            LanguageService service;
            auto diags = service.get_diagnostics("file:///test.meld", source);

            for (const auto& d : diags) {
                RC_ASSERT(d.source == "meld");
            }
        }
    );
}

/**
 * Property 3g: Multiple errors produce multiple diagnostics.
 */
TEST(SyntaxErrorDiagnosticsPropertyTest, MultipleErrorsProduceMultipleDiagnostics) {
    rc::check("Code with multiple syntax errors must produce multiple diagnostics",
        []() {
            auto source = *genMultipleErrors();

            LanguageService service;
            auto diags = service.get_diagnostics("file:///test.meld", source);

            // Should have at least 2 diagnostics (unclosed string + unclosed brace)
            RC_ASSERT(diags.size() >= 2);
        }
    );
}
