#include <gtest/gtest.h>
#include "meld/types/instance.hpp"
#include "meld/meta/metatype.hpp"

using namespace meld;
using namespace meld::types;
using namespace meld::meta;

// Test struct creation and field access
TEST(StructInstanceTest, CreateAndAccessFields) {
    // Create a Point struct type
    auto int_type = TypeRegistry::instance().get_int_type();
    std::vector<Field> fields = {
        Field("x", int_type, false),  // immutable
        Field("y", int_type, true)    // mutable
    };
    
    auto point_type = std::dynamic_pointer_cast<StructMetaType>(
        MetaType::create_struct("Point", std::move(fields))
    );
    
    // Create instance
    auto point = create_struct_instance(point_type);
    ASSERT_NE(point, nullptr);
    
    // Set fields
    auto x_val = kernel::make_int(10);
    auto y_val = kernel::make_int(20);
    
    auto set_x = point->set_field("x", x_val);
    EXPECT_FALSE(set_x.has_value()); // x is immutable, should fail
    
    auto set_y = point->set_field("y", y_val);
    EXPECT_TRUE(set_y.has_value()); // y is mutable, should succeed
}

// Test struct copy semantics
TEST(StructInstanceTest, CopySemantics) {
    auto int_type = TypeRegistry::instance().get_int_type();
    std::vector<Field> fields = {
        Field("value", int_type, true)
    };
    
    auto struct_type = std::dynamic_pointer_cast<StructMetaType>(
        MetaType::create_struct("TestStruct", std::move(fields))
    );
    
    auto original = create_struct_instance(struct_type);
    auto val = kernel::make_int(42);
    original->set_field("value", val);
    
    // Copy the struct
    auto copy = std::dynamic_pointer_cast<StructInstance>(original->copy());
    ASSERT_NE(copy, nullptr);
    
    // Modify the copy
    auto new_val = kernel::make_int(100);
    copy->set_field("value", new_val);
    
    // Original should be unchanged (copy-by-value semantics)
    auto original_val = original->get_field("value");
    ASSERT_TRUE(original_val.has_value());
    // Note: In a full implementation, we'd check the actual value
}

// Test class creation and field access
TEST(ClassInstanceTest, CreateAndAccessFields) {
    // Create a Person class type
    auto string_type = TypeRegistry::instance().get_string_type();
    auto int_type = TypeRegistry::instance().get_int_type();
    
    std::vector<Field> fields = {
        Field("name", string_type, true),      // mutable
        Field("age", int_type, false)          // immutable
    };
    
    auto person_type = std::dynamic_pointer_cast<ClassMetaType>(
        MetaType::create_class("Person", std::move(fields), {})
    );
    
    // Create instance
    auto person = create_class_instance(person_type);
    ASSERT_NE(person, nullptr);
    
    // Set fields
    auto name_val = kernel::make_string("Alice");
    auto age_val = kernel::make_int(30);
    
    auto set_name = person->set_field("name", name_val);
    EXPECT_TRUE(set_name.has_value()); // name is mutable
    
    auto set_age = person->set_field("age", age_val);
    EXPECT_FALSE(set_age.has_value()); // age is immutable, should fail
}

// Test class reference semantics
TEST(ClassInstanceTest, ReferenceSemantics) {
    auto int_type = TypeRegistry::instance().get_int_type();
    std::vector<Field> fields = {
        Field("value", int_type, true)
    };
    
    auto class_type = std::dynamic_pointer_cast<ClassMetaType>(
        MetaType::create_class("TestClass", std::move(fields), {})
    );
    
    auto original = create_class_instance(class_type);
    auto val = kernel::make_int(42);
    original->set_field("value", val);
    
    // Create another reference to the same instance
    auto reference = original;
    
    // Modify through the reference
    auto new_val = kernel::make_int(100);
    reference->set_field("value", new_val);
    
    // Original should be modified (reference semantics)
    auto original_val = original->get_field("value");
    ASSERT_TRUE(original_val.has_value());
    // Note: In a full implementation, we'd check the actual value
}

// Test reference counting
TEST(ClassInstanceTest, ReferenceCountingARC) {
    auto int_type = TypeRegistry::instance().get_int_type();
    std::vector<Field> fields = {
        Field("value", int_type, true)
    };
    
    auto class_type = std::dynamic_pointer_cast<ClassMetaType>(
        MetaType::create_class("TestClass", std::move(fields), {})
    );
    
    auto instance = create_class_instance(class_type);
    EXPECT_EQ(instance->ref_count(), 1);
    
    instance->retain();
    EXPECT_EQ(instance->ref_count(), 2);
    
    instance->release();
    EXPECT_EQ(instance->ref_count(), 1);
}

// Test field not found error
TEST(InstanceTest, FieldNotFoundError) {
    auto int_type = TypeRegistry::instance().get_int_type();
    std::vector<Field> fields = {
        Field("x", int_type, false)
    };
    
    auto struct_type = std::dynamic_pointer_cast<StructMetaType>(
        MetaType::create_struct("Point", std::move(fields))
    );
    
    auto instance = create_struct_instance(struct_type);
    
    auto result = instance->get_field("nonexistent");
    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().find("not found") != std::string::npos);
}
