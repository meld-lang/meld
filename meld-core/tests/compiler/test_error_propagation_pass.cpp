/// @file test_error_propagation_pass.cpp
/// @brief Unit tests for the Error-Propagation Operator Pass
///
/// Tests that the pass correctly:
///   - Detects postfix `?!` on Result values inside functions
///   - Rejects `?!` on optional[T] types (E4705)
///   - Rejects `?!` in functions with non-Result return type (E4706)
///   - Rejects `?!` at top level outside any function (E4706)
///   - Rejects `opr ?!` definitions (E4702)
///   - Allows `?!` in functions with Result return type
///   - Handles multiple `?!` in same function
///   - Handles chained: read-file(path)?! then parse-json(content)?!
///   - Verifies source file propagation in diagnostics
///   - Type helper static methods (is_optional_type, is_result_type)
///   - Forbidden symbols set contains only "?!"
///   - Mixed valid usage and forbidden overload
///
/// Requirements: 28.14, 28.15, 28.16, 24C.22

#include <gtest/gtest.h>
#include "meld/compiler/error_propagation_pass.hpp"

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

class ErrorPropagationPassTest : public ::testing::Test {
protected:
    ErrorPropagationPass pass;
};

// ===========================================================================
// Empty / no-op cases
// ===========================================================================

TEST_F(ErrorPropagationPassTest, EmptyProgramNoDiagnostics) {
    std::vector<ast::expression> exprs;
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.error_propagations_checked, 0u);
    EXPECT_EQ(result.operator_defs_checked, 0u);
    EXPECT_EQ(result.errors_emitted, 0u);
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST_F(ErrorPropagationPassTest, ProgramWithNoErrorPropagationNoDiagnostics) {
    std::vector<ast::expression> exprs = {make_id_expr("foo")};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.error_propagations_checked, 0u);
    EXPECT_TRUE(result.diagnostics.empty());
}

// ===========================================================================
// Basic ?! detection inside function with Result return type
// ===========================================================================

TEST_F(ErrorPropagationPassTest, BasicErrorPropagationInResultFunction) {
    // fnc load_config(path: string) -> Result[Config, IOError] { val content = read_file(path)?! }
    auto call_expr = make_function_call("read_file", {make_id_expr("path")});
    auto err_prop = make_unary("?!", std::move(call_expr));
    auto val_decl = make_val("content", std::move(err_prop));
    auto func = make_function_with_return_type(
        "load_config", make_result_type("Config", "IOError"), {val_decl});
    std::vector<ast::expression> exprs = {func};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.error_propagations_checked, 1u);
    EXPECT_EQ(result.errors_emitted, 0u);
}

// ===========================================================================
// E4706: ?! in function with non-Result return type
// ===========================================================================

TEST_F(ErrorPropagationPassTest, ErrorPropagationInNonResultFunctionEmitsE4706) {
    // fnc f() -> string { val x = read_file(path)?! }
    auto err_prop = make_unary("?!", make_id_expr("data"));
    auto val_decl = make_val("x", std::move(err_prop));
    auto func = make_function_with_return_type(
        "f", make_type("string", false), {val_decl});
    std::vector<ast::expression> exprs = {func};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error_propagations_checked, 1u);
    EXPECT_EQ(result.errors_emitted, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4706");
    EXPECT_EQ(result.diagnostics[0].level, ErrorPropagationDiagnostic::Level::Error);
    EXPECT_NE(result.diagnostics[0].message.find("string"), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("Result[_, E]"), std::string::npos);
}

// ===========================================================================
// E4706: ?! in function with optional return type
// ===========================================================================

TEST_F(ErrorPropagationPassTest, ErrorPropagationInOptionalFunctionEmitsE4706) {
    // fnc f() -> optional[string] { val x = data?! }
    auto err_prop = make_unary("?!", make_id_expr("data"));
    auto val_decl = make_val("x", std::move(err_prop));
    auto func = make_function_with_return_type(
        "f", make_optional_type("string"), {val_decl});
    std::vector<ast::expression> exprs = {func};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error_propagations_checked, 1u);
    EXPECT_EQ(result.errors_emitted, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4706");
}

// ===========================================================================
// E4706: ?! at top level (outside function)
// ===========================================================================

TEST_F(ErrorPropagationPassTest, ErrorPropagationAtTopLevelEmitsE4706) {
    // val x = data?! — at top level, no enclosing function
    auto err_prop = make_unary("?!", make_id_expr("data"));
    auto val_decl = make_val("x", std::move(err_prop));
    std::vector<ast::expression> exprs = {val_decl};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error_propagations_checked, 1u);
    EXPECT_EQ(result.errors_emitted, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4706");
    EXPECT_NE(result.diagnostics[0].message.find("<top-level>"), std::string::npos);
}

// ===========================================================================
// E4702: Rejection of opr ?! definitions
// ===========================================================================

