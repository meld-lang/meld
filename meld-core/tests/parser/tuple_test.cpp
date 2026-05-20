#include <gtest/gtest.h>
#include "meld/parser/parser.hpp"
#include "meld/parser/ast.hpp"

using namespace meld::parser;
using namespace meld::parser::ast;

// Test empty tuple
TEST(TupleTest, EmptyTuple) {
    Parser parser;
    expression result;
    
    std::string input = R"(())";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* tuple = boost::get<x3::forward_ast<tuple_literal>>(&result);
    ASSERT_NE(tuple, nullptr) << "Expected tuple_literal";
    
    const tuple_literal& tup = tuple->get();
    EXPECT_EQ(tup.elements.size(), 0);
}

// Test simple tuple with two elements
TEST(TupleTest, SimpleTuple) {
    Parser parser;
    expression result;
    
    std::string input = R"((1, "hello"))";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* tuple = boost::get<x3::forward_ast<tuple_literal>>(&result);
    ASSERT_NE(tuple, nullptr) << "Expected tuple_literal";
    
    const tuple_literal& tup = tuple->get();
    ASSERT_EQ(tup.elements.size(), 2);
    
    // Check first element
    EXPECT_FALSE(tup.elements[0].is_named);
    auto* int_val = boost::get<integer_literal>(&tup.elements[0].value.get());
    ASSERT_NE(int_val, nullptr);
    EXPECT_EQ(int_val->value, 1);
    
    // Check second element
    EXPECT_FALSE(tup.elements[1].is_named);
    auto* str_val = boost::get<string_literal>(&tup.elements[1].value.get());
    ASSERT_NE(str_val, nullptr);
    EXPECT_EQ(str_val->value, "hello");
}

// Test tuple with three elements
TEST(TupleTest, TripleTuple) {
    Parser parser;
    expression result;
    
    std::string input = R"((42, 3.14, true))";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* tuple = boost::get<x3::forward_ast<tuple_literal>>(&result);
    ASSERT_NE(tuple, nullptr);
    
    const tuple_literal& tup = tuple->get();
    ASSERT_EQ(tup.elements.size(), 3);
    
    EXPECT_FALSE(tup.elements[0].is_named);
    EXPECT_FALSE(tup.elements[1].is_named);
    EXPECT_FALSE(tup.elements[2].is_named);
}

// Test named tuple
TEST(TupleTest, NamedTuple) {
    Parser parser;
    expression result;
    
    std::string input = R"((x = 10, y = 20, z = 30))";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* tuple = boost::get<x3::forward_ast<tuple_literal>>(&result);
    ASSERT_NE(tuple, nullptr);
    
    const tuple_literal& tup = tuple->get();
    ASSERT_EQ(tup.elements.size(), 3);
    
    // Check first element
    EXPECT_TRUE(tup.elements[0].is_named);
    EXPECT_EQ(tup.elements[0].name, "x");
    auto* x_val = boost::get<integer_literal>(&tup.elements[0].value.get());
    ASSERT_NE(x_val, nullptr);
    EXPECT_EQ(x_val->value, 10);
    
    // Check second element
    EXPECT_TRUE(tup.elements[1].is_named);
    EXPECT_EQ(tup.elements[1].name, "y");
    auto* y_val = boost::get<integer_literal>(&tup.elements[1].value.get());
    ASSERT_NE(y_val, nullptr);
    EXPECT_EQ(y_val->value, 20);
    
    // Check third element
    EXPECT_TRUE(tup.elements[2].is_named);
    EXPECT_EQ(tup.elements[2].name, "z");
    auto* z_val = boost::get<integer_literal>(&tup.elements[2].value.get());
    ASSERT_NE(z_val, nullptr);
    EXPECT_EQ(z_val->value, 30);
}

// Test mixed named and unnamed tuple
TEST(TupleTest, MixedTuple) {
    Parser parser;
    expression result;
    
    std::string input = R"((1, name = "Alice", 30))";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* tuple = boost::get<x3::forward_ast<tuple_literal>>(&result);
    ASSERT_NE(tuple, nullptr);
    
    const tuple_literal& tup = tuple->get();
    ASSERT_EQ(tup.elements.size(), 3);
    
    EXPECT_FALSE(tup.elements[0].is_named);
    EXPECT_TRUE(tup.elements[1].is_named);
    EXPECT_EQ(tup.elements[1].name, "name");
    EXPECT_FALSE(tup.elements[2].is_named);
}

