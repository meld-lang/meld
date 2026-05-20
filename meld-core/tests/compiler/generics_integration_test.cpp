#include <gtest/gtest.h>
#include "meld/compiler/type_checker.hpp"
#include "meld/meta/metatype.hpp"

using namespace meld::compiler;
using namespace meld::parser;
using namespace meld::parser;
using namespace meld::meta;

// Test generic type instantiation with union types
TEST(GenericsIntegrationTest, GenericWithUnionTypeArgument) {
    auto& registry = TypeRegistry::instance();
    TypeChecker checker;
    
    // Create List[Int | String]
    auto union_type = MetaType::create_union({
        registry.get_int_type(),
        registry.get_string_type()
    });
    
    // Get the List type (assuming it's registered)
    auto list_type_result = registry.get_type("List");
    if (!list_type_result) {
        // Register a basic List type for testing
        auto list_type = MetaType::create_primitive("List", sizeof(void*));
        registry.register_type("List", list_type);
        list_type_result = registry.get_type("List");
    }
    
    ASSERT_TRUE(list_type_result.has_value());
    
    // Instantiate List with union type
    auto result = checker.instantiate_generic_type(*list_type_result, {union_type});
    
    // Should succeed - union types are valid type arguments
    EXPECT_TRUE(result.has_value());
    if (result) {
        EXPECT_TRUE((*result)->name().find("Int | String") != std::string::npos);
    }
}

// Test generic type instantiation with intersection types
TEST(GenericsIntegrationTest, GenericWithIntersectionTypeArgument) {
    auto& registry = TypeRegistry::instance();
    TypeChecker checker;
    
    // Create traits
    auto drawable = MetaType::create_trait("Drawable", {});
    auto clickable = MetaType::create_trait("Clickable", {});
    
    // Create Array[Drawable & Clickable]
    auto intersection_type = MetaType::create_intersection({drawable, clickable});
    
    // Get or create Array type
    auto array_type_result = registry.get_type("Array");
    if (!array_type_result) {
        auto array_type = MetaType::create_primitive("Array", sizeof(void*));
        registry.register_type("Array", array_type);
        array_type_result = registry.get_type("Array");
    }
    
    ASSERT_TRUE(array_type_result.has_value());
    
    // Instantiate Array with intersection type
    auto result = checker.instantiate_generic_type(*array_type_result, {intersection_type});
    
    // Should succeed - intersection types are valid type arguments
    EXPECT_TRUE(result.has_value());
    if (result) {
        EXPECT_TRUE((*result)->name().find("Drawable & Clickable") != std::string::npos);
    }
}

// Test nested generics with unions
TEST(GenericsIntegrationTest, NestedGenericsWithUnions) {
    auto& registry = TypeRegistry::instance();
    TypeChecker checker;
    
    // Create List[Int | String]
    auto union_type = MetaType::create_union({
        registry.get_int_type(),
        registry.get_string_type()
    });
    
    auto list_type_result = registry.get_type("List");
    if (!list_type_result) {
        auto list_type = MetaType::create_primitive("List", sizeof(void*));
        registry.register_type("List", list_type);
        list_type_result = registry.get_type("List");
    }
    
    ASSERT_TRUE(list_type_result.has_value());
    
    auto inner_list = checker.instantiate_generic_type(*list_type_result, {union_type});
    ASSERT_TRUE(inner_list.has_value());
    
    // Create Array[List[Int | String]]
    auto array_type_result = registry.get_type("Array");
    if (!array_type_result) {
        auto array_type = MetaType::create_primitive("Array", sizeof(void*));
        registry.register_type("Array", array_type);
        array_type_result = registry.get_type("Array");
    }
    
    ASSERT_TRUE(array_type_result.has_value());
    
    auto nested = checker.instantiate_generic_type(*array_type_result, {*inner_list});
    
    // Should succeed - nested generics with unions are valid
    EXPECT_TRUE(nested.has_value());
}

