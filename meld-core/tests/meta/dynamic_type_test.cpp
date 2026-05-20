#include <gtest/gtest.h>
#include "meld/meta/dynamic_type_builder.hpp"
#include "meld/meta/type_registration.hpp"
#include "meld/stdlib/reflect.hpp"
#include "meld/kernel/primitives.hpp"

using namespace meld::meta;
using namespace meld::stdlib::reflect;
using namespace meld::kernel;

class DynamicTypeTest : public ::testing::Test {
protected:
    void SetUp() override {
        initialize_meld_types();
    }
};

// Test creating a simple struct
TEST_F(DynamicTypeTest, CreateSimpleStruct) {
    auto result = Meta::createStruct("TestPoint")
        .addField("x", TypeRegistry::instance().get_int_type(), false)
        .addField("y", TypeRegistry::instance().get_int_type(), false)
        .build();
    
    ASSERT_TRUE(result.has_value());
    
    auto type = result.value();
    EXPECT_EQ(type->name(), "TestPoint");
    EXPECT_TRUE(type->is_value_type());
    
    // Verify it's a struct
    auto* struct_type = dynamic_cast<StructMetaType*>(type.get());
    ASSERT_NE(struct_type, nullptr);
    
    // Check fields
    EXPECT_EQ(struct_type->fields().size(), 2);
    
    auto x_field = struct_type->get_field("x");
    ASSERT_TRUE(x_field.has_value());
    EXPECT_EQ(x_field.value()->name, "x");
    EXPECT_FALSE(x_field.value()->is_mutable);
    
    auto y_field = struct_type->get_field("y");
    ASSERT_TRUE(y_field.has_value());
    EXPECT_EQ(y_field.value()->name, "y");
    EXPECT_FALSE(y_field.value()->is_mutable);
}

// Test creating a class with fields and methods
TEST_F(DynamicTypeTest, CreateClassWithFieldsAndMethods) {
    auto greet_impl = Value(std::make_shared<Function>(std::vector<std::shared_ptr<Symbol>>{}, make_string("Hello")));
    
    auto result = Meta::createClass("TestPerson")
        .addField("name", TypeRegistry::instance().get_string_type(), true)
        .addField("age", TypeRegistry::instance().get_int_type(), true)
        .addMethod(
            "greet",
            {},
            TypeRegistry::instance().get_string_type(),
            greet_impl
        )
        .build();
    
    ASSERT_TRUE(result.has_value());
    
    auto type = result.value();
    EXPECT_EQ(type->name(), "TestPerson");
    EXPECT_FALSE(type->is_value_type());
    
    // Verify it's a class
    auto* class_type = dynamic_cast<ClassMetaType*>(type.get());
    ASSERT_NE(class_type, nullptr);
    
    // Check fields
    EXPECT_EQ(class_type->fields().size(), 2);
    
    auto name_field = class_type->get_field("name");
    ASSERT_TRUE(name_field.has_value());
    EXPECT_EQ(name_field.value()->name, "name");
    EXPECT_TRUE(name_field.value()->is_mutable);
    
    // Check methods
    EXPECT_EQ(class_type->methods().size(), 1);
    
    auto greet_method = class_type->get_method("greet");
    ASSERT_TRUE(greet_method.has_value());
    EXPECT_EQ(greet_method.value()->name, "greet");
    EXPECT_EQ(greet_method.value()->param_types.size(), 0);
}

