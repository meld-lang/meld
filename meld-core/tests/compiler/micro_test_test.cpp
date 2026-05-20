#include <gtest/gtest.h>
#include "meld/compiler/micro_test_engine.hpp"
#include "meld/parser/parser.hpp"
#include "meld/kernel/primitives.hpp"

using namespace meld::compiler;
using namespace meld::parser;
using namespace meld::parser::ast;
using namespace meld::kernel;

class MicroTestEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = std::make_unique<MicroTestEngine>();
    }
    
    std::unique_ptr<MicroTestEngine> engine;
};

TEST_F(MicroTestEngineTest, ParseFunctionWithTestBlock) {
    std::string input = R"(
        fn add(a: int, b: int) -> int {
            test "addition works correctly" {
                assert a + b == 5, "Expected sum to be 5"
            }
            
            return a + b
        }
    )";
    
    Parser parser;
    ast::expression result;
    
    ASSERT_TRUE(parser.parse_expression(input, result));
    
    auto func_def_fwd = boost::get<boost::spirit::x3::forward_ast<ast::function_definition>>(&result);
    ASSERT_NE(func_def_fwd, nullptr);
    auto* func_def = &func_def_fwd->get();
    
    EXPECT_TRUE(func_def->has_tests);
    EXPECT_EQ(func_def->tests.size(), 1);
    
    const auto& test_block = func_def->tests[0];
    EXPECT_TRUE(test_block.has_description);
    EXPECT_EQ(test_block.description, "addition works correctly");
    EXPECT_FALSE(test_block.is_property_test);
}

TEST_F(MicroTestEngineTest, ParseFunctionWithMultipleTestBlocks) {
    std::string input = R"(
        fn multiply(a: int, b: int) -> int {
            test "positive numbers" {
                assert a * b > 0
            }
            
            test "commutative property" {
                assert a * b == b * a
            }
            
            return a * b
        }
    )";
    
    Parser parser;
    ast::expression result;
    
    ASSERT_TRUE(parser.parse_expression(input, result));
    
    auto func_def_fwd = boost::get<boost::spirit::x3::forward_ast<ast::function_definition>>(&result);
    ASSERT_NE(func_def_fwd, nullptr);
    auto* func_def = &func_def_fwd->get();
    
    EXPECT_TRUE(func_def->has_tests);
    EXPECT_EQ(func_def->tests.size(), 2);
    
    EXPECT_TRUE(func_def->tests[0].has_description);
    EXPECT_EQ(func_def->tests[0].description, "positive numbers");
    
    EXPECT_TRUE(func_def->tests[1].has_description);
    EXPECT_EQ(func_def->tests[1].description, "commutative property");
}

TEST_F(MicroTestEngineTest, ParseAssertionExpression) {
    std::string input = "assert x == 42";
    
    Parser parser;
    ast::expression result;
    
    ASSERT_TRUE(parser.parse_expression(input, result));
    
    auto assertion_fwd = boost::get<boost::spirit::x3::forward_ast<ast::assertion_expression>>(&result);
    ASSERT_NE(assertion_fwd, nullptr);
    auto* assertion = &assertion_fwd->get();
    
    EXPECT_FALSE(assertion->has_message);
    
    // Check that the condition is a binary operation
    auto bin_op_fwd = boost::get<boost::spirit::x3::forward_ast<ast::binary_operation>>(&assertion->condition.get());
    ASSERT_NE(bin_op_fwd, nullptr);
    auto* bin_op = &bin_op_fwd->get();
    EXPECT_EQ(bin_op->op, "==");
}

TEST_F(MicroTestEngineTest, ParseAssertionWithMessage) {
    std::string input = R"(assert x > 0, "x should be positive")";
    
    Parser parser;
    ast::expression result;
    
    ASSERT_TRUE(parser.parse_expression(input, result));
    
    auto assertion_fwd = boost::get<boost::spirit::x3::forward_ast<ast::assertion_expression>>(&result);
    ASSERT_NE(assertion_fwd, nullptr);
    auto* assertion = &assertion_fwd->get();
    
    EXPECT_TRUE(assertion->has_message);
    EXPECT_EQ(assertion->message, "x should be positive");
}

