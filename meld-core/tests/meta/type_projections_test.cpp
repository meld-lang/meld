#include <gtest/gtest.h>
#include "meld/meta/type_projections.hpp"

using namespace meld::meta;

// Test fixture for type projections
class TypeProjectionsTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto& registry = TypeRegistry::instance();
        
        // Create a Person struct for testing
        std::vector<Field> person_fields = {
            Field("name", registry.get_string_type(), false),
            Field("age", registry.get_int_type(), false),
            Field("email", registry.get_string_type(), false)
        };
        person_type = MetaType::create_struct("Person", std::move(person_fields));
        
        // Create a User class for testing
        std::vector<Field> user_fields = {
            Field("username", registry.get_string_type(), true),
            Field("password", registry.get_string_type(), true),
            Field("isActive", registry.get_bool_type(), true)
        };
        std::vector<Method> user_methods;
        user_type = MetaType::create_class("User", std::move(user_fields), std::move(user_methods));
        
        // Create a type with nullable fields
        std::vector<Field> optional_fields = {
            Field("required", registry.get_int_type(), false),
            Field("optional", registry.create_optional_type(registry.get_string_type()), false)
        };
        optional_type = MetaType::create_struct("OptionalData", std::move(optional_fields));
    }
    
    std::shared_ptr<MetaType> person_type;
    std::shared_ptr<MetaType> user_type;
    std::shared_ptr<MetaType> optional_type;
};

// Omit tests
TEST_F(TypeProjectionsTest, OmitRemovesSpecifiedFields) {
    std::set<std::string> keys_to_omit = {"email"};
    auto result = OmitProjection::apply(person_type, keys_to_omit);
    
    ASSERT_TRUE(result.has_value());
    auto omitted_type = *result;
    
    EXPECT_EQ(omitted_type->name(), "omit<Person, ...>");
    
    auto* struct_type = dynamic_cast<StructMetaType*>(omitted_type.get());
    ASSERT_NE(struct_type, nullptr);
    
    // Should have name and age, but not email
    EXPECT_TRUE(struct_type->get_field("name").has_value());
    EXPECT_TRUE(struct_type->get_field("age").has_value());
    EXPECT_FALSE(struct_type->get_field("email").has_value());
    
    EXPECT_EQ(struct_type->fields().size(), 2);
}

TEST_F(TypeProjectionsTest, OmitMultipleFields) {
    std::set<std::string> keys_to_omit = {"age", "email"};
    auto result = OmitProjection::apply(person_type, keys_to_omit);
    
    ASSERT_TRUE(result.has_value());
    auto omitted_type = *result;
    
    auto* struct_type = dynamic_cast<StructMetaType*>(omitted_type.get());
    ASSERT_NE(struct_type, nullptr);
    
    // Should only have name
    EXPECT_TRUE(struct_type->get_field("name").has_value());
    EXPECT_FALSE(struct_type->get_field("age").has_value());
    EXPECT_FALSE(struct_type->get_field("email").has_value());
    
    EXPECT_EQ(struct_type->fields().size(), 1);
}

TEST_F(TypeProjectionsTest, OmitWorksOnClasses) {
    std::set<std::string> keys_to_omit = {"password"};
    auto result = OmitProjection::apply(user_type, keys_to_omit);
    
    ASSERT_TRUE(result.has_value());
    auto omitted_type = *result;
    
    auto* class_type = dynamic_cast<ClassMetaType*>(omitted_type.get());
    ASSERT_NE(class_type, nullptr);
    
    EXPECT_TRUE(class_type->get_field("username").has_value());
    EXPECT_FALSE(class_type->get_field("password").has_value());
    EXPECT_TRUE(class_type->get_field("isActive").has_value());
}

TEST_F(TypeProjectionsTest, OmitEmptySet) {
    std::set<std::string> keys_to_omit;
    auto result = OmitProjection::apply(person_type, keys_to_omit);
    
    ASSERT_TRUE(result.has_value());
    auto omitted_type = *result;
    
    auto* struct_type = dynamic_cast<StructMetaType*>(omitted_type.get());
    ASSERT_NE(struct_type, nullptr);
    
    // Should have all fields
    EXPECT_EQ(struct_type->fields().size(), 3);
}

