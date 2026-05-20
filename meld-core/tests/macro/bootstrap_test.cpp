#include <gtest/gtest.h>
#include "meld/macro/bootstrap.hpp"
#include "meld/kernel/operations.hpp"
#include "meld/kernel/symbol_table.hpp"
#include "meld/meta/metatype.hpp"

using namespace meld;
using namespace meld::macro;
using namespace meld::kernel;
using namespace meld::meta;

class BootstrapTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear registries before each test
        MacroRegistry::instance().clear();
        
        // Register bootstrap macros
        register_bootstrap_macros();
    }
    
    void TearDown() override {
        MacroRegistry::instance().clear();
    }
};

TEST_F(BootstrapTest, ClassMacroRegistered) {
    auto result = MacroRegistry::instance().get_macro("class");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value()->name(), "class");
}

TEST_F(BootstrapTest, StructMacroRegistered) {
    auto result = MacroRegistry::instance().get_macro("struct");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value()->name(), "struct");
}

TEST_F(BootstrapTest, TraitMacroRegistered) {
    auto result = MacroRegistry::instance().get_macro("trait");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value()->name(), "trait");
}

TEST_F(BootstrapTest, SimpleStructExpansion) {
    // Create a struct definition:
    // (struct Point (field x Int false) (field y Int false))
    auto struct_call = list({
        Value(SymbolTable::instance().intern("struct")),
        Value(SymbolTable::instance().intern("Point")),
        list({
            Value(SymbolTable::instance().intern("field")),
            Value(SymbolTable::instance().intern("x")),
            Value(SymbolTable::instance().intern("Int"))
        }),
        list({
            Value(SymbolTable::instance().intern("field")),
            Value(SymbolTable::instance().intern("y")),
            Value(SymbolTable::instance().intern("Int"))
        })
    });
    
    // Expand the macro
    MacroExpander expander;
    auto result = expander.expand(struct_call);
    
    ASSERT_TRUE(result.has_value());
    
    // Check that Point type was registered
    auto type_result = TypeRegistry::instance().get_type("Point");
    ASSERT_TRUE(type_result.has_value());
    
    auto point_type = *type_result;
    EXPECT_EQ(point_type->name(), "Point");
    EXPECT_TRUE(point_type->is_value_type());
    
    // Check that it's a StructMetaType
    auto struct_type = std::dynamic_pointer_cast<StructMetaType>(point_type);
    ASSERT_NE(struct_type, nullptr);
    
    // Check fields
    const auto& fields = struct_type->fields();
    ASSERT_EQ(fields.size(), 2);
    EXPECT_EQ(fields[0].name, "x");
    EXPECT_EQ(fields[0].type->name(), "Int");
    EXPECT_EQ(fields[1].name, "y");
    EXPECT_EQ(fields[1].type->name(), "Int");
}

TEST_F(BootstrapTest, SimpleClassExpansion) {
    // Create a class definition:
    // (class Person (field name String false) (field age Int false))
    auto class_call = list({
        Value(SymbolTable::instance().intern("class")),
        Value(SymbolTable::instance().intern("Person")),
        list({
            Value(SymbolTable::instance().intern("field")),
            Value(SymbolTable::instance().intern("name")),
            Value(SymbolTable::instance().intern("String"))
        }),
        list({
            Value(SymbolTable::instance().intern("field")),
            Value(SymbolTable::instance().intern("age")),
            Value(SymbolTable::instance().intern("Int"))
        })
    });
    
    // Expand the macro
    MacroExpander expander;
    auto result = expander.expand(class_call);
    
    ASSERT_TRUE(result.has_value());
    
    // Check that Person type was registered
    auto type_result = TypeRegistry::instance().get_type("Person");
    ASSERT_TRUE(type_result.has_value());
    
    auto person_type = *type_result;
    EXPECT_EQ(person_type->name(), "Person");
    EXPECT_FALSE(person_type->is_value_type()); // Classes are reference types
    
    // Check that it's a ClassMetaType
    auto class_type = std::dynamic_pointer_cast<ClassMetaType>(person_type);
    ASSERT_NE(class_type, nullptr);
    
    // Check fields
    const auto& fields = class_type->fields();
    ASSERT_EQ(fields.size(), 2);
    EXPECT_EQ(fields[0].name, "name");
    EXPECT_EQ(fields[0].type->name(), "String");
    EXPECT_EQ(fields[1].name, "age");
    EXPECT_EQ(fields[1].type->name(), "Int");
}

