/// @file test_safe_chaining_pass.cpp
/// @brief Unit tests for the Safe-Chaining Operator Pass
///
/// Tests that the pass correctly:
///   - Detects `?.` safe-navigation on optional values
///   - Detects chained `?.` expressions (e.g., user?.address?.zip)
///   - Detects `?[]` safe-indexing expressions
///   - Detects `?()` safe-invocation expressions
///   - Rejects `?.` on Result[T, E] types (E4701)
///   - Rejects `opr ?.` definitions (E4702)
///   - Rejects `opr ?[]` definitions (E4702)
///   - Rejects `opr ?()` definitions (E4702)
///   - Verifies short-circuit semantics (expression-level, not function return)
///   - Handles mixed chains with `?.` and `?[]`
///
/// Requirements: 14B.7, 14B.8, 14B.9, 14B.10, 14B.11, 24C.19

#include <gtest/gtest.h>
#include "meld/compiler/safe_chaining_pass.hpp"

using namespace meld::compiler;
namespace ast = meld::parser::ast;

// ===========================================================================
// Helpers: build AST nodes for testing
// ===========================================================================

/// Create an identifier node.
static ast::identifier make_id(const std::string& name) {
    ast::identifier id;
    id.name = name;
    return id;
}

/// Create an identifier expression.
static ast::expression make_id_expr(const std::string& name) {
    return ast::expression(make_id(name));
}

/// Create a type_annotation with the given type name.
static ast::type_annotation make_type(const std::string& name, bool nullable = false) {
    ast::type_annotation type;
    type.type_name = make_id(name);
    type.is_nullable = nullable;
    return type;
}

/// Create an optional type annotation: optional[T].
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

/// Create a Result[T, E] type annotation.
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

/// Create a binary_operation expression node.
static ast::expression make_binop(
    const std::string& op,
    ast::expression left,
    ast::expression right
) {
    ast::binary_operation binop;
    binop.op = op;
    binop.left = boost::spirit::x3::forward_ast<ast::expression>(std::move(left));
    binop.right = boost::spirit::x3::forward_ast<ast::expression>(std::move(right));
    return ast::expression(
        boost::spirit::x3::forward_ast<ast::binary_operation>(std::move(binop)));
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
    op_def.op_type = ast::CustomOperatorType::INFIX;
    ast::block_expression body;
    op_def.body = boost::spirit::x3::forward_ast<ast::block_expression>(
        std::move(body));
    return ast::expression(
        boost::spirit::x3::forward_ast<ast::custom_operator_definition>(
            std::move(op_def)));
}

/// Create a function_definition with a body containing the given statements.
static ast::expression make_function_with_body(
    const std::string& name,
    std::vector<ast::expression> body_stmts
) {
    ast::function_definition func;
    func.name = make_id(name);
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

/// Create a val_declaration wrapping an expression.
static ast::expression make_val(const std::string& name, ast::expression value) {
    ast::val_declaration decl;
    decl.name = make_id(name);
    decl.value = boost::spirit::x3::forward_ast<ast::expression>(std::move(value));
    return ast::expression(
        boost::spirit::x3::forward_ast<ast::val_declaration>(std::move(decl)));
}

// ===========================================================================
// Test fixture
// ===========================================================================

class SafeChainingPassTest : public ::testing::Test {
protected:
    SafeChainingPass pass;
};

// ===========================================================================
// Empty / no-op cases
// ===========================================================================

TEST_F(SafeChainingPassTest, EmptyProgramNoDiagnostics) {
    std::vector<ast::expression> exprs;
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.safe_chains_checked, 0u);
    EXPECT_EQ(result.operator_defs_checked, 0u);
    EXPECT_EQ(result.errors_emitted, 0u);
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST_F(SafeChainingPassTest, ProgramWithNoSafeChainingNoDiagnostics) {
    // val x = foo
    std::vector<ast::expression> exprs = {make_id_expr("foo")};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.safe_chains_checked, 0u);
    EXPECT_TRUE(result.diagnostics.empty());
}