TEST_F(ErrorPropagationPassTest, RejectOperatorFunctionErrorPropagation) {
    // opr ?! { ... } — forbidden
    auto op = make_operator_func("?!");
    std::vector<ast::expression> exprs = {op};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.operator_defs_checked, 1u);
    EXPECT_EQ(result.errors_emitted, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4702");
    EXPECT_EQ(result.diagnostics[0].level, ErrorPropagationDiagnostic::Level::Error);
    EXPECT_NE(result.diagnostics[0].message.find("?!"), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("non-overloadable"),
              std::string::npos);
}

TEST_F(ErrorPropagationPassTest, RejectCustomOperatorErrorPropagation) {
    // custom opr ?! { ... } — also forbidden
    auto op = make_custom_operator("?!");
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

TEST_F(ErrorPropagationPassTest, AllowOperatorFunctionForNonForbiddenSymbol) {
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
// Multiple ?! in same function
// ===========================================================================

TEST_F(ErrorPropagationPassTest, MultipleErrorPropagationsInSameFunction) {
    // fnc f() -> Result[Config, IOError] { val a = x?!; val b = y?! }
    auto ep1 = make_unary("?!", make_id_expr("x"));
    auto val1 = make_val("a", std::move(ep1));
    auto ep2 = make_unary("?!", make_id_expr("y"));
    auto val2 = make_val("b", std::move(ep2));
    auto func = make_function_with_return_type(
        "f", make_result_type("Config", "IOError"), {val1, val2});
    std::vector<ast::expression> exprs = {func};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.error_propagations_checked, 2u);
    EXPECT_EQ(result.errors_emitted, 0u);
}

// ===========================================================================
// Chained: read_file(path)?! then parse_json(content)?!
// ===========================================================================

TEST_F(ErrorPropagationPassTest, ChainedErrorPropagationCalls) {
    // fnc load_config(path: string) -> Result[Config, IOError] {
    //     val content = read_file(path)?!
    //     val parsed = parse_json(content)?!
    // }
    auto call1 = make_function_call("read_file", {make_id_expr("path")});
    auto ep1 = make_unary("?!", std::move(call1));
    auto val1 = make_val("content", std::move(ep1));

    auto call2 = make_function_call("parse_json", {make_id_expr("content")});
    auto ep2 = make_unary("?!", std::move(call2));
    auto val2 = make_val("parsed", std::move(ep2));

    auto func = make_function_with_return_type(
        "load_config", make_result_type("Config", "IOError"), {val1, val2});
    std::vector<ast::expression> exprs = {func};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.error_propagations_checked, 2u);
    EXPECT_EQ(result.errors_emitted, 0u);
}

// ===========================================================================
// ?! in function without explicit return type (inferred — no error)
// ===========================================================================

TEST_F(ErrorPropagationPassTest, ErrorPropagationInFunctionWithoutReturnType) {
    // fnc f() { val a = x?! }
    // No explicit return type — pass does not emit E4706 (type inference)
    auto err_prop = make_unary("?!", make_id_expr("x"));
    auto val_decl = make_val("a", std::move(err_prop));
    auto func = make_function_no_return_type("f", {val_decl});
    std::vector<ast::expression> exprs = {func};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.error_propagations_checked, 1u);
    EXPECT_EQ(result.errors_emitted, 0u);
}

// ===========================================================================
// E4705: Direct test of non-Result error emitter
// ===========================================================================

TEST_F(ErrorPropagationPassTest, EmitNonResultErrorDirectly) {
    ErrorPropagationResult result;
    pass.emit_non_result_error("optional[string]",
                                "test.meld", 10, 5, result);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errors_emitted, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4705");
    EXPECT_EQ(result.diagnostics[0].level, ErrorPropagationDiagnostic::Level::Error);
    EXPECT_NE(result.diagnostics[0].message.find("?!"), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("optional[string]"),
              std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("Result[T, E]"), std::string::npos);
    EXPECT_EQ(result.diagnostics[0].source_file, "test.meld");
    EXPECT_EQ(result.diagnostics[0].line, 10u);
    EXPECT_EQ(result.diagnostics[0].column, 5u);
}

// ===========================================================================
// E4706: Direct test of incompatible return type error emitter
// ===========================================================================

TEST_F(ErrorPropagationPassTest, EmitIncompatibleReturnTypeErrorDirectly) {
    ErrorPropagationResult result;
    pass.emit_incompatible_return_type_error("string",
                                              "test.meld", 15, 8, result);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errors_emitted, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4706");
    EXPECT_EQ(result.diagnostics[0].level, ErrorPropagationDiagnostic::Level::Error);
    EXPECT_NE(result.diagnostics[0].message.find("string"), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("Result[_, E]"), std::string::npos);
    EXPECT_EQ(result.diagnostics[0].source_file, "test.meld");
    EXPECT_EQ(result.diagnostics[0].line, 15u);
    EXPECT_EQ(result.diagnostics[0].column, 8u);
}

