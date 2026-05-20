/**
 * Property-based tests for compiler parser acceptance of explicit val.
 *
 * Property 11: Compiler parser accepts val in new positions.
 *
 * Uses rapidcheck for property-based testing.
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "meld/compiler/cap.hpp"
#include <string>

using namespace meld::compiler::cap;

namespace {

rc::Gen<std::string> genVarName() {
    return rc::gen::map(
        rc::gen::inRange(0, 100),
        [](int n) { return "x_" + std::to_string(n); }
    );
}

} // anonymous namespace

// ===========================================================================
// Property 11: Compiler parser accepts val in new positions
//
// For any valid declaration using `val` as a mutability annotation, the
// compiler parser SHALL accept it without error.
//
// **Validates: Requirements 5.3**
// ===========================================================================

/**
 * Feature: explicit-val-annotation, Property 11
 *
 * Top-level `val name = expr` must compile without error.
 *
 * **Validates: Requirements 5.3**
 */
TEST(ExplicitValCompilerParserPropertyTest, TopLevelValAccepted) {
    rc::check("Top-level val declarations must be accepted by compiler parser",
        []() {
            auto var_name = *genVarName();
            auto value = *rc::gen::inRange(0, 1000);

            std::string source = "val " + var_name + " = " + std::to_string(value);

            CompilerAgentProtocol cap;
            auto result = cap.compile_source(source, "test.meld");

            RC_ASSERT(result.success);
        }
    );
}

/**
 * Feature: explicit-val-annotation, Property 11
 *
 * Function parameters with `val` must compile without error.
 *
 * **Validates: Requirements 5.3**
 */
TEST(ExplicitValCompilerParserPropertyTest, FunctionParameterValAccepted) {
    rc::check("Function parameters with val must be accepted by compiler parser",
        []() {
            auto param_name = *genVarName();
            auto value = *rc::gen::inRange(0, 1000);

            std::string source =
                "fnc add(val " + param_name + ") {\n"
                "    " + param_name + "\n"
                "}";

            CompilerAgentProtocol cap;
            auto result = cap.compile_source(source, "test.meld");

            RC_ASSERT(result.success);
        }
    );
}

/**
 * Feature: explicit-val-annotation, Property 11
 *
 * val and var produce equivalent compilation results (both succeed).
 *
 * **Validates: Requirements 5.3**
 */
TEST(ExplicitValCompilerParserPropertyTest, ValAndVarBothAccepted) {
    rc::check("Both val and var top-level declarations must be accepted",
        []() {
            auto var_name = *genVarName();
            auto value = *rc::gen::inRange(0, 1000);

            std::string val_source = "val " + var_name + " = " + std::to_string(value);
            std::string var_source = "var " + var_name + " = " + std::to_string(value);

            CompilerAgentProtocol cap;
            auto val_result = cap.compile_source(val_source, "test_val.meld");
            auto var_result = cap.compile_source(var_source, "test_var.meld");

            RC_ASSERT(val_result.success);
            RC_ASSERT(var_result.success);
        }
    );
}

