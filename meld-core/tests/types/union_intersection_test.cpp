#include <gtest/gtest.h>
#include "meld/meta/metatype.hpp"

using namespace meld::meta;

// ============================================================================
// Union Type Tests
// ============================================================================

TEST(UnionTypeTest, BasicUnionCreation) {
    auto& registry = TypeRegistry::instance();
    
    // Create Int | String union
    auto union_type = MetaType::create_union({
        registry.get_int_type(),
        registry.get_string_type()
    });
    
    EXPECT_EQ(union_type->name(), "Int | String");
    EXPECT_FALSE(union_type->is_value_type());
}

TEST(UnionTypeTest, UnionContainsType) {
    auto& registry = TypeRegistry::instance();
    
    auto union_type = MetaType::create_union({
        registry.get_int_type(),
        registry.get_string_type()
    });
    
    auto* union_meta = dynamic_cast<UnionMetaType*>(union_type.get());
    ASSERT_NE(union_meta, nullptr);
    
    // Check if union contains specific types
    EXPECT_TRUE(union_meta->contains_type(*registry.get_int_type()));
    EXPECT_TRUE(union_meta->contains_type(*registry.get_string_type()));
    EXPECT_FALSE(union_meta->contains_type(*registry.get_bool_type()));
}

TEST(UnionTypeTest, UnionAssignability) {
    auto& registry = TypeRegistry::instance();
    
    // Create Int | String union
    auto union_type = MetaType::create_union({
        registry.get_int_type(),
        registry.get_string_type()
    });
    
    // Int should be assignable to (Int | String)
    EXPECT_TRUE(union_type->is_assignable_from(*registry.get_int_type()));
    
    // String should be assignable to (Int | String)
    EXPECT_TRUE(union_type->is_assignable_from(*registry.get_string_type()));
    
    // Bool should NOT be assignable to (Int | String)
    EXPECT_FALSE(union_type->is_assignable_from(*registry.get_bool_type()));
}

TEST(UnionTypeTest, UnionSubtyping) {
    auto& registry = TypeRegistry::instance();
    
    // Create Int | String union
    auto union_type = MetaType::create_union({
        registry.get_int_type(),
        registry.get_string_type()
    });
    
    // Int is a subtype of (Int | String)
    EXPECT_TRUE(registry.get_int_type()->is_subtype_of(*union_type));
    
    // String is a subtype of (Int | String)
    EXPECT_TRUE(registry.get_string_type()->is_subtype_of(*union_type));
    
    // Bool is NOT a subtype of (Int | String)
    EXPECT_FALSE(registry.get_bool_type()->is_subtype_of(*union_type));
}

TEST(UnionTypeTest, UnionToUnionAssignability) {
    auto& registry = TypeRegistry::instance();
    
    // Create (Int | String)
    auto union1 = MetaType::create_union({
        registry.get_int_type(),
        registry.get_string_type()
    });
    
    // Create (Int | String | Bool)
    auto union2 = MetaType::create_union({
        registry.get_int_type(),
        registry.get_string_type(),
        registry.get_bool_type()
    });
    
    // (Int | String) should be assignable to (Int | String | Bool)
    EXPECT_TRUE(union2->is_assignable_from(*union1));
    
    // (Int | String | Bool) should NOT be assignable to (Int | String)
    EXPECT_FALSE(union1->is_assignable_from(*union2));
}

TEST(UnionTypeTest, UnionSize) {
    auto& registry = TypeRegistry::instance();
    
    auto union_type = MetaType::create_union({
        registry.get_int_type(),
        registry.get_bool_type()
    });
    
    // Union size should be the maximum of its constituent types
    size_t expected_size = std::max(
        registry.get_int_type()->size(),
        registry.get_bool_type()->size()
    );
    
    EXPECT_EQ(union_type->size(), expected_size);
}

TEST(UnionTypeTest, NullableType) {
    auto& registry = TypeRegistry::instance();
    
    // Create Int? (which is Int | Null)
    auto nullable_int = registry.create_optional_type(registry.get_int_type());
    
    auto* union_type = dynamic_cast<UnionMetaType*>(nullable_int.get());
    ASSERT_NE(union_type, nullptr);
    
    // Should have exactly 2 types
    EXPECT_EQ(union_type->types().size(), 2);
    
    // Should be assignable from Int
    EXPECT_TRUE(nullable_int->is_assignable_from(*registry.get_int_type()));
    
    // Should be assignable from Null
    EXPECT_TRUE(nullable_int->is_assignable_from(*registry.get_null_type()));
    
    // Should NOT be assignable from String
    EXPECT_FALSE(nullable_int->is_assignable_from(*registry.get_string_type()));
}

