#include <gtest/gtest.h>
#include "meld/cli/interpreter_module.hpp"
#include "meld/testing/property_test.hpp"
#include <random>
#include <sstream>

using namespace meld::cli;
using namespace meld::testing;

/**
 * **Feature: meld-cli, Property 7: REPL Expression Evaluation**
 * **Validates: Requirements 3.2**
 * 
 * Property: For any valid Meld expression entered in the REPL, it should 
 * evaluate to the same result as if executed in a regular program
 */
class ReplExpressionEvaluationTest : public PropertyTest {
protected:
    void SetUp() override {
        PropertyTest::SetUp();
        session_ = std::make_unique<ReplSession>();
    }
    
    void TearDown() override {
        PropertyTest::TearDown();
    }
    
    std::unique_ptr<ReplSession> session_;
};

// Generator for valid Meld expressions
class ExpressionGenerator  {
public:
    std::string generate(std::mt19937& rng) {
        std::uniform_int_distribution<int> expr_type_dist(0, 7);
        int expr_type = expr_type_dist(rng);
        
        switch (expr_type) {
            case 0:
                return generate_arithmetic_expression(rng);
            case 1:
                return generate_string_expression(rng);
            case 2:
                return generate_boolean_expression(rng);
            case 3:
                return generate_variable_assignment(rng);
            case 4:
                return generate_function_call(rng);
            case 5:
                return generate_literal_value(rng);
            case 6:
                return generate_comparison_expression(rng);
            case 7:
                return generate_conditional_expression(rng);
            default:
                return generate_arithmetic_expression(rng);
        }
    }
    
private:
    std::string generate_arithmetic_expression(std::mt19937& rng) {
        std::uniform_int_distribution<int> num_dist(1, 100);
        std::vector<std::string> operators = {"+", "-", "*"};
        std::uniform_int_distribution<size_t> op_dist(0, operators.size() - 1);
        
        int a = num_dist(rng);
        int b = num_dist(rng);
        std::string op = operators[op_dist(rng)];
        
        return std::to_string(a) + " " + op + " " + std::to_string(b);
    }
    
    std::string generate_string_expression(std::mt19937& rng) {
        std::vector<std::string> strings = {
            "\"hello\"", "\"world\"", "\"test\"", "\"REPL\"", "\"expression\""
        };
        std::uniform_int_distribution<size_t> str_dist(0, strings.size() - 1);
        
        std::string str1 = strings[str_dist(rng)];
        std::string str2 = strings[str_dist(rng)];
        
        std::vector<std::string> operations = {
            str1,  // Just a string literal
            str1 + " + " + str2,  // String concatenation
            "\"prefix_\" + " + str1  // Prefix concatenation
        };
        
        std::uniform_int_distribution<size_t> op_dist(0, operations.size() - 1);
        return operations[op_dist(rng)];
    }
    
    std::string generate_boolean_expression(std::mt19937& rng) {
        std::vector<std::string> expressions = {
            "true", "false",
            "true && false", "true || false",
            "!true", "!false"
        };
        
        std::uniform_int_distribution<size_t> expr_dist(0, expressions.size() - 1);
        return expressions[expr_dist(rng)];
    }
    
    std::string generate_variable_assignment(std::mt19937& rng) {
        std::uniform_int_distribution<int> var_num_dist(1, 10);
        std::uniform_int_distribution<int> value_dist(1, 1000);
        
        int var_num = var_num_dist(rng);
        int value = value_dist(rng);
        
        return "x" + std::to_string(var_num) + " = " + std::to_string(value);
    }
    
    std::string generate_function_call(std::mt19937& rng) {
        std::vector<std::string> functions = {
            "abs(-5)", "max(3, 7)", "min(10, 2)",
            "sqrt(16)", "pow(2, 3)"
        };
        
        std::uniform_int_distribution<size_t> func_dist(0, functions.size() - 1);
        return functions[func_dist(rng)];
    }
    
    std::string generate_literal_value(std::mt19937& rng) {
        std::uniform_int_distribution<int> type_dist(0, 3);
        int type = type_dist(rng);
        
        switch (type) {
            case 0: {
                std::uniform_int_distribution<int> int_dist(-100, 100);
                return std::to_string(int_dist(rng));
            }
            case 1: {
                std::uniform_real_distribution<double> float_dist(0.0, 100.0);
                return std::to_string(float_dist(rng));
            }
            case 2: {
                std::vector<std::string> strings = {"\"literal\"", "\"value\"", "\"test\""};
                std::uniform_int_distribution<size_t> str_dist(0, strings.size() - 1);
                return strings[str_dist(rng)];
            }
            case 3: {
                return rng() % 2 == 0 ? "true" : "false";
            }
            default:
                return "42";
        }
    }
    
