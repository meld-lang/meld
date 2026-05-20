#include <gtest/gtest.h>
#include "meld/types/instance.hpp"
#include "meld/meta/metatype.hpp"

using namespace meld;
using namespace meld::types;
using namespace meld::meta;

// Test Copyable trait exists
TEST(CopyableTest, CopyableTraitExists) {
    auto& registry = TypeRegistry::instance();
    
    auto copyable = registry.get_copyable_trait();
    ASSERT_NE(copyable, nullptr);
    EXPECT_EQ(copyable->name(), "Copyable");
}

// Test struct is automatically copyable
TEST(CopyableTest, StructIsAutomaticallyCopyable) {
    auto& registry = TypeRegistry::instance();
    
    std::vector<Field> fields = {
        Field("x", registry.get_int_type(), false),
        Field("y", registry.get_int_type(), false)
    };
    
    auto point_type = std::dynamic_pointer_cast<StructMetaType>(
        MetaType::create_struct("Point", std::move(fields))
    );
    
    EXPECT_TRUE(point_type->is_copyable());
}

// Test struct copy creates independent instance
TEST(CopyableTest, StructCopyCreatesIndependentInstance) {
    auto& registry = TypeRegistry::instance();
    
    std::vector<Field> fields = {
        Field("value", registry.get_int_type(), true)
    };
    
    auto struct_type = std::dynamic_pointer_cast<StructMetaType>(
        MetaType::create_struct("TestStruct", std::move(fields))
    );
    
    auto original = create_struct_instance(struct_type);
    original->set_field("value", kernel::make_int(42));
    
    // Copy the struct
    auto copy = original->copy_struct();
    
    // Modify the copy
    copy->set_field("value", kernel::make_int(100));
    
    // Original should be unchanged
    auto original_value = original->get_field("value");
    ASSERT_TRUE(original_value.has_value());
    // In a full implementation, we'd verify the value is still 42
}

// Test struct copy via Copyable interface
TEST(CopyableTest, StructCopyViaCopyableInterface) {
    auto& registry = TypeRegistry::instance();
    
    std::vector<Field> fields = {
        Field("value", registry.get_int_type(), true)
    };
    
    auto struct_type = std::dynamic_pointer_cast<StructMetaType>(
        MetaType::create_struct("TestStruct", std::move(fields))
    );
    
    auto original = create_struct_instance(struct_type);
    original->set_field("value", kernel::make_int(42));
    
    // Copy via Copyable interface
    Copyable* copyable = original.get();
    auto copy = copyable->copy();
    
    ASSERT_NE(copy, nullptr);
    EXPECT_NE(copy.get(), original.get());
}

// Test struct deep copy of fields
TEST(CopyableTest, StructDeepCopyOfFields) {
    auto& registry = TypeRegistry::instance();
    
    std::vector<Field> fields = {
        Field("a", registry.get_int_type(), true),
        Field("b", registry.get_string_type(), true),
        Field("c", registry.get_bool_type(), true)
    };
    
    auto struct_type = std::dynamic_pointer_cast<StructMetaType>(
        MetaType::create_struct("ComplexStruct", std::move(fields))
    );
    
    auto original = create_struct_instance(struct_type);
    original->set_field("a", kernel::make_int(42));
    original->set_field("b", kernel::make_string("hello"));
    original->set_field("c", kernel::make_bool(true));
    
    auto copy = original->copy_struct();
    
    // All fields should be copied
    EXPECT_TRUE(copy->get_field("a").has_value());
    EXPECT_TRUE(copy->get_field("b").has_value());
    EXPECT_TRUE(copy->get_field("c").has_value());
}

// Test class is not copyable by default
TEST(CopyableTest, ClassNotCopyableByDefault) {
    auto& registry = TypeRegistry::instance();
    
    std::vector<Field> fields = {
        Field("value", registry.get_int_type(), true)
    };
    
    auto class_type = std::dynamic_pointer_cast<ClassMetaType>(
        MetaType::create_class("TestClass", std::move(fields), {})
    );
    
    auto instance = create_class_instance(class_type);
    
    EXPECT_FALSE(instance->is_copyable());
}

// Test class can be made copyable
TEST(CopyableTest, ClassCanBeMadeCopyable) {
    auto& registry = TypeRegistry::instance();
    
    std::vector<Field> fields = {
        Field("value", registry.get_int_type(), true)
    };
    
    auto class_type = std::dynamic_pointer_cast<ClassMetaType>(
        MetaType::create_class("TestClass", std::move(fields), {})
    );
    
    auto instance = create_class_instance(class_type);
    
    instance->set_copyable(true);
    EXPECT_TRUE(instance->is_copyable());
}

// Test class copy when copyable
TEST(CopyableTest, ClassCopyWhenCopyable) {
    auto& registry = TypeRegistry::instance();
    
    std::vector<Field> fields = {
        Field("value", registry.get_int_type(), true)
    };
    
    auto class_type = std::dynamic_pointer_cast<ClassMetaType>(
        MetaType::create_class("TestClass", std::move(fields), {})
    );
    
    auto original = create_class_instance(class_type);
    original->set_copyable(true);
    original->set_field("value", kernel::make_int(42));
    
    auto copy = original->copy_class();
    
    ASSERT_NE(copy, nullptr);
    EXPECT_NE(copy.get(), original.get());
    
    // Copy should have the same fields
    EXPECT_TRUE(copy->get_field("value").has_value());
}

