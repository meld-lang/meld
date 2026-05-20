#include <gtest/gtest.h>
#include "meld/kernel/operators.hpp"
#include "meld/kernel/primitives.hpp"
#include <algorithm>

using namespace meld::kernel;

class OperatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Registry is a singleton, already initialized
    }
};

// Test arithmetic operators
TEST_F(OperatorTest, AdditionOperator) {
    auto& registry = OperatorRegistry::instance();
    
    std::vector<Value> args = {
        Value(std::make_shared<Integer>(5)),
        Value(std::make_shared<Integer>(3))
    };
    
    auto result = registry.dispatch_operator("+", args);
    ASSERT_TRUE(result.has_value());
    
    auto int_result = result->try_as<Integer>();
    ASSERT_TRUE(int_result.has_value());
    EXPECT_EQ((*int_result)->value(), 8);
}

TEST_F(OperatorTest, SubtractionOperator) {
    auto& registry = OperatorRegistry::instance();
    
    std::vector<Value> args = {
        Value(std::make_shared<Integer>(10)),
        Value(std::make_shared<Integer>(3))
    };
    
    auto result = registry.dispatch_operator("-", args);
    ASSERT_TRUE(result.has_value());
    
    auto int_result = result->try_as<Integer>();
    ASSERT_TRUE(int_result.has_value());
    EXPECT_EQ((*int_result)->value(), 7);
}

TEST_F(OperatorTest, MultiplicationOperator) {
    auto& registry = OperatorRegistry::instance();
    
    std::vector<Value> args = {
        Value(std::make_shared<Integer>(4)),
        Value(std::make_shared<Integer>(5))
    };
    
    auto result = registry.dispatch_operator("*", args);
    ASSERT_TRUE(result.has_value());
    
    auto int_result = result->try_as<Integer>();
    ASSERT_TRUE(int_result.has_value());
    EXPECT_EQ((*int_result)->value(), 20);
}

TEST_F(OperatorTest, DivisionOperator) {
    auto& registry = OperatorRegistry::instance();
    
    std::vector<Value> args = {
        Value(std::make_shared<Integer>(20)),
        Value(std::make_shared<Integer>(4))
    };
    
    auto result = registry.dispatch_operator("/", args);
    ASSERT_TRUE(result.has_value());
    
    auto int_result = result->try_as<Integer>();
    ASSERT_TRUE(int_result.has_value());
    EXPECT_EQ((*int_result)->value(), 5);
}

TEST_F(OperatorTest, DivisionByZero) {
    auto& registry = OperatorRegistry::instance();
    
    std::vector<Value> args = {
        Value(std::make_shared<Integer>(10)),
        Value(std::make_shared<Integer>(0))
    };
    
    auto result = registry.dispatch_operator("/", args);
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("Division by zero"), std::string::npos);
}

TEST_F(OperatorTest, ModuloOperator) {
    auto& registry = OperatorRegistry::instance();
    
    std::vector<Value> args = {
        Value(std::make_shared<Integer>(17)),
        Value(std::make_shared<Integer>(5))
    };
    
    auto result = registry.dispatch_operator("%", args);
    ASSERT_TRUE(result.has_value());
    
    auto int_result = result->try_as<Integer>();
    ASSERT_TRUE(int_result.has_value());
    EXPECT_EQ((*int_result)->value(), 2);
}

TEST_F(OperatorTest, NegationOperator) {
    auto& registry = OperatorRegistry::instance();
    
    std::vector<Value> args = {
        Value(std::make_shared<Integer>(42))
    };
    
    auto result = registry.dispatch_operator("-", args);
    ASSERT_TRUE(result.has_value());
    
    auto int_result = result->try_as<Integer>();
    ASSERT_TRUE(int_result.has_value());
    EXPECT_EQ((*int_result)->value(), -42);
}

// Test comparison operators
TEST_F(OperatorTest, LessThanOperator) {
    auto& registry = OperatorRegistry::instance();
    
    std::vector<Value> args = {
        Value(std::make_shared<Integer>(3)),
        Value(std::make_shared<Integer>(5))
    };
    
    auto result = registry.dispatch_operator("<", args);
    ASSERT_TRUE(result.has_value());
    
    auto bool_result = result->try_as<Boolean>();
    ASSERT_TRUE(bool_result.has_value());
    EXPECT_TRUE((*bool_result)->value());
}

