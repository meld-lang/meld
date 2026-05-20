#include <gtest/gtest.h>
#include "meld/meta/reflection_helper.hpp"
#include "meld/stdlib/reflect.hpp"
#include "meld/meta/type_registration.hpp"
#include "meld/kernel/primitives.hpp"
#include "meld/meta/metatype.hpp"

using namespace meld::meta;
using namespace meld::stdlib;
using namespace meld::kernel;

class ReflectionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize type registration
        initialize_meld_types();
    }
};

// Test ReflectionHelper::type_of
TEST_F(ReflectionTest, ReflectionHelper_TypeOf) {
    rttr::type symbol_type = ReflectionHelper::type_of<Symbol>();
    EXPECT_TRUE(symbol_type.is_valid());
    EXPECT_EQ(std::string(symbol_type.get_name()), "Symbol");
    
    rttr::type integer_type = ReflectionHelper::type_of<Integer>();
    EXPECT_TRUE(integer_type.is_valid());
    EXPECT_EQ(std::string(integer_type.get_name()), "Integer");
}

// Test ReflectionHelper::type_by_name
TEST_F(ReflectionTest, ReflectionHelper_TypeByName) {
    rttr::type symbol_type = ReflectionHelper::type_by_name("Symbol");
    EXPECT_TRUE(symbol_type.is_valid());
    EXPECT_EQ(std::string(symbol_type.get_name()), "Symbol");
    
    rttr::type invalid_type = ReflectionHelper::type_by_name("NonExistent");
    EXPECT_FALSE(invalid_type.is_valid());
}

// Test ReflectionHelper::get_properties
TEST_F(ReflectionTest, ReflectionHelper_GetProperties) {
    rttr::type symbol_type = ReflectionHelper::type_by_name("Symbol");
    auto props = ReflectionHelper::get_properties(symbol_type);
    
    EXPECT_FALSE(props.empty());
    
    // Symbol should have a "name" property
    bool has_name = false;
    for (const auto& prop : props) {
        if (prop.get_name() == std::string("name")) {
            has_name = true;
            break;
        }
    }
    EXPECT_TRUE(has_name);
}

// Test ReflectionHelper::get_property
TEST_F(ReflectionTest, ReflectionHelper_GetProperty) {
    rttr::type symbol_type = ReflectionHelper::type_by_name("Symbol");
    
    auto name_prop = ReflectionHelper::get_property(symbol_type, "name");
    EXPECT_TRUE(name_prop.has_value());
    EXPECT_EQ(std::string(name_prop->get_name()), "name");
    
    auto invalid_prop = ReflectionHelper::get_property(symbol_type, "nonexistent");
    EXPECT_FALSE(invalid_prop.has_value());
}

// Test ReflectionHelper::get_methods
TEST_F(ReflectionTest, ReflectionHelper_GetMethods) {
    rttr::type symbol_type = ReflectionHelper::type_by_name("Symbol");
    auto methods = ReflectionHelper::get_methods(symbol_type);
    
    EXPECT_FALSE(methods.empty());
    
    // Symbol should have a "to_string" method
    bool has_to_string = false;
    for (const auto& method : methods) {
        if (method.get_name() == std::string("to_string")) {
            has_to_string = true;
            break;
        }
    }
    EXPECT_TRUE(has_to_string);
}

// Test ReflectionHelper::get_method
TEST_F(ReflectionTest, ReflectionHelper_GetMethod) {
    rttr::type symbol_type = ReflectionHelper::type_by_name("Symbol");
    
    auto to_string_method = ReflectionHelper::get_method(symbol_type, "to_string");
    EXPECT_TRUE(to_string_method.has_value());
    EXPECT_EQ(std::string(to_string_method->get_name()), "to_string");
    
    auto invalid_method = ReflectionHelper::get_method(symbol_type, "nonexistent");
    EXPECT_FALSE(invalid_method.has_value());
}

// Test ReflectionHelper::get_constructors
TEST_F(ReflectionTest, ReflectionHelper_GetConstructors) {
    rttr::type symbol_type = ReflectionHelper::type_by_name("Symbol");
    auto ctors = ReflectionHelper::get_constructors(symbol_type);
    
    EXPECT_FALSE(ctors.empty());
}

