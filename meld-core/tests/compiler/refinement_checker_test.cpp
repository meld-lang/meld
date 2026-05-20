#include <gtest/gtest.h>
#include "meld/compiler/refinement_checker.hpp"
#include "meld/types/refinement.hpp"
#include "meld/parser/parser.hpp"

using namespace meld::compiler;
using namespace meld::types;
using namespace meld::parser;

class RefinementCheckerTest : public ::testing::Test {
protected:
    void SetUp() override {
        RefinementTypes::register_builtin_types();
    }
    
    RefinementChecker checker;
    Parser parser;
};

// Test compile-time validation of positive integer literals
TEST_F(RefinementCheckerTest, ValidatePositiveIntLiterals) {
    auto positive_int = RefinementTypes::create_positive_int();
    
    // Valid positive integer
    ast::expression valid_expr;
    ASSERT_TRUE(parser.parse_expression("42", valid_expr));
    
    auto result = checker.validate_static_assignment(valid_expr, positive_int);
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(result.value());
    
    // Invalid zero
    ast::expression zero_expr;
    ASSERT_TRUE(parser.parse_expression("0", zero_expr));
    
    auto zero_result = checker.validate_static_assignment(zero_expr, positive_int);
    EXPECT_FALSE(zero_result.has_value());
    EXPECT_TRUE(zero_result.error().find("refinement constraint") != std::string::npos);
    
    // Invalid negative
    ast::expression negative_expr;
    ASSERT_TRUE(parser.parse_expression("-5", negative_expr));
    
    auto negative_result = checker.validate_static_assignment(negative_expr, positive_int);
    EXPECT_FALSE(negative_result.has_value());
    EXPECT_TRUE(negative_result.error().find("refinement constraint") != std::string::npos);
}

// Test compile-time validation of uint literals
TEST_F(RefinementCheckerTest, ValidateUintLiterals) {
    auto uint_type = RefinementTypes::create_uint();
    
    // Valid unsigned integers
    ast::expression zero_expr;
    ASSERT_TRUE(parser.parse_expression("0", zero_expr));
    
    auto zero_result = checker.validate_static_assignment(zero_expr, uint_type);
    EXPECT_TRUE(zero_result.has_value());
    EXPECT_TRUE(zero_result.value());
    
    ast::expression positive_expr;
    ASSERT_TRUE(parser.parse_expression("100", positive_expr));
    
    auto positive_result = checker.validate_static_assignment(positive_expr, uint_type);
    EXPECT_TRUE(positive_result.has_value());
    EXPECT_TRUE(positive_result.value());
    
    // Invalid negative
    ast::expression negative_expr;
    ASSERT_TRUE(parser.parse_expression("-1", negative_expr));
    
    auto negative_result = checker.validate_static_assignment(negative_expr, uint_type);
    EXPECT_FALSE(negative_result.has_value());
    EXPECT_TRUE(negative_result.error().find("refinement constraint") != std::string::npos);
}

// Test compile-time validation of string literals
TEST_F(RefinementCheckerTest, ValidateNonEmptyStringLiterals) {
    auto non_empty_string = RefinementTypes::create_non_empty_string();
    
    // Valid non-empty string
    ast::expression valid_expr;
    ASSERT_TRUE(parser.parse_expression("\"hello\"", valid_expr));
    
    auto result = checker.validate_static_assignment(valid_expr, non_empty_string);
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(result.value());
    
    // Invalid empty string
    ast::expression empty_expr;
    ASSERT_TRUE(parser.parse_expression("\"\"", empty_expr));
    
    auto empty_result = checker.validate_static_assignment(empty_expr, non_empty_string);
    EXPECT_FALSE(empty_result.has_value());
    EXPECT_TRUE(empty_result.error().find("refinement constraint") != std::string::npos);
}

