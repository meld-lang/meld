#include <gtest/gtest.h>
#include "meld/parser/parser.hpp"
#include "meld/parser/ast.hpp"
#include <boost/variant/get.hpp>

using namespace meld::parser;
using namespace meld::parser::ast;

// Test fnc keyword with simple function
TEST(FunctionSignatureTest, FncKeywordFunctionDefinition) {
    Parser parser;
    expression result;
    
    std::string input = R"(fnc multiply(val x: Int, val y: Int) -> Int {
        val result = x
    })";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* func_def = boost::get<x3::forward_ast<function_definition>>(&result);
    ASSERT_NE(func_def, nullptr) << "Expected function_definition";
    
    EXPECT_EQ(func_def->get().name.name, "multiply");
    EXPECT_EQ(func_def->get().parameters.size(), 2);
    EXPECT_EQ(func_def->get().parameters[0].name.name, "x");
    EXPECT_EQ(func_def->get().parameters[0].type.type_name.name, "Int");
    EXPECT_EQ(func_def->get().parameters[1].name.name, "y");
    EXPECT_EQ(func_def->get().parameters[1].type.type_name.name, "Int");
    EXPECT_TRUE(func_def->get().has_return_type);
    EXPECT_EQ(func_def->get().return_type.type_name.name, "Int");
}

// Test fn keyword with simple function
TEST(FunctionSignatureTest, SimpleFunctionDefinition) {
    Parser parser;
    expression result;
    
    std::string input = R"(fn add(a: Int, b: Int) -> Int {
        val result = a
    })";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* func_def = boost::get<x3::forward_ast<function_definition>>(&result);
    ASSERT_NE(func_def, nullptr) << "Expected function_definition";
    
    EXPECT_EQ(func_def->get().name.name, "add");
    EXPECT_EQ(func_def->get().parameters.size(), 2);
    EXPECT_EQ(func_def->get().parameters[0].name.name, "a");
    EXPECT_EQ(func_def->get().parameters[0].type.type_name.name, "Int");
    EXPECT_EQ(func_def->get().parameters[1].name.name, "b");
    EXPECT_EQ(func_def->get().parameters[1].type.type_name.name, "Int");
    EXPECT_TRUE(func_def->get().has_return_type);
    EXPECT_EQ(func_def->get().return_type.type_name.name, "Int");
}

// Test function with default parameter values
TEST(FunctionSignatureTest, FunctionWithDefaultValues) {
    Parser parser;
    expression result;
    
    std::string input = R"(fn greet(name: String = "World", greeting: String = "Hello") -> String {
        val msg = greeting
    })";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* func_def = boost::get<x3::forward_ast<function_definition>>(&result);
    ASSERT_NE(func_def, nullptr);
    
    EXPECT_EQ(func_def->get().parameters.size(), 2);
    EXPECT_TRUE(func_def->get().parameters[0].has_default);
    EXPECT_TRUE(func_def->get().parameters[1].has_default);
    
    // Check default value for first parameter
    auto* default_val1 = boost::get<string_literal>(&func_def->get().parameters[0].default_value.get());
    ASSERT_NE(default_val1, nullptr);
    EXPECT_EQ(default_val1->value, "World");
    
    // Check default value for second parameter
    auto* default_val2 = boost::get<string_literal>(&func_def->get().parameters[1].default_value.get());
    ASSERT_NE(default_val2, nullptr);
    EXPECT_EQ(default_val2->value, "Hello");
}

// Test function with nullable types
TEST(FunctionSignatureTest, FunctionWithNullableTypes) {
    Parser parser;
    expression result;
    
    std::string input = R"(fn process(data: String?) -> Int? {
        val x = 42
    })";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* func_def = boost::get<x3::forward_ast<function_definition>>(&result);
    ASSERT_NE(func_def, nullptr);
    
    EXPECT_TRUE(func_def->get().parameters[0].type.is_nullable);
    EXPECT_TRUE(func_def->get().return_type.is_nullable);
}

