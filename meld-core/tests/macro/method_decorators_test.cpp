#include <gtest/gtest.h>
#include "meld/kernel/operations.hpp"
#include "meld/macro/method_decorators.hpp"
#include "meld/kernel/operations.hpp"
#include "meld/parser/ast.hpp"

using namespace meld::macro;
using namespace meld::parser::ast;

class MethodDecoratorsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear and register decorators before each test
        DecoratorRegistry::instance().clear();
        register_method_decorators();
    }
    
    void TearDown() override {
        DecoratorRegistry::instance().clear();
    }
};

TEST_F(MethodDecoratorsTest, ToStringDecoratorRegistered) {
    EXPECT_TRUE(DecoratorRegistry::instance().has_decorator("ToString"));
}

TEST_F(MethodDecoratorsTest, EqualsAndHashCodeDecoratorRegistered) {
    EXPECT_TRUE(DecoratorRegistry::instance().has_decorator("EqualsAndHashCode"));
}

TEST_F(MethodDecoratorsTest, ToStringGeneratesMethod) {
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
    
    // Apply @ToString decorator
    MacroExpander expander;
    DecoratorContext context;
    auto result = context.apply_decorator(class_def, "ToString", expander);
    
    EXPECT_TRUE(result.has_value());
    
    // Result should be a cons cell representing the toString method
    EXPECT_TRUE(result->is<meld::kernel::Cons>());
}

TEST_F(MethodDecoratorsTest, ToStringWithNoFields) {
    // Create an empty class
    class_definition class_def;
    class_def.name.name = "EmptyClass";
    
    // Apply @ToString decorator
    MacroExpander expander;
    DecoratorContext context;
    auto result = context.apply_decorator(class_def, "ToString", expander);
    
    EXPECT_TRUE(result.has_value());
    
    // Should still generate a toString method, just with no fields
    EXPECT_TRUE(result->is<meld::kernel::Cons>());
}

TEST_F(MethodDecoratorsTest, ToStringWithSingleField) {
    // Create a class with one field
    class_definition class_def;
    class_def.name.name = "SingleField";
    
    field_declaration field;
    field.name.name = "value";
    field.type.type_name.name = "String";
    field.is_mutable = false;
    class_def.fields.push_back(field);
    
    // Apply @ToString decorator
    MacroExpander expander;
    DecoratorContext context;
    auto result = context.apply_decorator(class_def, "ToString", expander);
    
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(result->is<meld::kernel::Cons>());
}

TEST_F(MethodDecoratorsTest, EqualsAndHashCodeGeneratesBothMethods) {
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
    
    // Apply @EqualsAndHashCode decorator
    MacroExpander expander;
    DecoratorContext context;
    auto result = context.apply_decorator(class_def, "EqualsAndHashCode", expander);
    
    EXPECT_TRUE(result.has_value());
    
    // Result should be a list of two methods (equals and hashCode)
    if (result->is<meld::kernel::Cons>()) {
        auto list_result = meld::kernel::list_to_array(*result);
        EXPECT_TRUE(list_result.has_value());
        EXPECT_EQ(list_result->size(), 2); // Two methods: equals and hashCode
    }
}

TEST_F(MethodDecoratorsTest, EqualsAndHashCodeWithNoFields) {
    // Create an empty class
    class_definition class_def;
    class_def.name.name = "EmptyClass";
    
    // Apply @EqualsAndHashCode decorator
    MacroExpander expander;
    DecoratorContext context;
    auto result = context.apply_decorator(class_def, "EqualsAndHashCode", expander);
    
    EXPECT_TRUE(result.has_value());
    
    // Should still generate both methods, even with no fields
    if (result->is<meld::kernel::Cons>()) {
        auto list_result = meld::kernel::list_to_array(*result);
        EXPECT_TRUE(list_result.has_value());
        EXPECT_EQ(list_result->size(), 2); // Two methods: equals and hashCode
    }
}

TEST_F(MethodDecoratorsTest, EqualsAndHashCodeWithMixedMutability) {
    // Create a class with both mutable and immutable fields
    class_definition class_def;
    class_def.name.name = "MixedClass";
    
    field_declaration field1;
    field1.name.name = "id";
    field1.type.type_name.name = "String";
    field1.is_mutable = false; // val
    class_def.fields.push_back(field1);
    
    field_declaration field2;
    field2.name.name = "count";
    field2.type.type_name.name = "Int";
    field2.is_mutable = true; // var
    class_def.fields.push_back(field2);
    
    field_declaration field3;
    field3.name.name = "name";
    field3.type.type_name.name = "String";
    field3.is_mutable = true; // var
    class_def.fields.push_back(field3);
    
    // Apply @EqualsAndHashCode decorator
    MacroExpander expander;
    DecoratorContext context;
    auto result = context.apply_decorator(class_def, "EqualsAndHashCode", expander);
    
    EXPECT_TRUE(result.has_value());
    
    // Should generate both methods considering all fields (regardless of mutability)
    if (result->is<meld::kernel::Cons>()) {
        auto list_result = meld::kernel::list_to_array(*result);
        EXPECT_TRUE(list_result.has_value());
        EXPECT_EQ(list_result->size(), 2); // Two methods: equals and hashCode
    }
}

TEST_F(MethodDecoratorsTest, BothDecoratorsCanBeAppliedSeparately) {
    // Create a class
    class_definition class_def;
    class_def.name.name = "TestClass";
    
    field_declaration field;
    field.name.name = "value";
    field.type.type_name.name = "Int";
    field.is_mutable = false;
    class_def.fields.push_back(field);
    
    MacroExpander expander;
    DecoratorContext context;
    
    // Apply @ToString decorator
    auto toString_result = context.apply_decorator(class_def, "ToString", expander);
    EXPECT_TRUE(toString_result.has_value());
    
    // Apply @EqualsAndHashCode decorator
    auto equals_result = context.apply_decorator(class_def, "EqualsAndHashCode", expander);
    EXPECT_TRUE(equals_result.has_value());
    
    // Both should succeed independently
    EXPECT_TRUE(toString_result->is<meld::kernel::Cons>());
    EXPECT_TRUE(equals_result->is<meld::kernel::Cons>());
}
