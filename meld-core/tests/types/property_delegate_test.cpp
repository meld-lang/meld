#include <gtest/gtest.h>
#include "meld/types/delegate.hpp"
#include "meld/types/property.hpp"
#include "meld/meta/metatype.hpp"

using namespace meld::types;
using namespace meld::meta;
using namespace meld::kernel;

// Test LazyDelegate
TEST(PropertyDelegateTest, LazyDelegateInitializesOnFirstAccess) {
    int call_count = 0;
    auto initializer = [&call_count]() -> Value {
        call_count++;
        return Value();  // Return some value
    };
    
    LazyDelegate lazy(initializer);
    
    EXPECT_EQ(call_count, 0);  // Not initialized yet
    
    auto result1 = lazy.get_value();
    EXPECT_TRUE(result1.has_value());
    EXPECT_EQ(call_count, 1);  // Initialized on first access
    
    auto result2 = lazy.get_value();
    EXPECT_TRUE(result2.has_value());
    EXPECT_EQ(call_count, 1);  // Cached, not called again
}

// Test LazyDelegate cannot be set
TEST(PropertyDelegateTest, LazyDelegateCannotBeSet) {
    auto initializer = []() -> Value { return Value(); };
    LazyDelegate lazy(initializer);
    
    Value new_value;
    auto result = lazy.set_value(new_value);
    
    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().find("Cannot set") != std::string::npos);
}

// Test ObservableDelegate
TEST(PropertyDelegateTest, ObservableDelegateNotifiesOnChange) {
    Value initial_value;
    Value observed_old;
    Value observed_new;
    bool callback_called = false;
    
    auto callback = [&](const Value& old_val, const Value& new_val) {
        observed_old = old_val;
        observed_new = new_val;
        callback_called = true;
    };
    
    ObservableDelegate observable(initial_value, callback);
    
    Value new_value;
    auto result = observable.set_value(new_value);
    
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(callback_called);
}

// Test ObservableDelegate get/set
TEST(PropertyDelegateTest, ObservableDelegateGetSet) {
    Value initial_value;
    ObservableDelegate observable(initial_value, nullptr);
    
    auto get_result = observable.get_value();
    EXPECT_TRUE(get_result.has_value());
    
    Value new_value;
    auto set_result = observable.set_value(new_value);
    EXPECT_TRUE(set_result.has_value());
}

// Test ValidatedDelegate with valid value
TEST(PropertyDelegateTest, ValidatedDelegateAcceptsValidValue) {
    Value initial_value;
    auto validator = [](const Value& val) -> std::expected<void, std::string> {
        // Always accept for this test
        return {};
    };
    
    ValidatedDelegate validated(initial_value, validator);
    
    Value new_value;
    auto result = validated.set_value(new_value);
    
    EXPECT_TRUE(result.has_value());
}

// Test ValidatedDelegate with invalid value
TEST(PropertyDelegateTest, ValidatedDelegateRejectsInvalidValue) {
    Value initial_value;
    auto validator = [](const Value& val) -> std::expected<void, std::string> {
        return std::unexpected("Validation failed");
    };
    
    ValidatedDelegate validated(initial_value, validator);
    
    Value new_value;
    auto result = validated.set_value(new_value);
    
    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().find("Validation failed") != std::string::npos);
}

// Test MappedDelegate
TEST(PropertyDelegateTest, MappedDelegateTransformsValues) {
    Value initial_value;
    
    bool getter_called = false;
    bool setter_called = false;
    
    auto getter = [&getter_called](const Value& val) -> Value {
        getter_called = true;
        return val;  // Transform on get
    };
    
    auto setter = [&setter_called](const Value& val) -> Value {
        setter_called = true;
        return val;  // Transform on set
    };
    
    MappedDelegate mapped(initial_value, getter, setter);
    
    auto get_result = mapped.get_value();
    EXPECT_TRUE(get_result.has_value());
    EXPECT_TRUE(getter_called);
    
    Value new_value;
    auto set_result = mapped.set_value(new_value);
    EXPECT_TRUE(set_result.has_value());
    EXPECT_TRUE(setter_called);
}

