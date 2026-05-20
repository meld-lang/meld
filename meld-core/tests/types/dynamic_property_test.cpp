#include <gtest/gtest.h>
#include "meld/types/dynamic_property_access.hpp"
#include "meld/meta/type_registration.hpp"
#include "meld/kernel/primitives.hpp"
#ifndef __APPLE__
#include <rttr/type>
#endif

using namespace meld::types;
using namespace meld::meta;
using namespace meld::kernel;
using namespace meld::kernel;

class DynamicPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize type registration
        initialize_meld_types();
    }
};

// Test get_property with valid property
TEST_F(DynamicPropertyTest, GetProperty_ValidProperty) {
    Symbol symbol("test");
    rttr::instance instance(symbol);
    
    auto result = DynamicPropertyAccess::get_property(instance, "name");
    ASSERT_TRUE(result.has_value());
    
    rttr::variant value = result.value();
    EXPECT_TRUE(value.is_valid());
    EXPECT_TRUE(value.is_type<std::string>());
    EXPECT_EQ(value.get_value<std::string>(), "test");
}

// Test get_property with invalid property
TEST_F(DynamicPropertyTest, GetProperty_InvalidProperty) {
    Symbol symbol("test");
    rttr::instance instance(symbol);
    
    auto result = DynamicPropertyAccess::get_property(instance, "nonexistent");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), PropertyAccessError::PropertyNotFound);
}

// Test get_property with invalid instance
TEST_F(DynamicPropertyTest, GetProperty_InvalidInstance) {
    rttr::instance instance;
    
    auto result = DynamicPropertyAccess::get_property(instance, "name");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), PropertyAccessError::InvalidInstance);
}

// Test set_property with valid property
TEST_F(DynamicPropertyTest, SetProperty_ValidProperty) {
    Symbol symbol("test");
    rttr::instance instance(symbol);
    
    rttr::variant new_value = std::string("updated");
    auto result = DynamicPropertyAccess::set_property(instance, "name", new_value);
    ASSERT_TRUE(result.has_value());
    
    // Verify the value was set
    auto get_result = DynamicPropertyAccess::get_property(instance, "name");
    ASSERT_TRUE(get_result.has_value());
    EXPECT_EQ(get_result.value().get_value<std::string>(), "updated");
}

// Test set_property with invalid property
TEST_F(DynamicPropertyTest, SetProperty_InvalidProperty) {
    Symbol symbol("test");
    rttr::instance instance(symbol);
    
    rttr::variant new_value = std::string("updated");
    auto result = DynamicPropertyAccess::set_property(instance, "nonexistent", new_value);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), PropertyAccessError::PropertyNotFound);
}

// Test set_property with read-only property
TEST_F(DynamicPropertyTest, SetProperty_ReadOnlyProperty) {
    Function func({}, Value());
    rttr::instance instance(func);
    
    // "parameters" is a read-only property
    rttr::variant new_value = std::vector<std::shared_ptr<Symbol>>();
    auto result = DynamicPropertyAccess::set_property(instance, "parameters", new_value);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), PropertyAccessError::PropertyReadOnly);
}

// Test set_property with invalid instance
TEST_F(DynamicPropertyTest, SetProperty_InvalidInstance) {
    rttr::instance instance;
    
    rttr::variant new_value = std::string("test");
    auto result = DynamicPropertyAccess::set_property(instance, "name", new_value);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), PropertyAccessError::InvalidInstance);
}

// Test get_property with type name
TEST_F(DynamicPropertyTest, GetPropertyWithType_ValidType) {
    Symbol symbol("test");
    rttr::instance instance(symbol);
    
    auto result = DynamicPropertyAccess::get_property(instance, "Symbol", "name");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().get_value<std::string>(), "test");
}

// Test get_property with wrong type name
TEST_F(DynamicPropertyTest, GetPropertyWithType_WrongType) {
    Symbol symbol("test");
    rttr::instance instance(symbol);
    
    auto result = DynamicPropertyAccess::get_property(instance, "Integer", "name");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), PropertyAccessError::TypeMismatch);
}

// Test get_property with invalid type name
TEST_F(DynamicPropertyTest, GetPropertyWithType_InvalidType) {
    Symbol symbol("test");
    rttr::instance instance(symbol);
    
    auto result = DynamicPropertyAccess::get_property(instance, "NonExistent", "name");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), PropertyAccessError::TypeNotFound);
}

