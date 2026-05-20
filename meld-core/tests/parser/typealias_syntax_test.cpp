#include <gtest/gtest.h>
#include "meld/parser/parser.hpp"
#include "meld/parser/ast.hpp"

using namespace meld::parser;
namespace x3 = boost::spirit::x3;

// Test parsing type alias with arrow syntax: type Name -> Type
TEST(TypeAliasSyntaxTest, ArrowSyntaxBasic) {
    std::string source = "type Point -> int";
    
    Parser parser(source);
    auto result = parser.parse();
    
    ASSERT_TRUE(result.has_value()) << "Parse error: " << parser.error_message();
    
    auto& expressions = *result;
    ASSERT_EQ(expressions.size(), 1);
    
    // Check that we got a typealias_declaration
    auto* decl = boost::get<x3::forward_ast<ast::typealias_declaration>>(&expressions[0]);
    ASSERT_NE(decl, nullptr);
    EXPECT_EQ(decl->get().alias_name.name, "Point");
    EXPECT_EQ(decl->get().target_type.type_name.name, "int");
}

// Test parsing type alias with old syntax: typealias Name = Type
TEST(TypeAliasSyntaxTest, OldSyntaxBasic) {
    std::string source = "typealias Coordinate = int";
    
    Parser parser(source);
    auto result = parser.parse();
    
    ASSERT_TRUE(result.has_value()) << "Parse error: " << parser.error_message();
    
    auto& expressions = *result;
    ASSERT_EQ(expressions.size(), 1);
    
    // Check that we got a typealias_declaration
    auto* decl = boost::get<x3::forward_ast<ast::typealias_declaration>>(&expressions[0]);
    ASSERT_NE(decl, nullptr);
    EXPECT_EQ(decl->get().alias_name.name, "Coordinate");
    EXPECT_EQ(decl->get().target_type.type_name.name, "int");
}

// Test parsing type alias with nullable type
TEST(TypeAliasSyntaxTest, ArrowSyntaxNullable) {
    std::string source = "type OptionalInt -> int?";
    
    Parser parser(source);
    auto result = parser.parse();
    
    ASSERT_TRUE(result.has_value()) << "Parse error: " << parser.error_message();
    
    auto& expressions = *result;
    ASSERT_EQ(expressions.size(), 1);
    
    auto* decl = boost::get<x3::forward_ast<ast::typealias_declaration>>(&expressions[0]);
    ASSERT_NE(decl, nullptr);
    EXPECT_EQ(decl->get().alias_name.name, "OptionalInt");
    EXPECT_EQ(decl->get().target_type.type_name.name, "int");
    EXPECT_TRUE(decl->get().target_type.is_nullable);
}

// Test parsing type alias with union type
TEST(TypeAliasSyntaxTest, ArrowSyntaxUnion) {
    std::string source = "type IntOrString -> int | string";
    
    Parser parser(source);
    auto result = parser.parse();
    
    ASSERT_TRUE(result.has_value()) << "Parse error: " << parser.error_message();
    
    auto& expressions = *result;
    ASSERT_EQ(expressions.size(), 1);
    
    auto* decl = boost::get<x3::forward_ast<ast::typealias_declaration>>(&expressions[0]);
    ASSERT_NE(decl, nullptr);
    EXPECT_EQ(decl->get().alias_name.name, "IntOrString");
    EXPECT_TRUE(decl->get().target_type.is_union);
}

// Test parsing type alias with intersection type
TEST(TypeAliasSyntaxTest, ArrowSyntaxIntersection) {
    std::string source = "type DrawableClickable -> Drawable & Clickable";
    
    Parser parser(source);
    auto result = parser.parse();
    
    ASSERT_TRUE(result.has_value()) << "Parse error: " << parser.error_message();
    
    auto& expressions = *result;
    ASSERT_EQ(expressions.size(), 1);
    
    auto* decl = boost::get<x3::forward_ast<ast::typealias_declaration>>(&expressions[0]);
    ASSERT_NE(decl, nullptr);
    EXPECT_EQ(decl->get().alias_name.name, "DrawableClickable");
    EXPECT_TRUE(decl->get().target_type.is_intersection);
}

