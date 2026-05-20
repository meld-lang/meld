#include <gtest/gtest.h>
#include "meld/parser/parser.hpp"
#include "meld/compiler/type_checker.hpp"
#include "meld/meta/metatype.hpp"

using namespace meld::parser;
using namespace meld::compiler;
using namespace meld::meta;

// Test type alias registration in type checker
TEST(TypeAliasCheckerTest, BasicAliasRegistration) {
    auto& registry = TypeRegistry::instance();
    
    std::string source = "type MyInt -> int";
    
    Parser parser;
std::vector<ast::expression> _exprs;
    bool parse_result_ok = parser.parse_file(source, _exprs);
    ASSERT_TRUE(parse_result_ok);
    
    TypeChecker checker;
    auto check_result = checker.check_program(_exprs);
    
    ASSERT_TRUE(check_result.has_value()) << "Type check failed";
    
    // Verify the alias was registered
    EXPECT_TRUE(registry.is_alias("MyInt"));
    
    // Verify we can resolve the alias
    auto resolved = registry.resolve_alias("MyInt");
    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ((*resolved)->name(), "Int");
}

// Test type alias with old syntax
TEST(TypeAliasCheckerTest, OldSyntaxRegistration) {
    auto& registry = TypeRegistry::instance();
    
    std::string source = "typealias MyString = string";
    
    Parser parser;
std::vector<ast::expression> _exprs;
    bool parse_result_ok = parser.parse_file(source, _exprs);
    ASSERT_TRUE(parse_result_ok);
    
    TypeChecker checker;
    auto check_result = checker.check_program(_exprs);
    
    ASSERT_TRUE(check_result.has_value()) << "Type check failed";
    
    // Verify the alias was registered
    EXPECT_TRUE(registry.is_alias("MyString"));
    
    // Verify we can resolve the alias
    auto resolved = registry.resolve_alias("MyString");
    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ((*resolved)->name(), "String");
}

// Test type alias usage in variable declaration
TEST(TypeAliasCheckerTest, AliasUsageInDeclaration) {
    auto& registry = TypeRegistry::instance();
    
    std::string source = R"(
        type MyInt -> int
        val x: MyInt = 42
    )";
    
    Parser parser;
std::vector<ast::expression> _exprs;
    bool parse_result_ok = parser.parse_file(source, _exprs);
    ASSERT_TRUE(parse_result_ok);
    
    TypeChecker checker;
    auto check_result = checker.check_program(_exprs);
    
    ASSERT_TRUE(check_result.has_value()) << "Type check failed";
    
    // Verify the alias was registered and used correctly
    EXPECT_TRUE(registry.is_alias("MyInt"));
}

// Test type alias with nullable type
TEST(TypeAliasCheckerTest, NullableTypeAlias) {
    auto& registry = TypeRegistry::instance();
    
    std::string source = "type OptionalInt -> int?";
    
    Parser parser;
std::vector<ast::expression> _exprs;
    bool parse_result_ok = parser.parse_file(source, _exprs);
    ASSERT_TRUE(parse_result_ok);
    
    TypeChecker checker;
    auto check_result = checker.check_program(_exprs);
    
    ASSERT_TRUE(check_result.has_value()) << "Type check failed";
    
    // Verify the alias was registered
    EXPECT_TRUE(registry.is_alias("OptionalInt"));
    
    // Verify the resolved type is nullable
    auto resolved = registry.resolve_alias("OptionalInt");
    ASSERT_TRUE(resolved.has_value());
    EXPECT_TRUE(registry.is_nullable_type(**resolved));
}

// Test type alias with union type
TEST(TypeAliasCheckerTest, UnionTypeAlias) {
    auto& registry = TypeRegistry::instance();
    
    std::string source = "type IntOrString -> int | string";
    
    Parser parser;
std::vector<ast::expression> _exprs;
    bool parse_result_ok = parser.parse_file(source, _exprs);
    ASSERT_TRUE(parse_result_ok);
    
    TypeChecker checker;
    auto check_result = checker.check_program(_exprs);
    
    ASSERT_TRUE(check_result.has_value()) << "Type check failed";
    
    // Verify the alias was registered
    EXPECT_TRUE(registry.is_alias("IntOrString"));
    
    // Verify the resolved type is a union
    auto resolved = registry.resolve_alias("IntOrString");
    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ((*resolved)->name(), "Int | String");
}

