#include <gtest/gtest.h>
#include "meld/macro/constructor_decorators.hpp"
#include "meld/kernel/operations.hpp"
#include "meld/parser/ast.hpp"

using namespace meld::macro;
using namespace meld::parser::ast;
using namespace meld::kernel;

class ConstructorDecoratorsTest : public ::testing::Test {
protected:
    void SetUp() override {
        DecoratorRegistry::instance().clear();
        register_constructor_decorators();
    }
    
    void TearDown() override {
        DecoratorRegistry::instance().clear();
    }

    // Helper: extract the first element (constructor name symbol) from a cons-cell AST
    std::string extract_constructor_name(const Value& val) {
        auto head = car(val);
        if (!head) return "";
        if (head->is<Symbol>()) return head->as<Symbol>()->name();
        return "";
    }

    // Helper: extract the third element (kind symbol) from a cons-cell AST
    std::string extract_kind(const Value& val) {
        auto rest1 = cdr(val);
        if (!rest1) return "";
        auto rest2 = cdr(*rest1);
        if (!rest2) return "";
        auto kind_val = car(*rest2);
        if (!kind_val) return "";
        if (kind_val->is<Symbol>()) return kind_val->as<Symbol>()->name();
        return "";
    }
};

TEST_F(ConstructorDecoratorsTest, NoArgsConstructorDecoratorRegistered) {
    EXPECT_TRUE(DecoratorRegistry::instance().has_decorator("NoArgsConstructor"));
}

TEST_F(ConstructorDecoratorsTest, RequiredArgsConstructorDecoratorRegistered) {
    EXPECT_TRUE(DecoratorRegistry::instance().has_decorator("RequiredArgsConstructor"));
}

TEST_F(ConstructorDecoratorsTest, AllArgsConstructorDecoratorRegistered) {
    EXPECT_TRUE(DecoratorRegistry::instance().has_decorator("AllArgsConstructor"));
}

TEST_F(ConstructorDecoratorsTest, NoArgsConstructorGeneratesConstructor) {
    class_definition class_def;
    class_def.name.name = "Person";
    
    field_declaration field1;
    field1.name.name = "name";
    field1.type.type_name.name = "String";
    field1.is_mutable = true;
    class_def.fields.push_back(field1);
    
    field_declaration field2;
    field2.name.name = "age";
    field2.type.type_name.name = "Int";
    field2.is_mutable = true;
    class_def.fields.push_back(field2);
    
    MacroExpander expander;
    DecoratorContext context;
    auto result = context.apply_decorator(class_def, "NoArgsConstructor", expander);
    
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(extract_constructor_name(*result), "Person");
    EXPECT_EQ(extract_kind(*result), "no_args_constructor");
}

TEST_F(ConstructorDecoratorsTest, RequiredArgsConstructorWithNonNullableFields) {
    class_definition class_def;
    class_def.name.name = "Person";
    
    field_declaration field1;
    field1.name.name = "name";
    field1.type.type_name.name = "String";
    field1.type.is_nullable = false;
    field1.is_mutable = false;
    class_def.fields.push_back(field1);
    
    field_declaration field2;
    field2.name.name = "email";
    field2.type.type_name.name = "String";
    field2.type.is_nullable = true;
    field2.is_mutable = true;
    class_def.fields.push_back(field2);
    
    field_declaration field3;
    field3.name.name = "age";
    field3.type.type_name.name = "Int";
    field3.type.is_nullable = false;
    field3.is_mutable = true;
    class_def.fields.push_back(field3);
    
    MacroExpander expander;
    DecoratorContext context;
    auto result = context.apply_decorator(class_def, "RequiredArgsConstructor", expander);
    
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(extract_constructor_name(*result), "Person");
    EXPECT_EQ(extract_kind(*result), "required_args_constructor");
}

TEST_F(ConstructorDecoratorsTest, RequiredArgsConstructorWithAllNullableFields) {
    class_definition class_def;
    class_def.name.name = "OptionalData";
    
    field_declaration field1;
    field1.name.name = "name";
    field1.type.type_name.name = "String";
    field1.type.is_nullable = true;
    field1.is_mutable = true;
    class_def.fields.push_back(field1);
    
    field_declaration field2;
    field2.name.name = "email";
    field2.type.type_name.name = "String";
    field2.type.is_nullable = true;
    field2.is_mutable = true;
    class_def.fields.push_back(field2);
    
    MacroExpander expander;
    DecoratorContext context;
    auto result = context.apply_decorator(class_def, "RequiredArgsConstructor", expander);
    
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(extract_constructor_name(*result), "OptionalData");
    EXPECT_EQ(extract_kind(*result), "required_args_constructor");
}

TEST_F(ConstructorDecoratorsTest, AllArgsConstructorGeneratesConstructor) {
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
    
    field_declaration field3;
    field3.name.name = "email";
    field3.type.type_name.name = "String";
    field3.type.is_nullable = true;
    field3.is_mutable = true;
    class_def.fields.push_back(field3);
    
    MacroExpander expander;
    DecoratorContext context;
    auto result = context.apply_decorator(class_def, "AllArgsConstructor", expander);
    
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(extract_constructor_name(*result), "Person");
    EXPECT_EQ(extract_kind(*result), "all_args_constructor");
}

TEST_F(ConstructorDecoratorsTest, AllArgsConstructorWithEmptyClass) {
    class_definition class_def;
    class_def.name.name = "EmptyClass";
    
    MacroExpander expander;
    DecoratorContext context;
    auto result = context.apply_decorator(class_def, "AllArgsConstructor", expander);
    
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(extract_constructor_name(*result), "EmptyClass");
    EXPECT_EQ(extract_kind(*result), "all_args_constructor");
}

TEST_F(ConstructorDecoratorsTest, MultipleConstructorDecorators) {
    class_definition class_def;
    class_def.name.name = "Person";
    
    field_declaration field1;
    field1.name.name = "name";
    field1.type.type_name.name = "String";
    field1.is_mutable = false;
    class_def.fields.push_back(field1);
    
    MacroExpander expander;
    DecoratorContext context;
    
    std::vector<std::string> decorators = {
        "NoArgsConstructor",
        "AllArgsConstructor"
    };
    auto result = context.apply_decorators(class_def, decorators, expander);
    
    EXPECT_TRUE(result.has_value());
}
