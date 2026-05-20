#include <gtest/gtest.h>
#include "meld/parser/parser.hpp"

using namespace meld::parser;

TEST(LexerTest, TokenizeSimpleIdentifier) {
    Lexer lexer("hello");
    auto tokens = lexer.tokenize();
    
    ASSERT_EQ(tokens.size(), 2); // identifier + EOF
    EXPECT_EQ(tokens[0].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[0].value, "hello");
    EXPECT_EQ(tokens[1].type, TokenType::END_OF_FILE);
}

TEST(LexerTest, TokenizeKeywords) {
    Lexer lexer("val var fn class");
    auto tokens = lexer.tokenize();
    
    ASSERT_EQ(tokens.size(), 5); // 4 keywords + EOF
    EXPECT_EQ(tokens[0].type, TokenType::KEYWORD);
    EXPECT_EQ(tokens[0].value, "val");
    EXPECT_EQ(tokens[1].type, TokenType::KEYWORD);
    EXPECT_EQ(tokens[1].value, "var");
    EXPECT_EQ(tokens[2].type, TokenType::KEYWORD);
    EXPECT_EQ(tokens[2].value, "fn");
    EXPECT_EQ(tokens[3].type, TokenType::KEYWORD);
    EXPECT_EQ(tokens[3].value, "class");
}

TEST(LexerTest, TokenizeNumbers) {
    Lexer lexer("42 3.14 10L 2.5F 1.0D");
    auto tokens = lexer.tokenize();
    
    ASSERT_EQ(tokens.size(), 6); // 5 numbers + EOF
    EXPECT_EQ(tokens[0].type, TokenType::NUMBER);
    EXPECT_EQ(tokens[0].value, "42");
    EXPECT_EQ(tokens[1].type, TokenType::NUMBER);
    EXPECT_EQ(tokens[1].value, "3.14");
    EXPECT_EQ(tokens[2].type, TokenType::NUMBER);
    EXPECT_EQ(tokens[2].value, "10L");
    EXPECT_EQ(tokens[3].type, TokenType::NUMBER);
    EXPECT_EQ(tokens[3].value, "2.5F");
    EXPECT_EQ(tokens[4].type, TokenType::NUMBER);
    EXPECT_EQ(tokens[4].value, "1.0D");
}

TEST(LexerTest, TokenizeString) {
    Lexer lexer("\"hello world\"");
    auto tokens = lexer.tokenize();
    
    ASSERT_EQ(tokens.size(), 2); // string + EOF
    EXPECT_EQ(tokens[0].type, TokenType::STRING);
    EXPECT_EQ(tokens[0].value, "hello world");
}

TEST(LexerTest, TokenizeStringWithInterpolation) {
    Lexer lexer("\"Hello ${name}!\"");
    auto tokens = lexer.tokenize();
    
    ASSERT_EQ(tokens.size(), 2); // string + EOF
    EXPECT_EQ(tokens[0].type, TokenType::STRING);
    EXPECT_TRUE(tokens[0].value.find("${") != std::string::npos);
}

TEST(LexerTest, TokenizeMultilineString) {
    Lexer lexer("\"\"\"Line 1\nLine 2\nLine 3\"\"\"");
    auto tokens = lexer.tokenize();
    
    ASSERT_EQ(tokens.size(), 2); // multiline string + EOF
    EXPECT_EQ(tokens[0].type, TokenType::MULTILINE_STRING);
    EXPECT_TRUE(tokens[0].value.find("Line 1") != std::string::npos);
    EXPECT_TRUE(tokens[0].value.find("Line 2") != std::string::npos);
}

TEST(LexerTest, TokenizeMultilineStringWithInterpolation) {
    Lexer lexer("\"\"\"Hello ${name}\nGoodbye ${name}\"\"\"");
    auto tokens = lexer.tokenize();
    
    ASSERT_EQ(tokens.size(), 2); // multiline string + EOF
    EXPECT_EQ(tokens[0].type, TokenType::MULTILINE_STRING);
    EXPECT_TRUE(tokens[0].value.find("${") != std::string::npos);
}

TEST(LexerTest, TokenizeRegex) {
    Lexer lexer("/[a-z]+/gi");
    auto tokens = lexer.tokenize();
    
    ASSERT_EQ(tokens.size(), 2); // regex + EOF
    EXPECT_EQ(tokens[0].type, TokenType::REGEX);
    EXPECT_TRUE(tokens[0].value.find("[a-z]+") != std::string::npos);
    EXPECT_TRUE(tokens[0].value.find("gi") != std::string::npos);
}

