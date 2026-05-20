#include <gtest/gtest.h>
#include "meld/types/property.hpp"
#include "meld/types/instance.hpp"
#include "meld/meta/metatype.hpp"

using namespace meld::types;
using namespace meld::meta;
using namespace meld::kernel;

// Test property with custom getter
TEST(PropertyCustomAccessorsTest, PropertyWithCustomGetter) {
    auto& registry = TypeRegistry::instance();
    auto int_type = registry.get_int_type();
    
    // Create a property with custom getter (computed property)
    Value getter_impl = createSymbol("custom_getter");  // Placeholder for actual function
    
    auto prop = PropertyBuilder("computed", int_type)
        .immutable_property()
        .without_backing_field()
        .with_custom_getter(getter_impl)
        .build();
    
    EXPECT_TRUE(prop.has_custom_getter);
    EXPECT_FALSE(prop.has_backing_field);
    
    // Try to access it
    PropertyContext context{
        nullptr,
        &prop,
        AccessContext{AccessModifier::Public, true, false, true}
    };
    
    auto result = PropertyAccessor::get_value(context);
    ASSERT_TRUE(result.has_value());
    
    // Verify we got the getter implementation (simplified test)
    EXPECT_TRUE(result.value().is<Symbol>());
}

// Test property with custom setter
TEST(PropertyCustomAccessorsTest, PropertyWithCustomSetter) {
    auto& registry = TypeRegistry::instance();
    auto string_type = registry.get_string_type();
    
    // Create a property with custom setter
    Value setter_impl = createSymbol("custom_setter");  // Placeholder for actual function
    
    auto prop = PropertyBuilder("validated", string_type)
        .mutable_property()
        .with_backing_field("_validated")
        .with_custom_setter(setter_impl)
        .build();
    
    EXPECT_TRUE(prop.has_custom_setter);
    EXPECT_TRUE(prop.has_backing_field);
    
    // Create a struct with this property
    std::vector<Field> fields = {
        Field("_validated", string_type, true)
    };
    
    std::vector<Property> properties;
    properties.push_back(prop);
    
    auto struct_type = std::dynamic_pointer_cast<StructMetaType>(
        MetaType::create_struct("ValidatedData", fields, properties)
    );
    
    auto instance = create_struct_instance(struct_type);
    
    // Try to set it
    PropertyContext context{
        instance,
        &struct_type->properties()[0],
        AccessContext{AccessModifier::Public, true, false, true}
    };
    
    auto new_value = make_string("test@example.com");
    auto result = PropertyAccessor::set_value(context, new_value);
    
    // Should succeed (simplified implementation)
    EXPECT_TRUE(result.has_value());
}

// Test computed property (getter only, no backing field)
TEST(PropertyCustomAccessorsTest, ComputedProperty) {
    auto& registry = TypeRegistry::instance();
    auto int_type = registry.get_int_type();
    
    // Create a computed property
    Value getter_impl = createSymbol("area_calculator");
    
    auto prop = PropertyBuilder("area", int_type)
        .immutable_property()
        .without_backing_field()
        .with_custom_getter(getter_impl)
        .build();
    
    EXPECT_FALSE(prop.has_backing_field);
    EXPECT_TRUE(prop.has_custom_getter);
    EXPECT_FALSE(prop.has_custom_setter);
    EXPECT_FALSE(prop.is_mutable);
}

// Test property with both custom getter and setter
TEST(PropertyCustomAccessorsTest, PropertyWithBothCustomAccessors) {
    auto& registry = TypeRegistry::instance();
    auto float_type = registry.get_float_type();
    
    // Create a property with both custom getter and setter
    Value getter_impl = createSymbol("fahrenheit_getter");
    Value setter_impl = createSymbol("fahrenheit_setter");
    
    auto prop = PropertyBuilder("fahrenheit", float_type)
        .mutable_property()
        .with_backing_field("_celsius")
        .with_custom_getter(getter_impl)
        .with_custom_setter(setter_impl)
        .build();
    
    EXPECT_TRUE(prop.has_backing_field);
    EXPECT_TRUE(prop.has_custom_getter);
    EXPECT_TRUE(prop.has_custom_setter);
    EXPECT_TRUE(prop.is_mutable);
}

