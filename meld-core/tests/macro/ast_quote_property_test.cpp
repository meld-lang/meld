/**
 * Property-based test for ast.quote round trip correctness.
 *
 * Feature: meld-lang, Property 68: ast.quote Round Trip
 *
 * For any valid AST fragment, generating it via ast.quote with interpolated
 * values should produce an AST structurally equivalent to manual construction.
 *
 * Specifically:
 *   (a) String interpolation in identifiers produces the same name as manual
 *       identifier construction
 *   (b) Type interpolation in return types / parameters produces the same
 *       type_annotation as manual construction
 *   (c) Function templates with interpolated name, type, and parameters
 *       produce function_definitions equivalent to manual construction
 *
 * Uses rapidcheck for property-based testing.
 *
 * **Validates: Requirements 2.9**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "meld/macro/ast_quote.hpp"
#include "meld/parser/ast.hpp"
#include "meld/parser/ast_node.hpp"
#include <boost/variant.hpp>
#include <string>
#include <vector>

using namespace meld::macro;
using namespace meld::parser::ast;

namespace {

// ---------------------------------------------------------------------------
// Generators
// ---------------------------------------------------------------------------

/// Generate a valid Meld identifier name (lowercase alpha + digits, no leading
/// digit, no hyphens at start/end). Kept simple to avoid parser edge cases.
rc::Gen<std::string> genIdentifierName() {
    return rc::gen::map(
        rc::gen::inRange(0, 80),
        [](int n) { return "id_" + std::to_string(n); }
    );
}

/// Generate a type name from the set of common Meld types.
rc::Gen<std::string> genTypeName() {
    static const std::vector<std::string> types = {
        "int", "string", "float", "bool", "User", "Config", "Point"
    };
    return rc::gen::map(
        rc::gen::inRange(0, static_cast<int>(types.size())),
        [&](int i) { return types[static_cast<size_t>(i)]; }
    );
}

/// Build a type_annotation from a type name string.
type_annotation make_type(const std::string& name) {
    type_annotation ta;
    ta.type_name.name = name;
    ta.is_nullable = false;
    return ta;
}

} // anonymous namespace

// ===========================================================================
// Property 68: ast.quote Round Trip
//
// For any valid AST fragment, generating it via ast.quote with interpolated
// values should produce an AST structurally equivalent to manual construction.
//
// **Validates: Requirements 2.9**
// ===========================================================================

TEST(AstQuotePropertyTest, StringInterpolationRoundTrip) {
    rc::check(
        "String interpolation produces identifier matching manual construction",
        []() {
            auto name = *genIdentifierName();

            // --- ast.quote path ---
            AstQuote quote;
            quote.bind("var_name", std::string(name));
            auto result = quote.parse("val ${var_name} = 42");
            RC_ASSERT(result.has_value());
            RC_ASSERT(result->expressions.size() == 1u);

            auto* val_decl = boost::get<
                boost::spirit::x3::forward_ast<val_declaration>>(
                &result->expressions[0]);
            RC_ASSERT(val_decl != nullptr);

            // --- Manual construction path ---
            // The parser would produce a val_declaration with name == name
            // We verify the ast.quote output matches.
            RC_ASSERT(val_decl->get().name.name == name);
        }
    );
}

TEST(AstQuotePropertyTest, TypeInterpolationRoundTrip) {
    rc::check(
        "Type interpolation in return type matches manual type_annotation",
        []() {
            auto field_name = *genIdentifierName();
            auto type_name = *genTypeName();

            // --- Manual construction ---
            type_annotation expected_type = make_type(type_name);

            // --- ast.quote path ---
            AstQuote quote;
            quote.bind("fname", std::string(field_name));
            quote.bind("ftype", type_annotation(expected_type));

            auto result = quote.parse_function(
                "fnc ${fname}() -> ${ftype} {\n"
                "    rtn 0\n"
                "}");
            RC_ASSERT(result.has_value());

            // Round-trip check: name and return type match manual construction
            RC_ASSERT(result->name.name == field_name);
            RC_ASSERT(result->has_return_type);
            RC_ASSERT(result->return_type.type_name.name == expected_type.type_name.name);
            RC_ASSERT(result->return_type.is_nullable == expected_type.is_nullable);
        }
    );
}

TEST(AstQuotePropertyTest, FunctionTemplateRoundTrip) {
    rc::check(
        "Function template with name + type + param produces equivalent function_definition",
        []() {
            auto func_name = *genIdentifierName();
            auto return_type_name = *genTypeName();
            auto param_name = *genIdentifierName();
            auto param_type_name = *genTypeName();

            // --- Manual construction ---
            function_definition manual;
            manual.name.name = "set_" + func_name;
            manual.has_return_type = false;
            function_parameter manual_param;
            manual_param.name.name = param_name;
            manual_param.type = make_type(param_type_name);
            manual.parameters.push_back(std::move(manual_param));

            // --- ast.quote path ---
            AstQuote quote;
            quote.bind("fname", std::string(func_name));
            quote.bind("pname", std::string(param_name));
            quote.bind("ptype", make_type(param_type_name));

            auto result = quote.parse_function(
                "fnc set_${fname}(val ${pname}: ${ptype}) {\n"
                "}");
            RC_ASSERT(result.has_value());

            auto& func = *result;

            // Round-trip: function name matches
            RC_ASSERT(func.name.name == manual.name.name);

            // Round-trip: parameter count matches
            RC_ASSERT(func.parameters.size() == manual.parameters.size());
            RC_ASSERT(func.parameters.size() == 1u);

            // Round-trip: parameter name and type match manual construction
            RC_ASSERT(func.parameters[0].name.name == manual.parameters[0].name.name);
            RC_ASSERT(func.parameters[0].type.type_name.name ==
                       manual.parameters[0].type.type_name.name);
        }
    );
}

TEST(AstQuotePropertyTest, StructTemplateRoundTrip) {
    rc::check(
        "Struct template with interpolated name produces equivalent struct_definition",
        []() {
            auto struct_name = *genIdentifierName();

            // --- ast.quote path ---
            AstQuote quote;
            quote.bind("sname", std::string(struct_name));

            auto result = quote.parse(
                "struct ${sname} {\n"
                "    val x: int\n"
                "}");
            RC_ASSERT(result.has_value());
            RC_ASSERT(result->expressions.size() == 1u);

            auto* sd = boost::get<
                boost::spirit::x3::forward_ast<struct_definition>>(
                &result->expressions[0]);
            RC_ASSERT(sd != nullptr);

            // Round-trip: struct name matches the interpolated value
            RC_ASSERT(sd->get().name.name == struct_name);
            // Round-trip: fields are preserved from the template
            RC_ASSERT(sd->get().fields.size() == 1u);
            RC_ASSERT(sd->get().fields[0].name.name == "x");
            RC_ASSERT(sd->get().fields[0].type.type_name.name == "int");
        }
    );
}

TEST(AstQuotePropertyTest, GetterSetterRoundTrip) {
    rc::check(
        "Getter+setter generation via ast.quote matches manual construction for any field",
        []() {
            auto field_name = *genIdentifierName();
            auto type_name = *genTypeName();
            auto is_mutable = *rc::gen::arbitrary<bool>();

            type_annotation field_type = make_type(type_name);

            // --- Generate getter via ast.quote ---
            AstQuote getter_quote;
            getter_quote.bind("field_name", std::string(field_name));
            getter_quote.bind("field_type", type_annotation(field_type));

            auto getter_result = getter_quote.parse_function(
                "fnc ${field_name}() -> ${field_type} {\n"
                "    rtn 0\n"
                "}");
            RC_ASSERT(getter_result.has_value());

            // Getter round-trip checks
            RC_ASSERT(getter_result->name.name == field_name);
            RC_ASSERT(getter_result->has_return_type);
            RC_ASSERT(getter_result->return_type.type_name.name == type_name);
            RC_ASSERT(getter_result->parameters.empty());

            // --- Generate setter via ast.quote (only if mutable) ---
            if (is_mutable) {
                AstQuote setter_quote;
                setter_quote.bind("field_name", std::string(field_name));
                setter_quote.bind("field_type", make_type(type_name));

                auto setter_result = setter_quote.parse_function(
                    "fnc set_${field_name}(val v: ${field_type}) {\n"
                    "}");
                RC_ASSERT(setter_result.has_value());

                std::string expected_setter_name = "set_" + field_name;
                RC_ASSERT(setter_result->name.name == expected_setter_name);
                RC_ASSERT(setter_result->parameters.size() == 1u);
                RC_ASSERT(setter_result->parameters[0].name.name == "v");
                RC_ASSERT(setter_result->parameters[0].type.type_name.name == type_name);
            }
        }
    );
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