    std::string generate_comparison_expression(std::mt19937& rng) {
        std::uniform_int_distribution<int> num_dist(1, 50);
        std::vector<std::string> operators = {"==", "!=", "<", ">", "<=", ">="};
        std::uniform_int_distribution<size_t> op_dist(0, operators.size() - 1);
        
        int a = num_dist(rng);
        int b = num_dist(rng);
        std::string op = operators[op_dist(rng)];
        
        return std::to_string(a) + " " + op + " " + std::to_string(b);
    }
    
    std::string generate_conditional_expression(std::mt19937& rng) {
        std::uniform_int_distribution<int> num_dist(1, 20);
        int condition_value = num_dist(rng);
        int true_value = num_dist(rng);
        int false_value = num_dist(rng);
        
        return std::to_string(condition_value) + " > 10 ? " + 
               std::to_string(true_value) + " : " + std::to_string(false_value);
    }
};

TEST_F(ReplExpressionEvaluationTest, ExpressionsEvaluateConsistently) {
    auto expr_gen = std::make_unique<ExpressionGenerator>();
    
    property_test("REPL expression evaluation consistency", 100, [&](std::mt19937& rng) {
        // Generate a valid expression
        std::string expression = expr_gen->generate(rng);
        
        // Evaluate the expression in REPL
        ReplResult result = session_->evaluate_line(expression);
        
        // Expression should evaluate successfully (for valid expressions)
        EXPECT_TRUE(result.success)
            << "Valid expression should evaluate successfully\n"
            << "Expression: " << expression << "\n"
            << "Errors: " << (result.has_errors() ? result.errors[0].message : "none");
        
        if (result.success) {
            // Result should have a value
            EXPECT_FALSE(result.value.empty())
                << "Successful evaluation should produce a value\n"
                << "Expression: " << expression;
            
            // Result should have a type
            EXPECT_FALSE(result.type.empty())
                << "Successful evaluation should have a type\n"
                << "Expression: " << expression;
            
            // Evaluate the same expression again - should get the same result
            ReplResult second_result = session_->evaluate_line(expression);
            
            EXPECT_TRUE(second_result.success)
                << "Re-evaluation should also succeed";
            
            EXPECT_EQ(result.value, second_result.value)
                << "Re-evaluation should produce the same value\n"
                << "Expression: " << expression << "\n"
                << "First result: " << result.value << "\n"
                << "Second result: " << second_result.value;
            
            EXPECT_EQ(result.type, second_result.type)
                << "Re-evaluation should produce the same type\n"
                << "Expression: " << expression;
        }
        
    });
}

TEST_F(ReplExpressionEvaluationTest, ArithmeticExpressionsEvaluateCorrectly) {
    property_test("Arithmetic expression evaluation", 50, [&](std::mt19937& rng) {
        std::uniform_int_distribution<int> num_dist(1, 50);
        std::vector<std::string> operators = {"+", "-", "*"};
        std::uniform_int_distribution<size_t> op_dist(0, operators.size() - 1);
        
        int a = num_dist(rng);
        int b = num_dist(rng);
        std::string op = operators[op_dist(rng)];
        
        std::string expression = std::to_string(a) + " " + op + " " + std::to_string(b);
        
        ReplResult result = session_->evaluate_line(expression);
        
        EXPECT_TRUE(result.success)
            << "Arithmetic expression should evaluate successfully\n"
            << "Expression: " << expression;
        
        if (result.success) {
            // Calculate expected result
            int expected;
            if (op == "+") {
                expected = a + b;
            } else if (op == "-") {
                expected = a - b;
            } else if (op == "*") {
                expected = a * b;
            } else {
                expected = 0; // Should not happen
            }
            
            // Check if the result matches expected value
            // Note: This is simplified - in a real implementation we'd parse the result
            std::string expected_str = std::to_string(expected);
            EXPECT_EQ(result.value, expected_str)
                << "Arithmetic result should be correct\n"
                << "Expression: " << expression << "\n"
                << "Expected: " << expected_str << "\n"
                << "Got: " << result.value;
        }
        
        return true;
    });
}

