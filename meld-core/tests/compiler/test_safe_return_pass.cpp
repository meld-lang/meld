/// @file test_safe_return_pass.cpp
/// @brief Unit tests for the Safe-Return Operator Pass
///
/// Tests that the pass correctly:
///   - Detects postfix `?` on optional values inside functions
///   - Rejects `?` on Result[T, E] types (E4703)
///   - Rejects `?` in functions with non-optional return type (E4704)
///   - Rejects `?` at top level outside any function (E4704)
///   - Rejects `opr ?` definitions (E4702)
///   - Allows `?` in functions with optional return type
///   - Handles chained usage: `user?.address?`
///   - Handles multiple `?` in same function
///   - Verifies source file propagation in diagnostics
///
/// Requirements: 14C.12, 14C.13, 14C.14, 24C.20

#include <gtest/gtest.h>
#include "meld/compiler/safe_return_pass.hpp"

using namespace meld::compiler;
namespace ast = meld::parser::ast;

// ===========================================================================
// Helpers: build AST nodes for testing
// ===========================================================================

static ast::identifier make_id(const std::string& name) {
    ast::identifier id;
    id.name = name;
    return id;
}

static ast::expression make_id_expr(const std::string& name) {
    return ast::expression(make_id(name));
}

static ast::type_annotation make_type(const std::string& name, bool nullable = false) {
    ast::type_annotation type;
    type.type_name = make_id(name);
    type.is_nullable = nullable;
    return type;
}

static ast::type_annotation make_optional_type(const std::string& inner_type) {
    ast::type_annotation type;
    type.type_name = make_id("optional");
    type.has_type_arguments = true;
    ast::type_annotation inner;
    inner.type_name = make_id(inner_type);
    type.type_arguments.push_back(
        boost::spirit::x3::forward_ast<ast::type_annotation>(std::move(inner)));
    return type;
}

static ast::type_annotation make_result_type(
    const std::string& ok_type, const std::string& err_type
) {
    ast::type_annotation type;
    type.type_name = make_id("Result");
    type.has_type_arguments = true;
    ast::type_annotation ok;
    ok.type_name = make_id(ok_type);
    ast::type_annotation err;
    err.type_name = make_id(err_type);
    type.type_arguments.push_back(
        boost::spirit::x3::forward_ast<ast::type_annotation>(std::move(ok)));
    type.type_arguments.push_back(
        boost::spirit::x3::forward_ast<ast::type_annotation>(std::move(err)));
    return type;
}

/// Create a unary_operation expression with the given op.
static ast::expression make_unary(const std::string& op, ast::expression operand) {
    ast::unary_operation unary;
    unary.op = op;
    unary.operand = boost::spirit::x3::forward_ast<ast::expression>(
        std::move(operand));
    return ast::expression(
        boost::spirit::x3::forward_ast<ast::unary_operation>(std::move(unary)));
}

/// Create a safe_navigation_expression: expr?.field_name
static ast::expression make_safe_nav(ast::expression nullable_expr,
                                      const std::string& field_name) {
    ast::safe_navigation_expression safe_nav;
    safe_nav.nullable_expr = boost::spirit::x3::forward_ast<ast::expression>(
        std::move(nullable_expr));
    safe_nav.field_name = field_name;
    return ast::expression(
        boost::spirit::x3::forward_ast<ast::safe_navigation_expression>(
            std::move(safe_nav)));
}

/// Create an operator_function definition (opr symbol).
static ast::expression make_operator_func(const std::string& symbol) {
    ast::operator_function op_func;
    op_func.symbol = symbol;
    ast::block_expression body;
    op_func.body = boost::spirit::x3::forward_ast<ast::block_expression>(
        std::move(body));
    return ast::expression(
        boost::spirit::x3::forward_ast<ast::operator_function>(
            std::move(op_func)));
}

/// Create a custom_operator_definition (custom opr symbol).
static ast::expression make_custom_operator(const std::string& symbol) {
    ast::custom_operator_definition op_def;
    op_def.symbol = symbol;
    op_def.op_type = ast::CustomOperatorType::POSTFIX;
    ast::block_expression body;
    op_def.body = boost::spirit::x3::forward_ast<ast::block_expression>(
        std::move(body));
    return ast::expression(
        boost::spirit::x3::forward_ast<ast::custom_operator_definition>(
            std::move(op_def)));
}

/// Create a val_declaration wrapping an expression.
static ast::expression make_val(const std::string& name, ast::expression value) {
    ast::val_declaration decl;
    decl.name = make_id(name);
    decl.value = boost::spirit::x3::forward_ast<ast::expression>(std::move(value));
    return ast::expression(
        boost::spirit::x3::forward_ast<ast::val_declaration>(std::move(decl)));
}

