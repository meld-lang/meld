#include <gtest/gtest.h>
#include "meld/parser/parser.hpp"

using namespace meld::parser;
namespace x3 = boost::spirit::x3;

class RefinementSyntaxTest : public ::testing::Test {
protected:
    Parser parser;
};

// Test basic refinement type syntax parsing
TEST_F(RefinementSyntaxTest, BasicRefinementType) {
    std::string input = "type PositiveInt -> int where { it > 0 }";
    
    ast::expression result;
    bool success = parser.parse_expression(input, result);
    
    EXPECT_TRUE(success) << "Parser error: " << parser.error_message();
    
    // Check that we got a refinement type definition
    auto refinement = boost::get<x3::forward_ast<ast::refinement_type_definition>>(&result);
    ASSERT_NE(refinement, nullptr);
    
    EXPECT_EQ(refinement->get().name.name, "PositiveInt");
    EXPECT_EQ(refinement->get().base_type.type_name.name, "int");
    EXPECT_FALSE(refinement->get().base_type.is_nullable);
}

// Test uint refinement type
TEST_F(RefinementSyntaxTest, UintRefinementType) {
    std::string input = "type uint -> int where { it >= 0 }";
    
    ast::expression result;
    bool success = parser.parse_expression(input, result);
    
    EXPECT_TRUE(success) << "Parser error: " << parser.error_message();
    
    auto refinement = boost::get<x3::forward_ast<ast::refinement_type_definition>>(&result);
    ASSERT_NE(refinement, nullptr);
    
    EXPECT_EQ(refinement->get().name.name, "uint");
    EXPECT_EQ(refinement->get().base_type.type_name.name, "int");
}

// Test non-empty string refinement type
TEST_F(RefinementSyntaxTest, NonEmptyStringRefinementType) {
    std::string input = "type NonEmptyString -> string where { it.length > 0 }";
    
    ast::expression result;
    bool success = parser.parse_expression(input, result);
    
    EXPECT_TRUE(success) << "Parser error: " << parser.error_message();
    
    auto refinement = boost::get<x3::forward_ast<ast::refinement_type_definition>>(&result);
    ASSERT_NE(refinement, nullptr);
    
    EXPECT_EQ(refinement->get().name.name, "NonEmptyString");
    EXPECT_EQ(refinement->get().base_type.type_name.name, "string");
}

// Test email refinement type with complex predicate
TEST_F(RefinementSyntaxTest, EmailRefinementType) {
    std::string input = "type Email -> string where { it.matches(/^[a-z]+@[a-z]+\\.[a-z]+$/) }";
    
    ast::expression result;
    bool success = parser.parse_expression(input, result);
    
    EXPECT_TRUE(success) << "Parser error: " << parser.error_message();
    
    auto refinement = boost::get<x3::forward_ast<ast::refinement_type_definition>>(&result);
    ASSERT_NE(refinement, nullptr);
    
    EXPECT_EQ(refinement->get().name.name, "Email");
    EXPECT_EQ(refinement->get().base_type.type_name.name, "string");
}

// Test ValidChar refinement type
TEST_F(RefinementSyntaxTest, ValidCharRefinementType) {
    std::string input = "type ValidChar -> int where { it >= 0 && it <= 0x10FFFF && !(it >= 0xD800 && it <= 0xDFFF) }";
    
    ast::expression result;
    bool success = parser.parse_expression(input, result);
    
    EXPECT_TRUE(success) << "Parser error: " << parser.error_message();
    
    auto refinement = boost::get<x3::forward_ast<ast::refinement_type_definition>>(&result);
    ASSERT_NE(refinement, nullptr);
    
    EXPECT_EQ(refinement->get().name.name, "ValidChar");
    EXPECT_EQ(refinement->get().base_type.type_name.name, "int");
}

// Test regular type alias (without where clause) still works
TEST_F(RefinementSyntaxTest, RegularTypeAlias) {
    std::string input = "type MyInt -> int";
    
    ast::expression result;
    bool success = parser.parse_expression(input, result);
    
    EXPECT_TRUE(success) << "Parser error: " << parser.error_message();
    
    // Should be a regular type alias, not a refinement type
    auto alias = boost::get<x3::forward_ast<ast::typealias_declaration>>(&result);
    ASSERT_NE(alias, nullptr);
    
    EXPECT_EQ(alias->get().alias_name.name, "MyInt");
    EXPECT_EQ(alias->get().target_type.type_name.name, "int");
}

// Test refinement type with nullable base type
TEST_F(RefinementSyntaxTest, RefinementWithNullableBase) {
    std::string input = "type NonNullString -> string? where { it != null }";
    
    ast::expression result;
    bool success = parser.parse_expression(input, result);
    
    EXPECT_TRUE(success) << "Parser error: " << parser.error_message();
    
    auto refinement = boost::get<x3::forward_ast<ast::refinement_type_definition>>(&result);
    ASSERT_NE(refinement, nullptr);
    
    EXPECT_EQ(refinement->get().name.name, "NonNullString");
    EXPECT_EQ(refinement->get().base_type.type_name.name, "string");
    EXPECT_TRUE(refinement->get().base_type.is_nullable);
}

// Test refinement type with generic base type
TEST_F(RefinementSyntaxTest, RefinementWithGenericBase) {
    std::string input = "type NonEmptyList -> List<T> where { it.size > 0 }";
    
    ast::expression result;
    bool success = parser.parse_expression(input, result);
    
    EXPECT_TRUE(success) << "Parser error: " << parser.error_message();
    
    auto refinement = boost::get<x3::forward_ast<ast::refinement_type_definition>>(&result);
    ASSERT_NE(refinement, nullptr);
    
    EXPECT_EQ(refinement->get().name.name, "NonEmptyList");
    EXPECT_EQ(refinement->get().base_type.type_name.name, "List");
    EXPECT_TRUE(refinement->get().base_type.has_type_arguments);
}

// Test error case: missing where clause braces
TEST_F(RefinementSyntaxTest, MissingWhereBraces) {
    std::string input = "type PositiveInt -> int where it > 0";
    
    ast::expression result;
    bool success = parser.parse_expression(input, result);
    
    EXPECT_FALSE(success);
    EXPECT_TRUE(parser.error_message().find("Expected '{'") != std::string::npos);
}

// Test error case: missing closing brace
TEST_F(RefinementSyntaxTest, MissingClosingBrace) {
    std::string input = "type PositiveInt -> int where { it > 0";
    
    ast::expression result;
    bool success = parser.parse_expression(input, result);
    
    EXPECT_FALSE(success);
    EXPECT_TRUE(parser.error_message().find("Expected '}'") != std::string::npos);
}
