/**
 * Tests for compile-time nil rejection (Task 48.1).
 *
 * Validates Requirements 14A-NIL.20 and 14A-NIL.21:
 * - nil assignment to non-optional types is rejected
 * - nil as argument to non-optional parameters is rejected
 * - nil return from non-optional return types is rejected
 * - optional types accept nil
 */

#include <gtest/gtest.h>
#include "meld/compiler/type_checker.hpp"
#include "meld/parser/ast.hpp"
#include "meld/meta/metatype.hpp"

using namespace meld::compiler;
using namespace meld::parser::ast;
using namespace meld::meta;
namespace x3 = boost::spirit::x3;

class NilRejectionTest : public ::testing::Test {
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

    expression make_val_expr(val_declaration& decl) {
        expression e;
        e = x3::forward_ast<val_declaration>(decl);
        return e;
    }

    expression make_var_expr(var_declaration& decl) {
        expression e;
        e = x3::forward_ast<var_declaration>(decl);
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
};

// --- val declaration nil rejection (Req 14A-NIL.20) ---

TEST_F(NilRejectionTest, ValWithNonOptionalStringRejectsNil) {
    val_declaration decl;
    decl.name.name = "x";
    decl.has_type_annotation = true;
    type_annotation ann;
    ann.type_name.name = "string";
    ann.is_nullable = false;
    decl.type_ann = ann;
    decl.value = make_nil_expr();

    auto result = checker->check_expression(make_val_expr(decl), env);
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().message.find("cannot assign nil to non-nullable type"), std::string::npos);
    EXPECT_NE(result.error().message.find("string"), std::string::npos);
}

TEST_F(NilRejectionTest, ValWithNonOptionalIntRejectsNil) {
    val_declaration decl;
    decl.name.name = "n";
    decl.has_type_annotation = true;
    type_annotation ann;
    ann.type_name.name = "int";
    ann.is_nullable = false;
    decl.type_ann = ann;
    decl.value = make_nil_expr();

    auto result = checker->check_expression(make_val_expr(decl), env);
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().message.find("cannot assign nil to non-nullable type"), std::string::npos);
    EXPECT_NE(result.error().message.find("int"), std::string::npos);
}

TEST_F(NilRejectionTest, ValWithOptionalTypeAcceptsNil) {
    val_declaration decl;
    decl.name.name = "x";
    decl.has_type_annotation = true;
    type_annotation ann;
    ann.type_name.name = "string";
    ann.is_nullable = true;
    decl.type_ann = ann;
    decl.value = make_nil_expr();

    auto result = checker->check_expression(make_val_expr(decl), env);
    ASSERT_TRUE(result.has_value());
}

TEST_F(NilRejectionTest, ValWithoutAnnotationAcceptsNil) {
    val_declaration decl;
    decl.name.name = "x";
    decl.has_type_annotation = false;
    decl.value = make_nil_expr();

    auto result = checker->check_expression(make_val_expr(decl), env);
    ASSERT_TRUE(result.has_value());
}

TEST_F(NilRejectionTest, ValWithNonOptionalTypeAcceptsNonNil) {
    val_declaration decl;
    decl.name.name = "x";
    decl.has_type_annotation = true;
    type_annotation ann;
    ann.type_name.name = "int";
    ann.is_nullable = false;
    decl.type_ann = ann;
    decl.value = make_int_expr(42);

    auto result = checker->check_expression(make_val_expr(decl), env);
    ASSERT_TRUE(result.has_value());
}

// --- var declaration nil rejection ---

TEST_F(NilRejectionTest, VarWithNonOptionalTypeRejectsNil) {
    var_declaration decl;
    decl.name.name = "x";
    decl.has_type_annotation = true;
    type_annotation ann;
    ann.type_name.name = "string";
    ann.is_nullable = false;
    decl.type_ann = ann;
    decl.value = make_nil_expr();

    auto result = checker->check_expression(make_var_expr(decl), env);
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().message.find("cannot assign nil to non-nullable type"), std::string::npos);
}

TEST_F(NilRejectionTest, VarWithOptionalTypeAcceptsNil) {
    var_declaration decl;
    decl.name.name = "x";
    decl.has_type_annotation = true;
    type_annotation ann;
    ann.type_name.name = "int";
    ann.is_nullable = true;
    decl.type_ann = ann;
    decl.value = make_nil_expr();

    auto result = checker->check_expression(make_var_expr(decl), env);
    ASSERT_TRUE(result.has_value());
}