// Test ReflectionHelper::has_property
TEST_F(ReflectionTest, ReflectionHelper_HasProperty) {
    rttr::type symbol_type = ReflectionHelper::type_by_name("Symbol");
    
    EXPECT_TRUE(ReflectionHelper::has_property(symbol_type, "name"));
    EXPECT_FALSE(ReflectionHelper::has_property(symbol_type, "nonexistent"));
}

// Test ReflectionHelper::has_method
TEST_F(ReflectionTest, ReflectionHelper_HasMethod) {
    rttr::type symbol_type = ReflectionHelper::type_by_name("Symbol");
    
    EXPECT_TRUE(ReflectionHelper::has_method(symbol_type, "to_string"));
    EXPECT_FALSE(ReflectionHelper::has_method(symbol_type, "nonexistent"));
}

// Test ReflectionHelper::get_base_classes
TEST_F(ReflectionTest, ReflectionHelper_GetBaseClasses) {
    rttr::type primitive_type = ReflectionHelper::type_by_name("PrimitiveMetaType");
    auto bases = ReflectionHelper::get_base_classes(primitive_type);
    
    // PrimitiveMetaType should derive from MetaType
    EXPECT_FALSE(bases.empty());
    
    bool has_metatype = false;
    for (const auto& base : bases) {
        if (std::string(base.get_name()) == std::string("MetaType")) {
            has_metatype = true;
            break;
        }
    }
    EXPECT_TRUE(has_metatype);
}

// Test ReflectionHelper::is_derived_from
TEST_F(ReflectionTest, ReflectionHelper_IsDerivedFrom) {
    rttr::type metatype = ReflectionHelper::type_by_name("MetaType");
    rttr::type primitive = ReflectionHelper::type_by_name("PrimitiveMetaType");
    rttr::type struct_type = ReflectionHelper::type_by_name("StructMetaType");
    
    EXPECT_TRUE(ReflectionHelper::is_derived_from(primitive, metatype));
    EXPECT_TRUE(ReflectionHelper::is_derived_from(struct_type, metatype));
    EXPECT_FALSE(ReflectionHelper::is_derived_from(metatype, primitive));
}

// Test ReflectionHelper::describe_type
TEST_F(ReflectionTest, ReflectionHelper_DescribeType) {
    rttr::type symbol_type = ReflectionHelper::type_by_name("Symbol");
    std::string description = ReflectionHelper::describe_type(symbol_type);
    
    EXPECT_FALSE(description.empty());
    EXPECT_NE(description.find("Symbol"), std::string::npos);
    EXPECT_NE(description.find("Properties"), std::string::npos);
    EXPECT_NE(description.find("Methods"), std::string::npos);
}

// Test ReflectionHelper::describe_property
TEST_F(ReflectionTest, ReflectionHelper_DescribeProperty) {
    rttr::type symbol_type = ReflectionHelper::type_by_name("Symbol");
    auto name_prop = ReflectionHelper::get_property(symbol_type, "name");
    ASSERT_TRUE(name_prop.has_value());
    
    std::string description = ReflectionHelper::describe_property(*name_prop);
    
    EXPECT_FALSE(description.empty());
    EXPECT_NE(description.find("name"), std::string::npos);
    EXPECT_NE(description.find("Type:"), std::string::npos);
}

// Test ReflectionHelper::describe_method
TEST_F(ReflectionTest, ReflectionHelper_DescribeMethod) {
    rttr::type symbol_type = ReflectionHelper::type_by_name("Symbol");
    auto to_string_method = ReflectionHelper::get_method(symbol_type, "to_string");
    ASSERT_TRUE(to_string_method.has_value());
    
    std::string description = ReflectionHelper::describe_method(*to_string_method);
    
    EXPECT_FALSE(description.empty());
    EXPECT_NE(description.find("to_string"), std::string::npos);
    EXPECT_NE(description.find("Return type:"), std::string::npos);
}

// Test reflect::typeOf with string
TEST_F(ReflectionTest, Reflect_TypeOfString) {
    rttr::type symbol_type = reflect::typeOf("Symbol");
    EXPECT_TRUE(symbol_type.is_valid());
    EXPECT_EQ(std::string(symbol_type.get_name()), "Symbol");
}

