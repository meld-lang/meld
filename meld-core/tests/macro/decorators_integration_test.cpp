#include <gtest/gtest.h>
#include "meld/macro/decorators.hpp"
#include "meld/parser/ast.hpp"

using namespace meld::macro;
using namespace meld::parser::ast;

class DecoratorsIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear and register all decorators before each test
        DecoratorRegistry::instance().clear();
        register_all_decorators();
    }
    
    void TearDown() override {
        DecoratorRegistry::instance().clear();
    }
};

TEST_F(DecoratorsIntegrationTest, AllRequiredDecoratorsRegistered) {
    EXPECT_TRUE(all_decorators_registered());
}

TEST_F(DecoratorsIntegrationTest, PropertyDecoratorsRegistered) {
    EXPECT_TRUE(DecoratorRegistry::instance().has_decorator("Getter"));
    EXPECT_TRUE(DecoratorRegistry::instance().has_decorator("Setter"));
}

TEST_F(DecoratorsIntegrationTest, MethodDecoratorsRegistered) {
    EXPECT_TRUE(DecoratorRegistry::instance().has_decorator("ToString"));
    EXPECT_TRUE(DecoratorRegistry::instance().has_decorator("EqualsAndHashCode"));
}

TEST_F(DecoratorsIntegrationTest, ConstructorDecoratorsRegistered) {
    EXPECT_TRUE(DecoratorRegistry::instance().has_decorator("NoArgsConstructor"));
    EXPECT_TRUE(DecoratorRegistry::instance().has_decorator("RequiredArgsConstructor"));
    EXPECT_TRUE(DecoratorRegistry::instance().has_decorator("AllArgsConstructor"));
}

TEST_F(DecoratorsIntegrationTest, CompositeDecoratorsRegistered) {
    EXPECT_TRUE(DecoratorRegistry::instance().has_decorator("Data"));
    EXPECT_TRUE(DecoratorRegistry::instance().has_decorator("Value"));
}

TEST_F(DecoratorsIntegrationTest, BuilderDecoratorRegistered) {
    EXPECT_TRUE(DecoratorRegistry::instance().has_decorator("Builder"));
}

TEST_F(DecoratorsIntegrationTest, GetRegisteredDecoratorNames) {
    auto names = get_registered_decorator_names();
    
    // Should have all 10 required decorators
    EXPECT_EQ(names.size(), 10);
    
    // Check that all required decorators are in the list
    std::vector<std::string> expected = {
        "AllArgsConstructor", "Builder", "Data", "EqualsAndHashCode", "Getter",
        "NoArgsConstructor", "RequiredArgsConstructor", "Setter", "ToString", "Value"
    };
    
    EXPECT_EQ(names, expected);
}

TEST_F(DecoratorsIntegrationTest, DataDecoratorComprehensiveTest) {
    // Create a comprehensive class for testing @Data decorator
    class_definition class_def;
    class_def.name.name = "Person";
    
    // Add mutable field
    field_declaration name_field;
    name_field.name.name = "name";
    name_field.type.type_name.name = "String";
    name_field.is_mutable = true;
    class_def.fields.push_back(name_field);
    
    // Add another mutable field
    field_declaration age_field;
    age_field.name.name = "age";
    age_field.type.type_name.name = "Int";
    age_field.is_mutable = true;
    class_def.fields.push_back(age_field);
    
    // Add immutable field
    field_declaration id_field;
    id_field.name.name = "id";
    id_field.type.type_name.name = "String";
    id_field.is_mutable = false;
    class_def.fields.push_back(id_field);
    
    // Apply @Data decorator
    MacroExpander expander;
    DecoratorContext context;
    auto result = context.apply_decorator(class_def, "Data", expander);
    
    EXPECT_TRUE(result.has_value());
    // @Data should generate: getters, setters (for mutable), toString, equals, hashCode, constructor
}

TEST_F(DecoratorsIntegrationTest, ValueDecoratorComprehensiveTest) {
    // Create a comprehensive class for testing @Value decorator
    class_definition class_def;
    class_def.name.name = "Point";
    
    // Add immutable fields (typical for @Value)
    field_declaration x_field;
    x_field.name.name = "x";
    x_field.type.type_name.name = "Int";
    x_field.is_mutable = false;
    class_def.fields.push_back(x_field);
    
    field_declaration y_field;
    y_field.name.name = "y";
    y_field.type.type_name.name = "Int";
    y_field.is_mutable = false;
    class_def.fields.push_back(y_field);
    
    // Apply @Value decorator
    MacroExpander expander;
    DecoratorContext context;
    auto result = context.apply_decorator(class_def, "Value", expander);
    
    EXPECT_TRUE(result.has_value());
    // @Value should generate: getters, toString, equals, hashCode, constructor, copy
}

TEST_F(DecoratorsIntegrationTest, BuilderDecoratorTest) {
    // Create a class for testing @Builder decorator
    class_definition class_def;
    class_def.name.name = "User";
    
    // Add fields
    field_declaration name_field;
    name_field.name.name = "name";
    name_field.type.type_name.name = "String";
    name_field.is_mutable = true;
    class_def.fields.push_back(name_field);
    
    field_declaration email_field;
    email_field.name.name = "email";
    email_field.type.type_name.name = "String";
    email_field.type.is_nullable = true;
    email_field.is_mutable = true;
    class_def.fields.push_back(email_field);
    
    // Apply @Builder decorator
    MacroExpander expander;
    DecoratorContext context;
    auto result = context.apply_decorator(class_def, "Builder", expander);
    
    EXPECT_TRUE(result.has_value());
    // @Builder should generate a builder class with fluent API
}

TEST_F(DecoratorsIntegrationTest, MultipleDecoratorsCanBeApplied) {
    // Test that multiple decorators can be applied to the same class
    class_definition class_def;
    class_def.name.name = "TestClass";
    
    field_declaration field;
    field.name.name = "value";
    field.type.type_name.name = "String";
    field.is_mutable = true;
    class_def.fields.push_back(field);
    
    MacroExpander expander;
    DecoratorContext context;
    
    // Apply multiple decorators
    auto getter_result = context.apply_decorator(class_def, "Getter", expander);
    EXPECT_TRUE(getter_result.has_value());
    
    auto setter_result = context.apply_decorator(class_def, "Setter", expander);
    EXPECT_TRUE(setter_result.has_value());
    
    auto toString_result = context.apply_decorator(class_def, "ToString", expander);
    EXPECT_TRUE(toString_result.has_value());
}

TEST_F(DecoratorsIntegrationTest, DecoratorErrorHandling) {
    // Test error handling for non-existent decorator
    class_definition class_def;
    class_def.name.name = "TestClass";
    
    MacroExpander expander;
    DecoratorContext context;
    
    auto result = context.apply_decorator(class_def, "NonExistentDecorator", expander);
    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().find("not found") != std::string::npos);
}