#include <gtest/gtest.h>
#include "meld/api/structural_search.hpp"
#include <iostream>

using namespace meld::api;
using namespace meld::parser;

class StructuralSearchTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test 1: Code.parse() for converting source to searchable AST
TEST_F(StructuralSearchTest, CodeParseBasic) {
    std::string source = "val x = 42";
    
    ASSERT_NO_THROW({
        auto ast = Code::parse(source);
        EXPECT_TRUE(true); // Successfully parsed
    });
}

TEST_F(StructuralSearchTest, CodeParseFunctionCall) {
    std::string source = "foo(1, 2, 3)";
    
    ASSERT_NO_THROW({
        auto ast = Code::parse(source);
        EXPECT_TRUE(true); // Successfully parsed
    });
}

TEST_F(StructuralSearchTest, CodeParseInvalidSyntax) {
    std::string source = "val x ="; // Incomplete
    
    EXPECT_THROW({
        auto ast = Code::parse(source);
    }, std::runtime_error);
}

// Test 2: ast`` literals for pattern matching with variable capture
TEST_F(StructuralSearchTest, CreatePatternSimple) {
    ASSERT_NO_THROW({
        auto pattern = Code::createPattern("foo($a, $b)");
        EXPECT_TRUE(true); // Successfully created pattern
    });
}

TEST_F(StructuralSearchTest, CreatePatternWithCapture) {
    std::string pattern_code = "$x + $y";
    
    ASSERT_NO_THROW({
        auto pattern = Code::createPattern(pattern_code);
        EXPECT_TRUE(true); // Successfully created pattern with captures
    });
}

// Test 3: findAll() method for pattern-based search
TEST_F(StructuralSearchTest, FindAllFunctionCalls) {
    std::string source = R"(
        val a = foo(1, 2)
        val b = bar(3, 4)
        val c = foo(5, 6)
    )";
    
    auto ast = Code::parse(source);
    auto pattern = Code::createPattern("foo($a, $b)");
    
    auto matches = ast.findAll(pattern);
    
    // Should find 2 matches for foo()
    EXPECT_EQ(matches.size(), 2);
}

TEST_F(StructuralSearchTest, FindAllWithCaptures) {
    std::string source = "val x = add(10, 20)";
    
    auto ast = Code::parse(source);
    auto pattern = Code::createPattern("add($a, $b)");
    
    auto matches = ast.findAll(pattern);
    
    ASSERT_EQ(matches.size(), 1);
    
    // Check that captures were recorded
    const auto& match = matches[0];
    EXPECT_TRUE(match.hasCapture("a"));
    EXPECT_TRUE(match.hasCapture("b"));
}

TEST_F(StructuralSearchTest, FindAllNoMatches) {
    std::string source = "val x = foo(1, 2)";
    
    auto ast = Code::parse(source);
    auto pattern = Code::createPattern("bar($a, $b)");
    
    auto matches = ast.findAll(pattern);
    
    EXPECT_EQ(matches.size(), 0);
}

// Test 4: replace() method for AST-based transformations
TEST_F(StructuralSearchTest, ReplaceAllBasic) {
    std::string source = "val x = oldFunc(1, 2)";
    
    auto ast = Code::parse(source);
    auto pattern = Code::createPattern("oldFunc($a, $b)");
    
    // Create replacement
    auto replacement_ast = Code::parse("newFunc(1, 2)");
    
    ASSERT_NO_THROW({
        ast.replaceAll(pattern, replacement_ast.getRootAST());
    });
}

TEST_F(StructuralSearchTest, ReplaceAllWithLambda) {
    std::string source = R"(
        val a = foo(1, 2)
        val b = foo(3, 4)
    )";
    
    auto ast = Code::parse(source);
    auto pattern = Code::createPattern("foo($a, $b)");
    
    // Replace with a function that uses the captures
    ASSERT_NO_THROW({
        ast.replaceAll(pattern, [](const PatternMatch& match) {
            // Create a new function call with different name
            meld::parser::ast::function_call new_call;
            new_call.function_name.name = "bar";
            new_call.arguments.push_back(match.getCapture("a"));
            new_call.arguments.push_back(match.getCapture("b"));
            return meld::parser::ast::expression(new_call);
        });
    });
}