// Test reflect::typeOf with template
TEST_F(ReflectionTest, Reflect_TypeOfTemplate) {
    rttr::type symbol_type = reflect::typeOf<Symbol>();
    EXPECT_TRUE(symbol_type.is_valid());
    EXPECT_EQ(std::string(symbol_type.get_name()), "Symbol");
}

// Test reflect::getProperties
TEST_F(ReflectionTest, Reflect_GetProperties) {
    auto props = reflect::getProperties("Symbol");
    EXPECT_FALSE(props.empty());
    
    bool has_name = std::find(props.begin(), props.end(), "name") != props.end();
    EXPECT_TRUE(has_name);
}

// Test reflect::getMethods
TEST_F(ReflectionTest, Reflect_GetMethods) {
    auto methods = reflect::getMethods("Symbol");
    EXPECT_FALSE(methods.empty());
    
    bool has_to_string = std::find(methods.begin(), methods.end(), "to_string") != methods.end();
    EXPECT_TRUE(has_to_string);
}

// Test reflect::getConstructorCount
TEST_F(ReflectionTest, Reflect_GetConstructorCount) {
    size_t count = reflect::getConstructorCount("Symbol");
    EXPECT_GT(count, 0);
}

// Test reflect::hasProperty
TEST_F(ReflectionTest, Reflect_HasProperty) {
    EXPECT_TRUE(reflect::hasProperty("Symbol", "name"));
    EXPECT_FALSE(reflect::hasProperty("Symbol", "nonexistent"));
}

// Test reflect::hasMethod
TEST_F(ReflectionTest, Reflect_HasMethod) {
    EXPECT_TRUE(reflect::hasMethod("Symbol", "to_string"));
    EXPECT_FALSE(reflect::hasMethod("Symbol", "nonexistent"));
}

// Test reflect::getBaseClasses
TEST_F(ReflectionTest, Reflect_GetBaseClasses) {
    auto bases = reflect::getBaseClasses("PrimitiveMetaType");
    EXPECT_FALSE(bases.empty());
    
    bool has_metatype = std::find(bases.begin(), bases.end(), "MetaType") != bases.end();
    EXPECT_TRUE(has_metatype);
}

// Test reflect::isDerivedFrom
TEST_F(ReflectionTest, Reflect_IsDerivedFrom) {
    EXPECT_TRUE(reflect::isDerivedFrom("PrimitiveMetaType", "MetaType"));
    EXPECT_TRUE(reflect::isDerivedFrom("StructMetaType", "MetaType"));
    EXPECT_FALSE(reflect::isDerivedFrom("MetaType", "PrimitiveMetaType"));
}

// Test reflect::describeType
TEST_F(ReflectionTest, Reflect_DescribeType) {
    std::string description = reflect::describeType("Symbol");
    
    EXPECT_FALSE(description.empty());
    EXPECT_NE(description.find("Symbol"), std::string::npos);
}

// Test reflect::describeProperty
TEST_F(ReflectionTest, Reflect_DescribeProperty) {
    std::string description = reflect::describeProperty("Symbol", "name");
    
    EXPECT_FALSE(description.empty());
    EXPECT_NE(description.find("name"), std::string::npos);
}

// Test reflect::describeMethod
TEST_F(ReflectionTest, Reflect_DescribeMethod) {
    std::string description = reflect::describeMethod("Symbol", "to_string");
    
    EXPECT_FALSE(description.empty());
    EXPECT_NE(description.find("to_string"), std::string::npos);
}

// Test reflect::getAllTypes
TEST_F(ReflectionTest, Reflect_GetAllTypes) {
    auto types = reflect::getAllTypes();
    EXPECT_FALSE(types.empty());
    
    bool has_symbol = std::find(types.begin(), types.end(), "Symbol") != types.end();
    EXPECT_TRUE(has_symbol);
}

// Test reflect::isTypeRegistered
TEST_F(ReflectionTest, Reflect_IsTypeRegistered) {
    EXPECT_TRUE(reflect::isTypeRegistered("Symbol"));
    EXPECT_TRUE(reflect::isTypeRegistered("Integer"));
    EXPECT_FALSE(reflect::isTypeRegistered("NonExistent"));
}