// --- function call nil argument rejection (Req 14A-NIL.21) ---

TEST_F(NilRejectionTest, FunctionCallRejectsNilForNonOptionalParam) {
    // First define a function with a non-optional string parameter
    function_definition func_def;
    func_def.name.name = "greet";
    func_def.has_return_type = true;
    func_def.return_type.type_name.name = "unit";
    function_parameter param;
    param.name.name = "user";
    param.type.type_name.name = "string";
    param.type.is_nullable = false;
    func_def.parameters.push_back(param);

    auto func_result = checker->check_expression(make_func_expr(func_def), env);
    ASSERT_TRUE(func_result.has_value());

    // Now call it with nil
    function_call call;
    call.function_name.name = "greet";
    call.arguments.push_back(make_nil_expr());

    auto result = checker->check_expression(make_call_expr(call), env);
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().message.find("cannot pass nil to parameter of type"), std::string::npos);
    EXPECT_NE(result.error().message.find("string"), std::string::npos);
}

TEST_F(NilRejectionTest, FunctionCallAcceptsNilForOptionalParam) {
    function_definition func_def;
    func_def.name.name = "maybe-greet";
    func_def.has_return_type = true;
    func_def.return_type.type_name.name = "unit";
    function_parameter param;
    param.name.name = "user";
    param.type.type_name.name = "string";
    param.type.is_nullable = true;
    func_def.parameters.push_back(param);

    auto func_result = checker->check_expression(make_func_expr(func_def), env);
    ASSERT_TRUE(func_result.has_value());

    function_call call;
    call.function_name.name = "maybe-greet";
    call.arguments.push_back(make_nil_expr());

    auto result = checker->check_expression(make_call_expr(call), env);
    ASSERT_TRUE(result.has_value());
}

// --- function return nil rejection ---

TEST_F(NilRejectionTest, FunctionWithNonOptionalReturnRejectsNilReturn) {
    function_definition func_def;
    func_def.name.name = "get-name";
    func_def.has_return_type = true;
    func_def.return_type.type_name.name = "string";
    func_def.return_type.is_nullable = false;

    block_expression body;
    return_statement ret;
    ret.has_expression = true;
    ret.expr = make_nil_expr();
    expression ret_expr;
    ret_expr = x3::forward_ast<return_statement>(ret);
    body.statements.push_back(ret_expr);
    func_def.body = body;

    auto result = checker->check_expression(make_func_expr(func_def), env);
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().message.find("cannot return nil from function"), std::string::npos);
    EXPECT_NE(result.error().message.find("string"), std::string::npos);
}

TEST_F(NilRejectionTest, FunctionWithOptionalReturnAcceptsNilReturn) {
    function_definition func_def;
    func_def.name.name = "maybe-name";
    func_def.has_return_type = true;
    func_def.return_type.type_name.name = "string";
    func_def.return_type.is_nullable = true;

    block_expression body;
    return_statement ret;
    ret.has_expression = true;
    ret.expr = make_nil_expr();
    expression ret_expr;
    ret_expr = x3::forward_ast<return_statement>(ret);
    body.statements.push_back(ret_expr);
    func_def.body = body;

    auto result = checker->check_expression(make_func_expr(func_def), env);
    ASSERT_TRUE(result.has_value());
}

// --- nil identifier type inference ---

TEST_F(NilRejectionTest, NilIdentifierReturnsNullType) {
    auto result = checker->check_expression(make_nil_expr(), env);
    ASSERT_TRUE(result.has_value());
    auto& registry = TypeRegistry::instance();
    EXPECT_EQ((*result).get(), registry.get_null_type().get());
}

// --- error message quality ---

TEST_F(NilRejectionTest, ErrorMessageSuggestsOptionalSyntax) {
    val_declaration decl;
    decl.name.name = "x";
    decl.has_type_annotation = true;
    type_annotation ann;
    ann.type_name.name = "string";
    ann.is_nullable = false;
    decl.type_ann = ann;
    decl.value = make_nil_expr();

    auto result = checker->check_expression(make_val_expr(decl), env);
    ASSERT_FALSE(result.has_value());
    // Error context should suggest using optional syntax
    EXPECT_TRUE(result.error().context.find("optional[string]") != std::string::npos ||
                result.error().context.find("string?") != std::string::npos);
}