// Test 5: Semantic search capabilities
TEST_F(StructuralSearchTest, SemanticSearchFindFunctionsByName) {
    std::string source = R"(
        val a = calculate(1, 2)
        val b = process(3, 4)
        val c = calculate(5, 6)
    )";
    
    auto ast = Code::parse(source);
    auto semantic = Code::createSemanticSearch(ast);
    
    auto matches = semantic.findFunctionsByName("calculate");
    
    EXPECT_EQ(matches.size(), 2);
}

TEST_F(StructuralSearchTest, SemanticSearchFindVariableUsages) {
    std::string source = R"(
        val x = 10
        val y = x + 5
        val z = x * 2
    )";
    
    auto ast = Code::parse(source);
    auto semantic = Code::createSemanticSearch(ast);
    
    auto matches = semantic.findVariableUsages("x");
    
    // Should find x in the declaration and two usages
    EXPECT_GE(matches.size(), 2);
}

TEST_F(StructuralSearchTest, SemanticSearchFindAssignments) {
    std::string source = R"(
        val x = 10
        var y = 20
        val x = 30
    )";
    
    auto ast = Code::parse(source);
    auto semantic = Code::createSemanticSearch(ast);
    
    auto matches = semantic.findAssignments("x");
    
    // Should find 2 assignments to x
    EXPECT_EQ(matches.size(), 2);
}

TEST_F(StructuralSearchTest, SemanticSearchFindLambdas) {
    std::string source = R"(
        val f = x => x + 1
        val g = (a, b) => a + b
    )";
    
    auto ast = Code::parse(source);
    auto semantic = Code::createSemanticSearch(ast);
    
    auto matches = semantic.findLambdaExpressions();
    
    EXPECT_EQ(matches.size(), 2);
}

// Test 6: Pattern matching with identifiers
TEST_F(StructuralSearchTest, PatternMatchIdentifier) {
    std::string source = "myVariable";
    
    auto ast = Code::parse(source);
    auto pattern = Code::createPattern("myVariable");
    
    auto matches = ast.findAll(pattern);
    
    EXPECT_EQ(matches.size(), 1);
}

// Test 7: Pattern matching with literals
TEST_F(StructuralSearchTest, PatternMatchIntegerLiteral) {
    std::string source = "42";
    
    auto ast = Code::parse(source);
    auto pattern = Code::createPattern("42");
    
    auto matches = ast.findAll(pattern);
    
    EXPECT_EQ(matches.size(), 1);
}

TEST_F(StructuralSearchTest, PatternMatchStringLiteral) {
    std::string source = R"("hello")";
    
    auto ast = Code::parse(source);
    auto pattern = Code::createPattern(R"("hello")");
    
    auto matches = ast.findAll(pattern);
    
    EXPECT_EQ(matches.size(), 1);
}

// Test 8: Pattern matching with binary operations
TEST_F(StructuralSearchTest, PatternMatchBinaryOperation) {
    std::string source = "x + y";
    
    auto ast = Code::parse(source);
    auto pattern = Code::createPattern("$a + $b");
    
    auto matches = ast.findAll(pattern);
    
    ASSERT_EQ(matches.size(), 1);
    EXPECT_TRUE(matches[0].hasCapture("a"));
    EXPECT_TRUE(matches[0].hasCapture("b"));
}

// Test 9: findFirst() method
TEST_F(StructuralSearchTest, FindFirstMatch) {
    std::string source = R"(
        val a = foo(1, 2)
        val b = foo(3, 4)
    )";
    
    auto ast = Code::parse(source);
    auto pattern = Code::createPattern("foo($a, $b)");
    
    auto match = ast.findFirst(pattern);
    
    ASSERT_TRUE(match.has_value());
    EXPECT_TRUE(match->hasCapture("a"));
    EXPECT_TRUE(match->hasCapture("b"));
}

TEST_F(StructuralSearchTest, FindFirstNoMatch) {
    std::string source = "val x = bar(1, 2)";
    
    auto ast = Code::parse(source);
    auto pattern = Code::createPattern("foo($a, $b)");
    
    auto match = ast.findFirst(pattern);
    
    EXPECT_FALSE(match.has_value());
}

// Test 10: toSourceCode() method
TEST_F(StructuralSearchTest, ToSourceCodeRoundTrip) {
    std::string source = "val x = 42";
    
    auto ast = Code::parse(source);
    std::string regenerated = ast.toSourceCode();
    
    // The regenerated code should be parseable
    ASSERT_NO_THROW({
        auto ast2 = Code::parse(regenerated);
    });
}

/* main removed - using gtest_main */