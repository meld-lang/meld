/// @file test_param_mutability_checker_pass.cpp
/// @brief Unit tests for the Parameter Mutability Checker Pass
///
/// Tests that the pass correctly:
///   - Detects direct assignment to immutable parameters (E5002)
///   - Detects field assignment on immutable parameters (E5002)
///   - Allows mutation of `var` parameters (no diagnostics)
///   - Warns on unnecessary `var` declarations (W5002)
///   - Handles top-level functions, class methods, and struct methods
///   - Handles multiple parameters with mixed mutability
///
/// Requirements: 57.5, 57.6, 57.7

#include <gtest/gtest.h>
#include "meld/compiler/param_mutability_checker_pass.hpp"

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

/// Create an identifier expression.
static ast::expression make_id_expr(const std::string& name) {
    return ast::expression(make_id(name));
}

/// Create a `target.field` dot-access expression.
static ast::expression make_dot_access(const std::string& target, const std::string& field) {
    return make_binop(".", make_id_expr(target), make_id_expr(field));
}

/// Create a `target = value` direct assignment expression.
static ast::expression make_direct_assign(
    const std::string& target,
    ast::expression value
) {
    return make_binop("=", make_id_expr(target), std::move(value));
}

/// Create a `target.field = value` field assignment expression.
static ast::expression make_field_assign(
    const std::string& target,
    const std::string& field,
    ast::expression value
) {
    return make_binop("=", make_dot_access(target, field), std::move(value));
}

/// Create a function_parameter.
static ast::function_parameter make_param(
    const std::string& name,
    bool is_mutable = false
) {
    ast::function_parameter param;
    param.name = make_id(name);
    param.is_mutable = is_mutable;
    return param;
}

/// Create a function_definition with parameters and body statements.
static ast::function_definition make_function(
    const std::string& name,
    std::vector<ast::function_parameter> params,
    std::vector<ast::expression> body_stmts
) {
    ast::function_definition func;
    func.name = make_id(name);
    func.parameters = std::move(params);
    ast::block_expression body;
    for (auto& stmt : body_stmts) {
        body.statements.push_back(
            boost::spirit::x3::forward_ast<ast::expression>(std::move(stmt)));
    }
    func.body = boost::spirit::x3::forward_ast<ast::block_expression>(std::move(body));
    return func;
}

/// Wrap a function_definition as a top-level expression.
static ast::expression make_func_expr(ast::function_definition func) {
    return ast::expression(
        boost::spirit::x3::forward_ast<ast::function_definition>(std::move(func)));
}

/// Create a class_definition expression with the given methods.
static ast::expression make_class_with_methods(
    const std::string& class_name,
    std::vector<ast::function_definition> methods
) {
    ast::class_definition cls;
    cls.name = make_id(class_name);
    cls.methods = std::move(methods);
    ast::block_expression body;
    cls.body = boost::spirit::x3::forward_ast<ast::block_expression>(std::move(body));
    return ast::expression(
        boost::spirit::x3::forward_ast<ast::class_definition>(std::move(cls)));
}

/// Create a struct_definition expression with the given methods.
static ast::expression make_struct_with_methods(
    const std::string& struct_name,
    std::vector<ast::function_definition> methods
) {
    ast::struct_definition s;
    s.name = make_id(struct_name);
    s.methods = std::move(methods);
    return ast::expression(
        boost::spirit::x3::forward_ast<ast::struct_definition>(std::move(s)));
}

// ===========================================================================
// Static helper tests
// ===========================================================================

TEST(GetDirectAssignTargetTest, DetectsDirectAssignment) {
    // x = 5
    auto assign_expr = make_direct_assign("x", make_id_expr("5"));
    const auto& binop = boost::get<
        boost::spirit::x3::forward_ast<ast::binary_operation>>(assign_expr).get();
    EXPECT_EQ(ParamMutabilityCheckerPass::get_direct_assign_target(binop), "x");
}

TEST(GetDirectAssignTargetTest, RejectsNonAssignment) {
    // x + 5
    auto add_expr = make_binop("+", make_id_expr("x"), make_id_expr("5"));
    const auto& binop = boost::get<
        boost::spirit::x3::forward_ast<ast::binary_operation>>(add_expr).get();
    EXPECT_EQ(ParamMutabilityCheckerPass::get_direct_assign_target(binop), "");
}

