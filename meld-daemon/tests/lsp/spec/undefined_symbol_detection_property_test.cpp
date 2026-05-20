/**
 * **Feature: meld-lsp-server, Property 13: Undefined symbol detection**
 *
 * For any undefined symbol reference, the LSP server should report an error
 * and suggest similar available names.
 *
 * **Validates: Requirements 3.3**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "meld/daemon/analysis_engine.hpp"
#include "meld/daemon/language_service.hpp"

#include <string>
#include <vector>
#include <algorithm>

namespace {

using namespace meld::lsp::analysis;
using namespace meld::lsp::services;

// ---------------------------------------------------------------------------
// Generators
// ---------------------------------------------------------------------------

/// Generate a valid Meld identifier
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

/// Generate a unique suffix to avoid collisions with keywords/builtins
rc::Gen<std::string> genUniqueSuffix() {
    return rc::gen::map(
        rc::gen::inRange(100, 999),
        [](int n) { return std::to_string(n); }
    );
}

/// Generate code that references an undeclared variable
rc::Gen<std::string> genUndeclaredVarRef() {
    return rc::gen::map(
        rc::gen::tuple(genId(), genUniqueSuffix()),
        [](const std::tuple<std::string, std::string>& t) {
            auto [base, suffix] = t;
            std::string declared = base + suffix;
            std::string undeclared = base + suffix + "xyz";
            return "let " + declared + " = 42\n"
                   "fnc main() {\n"
                   "    let result = " + undeclared + "\n"
                   "}\n";
        }
    );
}

/// Generate code that references an undeclared function
rc::Gen<std::string> genUndeclaredFuncRef() {
    return rc::gen::map(
        rc::gen::tuple(genId(), genUniqueSuffix()),
        [](const std::tuple<std::string, std::string>& t) {
            auto [base, suffix] = t;
            std::string declared_fn = base + suffix;
            std::string undeclared_fn = base + suffix + "zzz";
            return "fnc " + declared_fn + "(x: Int) {\n"
                   "    return x\n"
                   "}\n"
                   "fnc main() {\n"
                   "    let val = " + undeclared_fn + "\n"
                   "}\n";
        }
    );
}

/// Generate code with a similar-named variable (for suggestion testing)
rc::Gen<std::string> genSimilarNameRef() {
    return rc::gen::map(
        genUniqueSuffix(),
        [](const std::string& suffix) {
            std::string declared = "counter" + suffix;
            std::string typo = "conter" + suffix;  // missing 'u' - edit distance 1
            return "let " + declared + " = 0\n"
                   "fnc main() {\n"
                   "    let val = " + typo + "\n"
                   "}\n";
        }
    );
}

/// Generate code where all symbols are properly declared
rc::Gen<std::string> genAllDeclaredCode() {
    return rc::gen::map(
        rc::gen::tuple(genId(), genUniqueSuffix()),
        [](const std::tuple<std::string, std::string>& t) {
            auto [base, suffix] = t;
            std::string var_name = base + suffix;
            std::string fn_name = "calc" + suffix;
            return "let " + var_name + " = 42\n"
                   "fnc " + fn_name + "(x: Int) {\n"
                   "    let y = x\n"
                   "    return y\n"
                   "}\n";
        }
    );
}

/// Generate code with multiple undefined references
rc::Gen<std::string> genMultipleUndefinedRefs() {
    return rc::gen::map(
        genUniqueSuffix(),
        [](const std::string& suffix) {
            std::string undef1 = "unknown_a" + suffix;
            std::string undef2 = "unknown_b" + suffix;
            return "fnc main() {\n"
                   "    let x = " + undef1 + "\n"
                   "    let y = " + undef2 + "\n"
                   "}\n";
        }
    );
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

/**
 * Property 13a: Referencing an undeclared variable produces an undefined symbol error.
 */
TEST(UndefinedSymbolDetectionPropertyTest, UndeclaredVarProducesError) {
    rc::check("Referencing an undeclared variable must produce an undefined symbol error",
        []() {
            auto source = *genUndeclaredVarRef();

            AnalysisEngine engine;
            auto result = engine.detect_undefined_symbols("file:///test.meld", source);

            RC_ASSERT(result.has_errors());
            RC_ASSERT(!result.errors.empty());

            // Error message should mention "Undefined symbol"
            bool has_undef = std::any_of(
                result.errors.begin(), result.errors.end(),
                [](const UndefinedSymbolError& e) {
                    return e.message.find("Undefined symbol") != std::string::npos;
                });
            RC_ASSERT(has_undef);
        }
    );
}

/**
 * Property 13b: Referencing an undeclared function produces an undefined symbol error.
 */
TEST(UndefinedSymbolDetectionPropertyTest, UndeclaredFuncProducesError) {
    rc::check("Referencing an undeclared function must produce an undefined symbol error",
        []() {
            auto source = *genUndeclaredFuncRef();

            AnalysisEngine engine;
            auto result = engine.detect_undefined_symbols("file:///test.meld", source);

            RC_ASSERT(result.has_errors());

            bool has_undef = std::any_of(
                result.errors.begin(), result.errors.end(),
                [](const UndefinedSymbolError& e) {
                    return e.message.find("Undefined symbol") != std::string::npos;
                });
            RC_ASSERT(has_undef);
        }
    );
}

