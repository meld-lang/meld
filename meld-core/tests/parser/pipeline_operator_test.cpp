#include <gtest/gtest.h>
#include "meld/parser/parser.hpp"
#include "meld/compiler/type_checker.hpp"
#include "meld/meta/metatype.hpp"

using namespace meld::parser;
using namespace meld::compiler;
using namespace meld::meta;

class PipelineOperatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto& registry = TypeRegistry::instance();
        type_checker = std::make_unique<TypeChecker>(registry);
    }
    
    std::unique_ptr<TypeChecker> type_checker;
};

TEST_F(PipelineOperatorTest, ParseSimplePipeline) {
    std::string input = "value |> function";
    
    auto tokens = tokenize(input);
    ASSERT_TRUE(tokens.has_value());
    
    Parser parser(*tokens);
    auto result = parser.parse_expression();
    ASSERT_TRUE(result.has_value());
    
    // Check that we got a pipeline expression
    auto* pipeline = boost::get<boost::spirit::x3::forward_ast<ast::pipeline_expression>>(&result->get());
    ASSERT_NE(pipeline, nullptr);
    
    const auto& pipe_expr = pipeline->get();
    
    // Check the value side
    auto* value_id = boost::get<ast::identifier>(&pipe_expr.value.get());
    ASSERT_NE(value_id, nullptr);
    EXPECT_EQ(value_id->name, "value");
    
    // Check the function side
    auto* func_id = boost::get<ast::identifier>(&pipe_expr.function.get());
    ASSERT_NE(func_id, nullptr);
    EXPECT_EQ(func_id->name, "function");
}

TEST_F(PipelineOperatorTest, ParseChainedPipeline) {
    std::string input = "value |> func1 |> func2";
    
    auto tokens = tokenize(input);
    ASSERT_TRUE(tokens.has_value());
    
    Parser parser(*tokens);
    auto result = parser.parse_expression();
    ASSERT_TRUE(result.has_value());
    
    // Check that we got a pipeline expression
    auto* pipeline = boost::get<boost::spirit::x3::forward_ast<ast::pipeline_expression>>(&result->get());
    ASSERT_NE(pipeline, nullptr);
    
    const auto& pipe_expr = pipeline->get();
    
    // The outer pipeline should have func2 as the function
    auto* func2_id = boost::get<ast::identifier>(&pipe_expr.function.get());
    ASSERT_NE(func2_id, nullptr);
    EXPECT_EQ(func2_id->name, "func2");
    
    // The value should be another pipeline expression (value |> func1)
    auto* inner_pipeline = boost::get<boost::spirit::x3::forward_ast<ast::pipeline_expression>>(&pipe_expr.value.get());
    ASSERT_NE(inner_pipeline, nullptr);
    
    const auto& inner_pipe = inner_pipeline->get();
    
    // Check the inner pipeline
    auto* value_id = boost::get<ast::identifier>(&inner_pipe.value.get());
    ASSERT_NE(value_id, nullptr);
    EXPECT_EQ(value_id->name, "value");
    
    auto* func1_id = boost::get<ast::identifier>(&inner_pipe.function.get());
    ASSERT_NE(func1_id, nullptr);
    EXPECT_EQ(func1_id->name, "func1");
}

TEST_F(PipelineOperatorTest, ParsePipelineWithFunctionCall) {
    std::string input = "value |> transform(42)";
    
    auto tokens = tokenize(input);
    ASSERT_TRUE(tokens.has_value());
    
    Parser parser(*tokens);
    auto result = parser.parse_expression();
    ASSERT_TRUE(result.has_value());
    
    // Check that we got a pipeline expression
    auto* pipeline = boost::get<boost::spirit::x3::forward_ast<ast::pipeline_expression>>(&result->get());
    ASSERT_NE(pipeline, nullptr);
    
    const auto& pipe_expr = pipeline->get();
    
    // Check the value side
    auto* value_id = boost::get<ast::identifier>(&pipe_expr.value.get());
    ASSERT_NE(value_id, nullptr);
    EXPECT_EQ(value_id->name, "value");
    
    // Check the function side (should be a function call)
    auto* func_call = boost::get<boost::spirit::x3::forward_ast<ast::function_call>>(&pipe_expr.function.get());
    ASSERT_NE(func_call, nullptr);
    
    const auto& call = func_call->get();
    EXPECT_EQ(call.function_name.name, "transform");
    EXPECT_EQ(call.arguments.size(), 1);
}

