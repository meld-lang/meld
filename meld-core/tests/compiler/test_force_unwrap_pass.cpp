/// @file test_force_unwrap_pass.cpp
/// @brief Unit tests for the Force-Unwrap / Panic Operator Pass
///
/// Tests that the pass correctly:
///   - Detects postfix `!!` on optional values (no error)
///   - Detects postfix `!!` on Result values (no error)
///   - Warns (W4707) when `!!` is used on non-optional, non-Result type
///   - Rejects `opr !!` definitions (E4702)
///   - Allows `!!` at top level (no function context needed)
///   - Handles multiple `!!` in same function
///   - Handles `!!` inside function body
///   - Verifies source file propagation in diagnostics
///   - Type helper static methods (is_optional_type, is_result_type)
///   - Forbidden symbols set contains only "!!"
///   - Mixed valid usage and forbidden overload
///   - Non-!! unary operators are ignored
///
/// Requirements: 14E.18, 14E.19, 24C.23

#include <gtest/gtest.h>
#include "meld/compiler/force_unwrap_pass.hpp"

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

/// Create a function_call expression: func_name(args...)
static ast::expression make_function_call(
    const std::string& func_name,
    std::vector<ast::expression> args
) {
    ast::function_call call;
    call.function_name = make_id(func_name);
    for (auto& arg : args) {
        call.arguments.push_back(
            boost::spirit::x3::forward_ast<ast::expression>(std::move(arg)));
    }
    return ast::expression(
        boost::spirit::x3::forward_ast<ast::function_call>(std::move(call)));
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

class ForceUnwrapPassTest : public ::testing::Test {
protected:
    ForceUnwrapPass pass;
};

// ===========================================================================
// Empty / no-op cases
// ===========================================================================

TEST_F(ForceUnwrapPassTest, EmptyProgramNoDiagnostics) {
    std::vector<ast::expression> exprs;
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.force_unwraps_checked, 0u);
    EXPECT_EQ(result.operator_defs_checked, 0u);
    EXPECT_EQ(result.errors_emitted, 0u);
    EXPECT_EQ(result.warnings_emitted, 0u);
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST_F(ForceUnwrapPassTest, ProgramWithNoForceUnwrapNoDiagnostics) {
    std::vector<ast::expression> exprs = {make_id_expr("foo")};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.force_unwraps_checked, 0u);
    EXPECT_TRUE(result.diagnostics.empty());
}

// ===========================================================================
// Basic !! on optional value (no error) — Req 14E.18
// ===========================================================================

TEST_F(ForceUnwrapPassTest, BasicForceUnwrapOnOptionalValue) {
    // val name = nickname!!  — valid, targets optional[T]
    auto force = make_unary("!!", make_id_expr("nickname"));
    auto val_decl = make_val("name", std::move(force));
    std::vector<ast::expression> exprs = {val_decl};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.force_unwraps_checked, 1u);
    EXPECT_EQ(result.errors_emitted, 0u);
}

// ===========================================================================
// Basic !! on Result value (no error) — Req 14E.18
// ===========================================================================

TEST_F(ForceUnwrapPassTest, BasicForceUnwrapOnResultValue) {
    // val config = load_config(path)!!  — valid, targets Result[T, E]
    auto call_expr = make_function_call("load_config", {make_id_expr("path")});
    auto force = make_unary("!!", std::move(call_expr));
    auto val_decl = make_val("config", std::move(force));
    std::vector<ast::expression> exprs = {val_decl};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.force_unwraps_checked, 1u);
    EXPECT_EQ(result.errors_emitted, 0u);
}

// ===========================================================================
// W4707: !! on non-optional, non-Result type (warning via direct emitter)
// ===========================================================================

TEST_F(ForceUnwrapPassTest, UnnecessaryForceUnwrapWarningDirect) {
    ForceUnwrapResult result;
    pass.emit_unnecessary_force_unwrap_warning("string",
                                                "test.meld", 10, 5, result);
    EXPECT_TRUE(result.success);  // warnings don't fail the pass
    EXPECT_EQ(result.warnings_emitted, 1u);
    EXPECT_EQ(result.errors_emitted, 0u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "W4707");
    EXPECT_EQ(result.diagnostics[0].level, ForceUnwrapDiagnostic::Level::Warning);
    EXPECT_NE(result.diagnostics[0].message.find("!!"), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("string"), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("optional[T]"), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("Result[T, E]"), std::string::npos);
    EXPECT_EQ(result.diagnostics[0].source_file, "test.meld");
    EXPECT_EQ(result.diagnostics[0].line, 10u);
    EXPECT_EQ(result.diagnostics[0].column, 5u);
}