// Test type parameter bounds checking with union bounds
TEST(GenericsIntegrationTest, TypeParameterBoundsWithUnion) {
    auto& registry = TypeRegistry::instance();
    TypeChecker checker;
    
    // Create type parameter: T: (Int | String)
    ast::generic_type_parameter param;
    param.name.name = "T";
    param.variance = "";  // Invariant
    param.has_bound = true;
    
    // Create bound annotation
    ast::type_annotation bound_annotation;
    bound_annotation.is_union = true;
    
    ast::type_annotation int_annotation;
    int_annotation.type_name.name = "Int";
    ast::type_annotation string_annotation;
    string_annotation.type_name.name = "String";
    
    bound_annotation.union_types.push_back(
        boost::spirit::x3::forward_ast<ast::type_annotation>(int_annotation)
    );
    bound_annotation.union_types.push_back(
        boost::spirit::x3::forward_ast<ast::type_annotation>(string_annotation)
    );
    
    param.bound = boost::spirit::x3::forward_ast<ast::type_annotation>(bound_annotation);
    
    // Test with Int (should satisfy bound)
    auto result1 = checker.check_type_parameter_bounds({param}, {registry.get_int_type()});
    EXPECT_TRUE(result1.has_value());
    
    // Test with String (should satisfy bound)
    auto result2 = checker.check_type_parameter_bounds({param}, {registry.get_string_type()});
    EXPECT_TRUE(result2.has_value());
    
    // Test with Bool (should NOT satisfy bound)
    auto result3 = checker.check_type_parameter_bounds({param}, {registry.get_bool_type()});
    EXPECT_FALSE(result3.has_value());
}

// Test type parameter bounds checking with intersection bounds
TEST(GenericsIntegrationTest, TypeParameterBoundsWithIntersection) {
    auto& registry = TypeRegistry::instance();
    TypeChecker checker;
    
    // Create type parameter: T: (Comparable & Hashable)
    ast::generic_type_parameter param;
    param.name.name = "T";
    param.variance = "";  // Invariant
    param.has_bound = true;
    
    // Create bound annotation
    ast::type_annotation bound_annotation;
    bound_annotation.is_intersection = true;
    
    ast::type_annotation comparable_annotation;
    comparable_annotation.type_name.name = "Comparable";
    ast::type_annotation hashable_annotation;
    hashable_annotation.type_name.name = "Hashable";
    
    bound_annotation.intersection_types.push_back(
        boost::spirit::x3::forward_ast<ast::type_annotation>(comparable_annotation)
    );
    bound_annotation.intersection_types.push_back(
        boost::spirit::x3::forward_ast<ast::type_annotation>(hashable_annotation)
    );
    
    param.bound = boost::spirit::x3::forward_ast<ast::type_annotation>(bound_annotation);
    
    // Register the traits
    auto comparable = MetaType::create_trait("Comparable", {});
    auto hashable = MetaType::create_trait("Hashable", {});
    registry.register_type("Comparable", comparable);
    registry.register_type("Hashable", hashable);
    
    // Create a type that implements both traits
    auto both_traits = MetaType::create_intersection({comparable, hashable});
    
    // Test with a type that satisfies both constraints
    auto result = checker.check_type_parameter_bounds({param}, {both_traits});
    EXPECT_TRUE(result.has_value());
}

// Test variance annotations
TEST(GenericsIntegrationTest, VarianceAnnotations) {
    TypeChecker checker;
    
    // Test covariant (out)
    ast::generic_type_parameter covariant_param;
    covariant_param.name.name = "T";
    covariant_param.variance = "out";
    covariant_param.has_bound = false;
    
    auto result1 = checker.check_type_parameter_bounds(
        {covariant_param}, 
        {TypeRegistry::instance().get_int_type()}
    );
    EXPECT_TRUE(result1.has_value());
    
    // Test contravariant (in)
    ast::generic_type_parameter contravariant_param;
    contravariant_param.name.name = "T";
    contravariant_param.variance = "in";
    contravariant_param.has_bound = false;
    
    auto result2 = checker.check_type_parameter_bounds(
        {contravariant_param}, 
        {TypeRegistry::instance().get_int_type()}
    );
    EXPECT_TRUE(result2.has_value());
    
    // Test invariant (no variance)
    ast::generic_type_parameter invariant_param;
    invariant_param.name.name = "T";
    invariant_param.variance = "";
    invariant_param.has_bound = false;
    
    auto result3 = checker.check_type_parameter_bounds(
        {invariant_param}, 
        {TypeRegistry::instance().get_int_type()}
    );
    EXPECT_TRUE(result3.has_value());
    
    // Test invalid variance
    ast::generic_type_parameter invalid_param;
    invalid_param.name.name = "T";
    invalid_param.variance = "invalid";
    invalid_param.has_bound = false;
    
    auto result4 = checker.check_type_parameter_bounds(
        {invalid_param}, 
        {TypeRegistry::instance().get_int_type()}
    );
    EXPECT_FALSE(result4.has_value());
}

