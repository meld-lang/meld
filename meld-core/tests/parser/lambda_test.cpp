#include <gtest/gtest.h>
#include "meld/parser/parser.hpp"
#include "meld/parser/ast.hpp"
#include "meld/meta/metatype.hpp"

using namespace meld::parser;
using namespace meld::parser::ast;
using namespace meld::meta;
namespace x3 = boost::spirit::x3;

class LambdaTest : public ::testing::Test {
protected:
    bool parse_expr(const std::string& source, expression& result) {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        if (!lexer.errors().empty()) return false;
        TokenParser parser(tokens);
        return parser.parse_expression(result);
    }
};

TEST_F(LambdaTest, ParseSimpleLambda) {
    std::string input = "{ x => x + 1 }";

    expression result;
    ASSERT_TRUE(parse_expr(input, result));

    auto* lambda = boost::get<x3::forward_ast<lambda_expression>>(&result);
    ASSERT_NE(lambda, nullptr);

    const auto& lambda_expr = lambda->get();
    EXPECT_EQ(lambda_expr.parameters.size(), 1u);
    EXPECT_EQ(lambda_expr.parameters[0].name.name, "x");
    EXPECT_FALSE(lambda_expr.is_block);
}

TEST_F(LambdaTest, ParseArrowLambda) {
    std::string input = "x => x * 2";

    expression result;
    ASSERT_TRUE(parse_expr(input, result));

    auto* lambda = boost::get<x3::forward_ast<lambda_expression>>(&result);
    ASSERT_NE(lambda, nullptr);

    const auto& lambda_expr = lambda->get();
    EXPECT_EQ(lambda_expr.parameters.size(), 1u);
    EXPECT_EQ(lambda_expr.parameters[0].name.name, "x");
    EXPECT_FALSE(lambda_expr.is_block);
}

TEST_F(LambdaTest, ParseMultiParameterLambda) {
    std::string input = "(x, y) => x + y";

    expression result;
    ASSERT_TRUE(parse_expr(input, result));

    auto* lambda = boost::get<x3::forward_ast<lambda_expression>>(&result);
    ASSERT_NE(lambda, nullptr);

    const auto& lambda_expr = lambda->get();
    EXPECT_EQ(lambda_expr.parameters.size(), 2u);
    EXPECT_EQ(lambda_expr.parameters[0].name.name, "x");
    EXPECT_EQ(lambda_expr.parameters[1].name.name, "y");
    EXPECT_FALSE(lambda_expr.is_block);
}

TEST_F(LambdaTest, ParseTypedLambda) {
    std::string input = "(x: int, y: int) => x + y";

    expression result;
    ASSERT_TRUE(parse_expr(input, result));

    auto* lambda = boost::get<x3::forward_ast<lambda_expression>>(&result);
    ASSERT_NE(lambda, nullptr);

    const auto& lambda_expr = lambda->get();
    EXPECT_EQ(lambda_expr.parameters.size(), 2u);
    EXPECT_EQ(lambda_expr.parameters[0].name.name, "x");
    EXPECT_TRUE(lambda_expr.parameters[0].has_type);
    EXPECT_EQ(lambda_expr.parameters[0].type.type_name.name, "int");
    EXPECT_EQ(lambda_expr.parameters[1].name.name, "y");
    EXPECT_TRUE(lambda_expr.parameters[1].has_type);
    EXPECT_EQ(lambda_expr.parameters[1].type.type_name.name, "int");
}

TEST_F(LambdaTest, FunctionTypeCreation) {
    auto& registry = TypeRegistry::instance();

    std::vector<std::shared_ptr<MetaType>> param_types = {
        registry.get_int_type(),
        registry.get_int_type()
    };
    auto return_type = registry.get_int_type();

    auto func_type = registry.create_function_type(param_types, return_type);
    ASSERT_NE(func_type, nullptr);

    auto* function_meta = dynamic_cast<FunctionMetaType*>(func_type.get());
    ASSERT_NE(function_meta, nullptr);

    EXPECT_EQ(function_meta->param_types().size(), 2u);
    EXPECT_EQ(function_meta->param_types()[0], registry.get_int_type());
    EXPECT_EQ(function_meta->param_types()[1], registry.get_int_type());
    EXPECT_EQ(function_meta->return_type(), registry.get_int_type());
}

TEST_F(LambdaTest, FunctionTypeCompatibility) {
    auto& registry = TypeRegistry::instance();

    std::vector<std::shared_ptr<MetaType>> param_types = {
        registry.get_int_type(),
        registry.get_int_type()
    };
    auto return_type = registry.get_int_type();

    auto func_type1 = std::dynamic_pointer_cast<FunctionMetaType>(
        registry.create_function_type(param_types, return_type)
    );
    auto func_type2 = std::dynamic_pointer_cast<FunctionMetaType>(
        registry.create_function_type(param_types, return_type)
    );

    ASSERT_NE(func_type1, nullptr);
    ASSERT_NE(func_type2, nullptr);

    EXPECT_TRUE(func_type1->is_compatible_with(*func_type2));
    EXPECT_TRUE(func_type2->is_compatible_with(*func_type1));
}

TEST_F(LambdaTest, FunctionTypeCallability) {
    auto& registry = TypeRegistry::instance();

    std::vector<std::shared_ptr<MetaType>> param_types = {
        registry.get_int_type(),
        registry.get_int_type()
    };
    auto return_type = registry.get_int_type();

    auto func_type = std::dynamic_pointer_cast<FunctionMetaType>(
        registry.create_function_type(param_types, return_type)
    );
    ASSERT_NE(func_type, nullptr);

    // Correct argument types
    std::vector<std::shared_ptr<MetaType>> arg_types = {
        registry.get_int_type(),
        registry.get_int_type()
    };
    EXPECT_TRUE(func_type->can_call_with(arg_types));

    // Wrong argument count
    std::vector<std::shared_ptr<MetaType>> wrong_count = {
        registry.get_int_type()
    };
    EXPECT_FALSE(func_type->can_call_with(wrong_count));

    // Wrong argument types
    std::vector<std::shared_ptr<MetaType>> wrong_types = {
        registry.get_string_type(),
        registry.get_int_type()
    };
    EXPECT_FALSE(func_type->can_call_with(wrong_types));
}