// ===========================================================================
// E4702: Rejection of opr !! definitions — Req 14E.19
// ===========================================================================

TEST_F(ForceUnwrapPassTest, RejectOperatorFunctionForceUnwrap) {
    // opr !! { ... } — forbidden
    auto op = make_operator_func("!!");
    std::vector<ast::expression> exprs = {op};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.operator_defs_checked, 1u);
    EXPECT_EQ(result.errors_emitted, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4702");
    EXPECT_EQ(result.diagnostics[0].level, ForceUnwrapDiagnostic::Level::Error);
    EXPECT_NE(result.diagnostics[0].message.find("!!"), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("non-overloadable"),
              std::string::npos);
}

TEST_F(ForceUnwrapPassTest, RejectCustomOperatorForceUnwrap) {
    // custom opr !! { ... } — also forbidden
    auto op = make_custom_operator("!!");
    std::vector<ast::expression> exprs = {op};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errors_emitted, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4702");
}

// ===========================================================================
// !! at top level — allowed (unlike ? and ?!) — Req 14E.18
// ===========================================================================

TEST_F(ForceUnwrapPassTest, ForceUnwrapAtTopLevelAllowed) {
    // val x = data!! — at top level, no enclosing function — ALLOWED
    auto force = make_unary("!!", make_id_expr("data"));
    auto val_decl = make_val("x", std::move(force));
    std::vector<ast::expression> exprs = {val_decl};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.force_unwraps_checked, 1u);
    EXPECT_EQ(result.errors_emitted, 0u);
    EXPECT_TRUE(result.diagnostics.empty());
}

// ===========================================================================
// Multiple !! in same function
// ===========================================================================

TEST_F(ForceUnwrapPassTest, MultipleForceUnwrapsInSameFunction) {
    // fnc f() { val a = x!!; val b = y!! }
    auto fu1 = make_unary("!!", make_id_expr("x"));
    auto val1 = make_val("a", std::move(fu1));
    auto fu2 = make_unary("!!", make_id_expr("y"));
    auto val2 = make_val("b", std::move(fu2));
    auto func = make_function_no_return_type("f", {val1, val2});
    std::vector<ast::expression> exprs = {func};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.force_unwraps_checked, 2u);
    EXPECT_EQ(result.errors_emitted, 0u);
}

// ===========================================================================
// !! inside function body
// ===========================================================================

TEST_F(ForceUnwrapPassTest, ForceUnwrapInsideFunctionBody) {
    // fnc load() -> Config { val config = load_config(path)!! }
    auto call_expr = make_function_call("load_config", {make_id_expr("path")});
    auto force = make_unary("!!", std::move(call_expr));
    auto val_decl = make_val("config", std::move(force));
    auto func = make_function_with_return_type(
        "load", make_type("Config"), {val_decl});
    std::vector<ast::expression> exprs = {func};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.force_unwraps_checked, 1u);
    EXPECT_EQ(result.errors_emitted, 0u);
}

// ===========================================================================
// Source file propagation
// ===========================================================================

TEST_F(ForceUnwrapPassTest, SourceFileInDiagnostics) {
    auto op = make_operator_func("!!");
    std::vector<ast::expression> exprs = {op};
    auto result = pass.run(exprs, "src/quintet.meld");
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].source_file, "src/quintet.meld");
}

// ===========================================================================
// Type helper static methods
// ===========================================================================

TEST_F(ForceUnwrapPassTest, IsOptionalTypeNullable) {
    auto type = make_type("string", true);
    EXPECT_TRUE(ForceUnwrapPass::is_optional_type(type));
}

TEST_F(ForceUnwrapPassTest, IsOptionalTypeExplicit) {
    auto type = make_optional_type("string");
    EXPECT_TRUE(ForceUnwrapPass::is_optional_type(type));
}

TEST_F(ForceUnwrapPassTest, IsResultType) {
    auto type = make_result_type("Config", "IOError");
    EXPECT_TRUE(ForceUnwrapPass::is_result_type(type));
}

TEST_F(ForceUnwrapPassTest, NonOptionalNonResultType) {
    auto type = make_type("string", false);
    EXPECT_FALSE(ForceUnwrapPass::is_optional_type(type));
    EXPECT_FALSE(ForceUnwrapPass::is_result_type(type));
}

// ===========================================================================
// Forbidden operator symbols set
// ===========================================================================

