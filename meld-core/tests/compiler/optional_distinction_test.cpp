/**
 * Tests for optional[T] structural distinction enforcement (Task 48.2).
 *
 * Validates Requirement 14A-NIL.22:
 * - Direct member access on optional[T] values is rejected
 * - Passing optional[T] where T is expected is rejected
 * - Non-optional types still allow member access
 * - Error messages guide toward ?., ?:, !!, or narrowing
 */

#include <gtest/gtest.h>
#include "meld/compiler/type_checker.hpp"
#include "meld/parser/ast.hpp"
#include "meld/meta/metatype.hpp"

using namespace meld::compiler;
using namespace meld::parser::ast;
using namespace meld::meta;
namespace x3 = boost::spirit::x3;

class OptionalDistinctionTest : public ::testing::Test {
protected:
    void SetUp() override {
        checker = std::make_unique<TypeChecker>();
        env = std::make_shared<TypeEnvironment>();
    }

    std::unique_ptr<TypeChecker> checker;
    std::shared_ptr<TypeEnvironment> env;

    expression make_nil_expr() {
        identifier nil_id;
        nil_id.name = "nil";
        expression e;
        e = nil_id;
        return e;
    }

    expression make_int_expr(int64_t val) {
        integer_literal lit;
        lit.value = val;
        lit.suffix = "";
        expression e;
        e = lit;
        return e;
    }

    expression make_id_expr(const std::string& name) {
        identifier id;
        id.name = name;
        expression e;
        e = id;
        return e;
    }

    expression make_val_expr(val_declaration& decl) {
        expression e;
        e = x3::forward_ast<val_declaration>(decl);
        return e;
    }

    expression make_func_expr(function_definition& def) {
        expression e;
        e = x3::forward_ast<function_definition>(def);
        return e;
    }

    expression make_call_expr(function_call& call) {
        expression e;
        e = x3::forward_ast<function_call>(call);
        return e;
    }

    expression make_tuple_indexing_expr(tuple_indexing& ti) {
        expression e;
        e = x3::forward_ast<tuple_indexing>(ti);
        return e;
    }

    // Bind a variable with an optional type in the environment
    void bind_optional(const std::string& name, const std::string& inner_type_name) {
        auto& registry = TypeRegistry::instance();
        auto inner = registry.get_type(inner_type_name);
        if (inner) {
            auto opt_type = registry.create_optional_type(*inner);
            env->bind(name, opt_type);
        }
    }

    // Bind a variable with a non-optional type
    void bind_non_optional(const std::string& name, const std::string& type_name) {
        auto& registry = TypeRegistry::instance();
        auto type = registry.get_type(type_name);
        if (type) {
            env->bind(name, *type);
        }
    }
};

// --- Direct member access on optional[T] is rejected ---

TEST_F(OptionalDistinctionTest, MemberAccessOnOptionalStringRejected) {
    bind_optional("nickname", "string");

    tuple_indexing ti;
    ti.tuple = make_id_expr("nickname");
    ti.index = "length";
    ti.is_numeric = false;

    auto result = checker->check_expression(make_tuple_indexing_expr(ti), env);
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().message.find("cannot access member"), std::string::npos);
    EXPECT_NE(result.error().message.find("optional"), std::string::npos);
}

TEST_F(OptionalDistinctionTest, MemberAccessOnOptionalIntRejected) {
    bind_optional("count", "int");

    tuple_indexing ti;
    ti.tuple = make_id_expr("count");
    ti.index = "toString";
    ti.is_numeric = false;

    auto result = checker->check_expression(make_tuple_indexing_expr(ti), env);
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().message.find("cannot access member"), std::string::npos);
}

TEST_F(OptionalDistinctionTest, NumericIndexOnOptionalRejected) {
    bind_optional("data", "string");

    tuple_indexing ti;
    ti.tuple = make_id_expr("data");
    ti.index = "0";
    ti.is_numeric = true;

    auto result = checker->check_expression(make_tuple_indexing_expr(ti), env);
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().message.find("cannot access member"), std::string::npos);
}

// --- Non-optional types still allow member access ---

TEST_F(OptionalDistinctionTest, MemberAccessOnNonOptionalAllowed) {
    bind_non_optional("name", "string");

    tuple_indexing ti;
    ti.tuple = make_id_expr("name");
    ti.index = "length";
    ti.is_numeric = false;

    auto result = checker->check_expression(make_tuple_indexing_expr(ti), env);
    // Should not fail with an optional-related error
    // (may fail for other reasons like field not found, but not optional rejection)
    if (!result.has_value()) {
        EXPECT_EQ(result.error().message.find("optional"), std::string::npos);
    }
}

// --- Passing optional[T] where T is expected ---

