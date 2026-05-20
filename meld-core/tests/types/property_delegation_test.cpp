#include <gtest/gtest.h>
#include "meld/types/property.hpp"
#include "meld/types/delegate.hpp"
#include "meld/types/instance.hpp"
#include "meld/meta/metatype.hpp"

using namespace meld::types;
using namespace meld::meta;
using namespace meld::kernel;

// Helper to create a NativeHandle wrapping a PropertyDelegate
Value wrap_delegate(std::shared_ptr<PropertyDelegate> delegate) {
    return Value(std::make_shared<NativeHandle>(delegate.get(), "delegate"));
}

// Test lazy delegate property
TEST(PropertyDelegationTest, LazyDelegateProperty) {
    auto& registry = TypeRegistry::instance();
    auto string_type = registry.get_string_type();
    
    // Create a lazy delegate
    int call_count = 0;
    auto delegate = DelegateFactory::lazy([&call_count]() -> Value {
        call_count++;
        return make_string("lazy_value");
    });
    
    // Create a property with lazy delegation
    auto prop = PropertyBuilder("lazyName", string_type)
        .immutable_property()
        .delegated_to("lazy_delegate", wrap_delegate(delegate))
        .build();
    
    EXPECT_TRUE(prop.is_delegated);
    EXPECT_FALSE(prop.has_backing_field);
    
    // Access the property multiple times
    PropertyContext context{
        nullptr,
        &prop,
        AccessContext{AccessModifier::Public, true, false, true}
    };
    
    // First access - should initialize
    auto result1 = PropertyAccessor::get_value(context);
    ASSERT_TRUE(result1.has_value());
    EXPECT_EQ(call_count, 1);
    
    // Second access - should use cached value
    auto result2 = PropertyAccessor::get_value(context);
    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ(call_count, 1);  // Not called again
}

// Test observable delegate property
TEST(PropertyDelegationTest, ObservableDelegateProperty) {
    auto& registry = TypeRegistry::instance();
    auto int_type = registry.get_int_type();
    
    // Create an observable delegate
    Value old_observed;
    Value new_observed;
    bool callback_called = false;
    
    auto delegate = DelegateFactory::observable(
        make_int(0),
        [&](const Value& old_val, const Value& new_val) {
            old_observed = old_val;
            new_observed = new_val;
            callback_called = true;
        }
    );
    
    // Create a property with observable delegation
    auto prop = PropertyBuilder("count", int_type)
        .mutable_property()
        .delegated_to("observable_delegate", wrap_delegate(delegate))
        .build();
    
    EXPECT_TRUE(prop.is_delegated);
    EXPECT_TRUE(prop.is_mutable);
    
    // Set the property value
    PropertyContext context{
        nullptr,
        &prop,
        AccessContext{AccessModifier::Public, true, false, true}
    };
    
    auto new_value = make_int(42);
    auto result = PropertyAccessor::set_value(context, new_value);
    
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(callback_called);
}

// Test validated delegate property
TEST(PropertyDelegationTest, ValidatedDelegateProperty) {
    auto& registry = TypeRegistry::instance();
    auto int_type = registry.get_int_type();
    
    // Create a validated delegate (only positive numbers)
    auto delegate = DelegateFactory::validated(
        make_int(0),
        [](const Value& val) -> std::expected<void, std::string> {
            if (!val.is<Integer>()) {
                return std::unexpected("Value must be an integer");
            }
            auto int_val = val.as<Integer>();
            if (int_val->value() < 0) {
                return std::unexpected("Value must be positive");
            }
            return {};
        }
    );
    
    // Create a property with validated delegation
    auto prop = PropertyBuilder("positiveValue", int_type)
        .mutable_property()
        .delegated_to("validated_delegate", wrap_delegate(delegate))
        .build();
    
    PropertyContext context{
        nullptr,
        &prop,
        AccessContext{AccessModifier::Public, true, false, true}
    };
    
    // Try to set a valid value
    auto valid_result = PropertyAccessor::set_value(context, make_int(10));
    EXPECT_TRUE(valid_result.has_value());
    
    // Try to set an invalid value
    auto invalid_result = PropertyAccessor::set_value(context, make_int(-5));
    EXPECT_FALSE(invalid_result.has_value());
    EXPECT_TRUE(invalid_result.error().find("positive") != std::string::npos);
}

// Test mapped delegate property
TEST(PropertyDelegationTest, MappedDelegateProperty) {
    auto& registry = TypeRegistry::instance();
    auto float_type = registry.get_float_type();
    
    // Create a mapped delegate (Celsius to Fahrenheit)
    auto delegate = DelegateFactory::mapped(
        make_float(0.0),
        // Getter: C to F
        [](const Value& val) -> Value {
            if (val.is<Float>()) {
                auto celsius = val.as<Float>()->value();
                auto fahrenheit = celsius * 9.0 / 5.0 + 32.0;
                return make_float(fahrenheit);
            }
            return val;
        },
        // Setter: F to C
        [](const Value& val) -> Value {
            if (val.is<Float>()) {
                auto fahrenheit = val.as<Float>()->value();
                auto celsius = (fahrenheit - 32.0) * 5.0 / 9.0;
                return make_float(celsius);
            }
            return val;
        }
    );
    
    // Create a property with mapped delegation
    auto prop = PropertyBuilder("fahrenheit", float_type)
        .mutable_property()
        .delegated_to("mapped_delegate", wrap_delegate(delegate))
        .build();
    
    PropertyContext context{
        nullptr,
        &prop,
        AccessContext{AccessModifier::Public, true, false, true}
    };
    
    // Set Fahrenheit value (should be converted to Celsius internally)
    auto set_result = PropertyAccessor::set_value(context, make_float(32.0));
    EXPECT_TRUE(set_result.has_value());
    
    // Get Fahrenheit value (should be converted from Celsius)
    auto get_result = PropertyAccessor::get_value(context);
    ASSERT_TRUE(get_result.has_value());
}

