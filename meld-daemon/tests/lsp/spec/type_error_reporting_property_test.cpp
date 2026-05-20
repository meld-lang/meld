/**
 * **Feature: meld-lsp-server, Property 12: Type error reporting**
 *
 * For any code with type mismatches, the LSP server should report clear
 * type errors with descriptions and suggested fixes.
 *
 * **Validates: Requirements 3.2**
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

/// Generate code with a String assigned to an Int-typed variable
rc::Gen<std::string> genStringToIntMismatch() {
    return rc::gen::map(
        genId(),
        [](const std::string& name) {
            return "let " + name + " : Int = \"hello\"";
        }
    );
}

/// Generate code with an Int assigned to a String-typed variable
rc::Gen<std::string> genIntToStringMismatch() {
    return rc::gen::map(
        rc::gen::tuple(genId(), rc::gen::inRange(0, 9999)),
        [](const std::tuple<std::string, int>& t) {
            auto [name, val] = t;
            return "let " + name + " : String = " + std::to_string(val);
        }
    );
}

/// Generate code with a Bool assigned to an Int-typed variable
rc::Gen<std::string> genBoolToIntMismatch() {
    return rc::gen::map(
        rc::gen::tuple(genId(), rc::gen::element(std::string("true"), std::string("false"))),
        [](const std::tuple<std::string, std::string>& t) {
            auto [name, val] = t;
            return "let " + name + " : Int = " + val;
        }
    );
}

/// Generate code with a Float assigned to a String-typed variable
rc::Gen<std::string> genFloatToStringMismatch() {
    return rc::gen::map(
        genId(),
        [](const std::string& name) {
            return "let " + name + " : String = 3.14";
        }
    );
}

/// Generate code with correct type annotations (no mismatch)
rc::Gen<std::string> genCorrectlyTypedCode() {
    return rc::gen::map(
        rc::gen::tuple(genId(), genId(), genId(), genId()),
        [](const std::tuple<std::string, std::string, std::string, std::string>& t) {
            auto [n1, n2, n3, n4] = t;
            return "let " + n1 + " : Int = 42\n"
                   "let " + n2 + " : String = \"hello\"\n"
                   "let " + n3 + " : Bool = true\n"
                   "let " + n4 + " : Float = 3.14\n";
        }
    );
}

/// Generate code with multiple type mismatches
rc::Gen<std::string> genMultipleTypeMismatches() {
    return rc::gen::map(
        rc::gen::tuple(genId(), genId()),
        [](const std::tuple<std::string, std::string>& t) {
            auto [n1, n2] = t;
            return "let " + n1 + " : Int = \"wrong\"\n"
                   "let " + n2 + " : String = 42\n";
        }
    );
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

/**
 * Property 12a: String-to-Int type mismatch produces a type error diagnostic.
 */
TEST(TypeErrorReportingPropertyTest, StringToIntMismatchProducesError) {
    rc::check("Assigning String to Int-typed variable must produce type error",
        []() {
            auto source = *genStringToIntMismatch();

            AnalysisEngine engine;
            auto result = engine.check_types("file:///test.meld", source);

            RC_ASSERT(result.has_errors());
            RC_ASSERT(!result.errors.empty());

            // Error should mention expected and actual types
            const auto& err = result.errors[0];
            RC_ASSERT(err.expected_type == "Int");
            RC_ASSERT(err.actual_type == "String");
            RC_ASSERT(err.message.find("Int") != std::string::npos);
            RC_ASSERT(err.message.find("String") != std::string::npos);
        }
    );
}

/**
 * Property 12b: Int-to-String type mismatch produces a type error diagnostic.
 */
TEST(TypeErrorReportingPropertyTest, IntToStringMismatchProducesError) {
    rc::check("Assigning Int to String-typed variable must produce type error",
        []() {
            auto source = *genIntToStringMismatch();

            AnalysisEngine engine;
            auto result = engine.check_types("file:///test.meld", source);

            RC_ASSERT(result.has_errors());

            const auto& err = result.errors[0];
            RC_ASSERT(err.expected_type == "String");
            RC_ASSERT(err.actual_type == "Int");
        }
    );
}