// ===========================================================================
// E4702: Direct test of forbidden overload error emitter
// ===========================================================================

TEST_F(ErrorPropagationPassTest, EmitForbiddenOverloadErrorDirectly) {
    ErrorPropagationResult result;
    pass.emit_forbidden_overload_error("?!",
                                        "test.meld", 20, 3, result);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errors_emitted, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4702");
    EXPECT_EQ(result.diagnostics[0].level, ErrorPropagationDiagnostic::Level::Error);
    EXPECT_NE(result.diagnostics[0].message.find("?!"), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("non-overloadable"),
              std::string::npos);
    EXPECT_EQ(result.diagnostics[0].source_file, "test.meld");
    EXPECT_EQ(result.diagnostics[0].line, 20u);
    EXPECT_EQ(result.diagnostics[0].column, 3u);
}

// ===========================================================================
// Type checking static helpers
// ===========================================================================

TEST_F(ErrorPropagationPassTest, IsOptionalTypeNullable) {
    auto type = make_type("string", true);
    EXPECT_TRUE(ErrorPropagationPass::is_optional_type(type));
}

TEST_F(ErrorPropagationPassTest, IsOptionalTypeExplicit) {
    auto type = make_optional_type("string");
    EXPECT_TRUE(ErrorPropagationPass::is_optional_type(type));
}

TEST_F(ErrorPropagationPassTest, IsResultType) {
    auto type = make_result_type("Config", "IOError");
    EXPECT_TRUE(ErrorPropagationPass::is_result_type(type));
}

TEST_F(ErrorPropagationPassTest, NonOptionalNonResultType) {
    auto type = make_type("string", false);
    EXPECT_FALSE(ErrorPropagationPass::is_optional_type(type));
    EXPECT_FALSE(ErrorPropagationPass::is_result_type(type));
}

// ===========================================================================
// Forbidden operator symbols set
// ===========================================================================

TEST_F(ErrorPropagationPassTest, ForbiddenSymbolsContainQuestionBang) {
    const auto& symbols = ErrorPropagationPass::forbidden_operator_symbols();
    EXPECT_TRUE(symbols.count("?!"));
    EXPECT_EQ(symbols.size(), 1u);
}

TEST_F(ErrorPropagationPassTest, ForbiddenSymbolsDoNotContainOtherOps) {
    const auto& symbols = ErrorPropagationPass::forbidden_operator_symbols();
    EXPECT_FALSE(symbols.count("+"));
    EXPECT_FALSE(symbols.count("?."));
    EXPECT_FALSE(symbols.count("?"));
    EXPECT_FALSE(symbols.count("!!"));
}

// ===========================================================================
// Source file propagation
// ===========================================================================

TEST_F(ErrorPropagationPassTest, SourceFileInDiagnostics) {
    auto op = make_operator_func("?!");
    std::vector<ast::expression> exprs = {op};
    auto result = pass.run(exprs, "src/quintet.meld");
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].source_file, "src/quintet.meld");
}

// ===========================================================================
// Mixed valid usage and forbidden overload
// ===========================================================================

TEST_F(ErrorPropagationPassTest, MixedValidUsageAndForbiddenOverload) {
    // fnc f() -> Result[string, Error] { val a = x?! }  — valid
    // opr ?! { ... }                                     — forbidden
    auto err_prop = make_unary("?!", make_id_expr("x"));
    auto val_decl = make_val("a", std::move(err_prop));
    auto func = make_function_with_return_type(
        "f", make_result_type("string", "Error"), {val_decl});
    auto forbidden_op = make_operator_func("?!");
    std::vector<ast::expression> exprs = {func, forbidden_op};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error_propagations_checked, 1u);
    EXPECT_EQ(result.operator_defs_checked, 1u);
    EXPECT_EQ(result.errors_emitted, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4702");
}

// ===========================================================================
// Non-?! unary operators are ignored
// ===========================================================================

TEST_F(ErrorPropagationPassTest, NonQuestionBangUnaryIgnored) {
    // unary ! on an expression — should not be counted as error-propagation
    auto unary_not = make_unary("!", make_id_expr("flag"));
    std::vector<ast::expression> exprs = {unary_not};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.error_propagations_checked, 0u);
    EXPECT_EQ(result.errors_emitted, 0u);
}

// ===========================================================================
// ?! in optional-returning function emits E4706
// ===========================================================================

TEST_F(ErrorPropagationPassTest, ErrorPropagationInNullableReturnFunctionEmitsE4706) {
    // fnc f() -> string? { val a = x?! }
    auto err_prop = make_unary("?!", make_id_expr("x"));
    auto val_decl = make_val("a", std::move(err_prop));
    auto func = make_function_with_return_type(
        "f", make_type("string", true), {val_decl});
    std::vector<ast::expression> exprs = {func};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error_propagations_checked, 1u);
    EXPECT_EQ(result.errors_emitted, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4706");
}
