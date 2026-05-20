#include <gtest/gtest.h>
#include "meld/meta/metatype.hpp"
#include "meld/kernel/primitives.hpp"

using namespace meld::meta;
using namespace meld::kernel;

// Test basic property creation with backing fields
TEST(PropertyTest, BasicPropertyWithBackingField) {
    auto int_type = TypeRegistry::instance().get_int_type();
    auto string_type = TypeRegistry::instance().get_string_type();
    
    // Create a property with backing field
    Property name_prop("name", string_type, false);
    
    EXPECT_EQ(name_prop.name, "name");
    EXPECT_EQ(name_prop.type, string_type);
    EXPECT_FALSE(name_prop.is_mutable);
    EXPECT_TRUE(name_prop.has_backing_field);
    EXPECT_EQ(name_prop.backing_field_name, "_name");
    EXPECT_FALSE(name_prop.has_custom_getter);
    EXPECT_FALSE(name_prop.has_custom_setter);
}

// Test mutable property
TEST(PropertyTest, MutableProperty) {
    auto int_type = TypeRegistry::instance().get_int_type();
    
    Property age_prop("age", int_type, true);
    
    EXPECT_EQ(age_prop.name, "age");
    EXPECT_TRUE(age_prop.is_mutable);
    EXPECT_TRUE(age_prop.has_backing_field);
}

// Test struct with properties
TEST(PropertyTest, StructWithProperties) {
    auto int_type = TypeRegistry::instance().get_int_type();
    auto string_type = TypeRegistry::instance().get_string_type();
    
    // Create properties
    std::vector<Property> properties;
    properties.emplace_back("name", string_type, false);
    properties.emplace_back("age", int_type, false);
    
    // Create struct with properties
    auto person_struct = MetaType::create_struct("Person", {}, properties);
    
    EXPECT_EQ(person_struct->name(), "Person");
    EXPECT_TRUE(person_struct->is_value_type());
    
    // Cast to StructMetaType to access properties
    auto* struct_type = dynamic_cast<StructMetaType*>(person_struct.get());
    ASSERT_NE(struct_type, nullptr);
    
    const auto& props = struct_type->properties();
    EXPECT_EQ(props.size(), 2);
    EXPECT_EQ(props[0].name, "name");
    EXPECT_EQ(props[1].name, "age");
}

// Test class with properties
TEST(PropertyTest, ClassWithProperties) {
    auto int_type = TypeRegistry::instance().get_int_type();
    auto string_type = TypeRegistry::instance().get_string_type();
    
    // Create properties
    std::vector<Property> properties;
    properties.emplace_back("name", string_type, true);  // mutable
    properties.emplace_back("id", int_type, false);      // immutable
    
    // Create class with properties
    auto user_class = MetaType::create_class("User", {}, {}, properties);
    
    EXPECT_EQ(user_class->name(), "User");
    EXPECT_FALSE(user_class->is_value_type());
    
    // Cast to ClassMetaType to access properties
    auto* class_type = dynamic_cast<ClassMetaType*>(user_class.get());
    ASSERT_NE(class_type, nullptr);
    
    const auto& props = class_type->properties();
    EXPECT_EQ(props.size(), 2);
    EXPECT_EQ(props[0].name, "name");
    EXPECT_TRUE(props[0].is_mutable);
    EXPECT_EQ(props[1].name, "id");
    EXPECT_FALSE(props[1].is_mutable);
}

// Test property lookup
TEST(PropertyTest, PropertyLookup) {
    auto string_type = TypeRegistry::instance().get_string_type();
    
    std::vector<Property> properties;
    properties.emplace_back("title", string_type, false);
    properties.emplace_back("author", string_type, false);
    
    auto book_struct = MetaType::create_struct("Book", {}, properties);
    auto* struct_type = dynamic_cast<StructMetaType*>(book_struct.get());
    
    // Test successful lookup
    auto title_result = struct_type->get_property("title");
    ASSERT_TRUE(title_result.has_value());
    EXPECT_EQ(title_result.value()->name, "title");
    
    auto author_result = struct_type->get_property("author");
    ASSERT_TRUE(author_result.has_value());
    EXPECT_EQ(author_result.value()->name, "author");
    
    // Test failed lookup
    auto missing_result = struct_type->get_property("publisher");
    EXPECT_FALSE(missing_result.has_value());
}

// Test property access modifiers
TEST(PropertyTest, PropertyAccessModifiers) {
    auto int_type = TypeRegistry::instance().get_int_type();
    
    Property prop("value", int_type, true);
    
    // Default access modifiers
    EXPECT_EQ(prop.getter_access, AccessModifier::Public);
    EXPECT_EQ(prop.setter_access, AccessModifier::Public);
    
    // Modify access
    prop.setter_access = AccessModifier::Private;
    EXPECT_EQ(prop.getter_access, AccessModifier::Public);
    EXPECT_EQ(prop.setter_access, AccessModifier::Private);
}

// Test computed property (no backing field)
TEST(PropertyTest, ComputedProperty) {
    auto int_type = TypeRegistry::instance().get_int_type();
    
    Property computed("computed_value", int_type, false);
    computed.has_backing_field = false;
    computed.has_custom_getter = true;
    
    EXPECT_FALSE(computed.has_backing_field);
    EXPECT_TRUE(computed.has_custom_getter);
}

// Test property with custom accessors
TEST(PropertyTest, PropertyWithCustomAccessors) {
    auto string_type = TypeRegistry::instance().get_string_type();
    
    Property prop("data", string_type, true);
    prop.has_custom_getter = true;
    prop.has_custom_setter = true;
    
    EXPECT_TRUE(prop.has_custom_getter);
    EXPECT_TRUE(prop.has_custom_setter);
    EXPECT_TRUE(prop.has_backing_field);  // Can still have backing field
}
