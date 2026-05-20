/**
 * Property-based tests for explicit val annotation type checker enforcement.
 *
 * Properties 4 and 5: Semantic equivalence and immutable property setter errors.
 *
 * Uses rapidcheck for property-based testing.
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "meld/compiler/type_checker.hpp"
#include "meld/parser/parser.hpp"
#include "meld/parser/ast.hpp"
#include <boost/variant.hpp>
#include <string>
#include <vector>
#include <sstream>

using namespace meld::compiler;
using namespace meld::parser;
using namespace meld::parser::ast;

namespace {

// ---------------------------------------------------------------------------
// Generators
// ---------------------------------------------------------------------------

rc::Gen<std::string> genFieldName() {
    return rc::gen::map(
        rc::gen::inRange(0, 100),
        [](int n) { return "f_" + std::to_string(n); }
    );
}

rc::Gen<std::string> genTypeName() {
    return rc::gen::element(
        std::string("Int"), std::string("Float"),
        std::string("String"), std::string("Bool"));
}

rc::Gen<std::string> genClassName() {
    return rc::gen::map(
        rc::gen::inRange(0, 100),
        [](int n) { return "TestClass" + std::to_string(n); }
    );
}

rc::Gen<std::string> genStructName() {
    return rc::gen::map(
        rc::gen::inRange(0, 100),
        [](int n) { return "TestStruct" + std::to_string(n); }
    );
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

bool parse_source(const std::string& source, std::vector<expression>& results) {
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    if (!lexer.errors().empty()) return false;
    TokenParser parser(tokens);
    return parser.parse_file(results);
}

/// Type-check a parsed expression and return error message if any.
std::string type_check_source(const std::string& source) {
    std::vector<expression> results;
    if (!parse_source(source, results)) return "PARSE_ERROR";

    auto checker = std::make_unique<TypeChecker>();
    auto env = std::make_shared<TypeEnvironment>();

    for (const auto& expr : results) {
        auto result = checker->check_expression(expr, env);
        if (!result.has_value()) {
            return result.error().message;
        }
    }
    return "";  // no error
}

} // anonymous namespace

// ===========================================================================
// Property 5: Immutable property with setter produces type error
//
// For any class/struct with an immutable property (is_mutable == false) that
// has a custom setter, the type checker SHALL produce an error containing
// "Immutable property '{name}' cannot have a custom setter".
//
// **Validates: Requirements 2.5**
// ===========================================================================

/**
 * Feature: explicit-val-annotation, Property 5
 *
 * Class with explicit val property + C#-style getter/setter must be rejected
 * at the parser level (before type checking).
 *
 * **Validates: Requirements 17.6, 17.7**
 */
TEST(ExplicitValTypeCheckerPropertyTest, ExplicitValPropertyWithSetterRejectedByParser) {
    rc::check("Explicit val property with C#-style get/set must be rejected by parser",
        []() {
            auto class_name = *genClassName();
            auto prop_name = *genFieldName();
            auto type_name = *genTypeName();

            std::string source =
                "class " + class_name + " {\n"
                "    val " + prop_name + ": " + type_name + " {\n"
                "        get { 42 }\n"
                "        set(value) { }\n"
                "    }\n"
                "}";

            std::string error = type_check_source(source);
            RC_ASSERT(error == "PARSE_ERROR");
        }
    );
}

/**
 * Feature: explicit-val-annotation, Property 5
 *
 * Class with implicit val property (no keyword) + C#-style getter/setter must
 * also be rejected at the parser level.
 *
 * **Validates: Requirements 17.6, 17.7**
 */
TEST(ExplicitValTypeCheckerPropertyTest, ImplicitValPropertyWithSetterRejectedByParser) {
    rc::check("Implicit val property with C#-style get/set must be rejected by parser",
        []() {
            auto class_name = *genClassName();
            auto prop_name = *genFieldName();
            auto type_name = *genTypeName();

            std::string source =
                "class " + class_name + " {\n"
                "    " + prop_name + ": " + type_name + " {\n"
                "        get { 42 }\n"
                "        set(value) { }\n"
                "    }\n"
                "}";

            std::string error = type_check_source(source);
            RC_ASSERT(error == "PARSE_ERROR");
        }
    );
}

