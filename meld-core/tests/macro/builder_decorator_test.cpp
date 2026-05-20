#include <gtest/gtest.h>
#include "meld/macro/builder_decorator.hpp"
#include "meld/kernel/operations.hpp"
#include "meld/parser/ast.hpp"

using namespace meld::macro;
using namespace meld::parser::ast;
using namespace meld::kernel;

class BuilderDecoratorTest : public ::testing::Test {
protected:
    void SetUp() override {
        DecoratorRegistry::instance().clear();
        register_builder_decorator();
    }
    
    void TearDown() override {
        DecoratorRegistry::instance().clear();
    }

    // Helper: extract builder name (first element) from cons-cell AST
    std::string extract_builder_name(const Value& val) {
        auto head = car(val);
        if (!head) return "";
        if (head->is<Symbol>()) return head->as<Symbol>()->name();
        return "";
    }

    // Helper: extract target class name (second element) from cons-cell AST
    std::string extract_target_class(const Value& val) {
        auto rest1 = cdr(val);
        if (!rest1) return "";
        auto target = car(*rest1);
        if (!target) return "";
        if (target->is<Symbol>()) return target->as<Symbol>()->name();
        return "";
    }

    // Helper: extract kind (third element) from cons-cell AST
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

TEST_F(BuilderDecoratorTest, BuilderDecoratorRegistered) {
    EXPECT_TRUE(DecoratorRegistry::instance().has_decorator("Builder"));
}

TEST_F(BuilderDecoratorTest, BuilderGeneratesBuilderClass) {
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
    field2.is_mutable = false;
    class_def.fields.push_back(field2);
    
    MacroExpander expander;
    DecoratorContext context;
    auto result = context.apply_decorator(class_def, "Builder", expander);
    
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(extract_builder_name(*result), "PersonBuilder");
    EXPECT_EQ(extract_target_class(*result), "Person");
    EXPECT_EQ(extract_kind(*result), "builder");
}

TEST_F(BuilderDecoratorTest, BuilderWithMultipleFields) {
    class_definition class_def;
    class_def.name.name = "Product";
    
    field_declaration field1;
    field1.name.name = "name";
    field1.type.type_name.name = "String";
    field1.is_mutable = false;
    class_def.fields.push_back(field1);
    
    field_declaration field2;
    field2.name.name = "price";
    field2.type.type_name.name = "Float";
    field2.is_mutable = false;
    class_def.fields.push_back(field2);
    
    field_declaration field3;
    field3.name.name = "description";
    field3.type.type_name.name = "String";
    field3.type.is_nullable = true;
    field3.is_mutable = false;
    class_def.fields.push_back(field3);
    
    MacroExpander expander;
    DecoratorContext context;
    auto result = context.apply_decorator(class_def, "Builder", expander);
    
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(extract_builder_name(*result), "ProductBuilder");
    EXPECT_EQ(extract_target_class(*result), "Product");
}

TEST_F(BuilderDecoratorTest, BuilderWithEmptyClass) {
    class_definition class_def;
    class_def.name.name = "EmptyClass";
    
    MacroExpander expander;
    DecoratorContext context;
    auto result = context.apply_decorator(class_def, "Builder", expander);
    
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(extract_builder_name(*result), "EmptyClassBuilder");
}

TEST_F(BuilderDecoratorTest, BuilderWithNullableFields) {
    class_definition class_def;
    class_def.name.name = "OptionalData";
    
    field_declaration field1;
    field1.name.name = "value1";
    field1.type.type_name.name = "String";
    field1.type.is_nullable = true;
    field1.is_mutable = false;
    class_def.fields.push_back(field1);
    
    field_declaration field2;
    field2.name.name = "value2";
    field2.type.type_name.name = "Int";
    field2.type.is_nullable = true;
    field2.is_mutable = false;
    class_def.fields.push_back(field2);
    
    MacroExpander expander;
    DecoratorContext context;
    auto result = context.apply_decorator(class_def, "Builder", expander);
    
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(extract_builder_name(*result), "OptionalDataBuilder");
}

TEST_F(BuilderDecoratorTest, BuilderNamingConvention) {
    std::vector<std::string> class_names = {
        "User", "Product", "Order", "Customer", "Invoice"
    };
    
    for (const auto& class_name : class_names) {
        class_definition class_def;
        class_def.name.name = class_name;
        
        field_declaration field;
        field.name.name = "id";
        field.type.type_name.name = "Int";
        field.is_mutable = false;
        class_def.fields.push_back(field);
        
        MacroExpander expander;
        DecoratorContext context;
        auto result = context.apply_decorator(class_def, "Builder", expander);
        
        EXPECT_TRUE(result.has_value());
        
        std::string expected_name = class_name + "Builder";
        EXPECT_EQ(extract_builder_name(*result), expected_name);
        EXPECT_EQ(extract_target_class(*result), class_name);
    }
}
