#include <gtest/gtest.h>
#include "meld/parser/parser.hpp"
#include "meld/parser/ast.hpp"

using namespace meld::parser;
using namespace meld::parser::ast;

// Test basic initialization block parsing
TEST(InitializationBlockTest, BasicInitialization) {
    Parser parser;
    expression result;
    
    std::string input = R"(Person { name = "Alice", age = 30 })";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    // Check that result is an initialization_block
    auto* init_block = boost::get<x3::forward_ast<initialization_block>>(&result);
    ASSERT_NE(init_block, nullptr) << "Expected initialization_block";
    
    const initialization_block& block = init_block->get();
    
    // Check type name
    EXPECT_EQ(block.type_name.name, "Person");
    
    // Check parameters
    ASSERT_EQ(block.parameters.size(), 2);
    
    EXPECT_EQ(block.parameters[0].name.name, "name");
    auto* name_value = boost::get<string_literal>(&block.parameters[0].value.get());
    ASSERT_NE(name_value, nullptr);
    EXPECT_EQ(name_value->value, "Alice");
    
    EXPECT_EQ(block.parameters[1].name.name, "age");
    auto* age_value = boost::get<integer_literal>(&block.parameters[1].value.get());
    ASSERT_NE(age_value, nullptr);
    EXPECT_EQ(age_value->value, 30);
}

// Test initialization block with single parameter
TEST(InitializationBlockTest, SingleParameter) {
    Parser parser;
    expression result;
    
    std::string input = R"(Point { x = 10 })";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* init_block = boost::get<x3::forward_ast<initialization_block>>(&result);
    ASSERT_NE(init_block, nullptr);
    
    const initialization_block& block = init_block->get();
    
    EXPECT_EQ(block.type_name.name, "Point");
    ASSERT_EQ(block.parameters.size(), 1);
    EXPECT_EQ(block.parameters[0].name.name, "x");
}

// Test initialization block with no parameters
TEST(InitializationBlockTest, EmptyInitialization) {
    Parser parser;
    expression result;
    
    std::string input = R"(Empty { })";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* init_block = boost::get<x3::forward_ast<initialization_block>>(&result);
    ASSERT_NE(init_block, nullptr);
    
    const initialization_block& block = init_block->get();
    
    EXPECT_EQ(block.type_name.name, "Empty");
    EXPECT_EQ(block.parameters.size(), 0);
}

// Test initialization block with trailing comma
TEST(InitializationBlockTest, TrailingComma) {
    Parser parser;
    expression result;
    
    std::string input = R"(Person { name = "Bob", age = 25, })";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* init_block = boost::get<x3::forward_ast<initialization_block>>(&result);
    ASSERT_NE(init_block, nullptr);
    
    const initialization_block& block = init_block->get();
    
    EXPECT_EQ(block.type_name.name, "Person");
    ASSERT_EQ(block.parameters.size(), 2);
}

// Test initialization block with nested expressions
TEST(InitializationBlockTest, NestedExpressions) {
    Parser parser;
    expression result;
    
    std::string input = R"(Rectangle { width = 10, height = 20 })";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* init_block = boost::get<x3::forward_ast<initialization_block>>(&result);
    ASSERT_NE(init_block, nullptr);
    
    const initialization_block& block = init_block->get();
    
    EXPECT_EQ(block.type_name.name, "Rectangle");
    ASSERT_EQ(block.parameters.size(), 2);
    
    EXPECT_EQ(block.parameters[0].name.name, "width");
    EXPECT_EQ(block.parameters[1].name.name, "height");
}

// Test initialization block in val declaration
TEST(InitializationBlockTest, ValDeclarationWithInitialization) {
    Parser parser;
    expression result;
    
    std::string input = R"(val person = Person { name = "Charlie", age = 35 })";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* val_decl = boost::get<x3::forward_ast<val_declaration>>(&result);
    ASSERT_NE(val_decl, nullptr);
    
    const val_declaration& decl = val_decl->get();
    
    EXPECT_EQ(decl.name.name, "person");
    
    auto* init_block = boost::get<x3::forward_ast<initialization_block>>(&decl.value.get());
    ASSERT_NE(init_block, nullptr);
    
    const initialization_block& block = init_block->get();
    EXPECT_EQ(block.type_name.name, "Person");
    ASSERT_EQ(block.parameters.size(), 2);
}

// Test initialization block with kebab-case identifiers
TEST(InitializationBlockTest, KebabCaseIdentifiers) {
    Parser parser;
    expression result;
    
    std::string input = R"(my-type { first-name = "David", last-name = "Smith" })";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* init_block = boost::get<x3::forward_ast<initialization_block>>(&result);
    ASSERT_NE(init_block, nullptr);
    
    const initialization_block& block = init_block->get();
    
    EXPECT_EQ(block.type_name.name, "my-type");
    ASSERT_EQ(block.parameters.size(), 2);
    EXPECT_EQ(block.parameters[0].name.name, "first-name");
    EXPECT_EQ(block.parameters[1].name.name, "last-name");
}