// Test parsing type alias with generic type
TEST(TypeAliasSyntaxTest, ArrowSyntaxGeneric) {
    std::string source = "type IntList -> List<int>";
    
    Parser parser(source);
    auto result = parser.parse();
    
    ASSERT_TRUE(result.has_value()) << "Parse error: " << parser.error_message();
    
    auto& expressions = *result;
    ASSERT_EQ(expressions.size(), 1);
    
    auto* decl = boost::get<x3::forward_ast<ast::typealias_declaration>>(&expressions[0]);
    ASSERT_NE(decl, nullptr);
    EXPECT_EQ(decl->get().alias_name.name, "IntList");
    EXPECT_EQ(decl->get().target_type.type_name.name, "List");
    EXPECT_TRUE(decl->get().target_type.has_type_arguments);
}

// Test parsing multiple type aliases
TEST(TypeAliasSyntaxTest, MultipleAliases) {
    std::string source = R"(
        type Point -> int
        type Coordinate -> float
        typealias Distance = float
    )";
    
    Parser parser(source);
    auto result = parser.parse();
    
    ASSERT_TRUE(result.has_value()) << "Parse error: " << parser.error_message();
    
    auto& expressions = *result;
    ASSERT_EQ(expressions.size(), 3);
    
    // Check first alias (arrow syntax)
    auto* decl1 = boost::get<x3::forward_ast<ast::typealias_declaration>>(&expressions[0]);
    ASSERT_NE(decl1, nullptr);
    EXPECT_EQ(decl1->get().alias_name.name, "Point");
    
    // Check second alias (arrow syntax)
    auto* decl2 = boost::get<x3::forward_ast<ast::typealias_declaration>>(&expressions[1]);
    ASSERT_NE(decl2, nullptr);
    EXPECT_EQ(decl2->get().alias_name.name, "Coordinate");
    
    // Check third alias (old syntax)
    auto* decl3 = boost::get<x3::forward_ast<ast::typealias_declaration>>(&expressions[2]);
    ASSERT_NE(decl3, nullptr);
    EXPECT_EQ(decl3->get().alias_name.name, "Distance");
}

// Test that 'type' keyword doesn't interfere with other uses
TEST(TypeAliasSyntaxTest, TypeKeywordDisambiguation) {
    // This should parse as a type alias
    std::string source1 = "type MyType -> int";
    Parser parser1(source1);
    auto result1 = parser1.parse();
    ASSERT_TRUE(result1.has_value()) << "Parse error: " << parser1.error_message();
    
    auto* decl = boost::get<x3::forward_ast<ast::typealias_declaration>>(&(*result1)[0]);
    ASSERT_NE(decl, nullptr);
}

// Test kebab-case in type alias names
TEST(TypeAliasSyntaxTest, KebabCaseAliasName) {
    std::string source = "type my-custom-type -> int";
    
    Parser parser(source);
    auto result = parser.parse();
    
    ASSERT_TRUE(result.has_value()) << "Parse error: " << parser.error_message();
    
    auto& expressions = *result;
    ASSERT_EQ(expressions.size(), 1);
    
    auto* decl = boost::get<x3::forward_ast<ast::typealias_declaration>>(&expressions[0]);
    ASSERT_NE(decl, nullptr);
    EXPECT_EQ(decl->get().alias_name.name, "my-custom-type");
}

// Validates: Requirements 14.8
TEST(TypeAliasSyntaxTest, RequirementValidation) {
    // Requirement 14.8: THE Language Runtime SHALL support type aliases using -> syntax
    
    std::string source = "type Handler -> (string) => bool";
    
    Parser parser(source);
    auto result = parser.parse();
    
    ASSERT_TRUE(result.has_value()) << "Parse error: " << parser.error_message();
    
    auto& expressions = *result;
    ASSERT_EQ(expressions.size(), 1);
    
    auto* decl = boost::get<x3::forward_ast<ast::typealias_declaration>>(&expressions[0]);
    ASSERT_NE(decl, nullptr);
    EXPECT_EQ(decl->get().alias_name.name, "Handler");
    
    // Validates: Requirements 14.8
}
