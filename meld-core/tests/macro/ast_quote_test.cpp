/**
 * Tests for ast.quote quasiquoting facility.
 *
 * Validates that AstQuote can parse Meld code templates with ${} interpolation
 * and produce correct AST nodes with substituted values.
 *
 * Requirements: 2.9
 */

#include <gtest/gtest.h>
#include "meld/macro/ast_quote.hpp"
#include "meld/parser/ast.hpp"
#include "meld/parser/ast_node.hpp"
#include <boost/variant.hpp>
#include <string>

using namespace meld::macro;
using namespace meld::parser::ast;

// =============================================================================
// Basic string interpolation
// =============================================================================

TEST(AstQuoteTest, SimpleStringInterpolation) {
    AstQuote quote;
    quote.bind("name", std::string("age"));

    auto result = quote.parse("val age = 42");
    ASSERT_TRUE(result.has_value()) << result.error();
    ASSERT_EQ(result->expressions.size(), 1u);
}

TEST(AstQuoteTest, StringInterpolationInIdentifier) {
    AstQuote quote;
    quote.bind("field_name", std::string("age"));

    // The template uses ${field_name} which gets replaced with "age"
    // before parsing, so the parser sees: val age = 42
    auto result = quote.parse("val ${field_name} = 42");
    ASSERT_TRUE(result.has_value()) << result.error();
    ASSERT_EQ(result->expressions.size(), 1u);

    auto* val = boost::get<boost::spirit::x3::forward_ast<val_declaration>>(
        &result->expressions[0]);
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(val->get().name.name, "age");
}

// =============================================================================
// Unbound variable error
// =============================================================================

TEST(AstQuoteTest, UnboundVariableProducesError) {
    AstQuote quote;
    // Don't bind anything — ${missing} should fail
    auto result = quote.parse("val ${missing} = 42");
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("unbound"), std::string::npos);
}

// =============================================================================
// Unclosed interpolation error
// =============================================================================

TEST(AstQuoteTest, UnclosedInterpolationProducesError) {
    AstQuote quote;
    quote.bind("x", std::string("foo"));
    auto result = quote.parse("val ${x = 42");
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("unclosed"), std::string::npos);
}

// =============================================================================
// Multiple interpolations in one template
// =============================================================================

TEST(AstQuoteTest, MultipleStringInterpolations) {
    AstQuote quote;
    quote.bind("getter_name", std::string("name"));
    quote.bind("backing_field", std::string("_name"));

    // Template: val _name = name
    // After expansion: val _name = name
    auto result = quote.parse("val ${backing_field} = ${getter_name}");
    ASSERT_TRUE(result.has_value()) << result.error();
    ASSERT_EQ(result->expressions.size(), 1u);

    auto* val = boost::get<boost::spirit::x3::forward_ast<val_declaration>>(
        &result->expressions[0]);
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(val->get().name.name, "_name");
}

// =============================================================================
// parse_expression convenience method
// =============================================================================

TEST(AstQuoteTest, ParseExpressionReturnsSingle) {
    AstQuote quote;
    auto result = quote.parse_expression("val x = 10");
    ASSERT_TRUE(result.has_value()) << result.error();

    auto* val = boost::get<boost::spirit::x3::forward_ast<val_declaration>>(
        &*result);
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(val->get().name.name, "x");
}

// =============================================================================
// parse_function convenience method — getter generation
// =============================================================================

TEST(AstQuoteTest, ParseFunctionGeneratesGetter) {
    AstQuote quote;
    quote.bind("field_name", std::string("age"));

    auto result = quote.parse_function(
        "fnc ${field_name}() -> int {\n"
        "    rtn 0\n"
        "}");
    ASSERT_TRUE(result.has_value()) << result.error();

    auto& func = *result;
    EXPECT_EQ(func.name.name, "age");
    EXPECT_TRUE(func.has_return_type);
    EXPECT_EQ(func.return_type.type_name.name, "int");
    EXPECT_EQ(func.parameters.size(), 0u);
}

// =============================================================================
// parse_function with parameter — setter generation
// =============================================================================

