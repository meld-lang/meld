#include <gtest/gtest.h>
#include "meld/meta/metatype.hpp"

using namespace meld::meta;

// Test generic type with union types
TEST(GenericsTest, GenericWithUnionType) {
    auto& registry = TypeRegistry::instance();
    
    // Create List[Int | String]
    auto union_type = MetaType::create_union({
        registry.get_int_type(),
        registry.get_string_type()
    });
    
    auto list_type = registry.create_array_type(union_type);
    
    EXPECT_EQ(list_type->name(), "Array[Int | String]");
}

// Test generic type with intersection types
TEST(GenericsTest, GenericWithIntersectionType) {
    auto& registry = TypeRegistry::instance();
    
    // Create traits
    auto drawable = MetaType::create_trait("Drawable", {});
    auto clickable = MetaType::create_trait("Clickable", {});
    
    // Create Array[Drawable & Clickable]
    auto intersection_type = MetaType::create_intersection({drawable, clickable});
    auto array_type = registry.create_array_type(intersection_type);
    
    EXPECT_EQ(array_type->name(), "Array[Drawable & Clickable]");
}

// Test nested generics with unions
TEST(GenericsTest, NestedGenericsWithUnions) {
    auto& registry = TypeRegistry::instance();
    
    // Create List[Int | String]
    auto union_type = MetaType::create_union({
        registry.get_int_type(),
        registry.get_string_type()
    });
    auto list_type = registry.create_array_type(union_type);
    
    // Create Array[List[Int | String]]
    auto nested_type = registry.create_array_type(list_type);
    
    // Verify the type name
    EXPECT_TRUE(nested_type->name().find("Array") != std::string::npos);
}

// Test generic instantiation with union bound
TEST(GenericsTest, GenericInstantiationWithUnionBound) {
    auto& registry = TypeRegistry::instance();
    
    // Create T extends (Int | String)
    auto union_bound = MetaType::create_union({
        registry.get_int_type(),
        registry.get_string_type()
    });
    
    GenericMetaType generic_t("T", Variance::Invariant, union_bound);
    
    // Int should satisfy the bound
    auto result1 = generic_t.instantiate(registry.get_int_type());
    EXPECT_TRUE(result1.has_value());
    
    // String should satisfy the bound
    auto result2 = generic_t.instantiate(registry.get_string_type());
    EXPECT_TRUE(result2.has_value());
    
    // Bool should not satisfy the bound
    auto result3 = generic_t.instantiate(registry.get_bool_type());
    EXPECT_FALSE(result3.has_value());
}

// Test generic instantiation with intersection bound
TEST(GenericsTest, GenericInstantiationWithIntersectionBound) {
    auto& registry = TypeRegistry::instance();
    
    // Create traits
    auto comparable = MetaType::create_trait("Comparable", {});
    auto hashable = MetaType::create_trait("Hashable", {});
    
    // Create T extends (Comparable & Hashable)
    auto intersection_bound = MetaType::create_intersection({comparable, hashable});
    
    GenericMetaType generic_t("T", Variance::Invariant, intersection_bound);
    
    // A type that doesn't implement both traits should fail
    auto result = generic_t.instantiate(registry.get_int_type());
    EXPECT_FALSE(result.has_value());
}

// Test covariant generic with union
TEST(GenericsTest, CovariantGenericWithUnion) {
    auto& registry = TypeRegistry::instance();
    
    // Producer[out T] where T = Int | String
    auto union_type = MetaType::create_union({
        registry.get_int_type(),
        registry.get_string_type()
    });
    
    GenericMetaType covariant_t("T", Variance::Covariant);
    
    auto result = covariant_t.instantiate(union_type);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)->name(), "Int | String");
}

// Test contravariant generic with union
TEST(GenericsTest, ContravariantGenericWithUnion) {
    auto& registry = TypeRegistry::instance();
    
    // Consumer[in T] where T = Int | String
    auto union_type = MetaType::create_union({
        registry.get_int_type(),
        registry.get_string_type()
    });
    
    GenericMetaType contravariant_t("T", Variance::Contravariant);
    
    auto result = contravariant_t.instantiate(union_type);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)->name(), "Int | String");
}