// Test creating a class with properties
TEST_F(DynamicTypeTest, CreateClassWithProperties) {
    auto result = Meta::createClass("TestUser")
        .addProperty("username", TypeRegistry::instance().get_string_type(), false)
        .addProperty("email", TypeRegistry::instance().get_string_type(), true)
        .build();
    
    ASSERT_TRUE(result.has_value());
    
    auto type = result.value();
    auto* class_type = dynamic_cast<ClassMetaType*>(type.get());
    ASSERT_NE(class_type, nullptr);
    
    // Check properties
    EXPECT_EQ(class_type->properties().size(), 2);
    
    auto username_prop = class_type->get_property("username");
    ASSERT_TRUE(username_prop.has_value());
    EXPECT_EQ(username_prop.value()->name, "username");
    EXPECT_FALSE(username_prop.value()->is_mutable);
    
    auto email_prop = class_type->get_property("email");
    ASSERT_TRUE(email_prop.has_value());
    EXPECT_EQ(email_prop.value()->name, "email");
    EXPECT_TRUE(email_prop.value()->is_mutable);
}

// Test creating a trait
TEST_F(DynamicTypeTest, CreateTrait) {
    auto draw_impl = Value(std::make_shared<Function>(std::vector<std::shared_ptr<Symbol>>{}, make_string("")));
    
    auto result = Meta::createTrait("TestDrawable")
        .addMethod(
            "draw",
            {},
            TypeRegistry::instance().get_string_type(),
            draw_impl
        )
        .build();
    
    ASSERT_TRUE(result.has_value());
    
    auto type = result.value();
    EXPECT_EQ(type->name(), "TestDrawable");
    
    // Verify it's a trait
    auto* trait_type = dynamic_cast<TraitMetaType*>(type.get());
    ASSERT_NE(trait_type, nullptr);
    
    // Check methods
    EXPECT_EQ(trait_type->methods().size(), 1);
    
    auto draw_method = trait_type->get_method("draw");
    ASSERT_TRUE(draw_method.has_value());
    EXPECT_EQ(draw_method.value()->name, "draw");
}

// Test that struct cannot have methods
TEST_F(DynamicTypeTest, StructCannotHaveMethods) {
    auto result = Meta::createStruct("InvalidStruct")
        .addField("x", TypeRegistry::instance().get_int_type())
        .addMethod(
            "foo",
            {},
            TypeRegistry::instance().get_int_type(),
            Value(std::make_shared<Function>(std::vector<std::shared_ptr<Symbol>>{}, make_int(0)))
        )
        .build();
    
    EXPECT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("cannot have methods"), std::string::npos);
}

// Test that trait cannot have fields
TEST_F(DynamicTypeTest, TraitCannotHaveFields) {
    auto result = Meta::createTrait("InvalidTrait")
        .addField("x", TypeRegistry::instance().get_int_type())
        .build();
    
    EXPECT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("cannot have fields"), std::string::npos);
}

// Test that trait cannot have properties
TEST_F(DynamicTypeTest, TraitCannotHaveProperties) {
    auto result = Meta::createTrait("InvalidTrait2")
        .addProperty("x", TypeRegistry::instance().get_int_type())
        .build();
    
    EXPECT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("cannot have properties"), std::string::npos);
}

// Test that duplicate type names are rejected
TEST_F(DynamicTypeTest, DuplicateTypeRejected) {
    // Create first type
    auto result1 = Meta::createClass("DuplicateTest")
        .build();
    
    ASSERT_TRUE(result1.has_value());
    
    // Try to create duplicate
    auto result2 = Meta::createClass("DuplicateTest")
        .build();
    
    EXPECT_FALSE(result2.has_value());
    EXPECT_NE(result2.error().find("already exists"), std::string::npos);
}

// Test that dynamic types are registered
TEST_F(DynamicTypeTest, DynamicTypesAreRegistered) {
    auto result = Meta::createClass("RegisteredClass")
        .addField("value", TypeRegistry::instance().get_int_type())
        .build();
    
    ASSERT_TRUE(result.has_value());
    
    // Verify registration
    EXPECT_TRUE(isTypeRegistered("RegisteredClass"));
    
    // Verify we can get the type back
    auto retrieved = TypeRegistry::instance().get_type("RegisteredClass");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved.value()->name(), "RegisteredClass");
}

