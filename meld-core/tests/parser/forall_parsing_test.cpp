#include <gtest/gtest.h>
#include "meld/parser/parser.hpp"

class ForallParsingTest : public ::testing::Test {
protected:
    void SetUp() override {}
    
    bool parse_function(const std::string& input, meld::parser::ast::function_definition& result) {
        meld::parser::Lexer lexer(input);
        auto tokens = lexer.tokenize();
        
        if (!lexer.errors().empty()) {
            return false;
        }
        
        meld::parser::TokenParser parser(tokens);
        meld::parser::ast::expression expr;
        
        if (!parser.parse_expression(expr)) {
            return false;
        }
        
        auto func_def = boost::get<boost::spirit::x3::forward_ast<meld::parser::ast::function_definition>>(&expr);
        if (!func_def) {
            return false;
        }
        
        result = func_def->get();
        return true;
    }
};

TEST_F(ForallParsingTest, ParseBasicForallTest) {
    std::string input = R"(
        fn test_function(x: int) -> int {
            test "property test" forall a: int {
                assert a >= 0, "a should be non-negative"
            }
            
            return x * 2
        }
    )";
    
    meld::parser::ast::function_definition func_def;
    ASSERT_TRUE(parse_function(input, func_def));
    
    // Check if function has tests
    EXPECT_TRUE(func_def.has_tests);
    EXPECT_EQ(func_def.tests.size(), 1);
    
    // Check the test block
    const auto& test_block = func_def.tests[0];
    
    EXPECT_TRUE(test_block.has_description);
    EXPECT_EQ(test_block.description, "property test");
    EXPECT_TRUE(test_block.is_property_test);
    EXPECT_EQ(test_block.forall_variables.size(), 1);
    EXPECT_EQ(test_block.forall_variables[0].name, "a");
}

TEST_F(ForallParsingTest, ParseMultipleForallVariables) {
    std::string input = R"(
        fn test_function(x: int) -> int {
            test "multi-variable property" forall a: int, b: string, c: bool {
                assert a >= 0, "a should be non-negative"
                assert b.length() >= 0, "string should have non-negative length"
            }
            
            return x * 2
        }
    )";
    
    meld::parser::ast::function_definition func_def;
    ASSERT_TRUE(parse_function(input, func_def));
    
    // Check if function has tests
    EXPECT_TRUE(func_def.has_tests);
    EXPECT_EQ(func_def.tests.size(), 1);
    
    // Check the test block
    const auto& test_block = func_def.tests[0];
    
    EXPECT_TRUE(test_block.has_description);
    EXPECT_EQ(test_block.description, "multi-variable property");
    EXPECT_TRUE(test_block.is_property_test);
    EXPECT_EQ(test_block.forall_variables.size(), 3);
    EXPECT_EQ(test_block.forall_variables[0].name, "a");
    EXPECT_EQ(test_block.forall_variables[1].name, "b");
    EXPECT_EQ(test_block.forall_variables[2].name, "c");
}

TEST_F(ForallParsingTest, ParseForallWithNullableTypes) {
    std::string input = R"(
        fn test_function(x: int) -> int {
            test "nullable property" forall a: int?, b: string? {
                // Test nullable types
            }
            
            return x * 2
        }
    )";
    
    meld::parser::ast::function_definition func_def;
    ASSERT_TRUE(parse_function(input, func_def));
    
    // Check if function has tests
    EXPECT_TRUE(func_def.has_tests);
    EXPECT_EQ(func_def.tests.size(), 1);
    
    // Check the test block
    const auto& test_block = func_def.tests[0];
    
    EXPECT_TRUE(test_block.has_description);
    EXPECT_EQ(test_block.description, "nullable property");
    EXPECT_TRUE(test_block.is_property_test);
    EXPECT_EQ(test_block.forall_variables.size(), 2);
    EXPECT_EQ(test_block.forall_variables[0].name, "a");
    EXPECT_EQ(test_block.forall_variables[1].name, "b");
}

TEST_F(ForallParsingTest, ParseRegularTestWithoutForall) {
    std::string input = R"(
        fn test_function(x: int) -> int {
            test "regular test" {
                assert x > 0, "x should be positive"
            }
            
            return x * 2
        }
    )";
    
    meld::parser::ast::function_definition func_def;
    ASSERT_TRUE(parse_function(input, func_def));
    
    // Check if function has tests
    EXPECT_TRUE(func_def.has_tests);
    EXPECT_EQ(func_def.tests.size(), 1);
    
    // Check the test block
    const auto& test_block = func_def.tests[0];
    
    EXPECT_TRUE(test_block.has_description);
    EXPECT_EQ(test_block.description, "regular test");
    EXPECT_FALSE(test_block.is_property_test);  // Should NOT be a property test
    EXPECT_EQ(test_block.forall_variables.size(), 0);  // No forall variables
}

TEST_F(ForallParsingTest, ParseForallWithoutDescription) {
    std::string input = R"(
        fn test_function(x: int) -> int {
            test forall a: int {
                assert a >= 0, "a should be non-negative"
            }
            
            return x * 2
        }
    )";
    
    meld::parser::ast::function_definition func_def;
    ASSERT_TRUE(parse_function(input, func_def));
    
    // Check if function has tests
    EXPECT_TRUE(func_def.has_tests);
    EXPECT_EQ(func_def.tests.size(), 1);
    
    // Check the test block
    const auto& test_block = func_def.tests[0];
    
    EXPECT_FALSE(test_block.has_description);  // No description
    EXPECT_TRUE(test_block.is_property_test);
    EXPECT_EQ(test_block.forall_variables.size(), 1);
    EXPECT_EQ(test_block.forall_variables[0].name, "a");
}