TEST_F(OperatorTest, GreaterThanOperator) {
    auto& registry = OperatorRegistry::instance();
    
    std::vector<Value> args = {
        Value(std::make_shared<Integer>(10)),
        Value(std::make_shared<Integer>(5))
    };
    
    auto result = registry.dispatch_operator(">", args);
    ASSERT_TRUE(result.has_value());
    
    auto bool_result = result->try_as<Boolean>();
    ASSERT_TRUE(bool_result.has_value());
    EXPECT_TRUE((*bool_result)->value());
}

TEST_F(OperatorTest, EqualsOperator) {
    auto& registry = OperatorRegistry::instance();
    
    std::vector<Value> args = {
        Value(std::make_shared<Integer>(42)),
        Value(std::make_shared<Integer>(42))
    };
    
    auto result = registry.dispatch_operator("==", args);
    ASSERT_TRUE(result.has_value());
    
    auto bool_result = result->try_as<Boolean>();
    ASSERT_TRUE(bool_result.has_value());
    EXPECT_TRUE((*bool_result)->value());
}

TEST_F(OperatorTest, NotEqualsOperator) {
    auto& registry = OperatorRegistry::instance();
    
    std::vector<Value> args = {
        Value(std::make_shared<Integer>(42)),
        Value(std::make_shared<Integer>(43))
    };
    
    auto result = registry.dispatch_operator("!=", args);
    ASSERT_TRUE(result.has_value());
    
    auto bool_result = result->try_as<Boolean>();
    ASSERT_TRUE(bool_result.has_value());
    EXPECT_TRUE((*bool_result)->value());
}

// Test logical operators
TEST_F(OperatorTest, LogicalAndOperator) {
    auto& registry = OperatorRegistry::instance();
    
    std::vector<Value> args = {
        Value(Boolean::from(true)),
        Value(Boolean::from(false))
    };
    
    auto result = registry.dispatch_operator("&&", args);
    ASSERT_TRUE(result.has_value());
    
    auto bool_result = result->try_as<Boolean>();
    ASSERT_TRUE(bool_result.has_value());
    EXPECT_FALSE((*bool_result)->value());
}

TEST_F(OperatorTest, LogicalOrOperator) {
    auto& registry = OperatorRegistry::instance();
    
    std::vector<Value> args = {
        Value(Boolean::from(true)),
        Value(Boolean::from(false))
    };
    
    auto result = registry.dispatch_operator("||", args);
    ASSERT_TRUE(result.has_value());
    
    auto bool_result = result->try_as<Boolean>();
    ASSERT_TRUE(bool_result.has_value());
    EXPECT_TRUE((*bool_result)->value());
}

TEST_F(OperatorTest, LogicalNotOperator) {
    auto& registry = OperatorRegistry::instance();
    
    std::vector<Value> args = {
        Value(Boolean::from(true))
    };
    
    auto result = registry.dispatch_operator("!", args);
    ASSERT_TRUE(result.has_value());
    
    auto bool_result = result->try_as<Boolean>();
    ASSERT_TRUE(bool_result.has_value());
    EXPECT_FALSE((*bool_result)->value());
}

// Test operator overloading
TEST_F(OperatorTest, OperatorOverload) {
    auto& registry = OperatorRegistry::instance();
    
    // Register a custom overload for + on String type
    registry.register_operator_overload("+", "String", 
        [](const std::vector<Value>& args) -> std::expected<Value, std::string> {
            if (args.size() != 2) {
                return std::unexpected("String + requires 2 operands");
            }
            
            auto a = args[0].try_as<String>();
            auto b = args[1].try_as<String>();
            
            if (!a || !b) {
                return std::unexpected("String + requires string operands");
            }
            
            return Value(std::make_shared<String>((*a)->value() + (*b)->value()));
        }
    );
    
    std::vector<Value> args = {
        Value(std::make_shared<String>("Hello, ")),
        Value(std::make_shared<String>("World!"))
    };
    
    auto result = registry.dispatch_operator("+", args);
    ASSERT_TRUE(result.has_value());
    
    auto str_result = result->try_as<String>();
    ASSERT_TRUE(str_result.has_value());
    EXPECT_EQ((*str_result)->value(), "Hello, World!");
}