// ===========================================================================
// Basic ?. safe-navigation detection
// ===========================================================================

TEST_F(SafeChainingPassTest, BasicSafeNavigationDetected) {
    // nickname?.length
    auto expr = make_safe_nav(make_id_expr("nickname"), "length");
    std::vector<ast::expression> exprs = {expr};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.safe_chains_checked, 1u);
    EXPECT_EQ(result.errors_emitted, 0u);
}

TEST_F(SafeChainingPassTest, ChainedSafeNavigationDetected) {
    // user?.address?.zip — two chained ?. operators
    auto inner = make_safe_nav(make_id_expr("user"), "address");
    auto outer = make_safe_nav(std::move(inner), "zip");
    std::vector<ast::expression> exprs = {outer};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.safe_chains_checked, 2u);
}

TEST_F(SafeChainingPassTest, TripleChainedSafeNavigation) {
    // a?.b?.c?.d — three chained ?. operators
    auto chain1 = make_safe_nav(make_id_expr("a"), "b");
    auto chain2 = make_safe_nav(std::move(chain1), "c");
    auto chain3 = make_safe_nav(std::move(chain2), "d");
    std::vector<ast::expression> exprs = {chain3};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.safe_chains_checked, 3u);
}

// ===========================================================================
// ?[] safe-indexing detection
// ===========================================================================

TEST_F(SafeChainingPassTest, SafeIndexingDetected) {
    // users?[0] — represented as binary_operation with op "?[]"
    auto expr = make_binop("?[]", make_id_expr("users"), make_id_expr("0"));
    std::vector<ast::expression> exprs = {expr};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.safe_chains_checked, 1u);
}

// ===========================================================================
// ?() safe-invocation detection
// ===========================================================================

TEST_F(SafeChainingPassTest, SafeInvocationDetected) {
    // callback?() — represented as binary_operation with op "?()"
    auto expr = make_binop("?()", make_id_expr("callback"), make_id_expr("unit"));
    std::vector<ast::expression> exprs = {expr};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.safe_chains_checked, 1u);
}

// ===========================================================================
// Mixed chains: ?. and ?[]
// ===========================================================================

TEST_F(SafeChainingPassTest, MixedSafeNavAndIndexing) {
    // users?[0]?.name — safe indexing followed by safe navigation
    auto safe_index = make_binop("?[]", make_id_expr("users"), make_id_expr("0"));
    auto safe_nav = make_safe_nav(std::move(safe_index), "name");
    std::vector<ast::expression> exprs = {safe_nav};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    // 1 for ?[] binop + 1 for ?. safe_nav
    EXPECT_EQ(result.safe_chains_checked, 2u);
}

// ===========================================================================
// Safe-chaining inside function bodies
// ===========================================================================

TEST_F(SafeChainingPassTest, SafeNavigationInsideFunctionBody) {
    // fnc get_zip() { user?.address?.zip }
    auto inner = make_safe_nav(make_id_expr("user"), "address");
    auto outer = make_safe_nav(std::move(inner), "zip");
    auto func = make_function_with_body("get_zip", {outer});
    std::vector<ast::expression> exprs = {func};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.safe_chains_checked, 2u);
}

// ===========================================================================
// Safe-chaining inside val declarations
// ===========================================================================

TEST_F(SafeChainingPassTest, SafeNavigationInsideValDeclaration) {
    // val length = nickname?.length
    auto safe_nav = make_safe_nav(make_id_expr("nickname"), "length");
    auto val_decl = make_val("length", std::move(safe_nav));
    std::vector<ast::expression> exprs = {val_decl};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.safe_chains_checked, 1u);
}

// ===========================================================================
// E4702: Rejection of opr ?. definitions
// ===========================================================================