TEST_F(PipelineOperatorTest, ParsePipelineWithLambda) {
    std::string input = "value |> { x => x + 1 }";
    
    auto tokens = tokenize(input);
    ASSERT_TRUE(tokens.has_value());
    
    Parser parser(*tokens);
    auto result = parser.parse_expression();
    ASSERT_TRUE(result.has_value());
    
    // Check that we got a pipeline expression
    auto* pipeline = boost::get<boost::spirit::x3::forward_ast<ast::pipeline_expression>>(&result->get());
    ASSERT_NE(pipeline, nullptr);
    
    const auto& pipe_expr = pipeline->get();
    
    // Check the value side
    auto* value_id = boost::get<ast::identifier>(&pipe_expr.value.get());
    ASSERT_NE(value_id, nullptr);
    EXPECT_EQ(value_id->name, "value");
    
    // Check the function side (should be a lambda)
    auto* lambda = boost::get<boost::spirit::x3::forward_ast<ast::lambda_expression>>(&pipe_expr.function.get());
    ASSERT_NE(lambda, nullptr);
    
    const auto& lambda_expr = lambda->get();
    EXPECT_EQ(lambda_expr.parameters.size(), 1);
    EXPECT_EQ(lambda_expr.parameters[0].name.name, "x");
}

TEST_F(PipelineOperatorTest, ParseComplexPipelineChain) {
    std::string input = "data |> filter(isValid) |> map(transform) |> reduce(combine)";
    
    auto tokens = tokenize(input);
    ASSERT_TRUE(tokens.has_value());
    
    Parser parser(*tokens);
    auto result = parser.parse_expression();
    ASSERT_TRUE(result.has_value());
    
    // Should parse successfully as a series of nested pipeline expressions
    auto* pipeline = boost::get<boost::spirit::x3::forward_ast<ast::pipeline_expression>>(&result->get());
    ASSERT_NE(pipeline, nullptr);
    
    // The outermost function should be reduce(combine)
    const auto& outer_pipe = pipeline->get();
    auto* reduce_call = boost::get<boost::spirit::x3::forward_ast<ast::function_call>>(&outer_pipe.function.get());
    ASSERT_NE(reduce_call, nullptr);
    EXPECT_EQ(reduce_call->get().function_name.name, "reduce");
}

TEST_F(PipelineOperatorTest, PipelineOperatorPrecedence) {
    // Test that pipeline has lower precedence than function calls
    std::string input = "getValue() |> process";
    
    auto tokens = tokenize(input);
    ASSERT_TRUE(tokens.has_value());
    
    Parser parser(*tokens);
    auto result = parser.parse_expression();
    ASSERT_TRUE(result.has_value());
    
    // Should parse as (getValue()) |> process, not getValue() |> (process)
    auto* pipeline = boost::get<boost::spirit::x3::forward_ast<ast::pipeline_expression>>(&result->get());
    ASSERT_NE(pipeline, nullptr);
    
    const auto& pipe_expr = pipeline->get();
    
    // The value side should be a function call
    auto* func_call = boost::get<boost::spirit::x3::forward_ast<ast::function_call>>(&pipe_expr.value.get());
    ASSERT_NE(func_call, nullptr);
    EXPECT_EQ(func_call->get().function_name.name, "getValue");
    
    // The function side should be an identifier
    auto* func_id = boost::get<ast::identifier>(&pipe_expr.function.get());
    ASSERT_NE(func_id, nullptr);
    EXPECT_EQ(func_id->name, "process");
}
