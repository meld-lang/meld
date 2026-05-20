#include <gtest/gtest.h>
#include "meld/macro/operator_macro.hpp"
#include "meld/macro/operator_annotations.hpp"
#include "meld/macro/macro.hpp"
#include "meld/parser/custom_operator_parser.hpp"
#include "meld/kernel/symbol_table.hpp"
#include "meld/kernel/primitives.hpp"

using namespace meld::macro;
using namespace meld::parser;
using namespace meld::kernel;

class OperatorMacroTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Register the operator macro
        register_operator_macro();
        register_operator_annotation_processors();
    }
    
    void TearDown() override {
        // Clear macro registry
        MacroRegistry::instance().clear();
    }
};

TEST_F(OperatorMacroTest, RegisterOperatorMacro) {
    auto& registry = MacroRegistry::instance();
    
    EXPECT_TRUE(registry.has_macro("opr"));
    
    auto macro_result = registry.get_macro("opr");
    ASSERT_TRUE(macro_result.has_value());
    
    auto macro = macro_result.value();
    EXPECT_EQ(macro->name(), "opr");
    EXPECT_EQ(macro->params().size(), 3);
    EXPECT_EQ(macro->params()[0], "symbol");
    EXPECT_EQ(macro->params()[1], "params");
    EXPECT_EQ(macro->params()[2], "body");
}

TEST_F(OperatorMacroTest, MangleOperatorNames) {
    EXPECT_EQ(OperatorMacro::mangle_operator_name("+"), "__op_add__");
    EXPECT_EQ(OperatorMacro::mangle_operator_name("-"), "__op_sub__");
    EXPECT_EQ(OperatorMacro::mangle_operator_name("*"), "__op_mul__");
    EXPECT_EQ(OperatorMacro::mangle_operator_name("/"), "__op_div__");
    EXPECT_EQ(OperatorMacro::mangle_operator_name("=="), "__op_eq_eq__");
    EXPECT_EQ(OperatorMacro::mangle_operator_name("!="), "__op_not_eq__");
    EXPECT_EQ(OperatorMacro::mangle_operator_name("..."), "__op_ellipsis__");  // Ellipsis operator
}

TEST_F(OperatorMacroTest, ParseOperatorSymbol) {
    auto result1 = OperatorMacro::parse_operator_symbol("__op_add__");
    ASSERT_TRUE(result1.has_value());
    EXPECT_EQ(result1.value(), "add");
    
    auto result2 = OperatorMacro::parse_operator_symbol("__op_mul__");
    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ(result2.value(), "mul");
    
    auto result3 = OperatorMacro::parse_operator_symbol("invalid_name");
    EXPECT_FALSE(result3.has_value());
}

TEST_F(OperatorMacroTest, ValidateOperatorSymbols) {
    // Valid symbols
    EXPECT_TRUE(OperatorMacro::is_valid_operator_symbol("+"));
    EXPECT_TRUE(OperatorMacro::is_valid_operator_symbol("-"));
    EXPECT_TRUE(OperatorMacro::is_valid_operator_symbol("*"));
    EXPECT_TRUE(OperatorMacro::is_valid_operator_symbol("=="));
    EXPECT_TRUE(OperatorMacro::is_valid_operator_symbol("!="));
    EXPECT_TRUE(OperatorMacro::is_valid_operator_symbol("..."));  // Ellipsis operator
    
    // Invalid symbols
    EXPECT_FALSE(OperatorMacro::is_valid_operator_symbol(""));
    EXPECT_FALSE(OperatorMacro::is_valid_operator_symbol("invalid"));
    EXPECT_FALSE(OperatorMacro::is_valid_operator_symbol("@#$"));
}