// Test type substitution with union types
TEST(GenericsIntegrationTest, TypeSubstitutionWithUnions) {
    auto& registry = TypeRegistry::instance();
    TypeChecker checker;
    
    // Create a generic type parameter T
    auto generic_t = std::make_shared<GenericMetaType>("T", Variance::Invariant);
    
    // Create a union type Int | String
    auto union_type = MetaType::create_union({
        registry.get_int_type(),
        registry.get_string_type()
    });
    
    // Create substitution map: T -> (Int | String)
    std::map<std::string, std::shared_ptr<MetaType>> substitutions;
    substitutions["T"] = union_type;
    
    // Substitute T with the union type
    auto result = checker.substitute_type_parameters(generic_t, substitutions);
    
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->name(), "Int | String");
}

// Test type substitution with intersection types
TEST(GenericsIntegrationTest, TypeSubstitutionWithIntersections) {
    auto& registry = TypeRegistry::instance();
    TypeChecker checker;
    
    // Create a generic type parameter T
    auto generic_t = std::make_shared<GenericMetaType>("T", Variance::Invariant);
    
    // Create traits
    auto comparable = MetaType::create_trait("Comparable", {});
    auto hashable = MetaType::create_trait("Hashable", {});
    
    // Create an intersection type Comparable & Hashable
    auto intersection_type = MetaType::create_intersection({comparable, hashable});
    
    // Create substitution map: T -> (Comparable & Hashable)
    std::map<std::string, std::shared_ptr<MetaType>> substitutions;
    substitutions["T"] = intersection_type;
    
    // Substitute T with the intersection type
    auto result = checker.substitute_type_parameters(generic_t, substitutions);
    
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->name().find("Comparable") != std::string::npos);
    EXPECT_TRUE(result->name().find("Hashable") != std::string::npos);
}

// Test complex nested substitution
TEST(GenericsIntegrationTest, ComplexNestedSubstitution) {
    auto& registry = TypeRegistry::instance();
    TypeChecker checker;
    
    // Create generic type parameter T
    auto generic_t = std::make_shared<GenericMetaType>("T", Variance::Invariant);
    
    // Create a union containing an intersection: (Int | (Comparable & Hashable))
    auto comparable = MetaType::create_trait("Comparable", {});
    auto hashable = MetaType::create_trait("Hashable", {});
    auto intersection = MetaType::create_intersection({comparable, hashable});
    
    auto complex_union = MetaType::create_union({
        registry.get_int_type(),
        intersection
    });
    
    // Create substitution map
    std::map<std::string, std::shared_ptr<MetaType>> substitutions;
    substitutions["T"] = complex_union;
    
    // Substitute
    auto result = checker.substitute_type_parameters(generic_t, substitutions);
    
    ASSERT_NE(result, nullptr);
    // The result should be the complex union type
    auto* union_result = result->as<UnionMetaType>();
    ASSERT_NE(union_result, nullptr);
    EXPECT_EQ(union_result->types().size(), 2);
}

// Test multiple type parameters
TEST(GenericsIntegrationTest, MultipleTypeParameters) {
    TypeChecker checker;
    
    // Create Map[K, V] with K: String | Int, V: Bool | String
    ast::generic_type_parameter key_param;
    key_param.name.name = "K";
    key_param.variance = "";
    key_param.has_bound = false;
    
    ast::generic_type_parameter value_param;
    value_param.name.name = "V";
    value_param.variance = "";
    value_param.has_bound = false;
    
    auto& registry = TypeRegistry::instance();
    auto key_union = MetaType::create_union({
        registry.get_string_type(),
        registry.get_int_type()
    });
    
    auto value_union = MetaType::create_union({
        registry.get_bool_type(),
        registry.get_string_type()
    });
    
    // Check bounds for both parameters
    auto result = checker.check_type_parameter_bounds(
        {key_param, value_param},
        {key_union, value_union}
    );
    
    EXPECT_TRUE(result.has_value());
}

