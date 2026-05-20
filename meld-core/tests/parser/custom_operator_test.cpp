#include <gtest/gtest.h>
#include "meld/parser/parser.hpp"
#include "meld/parser/ast.hpp"

using namespace meld::parser;

class CustomOperatorTest : public ::testing::Test {
protected:
    Parser parser;
};

TEST_F(CustomOperatorTest, InfixOperatorDefinition) {
    std::string input = R"(
        infix func <|>(left: String, right: String): String precedence 100 associativity left {
            return left + " | " + right
        }
    )";
    
    ast::expression result;
    ASSERT_TRUE(parser.parse_expression(input, result));
    
    ASSERT_TRUE(std::holds_alternative<ast::custom_operator_definition>(result));
    auto& op_def = std::get<ast::custom_operator_definition>(result);
    
    EXPECT_EQ(op_def.op_type, ast::CustomOperatorType::INFIX);
    EXPECT_EQ(op_def.symbol, "<|>");
    EXPECT_EQ(op_def.precedence, 100);
    EXPECT_EQ(op_def.associativity, ast::CustomOperatorAssociativity::LEFT);
    EXPECT_EQ(op_def.parameters.size(), 2);
    EXPECT_EQ(op_def.parameters[0].name.name, "left");
    EXPECT_EQ(op_def.parameters[0].type.type_name.name, "String");
    EXPECT_EQ(op_def.parameters[1].name.name, "right");
    EXPECT_EQ(op_def.parameters[1].type.type_name.name, "String");
    EXPECT_TRUE(op_def.has_return_type);
    EXPECT_EQ(op_def.return_type.type_name.name, "String");
}

TEST_F(CustomOperatorTest, PrefixOperatorDefinition) {
    std::string input = R"(
        prefix func !(value: Bool): Bool {
            return not(value)
        }
    )";
    
    ast::expression result;
    ASSERT_TRUE(parser.parse_expression(input, result));
    
    ASSERT_TRUE(std::holds_alternative<ast::custom_operator_definition>(result));
    auto& op_def = std::get<ast::custom_operator_definition>(result);
    
    EXPECT_EQ(op_def.op_type, ast::CustomOperatorType::PREFIX);
    EXPECT_EQ(op_def.symbol, "!");
    EXPECT_EQ(op_def.parameters.size(), 1);
    EXPECT_EQ(op_def.parameters[0].name.name, "value");
    EXPECT_EQ(op_def.parameters[0].type.type_name.name, "Bool");
}

TEST_F(CustomOperatorTest, PostfixOperatorDefinition) {
    std::string input = R"(
        postfix func ++(value: Int): Int {
            return value + 1
        }
    )";
    
    ast::expression result;
    ASSERT_TRUE(parser.parse_expression(input, result));
    
    ASSERT_TRUE(std::holds_alternative<ast::custom_operator_definition>(result));
    auto& op_def = std::get<ast::custom_operator_definition>(result);
    
    EXPECT_EQ(op_def.op_type, ast::CustomOperatorType::POSTFIX);
    EXPECT_EQ(op_def.symbol, "++");
    EXPECT_EQ(op_def.parameters.size(), 1);
}

TEST_F(CustomOperatorTest, OperatorWithRightAssociativity) {
    std::string input = R"(
        infix func ^(base: Int, exp: Int): Int precedence 90 associativity right {
            return power(base, exp)
        }
    )";
    
    ast::expression result;
    ASSERT_TRUE(parser.parse_expression(input, result));
    
    ASSERT_TRUE(std::holds_alternative<ast::custom_operator_definition>(result));
    auto& op_def = std::get<ast::custom_operator_definition>(result);
    
    EXPECT_EQ(op_def.associativity, ast::CustomOperatorAssociativity::RIGHT);
    EXPECT_EQ(op_def.precedence, 90);
}

TEST_F(CustomOperatorTest, OperatorWithDefaultPrecedence) {
    std::string input = R"(
        infix func <=>(a: Int, b: Int): Int {
            return compare(a, b)
        }
    )";
    
    ast::expression result;
    ASSERT_TRUE(parser.parse_expression(input, result));
    
    ASSERT_TRUE(std::holds_alternative<ast::custom_operator_definition>(result));
    auto& op_def = std::get<ast::custom_operator_definition>(result);
    
    EXPECT_EQ(op_def.precedence, 50); // Default precedence
    EXPECT_EQ(op_def.associativity, ast::CustomOperatorAssociativity::LEFT); // Default associativity
}

TEST_F(CustomOperatorTest, OperatorWithNonAssociative) {
    std::string input = R"(
        infix func <->(a: Int, b: Int): Bool precedence 40 associativity none {
            return a == b
        }
    )";
    
    ast::expression result;
    ASSERT_TRUE(parser.parse_expression(input, result));
    
    ASSERT_TRUE(std::holds_alternative<ast::custom_operator_definition>(result));
    auto& op_def = std::get<ast::custom_operator_definition>(result);
    
    EXPECT_EQ(op_def.associativity, ast::CustomOperatorAssociativity::NONE);
}