// Test multiple type parameters with unions
TEST(GenericsTest, MultipleTypeParametersWithUnions) {
    auto& registry = TypeRegistry::instance();
    
    // Map[K, V] where K = String | Int, V = Bool | String
    auto key_union = MetaType::create_union({
        registry.get_string_type(),
        registry.get_int_type()
    });
    
    auto value_union = MetaType::create_union({
        registry.get_bool_type(),
        registry.get_string_type()
    });
    
    // In a full implementation, we'd create a Map type with two type parameters
    // For now, just verify the unions work
    EXPECT_EQ(key_union->name(), "String | Int");
    EXPECT_EQ(value_union->name(), "Bool | String");
}

// Test optional generic (T | Null)
TEST(GenericsTest, OptionalGeneric) {
    auto& registry = TypeRegistry::instance();
    
    // Optional[T] = T | Null
    GenericMetaType generic_t("T", Variance::Covariant);
    
    // Instantiate with Int
    auto int_result = generic_t.instantiate(registry.get_int_type());
    ASSERT_TRUE(int_result.has_value());
    
    // Create Optional[Int] = Int | Null
    auto optional_int = registry.create_optional_type(*int_result);
    EXPECT_TRUE(registry.is_nullable_type(*optional_int));
}

// Test variant (union type alias)
TEST(GenericsTest, VariantTypeAlias) {
    auto& registry = TypeRegistry::instance();
    
    // Variant[A, B, C] = A | B | C
    auto variant = MetaType::create_union({
        registry.get_int_type(),
        registry.get_string_type(),
        registry.get_bool_type()
    });
    
    EXPECT_EQ(variant->name(), "Int | String | Bool");
    
    // Check assignability
    EXPECT_TRUE(variant->is_assignable_from(*registry.get_int_type()));
    EXPECT_TRUE(variant->is_assignable_from(*registry.get_string_type()));
    EXPECT_TRUE(variant->is_assignable_from(*registry.get_bool_type()));
}

// Test collection with union element type
TEST(GenericsTest, CollectionWithUnionElements) {
    auto& registry = TypeRegistry::instance();
    
    // Array[Int | Float]
    auto union_type = MetaType::create_union({
        registry.get_int_type(),
        registry.get_int_type()  // Using Int twice for now, would be Float
    });
    
    auto array_type = registry.create_array_type(union_type);
    EXPECT_TRUE(array_type->name().find("Array") != std::string::npos);
}

// Test collection with intersection element type
TEST(GenericsTest, CollectionWithIntersectionElements) {
    auto& registry = TypeRegistry::instance();
    
    // Set[Comparable & Hashable]
    auto comparable = MetaType::create_trait("Comparable", {});
    auto hashable = MetaType::create_trait("Hashable", {});
    
    auto intersection = MetaType::create_intersection({comparable, hashable});
    auto array_type = registry.create_array_type(intersection);
    
    EXPECT_TRUE(array_type->name().find("Array") != std::string::npos);
    EXPECT_TRUE(array_type->name().find("Comparable & Hashable") != std::string::npos);
}

// Test function with generic union parameters
TEST(GenericsTest, FunctionWithGenericUnionParameters) {
    auto& registry = TypeRegistry::instance();
    
    // fn process[T](items: List[T]) where T: Int | String
    auto union_type = MetaType::create_union({
        registry.get_int_type(),
        registry.get_string_type()
    });
    
    auto list_type = registry.create_array_type(union_type);
    
    auto func_type = registry.create_function_type(
        {list_type},
        registry.get_unit_type()
    );
    
    EXPECT_TRUE(func_type->name().find("Array") != std::string::npos);
}

// Test bounded generic with complex constraint
TEST(GenericsTest, BoundedGenericComplexConstraint) {
    auto& registry = TypeRegistry::instance();
    
    // T: (Comparable & Copyable) | Null
    auto comparable = MetaType::create_trait("Comparable", {});
    auto copyable = MetaType::create_trait("Copyable", {});
    
    auto intersection = MetaType::create_intersection({comparable, copyable});
    auto union_with_null = MetaType::create_union({intersection, registry.get_null_type()});
    
    GenericMetaType generic_t("T", Variance::Invariant, union_with_null);
    
    EXPECT_EQ(generic_t.name(), "T");
    EXPECT_NE(generic_t.bound(), nullptr);
}
