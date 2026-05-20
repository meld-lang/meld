#include <gtest/gtest.h>
#include "meld/meta/metatype.hpp"

using namespace meld::meta;

// Test basic type alias
TEST(TypeAliasTest, BasicAlias) {
    auto& registry = TypeRegistry::instance();
    
    // typealias MyInt = Int
    registry.register_type_alias("MyInt", "Int");
    
    auto my_int = registry.get_type("MyInt");
    ASSERT_TRUE(my_int.has_value());
    EXPECT_EQ((*my_int)->name(), "Int");
    
    // Verify it's an alias
    EXPECT_TRUE(registry.is_alias("MyInt"));
    EXPECT_FALSE(registry.is_alias("Int"));
}

// Test alias to union type
TEST(TypeAliasTest, AliasToUnion) {
    auto& registry = TypeRegistry::instance();
    
    // typealias IntOrString = Int | String
    auto union_type = MetaType::create_union({
        registry.get_int_type(),
        registry.get_string_type()
    });
    
    registry.register_type_alias("IntOrString", union_type);
    
    auto result = registry.get_type("IntOrString");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)->name(), "Int | String");
}

// Test alias to intersection type
TEST(TypeAliasTest, AliasToIntersection) {
    auto& registry = TypeRegistry::instance();
    
    // typealias DrawableAndClickable = Drawable & Clickable
    auto drawable = MetaType::create_trait("Drawable", {});
    auto clickable = MetaType::create_trait("Clickable", {});
    auto intersection = MetaType::create_intersection({drawable, clickable});
    
    registry.register_type_alias("DrawableAndClickable", intersection);
    
    auto result = registry.get_type("DrawableAndClickable");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)->name(), "Drawable & Clickable");
}

// Test Optional type alias
TEST(TypeAliasTest, OptionalAlias) {
    auto& registry = TypeRegistry::instance();
    
    // typealias Optional<T> = T | Null
    // For this test, we'll create Optional<Int>
    auto optional_int = registry.create_optional_type(registry.get_int_type());
    registry.register_type_alias("OptionalInt", optional_int);
    
    auto result = registry.get_type("OptionalInt");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(registry.is_nullable_type(**result));
}

// Test Variant type alias
TEST(TypeAliasTest, VariantAlias) {
    auto& registry = TypeRegistry::instance();
    
    // typealias Variant<A, B, C> = A | B | C
    // For this test: Variant<Int, String, Bool>
    auto variant = MetaType::create_union({
        registry.get_int_type(),
        registry.get_string_type(),
        registry.get_bool_type()
    });
    
    registry.register_type_alias("MyVariant", variant);
    
    auto result = registry.get_type("MyVariant");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)->name(), "Int | String | Bool");
}

// Test alias to struct type
TEST(TypeAliasTest, AliasToStruct) {
    auto& registry = TypeRegistry::instance();
    
    // Create a Point struct
    std::vector<Field> fields = {
        Field("x", registry.get_int_type(), false),
        Field("y", registry.get_int_type(), false)
    };
    auto point_type = MetaType::create_struct("Point", std::move(fields));
    registry.register_type("Point", point_type);
    
    // typealias Coordinate = Point
    registry.register_type_alias("Coordinate", "Point");
    
    auto result = registry.get_type("Coordinate");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)->name(), "Point");
}

// Test alias to class type
TEST(TypeAliasTest, AliasToClass) {
    auto& registry = TypeRegistry::instance();
    
    // Create a Person class
    std::vector<Field> fields = {
        Field("name", registry.get_string_type(), true)
    };
    auto person_type = MetaType::create_class("Person", std::move(fields), {});
    registry.register_type("Person", person_type);
    
    // typealias User = Person
    registry.register_type_alias("User", "Person");
    
    auto result = registry.get_type("User");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)->name(), "Person");
}

// Test alias to generic type
TEST(TypeAliasTest, AliasToGenericType) {
    auto& registry = TypeRegistry::instance();
    
    // typealias IntArray = Array<Int>
    auto int_array = registry.create_array_type(registry.get_int_type());
    registry.register_type_alias("IntArray", int_array);
    
    auto result = registry.get_type("IntArray");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)->name(), "Array<Int>");
}