TEST_F(MicroTestEngineTest, ExecuteSimpleAssertion) {
    // Create a simple assertion: assert true
    ast::assertion_expression assertion;
    assertion.condition = expression(ast::boolean_literal{true});
    assertion.has_message = false;
    
    AssertionResult result = engine->execute_assertion(assertion);
    
    EXPECT_TRUE(result.passed);
    EXPECT_EQ(result.message, "Assertion passed");
}

TEST_F(MicroTestEngineTest, ExecuteFailingAssertion) {
    // Create a failing assertion: assert false
    ast::assertion_expression assertion;
    assertion.condition = expression(ast::boolean_literal{false});
    assertion.has_message = false;
    
    AssertionResult result = engine->execute_assertion(assertion);
    
    EXPECT_FALSE(result.passed);
    EXPECT_NE(result.message, "Assertion passed");
}

TEST_F(MicroTestEngineTest, ExecuteAssertionWithCustomMessage) {
    // Create a failing assertion with custom message
    ast::assertion_expression assertion;
    assertion.condition = expression(ast::boolean_literal{false});
    assertion.message = "Custom failure message";
    assertion.has_message = true;
    
    AssertionResult result = engine->execute_assertion(assertion);
    
    EXPECT_FALSE(result.passed);
    EXPECT_EQ(result.message, "Custom failure message");
}

TEST_F(MicroTestEngineTest, TruthyValues) {
    EXPECT_TRUE(engine->is_truthy(Value(Boolean::from(true))));
    EXPECT_FALSE(engine->is_truthy(Value(Boolean::from(false))));
    
    EXPECT_FALSE(engine->is_truthy(Value(Empty::instance())));
    
    EXPECT_TRUE(engine->is_truthy(Value(std::make_shared<Integer>(1))));
    EXPECT_FALSE(engine->is_truthy(Value(std::make_shared<Integer>(0))));
    EXPECT_TRUE(engine->is_truthy(Value(std::make_shared<Integer>(-1))));
    
    EXPECT_TRUE(engine->is_truthy(Value(std::make_shared<String>("hello"))));
    EXPECT_FALSE(engine->is_truthy(Value(std::make_shared<String>(""))));
}

TEST_F(MicroTestEngineTest, ExpressionToString) {
    ast::identifier id;
    id.name = "variable";
    EXPECT_EQ(engine->expression_to_string(expression(id)), "variable");
    
    ast::integer_literal int_lit;
    int_lit.value = 42;
    EXPECT_EQ(engine->expression_to_string(expression(int_lit)), "42");
    
    ast::boolean_literal bool_lit;
    bool_lit.value = true;
    EXPECT_EQ(engine->expression_to_string(expression(bool_lit)), "true");
    
    bool_lit.value = false;
    EXPECT_EQ(engine->expression_to_string(expression(bool_lit)), "false");
}

TEST_F(MicroTestEngineTest, StatisticsTracking) {
    auto initial_stats = engine->get_statistics();
    EXPECT_EQ(initial_stats.total_assertions, 0);
    EXPECT_EQ(initial_stats.passed_assertions, 0);
    EXPECT_EQ(initial_stats.failed_assertions, 0);
    
    // Execute a passing assertion
    ast::assertion_expression passing_assertion;
    passing_assertion.condition = expression(ast::boolean_literal{true});
    engine->execute_assertion(passing_assertion);
    
    auto stats_after_pass = engine->get_statistics();
    EXPECT_EQ(stats_after_pass.total_assertions, 1);
    EXPECT_EQ(stats_after_pass.passed_assertions, 1);
    EXPECT_EQ(stats_after_pass.failed_assertions, 0);
    
    // Execute a failing assertion
    ast::assertion_expression failing_assertion;
    failing_assertion.condition = expression(ast::boolean_literal{false});
    engine->execute_assertion(failing_assertion);
    
    auto stats_after_fail = engine->get_statistics();
    EXPECT_EQ(stats_after_fail.total_assertions, 2);
    EXPECT_EQ(stats_after_fail.passed_assertions, 1);
    EXPECT_EQ(stats_after_fail.failed_assertions, 1);
}