// Test function call with named arguments
TEST(FunctionSignatureTest, FunctionCallWithNamedArguments) {
    Parser parser;
    expression result;
    
    std::string input = R"(greet(name = "Alice", greeting = "Hi"))";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* func_call = boost::get<x3::forward_ast<function_call>>(&result);
    ASSERT_NE(func_call, nullptr);
    
    EXPECT_EQ(func_call->get().function_name.name, "greet");
    EXPECT_EQ(func_call->get().named_arguments.size(), 2);
    EXPECT_EQ(func_call->get().named_arguments[0].name.name, "name");
    EXPECT_EQ(func_call->get().named_arguments[1].name.name, "greeting");
    
    // Check argument values
    auto* arg1_val = boost::get<string_literal>(&func_call->get().named_arguments[0].value.get());
    ASSERT_NE(arg1_val, nullptr);
    EXPECT_EQ(arg1_val->value, "Alice");
    
    auto* arg2_val = boost::get<string_literal>(&func_call->get().named_arguments[1].value.get());
    ASSERT_NE(arg2_val, nullptr);
    EXPECT_EQ(arg2_val->value, "Hi");
}

// Test function call with mixed positional and named arguments
TEST(FunctionSignatureTest, FunctionCallWithMixedArguments) {
    Parser parser;
    expression result;
    
    std::string input = R"(configure("localhost", port = 8080, ssl = false))";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* func_call = boost::get<x3::forward_ast<function_call>>(&result);
    ASSERT_NE(func_call, nullptr);
    
    EXPECT_EQ(func_call->get().arguments.size(), 1);  // "localhost"
    EXPECT_EQ(func_call->get().named_arguments.size(), 2);  // port, ssl
    
    // Check positional argument
    auto* pos_arg = boost::get<string_literal>(&func_call->get().arguments[0].get());
    ASSERT_NE(pos_arg, nullptr);
    EXPECT_EQ(pos_arg->value, "localhost");
    
    // Check named arguments
    EXPECT_EQ(func_call->get().named_arguments[0].name.name, "port");
    EXPECT_EQ(func_call->get().named_arguments[1].name.name, "ssl");
}

// Test function without return type
TEST(FunctionSignatureTest, FunctionWithoutReturnType) {
    Parser parser;
    expression result;
    
    std::string input = R"(fn doSomething(x: Int) {
        val y = x
    })";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* func_def = boost::get<x3::forward_ast<function_definition>>(&result);
    ASSERT_NE(func_def, nullptr);
    
    EXPECT_FALSE(func_def->get().has_return_type);
}

// Test function with no parameters
TEST(FunctionSignatureTest, FunctionWithNoParameters) {
    Parser parser;
    expression result;
    
    std::string input = R"(fn getValue() -> Int {
        val x = 42
    })";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* func_def = boost::get<x3::forward_ast<function_definition>>(&result);
    ASSERT_NE(func_def, nullptr);
    
    EXPECT_EQ(func_def->get().parameters.size(), 0);
    EXPECT_TRUE(func_def->get().has_return_type);
}

// Test that positional arguments cannot follow named arguments
TEST(FunctionSignatureTest, PositionalAfterNamedError) {
    Parser parser;
    expression result;
    
    std::string input = R"(foo(name = "test", 42))";
    
    EXPECT_FALSE(parser.parse_expression(input, result));
    EXPECT_NE(parser.error_message().find("Positional arguments cannot follow named arguments"), 
              std::string::npos);
}

// Test parameter decorators
TEST(FunctionSignatureTest, ParameterDecorators) {
    Parser parser;
    expression result;
    
    std::string input = R"(fn process(@const data: List, @mut counter: Int) -> Int {
        val x = 42
    })";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* func_def = boost::get<x3::forward_ast<function_definition>>(&result);
    ASSERT_NE(func_def, nullptr);
    
    EXPECT_EQ(func_def->get().parameters.size(), 2);
    EXPECT_EQ(func_def->get().parameters[0].decorator, ParameterDecorator::CONST);
    EXPECT_EQ(func_def->get().parameters[1].decorator, ParameterDecorator::MUT);
}

// Test @ref decorator
TEST(FunctionSignatureTest, RefDecorator) {
    Parser parser;
    expression result;
    
    std::string input = R"(fn modifyPoint(@ref point: Point) -> Unit {
        val x = 10
    })";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* func_def = boost::get<x3::forward_ast<function_definition>>(&result);
    ASSERT_NE(func_def, nullptr);
    
    EXPECT_EQ(func_def->get().parameters[0].decorator, ParameterDecorator::REF);
}

