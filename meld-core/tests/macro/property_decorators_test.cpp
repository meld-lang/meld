#include <gtest/gtest.h>
#include "meld/kernel/operations.hpp"
#include "meld/macro/property_decorators.hpp"
#include "meld/kernel/operations.hpp"
#include "meld/parser/ast.hpp"

using namespace meld::macro;
using namespace meld::parser::ast;

class PropertyDecoratorsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear and register decorators before each test
        DecoratorRegistry::instance().clear();
        register_property_decorators();
    }
    
    void TearDown() override {
        DecoratorRegistry::instance().clear();
    }
};

TEST_F(PropertyDecoratorsTest, GetterDecoratorRegistered) {
    EXPECT_TRUE(DecoratorRegistry::instance().has_decorator("Getter"));
}

TEST_F(PropertyDecoratorsTest, SetterDecoratorRegistered) {
    EXPECT_TRUE(DecoratorRegistry::instance().has_decorator("Setter"));
}

TEST_F(PropertyDecoratorsTest, GetterGeneratesForAllFields) {
    // Create a class with multiple fields
    class_definition class_def;
    class_def.name.name = "Person";
    
    field_declaration field1;
    field1.name.name = "name";
    field1.type.type_name.name = "String";
    field1.is_mutable = false;
    class_def.fields.push_back(field1);
    
    field_declaration field2;
    field2.name.name = "age";
    field2.type.type_name.name = "Int";
    field2.is_mutable = true;
    class_def.fields.push_back(field2);
    
    // Apply @Getter decorator
    MacroExpander expander;
    DecoratorContext context;
    auto result = context.apply_decorator(class_def, "Getter", expander);
    
    EXPECT_TRUE(result.has_value());
    
    // Result should be a list of generated getters
    if (result->is<meld::kernel::Cons>()) {
        auto list_result = meld::kernel::list_to_array(*result);
        EXPECT_TRUE(list_result.has_value());
        EXPECT_EQ(list_result->size(), 2); // Two getters for two fields
    }
}

TEST_F(PropertyDecoratorsTest, SetterGeneratesOnlyForMutableFields) {
    // Create a class with both mutable and immutable fields
    class_definition class_def;
    class_def.name.name = "Person";
    
    field_declaration field1;
    field1.name.name = "name";
    field1.type.type_name.name = "String";
    field1.is_mutable = true; // var - should get setter
    class_def.fields.push_back(field1);
    
    field_declaration field2;
    field2.name.name = "birthYear";
    field2.type.type_name.name = "Int";
    field2.is_mutable = false; // val - should NOT get setter
    class_def.fields.push_back(field2);
    
    field_declaration field3;
    field3.name.name = "email";
    field3.type.type_name.name = "String";
    field3.is_mutable = true; // var - should get setter
    class_def.fields.push_back(field3);
    
    // Apply @Setter decorator
    MacroExpander expander;
    DecoratorContext context;
    auto result = context.apply_decorator(class_def, "Setter", expander);
    
    EXPECT_TRUE(result.has_value());
    
    // Result should be a list with only 2 setters (for mutable fields)
    if (result->is<meld::kernel::Cons>()) {
        auto list_result = meld::kernel::list_to_array(*result);
        EXPECT_TRUE(list_result.has_value());
        EXPECT_EQ(list_result->size(), 2); // Two setters for two mutable fields
    }
}

TEST_F(PropertyDecoratorsTest, GetterWithNoFields) {
    // Create an empty class
    class_definition class_def;
    class_def.name.name = "EmptyClass";
    
    // Apply @Getter decorator
    MacroExpander expander;
    DecoratorContext context;
    auto result = context.apply_decorator(class_def, "Getter", expander);
    
    EXPECT_TRUE(result.has_value());
    
    // Should return nil for empty class
    if (result->is<meld::kernel::Symbol>()) {
        auto sym = result->as<meld::kernel::Symbol>();
        EXPECT_EQ(sym->name(), "nil");
    }
}

TEST_F(PropertyDecoratorsTest, SetterWithNoMutableFields) {
    // Create a class with only immutable fields
    class_definition class_def;
    class_def.name.name = "ImmutablePerson";
    
    field_declaration field1;
    field1.name.name = "name";
    field1.type.type_name.name = "String";
    field1.is_mutable = false; // val
    class_def.fields.push_back(field1);
    
    field_declaration field2;
    field2.name.name = "birthYear";
    field2.type.type_name.name = "Int";
    field2.is_mutable = false; // val
    class_def.fields.push_back(field2);
    
    // Apply @Setter decorator
    MacroExpander expander;
    DecoratorContext context;
    auto result = context.apply_decorator(class_def, "Setter", expander);
    
    EXPECT_TRUE(result.has_value());
    
    // Should return nil since no mutable fields
    if (result->is<meld::kernel::Symbol>()) {
        auto sym = result->as<meld::kernel::Symbol>();
        EXPECT_EQ(sym->name(), "nil");
    }
}