// Test predicate analysis for simple comparisons
TEST_F(RefinementCheckerTest, AnalyzeSimpleComparison) {
    ast::expression predicate_expr;
    ASSERT_TRUE(parser.parse_expression("it > 0", predicate_expr));
    
    auto analysis = checker.analyze_predicate(predicate_expr);
    
    EXPECT_TRUE(analysis.is_statically_analyzable);
    EXPECT_EQ(analysis.type, RefinementChecker::PredicateAnalysis::PredicateType::COMPARISON);
    EXPECT_EQ(analysis.operator_symbol, ">");
    EXPECT_TRUE(analysis.comparison_value.is<meld::kernel::Integer>());
    EXPECT_EQ(analysis.comparison_value.as<meld::kernel::Integer>()->value(), 0);
}

// Test predicate analysis for range checks
TEST_F(RefinementCheckerTest, AnalyzeRangeCheck) {
    ast::expression predicate_expr;
    ASSERT_TRUE(parser.parse_expression("it >= 0 && it <= 100", predicate_expr));
    
    auto analysis = checker.analyze_predicate(predicate_expr);
    
    EXPECT_TRUE(analysis.is_statically_analyzable);
    EXPECT_EQ(analysis.type, RefinementChecker::PredicateAnalysis::PredicateType::RANGE_CHECK);
    EXPECT_TRUE(analysis.has_min);
    EXPECT_TRUE(analysis.has_max);
    EXPECT_EQ(analysis.min_value.as<meld::kernel::Integer>()->value(), 0);
    EXPECT_EQ(analysis.max_value.as<meld::kernel::Integer>()->value(), 100);
}

// Test compile-time constant detection
TEST_F(RefinementCheckerTest, DetectCompileTimeConstants) {
    ast::expression literal_expr;
    ASSERT_TRUE(parser.parse_expression("42", literal_expr));
    EXPECT_TRUE(checker.is_compile_time_constant(literal_expr));
    
    ast::expression string_expr;
    ASSERT_TRUE(parser.parse_expression("\"hello\"", string_expr));
    EXPECT_TRUE(checker.is_compile_time_constant(string_expr));
    
    ast::expression bool_expr;
    ASSERT_TRUE(parser.parse_expression("true", bool_expr));
    EXPECT_TRUE(checker.is_compile_time_constant(bool_expr));
    
    // Arithmetic on constants should be constant
    ast::expression arithmetic_expr;
    ASSERT_TRUE(parser.parse_expression("2 + 3", arithmetic_expr));
    EXPECT_TRUE(checker.is_compile_time_constant(arithmetic_expr));
    
    // Variable references are not constants
    ast::expression variable_expr;
    ASSERT_TRUE(parser.parse_expression("x", variable_expr));
    EXPECT_FALSE(checker.is_compile_time_constant(variable_expr));
}

// Test constant evaluation
TEST_F(RefinementCheckerTest, EvaluateConstants) {
    ast::expression literal_expr;
    ASSERT_TRUE(parser.parse_expression("42", literal_expr));
    
    auto result = checker.evaluate_constant(literal_expr);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->is<meld::kernel::Integer>());
    EXPECT_EQ(result->as<meld::kernel::Integer>()->value(), 42);
    
    // Test string evaluation
    ast::expression string_expr;
    ASSERT_TRUE(parser.parse_expression("\"test\"", string_expr));
    
    auto string_result = checker.evaluate_constant(string_expr);
    ASSERT_TRUE(string_result.has_value());
    EXPECT_TRUE(string_result->is<meld::kernel::String>());
    EXPECT_EQ(string_result->as<meld::kernel::String>()->value(), "test");
    
    // Test boolean evaluation
    ast::expression bool_expr;
    ASSERT_TRUE(parser.parse_expression("true", bool_expr));
    
    auto bool_result = checker.evaluate_constant(bool_expr);
    ASSERT_TRUE(bool_result.has_value());
    EXPECT_TRUE(bool_result->is<meld::kernel::Boolean>());
    EXPECT_TRUE(bool_result->as<meld::kernel::Boolean>()->value());
}