TEST(GetDirectAssignTargetTest, RejectsDotAccessAssignment) {
    // x.field = 5 (this is a field assign, not direct)
    auto assign_expr = make_field_assign("x", "field", make_id_expr("5"));
    const auto& binop = boost::get<
        boost::spirit::x3::forward_ast<ast::binary_operation>>(assign_expr).get();
    EXPECT_EQ(ParamMutabilityCheckerPass::get_direct_assign_target(binop), "");
}

TEST(GetFieldAssignTargetTest, DetectsFieldAssignment) {
    // user.name = "Alice"
    auto assign_expr = make_field_assign("user", "name", make_id_expr("Alice"));
    const auto& binop = boost::get<
        boost::spirit::x3::forward_ast<ast::binary_operation>>(assign_expr).get();
    EXPECT_EQ(ParamMutabilityCheckerPass::get_field_assign_target(binop), "user");
}

TEST(GetFieldAssignTargetTest, RejectsThisFieldAssignment) {
    // this.name = "Alice" — handled by MutabilityCheckerPass, not us
    auto assign_expr = make_field_assign("this", "name", make_id_expr("Alice"));
    const auto& binop = boost::get<
        boost::spirit::x3::forward_ast<ast::binary_operation>>(assign_expr).get();
    EXPECT_EQ(ParamMutabilityCheckerPass::get_field_assign_target(binop), "");
}

TEST(GetFieldAssignTargetTest, RejectsNonAssignment) {
    // user.name (dot access, not assignment)
    auto dot_expr = make_dot_access("user", "name");
    // Wrap in a non-assignment binop to test
    auto add_expr = make_binop("+", std::move(dot_expr), make_id_expr("5"));
    const auto& binop = boost::get<
        boost::spirit::x3::forward_ast<ast::binary_operation>>(add_expr).get();
    EXPECT_EQ(ParamMutabilityCheckerPass::get_field_assign_target(binop), "");
}

// ===========================================================================
// Integration tests — ParamMutabilityCheckerPass::run()
// ===========================================================================

class ParamMutabilityCheckerPassTest : public ::testing::Test {
protected:
    ParamMutabilityCheckerPass pass;
};

// --- Test 1: Empty program — no diagnostics ---

TEST_F(ParamMutabilityCheckerPassTest, EmptyProgramNoDiagnostics) {
    std::vector<ast::expression> exprs;
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.functions_checked, 0u);
    EXPECT_EQ(result.param_mutations_detected, 0u);
    EXPECT_EQ(result.errors_emitted, 0u);
    EXPECT_EQ(result.warnings_emitted, 0u);
    EXPECT_TRUE(result.diagnostics.empty());
}

// --- Test 2: Function with no parameters — no diagnostics ---

TEST_F(ParamMutabilityCheckerPassTest, FunctionWithNoParamsNoDiagnostics) {
    // fnc greet() { rtn "hello" }
    auto func = make_function("greet", {}, {make_id_expr("hello")});
    std::vector<ast::expression> exprs = {make_func_expr(std::move(func))};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.functions_checked, 1u);
    EXPECT_EQ(result.param_mutations_detected, 0u);
    EXPECT_TRUE(result.diagnostics.empty());
}

// --- Test 3: Immutable param, no mutation — OK ---

TEST_F(ParamMutabilityCheckerPassTest, ImmutableParamNoMutationOK) {
    // fnc read(user: User) { rtn user.name }
    auto func = make_function("read",
        {make_param("user")},
        {make_dot_access("user", "name")});
    std::vector<ast::expression> exprs = {make_func_expr(std::move(func))};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.functions_checked, 1u);
    EXPECT_EQ(result.param_mutations_detected, 0u);
    EXPECT_TRUE(result.diagnostics.empty());
}

// --- Test 4: Immutable param, direct assignment — E5002 ---

TEST_F(ParamMutabilityCheckerPassTest, ImmutableParamDirectAssignEmitsE5002) {
    // fnc update(user: User) { user = new_user }
    auto func = make_function("update",
        {make_param("user")},
        {make_direct_assign("user", make_id_expr("new_user"))});
    std::vector<ast::expression> exprs = {make_func_expr(std::move(func))};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.functions_checked, 1u);
    EXPECT_EQ(result.param_mutations_detected, 1u);
    EXPECT_EQ(result.errors_emitted, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E5002");
    EXPECT_EQ(result.diagnostics[0].level, ParamMutabilityDiagnostic::Level::Error);
    EXPECT_NE(result.diagnostics[0].message.find("user"), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("update"), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("var"), std::string::npos);
    EXPECT_EQ(result.diagnostics[0].source_file, "test.meld");
}

