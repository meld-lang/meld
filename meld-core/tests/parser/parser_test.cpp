#include <gtest/gtest.h>
#include "meld/parser/parser.hpp"
#include "meld/parser/ast.hpp"

using namespace meld::parser;
using namespace meld::parser::ast;

TEST(ParserTest, ParseSimpleIdentifier) {
    Parser parser;
    expression result;
    
    ASSERT_TRUE(parser.parse_expression("hello", result));
    
    auto* id = boost::get<identifier>(&result);
    ASSERT_NE(id, nullptr);
    EXPECT_EQ(id->name, "hello");
}

TEST(ParserTest, ParseIntegerLiteral) {
    Parser parser;
    expression result;
    
    ASSERT_TRUE(parser.parse_expression("42", result));
    
    auto* lit = boost::get<integer_literal>(&result);
    ASSERT_NE(lit, nullptr);
    EXPECT_EQ(lit->value, 42);
}

TEST(ParserTest, ParseIntegerWithSuffix) {
    Parser parser;
    expression result;
    
    ASSERT_TRUE(parser.parse_expression("42L", result));
    
    auto* lit = boost::get<integer_literal>(&result);
    ASSERT_NE(lit, nullptr);
    EXPECT_EQ(lit->value, 42);
    EXPECT_EQ(lit->suffix, "L");
}

TEST(ParserTest, ParseFloatLiteral) {
    Parser parser;
    expression result;
    
    ASSERT_TRUE(parser.parse_expression("3.14", result));
    
    auto* lit = boost::get<float_literal>(&result);
    ASSERT_NE(lit, nullptr);
    EXPECT_DOUBLE_EQ(lit->value, 3.14);
}

TEST(ParserTest, ParseFloatWithSuffix) {
    Parser parser;
    expression result;
    
    ASSERT_TRUE(parser.parse_expression("3.14F", result));
    
    auto* lit = boost::get<float_literal>(&result);
    ASSERT_NE(lit, nullptr);
    EXPECT_DOUBLE_EQ(lit->value, 3.14);
    EXPECT_EQ(lit->suffix, "F");
}

TEST(ParserTest, ParseStringLiteral) {
    Parser parser;
    expression result;
    
    ASSERT_TRUE(parser.parse_expression("\"hello world\"", result));
    
    auto* lit = boost::get<string_literal>(&result);
    ASSERT_NE(lit, nullptr);
    EXPECT_EQ(lit->value, "hello world");
    EXPECT_FALSE(lit->has_interpolation);
}

TEST(ParserTest, ParseStringWithInterpolation) {
    Parser parser;
    expression result;
    
    // Static strings ("...") no longer support interpolation.
    // Use backtick template strings for interpolation.
    ASSERT_TRUE(parser.parse_expression("\"Hello ${name}!\"", result));
    
    auto* lit = boost::get<string_literal>(&result);
    ASSERT_NE(lit, nullptr);
    // Static string: ${} is literal text, not interpolation
    EXPECT_FALSE(lit->has_interpolation);
    EXPECT_FALSE(lit->is_template);
}

TEST(ParserTest, ParseMultilineString) {
    Parser parser;
    expression result;
    
    ASSERT_TRUE(parser.parse_expression("\"\"\"Line 1\nLine 2\"\"\"", result));
    
    auto* lit = boost::get<multiline_string_literal>(&result);
    ASSERT_NE(lit, nullptr);
    EXPECT_TRUE(lit->value.find("Line 1") != std::string::npos);
}

TEST(ParserTest, ParseRegexLiteral) {
    Parser parser;
    expression result;
    
    ASSERT_TRUE(parser.parse_expression("/[a-z]+/gi", result));
    
    auto* lit = boost::get<regex_literal>(&result);
    ASSERT_NE(lit, nullptr);
    EXPECT_EQ(lit->pattern, "[a-z]+");
    EXPECT_EQ(lit->flags, "gi");
}

TEST(ParserTest, ParseBooleanTrue) {
    Parser parser;
    expression result;
    
    ASSERT_TRUE(parser.parse_expression("true", result));
    
    auto* lit = boost::get<boolean_literal>(&result);
    ASSERT_NE(lit, nullptr);
    EXPECT_TRUE(lit->value);
}

TEST(ParserTest, ParseBooleanFalse) {
    Parser parser;
    expression result;
    
    ASSERT_TRUE(parser.parse_expression("false", result));
    
    auto* lit = boost::get<boolean_literal>(&result);
    ASSERT_NE(lit, nullptr);
    EXPECT_FALSE(lit->value);
}

TEST(ParserTest, ParseValDeclaration) {
    Parser parser;
    expression result;
    
    ASSERT_TRUE(parser.parse_expression("val x = 42", result));
    
    auto* decl = boost::get<x3::forward_ast<val_declaration>>(&result);
    ASSERT_NE(decl, nullptr);
    EXPECT_EQ(decl->get().name.name, "x");
    
    auto* value = boost::get<integer_literal>(&decl->get().value.get());
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(value->value, 42);
}

TEST(ParserTest, ParseVarDeclaration) {
    Parser parser;
    expression result;
    
    ASSERT_TRUE(parser.parse_expression("var y = \"hello\"", result));
    
    auto* decl = boost::get<x3::forward_ast<var_declaration>>(&result);
    ASSERT_NE(decl, nullptr);
    EXPECT_EQ(decl->get().name.name, "y");
    
    auto* value = boost::get<string_literal>(&decl->get().value.get());
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(value->value, "hello");
}