TEST_F(OperatorMacroTest, ApplyOperatorMacro) {
    auto& registry = MacroRegistry::instance();
    auto macro_result = registry.get_macro("opr");
    ASSERT_TRUE(macro_result.has_value());
    
    auto macro = macro_result.value();
    MacroExpander expander;
    
    // Create AST for: (opr + (a b) (+ a b))
    auto& symbol_table = SymbolTable::instance();
    auto opr_sym = symbol_table.intern("opr");
    auto plus_sym = symbol_table.intern("+");
    auto a_sym = symbol_table.intern("a");
    auto b_sym = symbol_table.intern("b");
    
    // Build parameter list: (a b)
    auto b_cons = Value(std::make_shared<Cons>(Value(b_sym), nil()));
    auto params = Value(std::make_shared<Cons>(Value(a_sym), b_cons));
    
    // Build body: (+ a b)
    auto body_b = Value(std::make_shared<Cons>(Value(b_sym), nil()));
    auto body_a = Value(std::make_shared<Cons>(Value(a_sym), body_b));
    auto body = Value(std::make_shared<Cons>(Value(plus_sym), body_a));
    
    // Build full expression: (opr + (a b) (+ a b))
    auto body_cons = Value(std::make_shared<Cons>(body, nil()));
    auto params_cons = Value(std::make_shared<Cons>(params, body_cons));
    auto symbol_cons = Value(std::make_shared<Cons>(Value(plus_sym), params_cons));
    auto ast_node = Value(std::make_shared<Cons>(Value(opr_sym), symbol_cons));
    
    // Apply the macro
    auto result = macro->apply(ast_node, expander);
    ASSERT_TRUE(result.has_value());
    
    // Verify the result is a function definition
    auto result_cons = result.value().try_as<Cons>();
    ASSERT_TRUE(result_cons.has_value());
    
    auto head = (*result_cons)->head();
    auto fnc_symbol = head.try_as<Symbol>();
    ASSERT_TRUE(fnc_symbol.has_value());
    EXPECT_EQ((*fnc_symbol)->name(), "fnc");
}

TEST_F(OperatorMacroTest, DetermineOperatorType) {
    // Unary operators
    EXPECT_EQ(OperatorMacro::determine_operator_type("-", 1), OperatorType::PREFIX);
    EXPECT_EQ(OperatorMacro::determine_operator_type("!", 1), OperatorType::PREFIX);
    EXPECT_EQ(OperatorMacro::determine_operator_type("++", 1), OperatorType::POSTFIX);
    EXPECT_EQ(OperatorMacro::determine_operator_type("--", 1), OperatorType::POSTFIX);
    
    // Binary operators
    EXPECT_EQ(OperatorMacro::determine_operator_type("+", 2), OperatorType::INFIX);
    EXPECT_EQ(OperatorMacro::determine_operator_type("*", 2), OperatorType::INFIX);
    EXPECT_EQ(OperatorMacro::determine_operator_type("==", 2), OperatorType::INFIX);
}

// Test @infix annotation parsing
TEST_F(OperatorMacroTest, ParseInfixAnnotation) {
    // Test basic @infix annotation
    std::string annotation1 = "@infix(precedence=10, assoc=left)";
    auto result1 = OperatorAnnotationParser::parse_infix_annotation(annotation1);
    
    ASSERT_TRUE(result1.has_value());
    EXPECT_EQ(result1->precedence, 10);
    EXPECT_EQ(result1->associativity, Associativity::LEFT);
    
    // Test @infix annotation with right associativity
    std::string annotation2 = "@infix(precedence=25, assoc=right)";
    auto result2 = OperatorAnnotationParser::parse_infix_annotation(annotation2);
    
    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ(result2->precedence, 25);
    EXPECT_EQ(result2->associativity, Associativity::RIGHT);
    
    // Test @infix annotation with default values
    std::string annotation3 = "@infix()";
    auto result3 = OperatorAnnotationParser::parse_infix_annotation(annotation3);
    
    ASSERT_TRUE(result3.has_value());
    EXPECT_EQ(result3->precedence, 5);  // Default precedence
    EXPECT_EQ(result3->associativity, Associativity::LEFT);  // Default associativity
}