// --- Test 5: Immutable param, field assignment — E5002 ---

TEST_F(ParamMutabilityCheckerPassTest, ImmutableParamFieldAssignEmitsE5002) {
    // fnc update(user: User) { user.name = "Alice" }
    auto func = make_function("update",
        {make_param("user")},
        {make_field_assign("user", "name", make_id_expr("Alice"))});
    std::vector<ast::expression> exprs = {make_func_expr(std::move(func))};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.param_mutations_detected, 1u);
    EXPECT_EQ(result.errors_emitted, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E5002");
    EXPECT_NE(result.diagnostics[0].message.find("user"), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("update"), std::string::npos);
}

// --- Test 6: var param with mutation — OK (no diagnostics) ---

TEST_F(ParamMutabilityCheckerPassTest, VarParamWithMutationNoDiagnostics) {
    // fnc update(u: var User) { u.name = "Alice" }
    auto func = make_function("update",
        {make_param("u", /*is_mutable=*/true)},
        {make_field_assign("u", "name", make_id_expr("Alice"))});
    std::vector<ast::expression> exprs = {make_func_expr(std::move(func))};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.functions_checked, 1u);
    EXPECT_EQ(result.param_mutations_detected, 1u);
    EXPECT_EQ(result.errors_emitted, 0u);
    EXPECT_EQ(result.warnings_emitted, 0u);
    EXPECT_TRUE(result.diagnostics.empty());
}

// --- Test 7: var param, no mutation — W5002 ---

TEST_F(ParamMutabilityCheckerPassTest, VarParamNoMutationEmitsW5002) {
    // fnc read(u: var User) { rtn u.name }
    auto func = make_function("read",
        {make_param("u", /*is_mutable=*/true)},
        {make_dot_access("u", "name")});
    std::vector<ast::expression> exprs = {make_func_expr(std::move(func))};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_TRUE(result.success);  // Warnings don't block compilation
    EXPECT_EQ(result.functions_checked, 1u);
    EXPECT_EQ(result.param_mutations_detected, 0u);
    EXPECT_EQ(result.warnings_emitted, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "W5002");
    EXPECT_EQ(result.diagnostics[0].level, ParamMutabilityDiagnostic::Level::Warning);
    EXPECT_NE(result.diagnostics[0].message.find("u"), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("read"), std::string::npos);
}

// --- Test 8: Multiple parameters, mixed mutability ---

TEST_F(ParamMutabilityCheckerPassTest, MultipleParamsMixedMutability) {
    // fnc transfer(src: var Account, dst: Account, amount: Int) {
    //   src.balance = src.balance - amount
    //   dst.balance = dst.balance + amount   // E5002: dst not var
    // }
    auto func = make_function("transfer",
        {make_param("src", /*is_mutable=*/true),
         make_param("dst", /*is_mutable=*/false),
         make_param("amount", /*is_mutable=*/false)},
        {make_field_assign("src", "balance",
            make_binop("-", make_dot_access("src", "balance"), make_id_expr("amount"))),
         make_field_assign("dst", "balance",
            make_binop("+", make_dot_access("dst", "balance"), make_id_expr("amount")))});
    std::vector<ast::expression> exprs = {make_func_expr(std::move(func))};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.functions_checked, 1u);
    EXPECT_EQ(result.param_mutations_detected, 2u);  // src and dst both mutated
    EXPECT_EQ(result.errors_emitted, 1u);              // only dst is an error
    EXPECT_EQ(result.warnings_emitted, 0u);

    // Verify E5002 is for dst, not src
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E5002");
    EXPECT_NE(result.diagnostics[0].message.find("dst"), std::string::npos);
}

// --- Test 9: Methods inside classes — parameters checked correctly ---

TEST_F(ParamMutabilityCheckerPassTest, ClassMethodParamsChecked) {
    // class UserService {
    //   fnc update(user: User) { user.name = "Bob" }  // E5002
    // }
    auto method = make_function("update",
        {make_param("user")},
        {make_field_assign("user", "name", make_id_expr("Bob"))});
    auto cls = make_class_with_methods("UserService", {method});
    std::vector<ast::expression> exprs = {cls};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.functions_checked, 1u);
    EXPECT_EQ(result.errors_emitted, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E5002");
    EXPECT_NE(result.diagnostics[0].message.find("user"), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("update"), std::string::npos);
}