// Test multiple operator overloads for same operator
TEST_F(OperatorTest, MultipleOverloadsForSameOperator) {
    auto& registry = OperatorRegistry::instance();
    
    // Register overload for + on String
    registry.register_operator_overload("+", "String", 
        [](const std::vector<Value>& args) -> std::expected<Value, std::string> {
            auto a = args[0].try_as<String>();
            auto b = args[1].try_as<String>();
            return Value(std::make_shared<String>((*a)->value() + (*b)->value()));
        }
    );
    
    // Integer + should still work with built-in implementation
    std::vector<Value> int_args = {
        Value(std::make_shared<Integer>(5)),
        Value(std::make_shared<Integer>(3))
    };
    
    auto int_result = registry.dispatch_operator("+", int_args);
    ASSERT_TRUE(int_result.has_value());
    auto int_val = int_result->try_as<Integer>();
    ASSERT_TRUE(int_val.has_value());
    EXPECT_EQ((*int_val)->value(), 8);
    
    // String + should use the overload
    std::vector<Value> str_args = {
        Value(std::make_shared<String>("Hello")),
        Value(std::make_shared<String>("World"))
    };
    
    auto str_result = registry.dispatch_operator("+", str_args);
    ASSERT_TRUE(str_result.has_value());
    auto str_val = str_result->try_as<String>();
    ASSERT_TRUE(str_val.has_value());
    EXPECT_EQ((*str_val)->value(), "HelloWorld");
}

// Test operator overload for unary operator
TEST_F(OperatorTest, UnaryOperatorOverload) {
    auto& registry = OperatorRegistry::instance();
    
    // Register overload for unary - on String (silly example, but tests the concept)
    registry.register_operator_overload("-", "String", 
        [](const std::vector<Value>& args) -> std::expected<Value, std::string> {
            if (args.size() != 1) {
                return std::unexpected("Unary - on String requires 1 operand");
            }
            
            auto a = args[0].try_as<String>();
            if (!a) {
                return std::unexpected("Unary - requires string operand");
            }
            
            // Reverse the string
            std::string reversed = (*a)->value();
            std::reverse(reversed.begin(), reversed.end());
            return Value(std::make_shared<String>(reversed));
        }
    );
    
    std::vector<Value> args = {
        Value(std::make_shared<String>("hello"))
    };
    
    auto result = registry.dispatch_operator("-", args);
    ASSERT_TRUE(result.has_value());
    
    auto str_result = result->try_as<String>();
    ASSERT_TRUE(str_result.has_value());
    EXPECT_EQ((*str_result)->value(), "olleh");
}

// Test operator info retrieval
TEST_F(OperatorTest, GetOperatorInfo) {
    auto& registry = OperatorRegistry::instance();
    
    auto info = registry.get_operator_info("+");
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->symbol, "+");
    EXPECT_EQ(info->type, OperatorType::INFIX);
    EXPECT_EQ(info->precedence, 60);
    EXPECT_EQ(info->associativity, Associativity::LEFT);
}

TEST_F(OperatorTest, HasOperator) {
    auto& registry = OperatorRegistry::instance();
    
    EXPECT_TRUE(registry.has_operator("+"));
    EXPECT_TRUE(registry.has_operator("-"));
    EXPECT_TRUE(registry.has_operator("*"));
    EXPECT_FALSE(registry.has_operator("@@@"));
}

TEST_F(OperatorTest, GetAllOperators) {
    auto& registry = OperatorRegistry::instance();
    
    auto operators = registry.get_all_operators();
    EXPECT_GT(operators.size(), 0);
    
    // Check that we have some expected operators
    bool has_plus = false;
    bool has_multiply = false;
    
    for (const auto& op : operators) {
        if (op.symbol == "+") has_plus = true;
        if (op.symbol == "*") has_multiply = true;
    }
    
    EXPECT_TRUE(has_plus);
    EXPECT_TRUE(has_multiply);
}