TEST(LexerTest, TokenizeOperators) {
    Lexer lexer("+ - * / == != <= >= && || ?. ?? |> =>");
    auto tokens = lexer.tokenize();
    
    ASSERT_EQ(tokens.size(), 15); // 14 operators + EOF
    EXPECT_EQ(tokens[0].value, "+");
    EXPECT_EQ(tokens[1].value, "-");
    EXPECT_EQ(tokens[2].value, "*");
    EXPECT_EQ(tokens[3].value, "/");
    EXPECT_EQ(tokens[4].value, "==");
    EXPECT_EQ(tokens[5].value, "!=");
    EXPECT_EQ(tokens[6].value, "<=");
    EXPECT_EQ(tokens[7].value, ">=");
    EXPECT_EQ(tokens[8].value, "&&");
    EXPECT_EQ(tokens[9].value, "||");
    EXPECT_EQ(tokens[10].value, "?.");
    EXPECT_EQ(tokens[11].value, "??");
    EXPECT_EQ(tokens[12].value, "|>");
    EXPECT_EQ(tokens[13].value, "=>");
}

TEST(LexerTest, TokenizeComments) {
    Lexer lexer("val x = 42 // comment\nval y = 10");
    auto tokens = lexer.tokenize();
    
    // Should skip comments
    ASSERT_EQ(tokens.size(), 9); // val x = 42 val y = 10 EOF
    EXPECT_EQ(tokens[0].value, "val");
    EXPECT_EQ(tokens[1].value, "x");
    EXPECT_EQ(tokens[4].value, "val");
    EXPECT_EQ(tokens[5].value, "y");
}

TEST(LexerTest, TokenizeMultilineComments) {
    Lexer lexer("val x = /* comment */ 42");
    auto tokens = lexer.tokenize();
    
    ASSERT_EQ(tokens.size(), 5); // val x = 42 EOF
    EXPECT_EQ(tokens[0].value, "val");
    EXPECT_EQ(tokens[1].value, "x");
    EXPECT_EQ(tokens[2].value, "=");
    EXPECT_EQ(tokens[3].value, "42");
}

TEST(LexerTest, LineAndColumnTracking) {
    Lexer lexer("val x = 42\nvar y = 10");
    auto tokens = lexer.tokenize();
    
    EXPECT_EQ(tokens[0].line, 1);
    EXPECT_EQ(tokens[0].column, 1);
    
    // var should be on line 2
    EXPECT_EQ(tokens[4].line, 2);
    EXPECT_EQ(tokens[4].value, "var");
}

TEST(LexerTest, ErrorReporting) {
    Lexer lexer("\"unterminated string");
    auto tokens = lexer.tokenize();
    
    EXPECT_FALSE(lexer.errors().empty());
    EXPECT_TRUE(lexer.errors()[0].find("Unterminated") != std::string::npos);
}

// Tests for hyphenated identifiers (kebab-case)
TEST(LexerTest, TokenizeHyphenatedIdentifier) {
    Lexer lexer("my-variable");
    auto tokens = lexer.tokenize();
    
    ASSERT_EQ(tokens.size(), 2); // identifier + EOF
    EXPECT_EQ(tokens[0].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[0].value, "my-variable");
}

TEST(LexerTest, TokenizeMultipleHyphenatedIdentifiers) {
    Lexer lexer("first-name last-name email-address");
    auto tokens = lexer.tokenize();
    
    ASSERT_EQ(tokens.size(), 4); // 3 identifiers + EOF
    EXPECT_EQ(tokens[0].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[0].value, "first-name");
    EXPECT_EQ(tokens[1].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[1].value, "last-name");
    EXPECT_EQ(tokens[2].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[2].value, "email-address");
}

TEST(LexerTest, TokenizeHyphenatedFunctionName) {
    Lexer lexer("fn calculate-total() {}");
    auto tokens = lexer.tokenize();
    
    ASSERT_GE(tokens.size(), 2);
    EXPECT_EQ(tokens[0].type, TokenType::KEYWORD);
    EXPECT_EQ(tokens[0].value, "fn");
    EXPECT_EQ(tokens[1].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[1].value, "calculate-total");
}

TEST(LexerTest, TokenizeComplexHyphenatedIdentifier) {
    Lexer lexer("my-complex-variable-name");
    auto tokens = lexer.tokenize();
    
    ASSERT_EQ(tokens.size(), 2); // identifier + EOF
    EXPECT_EQ(tokens[0].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[0].value, "my-complex-variable-name");
}

TEST(LexerTest, TokenizeHyphenatedWithNumbers) {
    Lexer lexer("var-123 test-456-abc");
    auto tokens = lexer.tokenize();
    
    ASSERT_EQ(tokens.size(), 3); // 2 identifiers + EOF
    EXPECT_EQ(tokens[0].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[0].value, "var-123");
    EXPECT_EQ(tokens[1].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[1].value, "test-456-abc");
}

TEST(LexerTest, TokenizeHyphenatedWithUnderscore) {
    Lexer lexer("my_var-name test-var_name");
    auto tokens = lexer.tokenize();
    
    ASSERT_EQ(tokens.size(), 3); // 2 identifiers + EOF
    EXPECT_EQ(tokens[0].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[0].value, "my_var-name");
    EXPECT_EQ(tokens[1].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[1].value, "test-var_name");
}