// Test tuple with trailing comma
TEST(TupleTest, TrailingComma) {
    Parser parser;
    expression result;
    
    std::string input = R"((1, 2, 3,))";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* tuple = boost::get<x3::forward_ast<tuple_literal>>(&result);
    ASSERT_NE(tuple, nullptr);
    
    const tuple_literal& tup = tuple->get();
    EXPECT_EQ(tup.elements.size(), 3);
}

// Test single element tuple
TEST(TupleTest, SingleElement) {
    Parser parser;
    expression result;
    
    std::string input = R"((42))";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* tuple = boost::get<x3::forward_ast<tuple_literal>>(&result);
    ASSERT_NE(tuple, nullptr);
    
    const tuple_literal& tup = tuple->get();
    ASSERT_EQ(tup.elements.size(), 1);
    
    auto* int_val = boost::get<integer_literal>(&tup.elements[0].value.get());
    ASSERT_NE(int_val, nullptr);
    EXPECT_EQ(int_val->value, 42);
}

// Test tuple in val declaration
TEST(TupleTest, ValDeclarationWithTuple) {
    Parser parser;
    expression result;
    
    std::string input = R"(val pair = (1, "hello"))";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* val_decl = boost::get<x3::forward_ast<val_declaration>>(&result);
    ASSERT_NE(val_decl, nullptr);
    
    const val_declaration& decl = val_decl->get();
    EXPECT_EQ(decl.name.name, "pair");
    
    auto* tuple = boost::get<x3::forward_ast<tuple_literal>>(&decl.value.get());
    ASSERT_NE(tuple, nullptr);
    
    const tuple_literal& tup = tuple->get();
    EXPECT_EQ(tup.elements.size(), 2);
}


// Test per-binding destructuring (Req 169.7)
TEST(TupleTest, SimpleDestructuring) {
    Parser parser;
    expression result;
    
    std::string input = R"((val a, val b) = (1, 2))";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* destr = boost::get<x3::forward_ast<tuple_destructuring>>(&result);
    ASSERT_NE(destr, nullptr) << "Expected tuple_destructuring";
    
    const tuple_destructuring& dest = destr->get();
    ASSERT_EQ(dest.bindings.size(), 2);
    EXPECT_EQ(dest.bindings[0].name.name, "a");
    EXPECT_TRUE(dest.bindings[0].is_val);
    EXPECT_EQ(dest.bindings[1].name.name, "b");
    EXPECT_TRUE(dest.bindings[1].is_val);
    
    // Check the tuple expression
    auto* tuple = boost::get<x3::forward_ast<tuple_literal>>(&dest.tuple_expr.get());
    ASSERT_NE(tuple, nullptr);
    EXPECT_EQ(tuple->get().elements.size(), 2);
}

// Test per-binding destructuring with underscore placeholder
TEST(TupleTest, DestructuringWithUnderscore) {
    Parser parser;
    expression result;
    
    std::string input = R"((val a, _, val c) = (1, 2, 3))";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* destr = boost::get<x3::forward_ast<tuple_destructuring>>(&result);
    ASSERT_NE(destr, nullptr);
    
    const tuple_destructuring& dest = destr->get();
    ASSERT_EQ(dest.bindings.size(), 3);
    EXPECT_EQ(dest.bindings[0].name.name, "a");
    EXPECT_TRUE(dest.bindings[0].is_val);
    EXPECT_EQ(dest.bindings[1].name.name, "_");
    EXPECT_TRUE(dest.bindings[1].is_placeholder);
    EXPECT_EQ(dest.bindings[2].name.name, "c");
    EXPECT_TRUE(dest.bindings[2].is_val);
}