// Test DelegateFactory::lazy
TEST(PropertyDelegateTest, DelegateFactoryCreatesLazy) {
    auto initializer = []() -> Value { return Value(); };
    auto delegate = DelegateFactory::lazy(initializer);
    
    ASSERT_NE(delegate, nullptr);
    auto result = delegate->get_value();
    EXPECT_TRUE(result.has_value());
}

// Test DelegateFactory::observable
TEST(PropertyDelegateTest, DelegateFactoryCreatesObservable) {
    Value initial_value;
    bool callback_called = false;
    auto callback = [&callback_called](const Value&, const Value&) {
        callback_called = true;
    };
    
    auto delegate = DelegateFactory::observable(initial_value, callback);
    
    ASSERT_NE(delegate, nullptr);
    Value new_value;
    auto result = delegate->set_value(new_value);
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(callback_called);
}

// Test DelegateFactory::validated
TEST(PropertyDelegateTest, DelegateFactoryCreatesValidated) {
    Value initial_value;
    auto validator = [](const Value&) -> std::expected<void, std::string> {
        return {};
    };
    
    auto delegate = DelegateFactory::validated(initial_value, validator);
    
    ASSERT_NE(delegate, nullptr);
    Value new_value;
    auto result = delegate->set_value(new_value);
    EXPECT_TRUE(result.has_value());
}

// Test DelegateFactory::mapped
TEST(PropertyDelegateTest, DelegateFactoryCreatesMapped) {
    Value initial_value;
    auto getter = [](const Value& val) -> Value { return val; };
    auto setter = [](const Value& val) -> Value { return val; };
    
    auto delegate = DelegateFactory::mapped(initial_value, getter, setter);
    
    ASSERT_NE(delegate, nullptr);
    auto get_result = delegate->get_value();
    EXPECT_TRUE(get_result.has_value());
}

// Test PropertyBuilder with delegation
TEST(PropertyDelegateTest, PropertyBuilderWithDelegation) {
    auto string_type = TypeRegistry::instance().get_string_type();
    Value delegate_impl;
    
    auto prop = PropertyBuilder("lazy_value", string_type)
        .immutable_property()
        .delegated_to("lazy_delegate", delegate_impl)
        .build();
    
    EXPECT_EQ(prop.name, "lazy_value");
    EXPECT_TRUE(prop.is_delegated);
    EXPECT_EQ(prop.delegate_name, "lazy_delegate");
    EXPECT_FALSE(prop.has_backing_field);  // Delegated properties don't need backing fields
}

// Test delegated property structure
TEST(PropertyDelegateTest, DelegatedPropertyStructure) {
    auto int_type = TypeRegistry::instance().get_int_type();
    Value delegate_impl;
    
    auto prop = PropertyBuilder("computed", int_type)
        .immutable_property()
        .delegated_to("my_delegate", delegate_impl)
        .build();
    
    EXPECT_TRUE(prop.is_delegated);
    EXPECT_FALSE(prop.has_backing_field);
    EXPECT_FALSE(prop.has_custom_getter);
    EXPECT_FALSE(prop.has_custom_setter);
}

// Test class with delegated properties
TEST(PropertyDelegateTest, ClassWithDelegatedProperties) {
    auto string_type = TypeRegistry::instance().get_string_type();
    auto int_type = TypeRegistry::instance().get_int_type();
    
    std::vector<Property> properties;
    
    Value lazy_delegate;
    properties.push_back(
        PropertyBuilder("lazy_name", string_type)
            .immutable_property()
            .delegated_to("lazy_delegate", lazy_delegate)
            .build()
    );
    
    Value observable_delegate;
    properties.push_back(
        PropertyBuilder("observable_count", int_type)
            .mutable_property()
            .delegated_to("observable_delegate", observable_delegate)
            .build()
    );
    
    auto my_class = MetaType::create_class("MyClass", {}, {}, properties);
    auto* class_type = dynamic_cast<ClassMetaType*>(my_class.get());
    
    ASSERT_NE(class_type, nullptr);
    const auto& props = class_type->properties();
    EXPECT_EQ(props.size(), 2);
    EXPECT_TRUE(props[0].is_delegated);
    EXPECT_TRUE(props[1].is_delegated);
}
