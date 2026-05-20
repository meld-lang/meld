#include <gtest/gtest.h>
#include "meld/cli/interpreter_module.hpp"
#include "meld/testing/property_test.hpp"
#include <random>
#include <algorithm>

using namespace meld::cli;
using namespace meld::testing;

/**
 * **Feature: meld-cli, Property 8: REPL State Persistence**
 * **Validates: Requirements 3.3**
 * 
 * Property: For any sequence of variable or function definitions in the REPL, 
 * subsequent references should access the most recently defined values
 */
class ReplStatePersistenceTest : public PropertyTest {
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

// Generator for variable definitions
class VariableDefinitionGenerator {
public:
    std::pair<std::string, std::string> generate(std::mt19937& rng) {
        std::uniform_int_distribution<int> type_dist(0, 3);
        int type = type_dist(rng);
        
        switch (type) {
            case 0:
                return generate_integer_variable(rng);
            case 1:
                return generate_string_variable(rng);
            case 2:
                return generate_boolean_variable(rng);
            case 3:
                return generate_expression_variable(rng);
            default:
                return generate_integer_variable(rng);
        }
    }
    
private:
    std::pair<std::string, std::string> generate_integer_variable(std::mt19937& rng) {
        std::uniform_int_distribution<int> name_dist(1, 100);
        std::uniform_int_distribution<int> value_dist(-1000, 1000);
        
        std::string name = "intVar" + std::to_string(name_dist(rng));
        std::string value = std::to_string(value_dist(rng));
        
        return {name, value};
    }
    
    std::pair<std::string, std::string> generate_string_variable(std::mt19937& rng) {
        std::uniform_int_distribution<int> name_dist(1, 100);
        std::vector<std::string> values = {
            "\"hello\"", "\"world\"", "\"test\"", "\"value\"", "\"data\"",
            "\"persistent\"", "\"state\"", "\"variable\""
        };
        std::uniform_int_distribution<size_t> value_dist(0, values.size() - 1);
        
        std::string name = "strVar" + std::to_string(name_dist(rng));
        std::string value = values[value_dist(rng)];
        
        return {name, value};
    }
    
    std::pair<std::string, std::string> generate_boolean_variable(std::mt19937& rng) {
        std::uniform_int_distribution<int> name_dist(1, 100);
        std::string name = "boolVar" + std::to_string(name_dist(rng));
        std::string value = (rng() % 2 == 0) ? "true" : "false";
        
        return {name, value};
    }
    