TEST_F(ReplExpressionEvaluationTest, VariableAssignmentAndRetrieval) {
    property_test("Variable assignment and retrieval", 50, [&](std::mt19937& rng) {
        std::uniform_int_distribution<int> var_num_dist(1, 20);
        std::uniform_int_distribution<int> value_dist(1, 1000);
        
        int var_num = var_num_dist(rng);
        int value = value_dist(rng);
        
        std::string var_name = "testVar" + std::to_string(var_num);
        std::string assignment = var_name + " = " + std::to_string(value);
        
        // Assign the variable
        ReplResult assign_result = session_->evaluate_line(assignment);
        
        EXPECT_TRUE(assign_result.success)
            << "Variable assignment should succeed\n"
            << "Assignment: " << assignment;
        
        if (assign_result.success) {
            // Retrieve the variable
            ReplResult retrieve_result = session_->evaluate_line(var_name);
            
            EXPECT_TRUE(retrieve_result.success)
                << "Variable retrieval should succeed\n"
                << "Variable: " << var_name;
            
            if (retrieve_result.success) {
                // The retrieved value should match the assigned value
                std::string expected_value = std::to_string(value);
                EXPECT_EQ(retrieve_result.value, expected_value)
                    << "Retrieved variable value should match assigned value\n"
                    << "Variable: " << var_name << "\n"
                    << "Expected: " << expected_value << "\n"
                    << "Got: " << retrieve_result.value;
            }
        }
        
        return true;
    });
}

TEST_F(ReplExpressionEvaluationTest, BooleanExpressionsEvaluateCorrectly) {
    property_test("Boolean expression evaluation", 40, [&](std::mt19937& rng) {
        std::vector<std::pair<std::string, std::string>> boolean_tests = {
            {"true", "true"},
            {"false", "false"},
            {"true && true", "true"},
            {"true && false", "false"},
            {"false && true", "false"},
            {"false && false", "false"},
            {"true || true", "true"},
            {"true || false", "true"},
            {"false || true", "true"},
            {"false || false", "false"},
            {"!true", "false"},
            {"!false", "true"}
        };
        
        std::uniform_int_distribution<size_t> test_dist(0, boolean_tests.size() - 1);
        auto [expression, expected] = boolean_tests[test_dist(rng)];
        
        ReplResult result = session_->evaluate_line(expression);
        
        EXPECT_TRUE(result.success)
            << "Boolean expression should evaluate successfully\n"
            << "Expression: " << expression;
        
        if (result.success) {
            EXPECT_EQ(result.value, expected)
                << "Boolean expression should evaluate correctly\n"
                << "Expression: " << expression << "\n"
                << "Expected: " << expected << "\n"
                << "Got: " << result.value;
        }
        
        return true;
    });
}

TEST_F(ReplExpressionEvaluationTest, ComparisonExpressionsEvaluateCorrectly) {
    property_test("Comparison expression evaluation", 60, [&](std::mt19937& rng) {
        std::uniform_int_distribution<int> num_dist(1, 20);
        std::vector<std::string> operators = {"==", "!=", "<", ">", "<=", ">="};
        std::uniform_int_distribution<size_t> op_dist(0, operators.size() - 1);
        
        int a = num_dist(rng);
        int b = num_dist(rng);
        std::string op = operators[op_dist(rng)];
        
        std::string expression = std::to_string(a) + " " + op + " " + std::to_string(b);
        
        ReplResult result = session_->evaluate_line(expression);
        
        EXPECT_TRUE(result.success)
            << "Comparison expression should evaluate successfully\n"
            << "Expression: " << expression;
        
        if (result.success) {
            // Calculate expected result
            bool expected;
            if (op == "==") {
                expected = (a == b);
            } else if (op == "!=") {
                expected = (a != b);
            } else if (op == "<") {
                expected = (a < b);
            } else if (op == ">") {
                expected = (a > b);
            } else if (op == "<=") {
                expected = (a <= b);
            } else if (op == ">=") {
                expected = (a >= b);
            } else {
                expected = false; // Should not happen
            }
            
            std::string expected_str = expected ? "true" : "false";
            EXPECT_EQ(result.value, expected_str)
                << "Comparison result should be correct\n"
                << "Expression: " << expression << "\n"
                << "Expected: " << expected_str << "\n"
                << "Got: " << result.value;
        }
        
        return true;
    });
}

TEST_F(ReplExpressionEvaluationTest, EmptyAndWhitespaceInputHandling) {
    property_test("Empty and whitespace input handling", 20, [&](std::mt19937& rng) {
        std::vector<std::string> empty_inputs = {
            "",
            " ",
            "  ",
            "\t",
            "   \t  ",
            "\n",
            " \n \t "
        };
        
        std::uniform_int_distribution<size_t> input_dist(0, empty_inputs.size() - 1);
        std::string input = empty_inputs[input_dist(rng)];
        
        ReplResult result = session_->evaluate_line(input);
        
        // Empty input should succeed but produce no value
        EXPECT_TRUE(result.success)
            << "Empty/whitespace input should be handled gracefully\n"
            << "Input: '" << input << "'";
        
        // Should not produce any meaningful output
        EXPECT_TRUE(result.value.empty())
            << "Empty input should not produce a value\n"
            << "Input: '" << input << "'\n"
            << "Got value: '" << result.value << "'";
        
        return true;
    });
}