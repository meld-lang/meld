#include <gtest/gtest.h>
#include "meld/meta/metatype.hpp"

using namespace meld::meta;

TEST(MetaTypeTest, PrimitiveTypes) {
    auto& registry = TypeRegistry::instance();
    
    auto int_type = registry.get_int_type();
    EXPECT_EQ(int_type->name(), "Int");
    EXPECT_TRUE(int_type->is_value_type());
    EXPECT_EQ(int_type->size(), sizeof(int64_t));
    
    auto bool_type = registry.get_bool_type();
    EXPECT_EQ(bool_type->name(), "Bool");
    EXPECT_TRUE(bool_type->is_value_type());
    
    auto string_type = registry.get_string_type();
    EXPECT_EQ(string_type->name(), "String");
}

TEST(MetaTypeTest, StructType) {
    auto& registry = TypeRegistry::instance();
    
    std::vector<Field> fields = {
        Field("x", registry.get_int_type(), false),
        Field("y", registry.get_int_type(), false)
    };
    
    auto point_type = MetaType::create_struct("Point", std::move(fields));
    
    EXPECT_EQ(point_type->name(), "Point");
    EXPECT_TRUE(point_type->is_value_type());
    EXPECT_EQ(point_type->size(), sizeof(int64_t) * 2);
    
    auto* struct_type = dynamic_cast<StructMetaType*>(point_type.get());
    ASSERT_NE(struct_type, nullptr);
    
    auto x_field = struct_type->get_field("x");
    ASSERT_TRUE(x_field.has_value());
    EXPECT_EQ((*x_field)->name, "x");
    
    auto invalid_field = struct_type->get_field("z");
    EXPECT_FALSE(invalid_field.has_value());
}

TEST(MetaTypeTest, ClassType) {
    auto& registry = TypeRegistry::instance();
    
    std::vector<Field> fields = {
        Field("name", registry.get_string_type(), true)
    };
    
    std::vector<Method> methods;
    
    auto person_type = MetaType::create_class("Person", std::move(fields), std::move(methods));
    
    EXPECT_EQ(person_type->name(), "Person");
    EXPECT_FALSE(person_type->is_value_type());
    EXPECT_EQ(person_type->size(), sizeof(void*)); // Reference type
}

TEST(MetaTypeTest, UnionType) {
    auto& registry = TypeRegistry::instance();
    
    auto union_type = MetaType::create_union({
        registry.get_int_type(),
        registry.get_string_type()
    });
    
    EXPECT_EQ(union_type->name(), "Int | String");
    
    auto* union_meta = dynamic_cast<UnionMetaType*>(union_type.get());
    ASSERT_NE(union_meta, nullptr);
    
    // Int should be assignable to (Int | String)
    EXPECT_TRUE(union_type->is_assignable_from(*registry.get_int_type()));
    EXPECT_TRUE(union_type->is_assignable_from(*registry.get_string_type()));
    EXPECT_FALSE(union_type->is_assignable_from(*registry.get_bool_type()));
}

TEST(MetaTypeTest, IntersectionType) {
    auto& registry = TypeRegistry::instance();
    
    auto trait1 = MetaType::create_trait("Drawable", {});
    auto trait2 = MetaType::create_trait("Clickable", {});
    
    auto intersection_type = MetaType::create_intersection({trait1, trait2});
    
    EXPECT_EQ(intersection_type->name(), "Drawable & Clickable");
}

TEST(MetaTypeTest, OptionalType) {
    auto& registry = TypeRegistry::instance();
    
    auto optional_int = registry.create_optional_type(registry.get_int_type());
    
    // Optional<Int> is Int | Null
    auto* union_type = dynamic_cast<UnionMetaType*>(optional_int.get());
    ASSERT_NE(union_type, nullptr);
    EXPECT_EQ(union_type->types().size(), 2);
}

TEST(MetaTypeTest, ArrayType) {
    auto& registry = TypeRegistry::instance();
    
    auto array_int = registry.create_array_type(registry.get_int_type());
    
    EXPECT_EQ(array_int->name(), "Array<Int>");
}

TEST(MetaTypeTest, FunctionType) {
    auto& registry = TypeRegistry::instance();
    
    auto func_type = registry.create_function_type(
        {registry.get_int_type(), registry.get_int_type()},
        registry.get_int_type()
    );
    
    EXPECT_EQ(func_type->name(), "(Int, Int) => Int");
}

TEST(MetaTypeTest, GenericType) {
    auto& registry = TypeRegistry::instance();
    
    GenericMetaType generic_t("T", Variance::Invariant);
    
    EXPECT_EQ(generic_t.name(), "T");
    EXPECT_EQ(generic_t.variance(), Variance::Invariant);
    
    // Instantiate with Int
    auto result = generic_t.instantiate(registry.get_int_type());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)->name(), "Int");
}

