#include <gtest/gtest.h>
#include "meld/parser/parser.hpp"
#include "meld/parser/ast.hpp"

using namespace meld::parser;
using namespace meld::parser::ast;

TEST(PipelineTest, ParseSimplePipeline) {
    Parser parser;
    expression result;
    
    // Test: data |> parse
    ASSERT_TRUE(parser.parse_expression("data |> parse", result));
    
    auto* pipe = boost::get<x3::forward_ast<pipeline_expression>>(&result);
    ASSERT_NE(pipe, nullptr);
    
    auto& pipe_expr = pipe->get();
    
    // Check left side (value)
    auto* value_id = boost::get<identifier>(&pipe_expr.value.get());
    ASSERT_NE(value_id, nullptr);
    EXPECT_EQ(value_id->name, "data");
    
    // Check right side (function)
    auto* func_id = boost::get<identifier>(&pipe_expr.function.get());
    ASSERT_NE(func_id, nullptr);
    EXPECT_EQ(func_id->name, "parse");
}

TEST(PipelineTest, ParseChainedPipeline) {
    Parser parser;
    expression result;
    
    // Test: data |> parse |> validate
    ASSERT_TRUE(parser.parse_expression("data |> parse |> validate", result));
    
    // The result should be a pipeline expression
    auto* outer_pipe = boost::get<x3::forward_ast<pipeline_expression>>(&result);
    ASSERT_NE(outer_pipe, nullptr);
    
    auto& outer_expr = outer_pipe->get();
    
    // The value of the outer pipeline should be another pipeline expression
    auto* inner_pipe = boost::get<x3::forward_ast<pipeline_expression>>(&outer_expr.value.get());
    ASSERT_NE(inner_pipe, nullptr);
    
    auto& inner_expr = inner_pipe->get();
    
    // Check innermost value (data)
    auto* data_id = boost::get<identifier>(&inner_expr.value.get());
    ASSERT_NE(data_id, nullptr);
    EXPECT_EQ(data_id->name, "data");
    
    // Check inner function (parse)
    auto* parse_id = boost::get<identifier>(&inner_expr.function.get());
    ASSERT_NE(parse_id, nullptr);
    EXPECT_EQ(parse_id->name, "parse");
    
    // Check outer function (validate)
    auto* validate_id = boost::get<identifier>(&outer_expr.function.get());
    ASSERT_NE(validate_id, nullptr);
    EXPECT_EQ(validate_id->name, "validate");
}

TEST(PipelineTest, ParsePipelineWithFunctionCall) {
    Parser parser;
    expression result;
    
    // Test: data |> parse()
    ASSERT_TRUE(parser.parse_expression("data |> parse()", result));
    
    auto* pipe = boost::get<x3::forward_ast<pipeline_expression>>(&result);
    ASSERT_NE(pipe, nullptr);
    
    auto& pipe_expr = pipe->get();
    
    // Check left side (value)
    auto* value_id = boost::get<identifier>(&pipe_expr.value.get());
    ASSERT_NE(value_id, nullptr);
    EXPECT_EQ(value_id->name, "data");
    
    // Check right side (function call)
    auto* func_call = boost::get<x3::forward_ast<function_call>>(&pipe_expr.function.get());
    ASSERT_NE(func_call, nullptr);
    EXPECT_EQ(func_call->get().function_name.name, "parse");
}

TEST(PipelineTest, ParseComplexPipeline) {
    Parser parser;
    expression result;
    
    // Test: 42 |> double |> toString
    ASSERT_TRUE(parser.parse_expression("42 |> double |> toString", result));
    
    auto* outer_pipe = boost::get<x3::forward_ast<pipeline_expression>>(&result);
    ASSERT_NE(outer_pipe, nullptr);
    
    auto& outer_expr = outer_pipe->get();
    
    // The value should be another pipeline
    auto* inner_pipe = boost::get<x3::forward_ast<pipeline_expression>>(&outer_expr.value.get());
    ASSERT_NE(inner_pipe, nullptr);
    
    auto& inner_expr = inner_pipe->get();
    
    // Check the integer literal
    auto* int_lit = boost::get<integer_literal>(&inner_expr.value.get());
    ASSERT_NE(int_lit, nullptr);
    EXPECT_EQ(int_lit->value, 42);
    
    // Check middle function
    auto* double_id = boost::get<identifier>(&inner_expr.function.get());
    ASSERT_NE(double_id, nullptr);
    EXPECT_EQ(double_id->name, "double");
    
    // Check final function
    auto* toString_id = boost::get<identifier>(&outer_expr.function.get());
    ASSERT_NE(toString_id, nullptr);
    EXPECT_EQ(toString_id->name, "toString");
}

TEST(PipelineTest, ParsePipelineInValDeclaration) {
    Parser parser;
    expression result;
    
    // Test: val result = data |> parse |> validate
    ASSERT_TRUE(parser.parse_expression("val result = data |> parse |> validate", result));
    
    auto* val_decl = boost::get<x3::forward_ast<val_declaration>>(&result);
    ASSERT_NE(val_decl, nullptr);
    
    auto& decl = val_decl->get();
    EXPECT_EQ(decl.name.name, "result");
    
    // The value should be a pipeline expression
    auto* pipe = boost::get<x3::forward_ast<pipeline_expression>>(&decl.value.get());
    ASSERT_NE(pipe, nullptr);
}