TEST_F(PropertyDecoratorsTest, GetterWithProperties) {
    // Create a class with properties
    class_definition class_def;
    class_def.name.name = "Person";
    
    property_declaration prop1;
    prop1.name.name = "fullName";
    prop1.type.type_name.name = "String";
    prop1.is_mutable = false;
    class_def.properties.push_back(prop1);
    
    // Apply @Getter decorator
    MacroExpander expander;
    DecoratorContext context;
    auto result = context.apply_decorator(class_def, "Getter", expander);
    
    EXPECT_TRUE(result.has_value());
    
    // Should generate getter for property
    if (result->is<meld::kernel::Cons>()) {
        auto list_result = meld::kernel::list_to_array(*result);
        EXPECT_TRUE(list_result.has_value());
        EXPECT_EQ(list_result->size(), 1);
    }
}

TEST_F(PropertyDecoratorsTest, BothDecoratorsApplied) {
    // Create a class
    class_definition class_def;
    class_def.name.name = "Person";
    
    field_declaration field1;
    field1.name.name = "name";
    field1.type.type_name.name = "String";
    field1.is_mutable = true;
    class_def.fields.push_back(field1);
    
    // Apply both decorators
    MacroExpander expander;
    DecoratorContext context;
    
    std::vector<std::string> decorators = {"Getter", "Setter"};
    auto result = context.apply_decorators(class_def, decorators, expander);
    
    EXPECT_TRUE(result.has_value());
}


// ===========================================================================
// @Property field-level macro tests (Task 7.1)
// Requirements: 17.1, 17.2, 17.3, 17.4, 17.5, 25B.11
// ===========================================================================

class PropertyFieldLevelTest : public ::testing::Test {
protected:
    void SetUp() override {
        DecoratorRegistry::instance().clear();
        meld::parser::ASTParentMap::instance().clear();
        register_property_decorators();
    }

    void TearDown() override {
        DecoratorRegistry::instance().clear();
        meld::parser::ASTParentMap::instance().clear();
    }

    // Helper: create a class with a single field, parent pointers wired
    void setup_class_with_field(class_definition& cls, const std::string& cls_name,
                                const std::string& field_name, const std::string& field_type,
                                bool is_mutable) {
        cls.name.name = cls_name;
        field_declaration field;
        field.name.name = field_name;
        field.type.type_name.name = field_type;
        field.is_mutable = is_mutable;
        cls.fields.push_back(std::move(field));
        // Wire parent pointer: field → class
        cls.fields.back().set_parent(&cls);
    }
};

// --- Requirement 17.2: @Property renames field `name` to `_name` ---

TEST_F(PropertyFieldLevelTest, PropertyRenamesFieldToUnderscorePrefix) {
    class_definition cls;
    setup_class_with_field(cls, "User", "name", "string", true);

    MacroExpander expander;
    DecoratorContext context;
    auto result = context.apply_decorator(cls.fields[0], "Property", expander);

    EXPECT_TRUE(result.has_value());
    // Field should now be renamed to _name
    EXPECT_EQ(cls.fields[0].name.name, "_name");
}

// --- Requirement 17.3: @Property changes visibility to package-private ---

TEST_F(PropertyFieldLevelTest, PropertyChangesVisibilityToPackagePrivate) {
    class_definition cls;
    setup_class_with_field(cls, "User", "email", "string", true);

    // Verify default visibility before
    EXPECT_EQ(cls.fields[0].visibility, FieldVisibility::DEFAULT);

    MacroExpander expander;
    DecoratorContext context;
    auto result = context.apply_decorator(cls.fields[0], "Property", expander);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(cls.fields[0].visibility, FieldVisibility::PACKAGE_PRIVATE);
}

// --- Requirement 17.4: @Property generates public getter `fnc name() -> T` ---

TEST_F(PropertyFieldLevelTest, PropertyGeneratesGetterMethod) {
    class_definition cls;
    setup_class_with_field(cls, "User", "name", "string", true);

    MacroExpander expander;
    DecoratorContext context;
    auto result = context.apply_decorator(cls.fields[0], "Property", expander);

    EXPECT_TRUE(result.has_value());
    // Getter should be injected into parent class with original name
    EXPECT_TRUE(cls.has_method("name"));
    auto* getter = cls.find_method("name");
    ASSERT_NE(getter, nullptr);
    EXPECT_EQ(getter->name.name, "name");
    EXPECT_TRUE(getter->has_return_type);
    EXPECT_EQ(getter->return_type.type_name.name, "string");
}

// --- Requirement 17.5: @Property generates setter `fnc set_name(v: T)` for mutable ---

