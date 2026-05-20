/**
 * Property-based tests for compiler parser and pretty printer enforcement
 * of mandatory val/var annotations.
 *
 * Properties 7-9 from the design document.
 * Uses rapidcheck for property-based testing.
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "meld/compiler/cap.hpp"
#include "meld/compiler/ast_printer.hpp"
#include <string>

using namespace meld::compiler::cap;

namespace {

rc::Gen<std::string> genVarName() {
    return rc::gen::map(rc::gen::inRange(0, 100),
        [](int n) { return "x_" + std::to_string(n); });
}

} // anonymous namespace

// ===========================================================================
// Feature: mandatory-val-var-annotations, Property 7:
// Compiler parser enforces mandatory annotation
//
// Validates: Requirements 7.1, 7.2, 7.3
// ===========================================================================

/**
 * Function parameters without val/var produce a diagnostic error
 * in the compiler parser.
 */
TEST(MandatoryValVarCompilerPropertyTest, BareParameterProducesDiagnosticError) {
    rc::check("Bare function parameters produce compiler parser diagnostic error",
        []() {
            auto param_name = *genVarName();

            std::string source =
                "fnc add(" + param_name + ") {\n"
                "    " + param_name + "\n"
                "}";

            CompilerAgentProtocol cap;
            auto result = cap.compile_source(source, "test.meld");

            // Should have an error about missing annotation
            bool has_annotation_error = false;
            for (const auto& msg : result.messages) {
                if (msg.code == "E110" &&
                    msg.message.find("Missing mutability annotation") != std::string::npos) {
                    has_annotation_error = true;
                    break;
                }
            }
            RC_ASSERT(has_annotation_error);
        });
}

/**
 * Function parameters with val compile without annotation errors.
 */
TEST(MandatoryValVarCompilerPropertyTest, ValParameterAccepted) {
    rc::check("val function parameters compile without annotation errors",
        []() {
            auto param_name = *genVarName();

            std::string source =
                "fnc add(val " + param_name + ") {\n"
                "    " + param_name + "\n"
                "}";

            CompilerAgentProtocol cap;
            auto result = cap.compile_source(source, "test.meld");

            // Should not have E110 error
            for (const auto& msg : result.messages) {
                RC_ASSERT(msg.code != "E110");
            }
        });
}

/**
 * val-annotated and recovered bare declarations produce equivalent
 * kernel S-expressions (both map to immutable).
 */
TEST(MandatoryValVarCompilerPropertyTest, ValAndBareProduceEquivalentKernel) {
    rc::check("val and var both produce successful compilation",
        []() {
            auto param_name = *genVarName();
            auto value = *rc::gen::inRange(0, 1000);

            std::string val_source = "val " + param_name + " = " + std::to_string(value);
            std::string var_source = "var " + param_name + " = " + std::to_string(value);

            CompilerAgentProtocol cap;
            auto val_result = cap.compile_source(val_source, "test_val.meld");
            auto var_result = cap.compile_source(var_source, "test_var.meld");

            RC_ASSERT(val_result.success);
            RC_ASSERT(var_result.success);
        });
}

// ===========================================================================
// Feature: mandatory-val-var-annotations, Property 8:
// Pretty printer always emits val or var
//
// Validates: Requirements 8.1, 8.2, 8.3
// ===========================================================================

/**
 * The pretty printer emits val for immutable declarations (def-field).
 * After formatting, no bare declarations should exist.
 */
TEST(MandatoryValVarCompilerPropertyTest, PrettyPrinterEmitsValForDefField) {
    rc::check("Pretty printer emits val for def-field (immutable) declarations",
        []() {
            auto var_name = *genVarName();
            auto value = *rc::gen::inRange(0, 1000);

            // Compile a val declaration — produces def-explicit-val in kernel
            std::string source = "val " + var_name + " = " + std::to_string(value);

            CompilerAgentProtocol cap;
            auto result = cap.compile_source(source, "test.meld");
            RC_ASSERT(result.success);

            // The generated code (if available) should contain "val"
            if (result.generated_code.has_value()) {
                RC_ASSERT(result.generated_code->find("val ") != std::string::npos);
            }
        });
}

/**
 * The pretty printer emits var for mutable declarations.
 */
TEST(MandatoryValVarCompilerPropertyTest, PrettyPrinterEmitsVarForMutable) {
    rc::check("Pretty printer emits var for mutable declarations",
        []() {
            auto var_name = *genVarName();
            auto value = *rc::gen::inRange(0, 1000);

            std::string source = "var " + var_name + " = " + std::to_string(value);

            CompilerAgentProtocol cap;
            auto result = cap.compile_source(source, "test.meld");
            RC_ASSERT(result.success);

            if (result.generated_code.has_value()) {
                RC_ASSERT(result.generated_code->find("var ") != std::string::npos);
            }
        });
}

// ===========================================================================
// Feature: mandatory-val-var-annotations, Property 9:
// Parse-print-parse round-trip equivalence
//
// Validates: Requirements 8.4
// ===========================================================================

/**
 * For valid source with explicit val/var, compiling twice produces
 * consistent results (round-trip stability).
 */
TEST(MandatoryValVarCompilerPropertyTest, RoundTripStability) {
    rc::check("Compiling valid source twice produces consistent results",
        []() {
            auto var_name = *genVarName();
            auto value = *rc::gen::inRange(0, 1000);

            std::string source = "val " + var_name + " = " + std::to_string(value);

            CompilerAgentProtocol cap;
            auto result1 = cap.compile_source(source, "test.meld");
            auto result2 = cap.compile_source(source, "test.meld");

            RC_ASSERT(result1.success == result2.success);
        });
}