/**
 * Feature: explicit-val-annotation, Property 5
 *
 * Mutable property (var) with C#-style getter/setter must also be rejected
 * at the parser level.
 *
 * **Validates: Requirements 17.6, 17.7**
 */
TEST(ExplicitValTypeCheckerPropertyTest, VarPropertyWithSetterRejectedByParser) {
    rc::check("var property with C#-style get/set must be rejected by parser",
        []() {
            auto class_name = *genClassName();
            auto prop_name = *genFieldName();
            auto type_name = *genTypeName();

            std::string source =
                "class " + class_name + " {\n"
                "    var " + prop_name + ": " + type_name + " {\n"
                "        get { 42 }\n"
                "        set(value) { }\n"
                "    }\n"
                "}";

            std::string error = type_check_source(source);
            RC_ASSERT(error == "PARSE_ERROR");
        }
    );
}

// ===========================================================================
// Property 4: Semantic equivalence of explicit val and implicit default
//
// For any declaration, explicit `val` and no keyword (implicit default) SHALL
// produce identical type-checking results.
//
// **Validates: Requirements 1.5, 5.1, 5.2**
// ===========================================================================

/**
 * Feature: explicit-val-annotation, Property 4
 *
 * Struct with explicit val fields and struct with no-keyword fields must
 * produce identical type-checking results.
 *
 * **Validates: Requirements 1.5, 5.1, 5.2**
 */
TEST(ExplicitValTypeCheckerPropertyTest, ExplicitValAndImplicitDefaultAreEquivalent) {
    rc::check("Explicit val and implicit default produce identical type-check results",
        []() {
            auto struct_name = *genStructName();
            auto field_name = *genFieldName();
            auto type_name = *genTypeName();

            // With explicit val
            std::string source_val =
                "struct " + struct_name + " {\n"
                "    val " + field_name + ": " + type_name + "\n"
                "}";

            // Without keyword (implicit default)
            std::string source_default =
                "struct " + struct_name + " {\n"
                "    " + field_name + ": " + type_name + "\n"
                "}";

            std::string error_val = type_check_source(source_val);
            std::string error_default = type_check_source(source_default);

            // Both must produce the same result (both succeed or both fail with same error)
            RC_ASSERT(error_val == error_default);
        }
    );
}

/**
 * Feature: explicit-val-annotation, Property 4
 *
 * Class with explicit val property (C#-style getter only) and class with no-keyword
 * property (C#-style getter only) must both be rejected by the parser.
 *
 * **Validates: Requirements 17.6, 17.7**
 */
TEST(ExplicitValTypeCheckerPropertyTest, ExplicitValAndImplicitDefaultPropertyBothRejected) {
    rc::check("Explicit val and implicit default on C#-style properties both rejected by parser",
        []() {
            auto class_name = *genClassName();
            auto prop_name = *genFieldName();
            auto type_name = *genTypeName();

            // With explicit val
            std::string source_val =
                "class " + class_name + " {\n"
                "    val " + prop_name + ": " + type_name + " {\n"
                "        get { 42 }\n"
                "    }\n"
                "}";

            // Without keyword
            std::string source_default =
                "class " + class_name + " {\n"
                "    " + prop_name + ": " + type_name + " {\n"
                "        get { 42 }\n"
                "    }\n"
                "}";

            std::string error_val = type_check_source(source_val);
            std::string error_default = type_check_source(source_default);

            // Both must be rejected at the parser level
            RC_ASSERT(error_val == "PARSE_ERROR");
            RC_ASSERT(error_default == "PARSE_ERROR");
        }
    );
}