TEST(LexerTest, HyphenCannotStartIdentifier) {
    Lexer lexer("-variable");
    auto tokens = lexer.tokenize();
    
    // Should tokenize as operator(-) and identifier(variable)
    ASSERT_EQ(tokens.size(), 3); // operator + identifier + EOF
    EXPECT_EQ(tokens[0].type, TokenType::OPERATOR);
    EXPECT_EQ(tokens[0].value, "-");
    EXPECT_EQ(tokens[1].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[1].value, "variable");
}

TEST(LexerTest, HyphenCannotEndIdentifier) {
    Lexer lexer("variable-");
    auto tokens = lexer.tokenize();
    
    // Should tokenize as identifier(variable) and operator(-)
    ASSERT_EQ(tokens.size(), 3); // identifier + operator + EOF
    EXPECT_EQ(tokens[0].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[0].value, "variable");
    EXPECT_EQ(tokens[1].type, TokenType::OPERATOR);
    EXPECT_EQ(tokens[1].value, "-");
}

TEST(LexerTest, HyphenInExpression) {
    Lexer lexer("my-var - other-var");
    auto tokens = lexer.tokenize();
    
    // Should tokenize as: my-var, -, other-var
    ASSERT_EQ(tokens.size(), 4); // 2 identifiers + 1 operator + EOF
    EXPECT_EQ(tokens[0].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[0].value, "my-var");
    EXPECT_EQ(tokens[1].type, TokenType::OPERATOR);
    EXPECT_EQ(tokens[1].value, "-");
    EXPECT_EQ(tokens[2].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[2].value, "other-var");
}

TEST(LexerTest, DoubleHyphenNotAllowed) {
    Lexer lexer("my--var");
    auto tokens = lexer.tokenize();
    
    // Should tokenize as: my, -, -, var
    ASSERT_EQ(tokens.size(), 5); // identifier + operator + operator + identifier + EOF
    EXPECT_EQ(tokens[0].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[0].value, "my");
    EXPECT_EQ(tokens[1].type, TokenType::OPERATOR);
    EXPECT_EQ(tokens[1].value, "-");
    EXPECT_EQ(tokens[2].type, TokenType::OPERATOR);
    EXPECT_EQ(tokens[2].value, "-");
    EXPECT_EQ(tokens[3].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[3].value, "var");
}

TEST(LexerTest, HyphenatedInFunctionCall) {
    Lexer lexer("calculate-sum(first-value, second-value)");
    auto tokens = lexer.tokenize();
    
    ASSERT_GE(tokens.size(), 6);
    EXPECT_EQ(tokens[0].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[0].value, "calculate-sum");
    EXPECT_EQ(tokens[2].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[2].value, "first-value");
    EXPECT_EQ(tokens[4].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[4].value, "second-value");
}

TEST(LexerTest, HyphenatedInAssignment) {
    Lexer lexer("val my-variable = 42");
    auto tokens = lexer.tokenize();
    
    ASSERT_EQ(tokens.size(), 5); // val, my-variable, =, 42, EOF
    EXPECT_EQ(tokens[0].type, TokenType::KEYWORD);
    EXPECT_EQ(tokens[0].value, "val");
    EXPECT_EQ(tokens[1].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[1].value, "my-variable");
    EXPECT_EQ(tokens[2].type, TokenType::OPERATOR);
    EXPECT_EQ(tokens[2].value, "=");
    EXPECT_EQ(tokens[3].type, TokenType::NUMBER);
    EXPECT_EQ(tokens[3].value, "42");
}

TEST(LexerTest, HyphenatedClassAndMethodNames) {
    Lexer lexer("class User-Profile { fn get-full-name() {} }");
    auto tokens = lexer.tokenize();
    
    ASSERT_GE(tokens.size(), 8);
    EXPECT_EQ(tokens[0].type, TokenType::KEYWORD);
    EXPECT_EQ(tokens[0].value, "class");
    EXPECT_EQ(tokens[1].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[1].value, "User-Profile");
    EXPECT_EQ(tokens[3].type, TokenType::KEYWORD);
    EXPECT_EQ(tokens[3].value, "fn");
    EXPECT_EQ(tokens[4].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[4].value, "get-full-name");
}

TEST(LexerTest, HyphenBeforeOperator) {
    Lexer lexer("my-var-10");
    auto tokens = lexer.tokenize();
    
    // Should be a single identifier
    ASSERT_EQ(tokens.size(), 2); // identifier + EOF
    EXPECT_EQ(tokens[0].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[0].value, "my-var-10");
}

TEST(LexerTest, HyphenatedWithParentheses) {
    Lexer lexer("(my-var)");
    auto tokens = lexer.tokenize();
    
    ASSERT_EQ(tokens.size(), 4); // (, my-var, ), EOF
    EXPECT_EQ(tokens[0].type, TokenType::OPERATOR);
    EXPECT_EQ(tokens[0].value, "(");
    EXPECT_EQ(tokens[1].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[1].value, "my-var");
    EXPECT_EQ(tokens[2].type, TokenType::OPERATOR);
    EXPECT_EQ(tokens[2].value, ")");
}
