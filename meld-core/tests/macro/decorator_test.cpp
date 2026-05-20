#include <gtest/gtest.h>
#include "meld/macro/decorator.hpp"
#include "meld/parser/ast.hpp"

using namespace meld::macro;
using namespace meld::parser::ast;

class DecoratorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear registry before each test
        DecoratorRegistry::instance().clear();
    }
    
    void TearDown() override {
        // Clean up after each test
        DecoratorRegistry::instance().clear();
    }
};

TEST_F(DecoratorTest, RegisterDecorator) {
    // Create a simple decorator
    auto decorator = make_decorator("TestDecorator", 
        [](const class_definition& class_def, MacroExpander& expander) 
            -> std::expected<meld::kernel::Value, std::string> {
        return meld::kernel::Value(std::make_shared<meld::kernel::Symbol>("test"));
    });
    
    // Register it
    DecoratorRegistry::instance().register_decorator(decorator);
    
    // Check it exists
    EXPECT_TRUE(DecoratorRegistry::instance().has_decorator("TestDecorator"));
    
    // Retrieve it
    auto result = DecoratorRegistry::instance().get_decorator("TestDecorator");
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value()->name(), "TestDecorator");
}

TEST_F(DecoratorTest, GetNonExistentDecorator) {
    auto result = DecoratorRegistry::instance().get_decorator("NonExistent");
    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().find("not found") != std::string::npos);
}

TEST_F(DecoratorTest, ApplySimpleDecorator) {
    // Create a decorator that returns a symbol
    auto decorator = make_decorator("SimpleDecorator",
        [](const class_definition& class_def, MacroExpander& expander)
            -> std::expected<meld::kernel::Value, std::string> {
        auto sym = std::make_shared<meld::kernel::Symbol>(
            "decorated_" + class_def.name.name
        );
        return meld::kernel::Value(sym);
    });
    
    DecoratorRegistry::instance().register_decorator(decorator);
    
    // Create a simple class definition
    class_definition class_def;
    class_def.name.name = "TestClass";
    
    // Apply the decorator
    MacroExpander expander;
    DecoratorContext context;
    auto result = context.apply_decorator(class_def, "SimpleDecorator", expander);
    
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(result->is<meld::kernel::Symbol>());
    
    auto sym = result->as<meld::kernel::Symbol>();
    EXPECT_EQ(sym->name(), "decorated_TestClass");
}

TEST_F(DecoratorTest, ApplyMultipleDecorators) {
    // Create two decorators
    auto decorator1 = make_decorator("Decorator1",
        [](const class_definition& class_def, MacroExpander& expander)
            -> std::expected<meld::kernel::Value, std::string> {
        auto sym = std::make_shared<meld::kernel::Symbol>("d1_" + class_def.name.name);
        return meld::kernel::Value(sym);
    });
    
    auto decorator2 = make_decorator("Decorator2",
        [](const class_definition& class_def, MacroExpander& expander)
            -> std::expected<meld::kernel::Value, std::string> {
        auto sym = std::make_shared<meld::kernel::Symbol>("d2_" + class_def.name.name);
        return meld::kernel::Value(sym);
    });
    
    DecoratorRegistry::instance().register_decorator(decorator1);
    DecoratorRegistry::instance().register_decorator(decorator2);
    
    // Create a class definition
    class_definition class_def;
    class_def.name.name = "TestClass";
    
    // Apply both decorators
    MacroExpander expander;
    DecoratorContext context;
    std::vector<std::string> decorators = {"Decorator1", "Decorator2"};
    auto result = context.apply_decorators(class_def, decorators, expander);
    
    EXPECT_TRUE(result.has_value());
}

TEST_F(DecoratorTest, GenerateGetter) {
    field_declaration field;
    field.name.name = "myField";
    field.is_mutable = false;
    field.type.type_name.name = "Int";
    
    auto getter = generate_getter("MyClass", field);
    
    EXPECT_TRUE(getter.is<meld::kernel::Symbol>());
    auto sym = getter.as<meld::kernel::Symbol>();
    EXPECT_EQ(sym->name(), "getMyField");
}

TEST_F(DecoratorTest, GenerateSetter) {
    field_declaration field;
    field.name.name = "myField";
    field.is_mutable = true;
    field.type.type_name.name = "Int";
    
    auto setter = generate_setter("MyClass", field);
    
    EXPECT_TRUE(setter.is<meld::kernel::Symbol>());
    auto sym = setter.as<meld::kernel::Symbol>();
    EXPECT_EQ(sym->name(), "setMyField");
}

TEST_F(DecoratorTest, GenerateToString) {
    std::vector<field_declaration> fields;
    
    field_declaration field1;
    field1.name.name = "name";
    field1.type.type_name.name = "String";
    fields.push_back(field1);
    
    field_declaration field2;
    field2.name.name = "age";
    field2.type.type_name.name = "Int";
    fields.push_back(field2);
    
    auto to_string = generate_to_string("Person", fields);
    
    EXPECT_TRUE(to_string.is<meld::kernel::Symbol>());
    auto sym = to_string.as<meld::kernel::Symbol>();
    EXPECT_EQ(sym->name(), "toString");
}

TEST_F(DecoratorTest, GenerateConstructors) {
    std::vector<field_declaration> fields;
    
    field_declaration field1;
    field1.name.name = "name";
    field1.type.type_name.name = "String";
    fields.push_back(field1);
    
    // Test no-args constructor
    auto no_args = generate_no_args_constructor("Person");
    EXPECT_TRUE(no_args.is<meld::kernel::Symbol>());
    EXPECT_EQ(no_args.as<meld::kernel::Symbol>()->name(), "Person");
    
    // Test all-args constructor
    auto all_args = generate_all_args_constructor("Person", fields);
    EXPECT_TRUE(all_args.is<meld::kernel::Symbol>());
    EXPECT_EQ(all_args.as<meld::kernel::Symbol>()->name(), "Person");
}

TEST_F(DecoratorTest, GenerateBuilder) {
    std::vector<field_declaration> fields;
    
    field_declaration field1;
    field1.name.name = "name";
    field1.type.type_name.name = "String";
    fields.push_back(field1);
    
    auto builder = generate_builder("Person", fields);
    
    EXPECT_TRUE(builder.is<meld::kernel::Symbol>());
    auto sym = builder.as<meld::kernel::Symbol>();
    EXPECT_EQ(sym->name(), "PersonBuilder");
}