// Test has_property
TEST_F(DynamicPropertyTest, HasProperty_ValidProperty) {
    Symbol symbol("test");
    rttr::instance instance(symbol);
    
    EXPECT_TRUE(DynamicPropertyAccess::has_property(instance, "name"));
    EXPECT_FALSE(DynamicPropertyAccess::has_property(instance, "nonexistent"));
}

// Test has_property with invalid instance
TEST_F(DynamicPropertyTest, HasProperty_InvalidInstance) {
    rttr::instance instance;
    
    EXPECT_FALSE(DynamicPropertyAccess::has_property(instance, "name"));
}

// Test is_readonly
TEST_F(DynamicPropertyTest, IsReadonly_WritableProperty) {
    Symbol symbol("test");
    rttr::instance instance(symbol);
    
    EXPECT_FALSE(DynamicPropertyAccess::is_readonly(instance, "name"));
}

// Test is_readonly with read-only property
TEST_F(DynamicPropertyTest, IsReadonly_ReadOnlyProperty) {
    Function func({}, Value());
    rttr::instance instance(func);
    
    EXPECT_TRUE(DynamicPropertyAccess::is_readonly(instance, "parameters"));
}

// Test is_readonly with invalid property
TEST_F(DynamicPropertyTest, IsReadonly_InvalidProperty) {
    Symbol symbol("test");
    rttr::instance instance(symbol);
    
    EXPECT_TRUE(DynamicPropertyAccess::is_readonly(instance, "nonexistent"));
}

// Test get_property_type
TEST_F(DynamicPropertyTest, GetPropertyType_ValidProperty) {
    Symbol symbol("test");
    rttr::instance instance(symbol);
    
    auto type = DynamicPropertyAccess::get_property_type(instance, "name");
    ASSERT_TRUE(type.has_value());
    EXPECT_EQ(type->get_name().to_string(), "std::string");
}

// Test get_property_type with invalid property
TEST_F(DynamicPropertyTest, GetPropertyType_InvalidProperty) {
    Symbol symbol("test");
    rttr::instance instance(symbol);
    
    auto type = DynamicPropertyAccess::get_property_type(instance, "nonexistent");
    EXPECT_FALSE(type.has_value());
}

// Test value_to_variant
TEST_F(DynamicPropertyTest, ValueToVariant_Symbol) {
    auto symbol = std::make_shared<Symbol>("test");
    Value value(symbol);
    
    rttr::variant variant = DynamicPropertyAccess::value_to_variant(value);
    EXPECT_TRUE(variant.is_valid());
    EXPECT_TRUE(variant.is_type<std::shared_ptr<Symbol>>());
}

// Test value_to_variant with Integer
TEST_F(DynamicPropertyTest, ValueToVariant_Integer) {
    auto integer = std::make_shared<Integer>(42);
    Value value(integer);
    
    rttr::variant variant = DynamicPropertyAccess::value_to_variant(value);
    EXPECT_TRUE(variant.is_valid());
    EXPECT_TRUE(variant.is_type<std::shared_ptr<Integer>>());
}

// Test variant_to_value with Symbol
TEST_F(DynamicPropertyTest, VariantToValue_Symbol) {
    auto symbol = std::make_shared<Symbol>("test");
    rttr::variant variant(symbol);
    
    auto result = DynamicPropertyAccess::variant_to_value(variant);
    ASSERT_TRUE(result.has_value());
    
    // Verify it's a Symbol
    EXPECT_TRUE(result.value().is<Symbol>());
}

// Test variant_to_value with Integer
TEST_F(DynamicPropertyTest, VariantToValue_Integer) {
    auto integer = std::make_shared<Integer>(42);
    rttr::variant variant(integer);
    
    auto result = DynamicPropertyAccess::variant_to_value(variant);
    ASSERT_TRUE(result.has_value());
    
    // Verify it's an Integer
    EXPECT_TRUE(result.value().is<Integer>());
}

// Test variant_to_value with basic string
TEST_F(DynamicPropertyTest, VariantToValue_BasicString) {
    rttr::variant variant(std::string("test"));
    
    auto result = DynamicPropertyAccess::variant_to_value(variant);
    ASSERT_TRUE(result.has_value());
    
    // Should be converted to String
    EXPECT_TRUE(result.value().is<String>());
}