/**
 * Property 13c: Undefined symbol errors suggest similar available names.
 */
TEST(UndefinedSymbolDetectionPropertyTest, SuggestsSimilarNames) {
    rc::check("Undefined symbol errors should suggest similar available names",
        []() {
            auto source = *genSimilarNameRef();

            AnalysisEngine engine;
            auto result = engine.detect_undefined_symbols("file:///test.meld", source);

            RC_ASSERT(result.has_errors());

            // At least one error should have suggestions
            bool has_suggestions = std::any_of(
                result.errors.begin(), result.errors.end(),
                [](const UndefinedSymbolError& e) {
                    return !e.suggestions.empty();
                });
            RC_ASSERT(has_suggestions);

            // The "Did you mean" message should appear
            bool has_did_you_mean = std::any_of(
                result.errors.begin(), result.errors.end(),
                [](const UndefinedSymbolError& e) {
                    return e.message.find("Did you mean") != std::string::npos;
                });
            RC_ASSERT(has_did_you_mean);
        }
    );
}

/**
 * Property 13d: Code with all symbols properly declared produces no undefined symbol errors.
 */
TEST(UndefinedSymbolDetectionPropertyTest, AllDeclaredProducesNoErrors) {
    rc::check("Code with all symbols declared must produce no undefined symbol errors",
        []() {
            auto source = *genAllDeclaredCode();

            AnalysisEngine engine;
            auto result = engine.detect_undefined_symbols("file:///test.meld", source);

            RC_ASSERT(!result.has_errors());
            RC_ASSERT(result.errors.empty());
        }
    );
}

/**
 * Property 13e: Multiple undefined references produce multiple errors.
 */
TEST(UndefinedSymbolDetectionPropertyTest, MultipleUndefinedProduceMultipleErrors) {
    rc::check("Multiple undefined references must produce multiple errors",
        []() {
            auto source = *genMultipleUndefinedRefs();

            AnalysisEngine engine;
            auto result = engine.detect_undefined_symbols("file:///test.meld", source);

            RC_ASSERT(result.errors.size() >= 2);
        }
    );
}

/**
 * Property 13f: Undefined symbol errors are surfaced through get_all_diagnostics.
 */
TEST(UndefinedSymbolDetectionPropertyTest, UndefinedSymbolsAppearInAllDiagnostics) {
    rc::check("Undefined symbol errors must appear in get_all_diagnostics output",
        []() {
            auto source = *genUndeclaredVarRef();

            LanguageService service;
            auto diags = service.get_all_diagnostics("file:///test.meld", source);

            bool has_symbol_diag = std::any_of(
                diags.begin(), diags.end(),
                [](const Diagnostic& d) {
                    return d.source == "meld-symbol-resolver";
                });
            RC_ASSERT(has_symbol_diag);
        }
    );
}

/**
 * Property 13g: Undefined symbol error positions are valid.
 */
TEST(UndefinedSymbolDetectionPropertyTest, ErrorPositionsAreValid) {
    rc::check("Undefined symbol error positions must be non-negative",
        []() {
            auto source = *rc::gen::oneOf(
                genUndeclaredVarRef(),
                genUndeclaredFuncRef(),
                genMultipleUndefinedRefs()
            );

            AnalysisEngine engine;
            auto result = engine.detect_undefined_symbols("file:///test.meld", source);

            for (const auto& err : result.errors) {
                RC_ASSERT(err.line >= 0);
                RC_ASSERT(err.character >= 0);
                RC_ASSERT(err.end_line >= err.line);
                RC_ASSERT(err.end_character > err.character);
                RC_ASSERT(!err.symbol_name.empty());
            }
        }
    );
}

/**
 * Property 13h: Levenshtein distance is symmetric.
 */
TEST(UndefinedSymbolDetectionPropertyTest, LevenshteinDistanceIsSymmetric) {
    rc::check("Levenshtein distance must be symmetric",
        []() {
            auto a = *rc::gen::map(
                rc::gen::inRange(1, 8),
                [](int len) {
                    std::string s;
                    for (int i = 0; i < len; ++i)
                        s += static_cast<char>('a' + ((i * 13 + 5) % 26));
                    return s;
                });
            auto b = *rc::gen::map(
                rc::gen::inRange(1, 8),
                [](int len) {
                    std::string s;
                    for (int i = 0; i < len; ++i)
                        s += static_cast<char>('a' + ((i * 11 + 7) % 26));
                    return s;
                });

            int d1 = AnalysisEngine::levenshtein_distance(a, b);
            int d2 = AnalysisEngine::levenshtein_distance(b, a);
            RC_ASSERT(d1 == d2);
            RC_ASSERT(d1 >= 0);
        }
    );
}