TEST(MetaTypeTest, GenericWithBound) {
    auto& registry = TypeRegistry::instance();
    
    // T extends Comparable
    auto comparable_trait = MetaType::create_trait("Comparable", {});
    GenericMetaType generic_t("T", Variance::Invariant, comparable_trait);
    
    // Should fail if type doesn't satisfy bound
    auto result = generic_t.instantiate(registry.get_int_type());
    EXPECT_FALSE(result.has_value()); // Int doesn't extend Comparable (not set up)
}

TEST(MetaTypeTest, Variance) {
    GenericMetaType covariant("T", Variance::Covariant);
    GenericMetaType contravariant("T", Variance::Contravariant);
    GenericMetaType invariant("T", Variance::Invariant);
    
    EXPECT_EQ(covariant.variance(), Variance::Covariant);
    EXPECT_EQ(contravariant.variance(), Variance::Contravariant);
    EXPECT_EQ(invariant.variance(), Variance::Invariant);
}

TEST(MetaTypeTest, ClassInheritance) {
    auto& registry = TypeRegistry::instance();
    
    // Base class
    std::vector<Field> base_fields = {
        Field("id", registry.get_int_type(), false)
    };
    auto base_class = std::make_shared<ClassMetaType>(
        "Entity", std::move(base_fields), std::vector<Method>{}
    );
    
    // Derived class
    std::vector<Field> derived_fields = {
        Field("name", registry.get_string_type(), true)
    };
    auto derived_class = std::make_shared<ClassMetaType>(
        "Person", std::move(derived_fields), std::vector<Method>{}, std::vector<Property>{}, base_class
    );
    
    // Person should be subtype of Entity
    EXPECT_TRUE(derived_class->is_subtype_of(*base_class));
    EXPECT_FALSE(base_class->is_subtype_of(*derived_class));
    
    // Person should have access to base class fields
    auto id_field = derived_class->get_field("id");
    EXPECT_TRUE(id_field.has_value());
    
    auto name_field = derived_class->get_field("name");
    EXPECT_TRUE(name_field.has_value());
}

TEST(MetaTypeTest, MetaTypeOfMetaType) {
    auto& registry = TypeRegistry::instance();
    
    auto int_type = registry.get_int_type();
    auto metatype = int_type->get_metatype();
    
    EXPECT_EQ(metatype->name(), "MetaType");
    
    // MetaType is the type of all types, including itself
    auto metatype_of_metatype = metatype->get_metatype();
    EXPECT_EQ(metatype_of_metatype->name(), "MetaType");
    
    // Verify self-referential property: typeof(MetaType) == MetaType
    EXPECT_EQ(metatype.get(), metatype_of_metatype.get());
}

TEST(MetaTypeTest, TypeOfOperation) {
    auto& registry = TypeRegistry::instance();
    
    // Test typeof for various types
    auto int_type = registry.get_int_type();
    auto typeof_int = MetaType::typeof_value(*int_type);
    EXPECT_EQ(typeof_int->name(), "MetaType");
    
    auto string_type = registry.get_string_type();
    auto typeof_string = MetaType::typeof_value(*string_type);
    EXPECT_EQ(typeof_string->name(), "MetaType");
    
    // typeof(MetaType) should return MetaType itself
    auto metatype = MetaType::get_metatype_instance();
    auto typeof_metatype = MetaType::typeof_value(*metatype);
    EXPECT_EQ(typeof_metatype->name(), "MetaType");
    EXPECT_EQ(metatype.get(), typeof_metatype.get());
}

TEST(MetaTypeTest, MetaTypeIntrospection) {
    auto& registry = TypeRegistry::instance();
    
    // Test introspection methods on MetaType
    auto metatype = MetaType::get_metatype_instance();
    
    EXPECT_EQ(metatype->name(), "MetaType");
    EXPECT_EQ(metatype->size(), sizeof(void*));
    EXPECT_FALSE(metatype->is_value_type());
}

TEST(MetaTypeTest, AllTypesAreInstancesOfMetaType) {
    auto& registry = TypeRegistry::instance();
    
    // All types should have MetaType as their type
    auto int_type = registry.get_int_type();
    EXPECT_EQ(int_type->get_metatype()->name(), "MetaType");
    
    auto bool_type = registry.get_bool_type();
    EXPECT_EQ(bool_type->get_metatype()->name(), "MetaType");
    
    auto string_type = registry.get_string_type();
    EXPECT_EQ(string_type->get_metatype()->name(), "MetaType");
    
    // Custom types too
    auto point_type = MetaType::create_struct("Point", {
        Field("x", int_type, false),
        Field("y", int_type, false)
    });
    EXPECT_EQ(point_type->get_metatype()->name(), "MetaType");
    
    // And MetaType itself
    auto metatype = MetaType::get_metatype_instance();
    EXPECT_EQ(metatype->get_metatype()->name(), "MetaType");
}