/// Create a function_definition with return type and body statements.
static ast::expression make_function_with_return_type(
    const std::string& name,
    ast::type_annotation return_type,
    std::vector<ast::expression> body_stmts
) {
    ast::function_definition func;
    func.name = make_id(name);
    func.has_return_type = true;
    func.return_type = std::move(return_type);
    ast::block_expression body;
    for (auto& stmt : body_stmts) {
        body.statements.push_back(
            boost::spirit::x3::forward_ast<ast::expression>(std::move(stmt)));
    }
    func.body = boost::spirit::x3::forward_ast<ast::block_expression>(
        std::move(body));
    return ast::expression(
        boost::spirit::x3::forward_ast<ast::function_definition>(
            std::move(func)));
}

/// Create a function_definition without explicit return type.
static ast::expression make_function_no_return_type(
    const std::string& name,
    std::vector<ast::expression> body_stmts
) {
    ast::function_definition func;
    func.name = make_id(name);
    func.has_return_type = false;
    ast::block_expression body;
    for (auto& stmt : body_stmts) {
        body.statements.push_back(
            boost::spirit::x3::forward_ast<ast::expression>(std::move(stmt)));
    }
    func.body = boost::spirit::x3::forward_ast<ast::block_expression>(
        std::move(body));
    return ast::expression(
        boost::spirit::x3::forward_ast<ast::function_definition>(
            std::move(func)));
}

// ===========================================================================
// Test fixture
// ===========================================================================

class SafeReturnPassTest : public ::testing::Test {
protected:
    SafeReturnPass pass;
};

// ===========================================================================
// Empty / no-op cases
// ===========================================================================

TEST_F(SafeReturnPassTest, EmptyProgramNoDiagnostics) {
    std::vector<ast::expression> exprs;
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.safe_returns_checked, 0u);
    EXPECT_EQ(result.operator_defs_checked, 0u);
    EXPECT_EQ(result.errors_emitted, 0u);
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST_F(SafeReturnPassTest, ProgramWithNoSafeReturnNoDiagnostics) {
    std::vector<ast::expression> exprs = {make_id_expr("foo")};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.safe_returns_checked, 0u);
    EXPECT_TRUE(result.diagnostics.empty());
}

// ===========================================================================
// Basic ? detection inside function with optional return type
// ===========================================================================

TEST_F(SafeReturnPassTest, BasicSafeReturnInOptionalFunction) {
    // fnc get_zip() -> optional[string] { val addr = user? }
    auto safe_ret = make_unary("?", make_id_expr("user"));
    auto val_decl = make_val("addr", std::move(safe_ret));
    auto func = make_function_with_return_type(
        "get_zip", make_optional_type("string"), {val_decl});
    std::vector<ast::expression> exprs = {func};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.safe_returns_checked, 1u);
    EXPECT_EQ(result.errors_emitted, 0u);
}

TEST_F(SafeReturnPassTest, SafeReturnInNullableReturnFunction) {
    // fnc get_name() -> string? { val n = nickname? }
    auto safe_ret = make_unary("?", make_id_expr("nickname"));
    auto val_decl = make_val("n", std::move(safe_ret));
    auto func = make_function_with_return_type(
        "get_name", make_type("string", true), {val_decl});
    std::vector<ast::expression> exprs = {func};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.safe_returns_checked, 1u);
    EXPECT_EQ(result.errors_emitted, 0u);
}

// ===========================================================================
// E4704: ? in function with non-optional return type
// ===========================================================================

TEST_F(SafeReturnPassTest, SafeReturnInNonOptionalFunctionEmitsE4704) {
    // fnc get_zip() -> string { val addr = user? }
    auto safe_ret = make_unary("?", make_id_expr("user"));
    auto val_decl = make_val("addr", std::move(safe_ret));
    auto func = make_function_with_return_type(
        "get_zip", make_type("string", false), {val_decl});
    std::vector<ast::expression> exprs = {func};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.safe_returns_checked, 1u);
    EXPECT_EQ(result.errors_emitted, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4704");
    EXPECT_EQ(result.diagnostics[0].level, SafeReturnDiagnostic::Level::Error);
    EXPECT_NE(result.diagnostics[0].message.find("string"), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("optional[T]"), std::string::npos);
}

// ===========================================================================
// E4704: ? at top level (outside function)
// ===========================================================================

TEST_F(SafeReturnPassTest, SafeReturnAtTopLevelEmitsE4704) {
    // val x = user? — at top level, no enclosing function
    auto safe_ret = make_unary("?", make_id_expr("user"));
    auto val_decl = make_val("x", std::move(safe_ret));
    std::vector<ast::expression> exprs = {val_decl};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.safe_returns_checked, 1u);
    EXPECT_EQ(result.errors_emitted, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4704");
    EXPECT_NE(result.diagnostics[0].message.find("<top-level>"), std::string::npos);
}