// Test creating a class with base class
TEST_F(DynamicTypeTest, CreateClassWithBaseClass) {
    // Create base class
    auto base_result = Meta::createClass("BaseClass")
        .addField("base_field", TypeRegistry::instance().get_int_type())
        .build();
    
    ASSERT_TRUE(base_result.has_value());
    
    auto base_class = std::dynamic_pointer_cast<ClassMetaType>(base_result.value());
    ASSERT_NE(base_class, nullptr);
    
    // Create derived class
    auto derived_result = Meta::createClass("DerivedClass")
        .setBaseClass(base_class)
        .addField("derived_field", TypeRegistry::instance().get_string_type())
        .build();
    
    ASSERT_TRUE(derived_result.has_value());
    
    auto derived_type = derived_result.value();
    auto* derived_class = dynamic_cast<ClassMetaType*>(derived_type.get());
    ASSERT_NE(derived_class, nullptr);
    
    // Verify base class is set
    EXPECT_NE(derived_class->base_class(), nullptr);
    EXPECT_EQ(derived_class->base_class()->name(), "BaseClass");
    
    // Verify derived class has its own field
    EXPECT_EQ(derived_class->fields().size(), 1);
    auto derived_field = derived_class->get_field("derived_field");
    ASSERT_TRUE(derived_field.has_value());
    
    // Verify derived class can access base class field
    auto base_field = derived_class->get_field("base_field");
    ASSERT_TRUE(base_field.has_value());
}

// Test that only classes can have base classes
TEST_F(DynamicTypeTest, OnlyClassesCanHaveBaseClass) {
    auto base_result = Meta::createClass("BaseForStruct")
        .build();
    
    ASSERT_TRUE(base_result.has_value());
    auto base_class = std::dynamic_pointer_cast<ClassMetaType>(base_result.value());
    
    // Try to set base class on struct (should throw)
    EXPECT_THROW({
        Meta::createStruct("StructWithBase")
            .setBaseClass(base_class);
    }, std::runtime_error);
}

// Test property with custom getter
TEST_F(DynamicTypeTest, PropertyWithCustomGetter) {
    auto getter = Value(std::make_shared<Function>(std::vector<std::shared_ptr<Symbol>>{}, make_string("computed")));
    
    auto result = Meta::createClass("ClassWithComputedProperty")
        .addPropertyWithGetter(
            "computed",
            TypeRegistry::instance().get_string_type(),
            getter
        )
        .build();
    
    ASSERT_TRUE(result.has_value());
    
    auto* class_type = dynamic_cast<ClassMetaType*>(result.value().get());
    ASSERT_NE(class_type, nullptr);
    
    auto prop = class_type->get_property("computed");
    ASSERT_TRUE(prop.has_value());
    EXPECT_TRUE(prop.value()->has_custom_getter);
    EXPECT_FALSE(prop.value()->has_backing_field);
}

// Test property with custom getter and setter
TEST_F(DynamicTypeTest, PropertyWithCustomAccessors) {
    auto getter = Value(std::make_shared<Function>(std::vector<std::shared_ptr<Symbol>>{}, make_string("value")));
    auto setter = Value(std::make_shared<Function>(std::vector<std::shared_ptr<Symbol>>{}, make_int(0)));
    
    auto result = Meta::createClass("ClassWithAccessors")
        .addPropertyWithAccessors(
            "value",
            TypeRegistry::instance().get_string_type(),
            getter,
            setter
        )
        .build();
    
    ASSERT_TRUE(result.has_value());
    
    auto* class_type = dynamic_cast<ClassMetaType*>(result.value().get());
    ASSERT_NE(class_type, nullptr);
    
    auto prop = class_type->get_property("value");
    ASSERT_TRUE(prop.has_value());
    EXPECT_TRUE(prop.value()->has_custom_getter);
    EXPECT_TRUE(prop.value()->has_custom_setter);
    EXPECT_FALSE(prop.value()->has_backing_field);
}
