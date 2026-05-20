#include <gtest/gtest.h>
#include "meld/types/property.hpp"
#include "meld/meta/metatype.hpp"

using namespace meld::types;
using namespace meld::meta;
using namespace meld::kernel;

// Test public access (always allowed)
TEST(PropertyAccessTest, PublicAccessAlwaysAllowed) {
    auto int_type = TypeRegistry::instance().get_int_type();
    
    auto prop = PropertyBuilder("value", int_type)
        .mutable_property()
        .with_backing_field("_value")
        .getter_access(AccessModifier::Public)
        .setter_access(AccessModifier::Public)
        .build();
    
    // Test from outside the class
    AccessContext outside_context{
        AccessModifier::Public,
        false,  // not same class
        false,  // not subclass
        false   // not same module
    };
    
    PropertyContext context{nullptr, &prop, outside_context};
    
    EXPECT_TRUE(PropertyAccessor::can_access_getter(context));
    EXPECT_TRUE(PropertyAccessor::can_access_setter(context));
}

// Test private access (only same class)
TEST(PropertyAccessTest, PrivateAccessOnlySameClass) {
    auto int_type = TypeRegistry::instance().get_int_type();
    
    auto prop = PropertyBuilder("value", int_type)
        .mutable_property()
        .with_backing_field("_value")
        .getter_access(AccessModifier::Private)
        .setter_access(AccessModifier::Private)
        .build();
    
    // Test from same class
    AccessContext same_class_context{
        AccessModifier::Private,
        true,   // same class
        false,  // not subclass
        true    // same module
    };
    
    PropertyContext same_class{nullptr, &prop, same_class_context};
    EXPECT_TRUE(PropertyAccessor::can_access_getter(same_class));
    EXPECT_TRUE(PropertyAccessor::can_access_setter(same_class));
    
    // Test from outside class
    AccessContext outside_context{
        AccessModifier::Public,
        false,  // not same class
        false,  // not subclass
        true    // same module
    };
    
    PropertyContext outside{nullptr, &prop, outside_context};
    EXPECT_FALSE(PropertyAccessor::can_access_getter(outside));
    EXPECT_FALSE(PropertyAccessor::can_access_setter(outside));
}

// Test protected access (same class or subclass)
TEST(PropertyAccessTest, ProtectedAccessSameClassOrSubclass) {
    auto int_type = TypeRegistry::instance().get_int_type();
    
    auto prop = PropertyBuilder("value", int_type)
        .mutable_property()
        .with_backing_field("_value")
        .getter_access(AccessModifier::Protected)
        .setter_access(AccessModifier::Protected)
        .build();
    
    // Test from same class
    AccessContext same_class_context{
        AccessModifier::Protected,
        true,   // same class
        false,  // not subclass
        true    // same module
    };
    
    PropertyContext same_class{nullptr, &prop, same_class_context};
    EXPECT_TRUE(PropertyAccessor::can_access_getter(same_class));
    EXPECT_TRUE(PropertyAccessor::can_access_setter(same_class));
    
    // Test from subclass
    AccessContext subclass_context{
        AccessModifier::Protected,
        false,  // not same class
        true,   // is subclass
        true    // same module
    };
    
    PropertyContext subclass{nullptr, &prop, subclass_context};
    EXPECT_TRUE(PropertyAccessor::can_access_getter(subclass));
    EXPECT_TRUE(PropertyAccessor::can_access_setter(subclass));
    
    // Test from outside
    AccessContext outside_context{
        AccessModifier::Public,
        false,  // not same class
        false,  // not subclass
        true    // same module
    };
    
    PropertyContext outside{nullptr, &prop, outside_context};
    EXPECT_FALSE(PropertyAccessor::can_access_getter(outside));
    EXPECT_FALSE(PropertyAccessor::can_access_setter(outside));
}

// Test internal access (same module)
TEST(PropertyAccessTest, InternalAccessSameModule) {
    auto int_type = TypeRegistry::instance().get_int_type();
    
    auto prop = PropertyBuilder("value", int_type)
        .mutable_property()
        .with_backing_field("_value")
        .getter_access(AccessModifier::Internal)
        .setter_access(AccessModifier::Internal)
        .build();
    
    // Test from same module
    AccessContext same_module_context{
        AccessModifier::Internal,
        false,  // not same class
        false,  // not subclass
        true    // same module
    };
    
    PropertyContext same_module{nullptr, &prop, same_module_context};
    EXPECT_TRUE(PropertyAccessor::can_access_getter(same_module));
    EXPECT_TRUE(PropertyAccessor::can_access_setter(same_module));
    
    // Test from different module
    AccessContext different_module_context{
        AccessModifier::Public,
        false,  // not same class
        false,  // not subclass
        false   // different module
    };
    
    PropertyContext different_module{nullptr, &prop, different_module_context};
    EXPECT_FALSE(PropertyAccessor::can_access_getter(different_module));
    EXPECT_FALSE(PropertyAccessor::can_access_setter(different_module));
}