// ===========================================================================
// E4702: Rejection of opr ? definitions
// ===========================================================================

TEST_F(SafeReturnPassTest, RejectOperatorFunctionSafeReturn) {
    // opr ? { ... } — forbidden
    auto op = make_operator_func("?");
    std::vector<ast::expression> exprs = {op};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.operator_defs_checked, 1u);
    EXPECT_EQ(result.errors_emitted, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4702");
    EXPECT_EQ(result.diagnostics[0].level, SafeReturnDiagnostic::Level::Error);
    EXPECT_NE(result.diagnostics[0].message.find("?"), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("non-overloadable"),
              std::string::npos);
}

TEST_F(SafeReturnPassTest, RejectCustomOperatorSafeReturn) {
    // custom opr ? { ... } — also forbidden
    auto op = make_custom_operator("?");
    std::vector<ast::expression> exprs = {op};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errors_emitted, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4702");
}

// ===========================================================================
// Allowed operator definitions (non-forbidden symbols)
// ===========================================================================

TEST_F(SafeReturnPassTest, AllowOperatorFunctionForNonForbiddenSymbol) {
    // opr + { ... } — allowed
    auto op = make_operator_func("+");
    std::vector<ast::expression> exprs = {op};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.operator_defs_checked, 1u);
    EXPECT_EQ(result.errors_emitted, 0u);
    EXPECT_TRUE(result.diagnostics.empty());
}

// ===========================================================================
// Chained usage: user?.address?
// ===========================================================================

TEST_F(SafeReturnPassTest, ChainedSafeNavThenSafeReturn) {
    // fnc get_zip() -> optional[string] { val addr = user?.address? }
    // user?.address yields safe_navigation_expression
    // then postfix ? on the result yields unary_operation with op "?"
    auto safe_nav = make_safe_nav(make_id_expr("user"), "address");
    auto safe_ret = make_unary("?", std::move(safe_nav));
    auto val_decl = make_val("addr", std::move(safe_ret));
    auto func = make_function_with_return_type(
        "get_zip", make_optional_type("string"), {val_decl});
    std::vector<ast::expression> exprs = {func};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.safe_returns_checked, 1u);
    EXPECT_EQ(result.errors_emitted, 0u);
}

// ===========================================================================
// Multiple ? in same function
// ===========================================================================

TEST_F(SafeReturnPassTest, MultipleSafeReturnsInSameFunction) {
    // fnc f() -> optional[string] { val a = x?; val b = y? }
    auto sr1 = make_unary("?", make_id_expr("x"));
    auto val1 = make_val("a", std::move(sr1));
    auto sr2 = make_unary("?", make_id_expr("y"));
    auto val2 = make_val("b", std::move(sr2));
    auto func = make_function_with_return_type(
        "f", make_optional_type("string"), {val1, val2});
    std::vector<ast::expression> exprs = {func};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.safe_returns_checked, 2u);
    EXPECT_EQ(result.errors_emitted, 0u);
}

// ===========================================================================
// ? in function without explicit return type (inferred — no error)
// ===========================================================================

TEST_F(SafeReturnPassTest, SafeReturnInFunctionWithoutReturnType) {
    // fnc f() { val a = x? }
    // No explicit return type — pass does not emit E4704 (type inference)
    auto safe_ret = make_unary("?", make_id_expr("x"));
    auto val_decl = make_val("a", std::move(safe_ret));
    auto func = make_function_no_return_type("f", {val_decl});
    std::vector<ast::expression> exprs = {func};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.safe_returns_checked, 1u);
    EXPECT_EQ(result.errors_emitted, 0u);
}

// ===========================================================================
// E4703: Direct test of non-optional error emitter
// ===========================================================================

TEST_F(SafeReturnPassTest, EmitNonOptionalErrorDirectly) {
    SafeReturnResult result;
    pass.emit_non_optional_error("Result[Config, IOError]",
                                  "test.meld", 10, 5, result);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errors_emitted, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4703");
    EXPECT_EQ(result.diagnostics[0].level, SafeReturnDiagnostic::Level::Error);
    EXPECT_NE(result.diagnostics[0].message.find("?"), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("Result[Config, IOError]"),
              std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("?!"), std::string::npos);
    EXPECT_EQ(result.diagnostics[0].source_file, "test.meld");
    EXPECT_EQ(result.diagnostics[0].line, 10u);
    EXPECT_EQ(result.diagnostics[0].column, 5u);
}

// ===========================================================================
// E4704: Direct test of incompatible return type error emitter
// ===========================================================================