// Test initialization block with boolean values
TEST(InitializationBlockTest, BooleanValues) {
    Parser parser;
    expression result;
    
    std::string input = R"(Config { enabled = true, debug = false })";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* init_block = boost::get<x3::forward_ast<initialization_block>>(&result);
    ASSERT_NE(init_block, nullptr);
    
    const initialization_block& block = init_block->get();
    
    EXPECT_EQ(block.type_name.name, "Config");
    ASSERT_EQ(block.parameters.size(), 2);
    
    auto* enabled_value = boost::get<boolean_literal>(&block.parameters[0].value.get());
    ASSERT_NE(enabled_value, nullptr);
    EXPECT_TRUE(enabled_value->value);
    
    auto* debug_value = boost::get<boolean_literal>(&block.parameters[1].value.get());
    ASSERT_NE(debug_value, nullptr);
    EXPECT_FALSE(debug_value->value);
}

// Test nested initialization blocks
TEST(InitializationBlockTest, NestedInitialization) {
    Parser parser;
    expression result;
    
    std::string input = R"(Company { 
        name = "TechCorp", 
        address = Address { 
            street = "123 Main St", 
            city = "Techville" 
        } 
    })";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* init_block = boost::get<x3::forward_ast<initialization_block>>(&result);
    ASSERT_NE(init_block, nullptr);
    
    const initialization_block& block = init_block->get();
    
    EXPECT_EQ(block.type_name.name, "Company");
    ASSERT_EQ(block.parameters.size(), 2);
    
    // Check first parameter (name)
    EXPECT_EQ(block.parameters[0].name.name, "name");
    auto* name_value = boost::get<string_literal>(&block.parameters[0].value.get());
    ASSERT_NE(name_value, nullptr);
    EXPECT_EQ(name_value->value, "TechCorp");
    
    // Check second parameter (address) - should be a nested initialization block
    EXPECT_EQ(block.parameters[1].name.name, "address");
    auto* nested_init = boost::get<x3::forward_ast<initialization_block>>(&block.parameters[1].value.get());
    ASSERT_NE(nested_init, nullptr) << "Expected nested initialization block for address";
    
    const initialization_block& nested_block = nested_init->get();
    EXPECT_EQ(nested_block.type_name.name, "Address");
    ASSERT_EQ(nested_block.parameters.size(), 2);
    
    EXPECT_EQ(nested_block.parameters[0].name.name, "street");
    auto* street_value = boost::get<string_literal>(&nested_block.parameters[0].value.get());
    ASSERT_NE(street_value, nullptr);
    EXPECT_EQ(street_value->value, "123 Main St");
    
    EXPECT_EQ(nested_block.parameters[1].name.name, "city");
    auto* city_value = boost::get<string_literal>(&nested_block.parameters[1].value.get());
    ASSERT_NE(city_value, nullptr);
    EXPECT_EQ(city_value->value, "Techville");
}

// Test initialization block with list of nested initialization blocks
TEST(InitializationBlockTest, ListOfNestedInitialization) {
    Parser parser;
    expression result;
    
    std::string input = R"(Company { 
        name = "TechCorp",
        employees = [
            Person { name = "Alice", age = 30 },
            Person { name = "Bob", age = 25 }
        ]
    })";
    
    ASSERT_TRUE(parser.parse_expression(input, result)) << "Parse error: " << parser.error_message();
    
    auto* init_block = boost::get<x3::forward_ast<initialization_block>>(&result);
    ASSERT_NE(init_block, nullptr);
    
    const initialization_block& block = init_block->get();
    
    EXPECT_EQ(block.type_name.name, "Company");
    ASSERT_EQ(block.parameters.size(), 2);
    
    // Check employees parameter - should be an array
    EXPECT_EQ(block.parameters[1].name.name, "employees");
    auto* array_expr = boost::get<x3::forward_ast<anonymous_array_literal>>(&block.parameters[1].value.get());
    ASSERT_NE(array_expr, nullptr) << "Expected anonymous array literal for employees";
    
    const anonymous_array_literal& list = array_expr->get();
    ASSERT_EQ(list.elements.size(), 2);
    
    // Check first employee
    auto* employee1 = boost::get<x3::forward_ast<initialization_block>>(&list.elements[0].get());
    ASSERT_NE(employee1, nullptr);
    EXPECT_EQ(employee1->get().type_name.name, "Person");
    ASSERT_EQ(employee1->get().parameters.size(), 2);
    
    // Check second employee
    auto* employee2 = boost::get<x3::forward_ast<initialization_block>>(&list.elements[1].get());
    ASSERT_NE(employee2, nullptr);
    EXPECT_EQ(employee2->get().type_name.name, "Person");
    ASSERT_EQ(employee2->get().parameters.size(), 2);
}