// Test per-binding destructuring with multiple underscores
TEST(TupleTest, DestructuringMultipleUnderscores) {
    Parser parser;
    expression result;
    
    std::string input = R"((val first, _, _, val last) = (1, 2, 3, 4))";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* destr = boost::get<x3::forward_ast<tuple_destructuring>>(&result);
    ASSERT_NE(destr, nullptr);
    
    const tuple_destructuring& dest = destr->get();
    ASSERT_EQ(dest.bindings.size(), 4);
    EXPECT_EQ(dest.bindings[0].name.name, "first");
    EXPECT_EQ(dest.bindings[1].name.name, "_");
    EXPECT_EQ(dest.bindings[2].name.name, "_");
    EXPECT_EQ(dest.bindings[3].name.name, "last");
}

// Test per-binding destructuring from identifier
TEST(TupleTest, DestructuringFromIdentifier) {
    Parser parser;
    expression result;
    
    std::string input = R"((val x, val y) = pair)";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* destr = boost::get<x3::forward_ast<tuple_destructuring>>(&result);
    ASSERT_NE(destr, nullptr);
    
    const tuple_destructuring& dest = destr->get();
    ASSERT_EQ(dest.bindings.size(), 2);
    EXPECT_EQ(dest.bindings[0].name.name, "x");
    EXPECT_EQ(dest.bindings[1].name.name, "y");
    
    // Check the tuple expression is an identifier
    auto* id = boost::get<identifier>(&dest.tuple_expr.get());
    ASSERT_NE(id, nullptr);
    EXPECT_EQ(id->name, "pair");
}

// Test mixed val/var qualifiers (Req 169.9)
TEST(TupleTest, MixedQualifiers) {
    Parser parser;
    expression result;
    
    std::string input = R"((val key, var value) = entry)";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* destr = boost::get<x3::forward_ast<tuple_destructuring>>(&result);
    ASSERT_NE(destr, nullptr);
    
    const tuple_destructuring& dest = destr->get();
    ASSERT_EQ(dest.bindings.size(), 2);
    EXPECT_EQ(dest.bindings[0].name.name, "key");
    EXPECT_TRUE(dest.bindings[0].is_val);
    EXPECT_EQ(dest.bindings[1].name.name, "value");
    EXPECT_FALSE(dest.bindings[1].is_val); // var
}

// Test old shorthand syntax is rejected (Req 169.7)
TEST(TupleTest, OldShorthandRejected) {
    Parser parser;
    expression result;
    
    std::string input = R"(val (a, b) = (1, 2))";
    
    EXPECT_FALSE(parser.parse_expression(input, result));
    EXPECT_NE(parser.error_message().find("no longer supported"), std::string::npos);
}

// Test tuple indexing with numeric index
TEST(TupleTest, NumericIndexing) {
    Parser parser;
    expression result;
    
    std::string input = R"(tuple.0)";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* indexing = boost::get<x3::forward_ast<tuple_indexing>>(&result);
    ASSERT_NE(indexing, nullptr) << "Expected tuple_indexing";
    
    const tuple_indexing& idx = indexing->get();
    EXPECT_TRUE(idx.is_numeric);
    EXPECT_EQ(idx.index, "0");
    
    // Check the tuple expression is an identifier
    auto* tuple_id = boost::get<identifier>(&idx.tuple.get());
    ASSERT_NE(tuple_id, nullptr);
    EXPECT_EQ(tuple_id->name, "tuple");
}

// Test tuple indexing with named field
TEST(TupleTest, NamedFieldAccess) {
    Parser parser;
    expression result;
    
    std::string input = R"(point.x)";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* indexing = boost::get<x3::forward_ast<tuple_indexing>>(&result);
    ASSERT_NE(indexing, nullptr) << "Expected tuple_indexing";
    
    const tuple_indexing& idx = indexing->get();
    EXPECT_FALSE(idx.is_numeric);
    EXPECT_EQ(idx.index, "x");
    
    // Check the tuple expression is an identifier
    auto* tuple_id = boost::get<identifier>(&idx.tuple.get());
    ASSERT_NE(tuple_id, nullptr);
    EXPECT_EQ(tuple_id->name, "point");
}