/**
 * Property 12c: Bool-to-Int type mismatch produces a type error diagnostic.
 */
TEST(TypeErrorReportingPropertyTest, BoolToIntMismatchProducesError) {
    rc::check("Assigning Bool to Int-typed variable must produce type error",
        []() {
            auto source = *genBoolToIntMismatch();

            AnalysisEngine engine;
            auto result = engine.check_types("file:///test.meld", source);

            RC_ASSERT(result.has_errors());

            const auto& err = result.errors[0];
            RC_ASSERT(err.expected_type == "Int");
            RC_ASSERT(err.actual_type == "Bool");
        }
    );
}

/**
 * Property 12d: Type errors include suggested fixes.
 */
TEST(TypeErrorReportingPropertyTest, TypeErrorsIncludeSuggestions) {
    rc::check("Type errors must include a non-empty suggestion for fixing",
        []() {
            auto source = *rc::gen::oneOf(
                genStringToIntMismatch(),
                genIntToStringMismatch(),
                genBoolToIntMismatch(),
                genFloatToStringMismatch()
            );

            AnalysisEngine engine;
            auto result = engine.check_types("file:///test.meld", source);

            RC_ASSERT(result.has_errors());
            for (const auto& err : result.errors) {
                RC_ASSERT(!err.suggestion.empty());
            }
        }
    );
}

/**
 * Property 12e: Correctly typed code produces no type errors.
 */
TEST(TypeErrorReportingPropertyTest, CorrectTypesProduceNoErrors) {
    rc::check("Code with correct type annotations must produce no type errors",
        []() {
            auto source = *genCorrectlyTypedCode();

            AnalysisEngine engine;
            auto result = engine.check_types("file:///test.meld", source);

            RC_ASSERT(!result.has_errors());
            RC_ASSERT(result.errors.empty());
        }
    );
}

/**
 * Property 12f: Multiple type mismatches produce multiple errors.
 */
TEST(TypeErrorReportingPropertyTest, MultipleTypeMismatchesProduceMultipleErrors) {
    rc::check("Code with multiple type mismatches must produce multiple errors",
        []() {
            auto source = *genMultipleTypeMismatches();

            AnalysisEngine engine;
            auto result = engine.check_types("file:///test.meld", source);

            RC_ASSERT(result.errors.size() >= 2);
        }
    );
}

/**
 * Property 12g: Type errors are surfaced through get_all_diagnostics.
 */
TEST(TypeErrorReportingPropertyTest, TypeErrorsAppearInAllDiagnostics) {
    rc::check("Type errors must appear in get_all_diagnostics output",
        []() {
            auto source = *genStringToIntMismatch();

            LanguageService service;
            auto diags = service.get_all_diagnostics("file:///test.meld", source);

            // Should have at least one diagnostic from the type checker
            bool has_type_diag = std::any_of(
                diags.begin(), diags.end(),
                [](const Diagnostic& d) {
                    return d.source == "meld-type-checker";
                });
            RC_ASSERT(has_type_diag);

            // The type diagnostic should mention the type mismatch
            bool mentions_types = std::any_of(
                diags.begin(), diags.end(),
                [](const Diagnostic& d) {
                    return d.source == "meld-type-checker" &&
                           d.message.find("Int") != std::string::npos &&
                           d.message.find("String") != std::string::npos;
                });
            RC_ASSERT(mentions_types);
        }
    );
}

/**
 * Property 12h: Type error line/column positions are non-negative.
 */
TEST(TypeErrorReportingPropertyTest, TypeErrorPositionsAreValid) {
    rc::check("Type error positions must be non-negative",
        []() {
            auto source = *rc::gen::oneOf(
                genStringToIntMismatch(),
                genIntToStringMismatch(),
                genBoolToIntMismatch(),
                genFloatToStringMismatch(),
                genMultipleTypeMismatches()
            );

            AnalysisEngine engine;
            auto result = engine.check_types("file:///test.meld", source);

            for (const auto& err : result.errors) {
                RC_ASSERT(err.line >= 0);
                RC_ASSERT(err.character >= 0);
                RC_ASSERT(err.end_line >= err.line);
                RC_ASSERT(err.end_character >= 0);
            }
        }
    );
}