// --- Test 9b: Methods inside structs — parameters checked correctly ---

TEST_F(ParamMutabilityCheckerPassTest, StructMethodParamsChecked) {
    // struct Processor {
    //   fnc process(data: var Data) { data.value = 0 }  // OK
    // }
    auto method = make_function("process",
        {make_param("data", /*is_mutable=*/true)},
        {make_field_assign("data", "value", make_id_expr("0"))});
    auto s = make_struct_with_methods("Processor", {method});
    std::vector<ast::expression> exprs = {s};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.functions_checked, 1u);
    EXPECT_EQ(result.param_mutations_detected, 1u);
    EXPECT_TRUE(result.diagnostics.empty());
}

// --- Test 10: Top-level function with var param direct assignment ---

TEST_F(ParamMutabilityCheckerPassTest, TopLevelVarParamDirectAssignOK) {
    // fnc reset(counter: var Int) { counter = 0 }
    auto func = make_function("reset",
        {make_param("counter", /*is_mutable=*/true)},
        {make_direct_assign("counter", make_id_expr("0"))});
    std::vector<ast::expression> exprs = {make_func_expr(std::move(func))};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.param_mutations_detected, 1u);
    EXPECT_TRUE(result.diagnostics.empty());
}

// --- Multiple functions in one program ---

TEST_F(ParamMutabilityCheckerPassTest, MultipleFunctionsAnalyzedIndependently) {
    // fnc good(x: var Int) { x = 1 }       — OK
    // fnc bad(y: Int) { y = 2 }             — E5002
    auto good = make_function("good",
        {make_param("x", /*is_mutable=*/true)},
        {make_direct_assign("x", make_id_expr("1"))});
    auto bad = make_function("bad",
        {make_param("y")},
        {make_direct_assign("y", make_id_expr("2"))});

    std::vector<ast::expression> exprs = {
        make_func_expr(std::move(good)),
        make_func_expr(std::move(bad))
    };
    auto result = pass.run(exprs, "test.meld");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.functions_checked, 2u);
    EXPECT_EQ(result.errors_emitted, 1u);
    ASSERT_GE(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E5002");
    EXPECT_NE(result.diagnostics[0].message.find("y"), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("bad"), std::string::npos);
}

// --- Source file propagation ---

TEST_F(ParamMutabilityCheckerPassTest, SourceFileInDiagnostics) {
    auto func = make_function("update",
        {make_param("x")},
        {make_direct_assign("x", make_id_expr("1"))});
    std::vector<ast::expression> exprs = {make_func_expr(std::move(func))};
    auto result = pass.run(exprs, "src/service.meld");

    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].source_file, "src/service.meld");
}

// --- Non-parameter variable assignment should not trigger ---

TEST_F(ParamMutabilityCheckerPassTest, LocalVarAssignDoesNotTrigger) {
    // fnc compute(x: Int) { y = x + 1 }
    // y is not a parameter, so no diagnostic
    auto func = make_function("compute",
        {make_param("x")},
        {make_direct_assign("y", make_binop("+", make_id_expr("x"), make_id_expr("1")))});
    std::vector<ast::expression> exprs = {make_func_expr(std::move(func))};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.param_mutations_detected, 0u);
    EXPECT_TRUE(result.diagnostics.empty());
}

// --- Both E5002 and W5002 in same function ---

TEST_F(ParamMutabilityCheckerPassTest, MixedErrorAndWarningInSameFunction) {
    // fnc process(a: Int, b: var Int) { a = 1 }
    // a is mutated without var → E5002
    // b is var but not mutated → W5002
    auto func = make_function("process",
        {make_param("a"), make_param("b", /*is_mutable=*/true)},
        {make_direct_assign("a", make_id_expr("1"))});
    std::vector<ast::expression> exprs = {make_func_expr(std::move(func))};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errors_emitted, 1u);
    EXPECT_EQ(result.warnings_emitted, 1u);
    ASSERT_EQ(result.diagnostics.size(), 2u);

    bool found_e5002_a = false;
    bool found_w5002_b = false;
    for (const auto& diag : result.diagnostics) {
        if (diag.code == "E5002" && diag.message.find("'a'") != std::string::npos) {
            found_e5002_a = true;
        }
        if (diag.code == "W5002" && diag.message.find("'b'") != std::string::npos) {
            found_w5002_b = true;
        }
    }
    EXPECT_TRUE(found_e5002_a);
    EXPECT_TRUE(found_w5002_b);
}