// Pick tests
TEST_F(TypeProjectionsTest, PickSelectsSpecifiedFields) {
    std::set<std::string> keys_to_pick = {"name", "age"};
    auto result = PickProjection::apply(person_type, keys_to_pick);
    
    ASSERT_TRUE(result.has_value());
    auto picked_type = *result;
    
    EXPECT_EQ(picked_type->name(), "pick<Person, ...>");
    
    auto* struct_type = dynamic_cast<StructMetaType*>(picked_type.get());
    ASSERT_NE(struct_type, nullptr);
    
    // Should have name and age, but not email
    EXPECT_TRUE(struct_type->get_field("name").has_value());
    EXPECT_TRUE(struct_type->get_field("age").has_value());
    EXPECT_FALSE(struct_type->get_field("email").has_value());
    
    EXPECT_EQ(struct_type->fields().size(), 2);
}

TEST_F(TypeProjectionsTest, PickSingleField) {
    std::set<std::string> keys_to_pick = {"email"};
    auto result = PickProjection::apply(person_type, keys_to_pick);
    
    ASSERT_TRUE(result.has_value());
    auto picked_type = *result;
    
    auto* struct_type = dynamic_cast<StructMetaType*>(picked_type.get());
    ASSERT_NE(struct_type, nullptr);
    
    // Should only have email
    EXPECT_FALSE(struct_type->get_field("name").has_value());
    EXPECT_FALSE(struct_type->get_field("age").has_value());
    EXPECT_TRUE(struct_type->get_field("email").has_value());
    
    EXPECT_EQ(struct_type->fields().size(), 1);
}

TEST_F(TypeProjectionsTest, PickNonExistentFieldFails) {
    std::set<std::string> keys_to_pick = {"name", "nonexistent"};
    auto result = PickProjection::apply(person_type, keys_to_pick);
    
    EXPECT_FALSE(result.has_value());
}

TEST_F(TypeProjectionsTest, PickWorksOnClasses) {
    std::set<std::string> keys_to_pick = {"username", "isActive"};
    auto result = PickProjection::apply(user_type, keys_to_pick);
    
    ASSERT_TRUE(result.has_value());
    auto picked_type = *result;
    
    auto* class_type = dynamic_cast<ClassMetaType*>(picked_type.get());
    ASSERT_NE(class_type, nullptr);
    
    EXPECT_TRUE(class_type->get_field("username").has_value());
    EXPECT_FALSE(class_type->get_field("password").has_value());
    EXPECT_TRUE(class_type->get_field("isActive").has_value());
}

// Partial tests
TEST_F(TypeProjectionsTest, PartialMakesAllFieldsNullable) {
    auto result = PartialProjection::apply(person_type);
    
    ASSERT_TRUE(result.has_value());
    auto partial_type = *result;
    
    EXPECT_EQ(partial_type->name(), "partial<Person>");
    
    auto* struct_type = dynamic_cast<StructMetaType*>(partial_type.get());
    ASSERT_NE(struct_type, nullptr);
    
    // All fields should be nullable
    auto& registry = TypeRegistry::instance();
    for (const auto& field : struct_type->fields()) {
        EXPECT_TRUE(registry.is_nullable_type(*field.type))
            << "Field " << field.name << " should be nullable";
    }
    
    EXPECT_EQ(struct_type->fields().size(), 3);
}

TEST_F(TypeProjectionsTest, PartialWorksOnClasses) {
    auto result = PartialProjection::apply(user_type);
    
    ASSERT_TRUE(result.has_value());
    auto partial_type = *result;
    
    auto* class_type = dynamic_cast<ClassMetaType*>(partial_type.get());
    ASSERT_NE(class_type, nullptr);
    
    // All fields should be nullable
    auto& registry = TypeRegistry::instance();
    for (const auto& field : class_type->fields()) {
        EXPECT_TRUE(registry.is_nullable_type(*field.type))
            << "Field " << field.name << " should be nullable";
    }
}

TEST_F(TypeProjectionsTest, PartialPreservesAlreadyNullableFields) {
    auto result = PartialProjection::apply(optional_type);
    
    ASSERT_TRUE(result.has_value());
    auto partial_type = *result;
    
    auto* struct_type = dynamic_cast<StructMetaType*>(partial_type.get());
    ASSERT_NE(struct_type, nullptr);
    
    // All fields should be nullable
    auto& registry = TypeRegistry::instance();
    for (const auto& field : struct_type->fields()) {
        EXPECT_TRUE(registry.is_nullable_type(*field.type))
            << "Field " << field.name << " should be nullable";
    }
}

