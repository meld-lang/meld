#include <gtest/gtest.h>
#include "meld/parser/parser.hpp"
#include "meld/parser/ast.hpp"

using namespace meld::parser;
namespace x3 = boost::spirit::x3;

class EllipsisTest : public ::testing::Test {
protected:
    Parser parser;
};

TEST_F(EllipsisTest, RestParameterInFunction) {
    std::string input = R"(
        fn sum(...numbers: Int): Int {
            return numbers.reduce(0, { acc, n => acc + n })
        }
    )";
    
    ast::expression result;
    ASSERT_TRUE(parser.parse_expression(input, result));
    
    auto* func_ast = boost::get<x3::forward_ast<ast::function_definition>>(&result);
    ASSERT_NE(func_ast, nullptr);
    auto& func_def = func_ast->get();
    
    EXPECT_EQ(func_def.name.name, "sum");
    ASSERT_EQ(func_def.parameters.size(), 1);
    EXPECT_TRUE(func_def.parameters[0].is_rest);
    EXPECT_EQ(func_def.parameters[0].name.name, "numbers");
    EXPECT_EQ(func_def.parameters[0].type.type_name.name, "Int");
}

TEST_F(EllipsisTest, RestParameterWithOtherParameters) {
    std::string input = R"(
        fn format(val template: String, ...args: String): String {
            return template.format(args)
        }
    )";
    
    ast::expression result;
    ASSERT_TRUE(parser.parse_expression(input, result));
    
    auto* func_ast = boost::get<x3::forward_ast<ast::function_definition>>(&result);
    ASSERT_NE(func_ast, nullptr);
    auto& func_def = func_ast->get();
    
    EXPECT_EQ(func_def.name.name, "format");
    ASSERT_EQ(func_def.parameters.size(), 2);
    EXPECT_FALSE(func_def.parameters[0].is_rest);
    EXPECT_EQ(func_def.parameters[0].name.name, "template");
    EXPECT_TRUE(func_def.parameters[1].is_rest);
    EXPECT_EQ(func_def.parameters[1].name.name, "args");
}

TEST_F(EllipsisTest, SpreadInListLiteral) {
    std::string input = R"(
        val list1 = [1, 2, 3]
        val list2 = [0, ...list1, 4]
    )";
    
    std::vector<ast::expression> results;
    ASSERT_TRUE(parser.parse_file(input, results));
    ASSERT_EQ(results.size(), 2);
    
    // Check second declaration (list2)
    auto* val_ast = boost::get<x3::forward_ast<ast::val_declaration>>(&results[1]);
    ASSERT_NE(val_ast, nullptr);
    auto& val_decl = val_ast->get();
    EXPECT_EQ(val_decl.name.name, "list2");
    
    // Check that value is an anonymous array literal
    auto* arr_ast = boost::get<x3::forward_ast<ast::anonymous_array_literal>>(&val_decl.value.get());
    ASSERT_NE(arr_ast, nullptr);
    auto& list_expr = arr_ast->get();
    ASSERT_EQ(list_expr.elements.size(), 3);
    
    // First element should be integer 0
    ASSERT_NE(boost::get<ast::integer_literal>(&list_expr.elements[0].get()), nullptr);
    
    // Second element should be spread expression
    auto* spread_ast = boost::get<x3::forward_ast<ast::spread_expression>>(&list_expr.elements[1].get());
    ASSERT_NE(spread_ast, nullptr);
    auto& spread = spread_ast->get();
    auto* spread_id = boost::get<ast::identifier>(&spread.collection.get());
    ASSERT_NE(spread_id, nullptr);
    EXPECT_EQ(spread_id->name, "list1");
    
    // Third element should be integer 4
    ASSERT_NE(boost::get<ast::integer_literal>(&list_expr.elements[2].get()), nullptr);
}

TEST_F(EllipsisTest, MultipleSpreadInList) {
    std::string input = R"(
        val combined = [...list1, ...list2, ...list3]
    )";
    
    ast::expression result;
    ASSERT_TRUE(parser.parse_expression(input, result));
    
    auto* val_ast = boost::get<x3::forward_ast<ast::val_declaration>>(&result);
    ASSERT_NE(val_ast, nullptr);
    auto& val_decl = val_ast->get();
    
    auto* arr_ast = boost::get<x3::forward_ast<ast::anonymous_array_literal>>(&val_decl.value.get());
    ASSERT_NE(arr_ast, nullptr);
    auto& list_expr = arr_ast->get();
    ASSERT_EQ(list_expr.elements.size(), 3);
    
    // All elements should be spread expressions
    for (const auto& elem : list_expr.elements) {
        EXPECT_NE(boost::get<x3::forward_ast<ast::spread_expression>>(&elem.get()), nullptr);
    }
}

TEST_F(EllipsisTest, SpreadWithRegularElements) {
    std::string input = R"(
        val mixed = [1, 2, ...middle, 3, 4]
    )";
    
    ast::expression result;
    ASSERT_TRUE(parser.parse_expression(input, result));
    
    auto* val_ast = boost::get<x3::forward_ast<ast::val_declaration>>(&result);
    ASSERT_NE(val_ast, nullptr);
    auto& val_decl = val_ast->get();
    
    auto* arr_ast = boost::get<x3::forward_ast<ast::anonymous_array_literal>>(&val_decl.value.get());
    ASSERT_NE(arr_ast, nullptr);
    auto& list_expr = arr_ast->get();
    ASSERT_EQ(list_expr.elements.size(), 5);
    
    // Check pattern: int, int, spread, int, int
    EXPECT_NE(boost::get<ast::integer_literal>(&list_expr.elements[0].get()), nullptr);
    EXPECT_NE(boost::get<ast::integer_literal>(&list_expr.elements[1].get()), nullptr);
    EXPECT_NE(boost::get<x3::forward_ast<ast::spread_expression>>(&list_expr.elements[2].get()), nullptr);
    EXPECT_NE(boost::get<ast::integer_literal>(&list_expr.elements[3].get()), nullptr);
    EXPECT_NE(boost::get<ast::integer_literal>(&list_expr.elements[4].get()), nullptr);
}

TEST_F(EllipsisTest, RestParameterInExtensionMethod) {
    std::string input = R"(
        extend List {
            fn append(...items: T): List<T> {
                return this.concat(items)
            }
        }
    )";
    
    ast::expression result;
    ASSERT_TRUE(parser.parse_expression(input, result));
    
    auto* ext_ast = boost::get<x3::forward_ast<ast::extension_block>>(&result);
    ASSERT_NE(ext_ast, nullptr);
    auto& ext_block = ext_ast->get();
    
    ASSERT_EQ(ext_block.methods.size(), 1);
    auto& method = ext_block.methods[0];
    
    EXPECT_EQ(method.name.name, "append");
    ASSERT_EQ(method.parameters.size(), 1);
    EXPECT_TRUE(method.parameters[0].is_rest);
    EXPECT_EQ(method.parameters[0].name.name, "items");
}