    std::pair<std::string, std::string> generate_expression_variable(std::mt19937& rng) {
        std::uniform_int_distribution<int> name_dist(1, 100);
        std::uniform_int_distribution<int> num_dist(1, 50);
        
        std::string name = "exprVar" + std::to_string(name_dist(rng));
        
        int a = num_dist(rng);
        int b = num_dist(rng);
        std::vector<std::string> operators = {"+", "-", "*"};
        std::uniform_int_distribution<size_t> op_dist(0, operators.size() - 1);
        std::string op = operators[op_dist(rng)];
        
        std::string value = std::to_string(a) + " " + op + " " + std::to_string(b);
        
        return {name, value};
    }
};

// Generator for sequences of variable operations
class VariableSequenceGenerator {
public:
    std::vector<std::pair<std::string, std::string>> generate(std::mt19937& rng) {
        std::uniform_int_distribution<size_t> size_dist(2, 10);
        size_t sequence_length = size_dist(rng);
        
        std::vector<std::pair<std::string, std::string>> sequence;
        VariableDefinitionGenerator var_gen;
        
        for (size_t i = 0; i < sequence_length; ++i) {
            sequence.push_back(var_gen.generate(rng));
        }
        
        return sequence;
    }
};

TEST_F(ReplStatePersistenceTest, VariablesPersistAcrossEvaluations) {
    auto var_gen = std::make_unique<VariableDefinitionGenerator>();
    
    property_test("Variable persistence across evaluations", 100, [&](std::mt19937& rng) {
        // Generate a variable definition
        auto [var_name, var_value] = var_gen->generate(rng);
        
        // Define the variable
        std::string assignment = var_name + " = " + var_value;
        ReplResult assign_result = session_->evaluate_line(assignment);
        
        EXPECT_TRUE(assign_result.success)
            << "Variable assignment should succeed\n"
            << "Assignment: " << assignment;
        
        if (assign_result.success) {
            // Access the variable in a subsequent evaluation
            ReplResult access_result = session_->evaluate_line(var_name);
            
            EXPECT_TRUE(access_result.success)
                << "Variable access should succeed\n"
                << "Variable: " << var_name;
            
            if (access_result.success) {
                // The value should be preserved
                EXPECT_EQ(access_result.value, var_value)
                    << "Variable value should persist\n"
                    << "Variable: " << var_name << "\n"
                    << "Expected: " << var_value << "\n"
                    << "Got: " << access_result.value;
                
                // Access the variable again - should still be there
                ReplResult second_access = session_->evaluate_line(var_name);
                
                EXPECT_TRUE(second_access.success)
                    << "Variable should remain accessible";
                
                EXPECT_EQ(second_access.value, var_value)
                    << "Variable value should remain consistent";
            }
        }
        
    });
}

TEST_F(ReplStatePersistenceTest, VariableRedefinitionUpdatesState) {
    property_test("Variable redefinition updates state", 50, [&](std::mt19937& rng) {
        std::uniform_int_distribution<int> name_dist(1, 50);
        std::uniform_int_distribution<int> value_dist(1, 1000);
        
        std::string var_name = "redefinedVar" + std::to_string(name_dist(rng));
        int first_value = value_dist(rng);
        int second_value = value_dist(rng);
        
        // Ensure the values are different
        while (second_value == first_value) {
            second_value = value_dist(rng);
        }
        
        // Define the variable with first value
        std::string first_assignment = var_name + " = " + std::to_string(first_value);
        ReplResult first_assign = session_->evaluate_line(first_assignment);
        
        EXPECT_TRUE(first_assign.success)
            << "First assignment should succeed";
        
        // Verify first value
        ReplResult first_access = session_->evaluate_line(var_name);
        EXPECT_TRUE(first_access.success);
        EXPECT_EQ(first_access.value, std::to_string(first_value));
        
        // Redefine the variable with second value
        std::string second_assignment = var_name + " = " + std::to_string(second_value);
        ReplResult second_assign = session_->evaluate_line(second_assignment);
        
        EXPECT_TRUE(second_assign.success)
            << "Second assignment should succeed";
        
        // Verify the variable now has the second value
        ReplResult second_access = session_->evaluate_line(var_name);
        
        EXPECT_TRUE(second_access.success)
            << "Variable access after redefinition should succeed";
        
        EXPECT_EQ(second_access.value, std::to_string(second_value))
            << "Variable should have the most recent value\n"
            << "Variable: " << var_name << "\n"
            << "Expected: " << second_value << "\n"
            << "Got: " << second_access.value;
        
        EXPECT_NE(second_access.value, std::to_string(first_value))
            << "Variable should not have the old value";
        
        return true;
    });
}

TEST_F(ReplStatePersistenceTest, MultipleVariablesPersistIndependently) {
    auto seq_gen = std::make_unique<VariableSequenceGenerator>();
    
    property_test("Multiple variables persist independently", 50, [&](std::mt19937& rng) {
        // Generate a sequence of variable definitions
        auto variable_sequence = seq_gen->generate(rng);
        
        // Define all variables
        std::map<std::string, std::string> expected_values;
        for (const auto& [var_name, var_value] : variable_sequence) {
            std::string assignment = var_name + " = " + var_value;
            ReplResult result = session_->evaluate_line(assignment);
            
            EXPECT_TRUE(result.success)
                << "Assignment should succeed: " << assignment;
            
            // Store the expected value (most recent for each variable)
            expected_values[var_name] = var_value;
        }
        
        // Verify all variables are accessible with correct values
        for (const auto& [var_name, expected_value] : expected_values) {
            ReplResult access_result = session_->evaluate_line(var_name);
            
            EXPECT_TRUE(access_result.success)
                << "Variable should be accessible: " << var_name;
            
            if (access_result.success) {
                EXPECT_EQ(access_result.value, expected_value)
                    << "Variable should have correct value\n"
                    << "Variable: " << var_name << "\n"
                    << "Expected: " << expected_value << "\n"
                    << "Got: " << access_result.value;
            }
        }
        
        return true;
    });
}

TEST_F(ReplStatePersistenceTest, VariablesUsedInExpressions) {
    property_test("Variables used in expressions", 40, [&](std::mt19937& rng) {
        std::uniform_int_distribution<int> value_dist(1, 50);
        
        int a_value = value_dist(rng);
        int b_value = value_dist(rng);
        
        // Define two variables
        std::string a_assignment = "a = " + std::to_string(a_value);
        std::string b_assignment = "b = " + std::to_string(b_value);
        
        ReplResult a_result = session_->evaluate_line(a_assignment);
        ReplResult b_result = session_->evaluate_line(b_assignment);
        
        EXPECT_TRUE(a_result.success && b_result.success)
            << "Variable assignments should succeed";
        
        if (a_result.success && b_result.success) {
            // Use variables in an expression
            std::vector<std::string> operations = {"+", "-", "*"};
            std::uniform_int_distribution<size_t> op_dist(0, operations.size() - 1);
            std::string op = operations[op_dist(rng)];
            
            std::string expression = "a " + op + " b";
            ReplResult expr_result = session_->evaluate_line(expression);
            
            EXPECT_TRUE(expr_result.success)
                << "Expression using variables should succeed\n"
                << "Expression: " << expression;
            
            if (expr_result.success) {
                // Calculate expected result
                int expected;
                if (op == "+") {
                    expected = a_value + b_value;
                } else if (op == "-") {
                    expected = a_value - b_value;
                } else if (op == "*") {
                    expected = a_value * b_value;
                } else {
                    expected = 0;
                }
                
                EXPECT_EQ(expr_result.value, std::to_string(expected))
                    << "Expression result should be correct\n"
                    << "Expression: " << expression << "\n"
                    << "a = " << a_value << ", b = " << b_value << "\n"
                    << "Expected: " << expected << "\n"
                    << "Got: " << expr_result.value;
            }
        }
        
        return true;
    });
}

TEST_F(ReplStatePersistenceTest, StatePreservedAfterErrors) {
    property_test("State preserved after errors", 30, [&](std::mt19937& rng) {
        std::uniform_int_distribution<int> value_dist(1, 100);
        int valid_value = value_dist(rng);
        
        // Define a valid variable
        std::string valid_assignment = "validVar = " + std::to_string(valid_value);
        ReplResult valid_result = session_->evaluate_line(valid_assignment);
        
        EXPECT_TRUE(valid_result.success)
            << "Valid assignment should succeed";
        
        // Try an invalid operation that should cause an error
        std::vector<std::string> invalid_operations = {
            "invalidVar",  // Undefined variable
            "1 / 0",       // Division by zero
            "validVar.nonexistent",  // Invalid property access
            "undefinedFunction()",   // Undefined function call
        };
        
        std::uniform_int_distribution<size_t> invalid_dist(0, invalid_operations.size() - 1);
        std::string invalid_op = invalid_operations[invalid_dist(rng)];
        
        ReplResult invalid_result = session_->evaluate_line(invalid_op);
        
        // The invalid operation should fail (or at least not crash)
        // We don't require it to fail since some might be handled gracefully
        
        // But the valid variable should still be accessible
        ReplResult access_after_error = session_->evaluate_line("validVar");
        
        EXPECT_TRUE(access_after_error.success)
            << "Valid variable should still be accessible after error\n"
            << "Invalid operation: " << invalid_op;
        
        if (access_after_error.success) {
            EXPECT_EQ(access_after_error.value, std::to_string(valid_value))
                << "Variable value should be preserved after error\n"
                << "Expected: " << valid_value << "\n"
                << "Got: " << access_after_error.value;
        }
        
        return true;
    });
}

TEST_F(ReplStatePersistenceTest, EnvironmentVariablesPersist) {
    property_test("Environment variables persist", 25, [&](std::mt19937& rng) {
        std::uniform_int_distribution<int> name_dist(1, 50);
        std::uniform_int_distribution<int> value_dist(1, 1000);
        
        std::string env_name = "TEST_ENV_" + std::to_string(name_dist(rng));
        std::string env_value = "value_" + std::to_string(value_dist(rng));
        
        // Set environment variable
        session_->set_environment(env_name, env_value);
        
        // Verify it's in the environment
        const auto& env = session_->get_environment();
        auto it = env.find(env_name);
        
        EXPECT_NE(it, env.end())
            << "Environment variable should be set\n"
            << "Variable: " << env_name;
        
        if (it != env.end()) {
            EXPECT_EQ(it->second, env_value)
                << "Environment variable should have correct value\n"
                << "Variable: " << env_name << "\n"
                << "Expected: " << env_value << "\n"
                << "Got: " << it->second;
        }
        
        // Set another environment variable
        std::string env_name2 = "TEST_ENV2_" + std::to_string(name_dist(rng));
        std::string env_value2 = "value2_" + std::to_string(value_dist(rng));
        
        session_->set_environment(env_name2, env_value2);
        
        // Both should be present
        const auto& env_after = session_->get_environment();
        
        EXPECT_NE(env_after.find(env_name), env_after.end())
            << "First environment variable should still be present";
        
        EXPECT_NE(env_after.find(env_name2), env_after.end())
            << "Second environment variable should be present";
        
        return true;
    });
}