// Test @move decorator
TEST(FunctionSignatureTest, MoveDecorator) {
    Parser parser;
    expression result;
    
    std::string input = R"(fn consume(@move resource: FileHandle) -> Unit {
        val x = 42
    })";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* func_def = boost::get<x3::forward_ast<function_definition>>(&result);
    ASSERT_NE(func_def, nullptr);
    
    EXPECT_EQ(func_def->get().parameters[0].decorator, ParameterDecorator::MOVE);
}

// Test mixed decorators and default values
TEST(FunctionSignatureTest, DecoratorsWithDefaults) {
    Parser parser;
    expression result;
    
    std::string input = R"(fn configure(@const host: String = "localhost", @const port: Int = 8080) -> Unit {
        val x = 42
    })";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* func_def = boost::get<x3::forward_ast<function_definition>>(&result);
    ASSERT_NE(func_def, nullptr);
    
    EXPECT_EQ(func_def->get().parameters.size(), 2);
    EXPECT_EQ(func_def->get().parameters[0].decorator, ParameterDecorator::CONST);
    EXPECT_TRUE(func_def->get().parameters[0].has_default);
    EXPECT_EQ(func_def->get().parameters[1].decorator, ParameterDecorator::CONST);
    EXPECT_TRUE(func_def->get().parameters[1].has_default);
}

// Test named return values
TEST(FunctionSignatureTest, NamedReturnValues) {
    Parser parser;
    expression result;
    
    std::string input = R"(fn divmod(a: Int, b: Int) -> (quotient: Int, remainder: Int) {
        val x = 42
    })";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* func_def = boost::get<x3::forward_ast<function_definition>>(&result);
    ASSERT_NE(func_def, nullptr);
    
    EXPECT_TRUE(func_def->get().has_named_returns);
    EXPECT_FALSE(func_def->get().has_return_type);
    EXPECT_EQ(func_def->get().named_returns.size(), 2);
    EXPECT_EQ(func_def->get().named_returns[0].name.name, "quotient");
    EXPECT_EQ(func_def->get().named_returns[0].type.type_name.name, "Int");
    EXPECT_EQ(func_def->get().named_returns[1].name.name, "remainder");
    EXPECT_EQ(func_def->get().named_returns[1].type.type_name.name, "Int");
}

// Test named return values with defaults
TEST(FunctionSignatureTest, NamedReturnValuesWithDefaults) {
    Parser parser;
    expression result;
    
    std::string input = R"(fn analyze(data: List) -> (mean: Float = 0.0, median: Float = 0.0, mode: Int = 0) {
        val x = 42
    })";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* func_def = boost::get<x3::forward_ast<function_definition>>(&result);
    ASSERT_NE(func_def, nullptr);
    
    EXPECT_TRUE(func_def->get().has_named_returns);
    EXPECT_EQ(func_def->get().named_returns.size(), 3);
    EXPECT_TRUE(func_def->get().named_returns[0].has_default);
    EXPECT_TRUE(func_def->get().named_returns[1].has_default);
    EXPECT_TRUE(func_def->get().named_returns[2].has_default);
}

// Test named return values with nullable types
TEST(FunctionSignatureTest, NamedReturnValuesNullable) {
    Parser parser;
    expression result;
    
    std::string input = R"(fn process(data: String) -> (result: Int?, error: String?) {
        val x = 42
    })";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* func_def = boost::get<x3::forward_ast<function_definition>>(&result);
    ASSERT_NE(func_def, nullptr);
    
    EXPECT_TRUE(func_def->get().has_named_returns);
    EXPECT_EQ(func_def->get().named_returns.size(), 2);
    EXPECT_TRUE(func_def->get().named_returns[0].type.is_nullable);
    EXPECT_TRUE(func_def->get().named_returns[1].type.is_nullable);
}

// Test rtn keyword with simple return
TEST(FunctionSignatureTest, SimpleReturnStatement) {
    Parser parser;
    expression result;
    
    std::string input = R"(rtn 42)";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* ret_stmt = boost::get<x3::forward_ast<return_statement>>(&result);
    ASSERT_NE(ret_stmt, nullptr) << "Expected return_statement";
    
    EXPECT_TRUE(ret_stmt->get().has_expression);
    EXPECT_FALSE(ret_stmt->get().has_named_returns);
    EXPECT_FALSE(ret_stmt->get().is_tuple_return);
    
    auto* ret_val = boost::get<integer_literal>(&ret_stmt->get().expr.get());
    ASSERT_NE(ret_val, nullptr);
    EXPECT_EQ(ret_val->value, 42);
}