// Test @prefix annotation parsing
TEST_F(OperatorMacroTest, ParsePrefixAnnotation) {
    // Test basic @prefix annotation
    std::string annotation1 = "@prefix(precedence=80)";
    auto result1 = OperatorAnnotationParser::parse_prefix_annotation(annotation1);
    
    ASSERT_TRUE(result1.has_value());
    EXPECT_EQ(result1->precedence, 80);
    
    // Test @prefix annotation with default values
    std::string annotation2 = "@prefix()";
    auto result2 = OperatorAnnotationParser::parse_prefix_annotation(annotation2);
    
    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ(result2->precedence, 80);  // Default precedence for prefix
}

// Test @postfix annotation parsing
TEST_F(OperatorMacroTest, ParsePostfixAnnotation) {
    // Test basic @postfix annotation
    std::string annotation1 = "@postfix(precedence=90)";
    auto result1 = OperatorAnnotationParser::parse_postfix_annotation(annotation1);
    
    ASSERT_TRUE(result1.has_value());
    EXPECT_EQ(result1->precedence, 90);
    
    // Test @postfix annotation with default values
    std::string annotation2 = "@postfix()";
    auto result2 = OperatorAnnotationParser::parse_postfix_annotation(annotation2);
    
    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ(result2->precedence, 90);  // Default precedence for postfix
}

// Test operator annotation registry
TEST_F(OperatorMacroTest, OperatorAnnotationRegistry) {
    auto& registry = OperatorAnnotationRegistry::instance();
    
    // Register an infix operator
    InfixConfig infix_config(15, Associativity::LEFT);
    registry.register_infix_operator("**", infix_config);
    
    // Check if operator is registered
    EXPECT_TRUE(registry.has_operator("**"));
    
    // Get operator info
    auto info_result = registry.get_operator_info("**");
    ASSERT_TRUE(info_result.has_value());
    
    auto info = *info_result;
    EXPECT_EQ(info.symbol, "**");
    EXPECT_EQ(info.type, OperatorType::INFIX);
    EXPECT_EQ(info.precedence, 15);
    EXPECT_EQ(info.associativity, Associativity::LEFT);
}

// Test custom operator precedence table
TEST_F(OperatorMacroTest, CustomOperatorPrecedenceTable) {
    auto& table = CustomOperatorPrecedenceTable::instance();
    
    // Register a custom infix operator
    table.register_infix_operator("•", 20, Associativity::LEFT);
    
    // Check if operator is registered
    EXPECT_TRUE(table.has_operator("•"));
    
    // Get precedence
    auto precedence_result = table.get_precedence("•");
    ASSERT_TRUE(precedence_result.has_value());
    EXPECT_EQ(*precedence_result, 20);
    
    // Get associativity
    auto assoc_result = table.get_associativity("•");
    ASSERT_TRUE(assoc_result.has_value());
    EXPECT_EQ(*assoc_result, Associativity::LEFT);
    
    // Get operator type
    auto type_result = table.get_operator_type("•");
    ASSERT_TRUE(type_result.has_value());
    EXPECT_EQ(*type_result, OperatorType::INFIX);
}

// Test custom operator parser
TEST_F(OperatorMacroTest, CustomOperatorParser) {
    // Test built-in operator precedence
    EXPECT_EQ(CustomOperatorParser::get_operator_precedence("+"), 10);
    EXPECT_EQ(CustomOperatorParser::get_operator_precedence("*"), 20);
    EXPECT_EQ(CustomOperatorParser::get_operator_precedence("=="), 5);
    EXPECT_EQ(CustomOperatorParser::get_operator_precedence("..."), 35);  // Ellipsis operator
    
    // Test built-in operator associativity
    EXPECT_EQ(CustomOperatorParser::get_operator_associativity("+"), Associativity::LEFT);
    EXPECT_EQ(CustomOperatorParser::get_operator_associativity("="), Associativity::RIGHT);
    EXPECT_EQ(CustomOperatorParser::get_operator_associativity("..."), Associativity::RIGHT);  // Ellipsis operator
    
    // Test custom operator recognition
    auto& table = CustomOperatorPrecedenceTable::instance();
    table.register_infix_operator("~=", 6, Associativity::NONE);
    
    EXPECT_TRUE(CustomOperatorParser::is_custom_operator("~="));
    EXPECT_TRUE(CustomOperatorParser::is_custom_operator("..."));  // Ellipsis operator
    EXPECT_EQ(CustomOperatorParser::get_operator_precedence("~="), 6);
    EXPECT_EQ(CustomOperatorParser::get_operator_associativity("~="), Associativity::NONE);
}