TEST_F(OptionalDistinctionTest, OptionalArgToNonOptionalParamRejected) {
    // Define function: fnc greet(name: string) -> unit
    function_definition func_def;
    func_def.name.name = "greet";
    func_def.has_return_type = true;
    func_def.return_type.type_name.name = "unit";
    function_parameter param;
    param.name.name = "name";
    param.type.type_name.name = "string";
    param.type.is_nullable = false;
    func_def.parameters.push_back(param);

    auto func_result = checker->check_expression(make_func_expr(func_def), env);
    ASSERT_TRUE(func_result.has_value());

    // Bind an optional[string] variable
    bind_optional("nickname", "string");

    // Call greet(nickname) — should be rejected
    function_call call;
    call.function_name.name = "greet";
    call.arguments.push_back(make_id_expr("nickname"));

    auto result = checker->check_expression(make_call_expr(call), env);
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().message.find("cannot pass optional"), std::string::npos);
    EXPECT_NE(result.error().message.find("string"), std::string::npos);
}

TEST_F(OptionalDistinctionTest, OptionalIntArgToNonOptionalParamRejected) {
    function_definition func_def;
    func_def.name.name = "double-it";
    func_def.has_return_type = true;
    func_def.return_type.type_name.name = "unit";
    function_parameter param;
    param.name.name = "n";
    param.type.type_name.name = "int";
    param.type.is_nullable = false;
    func_def.parameters.push_back(param);

    auto func_result = checker->check_expression(make_func_expr(func_def), env);
    ASSERT_TRUE(func_result.has_value());

    bind_optional("maybe-count", "int");

    function_call call;
    call.function_name.name = "double-it";
    call.arguments.push_back(make_id_expr("maybe-count"));

    auto result = checker->check_expression(make_call_expr(call), env);
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().message.find("cannot pass optional"), std::string::npos);
}

TEST_F(OptionalDistinctionTest, NonOptionalArgToNonOptionalParamAllowed) {
    function_definition func_def;
    func_def.name.name = "greet";
    func_def.has_return_type = true;
    func_def.return_type.type_name.name = "unit";
    function_parameter param;
    param.name.name = "name";
    param.type.type_name.name = "string";
    param.type.is_nullable = false;
    func_def.parameters.push_back(param);

    auto func_result = checker->check_expression(make_func_expr(func_def), env);
    ASSERT_TRUE(func_result.has_value());

    bind_non_optional("real-name", "string");

    function_call call;
    call.function_name.name = "greet";
    call.arguments.push_back(make_id_expr("real-name"));

    auto result = checker->check_expression(make_call_expr(call), env);
    ASSERT_TRUE(result.has_value());
}

TEST_F(OptionalDistinctionTest, OptionalArgToOptionalParamAllowed) {
    function_definition func_def;
    func_def.name.name = "maybe-greet";
    func_def.has_return_type = true;
    func_def.return_type.type_name.name = "unit";
    function_parameter param;
    param.name.name = "name";
    param.type.type_name.name = "string";
    param.type.is_nullable = true;
    func_def.parameters.push_back(param);

    auto func_result = checker->check_expression(make_func_expr(func_def), env);
    ASSERT_TRUE(func_result.has_value());

    bind_optional("nickname", "string");

    function_call call;
    call.function_name.name = "maybe-greet";
    call.arguments.push_back(make_id_expr("nickname"));

    auto result = checker->check_expression(make_call_expr(call), env);
    ASSERT_TRUE(result.has_value());
}

// --- Error message quality ---

TEST_F(OptionalDistinctionTest, MemberAccessErrorSuggestsAlternatives) {
    bind_optional("nickname", "string");

    tuple_indexing ti;
    ti.tuple = make_id_expr("nickname");
    ti.index = "length";
    ti.is_numeric = false;

    auto result = checker->check_expression(make_tuple_indexing_expr(ti), env);
    ASSERT_FALSE(result.has_value());
    // Error context should suggest safe navigation, elvis, force unwrap, or narrowing
    EXPECT_NE(result.error().context.find("?."), std::string::npos);
    EXPECT_NE(result.error().context.find("?:"), std::string::npos);
    EXPECT_NE(result.error().context.find("!!"), std::string::npos);
}

TEST_F(OptionalDistinctionTest, FunctionCallErrorSuggestsAlternatives) {
    function_definition func_def;
    func_def.name.name = "print-name";
    func_def.has_return_type = true;
    func_def.return_type.type_name.name = "unit";
    function_parameter param;
    param.name.name = "name";
    param.type.type_name.name = "string";
    param.type.is_nullable = false;
    func_def.parameters.push_back(param);

    auto func_result = checker->check_expression(make_func_expr(func_def), env);
    ASSERT_TRUE(func_result.has_value());

    bind_optional("nickname", "string");

    function_call call;
    call.function_name.name = "print-name";
    call.arguments.push_back(make_id_expr("nickname"));

    auto result = checker->check_expression(make_call_expr(call), env);
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().context.find("?:"), std::string::npos);
    EXPECT_NE(result.error().context.find("!!"), std::string::npos);
}

TEST_F(OptionalDistinctionTest, ErrorMessageIncludesInnerTypeName) {
    bind_optional("nickname", "string");

    tuple_indexing ti;
    ti.tuple = make_id_expr("nickname");
    ti.index = "length";
    ti.is_numeric = false;

    auto result = checker->check_expression(make_tuple_indexing_expr(ti), env);
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().message.find("string"), std::string::npos);
}