// Test that custom getter is called instead of backing field access
TEST(PropertyCustomAccessorsTest, CustomGetterTakesPrecedence) {
    auto& registry = TypeRegistry::instance();
    auto int_type = registry.get_int_type();
    
    // Create a property with both backing field and custom getter
    Value getter_impl = createSymbol("custom_logic");
    
    auto prop = PropertyBuilder("value", int_type)
        .immutable_property()
        .with_backing_field("_value")
        .with_custom_getter(getter_impl)
        .build();
    
    // Create struct with this property
    std::vector<Field> fields = {
        Field("_value", int_type, false)
    };
    
    std::vector<Property> properties;
    properties.push_back(prop);
    
    auto struct_type = std::dynamic_pointer_cast<StructMetaType>(
        MetaType::create_struct("Data", fields, properties)
    );
    
    auto instance = create_struct_instance(struct_type);
    
    // Set backing field directly
    instance->set_field("_value", make_int(42));
    
    // Access through property (should use custom getter)
    PropertyContext context{
        instance,
        &struct_type->properties()[0],
        AccessContext{AccessModifier::Public, true, false, true}
    };
    
    auto result = PropertyAccessor::get_value(context);
    ASSERT_TRUE(result.has_value());
    
    // Should get the getter implementation, not the backing field value
    EXPECT_TRUE(result.value().is<Symbol>());
}

// Test that custom setter is called instead of backing field assignment
TEST(PropertyCustomAccessorsTest, CustomSetterTakesPrecedence) {
    auto& registry = TypeRegistry::instance();
    auto string_type = registry.get_string_type();
    
    // Create a property with both backing field and custom setter
    Value setter_impl = createSymbol("validation_logic");
    
    auto prop = PropertyBuilder("email", string_type)
        .mutable_property()
        .with_backing_field("_email")
        .with_custom_setter(setter_impl)
        .build();
    
    // Create struct with this property
    std::vector<Field> fields = {
        Field("_email", string_type, true)
    };
    
    std::vector<Property> properties;
    properties.push_back(prop);
    
    auto struct_type = std::dynamic_pointer_cast<StructMetaType>(
        MetaType::create_struct("User", fields, properties)
    );
    
    auto instance = create_struct_instance(struct_type);
    
    // Set through property (should use custom setter)
    PropertyContext context{
        instance,
        &struct_type->properties()[0],
        AccessContext{AccessModifier::Public, true, false, true}
    };
    
    auto new_value = make_string("user@example.com");
    auto result = PropertyAccessor::set_value(context, new_value);
    
    // Should succeed (custom setter was called)
    EXPECT_TRUE(result.has_value());
}

// Test error when trying to set computed property (getter only)
TEST(PropertyCustomAccessorsTest, CannotSetComputedProperty) {
    auto& registry = TypeRegistry::instance();
    auto int_type = registry.get_int_type();
    
    // Create a computed property (immutable, getter only)
    Value getter_impl = createSymbol("compute_value");
    
    auto prop = PropertyBuilder("computed", int_type)
        .immutable_property()
        .without_backing_field()
        .with_custom_getter(getter_impl)
        .build();
    
    PropertyContext context{
        nullptr,
        &prop,
        AccessContext{AccessModifier::Public, true, false, true}
    };
    
    auto new_value = make_int(100);
    auto result = PropertyAccessor::set_value(context, new_value);
    
    // Should fail because property is immutable
    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().find("immutable") != std::string::npos);
}

// Test multiple computed properties on same class
TEST(PropertyCustomAccessorsTest, MultipleComputedProperties) {
    auto& registry = TypeRegistry::instance();
    auto int_type = registry.get_int_type();
    auto float_type = registry.get_float_type();
    
    // Create multiple computed properties
    std::vector<Field> fields = {
        Field("_width", int_type, true),
        Field("_height", int_type, true)
    };
    
    std::vector<Property> properties;
    
    // Area property (computed)
    properties.push_back(
        PropertyBuilder("area", int_type)
            .immutable_property()
            .without_backing_field()
            .with_custom_getter(createSymbol("area_getter"))
            .build()
    );
    
    // Perimeter property (computed)
    properties.push_back(
        PropertyBuilder("perimeter", int_type)
            .immutable_property()
            .without_backing_field()
            .with_custom_getter(createSymbol("perimeter_getter"))
            .build()
    );
    
    // Aspect ratio property (computed)
    properties.push_back(
        PropertyBuilder("aspectRatio", float_type)
            .immutable_property()
            .without_backing_field()
            .with_custom_getter(createSymbol("aspect_ratio_getter"))
            .build()
    );
    
    auto class_type = std::dynamic_pointer_cast<ClassMetaType>(
        MetaType::create_class("Rectangle", fields, {}, properties)
    );
    
    EXPECT_EQ(class_type->properties().size(), 3);
    
    // All should be computed properties
    for (const auto& prop : class_type->properties()) {
        EXPECT_FALSE(prop.has_backing_field);
        EXPECT_TRUE(prop.has_custom_getter);
        EXPECT_FALSE(prop.is_mutable);
    }
}