// Test mixed access modifiers (public getter, private setter)
TEST(PropertyAccessTest, MixedAccessModifiers) {
    auto int_type = TypeRegistry::instance().get_int_type();
    
    auto prop = PropertyBuilder("balance", int_type)
        .mutable_property()
        .with_backing_field("_balance")
        .getter_access(AccessModifier::Public)
        .setter_access(AccessModifier::Private)
        .build();
    
    // Test from outside class
    AccessContext outside_context{
        AccessModifier::Public,
        false,  // not same class
        false,  // not subclass
        false   // different module
    };
    
    PropertyContext outside{nullptr, &prop, outside_context};
    EXPECT_TRUE(PropertyAccessor::can_access_getter(outside));   // Public getter
    EXPECT_FALSE(PropertyAccessor::can_access_setter(outside));  // Private setter
    
    // Test from same class
    AccessContext same_class_context{
        AccessModifier::Private,
        true,   // same class
        false,  // not subclass
        true    // same module
    };
    
    PropertyContext same_class{nullptr, &prop, same_class_context};
    EXPECT_TRUE(PropertyAccessor::can_access_getter(same_class));
    EXPECT_TRUE(PropertyAccessor::can_access_setter(same_class));
}

// Test access control in get_value
TEST(PropertyAccessTest, GetValueAccessControl) {
    auto int_type = TypeRegistry::instance().get_int_type();
    
    auto prop = PropertyBuilder("secret", int_type)
        .immutable_property()
        .with_backing_field("_secret")
        .getter_access(AccessModifier::Private)
        .build();
    
    // Try to access from outside
    AccessContext outside_context{
        AccessModifier::Public,
        false,  // not same class
        false,  // not subclass
        false   // different module
    };
    
    PropertyContext context{nullptr, &prop, outside_context};
    auto result = PropertyAccessor::get_value(context);
    
    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().find("insufficient access") != std::string::npos);
}

// Test access control in set_value
TEST(PropertyAccessTest, SetValueAccessControl) {
    auto int_type = TypeRegistry::instance().get_int_type();
    
    auto prop = PropertyBuilder("protected_value", int_type)
        .mutable_property()
        .with_backing_field("_protected_value")
        .setter_access(AccessModifier::Protected)
        .build();
    
    // Try to set from outside
    AccessContext outside_context{
        AccessModifier::Public,
        false,  // not same class
        false,  // not subclass
        false   // different module
    };
    
    PropertyContext context{nullptr, &prop, outside_context};
    Value new_value;
    auto result = PropertyAccessor::set_value(context, new_value);
    
    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().find("insufficient access") != std::string::npos);
}

// Test typical use case: public getter, private setter
TEST(PropertyAccessTest, TypicalPublicGetterPrivateSetter) {
    auto int_type = TypeRegistry::instance().get_int_type();
    
    auto prop = PropertyBuilder("id", int_type)
        .mutable_property()
        .with_backing_field("_id")
        .getter_access(AccessModifier::Public)
        .setter_access(AccessModifier::Private)
        .build();
    
    EXPECT_EQ(prop.getter_access, AccessModifier::Public);
    EXPECT_EQ(prop.setter_access, AccessModifier::Private);
    
    // Anyone can read
    AccessContext public_context{
        AccessModifier::Public,
        false, false, false
    };
    PropertyContext read_context{nullptr, &prop, public_context};
    EXPECT_TRUE(PropertyAccessor::can_access_getter(read_context));
    EXPECT_FALSE(PropertyAccessor::can_access_setter(read_context));
    
    // Only same class can write
    AccessContext private_context{
        AccessModifier::Private,
        true, false, true
    };
    PropertyContext write_context{nullptr, &prop, private_context};
    EXPECT_TRUE(PropertyAccessor::can_access_getter(write_context));
    EXPECT_TRUE(PropertyAccessor::can_access_setter(write_context));
}