TEST_F(PropertyFieldLevelTest, PropertyGeneratesSetterForMutableField) {
    class_definition cls;
    setup_class_with_field(cls, "User", "name", "string", true);

    MacroExpander expander;
    DecoratorContext context;
    auto result = context.apply_decorator(cls.fields[0], "Property", expander);

    EXPECT_TRUE(result.has_value());
    // Setter should be injected with set_ prefix
    EXPECT_TRUE(cls.has_method("set_name"));
    auto* setter = cls.find_method("set_name");
    ASSERT_NE(setter, nullptr);
    EXPECT_EQ(setter->name.name, "set_name");
    ASSERT_EQ(setter->parameters.size(), 1);
    EXPECT_EQ(setter->parameters[0].name.name, "v");
    EXPECT_EQ(setter->parameters[0].type.type_name.name, "string");
}

// --- Requirement 17.5: No setter for immutable (val) fields ---

TEST_F(PropertyFieldLevelTest, PropertyDoesNotGenerateSetterForImmutableField) {
    class_definition cls;
    setup_class_with_field(cls, "Config", "host", "string", false);

    MacroExpander expander;
    DecoratorContext context;
    auto result = context.apply_decorator(cls.fields[0], "Property", expander);

    EXPECT_TRUE(result.has_value());
    // Getter should exist
    EXPECT_TRUE(cls.has_method("host"));
    // Setter should NOT exist for val field
    EXPECT_FALSE(cls.has_method("set_host"));
}

// --- Return value contains correct symbol names ---

TEST_F(PropertyFieldLevelTest, PropertyReturnValueContainsCorrectSymbols) {
    class_definition cls;
    setup_class_with_field(cls, "User", "age", "int", true);

    MacroExpander expander;
    DecoratorContext context;
    auto result = context.apply_decorator(cls.fields[0], "Property", expander);

    ASSERT_TRUE(result.has_value());
    // Should return a list with getter and setter symbols
    auto list_result = meld::kernel::list_to_array(*result);
    ASSERT_TRUE(list_result.has_value());
    EXPECT_EQ(list_result->size(), 2); // getter + setter for mutable field

    // First symbol: getter name (original field name)
    EXPECT_TRUE((*list_result)[0].is<meld::kernel::Symbol>());
    EXPECT_EQ((*list_result)[0].as<meld::kernel::Symbol>()->name(), "age");

    // Second symbol: setter name (set_ + original field name)
    EXPECT_TRUE((*list_result)[1].is<meld::kernel::Symbol>());
    EXPECT_EQ((*list_result)[1].as<meld::kernel::Symbol>()->name(), "set_age");
}

TEST_F(PropertyFieldLevelTest, PropertyReturnValueForImmutableHasOnlyGetter) {
    class_definition cls;
    setup_class_with_field(cls, "Config", "port", "int", false);

    MacroExpander expander;
    DecoratorContext context;
    auto result = context.apply_decorator(cls.fields[0], "Property", expander);

    ASSERT_TRUE(result.has_value());
    auto list_result = meld::kernel::list_to_array(*result);
    ASSERT_TRUE(list_result.has_value());
    EXPECT_EQ(list_result->size(), 1); // getter only

    EXPECT_TRUE((*list_result)[0].is<meld::kernel::Symbol>());
    EXPECT_EQ((*list_result)[0].as<meld::kernel::Symbol>()->name(), "port");
}

// --- Multiple fields: each @Property application is independent ---

TEST_F(PropertyFieldLevelTest, PropertyWorksOnMultipleFieldsIndependently) {
    class_definition cls;
    cls.name.name = "User";

    // Add two fields
    field_declaration f1;
    f1.name.name = "name";
    f1.type.type_name.name = "string";
    f1.is_mutable = true;
    cls.fields.push_back(std::move(f1));
    cls.fields.back().set_parent(&cls);

    field_declaration f2;
    f2.name.name = "age";
    f2.type.type_name.name = "int";
    f2.is_mutable = true;
    cls.fields.push_back(std::move(f2));
    cls.fields.back().set_parent(&cls);

    MacroExpander expander;
    DecoratorContext context;

    // Apply @Property to first field
    auto r1 = context.apply_decorator(cls.fields[0], "Property", expander);
    EXPECT_TRUE(r1.has_value());

    // Apply @Property to second field
    auto r2 = context.apply_decorator(cls.fields[1], "Property", expander);
    EXPECT_TRUE(r2.has_value());

    // Both fields renamed
    EXPECT_EQ(cls.fields[0].name.name, "_name");
    EXPECT_EQ(cls.fields[1].name.name, "_age");

    // Both fields package-private
    EXPECT_EQ(cls.fields[0].visibility, FieldVisibility::PACKAGE_PRIVATE);
    EXPECT_EQ(cls.fields[1].visibility, FieldVisibility::PACKAGE_PRIVATE);

    // All four methods injected
    EXPECT_TRUE(cls.has_method("name"));
    EXPECT_TRUE(cls.has_method("set_name"));
    EXPECT_TRUE(cls.has_method("age"));
    EXPECT_TRUE(cls.has_method("set_age"));
}
