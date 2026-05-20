#include <gtest/gtest.h>
#include "meld/kernel/operations.hpp"
#include "meld/macro/composite_decorators.hpp"
#include "meld/kernel/operations.hpp"
#include "meld/parser/ast.hpp"

using namespace meld::macro;
using namespace meld::parser::ast;

class CompositeDecoratorsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear and register decorators before each test
        DecoratorRegistry::instance().clear();
        register_composite_decorators();
    }
    
    void TearDown() override {
        DecoratorRegistry::instance().clear();
    }
};

TEST_F(CompositeDecoratorsTest, DataDecoratorRegistered) {
    EXPECT_TRUE(DecoratorRegistry::instance().has_decorator("Data"));
}

TEST_F(CompositeDecoratorsTest, ValueDecoratorRegistered) {
    EXPECT_TRUE(DecoratorRegistry::instance().has_decorator("Value"));
}

TEST_F(CompositeDecoratorsTest, DataDecoratorGeneratesMultipleMethods) {
    // Create a mutable class
    class_definition class_def;
    class_def.name.name = "Person";
    
    field_declaration field1;
    field1.name.name = "name";
    field1.type.type_name.name = "String";
    field1.is_mutable = true; // var
    class_def.fields.push_back(field1);
    
    field_declaration field2;
    field2.name.name = "age";
    field2.type.type_name.name = "Int";
    field2.is_mutable = true; // var
    class_def.fields.push_back(field2);
    
    // Apply @Data decorator
    MacroExpander expander;
    DecoratorContext context;
    auto result = context.apply_decorator(class_def, "Data", expander);
    
    EXPECT_TRUE(result.has_value());
    
    // Result should be a list of generated methods
    if (result->is<meld::kernel::Cons>()) {
        auto list_result = meld::kernel::list_to_array(*result);
        EXPECT_TRUE(list_result.has_value());
        
        // @Data should generate:
        // - 2 getters (getName, getAge)
        // - 2 setters (setName, setAge)
        // - 1 toString
        // - 1 equals
        // - 1 hashCode
        // - 1 constructor
        // Total: 8 methods
        EXPECT_EQ(list_result->size(), 8);
    }
}

TEST_F(CompositeDecoratorsTest, DataDecoratorWithMixedMutability) {
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
    
    // Apply @Data decorator
    MacroExpander expander;
    DecoratorContext context;
    auto result = context.apply_decorator(class_def, "Data", expander);
    
    EXPECT_TRUE(result.has_value());
    
    // Result should be a list of generated methods
    if (result->is<meld::kernel::Cons>()) {
        auto list_result = meld::kernel::list_to_array(*result);
        EXPECT_TRUE(list_result.has_value());
        
        // @Data should generate:
        // - 2 getters (getName, getBirthYear)
        // - 1 setter (setName only, not setBirthYear)
        // - 1 toString
        // - 1 equals
        // - 1 hashCode
        // - 1 constructor
        // Total: 7 methods
        EXPECT_EQ(list_result->size(), 7);
    }
}

TEST_F(CompositeDecoratorsTest, ValueDecoratorGeneratesMultipleMethods) {
    // Create an immutable class
    class_definition class_def;
    class_def.name.name = "Point";
    
    field_declaration field1;
    field1.name.name = "x";
    field1.type.type_name.name = "Int";
    field1.is_mutable = false; // val
    class_def.fields.push_back(field1);
    
    field_declaration field2;
    field2.name.name = "y";
    field2.type.type_name.name = "Int";
    field2.is_mutable = false; // val
    class_def.fields.push_back(field2);
    
    // Apply @Value decorator
    MacroExpander expander;
    DecoratorContext context;
    auto result = context.apply_decorator(class_def, "Value", expander);
    
    EXPECT_TRUE(result.has_value());
    
    // Result should be a list of generated methods
    if (result->is<meld::kernel::Cons>()) {
        auto list_result = meld::kernel::list_to_array(*result);
        EXPECT_TRUE(list_result.has_value());
        
        // @Value should generate:
        // - 2 getters (getX, getY)
        // - 1 toString
        // - 1 equals
        // - 1 hashCode
        // - 1 constructor
        // - 1 copy
        // Total: 7 methods
        EXPECT_EQ(list_result->size(), 7);
    }
}

TEST_F(CompositeDecoratorsTest, ValueDecoratorDoesNotGenerateSetters) {
    // Create an immutable class
    class_definition class_def;
    class_def.name.name = "ImmutableData";
    
    field_declaration field1;
    field1.name.name = "value";
    field1.type.type_name.name = "String";
    field1.is_mutable = false; // val
    class_def.fields.push_back(field1);
    
    // Apply @Value decorator
    MacroExpander expander;
    DecoratorContext context;
    auto result = context.apply_decorator(class_def, "Value", expander);
    
    EXPECT_TRUE(result.has_value());
    
    // Verify no setters are generated
    if (result->is<meld::kernel::Cons>()) {
        auto list_result = meld::kernel::list_to_array(*result);
        EXPECT_TRUE(list_result.has_value());
        
        // Check that none of the generated methods are setters
        for (const auto& method : *list_result) {
            if (method.is<meld::kernel::Symbol>()) {
                auto sym = method.as<meld::kernel::Symbol>();
                // Setter names start with "set"
                EXPECT_FALSE(sym->name().starts_with("set"));
            }
        }
    }
}

TEST_F(CompositeDecoratorsTest, DataDecoratorWithEmptyClass) {
    // Create an empty class
    class_definition class_def;
    class_def.name.name = "EmptyClass";
    
    // Apply @Data decorator
    MacroExpander expander;
    DecoratorContext context;
    auto result = context.apply_decorator(class_def, "Data", expander);
    
    EXPECT_TRUE(result.has_value());
    
    // Should still generate some methods (toString, equals, hashCode, constructor)
    if (result->is<meld::kernel::Cons>()) {
        auto list_result = meld::kernel::list_to_array(*result);
        EXPECT_TRUE(list_result.has_value());
        
        // @Data with no fields should generate:
        // - 0 getters
        // - 0 setters
        // - 1 toString
        // - 1 equals
        // - 1 hashCode
        // - 1 constructor
        // Total: 4 methods
        EXPECT_EQ(list_result->size(), 4);
    }
}

TEST_F(CompositeDecoratorsTest, ValueDecoratorWithEmptyClass) {
    // Create an empty class
    class_definition class_def;
    class_def.name.name = "EmptyValue";
    
    // Apply @Value decorator
    MacroExpander expander;
    DecoratorContext context;
    auto result = context.apply_decorator(class_def, "Value", expander);
    
    EXPECT_TRUE(result.has_value());
    
    // Should still generate some methods
    if (result->is<meld::kernel::Cons>()) {
        auto list_result = meld::kernel::list_to_array(*result);
        EXPECT_TRUE(list_result.has_value());
        
        // @Value with no fields should generate:
        // - 0 getters
        // - 1 toString
        // - 1 equals
        // - 1 hashCode
        // - 1 constructor
        // - 1 copy
        // Total: 5 methods
        EXPECT_EQ(list_result->size(), 5);
    }
}