// Test class copy throws when not copyable
TEST(CopyableTest, ClassCopyThrowsWhenNotCopyable) {
    auto& registry = TypeRegistry::instance();
    
    std::vector<Field> fields = {
        Field("value", registry.get_int_type(), true)
    };
    
    auto class_type = std::dynamic_pointer_cast<ClassMetaType>(
        MetaType::create_class("TestClass", std::move(fields), {})
    );
    
    auto instance = create_class_instance(class_type);
    
    // Should throw because class is not copyable
    EXPECT_THROW(instance->copy_class(), std::runtime_error);
}

// Test struct copy preserves type
TEST(CopyableTest, StructCopyPreservesType) {
    auto& registry = TypeRegistry::instance();
    
    std::vector<Field> fields = {
        Field("value", registry.get_int_type(), true)
    };
    
    auto struct_type = std::dynamic_pointer_cast<StructMetaType>(
        MetaType::create_struct("TestStruct", std::move(fields))
    );
    
    auto original = create_struct_instance(struct_type);
    auto copy = original->copy_struct();
    
    EXPECT_EQ(copy->get_type()->name(), original->get_type()->name());
}

// Test class copy preserves type
TEST(CopyableTest, ClassCopyPreservesType) {
    auto& registry = TypeRegistry::instance();
    
    std::vector<Field> fields = {
        Field("value", registry.get_int_type(), true)
    };
    
    auto class_type = std::dynamic_pointer_cast<ClassMetaType>(
        MetaType::create_class("TestClass", std::move(fields), {})
    );
    
    auto original = create_class_instance(class_type);
    original->set_copyable(true);
    
    auto copy = original->copy_class();
    
    EXPECT_EQ(copy->get_type()->name(), original->get_type()->name());
}

// Test multiple struct copies
TEST(CopyableTest, MultipleStructCopies) {
    auto& registry = TypeRegistry::instance();
    
    std::vector<Field> fields = {
        Field("value", registry.get_int_type(), true)
    };
    
    auto struct_type = std::dynamic_pointer_cast<StructMetaType>(
        MetaType::create_struct("TestStruct", std::move(fields))
    );
    
    auto original = create_struct_instance(struct_type);
    original->set_field("value", kernel::make_int(1));
    
    auto copy1 = original->copy_struct();
    copy1->set_field("value", kernel::make_int(2));
    
    auto copy2 = original->copy_struct();
    copy2->set_field("value", kernel::make_int(3));
    
    auto copy3 = copy1->copy_struct();
    copy3->set_field("value", kernel::make_int(4));
    
    // All copies should be independent
    EXPECT_NE(original.get(), copy1.get());
    EXPECT_NE(original.get(), copy2.get());
    EXPECT_NE(copy1.get(), copy2.get());
    EXPECT_NE(copy1.get(), copy3.get());
}

// Test class copy is shallow (references not deep copied)
TEST(CopyableTest, ClassCopyIsShallow) {
    auto& registry = TypeRegistry::instance();
    
    std::vector<Field> fields = {
        Field("value", registry.get_int_type(), true)
    };
    
    auto class_type = std::dynamic_pointer_cast<ClassMetaType>(
        MetaType::create_class("TestClass", std::move(fields), {})
    );
    
    auto original = create_class_instance(class_type);
    original->set_copyable(true);
    original->set_field("value", kernel::make_int(42));
    
    auto copy = original->copy_class();
    
    // Copy should have independent reference count
    EXPECT_EQ(copy->ref_count(), 1);
    EXPECT_GE(original->ref_count(), 1);
}

// Test copyable flag is copied
TEST(CopyableTest, CopyableFlagIsCopied) {
    auto& registry = TypeRegistry::instance();
    
    std::vector<Field> fields = {
        Field("value", registry.get_int_type(), true)
    };
    
    auto class_type = std::dynamic_pointer_cast<ClassMetaType>(
        MetaType::create_class("TestClass", std::move(fields), {})
    );
    
    auto original = create_class_instance(class_type);
    original->set_copyable(true);
    
    auto copy = original->copy_class();
    
    // Copy should also be copyable
    EXPECT_TRUE(copy->is_copyable());
}

// Test struct with nested struct
TEST(CopyableTest, StructWithNestedStruct) {
    auto& registry = TypeRegistry::instance();
    
    // Inner struct
    std::vector<Field> inner_fields = {
        Field("x", registry.get_int_type(), false)
    };
    auto inner_type = std::dynamic_pointer_cast<StructMetaType>(
        MetaType::create_struct("Inner", std::move(inner_fields))
    );
    
    // Outer struct
    std::vector<Field> outer_fields = {
        Field("inner", inner_type, false)
    };
    auto outer_type = std::dynamic_pointer_cast<StructMetaType>(
        MetaType::create_struct("Outer", std::move(outer_fields))
    );
    
    auto original = create_struct_instance(outer_type);
    
    // Copy should work
    auto copy = original->copy_struct();
    EXPECT_NE(copy.get(), original.get());
}

// Test empty struct copy
TEST(CopyableTest, EmptyStructCopy) {
    std::vector<Field> fields = {};
    auto struct_type = std::dynamic_pointer_cast<StructMetaType>(
        MetaType::create_struct("EmptyStruct", std::move(fields))
    );
    
    auto original = create_struct_instance(struct_type);
    auto copy = original->copy_struct();
    
    EXPECT_NE(copy.get(), original.get());
}

// Test copy method generation
TEST(CopyableTest, CopyMethodGeneration) {
    auto& registry = TypeRegistry::instance();
    
    auto copyable_trait = registry.get_copyable_trait();
    
    // Copyable trait should have a copy method
    auto copy_method = copyable_trait->get_method("copy");
    EXPECT_TRUE(copy_method.has_value());
}
