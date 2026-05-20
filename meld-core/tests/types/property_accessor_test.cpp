#include <gtest/gtest.h>
#include "meld/types/property.hpp"
#include "meld/meta/metatype.hpp"

using namespace meld::types;
using namespace meld::meta;
using namespace meld::kernel;

// Test PropertyBuilder fluent API
TEST(PropertyAccessorTest, PropertyBuilderBasic) {
    auto string_type = TypeRegistry::instance().get_string_type();
    
    auto prop = PropertyBuilder("name", string_type)
        .immutable_property()
        .with_backing_field("_name")
        .build();
    
    EXPECT_EQ(prop.name, "name");
    EXPECT_FALSE(prop.is_mutable);
    EXPECT_TRUE(prop.has_backing_field);
    EXPECT_EQ(prop.backing_field_name, "_name");
}

// Test PropertyBuilder with custom getter
TEST(PropertyAccessorTest, PropertyBuilderWithCustomGetter) {
    auto int_type = TypeRegistry::instance().get_int_type();
    
    Value getter_impl;  // Placeholder for actual function
    
    auto prop = PropertyBuilder("computed", int_type)
        .without_backing_field()
        .with_custom_getter(getter_impl)
        .build();
    
    EXPECT_EQ(prop.name, "computed");
    EXPECT_FALSE(prop.has_backing_field);
    EXPECT_TRUE(prop.has_custom_getter);
}

// Test PropertyBuilder with custom setter
TEST(PropertyAccessorTest, PropertyBuilderWithCustomSetter) {
    auto string_type = TypeRegistry::instance().get_string_type();
    
    Value setter_impl;  // Placeholder for actual function
    
    auto prop = PropertyBuilder("validated", string_type)
        .mutable_property()
        .with_backing_field("_validated")
        .with_custom_setter(setter_impl)
        .build();
    
    EXPECT_EQ(prop.name, "validated");
    EXPECT_TRUE(prop.is_mutable);
    EXPECT_TRUE(prop.has_backing_field);
    EXPECT_TRUE(prop.has_custom_setter);
}

// Test PropertyBuilder with access modifiers
TEST(PropertyAccessorTest, PropertyBuilderWithAccessModifiers) {
    auto int_type = TypeRegistry::instance().get_int_type();
    
    auto prop = PropertyBuilder("balance", int_type)
        .mutable_property()
        .with_backing_field("_balance")
        .getter_access(AccessModifier::Public)
        .setter_access(AccessModifier::Private)
        .build();
    
    EXPECT_EQ(prop.name, "balance");
    EXPECT_EQ(prop.getter_access, AccessModifier::Public);
    EXPECT_EQ(prop.setter_access, AccessModifier::Private);
}

// Test PropertyBuilder for computed property
TEST(PropertyAccessorTest, PropertyBuilderComputedProperty) {
    auto string_type = TypeRegistry::instance().get_string_type();
    
    Value getter_impl;  // Placeholder
    
    auto prop = PropertyBuilder("fullName", string_type)
        .immutable_property()
        .without_backing_field()
        .with_custom_getter(getter_impl)
        .build();
    
    EXPECT_EQ(prop.name, "fullName");
    EXPECT_FALSE(prop.is_mutable);
    EXPECT_FALSE(prop.has_backing_field);
    EXPECT_TRUE(prop.has_custom_getter);
    EXPECT_FALSE(prop.has_custom_setter);
}

// Test PropertyAccessor error handling for immutable property
TEST(PropertyAccessorTest, CannotSetImmutableProperty) {
    auto int_type = TypeRegistry::instance().get_int_type();
    
    auto prop = PropertyBuilder("id", int_type)
        .immutable_property()
        .with_backing_field("_id")
        .build();
    
    PropertyContext context{nullptr, &prop, AccessContext{AccessModifier::Public, false, false, false}};
    Value new_value;
    
    auto result = PropertyAccessor::set_value(context, new_value);
    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().find("immutable") != std::string::npos);
}

// Test PropertyAccessor error handling for missing getter
TEST(PropertyAccessorTest, ErrorOnMissingGetter) {
    auto int_type = TypeRegistry::instance().get_int_type();
    
    auto prop = PropertyBuilder("broken", int_type)
        .without_backing_field()
        .build();
    
    PropertyContext context{nullptr, &prop, AccessContext{AccessModifier::Public, false, false, false}};
    
    auto result = PropertyAccessor::get_value(context);
    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().find("no getter") != std::string::npos);
}

// Test PropertyAccessor error handling for missing setter
TEST(PropertyAccessorTest, ErrorOnMissingSetter) {
    auto int_type = TypeRegistry::instance().get_int_type();
    
    auto prop = PropertyBuilder("broken", int_type)
        .mutable_property()
        .without_backing_field()
        .build();
    
    PropertyContext context{nullptr, &prop, AccessContext{AccessModifier::Public, false, false, false}};
    Value new_value;
    
    auto result = PropertyAccessor::set_value(context, new_value);
    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().find("no setter") != std::string::npos);
}

// Test property with both custom getter and setter
TEST(PropertyAccessorTest, PropertyWithBothCustomAccessors) {
    auto string_type = TypeRegistry::instance().get_string_type();
    
    Value getter_impl;
    Value setter_impl;
    
    auto prop = PropertyBuilder("data", string_type)
        .mutable_property()
        .with_backing_field("_data")
        .with_custom_getter(getter_impl)
        .with_custom_setter(setter_impl)
        .build();
    
    EXPECT_TRUE(prop.has_custom_getter);
    EXPECT_TRUE(prop.has_custom_setter);
    EXPECT_TRUE(prop.has_backing_field);
}