// Test chained tuple indexing
TEST(TupleTest, ChainedIndexing) {
    Parser parser;
    expression result;
    
    std::string input = R"(nested.0.x)";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* outer_indexing = boost::get<x3::forward_ast<tuple_indexing>>(&result);
    ASSERT_NE(outer_indexing, nullptr) << "Expected tuple_indexing";
    
    const tuple_indexing& outer_idx = outer_indexing->get();
    EXPECT_FALSE(outer_idx.is_numeric);
    EXPECT_EQ(outer_idx.index, "x");
    
    // Check the inner tuple indexing
    auto* inner_indexing = boost::get<x3::forward_ast<tuple_indexing>>(&outer_idx.tuple.get());
    ASSERT_NE(inner_indexing, nullptr) << "Expected nested tuple_indexing";
    
    const tuple_indexing& inner_idx = inner_indexing->get();
    EXPECT_TRUE(inner_idx.is_numeric);
    EXPECT_EQ(inner_idx.index, "0");
    
    // Check the base identifier
    auto* base_id = boost::get<identifier>(&inner_idx.tuple.get());
    ASSERT_NE(base_id, nullptr);
    EXPECT_EQ(base_id->name, "nested");
}

// Test function parameter destructuring
TEST(TupleTest, FunctionParameterDestructuring) {
    Parser parser;
    expression result;
    
    std::string input = R"(fn processPoint((x, y): (int, int)) { })";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* func_def = boost::get<x3::forward_ast<function_definition>>(&result);
    ASSERT_NE(func_def, nullptr) << "Expected function_definition";
    
    const function_definition& func = func_def->get();
    EXPECT_EQ(func.name.name, "processPoint");
    ASSERT_EQ(func.parameters.size(), 1);
    
    const function_parameter& param = func.parameters[0];
    EXPECT_TRUE(param.is_destructured);
    ASSERT_EQ(param.destructured_names.size(), 2);
    EXPECT_EQ(param.destructured_names[0].name, "x");
    EXPECT_EQ(param.destructured_names[1].name, "y");
    
    // Check the type annotation
    EXPECT_EQ(param.type.type_name.name, "");  // Should be empty for tuple types
    // Note: Full tuple type parsing would require more complex type annotation handling
}

// Test function parameter destructuring with more elements
TEST(TupleTest, FunctionParameterDestructuringMultiple) {
    Parser parser;
    expression result;
    
    std::string input = R"(fn processData((a, b, c): (int, string, bool)) { })";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* func_def = boost::get<x3::forward_ast<function_definition>>(&result);
    ASSERT_NE(func_def, nullptr) << "Expected function_definition";
    
    const function_definition& func = func_def->get();
    EXPECT_EQ(func.name.name, "processData");
    ASSERT_EQ(func.parameters.size(), 1);
    
    const function_parameter& param = func.parameters[0];
    EXPECT_TRUE(param.is_destructured);
    ASSERT_EQ(param.destructured_names.size(), 3);
    EXPECT_EQ(param.destructured_names[0].name, "a");
    EXPECT_EQ(param.destructured_names[1].name, "b");
    EXPECT_EQ(param.destructured_names[2].name, "c");
}

// Test mixed regular and destructured parameters
TEST(TupleTest, MixedFunctionParameters) {
    Parser parser;
    expression result;
    
    std::string input = R"(fn mixedParams(id: int, (x, y): (int, int), name: string) { })";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* func_def = boost::get<x3::forward_ast<function_definition>>(&result);
    ASSERT_NE(func_def, nullptr) << "Expected function_definition";
    
    const function_definition& func = func_def->get();
    EXPECT_EQ(func.name.name, "mixedParams");
    ASSERT_EQ(func.parameters.size(), 3);
    
    // First parameter: regular
    const function_parameter& param1 = func.parameters[0];
    EXPECT_FALSE(param1.is_destructured);
    EXPECT_EQ(param1.name.name, "id");
    
    // Second parameter: destructured
    const function_parameter& param2 = func.parameters[1];
    EXPECT_TRUE(param2.is_destructured);
    ASSERT_EQ(param2.destructured_names.size(), 2);
    EXPECT_EQ(param2.destructured_names[0].name, "x");
    EXPECT_EQ(param2.destructured_names[1].name, "y");
    
    // Third parameter: regular
    const function_parameter& param3 = func.parameters[2];
    EXPECT_FALSE(param3.is_destructured);
    EXPECT_EQ(param3.name.name, "name");
}

