#include <gtest/gtest.h>
#include "meld/types/property.hpp"
#include "meld/types/instance.hpp"
#include "meld/meta/metatype.hpp"

using namespace meld::types;
using namespace meld::meta;
using namespace meld::kernel;

// Test basic property with backing field on struct
TEST(PropertyBasicTest, StructPropertyWithBackingField) {
    auto& registry = TypeRegistry::instance();
    auto string_type = registry.get_string_type();
    
    // Create a struct with a property
    std::vector<Field> fields = {
        Field("_name", string_type, true)  // Backing field
    };
    
    std::vector<Property> properties;
    properties.push_back(
        PropertyBuilder("name", string_type)
            .mutable_property()
            .with_backing_field("_name")
            .build()
    );
    
    auto struct_type = std::dynamic_pointer_cast<StructMetaType>(
        MetaType::create_struct("Person", fields, properties)
    );
    
    // Create instance
    auto instance = create_struct_instance(struct_type);
    ASSERT_NE(instance, nullptr);
    
    // Set backing field directly
    auto name_value = make_string("Alice");
    auto set_result = instance->set_field("_name", name_value);
    EXPECT_TRUE(set_result.has_value());
    
    // Access property through PropertyAccessor
    const auto& prop = struct_type->properties()[0];
    PropertyContext context{
        instance,  // Pass the instance directly, not to_value()
        &prop,
        AccessContext{AccessModifier::Public, true, false, true}
    };
    
    auto get_result = PropertyAccessor::get_value(context);
    ASSERT_TRUE(get_result.has_value());
    
    // Verify we got the correct value
    auto result_str = get_result.value().try_as<String>();
    ASSERT_TRUE(result_str.has_value());
    EXPECT_EQ(result_str.value()->value(), "Alice");
}

// Test basic property with backing field on class
TEST(PropertyBasicTest, ClassPropertyWithBackingField) {
    auto& registry = TypeRegistry::instance();
    auto int_type = registry.get_int_type();
    
    // Create a class with a property
    std::vector<Field> fields = {
        Field("_age", int_type, true)  // Backing field
    };
    
    std::vector<Property> properties;
    properties.push_back(
        PropertyBuilder("age", int_type)
            .mutable_property()
            .with_backing_field("_age")
            .build()
    );
    
    auto class_type = std::dynamic_pointer_cast<ClassMetaType>(
        MetaType::create_class("Person", fields, {}, properties)
    );
    
    // Create instance
    auto instance = create_class_instance(class_type);
    ASSERT_NE(instance, nullptr);
    
    // Set backing field directly
    auto age_value = make_int(30);
    auto set_result = instance->set_field("_age", age_value);
    EXPECT_TRUE(set_result.has_value());
    
    // Access property through PropertyAccessor
    const auto& prop = class_type->properties()[0];
    PropertyContext context{
        instance,  // Pass the instance directly
        &prop,
        AccessContext{AccessModifier::Public, true, false, true}
    };
    
    auto get_result = PropertyAccessor::get_value(context);
    ASSERT_TRUE(get_result.has_value());
    
    // Verify we got the correct value
    auto result_int = get_result.value().try_as<Integer>();
    ASSERT_TRUE(result_int.has_value());
    EXPECT_EQ(result_int.value()->value(), 30);
}

// Test setting property value through PropertyAccessor
TEST(PropertyBasicTest, SetPropertyValue) {
    auto& registry = TypeRegistry::instance();
    auto string_type = registry.get_string_type();
    
    // Create a struct with a mutable property
    std::vector<Field> fields = {
        Field("_email", string_type, true)
    };
    
    std::vector<Property> properties;
    properties.push_back(
        PropertyBuilder("email", string_type)
            .mutable_property()
            .with_backing_field("_email")
            .build()
    );
    
    auto struct_type = std::dynamic_pointer_cast<StructMetaType>(
        MetaType::create_struct("User", fields, properties)
    );
    
    auto instance = create_struct_instance(struct_type);
    
    // Set property value through PropertyAccessor
    const auto& prop = struct_type->properties()[0];
    PropertyContext context{
        instance,  // Pass the instance directly
        &prop,
        AccessContext{AccessModifier::Public, true, false, true}
    };
    
    auto new_value = make_string("user@example.com");
    auto set_result = PropertyAccessor::set_value(context, new_value);
    EXPECT_TRUE(set_result.has_value());
    
    // Verify the backing field was updated
    auto get_result = instance->get_field("_email");
    ASSERT_TRUE(get_result.has_value());
    
    auto result_str = get_result.value().try_as<String>();
    ASSERT_TRUE(result_str.has_value());
    EXPECT_EQ(result_str.value()->value(), "user@example.com");
}