// Test variant_to_value with basic int
TEST_F(DynamicPropertyTest, VariantToValue_BasicInt) {
    rttr::variant variant(42);
    
    auto result = DynamicPropertyAccess::variant_to_value(variant);
    ASSERT_TRUE(result.has_value());
    
    // Should be converted to Integer
    EXPECT_TRUE(result.value().is<Integer>());
}

// Test variant_to_value with basic bool
TEST_F(DynamicPropertyTest, VariantToValue_BasicBool) {
    rttr::variant variant(true);
    
    auto result = DynamicPropertyAccess::variant_to_value(variant);
    ASSERT_TRUE(result.has_value());
    
    // Should be converted to Boolean
    EXPECT_TRUE(result.value().is<Boolean>());
}

// Test variant_to_value with invalid variant
TEST_F(DynamicPropertyTest, VariantToValue_InvalidVariant) {
    rttr::variant variant;
    
    auto result = DynamicPropertyAccess::variant_to_value(variant);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), PropertyAccessError::ConversionFailed);
}

// Test get_all_properties
TEST_F(DynamicPropertyTest, GetAllProperties_ValidInstance) {
    Symbol symbol("test");
    rttr::instance instance(symbol);
    
    auto props = DynamicPropertyAccess::get_all_properties(instance);
    EXPECT_FALSE(props.empty());
    
    // Should have "name" property
    auto it = std::find(props.begin(), props.end(), "name");
    EXPECT_NE(it, props.end());
}

// Test get_all_properties with invalid instance
TEST_F(DynamicPropertyTest, GetAllProperties_InvalidInstance) {
    rttr::instance instance;
    
    auto props = DynamicPropertyAccess::get_all_properties(instance);
    EXPECT_TRUE(props.empty());
}

// Test get_all_property_values
TEST_F(DynamicPropertyTest, GetAllPropertyValues_ValidInstance) {
    Symbol symbol("test");
    rttr::instance instance(symbol);
    
    auto values = DynamicPropertyAccess::get_all_property_values(instance);
    EXPECT_FALSE(values.empty());
    
    // Should have "name" property with value
    EXPECT_TRUE(values.contains("name"));
    EXPECT_EQ(values["name"].get_value<std::string>(), "test");
}

// Test get_all_property_values with invalid instance
TEST_F(DynamicPropertyTest, GetAllPropertyValues_InvalidInstance) {
    rttr::instance instance;
    
    auto values = DynamicPropertyAccess::get_all_property_values(instance);
    EXPECT_TRUE(values.empty());
}

// Test set_properties
TEST_F(DynamicPropertyTest, SetProperties_ValidProperties) {
    Symbol symbol("test");
    rttr::instance instance(symbol);
    
    std::unordered_map<std::string, rttr::variant> props;
    props["name"] = std::string("updated");
    
    auto result = DynamicPropertyAccess::set_properties(instance, props);
    ASSERT_TRUE(result.has_value());
    
    // Verify the value was set
    auto get_result = DynamicPropertyAccess::get_property(instance, "name");
    ASSERT_TRUE(get_result.has_value());
    EXPECT_EQ(get_result.value().get_value<std::string>(), "updated");
}

// Test set_properties with invalid property
TEST_F(DynamicPropertyTest, SetProperties_InvalidProperty) {
    Symbol symbol("test");
    rttr::instance instance(symbol);
    
    std::unordered_map<std::string, rttr::variant> props;
    props["nonexistent"] = std::string("value");
    
    auto result = DynamicPropertyAccess::set_properties(instance, props);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), PropertyAccessError::PropertyNotFound);
}

// Test error message conversion
TEST_F(DynamicPropertyTest, ErrorToString) {
    EXPECT_EQ(to_string(PropertyAccessError::TypeNotFound), "Type not found");
    EXPECT_EQ(to_string(PropertyAccessError::PropertyNotFound), "Property not found");
    EXPECT_EQ(to_string(PropertyAccessError::PropertyReadOnly), "Property is read-only");
    EXPECT_EQ(to_string(PropertyAccessError::TypeMismatch), "Type mismatch");
    EXPECT_EQ(to_string(PropertyAccessError::ConversionFailed), "Conversion failed");
    EXPECT_EQ(to_string(PropertyAccessError::InvalidInstance), "Invalid instance");
}