// Test that lazy delegate cannot be set
TEST(PropertyDelegationTest, LazyDelegateCannotBeSet) {
    auto& registry = TypeRegistry::instance();
    auto string_type = registry.get_string_type();
    
    auto delegate = DelegateFactory::lazy([]() -> Value {
        return make_string("computed");
    });
    
    auto prop = PropertyBuilder("computed", string_type)
        .mutable_property()  // Even if marked mutable
        .delegated_to("lazy_delegate", wrap_delegate(delegate))
        .build();
    
    PropertyContext context{
        nullptr,
        &prop,
        AccessContext{AccessModifier::Public, true, false, true}
    };
    
    // Try to set (should fail)
    auto result = PropertyAccessor::set_value(context, make_string("new_value"));
    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().find("Cannot set") != std::string::npos);
}

// Test delegated property on struct
TEST(PropertyDelegationTest, DelegatedPropertyOnStruct) {
    auto& registry = TypeRegistry::instance();
    auto int_type = registry.get_int_type();
    
    // Create a lazy delegate
    auto delegate = DelegateFactory::lazy([]() -> Value {
        return make_int(100);
    });
    
    // Create a struct with delegated property
    std::vector<Field> fields;  // No backing fields needed
    
    std::vector<Property> properties;
    properties.push_back(
        PropertyBuilder("lazyValue", int_type)
            .immutable_property()
            .delegated_to("lazy_delegate", wrap_delegate(delegate))
            .build()
    );
    
    auto struct_type = std::dynamic_pointer_cast<StructMetaType>(
        MetaType::create_struct("LazyData", fields, properties)
    );
    
    auto instance = create_struct_instance(struct_type);
    
    // Access the delegated property
    PropertyContext context{
        instance,
        &struct_type->properties()[0],
        AccessContext{AccessModifier::Public, true, false, true}
    };
    
    auto result = PropertyAccessor::get_value(context);
    ASSERT_TRUE(result.has_value());
    
    auto int_result = result.value().try_as<Integer>();
    ASSERT_TRUE(int_result.has_value());
    EXPECT_EQ(int_result.value()->value(), 100);
}

// Test multiple delegated properties on same class
TEST(PropertyDelegationTest, MultipleDelegatedProperties) {
    auto& registry = TypeRegistry::instance();
    auto string_type = registry.get_string_type();
    auto int_type = registry.get_int_type();
    
    // Create multiple delegates
    auto lazy_delegate = DelegateFactory::lazy([]() -> Value {
        return make_string("lazy_computed");
    });
    
    auto observable_delegate = DelegateFactory::observable(
        make_int(0),
        nullptr  // No callback for this test
    );
    
    // Create a class with multiple delegated properties
    std::vector<Field> fields;
    std::vector<Property> properties;
    
    properties.push_back(
        PropertyBuilder("lazyName", string_type)
            .immutable_property()
            .delegated_to("lazy_delegate", wrap_delegate(lazy_delegate))
            .build()
    );
    
    properties.push_back(
        PropertyBuilder("observableCount", int_type)
            .mutable_property()
            .delegated_to("observable_delegate", wrap_delegate(observable_delegate))
            .build()
    );
    
    auto class_type = std::dynamic_pointer_cast<ClassMetaType>(
        MetaType::create_class("DelegatedClass", fields, {}, properties)
    );
    
    EXPECT_EQ(class_type->properties().size(), 2);
    EXPECT_TRUE(class_type->properties()[0].is_delegated);
    EXPECT_TRUE(class_type->properties()[1].is_delegated);
}

// Test delegation with access modifiers
TEST(PropertyDelegationTest, DelegationWithAccessModifiers) {
    auto& registry = TypeRegistry::instance();
    auto int_type = registry.get_int_type();
    
    auto delegate = DelegateFactory::observable(make_int(0), nullptr);
    
    // Create a property with delegation and private setter
    auto prop = PropertyBuilder("balance", int_type)
        .mutable_property()
        .delegated_to("observable_delegate", wrap_delegate(delegate))
        .getter_access(AccessModifier::Public)
        .setter_access(AccessModifier::Private)
        .build();
    
    EXPECT_TRUE(prop.is_delegated);
    EXPECT_EQ(prop.getter_access, AccessModifier::Public);
    EXPECT_EQ(prop.setter_access, AccessModifier::Private);
    
    // Public access can read
    PropertyContext public_context{
        nullptr,
        &prop,
        AccessContext{AccessModifier::Public, false, false, false}
    };
    
    auto get_result = PropertyAccessor::get_value(public_context);
    EXPECT_TRUE(get_result.has_value());
    
    // Public access cannot write
    auto set_result = PropertyAccessor::set_value(public_context, make_int(100));
    EXPECT_FALSE(set_result.has_value());
    EXPECT_TRUE(set_result.error().find("insufficient access") != std::string::npos);
}