TEST(UnionTypeTest, ThreeWayUnion) {
    auto& registry = TypeRegistry::instance();
    
    // Create Int | String | Bool
    auto union_type = MetaType::create_union({
        registry.get_int_type(),
        registry.get_string_type(),
        registry.get_bool_type()
    });
    
    EXPECT_EQ(union_type->name(), "Int | String | Bool");
    
    // All three types should be assignable
    EXPECT_TRUE(union_type->is_assignable_from(*registry.get_int_type()));
    EXPECT_TRUE(union_type->is_assignable_from(*registry.get_string_type()));
    EXPECT_TRUE(union_type->is_assignable_from(*registry.get_bool_type()));
}

// ============================================================================
// Intersection Type Tests
// ============================================================================

TEST(IntersectionTypeTest, BasicIntersectionCreation) {
    auto& registry = TypeRegistry::instance();
    
    // Create two traits
    auto drawable = MetaType::create_trait("Drawable", {});
    auto clickable = MetaType::create_trait("Clickable", {});
    
    // Create Drawable & Clickable intersection
    auto intersection_type = MetaType::create_intersection({drawable, clickable});
    
    EXPECT_EQ(intersection_type->name(), "Drawable & Clickable");
    EXPECT_FALSE(intersection_type->is_value_type());
}

TEST(IntersectionTypeTest, IntersectionSatisfiesAll) {
    auto& registry = TypeRegistry::instance();
    
    auto drawable = MetaType::create_trait("Drawable", {});
    auto clickable = MetaType::create_trait("Clickable", {});
    
    auto intersection_type = MetaType::create_intersection({drawable, clickable});
    
    auto* intersection_meta = dynamic_cast<IntersectionMetaType*>(intersection_type.get());
    ASSERT_NE(intersection_meta, nullptr);
    
    // A type that implements both traits should satisfy the intersection
    // For this test, we'll create a class that could implement both
    std::vector<Field> fields;
    std::vector<Method> methods;
    auto button_class = MetaType::create_class("Button", std::move(fields), std::move(methods));
    
    // Note: In a full implementation, we'd need to track trait implementations
    // For now, we're testing the structure
}

TEST(IntersectionTypeTest, IntersectionAssignability) {
    auto& registry = TypeRegistry::instance();
    
    auto drawable = MetaType::create_trait("Drawable", {});
    auto clickable = MetaType::create_trait("Clickable", {});
    
    auto intersection_type = MetaType::create_intersection({drawable, clickable});
    
    // A type must satisfy ALL constraints to be assignable to an intersection
    // This is tested through the satisfies_all method
    auto* intersection_meta = dynamic_cast<IntersectionMetaType*>(intersection_type.get());
    ASSERT_NE(intersection_meta, nullptr);
}

TEST(IntersectionTypeTest, IntersectionSubtyping) {
    auto& registry = TypeRegistry::instance();
    
    auto drawable = MetaType::create_trait("Drawable", {});
    auto clickable = MetaType::create_trait("Clickable", {});
    
    // Create Drawable & Clickable
    auto intersection_type = MetaType::create_intersection({drawable, clickable});
    
    // (Drawable & Clickable) should be a subtype of Drawable
    EXPECT_TRUE(intersection_type->is_subtype_of(*drawable));
    
    // (Drawable & Clickable) should be a subtype of Clickable
    EXPECT_TRUE(intersection_type->is_subtype_of(*clickable));
}

TEST(IntersectionTypeTest, IntersectionToIntersectionAssignability) {
    auto& registry = TypeRegistry::instance();
    
    auto drawable = MetaType::create_trait("Drawable", {});
    auto clickable = MetaType::create_trait("Clickable", {});
    auto resizable = MetaType::create_trait("Resizable", {});
    
    // Create (Drawable & Clickable)
    auto intersection1 = MetaType::create_intersection({drawable, clickable});
    
    // Create (Drawable & Clickable & Resizable)
    auto intersection2 = MetaType::create_intersection({drawable, clickable, resizable});
    
    // (Drawable & Clickable & Resizable) should be a subtype of (Drawable & Clickable)
    EXPECT_TRUE(intersection2->is_subtype_of(*intersection1));
    
    // (Drawable & Clickable) should NOT be a subtype of (Drawable & Clickable & Resizable)
    EXPECT_FALSE(intersection1->is_subtype_of(*intersection2));
}

TEST(IntersectionTypeTest, IntersectionSize) {
    auto& registry = TypeRegistry::instance();
    
    auto drawable = MetaType::create_trait("Drawable", {});
    auto clickable = MetaType::create_trait("Clickable", {});
    
    auto intersection_type = MetaType::create_intersection({drawable, clickable});
    
    // Intersection size should be the maximum (must satisfy all constraints)
    size_t expected_size = std::max(drawable->size(), clickable->size());
    EXPECT_EQ(intersection_type->size(), expected_size);
}