TEST_F(SafeReturnPassTest, EmitIncompatibleReturnTypeErrorDirectly) {
    SafeReturnResult result;
    pass.emit_incompatible_return_type_error("string",
                                              "test.meld", 15, 8, result);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errors_emitted, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4704");
    EXPECT_EQ(result.diagnostics[0].level, SafeReturnDiagnostic::Level::Error);
    EXPECT_NE(result.diagnostics[0].message.find("string"), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("optional[T]"), std::string::npos);
    EXPECT_EQ(result.diagnostics[0].source_file, "test.meld");
    EXPECT_EQ(result.diagnostics[0].line, 15u);
    EXPECT_EQ(result.diagnostics[0].column, 8u);
}

// ===========================================================================
// Type checking static helpers
// ===========================================================================

TEST_F(SafeReturnPassTest, IsOptionalTypeNullable) {
    auto type = make_type("string", true);
    EXPECT_TRUE(SafeReturnPass::is_optional_type(type));
}

TEST_F(SafeReturnPassTest, IsOptionalTypeExplicit) {
    auto type = make_optional_type("string");
    EXPECT_TRUE(SafeReturnPass::is_optional_type(type));
}

TEST_F(SafeReturnPassTest, IsResultType) {
    auto type = make_result_type("Config", "IOError");
    EXPECT_TRUE(SafeReturnPass::is_result_type(type));
}

TEST_F(SafeReturnPassTest, NonOptionalNonResultType) {
    auto type = make_type("string", false);
    EXPECT_FALSE(SafeReturnPass::is_optional_type(type));
    EXPECT_FALSE(SafeReturnPass::is_result_type(type));
}

// ===========================================================================
// Forbidden operator symbols set
// ===========================================================================

TEST_F(SafeReturnPassTest, ForbiddenSymbolsContainQuestionMark) {
    const auto& symbols = SafeReturnPass::forbidden_operator_symbols();
    EXPECT_TRUE(symbols.count("?"));
    EXPECT_EQ(symbols.size(), 1u);
}

TEST_F(SafeReturnPassTest, ForbiddenSymbolsDoNotContainOtherOps) {
    const auto& symbols = SafeReturnPass::forbidden_operator_symbols();
    EXPECT_FALSE(symbols.count("+"));
    EXPECT_FALSE(symbols.count("?."));
    EXPECT_FALSE(symbols.count("?!"));
    EXPECT_FALSE(symbols.count("!!"));
}

// ===========================================================================
// Source file propagation
// ===========================================================================

TEST_F(SafeReturnPassTest, SourceFileInDiagnostics) {
    auto op = make_operator_func("?");
    std::vector<ast::expression> exprs = {op};
    auto result = pass.run(exprs, "src/quintet.meld");
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].source_file, "src/quintet.meld");
}

// ===========================================================================
// Mixed valid and invalid in one program
// ===========================================================================

TEST_F(SafeReturnPassTest, MixedValidUsageAndForbiddenOverload) {
    // fnc f() -> optional[string] { val a = x? }  — valid
    // opr ? { ... }                                — forbidden
    auto safe_ret = make_unary("?", make_id_expr("x"));
    auto val_decl = make_val("a", std::move(safe_ret));
    auto func = make_function_with_return_type(
        "f", make_optional_type("string"), {val_decl});
    auto forbidden_op = make_operator_func("?");
    std::vector<ast::expression> exprs = {func, forbidden_op};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.safe_returns_checked, 1u);
    EXPECT_EQ(result.operator_defs_checked, 1u);
    EXPECT_EQ(result.errors_emitted, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4702");
}

// ===========================================================================
// Non-? unary operators are ignored
// ===========================================================================

TEST_F(SafeReturnPassTest, NonQuestionMarkUnaryIgnored) {
    // unary ! on an expression — should not be counted as safe-return
    auto unary_not = make_unary("!", make_id_expr("flag"));
    std::vector<ast::expression> exprs = {unary_not};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.safe_returns_checked, 0u);
    EXPECT_EQ(result.errors_emitted, 0u);
}

// ===========================================================================
// ? in Result-returning function emits E4704
// ===========================================================================

TEST_F(SafeReturnPassTest, SafeReturnInResultFunctionEmitsE4704) {
    // fnc f() -> Result[string, Error] { val a = x? }
    auto safe_ret = make_unary("?", make_id_expr("x"));
    auto val_decl = make_val("a", std::move(safe_ret));
    auto func = make_function_with_return_type(
        "f", make_result_type("string", "Error"), {val_decl});
    std::vector<ast::expression> exprs = {func};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.safe_returns_checked, 1u);
    EXPECT_EQ(result.errors_emitted, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4704");
}