// Test arithmetic constant evaluation
TEST_F(RefinementCheckerTest, EvaluateArithmeticConstants) {
    ast::expression add_expr;
    ASSERT_TRUE(parser.parse_expression("2 + 3", add_expr));
    
    auto result = checker.evaluate_constant(add_expr);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->is<meld::kernel::Integer>());
    EXPECT_EQ(result->as<meld::kernel::Integer>()->value(), 5);
    
    ast::expression mult_expr;
    ASSERT_TRUE(parser.parse_expression("4 * 5", mult_expr));
    
    auto mult_result = checker.evaluate_constant(mult_expr);
    ASSERT_TRUE(mult_result.has_value());
    EXPECT_TRUE(mult_result->is<meld::kernel::Integer>());
    EXPECT_EQ(mult_result->as<meld::kernel::Integer>()->value(), 20);
}

// Test error message generation
TEST_F(RefinementCheckerTest, GenerateViolationMessages) {
    auto positive_int = RefinementTypes::create_positive_int();
    
    ast::expression invalid_expr;
    ASSERT_TRUE(parser.parse_expression("-5", invalid_expr));
    
    std::string message = checker.generate_violation_message(
        invalid_expr, 
        positive_int, 
        "Value is negative"
    );
    
    EXPECT_TRUE(message.find("positive_int") != std::string::npos);
    EXPECT_TRUE(message.find("Value is negative") != std::string::npos);
}

// Test literal validation against predicates
TEST_F(RefinementCheckerTest, ValidateLiteralAgainstPredicate) {
    // Create a simple comparison analysis
    RefinementChecker::PredicateAnalysis analysis;
    analysis.is_statically_analyzable = true;
    analysis.type = RefinementChecker::PredicateAnalysis::PredicateType::COMPARISON;
    analysis.operator_symbol = ">";
    analysis.comparison_value = meld::kernel::Value(std::make_shared<meld::kernel::Integer>(0));
    
    // Test valid literal
    ast::expression valid_literal;
    ASSERT_TRUE(parser.parse_expression("5", valid_literal));
    
    auto valid_result = checker.validate_literal_value(valid_literal, analysis);
    EXPECT_TRUE(valid_result.has_value());
    EXPECT_TRUE(valid_result.value());
    
    // Test invalid literal
    ast::expression invalid_literal;
    ASSERT_TRUE(parser.parse_expression("-1", invalid_literal));
    
    auto invalid_result = checker.validate_literal_value(invalid_literal, analysis);
    EXPECT_TRUE(invalid_result.has_value());
    EXPECT_FALSE(invalid_result.value());
}

// Test range validation
TEST_F(RefinementCheckerTest, ValidateRangeConstraints) {
    // Create a range analysis (0 <= it <= 100)
    RefinementChecker::PredicateAnalysis analysis;
    analysis.is_statically_analyzable = true;
    analysis.type = RefinementChecker::PredicateAnalysis::PredicateType::RANGE_CHECK;
    analysis.has_min = true;
    analysis.has_max = true;
    analysis.min_value = meld::kernel::Value(std::make_shared<meld::kernel::Integer>(0));
    analysis.max_value = meld::kernel::Value(std::make_shared<meld::kernel::Integer>(100));
    
    // Test valid value in range
    ast::expression valid_literal;
    ASSERT_TRUE(parser.parse_expression("50", valid_literal));
    
    auto valid_result = checker.validate_literal_value(valid_literal, analysis);
    EXPECT_TRUE(valid_result.has_value());
    EXPECT_TRUE(valid_result.value());
    
    // Test value below range
    ast::expression below_literal;
    ASSERT_TRUE(parser.parse_expression("-1", below_literal));
    
    auto below_result = checker.validate_literal_value(below_literal, analysis);
    EXPECT_TRUE(below_result.has_value());
    EXPECT_FALSE(below_result.value());
    
    // Test value above range
    ast::expression above_literal;
    ASSERT_TRUE(parser.parse_expression("101", above_literal));
    
    auto above_result = checker.validate_literal_value(above_literal, analysis);
    EXPECT_TRUE(above_result.has_value());
    EXPECT_FALSE(above_result.value());
}