TEST(AstQuoteTest, ParseFunctionGeneratesSetter) {
    AstQuote quote;
    quote.bind("field_name", std::string("age"));

    auto result = quote.parse_function(
        "fnc set_${field_name}(val v: int) {\n"
        "}");
    ASSERT_TRUE(result.has_value()) << result.error();

    auto& func = *result;
    EXPECT_EQ(func.name.name, "set_age");
    ASSERT_EQ(func.parameters.size(), 1u);
    EXPECT_EQ(func.parameters[0].name.name, "v");
    EXPECT_EQ(func.parameters[0].type.type_name.name, "int");
}

// =============================================================================
// Type interpolation — replacing type annotations
// =============================================================================

TEST(AstQuoteTest, TypeInterpolationInReturnType) {
    AstQuote quote;
    quote.bind("field_name", std::string("age"));

    type_annotation float_type;
    float_type.type_name.name = "float";
    float_type.is_nullable = false;
    quote.bind("field_type", std::move(float_type));

    auto result = quote.parse_function(
        "fnc ${field_name}() -> ${field_type} {\n"
        "    rtn 0\n"
        "}");
    ASSERT_TRUE(result.has_value()) << result.error();

    auto& func = *result;
    EXPECT_EQ(func.name.name, "age");
    EXPECT_TRUE(func.has_return_type);
    EXPECT_EQ(func.return_type.type_name.name, "float");
}

TEST(AstQuoteTest, TypeInterpolationInParameter) {
    AstQuote quote;
    quote.bind("field_name", std::string("age"));

    type_annotation int_type;
    int_type.type_name.name = "int";
    int_type.is_nullable = false;
    quote.bind("field_type", std::move(int_type));

    auto result = quote.parse_function(
        "fnc set_${field_name}(val v: ${field_type}) {\n"
        "}");
    ASSERT_TRUE(result.has_value()) << result.error();

    auto& func = *result;
    EXPECT_EQ(func.name.name, "set_age");
    ASSERT_EQ(func.parameters.size(), 1u);
    EXPECT_EQ(func.parameters[0].type.type_name.name, "int");
}

// =============================================================================
// Binding management
// =============================================================================

TEST(AstQuoteTest, HasBindingReturnsCorrectly) {
    AstQuote quote;
    EXPECT_FALSE(quote.has_binding("x"));
    quote.bind("x", std::string("hello"));
    EXPECT_TRUE(quote.has_binding("x"));
}

TEST(AstQuoteTest, ClearRemovesAllBindings) {
    AstQuote quote;
    quote.bind("x", std::string("hello"));
    quote.bind("y", std::string("world"));
    EXPECT_TRUE(quote.has_binding("x"));
    EXPECT_TRUE(quote.has_binding("y"));

    quote.clear();
    EXPECT_FALSE(quote.has_binding("x"));
    EXPECT_FALSE(quote.has_binding("y"));
}

// =============================================================================
// Template with no interpolation — passthrough
// =============================================================================

TEST(AstQuoteTest, NoInterpolationPassthrough) {
    AstQuote quote;
    auto result = quote.parse("val x = 42");
    ASSERT_TRUE(result.has_value()) << result.error();
    ASSERT_EQ(result->expressions.size(), 1u);
}

// =============================================================================
// Struct definition template
// =============================================================================

TEST(AstQuoteTest, StructDefinitionTemplate) {
    AstQuote quote;
    quote.bind("type_name", std::string("Point"));

    auto result = quote.parse(
        "struct ${type_name} {\n"
        "    val x: float\n"
        "    val y: float\n"
        "}");
    ASSERT_TRUE(result.has_value()) << result.error();
    ASSERT_EQ(result->expressions.size(), 1u);

    auto* sd = boost::get<boost::spirit::x3::forward_ast<struct_definition>>(
        &result->expressions[0]);
    ASSERT_NE(sd, nullptr);
    EXPECT_EQ(sd->get().name.name, "Point");
    EXPECT_EQ(sd->get().fields.size(), 2u);
}
// main() provided by gtest_main