// Required tests
TEST_F(TypeProjectionsTest, RequiredMakesAllFieldsNonNullable) {
    auto result = RequiredProjection::apply(optional_type);
    
    ASSERT_TRUE(result.has_value());
    auto required_type = *result;
    
    EXPECT_EQ(required_type->name(), "required<OptionalData>");
    
    auto* struct_type = dynamic_cast<StructMetaType*>(required_type.get());
    ASSERT_NE(struct_type, nullptr);
    
    // All fields should be non-nullable
    auto& registry = TypeRegistry::instance();
    for (const auto& field : struct_type->fields()) {
        EXPECT_FALSE(registry.is_nullable_type(*field.type))
            << "Field " << field.name << " should not be nullable";
    }
}

TEST_F(TypeProjectionsTest, RequiredWorksOnClasses) {
    // First make user_type partial, then make it required
    auto partial_result = PartialProjection::apply(user_type);
    ASSERT_TRUE(partial_result.has_value());
    
    auto required_result = RequiredProjection::apply(*partial_result);
    ASSERT_TRUE(required_result.has_value());
    
    auto required_type = *required_result;
    auto* class_type = dynamic_cast<ClassMetaType*>(required_type.get());
    ASSERT_NE(class_type, nullptr);
    
    // All fields should be non-nullable
    auto& registry = TypeRegistry::instance();
    for (const auto& field : class_type->fields()) {
        EXPECT_FALSE(registry.is_nullable_type(*field.type))
            << "Field " << field.name << " should not be nullable";
    }
}

TEST_F(TypeProjectionsTest, RequiredPreservesNonNullableFields) {
    auto result = RequiredProjection::apply(person_type);
    
    ASSERT_TRUE(result.has_value());
    auto required_type = *result;
    
    auto* struct_type = dynamic_cast<StructMetaType*>(required_type.get());
    ASSERT_NE(struct_type, nullptr);
    
    // All fields should remain non-nullable
    auto& registry = TypeRegistry::instance();
    for (const auto& field : struct_type->fields()) {
        EXPECT_FALSE(registry.is_nullable_type(*field.type))
            << "Field " << field.name << " should not be nullable";
    }
}

// Readonly tests
TEST_F(TypeProjectionsTest, ReadonlyMakesAllFieldsImmutable) {
    auto result = ReadonlyProjection::apply(user_type);
    
    ASSERT_TRUE(result.has_value());
    auto readonly_type = *result;
    
    EXPECT_EQ(readonly_type->name(), "readonly<User>");
    
    auto* class_type = dynamic_cast<ClassMetaType*>(readonly_type.get());
    ASSERT_NE(class_type, nullptr);
    
    // All fields should be immutable
    for (const auto& field : class_type->fields()) {
        EXPECT_FALSE(field.is_mutable)
            << "Field " << field.name << " should be immutable";
    }
}

TEST_F(TypeProjectionsTest, ReadonlyWorksOnStructs) {
    auto result = ReadonlyProjection::apply(person_type);
    
    ASSERT_TRUE(result.has_value());
    auto readonly_type = *result;
    
    auto* struct_type = dynamic_cast<StructMetaType*>(readonly_type.get());
    ASSERT_NE(struct_type, nullptr);
    
    // All fields should be immutable
    for (const auto& field : struct_type->fields()) {
        EXPECT_FALSE(field.is_mutable)
            << "Field " << field.name << " should be immutable";
    }
}

TEST_F(TypeProjectionsTest, ReadonlyPreservesImmutableFields) {
    auto result = ReadonlyProjection::apply(person_type);
    
    ASSERT_TRUE(result.has_value());
    auto readonly_type = *result;
    
    auto* struct_type = dynamic_cast<StructMetaType*>(readonly_type.get());
    ASSERT_NE(struct_type, nullptr);
    
    // All fields should remain immutable
    for (const auto& field : struct_type->fields()) {
        EXPECT_FALSE(field.is_mutable)
            << "Field " << field.name << " should be immutable";
    }
}