// Test error handling for invalid types
TEST_F(ReflectionTest, Reflect_InvalidTypeHandling) {
    auto props = reflect::getProperties("NonExistent");
    EXPECT_TRUE(props.empty());
    
    auto methods = reflect::getMethods("NonExistent");
    EXPECT_TRUE(methods.empty());
    
    EXPECT_EQ(reflect::getConstructorCount("NonExistent"), 0);
    EXPECT_FALSE(reflect::hasProperty("NonExistent", "anything"));
    EXPECT_FALSE(reflect::hasMethod("NonExistent", "anything"));
}

// Test dynamic property access
TEST_F(ReflectionTest, Reflect_GetSetPropertyValue) {
    // Create a Symbol instance
    Symbol sym("test");
    rttr::instance instance(sym);
    
    // Get property value
    rttr::variant name_value = reflect::getPropertyValue(instance, "name");
    EXPECT_TRUE(name_value.is_valid());
    EXPECT_EQ(name_value.get_value<std::string>(), "test");
    
    // Try to get non-existent property
    rttr::variant invalid = reflect::getPropertyValue(instance, "nonexistent");
    EXPECT_FALSE(invalid.is_valid());
}

// Test dynamic method invocation
TEST_F(ReflectionTest, Reflect_InvokeMethod) {
    // Create a Symbol instance
    Symbol sym("hello");
    rttr::instance instance(sym);
    
    // Invoke to_string method
    rttr::variant result = reflect::invokeMethod(instance, "to_string");
    EXPECT_TRUE(result.is_valid());
    EXPECT_EQ(result.get_value<std::string>(), "hello");
    
    // Invoke hash method
    rttr::variant hash_result = reflect::invokeMethod(instance, "hash");
    EXPECT_TRUE(hash_result.is_valid());
    
    // Try to invoke non-existent method
    rttr::variant invalid = reflect::invokeMethod(instance, "nonexistent");
    EXPECT_FALSE(invalid.is_valid());
}

// Test dynamic instance creation
TEST_F(ReflectionTest, Reflect_CreateInstance) {
    // Create instance with default constructor
    rttr::variant instance = reflect::createInstance("Symbol");
    EXPECT_TRUE(instance.is_valid());
    
    // Create instance with parameterized constructor
    std::vector<rttr::variant> args = { rttr::variant(std::string("test")) };
    rttr::variant instance2 = reflect::createInstance("Symbol", args);
    EXPECT_TRUE(instance2.is_valid());
    
    // Verify the created instance
    rttr::variant name = reflect::getPropertyValue(instance2, "name");
    EXPECT_TRUE(name.is_valid());
    EXPECT_EQ(name.get_value<std::string>(), "test");
    
    // Try to create instance of non-existent type
    rttr::variant invalid = reflect::createInstance("NonExistent");
    EXPECT_FALSE(invalid.is_valid());
}

// Test ReflectionHelper property access
TEST_F(ReflectionTest, ReflectionHelper_PropertyAccess) {
    Symbol sym("test");
    rttr::instance instance(sym);
    
    // Get property value
    rttr::variant value = ReflectionHelper::get_property_value(instance, "name");
    EXPECT_TRUE(value.is_valid());
    EXPECT_EQ(value.get_value<std::string>(), "test");
}

// Test ReflectionHelper method invocation
TEST_F(ReflectionTest, ReflectionHelper_MethodInvocation) {
    Symbol sym("world");
    rttr::instance instance(sym);
    
    // Invoke method
    rttr::variant result = ReflectionHelper::invoke_method(instance, "to_string");
    EXPECT_TRUE(result.is_valid());
    EXPECT_EQ(result.get_value<std::string>(), "world");
}

// Test ReflectionHelper instance creation
TEST_F(ReflectionTest, ReflectionHelper_CreateInstance) {
    rttr::type symbol_type = ReflectionHelper::type_by_name("Symbol");
    
    // Create with default constructor
    rttr::variant instance = ReflectionHelper::create_instance(symbol_type);
    EXPECT_TRUE(instance.is_valid());
    
    // Create with parameterized constructor
    std::vector<rttr::variant> args = { rttr::variant(std::string("created")) };
    rttr::variant instance2 = ReflectionHelper::create_instance(symbol_type, args);
    EXPECT_TRUE(instance2.is_valid());
}