// Test immutable property cannot be set
TEST(PropertyBasicTest, ImmutablePropertyCannotBeSet) {
    auto& registry = TypeRegistry::instance();
    auto int_type = registry.get_int_type();
    
    // Create a struct with an immutable property
    std::vector<Field> fields = {
        Field("_id", int_type, false)  // Immutable backing field
    };
    
    std::vector<Property> properties;
    properties.push_back(
        PropertyBuilder("id", int_type)
            .immutable_property()
            .with_backing_field("_id")
            .build()
    );
    
    auto struct_type = std::dynamic_pointer_cast<StructMetaType>(
        MetaType::create_struct("Entity", fields, properties)
    );
    
    auto instance = create_struct_instance(struct_type);
    
    // Try to set immutable property
    const auto& prop = struct_type->properties()[0];
    PropertyContext context{
        instance,  // Pass the instance directly
        &prop,
        AccessContext{AccessModifier::Public, true, false, true}
    };
    
    auto new_value = make_int(123);
    auto set_result = PropertyAccessor::set_value(context, new_value);
    
    EXPECT_FALSE(set_result.has_value());
    EXPECT_TRUE(set_result.error().find("immutable") != std::string::npos);
}

// Test property with auto-generated backing field name
TEST(PropertyBasicTest, AutoGeneratedBackingFieldName) {
    auto& registry = TypeRegistry::instance();
    auto string_type = registry.get_string_type();
    
    // Create property without explicitly setting backing field name
    auto prop = PropertyBuilder("username", string_type)
        .mutable_property()
        .with_backing_field("_username")  // Explicitly set for this test
        .build();
    
    EXPECT_TRUE(prop.has_backing_field);
    EXPECT_EQ(prop.backing_field_name, "_username");
}

// Test multiple properties on same struct
TEST(PropertyBasicTest, MultiplePropertiesOnStruct) {
    auto& registry = TypeRegistry::instance();
    auto string_type = registry.get_string_type();
    auto int_type = registry.get_int_type();
    
    // Create a struct with multiple properties
    std::vector<Field> fields = {
        Field("_firstName", string_type, true),
        Field("_lastName", string_type, true),
        Field("_age", int_type, true)
    };
    
    std::vector<Property> properties;
    properties.push_back(
        PropertyBuilder("firstName", string_type)
            .mutable_property()
            .with_backing_field("_firstName")
            .build()
    );
    properties.push_back(
        PropertyBuilder("lastName", string_type)
            .mutable_property()
            .with_backing_field("_lastName")
            .build()
    );
    properties.push_back(
        PropertyBuilder("age", int_type)
            .mutable_property()
            .with_backing_field("_age")
            .build()
    );
    
    auto struct_type = std::dynamic_pointer_cast<StructMetaType>(
        MetaType::create_struct("Person", fields, properties)
    );
    
    auto instance = create_struct_instance(struct_type);
    
    // Set all properties
    AccessContext access{AccessModifier::Public, true, false, true};
    
    PropertyContext ctx1{instance, &struct_type->properties()[0], access};
    EXPECT_TRUE(PropertyAccessor::set_value(ctx1, make_string("John")).has_value());
    
    PropertyContext ctx2{instance, &struct_type->properties()[1], access};
    EXPECT_TRUE(PropertyAccessor::set_value(ctx2, make_string("Doe")).has_value());
    
    PropertyContext ctx3{instance, &struct_type->properties()[2], access};
    EXPECT_TRUE(PropertyAccessor::set_value(ctx3, make_int(25)).has_value());
    
    // Verify all properties
    auto first_result = PropertyAccessor::get_value(ctx1);
    ASSERT_TRUE(first_result.has_value());
    EXPECT_EQ(first_result.value().try_as<String>().value()->value(), "John");
    
    auto last_result = PropertyAccessor::get_value(ctx2);
    ASSERT_TRUE(last_result.has_value());
    EXPECT_EQ(last_result.value().try_as<String>().value()->value(), "Doe");
    
    auto age_result = PropertyAccessor::get_value(ctx3);
    ASSERT_TRUE(age_result.has_value());
    EXPECT_EQ(age_result.value().try_as<Integer>().value()->value(), 25);
}

// Test property without backing field (should fail)
TEST(PropertyBasicTest, PropertyWithoutBackingFieldFails) {
    auto& registry = TypeRegistry::instance();
    auto int_type = registry.get_int_type();
    
    // Create property without backing field
    auto prop = PropertyBuilder("computed", int_type)
        .immutable_property()
        .without_backing_field()
        .build();
    
    EXPECT_FALSE(prop.has_backing_field);
    
    // Try to access it (should fail since no getter is defined)
    PropertyContext context{
        nullptr,  // Null instance
        &prop,
        AccessContext{AccessModifier::Public, true, false, true}
    };
    
    auto get_result = PropertyAccessor::get_value(context);
    EXPECT_FALSE(get_result.has_value());
    EXPECT_TRUE(get_result.error().find("no getter") != std::string::npos);
}