TEST_F(SafeChainingPassTest, RejectOperatorFunctionSafeNav) {
    // opr ?. { ... } — forbidden
    auto op = make_operator_func("?.");
    std::vector<ast::expression> exprs = {op};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.operator_defs_checked, 1u);
    EXPECT_EQ(result.errors_emitted, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4702");
    EXPECT_EQ(result.diagnostics[0].level, SafeChainingDiagnostic::Level::Error);
    EXPECT_NE(result.diagnostics[0].message.find("?."), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("non-overloadable"),
              std::string::npos);
}

TEST_F(SafeChainingPassTest, RejectOperatorFunctionSafeIndex) {
    // opr ?[] { ... } — forbidden
    auto op = make_operator_func("?[]");
    std::vector<ast::expression> exprs = {op};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errors_emitted, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4702");
    EXPECT_NE(result.diagnostics[0].message.find("?[]"), std::string::npos);
}

TEST_F(SafeChainingPassTest, RejectOperatorFunctionSafeInvoke) {
    // opr ?() { ... } — forbidden
    auto op = make_operator_func("?()");
    std::vector<ast::expression> exprs = {op};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errors_emitted, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4702");
    EXPECT_NE(result.diagnostics[0].message.find("?()"), std::string::npos);
}

TEST_F(SafeChainingPassTest, RejectCustomOperatorSafeNav) {
    // custom opr ?. { ... } — also forbidden
    auto op = make_custom_operator("?.");
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

TEST_F(SafeChainingPassTest, AllowOperatorFunctionForNonForbiddenSymbol) {
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
// E4701: Type checking — reject Result[T, E] operands
// ===========================================================================

TEST_F(SafeChainingPassTest, IsOptionalTypeNullable) {
    auto type = make_type("string", true);
    EXPECT_TRUE(SafeChainingPass::is_optional_type(type));
}

TEST_F(SafeChainingPassTest, IsOptionalTypeExplicit) {
    auto type = make_optional_type("string");
    EXPECT_TRUE(SafeChainingPass::is_optional_type(type));
}

TEST_F(SafeChainingPassTest, IsResultType) {
    auto type = make_result_type("Config", "IOError");
    EXPECT_TRUE(SafeChainingPass::is_result_type(type));
}

TEST_F(SafeChainingPassTest, NonOptionalNonResultType) {
    auto type = make_type("string", false);
    EXPECT_FALSE(SafeChainingPass::is_optional_type(type));
    EXPECT_FALSE(SafeChainingPass::is_result_type(type));
}

TEST_F(SafeChainingPassTest, EmitNonOptionalErrorDirectly) {
    // Directly test the error emitter for E4701
    SafeChainingResult result;
    pass.emit_non_optional_error("?.", "Result[Config, IOError]",
                                  "test.meld", 10, 5, result);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errors_emitted, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4701");
    EXPECT_EQ(result.diagnostics[0].level, SafeChainingDiagnostic::Level::Error);
    EXPECT_NE(result.diagnostics[0].message.find("?."), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("Result[Config, IOError]"),
              std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("?!"), std::string::npos);
    EXPECT_EQ(result.diagnostics[0].source_file, "test.meld");
    EXPECT_EQ(result.diagnostics[0].line, 10u);
    EXPECT_EQ(result.diagnostics[0].column, 5u);
}

// ===========================================================================
// Short-circuit semantics verification
// ===========================================================================

TEST_F(SafeChainingPassTest, ShortCircuitOnOptionalType) {
    // When left type is optional, chain CAN short-circuit to nil
    auto type = make_optional_type("Address");
    ast::safe_navigation_expression safe_nav;
    safe_nav.nullable_expr = boost::spirit::x3::forward_ast<ast::expression>(
        make_id_expr("user"));
    safe_nav.field_name = "address";

    auto eval = SafeChainingPass::evaluate_chain(safe_nav, type);
    EXPECT_TRUE(eval.short_circuits);
    EXPECT_TRUE(eval.is_optional_result);
    EXPECT_EQ(eval.result_type_name, "optional");
}

TEST_F(SafeChainingPassTest, NoShortCircuitOnNonOptionalType) {
    // When left type is NOT optional, chain does NOT short-circuit
    auto type = make_type("User", false);
    ast::safe_navigation_expression safe_nav;
    safe_nav.nullable_expr = boost::spirit::x3::forward_ast<ast::expression>(
        make_id_expr("user"));
    safe_nav.field_name = "address";

    auto eval = SafeChainingPass::evaluate_chain(safe_nav, type);
    EXPECT_FALSE(eval.short_circuits);
    EXPECT_FALSE(eval.is_optional_result);
    EXPECT_EQ(eval.result_type_name, "User");
}

TEST_F(SafeChainingPassTest, ShortCircuitOnNullableType) {
    // string? (is_nullable = true) should also short-circuit
    auto type = make_type("string", true);
    ast::safe_navigation_expression safe_nav;
    safe_nav.nullable_expr = boost::spirit::x3::forward_ast<ast::expression>(
        make_id_expr("nickname"));
    safe_nav.field_name = "length";

    auto eval = SafeChainingPass::evaluate_chain(safe_nav, type);
    EXPECT_TRUE(eval.short_circuits);
    EXPECT_TRUE(eval.is_optional_result);
}

// ===========================================================================
// Forbidden operator symbols set
// ===========================================================================

TEST_F(SafeChainingPassTest, ForbiddenSymbolsContainAllThree) {
    const auto& symbols = SafeChainingPass::forbidden_operator_symbols();
    EXPECT_TRUE(symbols.count("?."));
    EXPECT_TRUE(symbols.count("?[]"));
    EXPECT_TRUE(symbols.count("?()"));
    EXPECT_EQ(symbols.size(), 3u);
}

TEST_F(SafeChainingPassTest, ForbiddenSymbolsDoNotContainRegularOps) {
    const auto& symbols = SafeChainingPass::forbidden_operator_symbols();
    EXPECT_FALSE(symbols.count("+"));
    EXPECT_FALSE(symbols.count("-"));
    EXPECT_FALSE(symbols.count("*"));
    EXPECT_FALSE(symbols.count("[]"));
}

// ===========================================================================
// Multiple forbidden operator definitions in one program
// ===========================================================================

TEST_F(SafeChainingPassTest, MultipleForbidenOperatorsAllRejected) {
    // opr ?. { ... }
    // opr ?[] { ... }
    // opr ?() { ... }
    auto op1 = make_operator_func("?.");
    auto op2 = make_operator_func("?[]");
    auto op3 = make_operator_func("?()");
    std::vector<ast::expression> exprs = {op1, op2, op3};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.operator_defs_checked, 3u);
    EXPECT_EQ(result.errors_emitted, 3u);
    ASSERT_EQ(result.diagnostics.size(), 3u);
    for (const auto& diag : result.diagnostics) {
        EXPECT_EQ(diag.code, "E4702");
    }
}

// ===========================================================================
// Source file propagation
// ===========================================================================

TEST_F(SafeChainingPassTest, SourceFileInDiagnostics) {
    auto op = make_operator_func("?.");
    std::vector<ast::expression> exprs = {op};
    auto result = pass.run(exprs, "src/quintet.meld");
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].source_file, "src/quintet.meld");
}

// ===========================================================================
// Mixed valid and invalid in one program
// ===========================================================================

TEST_F(SafeChainingPassTest, MixedValidUsageAndForbiddenOverload) {
    // val length = nickname?.length   — valid usage
    // opr ?. { ... }                  — forbidden
    auto safe_nav = make_safe_nav(make_id_expr("nickname"), "length");
    auto val_decl = make_val("length", std::move(safe_nav));
    auto forbidden_op = make_operator_func("?.");
    std::vector<ast::expression> exprs = {val_decl, forbidden_op};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.safe_chains_checked, 1u);
    EXPECT_EQ(result.operator_defs_checked, 1u);
    EXPECT_EQ(result.errors_emitted, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4702");
}