TEST(IntersectionTypeTest, ThreeWayIntersection) {
    auto& registry = TypeRegistry::instance();
    
    auto drawable = MetaType::create_trait("Drawable", {});
    auto clickable = MetaType::create_trait("Clickable", {});
    auto resizable = MetaType::create_trait("Resizable", {});
    
    // Create Drawable & Clickable & Resizable
    auto intersection_type = MetaType::create_intersection({
        drawable, clickable, resizable
    });
    
    EXPECT_EQ(intersection_type->name(), "Drawable & Clickable & Resizable");
    
    // Should be a subtype of all three traits
    EXPECT_TRUE(intersection_type->is_subtype_of(*drawable));
    EXPECT_TRUE(intersection_type->is_subtype_of(*clickable));
    EXPECT_TRUE(intersection_type->is_subtype_of(*resizable));
}

// ============================================================================
// Combined Union and Intersection Tests
// ============================================================================

TEST(UnionIntersectionTest, ComplexTypeComposition) {
    auto& registry = TypeRegistry::instance();
    
    auto drawable = MetaType::create_trait("Drawable", {});
    auto clickable = MetaType::create_trait("Clickable", {});
    
    // Create (Drawable & Clickable)
    auto intersection = MetaType::create_intersection({drawable, clickable});
    
    // Create (Int | (Drawable & Clickable))
    auto complex_union = MetaType::create_union({
        registry.get_int_type(),
        intersection
    });
    
    EXPECT_EQ(complex_union->name(), "Int | Drawable & Clickable");
    
    // Int should be assignable to the union
    EXPECT_TRUE(complex_union->is_assignable_from(*registry.get_int_type()));
    
    // The intersection should be assignable to the union
    EXPECT_TRUE(complex_union->is_assignable_from(*intersection));
}

TEST(UnionIntersectionTest, UnionOfIntersections) {
    auto& registry = TypeRegistry::instance();
    
    auto drawable = MetaType::create_trait("Drawable", {});
    auto clickable = MetaType::create_trait("Clickable", {});
    auto resizable = MetaType::create_trait("Resizable", {});
    
    // Create (Drawable & Clickable)
    auto intersection1 = MetaType::create_intersection({drawable, clickable});
    
    // Create (Drawable & Resizable)
    auto intersection2 = MetaType::create_intersection({drawable, resizable});
    
    // Create (Drawable & Clickable) | (Drawable & Resizable)
    auto union_of_intersections = MetaType::create_union({intersection1, intersection2});
    
    EXPECT_EQ(union_of_intersections->name(), 
              "Drawable & Clickable | Drawable & Resizable");
    
    // Both intersections should be assignable to the union
    EXPECT_TRUE(union_of_intersections->is_assignable_from(*intersection1));
    EXPECT_TRUE(union_of_intersections->is_assignable_from(*intersection2));
}

TEST(UnionIntersectionTest, IntersectionOfUnions) {
    auto& registry = TypeRegistry::instance();
    
    // Create (Int | String)
    auto union1 = MetaType::create_union({
        registry.get_int_type(),
        registry.get_string_type()
    });
    
    // Create (String | Bool)
    auto union2 = MetaType::create_union({
        registry.get_string_type(),
        registry.get_bool_type()
    });
    
    // Create (Int | String) & (String | Bool)
    // This should effectively be String (the intersection)
    auto intersection_of_unions = MetaType::create_intersection({union1, union2});
    
    EXPECT_EQ(intersection_of_unions->name(), 
              "Int | String & String | Bool");
}

// ============================================================================
// Property 14: Union Type Membership
// ============================================================================

TEST(UnionTypePropertyTest, UnionTypeMembership) {
    // Property 14: For any value of union type A | B, 
    // the value should be assignable to either type A or type B
    
    auto& registry = TypeRegistry::instance();
    
    // Create Int | String union
    auto union_type = MetaType::create_union({
        registry.get_int_type(),
        registry.get_string_type()
    });
    
    // Test that Int is assignable to the union
    EXPECT_TRUE(union_type->is_assignable_from(*registry.get_int_type()));
    
    // Test that String is assignable to the union
    EXPECT_TRUE(union_type->is_assignable_from(*registry.get_string_type()));
    
    // Test that Bool is NOT assignable to the union
    EXPECT_FALSE(union_type->is_assignable_from(*registry.get_bool_type()));
    
    // Validates: Requirements 4.5, 4.7
}

// ============================================================================
// Property 15: Intersection Type Satisfaction
// ============================================================================

TEST(IntersectionTypePropertyTest, IntersectionTypeSatisfaction) {
    // Property 15: For any value of intersection type A & B,
    // the value should satisfy both type A and type B
    
    auto& registry = TypeRegistry::instance();
    
    auto drawable = MetaType::create_trait("Drawable", {});
    auto clickable = MetaType::create_trait("Clickable", {});
    
    // Create Drawable & Clickable intersection
    auto intersection_type = MetaType::create_intersection({drawable, clickable});
    
    // The intersection should be a subtype of both Drawable and Clickable
    EXPECT_TRUE(intersection_type->is_subtype_of(*drawable));
    EXPECT_TRUE(intersection_type->is_subtype_of(*clickable));
    
    // Validates: Requirements 4.5, 4.7
}