// Test annotation type recognition
TEST_F(OperatorMacroTest, AnnotationTypeRecognition) {
    // Test valid annotation types
    auto infix_result = OperatorAnnotationParser::get_annotation_type("infix");
    ASSERT_TRUE(infix_result.has_value());
    EXPECT_EQ(*infix_result, OperatorAnnotationType::INFIX);
    
    auto prefix_result = OperatorAnnotationParser::get_annotation_type("prefix");
    ASSERT_TRUE(prefix_result.has_value());
    EXPECT_EQ(*prefix_result, OperatorAnnotationType::PREFIX);
    
    auto postfix_result = OperatorAnnotationParser::get_annotation_type("postfix");
    ASSERT_TRUE(postfix_result.has_value());
    EXPECT_EQ(*postfix_result, OperatorAnnotationType::POSTFIX);
    
    // Test invalid annotation type
    auto invalid_result = OperatorAnnotationParser::get_annotation_type("invalid");
    EXPECT_FALSE(invalid_result.has_value());
}

// Test associativity parsing
TEST_F(OperatorMacroTest, AssociativityParsing) {
    // Test valid associativity values
    auto left_result = OperatorAnnotationParser::parse_associativity("left");
    ASSERT_TRUE(left_result.has_value());
    EXPECT_EQ(*left_result, Associativity::LEFT);
    
    auto right_result = OperatorAnnotationParser::parse_associativity("right");
    ASSERT_TRUE(right_result.has_value());
    EXPECT_EQ(*right_result, Associativity::RIGHT);
    
    auto none_result = OperatorAnnotationParser::parse_associativity("none");
    ASSERT_TRUE(none_result.has_value());
    EXPECT_EQ(*none_result, Associativity::NONE);
    
    // Test invalid associativity value
    auto invalid_result = OperatorAnnotationParser::parse_associativity("invalid");
    EXPECT_FALSE(invalid_result.has_value());
}

// Test precedence ordering
TEST_F(OperatorMacroTest, PrecedenceOrdering) {
    auto& table = CustomOperatorPrecedenceTable::instance();
    
    // Register operators with different precedences
    table.register_infix_operator("**", 25, Associativity::RIGHT);  // Highest
    table.register_infix_operator("•", 20, Associativity::LEFT);    // High
    table.register_infix_operator("*", 15, Associativity::LEFT);    // Medium-high
    table.register_infix_operator("+", 10, Associativity::LEFT);    // Medium
    table.register_infix_operator("==", 6, Associativity::LEFT);    // Low-medium
    table.register_infix_operator("<|>", 2, Associativity::LEFT);   // Low
    
    // Verify precedence ordering
    EXPECT_GT(CustomOperatorParser::get_operator_precedence("**"), 
              CustomOperatorParser::get_operator_precedence("•"));
    EXPECT_GT(CustomOperatorParser::get_operator_precedence("•"), 
              CustomOperatorParser::get_operator_precedence("*"));
    EXPECT_GT(CustomOperatorParser::get_operator_precedence("*"), 
              CustomOperatorParser::get_operator_precedence("+"));
    EXPECT_GT(CustomOperatorParser::get_operator_precedence("+"), 
              CustomOperatorParser::get_operator_precedence("=="));
    EXPECT_GT(CustomOperatorParser::get_operator_precedence("=="), 
              CustomOperatorParser::get_operator_precedence("<|>"));
}