// Composition tests
TEST_F(TypeProjectionsTest, CompositionPartialThenPick) {
    // Make Person partial, then pick only name and age
    auto partial_result = PartialProjection::apply(person_type);
    ASSERT_TRUE(partial_result.has_value());
    
    std::set<std::string> keys_to_pick = {"name", "age"};
    auto pick_result = PickProjection::apply(*partial_result, keys_to_pick);
    ASSERT_TRUE(pick_result.has_value());
    
    auto final_type = *pick_result;
    auto* struct_type = dynamic_cast<StructMetaType*>(final_type.get());
    ASSERT_NE(struct_type, nullptr);
    
    // Should have 2 fields, both nullable
    EXPECT_EQ(struct_type->fields().size(), 2);
    
    auto& registry = TypeRegistry::instance();
    for (const auto& field : struct_type->fields()) {
        EXPECT_TRUE(registry.is_nullable_type(*field.type))
            << "Field " << field.name << " should be nullable";
    }
}

TEST_F(TypeProjectionsTest, CompositionOmitThenReadonly) {
    // Omit password from User, then make readonly
    std::set<std::string> keys_to_omit = {"password"};
    auto omit_result = OmitProjection::apply(user_type, keys_to_omit);
    ASSERT_TRUE(omit_result.has_value());
    
    auto readonly_result = ReadonlyProjection::apply(*omit_result);
    ASSERT_TRUE(readonly_result.has_value());
    
    auto final_type = *readonly_result;
    auto* class_type = dynamic_cast<ClassMetaType*>(final_type.get());
    ASSERT_NE(class_type, nullptr);
    
    // Should have 2 fields (username, isActive), both immutable
    EXPECT_EQ(class_type->fields().size(), 2);
    
    for (const auto& field : class_type->fields()) {
        EXPECT_FALSE(field.is_mutable)
            << "Field " << field.name << " should be immutable";
    }
    
    EXPECT_FALSE(class_type->get_field("password").has_value());
}

TEST_F(TypeProjectionsTest, CompositionPartialThenRequired) {
    // Make Person partial, then required (should be back to original)
    auto partial_result = PartialProjection::apply(person_type);
    ASSERT_TRUE(partial_result.has_value());
    
    auto required_result = RequiredProjection::apply(*partial_result);
    ASSERT_TRUE(required_result.has_value());
    
    auto final_type = *required_result;
    auto* struct_type = dynamic_cast<StructMetaType*>(final_type.get());
    ASSERT_NE(struct_type, nullptr);
    
    // All fields should be non-nullable again
    auto& registry = TypeRegistry::instance();
    for (const auto& field : struct_type->fields()) {
        EXPECT_FALSE(registry.is_nullable_type(*field.type))
            << "Field " << field.name << " should not be nullable";
    }
}

// Error handling tests
TEST_F(TypeProjectionsTest, OmitOnPrimitiveTypeFails) {
    auto& registry = TypeRegistry::instance();
    auto int_type = registry.get_int_type();
    
    std::set<std::string> keys = {"x"};
    auto result = OmitProjection::apply(int_type, keys);
    
    EXPECT_FALSE(result.has_value());
}

TEST_F(TypeProjectionsTest, PickOnPrimitiveTypeFails) {
    auto& registry = TypeRegistry::instance();
    auto string_type = registry.get_string_type();
    
    std::set<std::string> keys = {"length"};
    auto result = PickProjection::apply(string_type, keys);
    
    EXPECT_FALSE(result.has_value());
}

TEST_F(TypeProjectionsTest, PartialOnPrimitiveTypeFails) {
    auto& registry = TypeRegistry::instance();
    auto bool_type = registry.get_bool_type();
    
    auto result = PartialProjection::apply(bool_type);
    
    EXPECT_FALSE(result.has_value());
}

TEST_F(TypeProjectionsTest, RequiredOnPrimitiveTypeFails) {
    auto& registry = TypeRegistry::instance();
    auto int_type = registry.get_int_type();
    
    auto result = RequiredProjection::apply(int_type);
    
    EXPECT_FALSE(result.has_value());
}

TEST_F(TypeProjectionsTest, ReadonlyOnPrimitiveTypeFails) {
    auto& registry = TypeRegistry::instance();
    auto string_type = registry.get_string_type();
    
    auto result = ReadonlyProjection::apply(string_type);
    
    EXPECT_FALSE(result.has_value());
}