// Test rtn keyword with named returns
TEST(FunctionSignatureTest, NamedReturnStatement) {
    Parser parser;
    expression result;
    
    std::string input = R"(rtn (quotient = a / b, remainder = a % b))";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* ret_stmt = boost::get<x3::forward_ast<return_statement>>(&result);
    ASSERT_NE(ret_stmt, nullptr) << "Expected return_statement";
    
    EXPECT_FALSE(ret_stmt->get().has_expression);
    EXPECT_TRUE(ret_stmt->get().has_named_returns);
    EXPECT_FALSE(ret_stmt->get().is_tuple_return);
    EXPECT_EQ(ret_stmt->get().named_returns.size(), 2);
    EXPECT_EQ(ret_stmt->get().named_returns[0].name.name, "quotient");
    EXPECT_EQ(ret_stmt->get().named_returns[1].name.name, "remainder");
}

// Test rtn keyword with tuple return
TEST(FunctionSignatureTest, TupleReturnStatement) {
    Parser parser;
    expression result;
    
    std::string input = R"(rtn (100, 200, 300))";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* ret_stmt = boost::get<x3::forward_ast<return_statement>>(&result);
    ASSERT_NE(ret_stmt, nullptr) << "Expected return_statement";
    
    EXPECT_FALSE(ret_stmt->get().has_expression);
    EXPECT_FALSE(ret_stmt->get().has_named_returns);
    EXPECT_TRUE(ret_stmt->get().is_tuple_return);
    EXPECT_EQ(ret_stmt->get().tuple_values.size(), 3);
    
    auto* val1 = boost::get<integer_literal>(&ret_stmt->get().tuple_values[0].get());
    ASSERT_NE(val1, nullptr);
    EXPECT_EQ(val1->value, 100);
    
    auto* val2 = boost::get<integer_literal>(&ret_stmt->get().tuple_values[1].get());
    ASSERT_NE(val2, nullptr);
    EXPECT_EQ(val2->value, 200);
    
    auto* val3 = boost::get<integer_literal>(&ret_stmt->get().tuple_values[2].get());
    ASSERT_NE(val3, nullptr);
    EXPECT_EQ(val3->value, 300);
}

// Test empty rtn statement
TEST(FunctionSignatureTest, EmptyReturnStatement) {
    Parser parser;
    expression result;
    
    std::string input = R"(rtn)";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* ret_stmt = boost::get<x3::forward_ast<return_statement>>(&result);
    ASSERT_NE(ret_stmt, nullptr) << "Expected return_statement";
    
    EXPECT_FALSE(ret_stmt->get().has_expression);
    EXPECT_FALSE(ret_stmt->get().has_named_returns);
    EXPECT_FALSE(ret_stmt->get().is_tuple_return);
}

// Test named return value assignment
TEST(FunctionSignatureTest, NamedReturnAssignment) {
    Parser parser;
    expression result;
    
    std::string input = R"(quotient = a / b)";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* assignment = boost::get<x3::forward_ast<named_return_assignment>>(&result);
    ASSERT_NE(assignment, nullptr) << "Expected named_return_assignment";
    
    EXPECT_EQ(assignment->get().return_name.name, "quotient");
    
    // Check that the value is a binary operation (a / b)
    auto* binary_op = boost::get<x3::forward_ast<binary_operation>>(&assignment->get().value.get());
    ASSERT_NE(binary_op, nullptr);
    EXPECT_EQ(binary_op->get().op, "/");
}

// Test function with named returns and assignments in body
TEST(FunctionSignatureTest, FunctionWithNamedReturnAssignments) {
    Parser parser;
    expression result;
    
    std::string input = R"(fnc divide(val a: Int, val b: Int) -> (quotient: Int, remainder: Int) {
        quotient = a / b
        remainder = a % b
    })";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* func_def = boost::get<x3::forward_ast<function_definition>>(&result);
    ASSERT_NE(func_def, nullptr) << "Expected function_definition";
    
    EXPECT_EQ(func_def->get().name.name, "divide");
    EXPECT_TRUE(func_def->get().has_named_returns);
    EXPECT_EQ(func_def->get().named_returns.size(), 2);
    EXPECT_EQ(func_def->get().named_returns[0].name.name, "quotient");
    EXPECT_EQ(func_def->get().named_returns[1].name.name, "remainder");
    
    // Check function body contains assignments
    EXPECT_EQ(func_def->get().body.get().statements.size(), 2);
}