// Test alias to function type
TEST(TypeAliasTest, AliasToFunctionType) {
    auto& registry = TypeRegistry::instance();
    
    // typealias BinaryOp = (Int, Int) => Int
    auto func_type = registry.create_function_type(
        {registry.get_int_type(), registry.get_int_type()},
        registry.get_int_type()
    );
    registry.register_type_alias("BinaryOp", func_type);
    
    auto result = registry.get_type("BinaryOp");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)->name(), "(Int, Int) => Int");
}

// Test chained aliases
TEST(TypeAliasTest, ChainedAliases) {
    auto& registry = TypeRegistry::instance();
    
    // typealias A = Int
    registry.register_type_alias("A", "Int");
    
    // typealias B = A
    registry.register_type_alias("B", "A");
    
    auto result = registry.get_type("B");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)->name(), "Int");
}

// Test resolve_alias method
TEST(TypeAliasTest, ResolveAlias) {
    auto& registry = TypeRegistry::instance();
    
    registry.register_type_alias("MyString", "String");
    
    auto resolved = registry.resolve_alias("MyString");
    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ((*resolved)->name(), "String");
    
    // Non-existent alias should fail
    auto invalid = registry.resolve_alias("NonExistent");
    EXPECT_FALSE(invalid.has_value());
}

// Test is_alias method
TEST(TypeAliasTest, IsAlias) {
    auto& registry = TypeRegistry::instance();
    
    registry.register_type_alias("MyBool", "Bool");
    
    EXPECT_TRUE(registry.is_alias("MyBool"));
    EXPECT_FALSE(registry.is_alias("Bool"));
    EXPECT_FALSE(registry.is_alias("NonExistent"));
}

// Test alias with nullable type
TEST(TypeAliasTest, AliasWithNullable) {
    auto& registry = TypeRegistry::instance();
    
    // typealias NullableString = String?
    auto nullable_string = registry.create_optional_type(registry.get_string_type());
    registry.register_type_alias("NullableString", nullable_string);
    
    auto result = registry.get_type("NullableString");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(registry.is_nullable_type(**result));
    
    // Get inner type
    auto inner = registry.get_inner_type(**result);
    ASSERT_TRUE(inner.has_value());
    EXPECT_EQ((*inner)->name(), "String");
}

// Test complex alias with nested generics
TEST(TypeAliasTest, ComplexNestedAlias) {
    auto& registry = TypeRegistry::instance();
    
    // typealias StringOrInt = String | Int
    auto union_type = MetaType::create_union({
        registry.get_string_type(),
        registry.get_int_type()
    });
    registry.register_type_alias("StringOrInt", union_type);
    
    // typealias ListOfStringOrInt = Array<StringOrInt>
    auto resolved_union = registry.get_type("StringOrInt");
    ASSERT_TRUE(resolved_union.has_value());
    
    auto array_type = registry.create_array_type(*resolved_union);
    registry.register_type_alias("ListOfStringOrInt", array_type);
    
    auto result = registry.get_type("ListOfStringOrInt");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE((*result)->name().find("Array") != std::string::npos);
}

// Test alias overwriting (should replace existing alias)
TEST(TypeAliasTest, AliasOverwriting) {
    auto& registry = TypeRegistry::instance();
    
    // First alias
    registry.register_type_alias("MyType", "Int");
    auto result1 = registry.get_type("MyType");
    ASSERT_TRUE(result1.has_value());
    EXPECT_EQ((*result1)->name(), "Int");
    
    // Overwrite with new alias
    registry.register_type_alias("MyType", "String");
    auto result2 = registry.get_type("MyType");
    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ((*result2)->name(), "String");
}

// Test Tuple type alias (empty type parameter list)
TEST(TypeAliasTest, TupleAlias) {
    auto& registry = TypeRegistry::instance();
    
    // typealias Tuple<> = ()  (Unit type)
    registry.register_type_alias("EmptyTuple", "Unit");
    
    auto result = registry.get_type("EmptyTuple");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)->name(), "Unit");
}
