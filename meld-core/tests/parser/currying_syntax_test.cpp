#include <gtest/gtest.h>
#include "meld/parser/parser.hpp"
#include "meld/parser/lexer.hpp"

using namespace meld::parser;

class CurryingSyntaxTest : public ::testing::Test {
protected:
    void SetUp() override {}
    
    bool parse_source(const std::string& source, ast::expression& result) {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        if (!tokens.has_value()) {
            return false;
        }
        
        Parser parser(tokens.value());
        return parser.parse_expression(result);
    }
};

TEST_F(CurryingSyntaxTest, ParseSimpleCurriedFunction) {
    std::string source = R"(
        fnc add(a: int)(b: int) -> int {
            rtn a + b
        }
    )";
    
    ast::expression result;
    ASSERT_TRUE(parse_source(source, result));
    
    ASSERT_TRUE(std::holds_alternative<ast::function_definition>(result));
    auto func_def = std::get<ast::function_definition>(result);
    
    // Check that it's marked as curried
    EXPECT_TRUE(func_def.is_curried);
    
    // Check first parameter group
    ASSERT_EQ(func_def.parameters.size(), 1);
    EXPECT_EQ(func_def.parameters[0].name.name, "a");
    EXPECT_EQ(func_def.parameters[0].type.type_name.name, "int");
    
    // Check curried parameter groups
    ASSERT_EQ(func_def.curried_parameter_groups.size(), 1);
    ASSERT_EQ(func_def.curried_parameter_groups[0].size(), 1);
    EXPECT_EQ(func_def.curried_parameter_groups[0][0].name.name, "b");
    EXPECT_EQ(func_def.curried_parameter_groups[0][0].type.type_name.name, "int");
    
    // Check return type
    EXPECT_TRUE(func_def.has_return_type);
    EXPECT_EQ(func_def.return_type.type_name.name, "int");
}

TEST_F(CurryingSyntaxTest, ParseThreeParameterCurriedFunction) {
    std::string source = R"(
        fnc multiply(a: int)(b: int)(c: int) -> int {
            rtn a * b * c
        }
    )";
    
    ast::expression result;
    ASSERT_TRUE(parse_source(source, result));
    
    ASSERT_TRUE(std::holds_alternative<ast::function_definition>(result));
    auto func_def = std::get<ast::function_definition>(result);
    
    // Check that it's marked as curried
    EXPECT_TRUE(func_def.is_curried);
    
    // Check first parameter group
    ASSERT_EQ(func_def.parameters.size(), 1);
    EXPECT_EQ(func_def.parameters[0].name.name, "a");
    
    // Check curried parameter groups (should have 2 groups)
    ASSERT_EQ(func_def.curried_parameter_groups.size(), 2);
    
    // Second parameter group
    ASSERT_EQ(func_def.curried_parameter_groups[0].size(), 1);
    EXPECT_EQ(func_def.curried_parameter_groups[0][0].name.name, "b");
    
    // Third parameter group
    ASSERT_EQ(func_def.curried_parameter_groups[1].size(), 1);
    EXPECT_EQ(func_def.curried_parameter_groups[1][0].name.name, "c");
}

TEST_F(CurryingSyntaxTest, ParseCurriedFunctionWithMultipleParametersInGroup) {
    std::string source = R"(
        fnc process(x: int, y: int)(z: string) -> string {
            rtn z + (x + y).toString()
        }
    )";
    
    ast::expression result;
    ASSERT_TRUE(parse_source(source, result));
    
    ASSERT_TRUE(std::holds_alternative<ast::function_definition>(result));
    auto func_def = std::get<ast::function_definition>(result);
    
    // Check that it's marked as curried
    EXPECT_TRUE(func_def.is_curried);
    
    // Check first parameter group (should have 2 parameters)
    ASSERT_EQ(func_def.parameters.size(), 2);
    EXPECT_EQ(func_def.parameters[0].name.name, "x");
    EXPECT_EQ(func_def.parameters[1].name.name, "y");
    
    // Check curried parameter groups (should have 1 group with 1 parameter)
    ASSERT_EQ(func_def.curried_parameter_groups.size(), 1);
    ASSERT_EQ(func_def.curried_parameter_groups[0].size(), 1);
    EXPECT_EQ(func_def.curried_parameter_groups[0][0].name.name, "z");
    EXPECT_EQ(func_def.curried_parameter_groups[0][0].type.type_name.name, "string");
}

TEST_F(CurryingSyntaxTest, ParseRegularFunctionIsNotCurried) {
    std::string source = R"(
        fnc add(a: int, b: int) -> int {
            rtn a + b
        }
    )";
    
    ast::expression result;
    ASSERT_TRUE(parse_source(source, result));
    
    ASSERT_TRUE(std::holds_alternative<ast::function_definition>(result));
    auto func_def = std::get<ast::function_definition>(result);
    
    // Check that it's NOT marked as curried
    EXPECT_FALSE(func_def.is_curried);
    
    // Check parameters
    ASSERT_EQ(func_def.parameters.size(), 2);
    EXPECT_EQ(func_def.parameters[0].name.name, "a");
    EXPECT_EQ(func_def.parameters[1].name.name, "b");
    
    // Check no curried parameter groups
    EXPECT_EQ(func_def.curried_parameter_groups.size(), 0);
}

TEST_F(CurryingSyntaxTest, ParseCurriedFunctionWithDefaultValues) {
    std::string source = R"(
        fnc greet(prefix: string = "Hello")(name: string) -> string {
            rtn prefix + " " + name
        }
    )";
    
    ast::expression result;
    ASSERT_TRUE(parse_source(source, result));
    
    ASSERT_TRUE(std::holds_alternative<ast::function_definition>(result));
    auto func_def = std::get<ast::function_definition>(result);
    
    // Check that it's marked as curried
    EXPECT_TRUE(func_def.is_curried);
    
    // Check first parameter has default value
    ASSERT_EQ(func_def.parameters.size(), 1);
    EXPECT_EQ(func_def.parameters[0].name.name, "prefix");
    EXPECT_TRUE(func_def.parameters[0].has_default);
    
    // Check curried parameter
    ASSERT_EQ(func_def.curried_parameter_groups.size(), 1);
    ASSERT_EQ(func_def.curried_parameter_groups[0].size(), 1);
    EXPECT_EQ(func_def.curried_parameter_groups[0][0].name.name, "name");
    EXPECT_FALSE(func_def.curried_parameter_groups[0][0].has_default);
}