TEST(ParserTest, ParseFunctionCall) {
    Parser parser;
    expression result;
    
    ASSERT_TRUE(parser.parse_expression("foo(42, \"bar\")", result));
    
    auto* call = boost::get<x3::forward_ast<function_call>>(&result);
    ASSERT_NE(call, nullptr);
    EXPECT_EQ(call->get().function_name.name, "foo");
    EXPECT_EQ(call->get().arguments.size(), 2);
}

TEST(ParserTest, ParseArrayExpression) {
    Parser parser;
    expression result;
    
    ASSERT_TRUE(parser.parse_expression("[1, 2, 3]", result));
    
    auto* array = boost::get<x3::forward_ast<anonymous_array_literal>>(&result);
    ASSERT_NE(array, nullptr);
    EXPECT_EQ(array->get().elements.size(), 3);
}

TEST(ParserTest, ParseEmptyArray) {
    Parser parser;
    expression result;
    
    ASSERT_TRUE(parser.parse_expression("[]", result));
    
    auto* array = boost::get<x3::forward_ast<anonymous_array_literal>>(&result);
    ASSERT_NE(array, nullptr);
    EXPECT_EQ(array->get().elements.size(), 0);
}

TEST(ParserTest, ParseNamedTuple) {
    Parser parser;
    expression result;
    
    ASSERT_TRUE(parser.parse_expression("[x: 10, y: 20]", result));
    
    auto* tuple = boost::get<x3::forward_ast<anonymous_tuple_literal>>(&result);
    ASSERT_NE(tuple, nullptr);
    EXPECT_EQ(tuple->get().elements.size(), 2);
    EXPECT_TRUE(tuple->get().elements[0].is_named);
    EXPECT_EQ(tuple->get().elements[0].name, "x");
    EXPECT_TRUE(tuple->get().elements[1].is_named);
    EXPECT_EQ(tuple->get().elements[1].name, "y");
}

TEST(ParserTest, ParseMultipleExpressions) {
    Parser parser;
    std::vector<expression> results;
    
    ASSERT_TRUE(parser.parse_file("val x = 42\nvar y = \"hello\"", results));
    
    ASSERT_EQ(results.size(), 2);
}

TEST(ParserTest, ErrorReportingMissingEquals) {
    Parser parser;
    expression result;
    
    ASSERT_FALSE(parser.parse_expression("val x 42", result));
    EXPECT_FALSE(parser.error_message().empty());
}

TEST(ParserTest, ErrorReportingUnterminatedString) {
    Parser parser;
    expression result;
    
    ASSERT_FALSE(parser.parse_expression("\"unterminated", result));
    EXPECT_TRUE(parser.error_message().find("Unterminated") != std::string::npos);
}

TEST(ParserTest, ErrorReportingWithLineColumn) {
    Parser parser;
    expression result;
    
    // This should fail and report position
    ASSERT_FALSE(parser.parse_expression("val x = ", result));
    EXPECT_FALSE(parser.error_message().empty());
}

TEST(ParserTest, ParseArrayIndexing) {
    Parser parser;
    expression result;
    
    ASSERT_TRUE(parser.parse_expression("arr[0]", result));
    
    auto* indexing = boost::get<x3::forward_ast<array_indexing>>(&result);
    ASSERT_NE(indexing, nullptr);
    
    // Check array part
    auto* array_id = boost::get<identifier>(&indexing->get().array.get());
    ASSERT_NE(array_id, nullptr);
    EXPECT_EQ(array_id->name, "arr");
    
    // Check index part
    auto* index_lit = boost::get<integer_literal>(&indexing->get().index.get());
    ASSERT_NE(index_lit, nullptr);
    EXPECT_EQ(index_lit->value, 0);
}

TEST(ParserTest, ParseArrayIndexingWithVariable) {
    Parser parser;
    expression result;
    
    ASSERT_TRUE(parser.parse_expression("data[index]", result));
    
    auto* indexing = boost::get<x3::forward_ast<array_indexing>>(&result);
    ASSERT_NE(indexing, nullptr);
    
    // Check array part
    auto* array_id = boost::get<identifier>(&indexing->get().array.get());
    ASSERT_NE(array_id, nullptr);
    EXPECT_EQ(array_id->name, "data");
    
    // Check index part
    auto* index_id = boost::get<identifier>(&indexing->get().index.get());
    ASSERT_NE(index_id, nullptr);
    EXPECT_EQ(index_id->name, "index");
}

TEST(ParserTest, ParseChainedArrayIndexing) {
    Parser parser;
    expression result;
    
    ASSERT_TRUE(parser.parse_expression("matrix[i][j]", result));
    
    auto* outer_indexing = boost::get<x3::forward_ast<array_indexing>>(&result);
    ASSERT_NE(outer_indexing, nullptr);
    
    // Check that the array part is also an array_indexing (chained)
    auto* inner_indexing = boost::get<x3::forward_ast<array_indexing>>(&outer_indexing->get().array.get());
    ASSERT_NE(inner_indexing, nullptr);
    
    // Check the innermost array
    auto* matrix_id = boost::get<identifier>(&inner_indexing->get().array.get());
    ASSERT_NE(matrix_id, nullptr);
    EXPECT_EQ(matrix_id->name, "matrix");
}

TEST(ParserTest, ParseArrayLiteralWithIndexing) {
    Parser parser;
    expression result;
    
    ASSERT_TRUE(parser.parse_expression("[1, 2, 3][0]", result));
    
    auto* indexing = boost::get<x3::forward_ast<array_indexing>>(&result);
    ASSERT_NE(indexing, nullptr);
    
    // Check that the array part is an anonymous_array_literal
    auto* array_lit = boost::get<x3::forward_ast<anonymous_array_literal>>(&indexing->get().array.get());
    ASSERT_NE(array_lit, nullptr);
    EXPECT_EQ(array_lit->get().elements.size(), 3);
}
