#include <gtest/gtest.h>
#include "meld/api/structural_search.hpp"
#include <iostream>

using namespace meld::api;
using namespace meld::parser;

class StructuralSearchIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Integration test: End-to-end workflow
TEST_F(StructuralSearchIntegrationTest, EndToEndWorkflow) {
    // Step 1: Parse source code
    std::string source = R"(
        val result1 = calculate(10, 20)
        val result2 = process(30, 40)
        val result3 = calculate(50, 60)
    )";
    
    ASSERT_NO_THROW({
        auto ast = Code::parse(source);
        
        // Step 2: Create pattern with variable capture
        auto pattern = Code::createPattern("calculate($a, $b)");
        
        // Step 3: Find all matches
        auto matches = ast.findAll(pattern);
        
        // Should find 2 calculate calls
        EXPECT_EQ(matches.size(), 2);
        
        // Step 4: Verify captures work
        for (const auto& match : matches) {
            EXPECT_TRUE(match.hasCapture("a"));
            EXPECT_TRUE(match.hasCapture("b"));
        }
        
        // Step 5: Test semantic search
        auto semantic = Code::createSemanticSearch(ast);
        auto calc_calls = semantic.findFunctionsByName("calculate");
        EXPECT_EQ(calc_calls.size(), 2);
        
        auto process_calls = semantic.findFunctionsByName("process");
        EXPECT_EQ(process_calls.size(), 1);
        
        // Step 6: Test transformation
        ast.replaceAll(pattern, [](const PatternMatch& match) {
            meld::parser::ast::function_call new_call;
            new_call.function_name.name = "compute";
            new_call.arguments.push_back(match.getCapture("a"));
            new_call.arguments.push_back(match.getCapture("b"));
            return meld::parser::ast::expression(new_call);
        });
        
        // Step 7: Verify transformation worked
        auto compute_calls = Code::createSemanticSearch(ast).findFunctionsByName("compute");
        EXPECT_EQ(compute_calls.size(), 2);
        
        auto remaining_calc_calls = Code::createSemanticSearch(ast).findFunctionsByName("calculate");
        EXPECT_EQ(remaining_calc_calls.size(), 0);
    });
}

// Integration test: Complex pattern matching
TEST_F(StructuralSearchIntegrationTest, ComplexPatternMatching) {
    std::string source = R"(
        val x = a + b
        val y = c * d
        val z = e + f
        val w = g - h
    )";
    
    ASSERT_NO_THROW({
        auto ast = Code::parse(source);
        
        // Find all addition operations
        auto add_pattern = Code::createPattern("$left + $right");
        auto add_matches = ast.findAll(add_pattern);
        EXPECT_EQ(add_matches.size(), 2); // x and z assignments
        
        // Find all multiplication operations
        auto mul_pattern = Code::createPattern("$left * $right");
        auto mul_matches = ast.findAll(mul_pattern);
        EXPECT_EQ(mul_matches.size(), 1); // y assignment
        
        // Find all subtraction operations
        auto sub_pattern = Code::createPattern("$left - $right");
        auto sub_matches = ast.findAll(sub_pattern);
        EXPECT_EQ(sub_matches.size(), 1); // w assignment
    });
}

// Integration test: Variable usage tracking
TEST_F(StructuralSearchIntegrationTest, VariableUsageTracking) {
    std::string source = R"(
        val x = 10
        val y = x + 5
        val z = x * 2
        val result = process(x, y, z)
    )";
    
    ASSERT_NO_THROW({
        auto ast = Code::parse(source);
        auto semantic = Code::createSemanticSearch(ast);
        
        // Find all usages of variable 'x'
        auto x_usages = semantic.findVariableUsages("x");
        EXPECT_GE(x_usages.size(), 3); // At least in y, z, and result assignments
        
        // Find assignments to 'y'
        auto y_assignments = semantic.findAssignments("y");
        EXPECT_EQ(y_assignments.size(), 1);
        
        // Find assignments to 'x'
        auto x_assignments = semantic.findAssignments("x");
        EXPECT_EQ(x_assignments.size(), 1);
    });
}

// Integration test: Nested function calls
TEST_F(StructuralSearchIntegrationTest, NestedFunctionCalls) {
    std::string source = R"(
        val result = outer(inner(42))
        val other = outer(simple(10))
    )";
    
    ASSERT_NO_THROW({
        auto ast = Code::parse(source);
        auto semantic = Code::createSemanticSearch(ast);
        
        // Find all calls to 'outer'
        auto outer_calls = semantic.findFunctionsByName("outer");
        EXPECT_EQ(outer_calls.size(), 2);
        
        // Find all calls to 'inner'
        auto inner_calls = semantic.findFunctionsByName("inner");
        EXPECT_EQ(inner_calls.size(), 1);
        
        // Find all calls to 'simple'
        auto simple_calls = semantic.findFunctionsByName("simple");
        EXPECT_EQ(simple_calls.size(), 1);
    });
}

// Integration test: Round-trip source code generation
TEST_F(StructuralSearchIntegrationTest, RoundTripSourceGeneration) {
    std::string source = "val x = 42";
    
    ASSERT_NO_THROW({
        auto ast = Code::parse(source);
        std::string regenerated = ast.toSourceCode();
        
        // The regenerated code should be parseable
        auto ast2 = Code::parse(regenerated);
        
        // Both ASTs should find the same patterns
        auto pattern = Code::createPattern("val $name = $value");
        
        auto matches1 = ast.findAll(pattern);
        auto matches2 = ast2.findAll(pattern);
        
        EXPECT_EQ(matches1.size(), matches2.size());
        EXPECT_EQ(matches1.size(), 1);
    });
}

/* main removed - using gtest_main */