// Test type alias with intersection type
TEST(TypeAliasCheckerTest, IntersectionTypeAlias) {
    auto& registry = TypeRegistry::instance();
    
    // First create the traits
    auto drawable = MetaType::create_trait("Drawable", {});
    auto clickable = MetaType::create_trait("Clickable", {});
    registry.register_type("Drawable", drawable);
    registry.register_type("Clickable", clickable);
    
    std::string source = "type DrawableClickable -> Drawable & Clickable";
    
    Parser parser;
std::vector<ast::expression> _exprs;
    bool parse_result_ok = parser.parse_file(source, _exprs);
    ASSERT_TRUE(parse_result_ok);
    
    TypeChecker checker;
    auto check_result = checker.check_program(_exprs);
    
    ASSERT_TRUE(check_result.has_value()) << "Type check failed";
    
    // Verify the alias was registered
    EXPECT_TRUE(registry.is_alias("DrawableClickable"));
    
    // Verify the resolved type is an intersection
    auto resolved = registry.resolve_alias("DrawableClickable");
    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ((*resolved)->name(), "Drawable & Clickable");
}

// Test chained type aliases
TEST(TypeAliasCheckerTest, ChainedAliases) {
    auto& registry = TypeRegistry::instance();
    
    std::string source = R"(
        type A -> int
        type B -> A
    )";
    
    Parser parser;
std::vector<ast::expression> _exprs;
    bool parse_result_ok = parser.parse_file(source, _exprs);
    ASSERT_TRUE(parse_result_ok);
    
    TypeChecker checker;
    auto check_result = checker.check_program(_exprs);
    
    ASSERT_TRUE(check_result.has_value()) << "Type check failed";
    
    // Verify both aliases were registered
    EXPECT_TRUE(registry.is_alias("A"));
    EXPECT_TRUE(registry.is_alias("B"));
    
    // Verify B resolves to A, which resolves to int
    auto resolved_b = registry.resolve_alias("B");
    ASSERT_TRUE(resolved_b.has_value());
    // B should resolve to A (which is an alias to int)
    // The get_type method should follow the chain
    auto final_type = registry.get_type("B");
    ASSERT_TRUE(final_type.has_value());
    EXPECT_EQ((*final_type)->name(), "Int");
}

// Test type alias with generic type
TEST(TypeAliasCheckerTest, GenericTypeAlias) {
    auto& registry = TypeRegistry::instance();
    
    std::string source = "type IntList -> List<int>";
    
    Parser parser;
std::vector<ast::expression> _exprs;
    bool parse_result_ok = parser.parse_file(source, _exprs);
    ASSERT_TRUE(parse_result_ok);
    
    TypeChecker checker;
    auto check_result = checker.check_program(_exprs);
    
    ASSERT_TRUE(check_result.has_value()) << "Type check failed";
    
    // Verify the alias was registered
    EXPECT_TRUE(registry.is_alias("IntList"));
}

// Test error handling for invalid target type
TEST(TypeAliasCheckerTest, InvalidTargetType) {
    std::string source = "type MyType -> NonExistentType";
    
    Parser parser;
std::vector<ast::expression> _exprs;
    bool parse_result_ok = parser.parse_file(source, _exprs);
    ASSERT_TRUE(parse_result_ok);
    
    TypeChecker checker;
    auto check_result = checker.check_program(_exprs);
    
    // Should fail because NonExistentType doesn't exist
    EXPECT_FALSE(check_result.has_value());
}

// Test multiple type aliases in one program
TEST(TypeAliasCheckerTest, MultipleAliases) {
    auto& registry = TypeRegistry::instance();
    
    std::string source = R"(
        type Point -> int
        type Coordinate -> float
        typealias Distance = float
    )";
    
    Parser parser;
std::vector<ast::expression> _exprs;
    bool parse_result_ok = parser.parse_file(source, _exprs);
    ASSERT_TRUE(parse_result_ok);
    
    TypeChecker checker;
    auto check_result = checker.check_program(_exprs);
    
    ASSERT_TRUE(check_result.has_value()) << "Type check failed";
    
    // Verify all aliases were registered
    EXPECT_TRUE(registry.is_alias("Point"));
    EXPECT_TRUE(registry.is_alias("Coordinate"));
    EXPECT_TRUE(registry.is_alias("Distance"));
}

// Validates: Requirements 14.8
TEST(TypeAliasCheckerTest, RequirementValidation) {
    // Requirement 14.8: THE Language Runtime SHALL support type aliases using -> syntax
    
    auto& registry = TypeRegistry::instance();
    
    std::string source = "type Handler -> (string) => bool";
    
    Parser parser;
std::vector<ast::expression> _exprs;
    bool parse_result_ok = parser.parse_file(source, _exprs);
    ASSERT_TRUE(parse_result_ok);
    
    TypeChecker checker;
    auto check_result = checker.check_program(_exprs);
    
    ASSERT_TRUE(check_result.has_value()) << "Type check failed";
    
    // Verify the alias was registered
    EXPECT_TRUE(registry.is_alias("Handler"));
    
    // Validates: Requirements 14.8
}