TEST_F(ForceUnwrapPassTest, ForbiddenSymbolsContainDoubleBang) {
    const auto& symbols = ForceUnwrapPass::forbidden_operator_symbols();
    EXPECT_TRUE(symbols.count("!!"));
    EXPECT_EQ(symbols.size(), 1u);
}

TEST_F(ForceUnwrapPassTest, ForbiddenSymbolsDoNotContainOtherOps) {
    const auto& symbols = ForceUnwrapPass::forbidden_operator_symbols();
    EXPECT_FALSE(symbols.count("+"));
    EXPECT_FALSE(symbols.count("?."));
    EXPECT_FALSE(symbols.count("?"));
    EXPECT_FALSE(symbols.count("?!"));
}

// ===========================================================================
// Mixed valid usage and forbidden overload
// ===========================================================================

TEST_F(ForceUnwrapPassTest, MixedValidUsageAndForbiddenOverload) {
    // val a = x!!                — valid force-unwrap
    // opr !! { ... }             — forbidden overload
    auto force = make_unary("!!", make_id_expr("x"));
    auto val_decl = make_val("a", std::move(force));
    auto forbidden_op = make_operator_func("!!");
    std::vector<ast::expression> exprs = {val_decl, forbidden_op};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.force_unwraps_checked, 1u);
    EXPECT_EQ(result.operator_defs_checked, 1u);
    EXPECT_EQ(result.errors_emitted, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4702");
}

// ===========================================================================
// Non-!! unary operators are ignored
// ===========================================================================

TEST_F(ForceUnwrapPassTest, NonDoubleBangUnaryIgnored) {
    // unary ! on an expression — should not be counted as force-unwrap
    auto unary_not = make_unary("!", make_id_expr("flag"));
    std::vector<ast::expression> exprs = {unary_not};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.force_unwraps_checked, 0u);
    EXPECT_EQ(result.errors_emitted, 0u);
}

TEST_F(ForceUnwrapPassTest, QuestionBangUnaryIgnored) {
    // unary ?! on an expression — should not be counted as force-unwrap
    auto unary_qb = make_unary("?!", make_id_expr("data"));
    std::vector<ast::expression> exprs = {unary_qb};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.force_unwraps_checked, 0u);
}

// ===========================================================================
// Allowed operator definitions (non-forbidden symbols)
// ===========================================================================

TEST_F(ForceUnwrapPassTest, AllowOperatorFunctionForNonForbiddenSymbol) {
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
// E4702: Direct test of forbidden overload error emitter
// ===========================================================================

TEST_F(ForceUnwrapPassTest, EmitForbiddenOverloadErrorDirectly) {
    ForceUnwrapResult result;
    pass.emit_forbidden_overload_error("!!",
                                        "test.meld", 20, 3, result);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errors_emitted, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4702");
    EXPECT_EQ(result.diagnostics[0].level, ForceUnwrapDiagnostic::Level::Error);
    EXPECT_NE(result.diagnostics[0].message.find("!!"), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("non-overloadable"),
              std::string::npos);
    EXPECT_EQ(result.diagnostics[0].source_file, "test.meld");
    EXPECT_EQ(result.diagnostics[0].line, 20u);
    EXPECT_EQ(result.diagnostics[0].column, 3u);
}

// ===========================================================================
// !! in function with Result return type — valid
// ===========================================================================

TEST_F(ForceUnwrapPassTest, ForceUnwrapInResultReturningFunction) {
    // fnc f() -> Result[Config, IOError] { val a = x!! }
    auto force = make_unary("!!", make_id_expr("x"));
    auto val_decl = make_val("a", std::move(force));
    auto func = make_function_with_return_type(
        "f", make_result_type("Config", "IOError"), {val_decl});
    std::vector<ast::expression> exprs = {func};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.force_unwraps_checked, 1u);
    EXPECT_EQ(result.errors_emitted, 0u);
}

// ===========================================================================
// !! in function with optional return type — valid (!! works on both)
// ===========================================================================

TEST_F(ForceUnwrapPassTest, ForceUnwrapInOptionalReturningFunction) {
    // fnc f() -> optional[string] { val a = x!! }
    auto force = make_unary("!!", make_id_expr("x"));
    auto val_decl = make_val("a", std::move(force));
    auto func = make_function_with_return_type(
        "f", make_optional_type("string"), {val_decl});
    std::vector<ast::expression> exprs = {func};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.force_unwraps_checked, 1u);
    EXPECT_EQ(result.errors_emitted, 0u);
}