TEST_F(BootstrapTest, ClassWithMethods) {
    // Create a class with a method:
    // (class Counter 
    //   (field count Int false)
    //   (method increment () Int (begin)))
    auto class_call = list({
        Value(SymbolTable::instance().intern("class")),
        Value(SymbolTable::instance().intern("Counter")),
        list({
            Value(SymbolTable::instance().intern("field")),
            Value(SymbolTable::instance().intern("count")),
            Value(SymbolTable::instance().intern("Int"))
        }),
        list({
            Value(SymbolTable::instance().intern("method")),
            Value(SymbolTable::instance().intern("increment")),
            list({}), // No parameters
            Value(SymbolTable::instance().intern("Int")),
            Value(SymbolTable::instance().intern("begin"))
        })
    });
    
    // Expand the macro
    MacroExpander expander;
    auto result = expander.expand(class_call);
    
    ASSERT_TRUE(result.has_value());
    
    // Check that Counter type was registered
    auto type_result = TypeRegistry::instance().get_type("Counter");
    ASSERT_TRUE(type_result.has_value());
    
    auto counter_type = std::dynamic_pointer_cast<ClassMetaType>(*type_result);
    ASSERT_NE(counter_type, nullptr);
    
    // Check methods
    const auto& methods = counter_type->methods();
    ASSERT_EQ(methods.size(), 1);
    EXPECT_EQ(methods[0].name, "increment");
    EXPECT_EQ(methods[0].return_type->name(), "Int");
    EXPECT_EQ(methods[0].param_types.size(), 0);
}

TEST_F(BootstrapTest, SimpleTraitExpansion) {
    // Create a trait definition:
    // (trait Drawable (method draw () String (begin)))
    auto trait_call = list({
        Value(SymbolTable::instance().intern("trait")),
        Value(SymbolTable::instance().intern("Drawable")),
        list({
            Value(SymbolTable::instance().intern("method")),
            Value(SymbolTable::instance().intern("draw")),
            list({}), // No parameters
            Value(SymbolTable::instance().intern("String")),
            Value(SymbolTable::instance().intern("begin"))
        })
    });
    
    // Expand the macro
    MacroExpander expander;
    auto result = expander.expand(trait_call);
    
    ASSERT_TRUE(result.has_value());
    
    // Check that Drawable type was registered
    auto type_result = TypeRegistry::instance().get_type("Drawable");
    ASSERT_TRUE(type_result.has_value());
    
    auto drawable_type = std::dynamic_pointer_cast<TraitMetaType>(*type_result);
    ASSERT_NE(drawable_type, nullptr);
    
    EXPECT_EQ(drawable_type->name(), "Drawable");
    
    // Check methods
    const auto& methods = drawable_type->methods();
    ASSERT_EQ(methods.size(), 1);
    EXPECT_EQ(methods[0].name, "draw");
    EXPECT_EQ(methods[0].return_type->name(), "String");
}

TEST_F(BootstrapTest, ExtractFields) {
    // Create field definitions
    std::vector<Value> field_defs = {
        list({
            Value(SymbolTable::instance().intern("field")),
            Value(SymbolTable::instance().intern("x")),
            Value(SymbolTable::instance().intern("Int"))
        }),
        list({
            Value(SymbolTable::instance().intern("field")),
            Value(SymbolTable::instance().intern("name")),
            Value(SymbolTable::instance().intern("String")),
            Value(Boolean::from(true)) // mutable
        })
    };
    
    auto result = extract_fields(field_defs);
    ASSERT_TRUE(result.has_value());
    
    const auto& fields = *result;
    ASSERT_EQ(fields.size(), 2);
    
    EXPECT_EQ(fields[0].name, "x");
    EXPECT_EQ(fields[0].type->name(), "Int");
    EXPECT_FALSE(fields[0].is_mutable);
    
    EXPECT_EQ(fields[1].name, "name");
    EXPECT_EQ(fields[1].type->name(), "String");
    EXPECT_TRUE(fields[1].is_mutable);
}

TEST_F(BootstrapTest, ExtractMethods) {
    // Create method definitions
    std::vector<Value> method_defs = {
        list({
            Value(SymbolTable::instance().intern("method")),
            Value(SymbolTable::instance().intern("getName")),
            list({}),
            Value(SymbolTable::instance().intern("String")),
            Value(SymbolTable::instance().intern("begin"))
        }),
        list({
            Value(SymbolTable::instance().intern("method")),
            Value(SymbolTable::instance().intern("add")),
            list({
                Value(SymbolTable::instance().intern("Int")),
                Value(SymbolTable::instance().intern("Int"))
            }),
            Value(SymbolTable::instance().intern("Int")),
            Value(SymbolTable::instance().intern("begin"))
        })
    };
    
    auto result = extract_methods(method_defs);
    ASSERT_TRUE(result.has_value());
    
    const auto& methods = *result;
    ASSERT_EQ(methods.size(), 2);
    
    EXPECT_EQ(methods[0].name, "getName");
    EXPECT_EQ(methods[0].return_type->name(), "String");
    EXPECT_EQ(methods[0].param_types.size(), 0);
    
    EXPECT_EQ(methods[1].name, "add");
    EXPECT_EQ(methods[1].return_type->name(), "Int");
    ASSERT_EQ(methods[1].param_types.size(), 2);
    EXPECT_EQ(methods[1].param_types[0]->name(), "Int");
    EXPECT_EQ(methods[1].param_types[1]->name(), "Int");
}
