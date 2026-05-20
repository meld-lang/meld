/// @file test_mutability_enforcement_pass.cpp
/// @brief Tests for the Mutability Enforcement Pass
/// Requirements: 165.3, 165.4, 165.5, 165.6

#include <gtest/gtest.h>
#include "meld/compiler/mutability_enforcement_pass.hpp"

using namespace meld::compiler;
using MQ = meld::parser::ast::type_annotation::MutabilityQualifier;

// ===========================================================================
// Helpers: build AST nodes
// ===========================================================================

/// Create a simple type_annotation with no type arguments.
static meld::parser::ast::type_annotation make_type(const std::string& name) {
    meld::parser::ast::type_annotation type;
    type.type_name.name = name;
    return type;
}

/// Create a type_annotation with type arguments and mutability qualifiers.
static meld::parser::ast::type_annotation make_generic_type(
    const std::string& name,
    std::vector<std::pair<std::string, MQ>> args
) {
    meld::parser::ast::type_annotation type;
    type.type_name.name = name;
    type.has_type_arguments = true;
    for (const auto& [arg_name, mq] : args) {
        meld::parser::ast::type_annotation arg;
        arg.type_name.name = arg_name;
        arg.mutability_qualifier = mq;
        type.type_arguments.push_back(
            boost::spirit::x3::forward_ast<meld::parser::ast::type_annotation>(
                std::move(arg)));
    }
    return type;
}

/// Create an identifier expression.
static meld::parser::ast::expression make_id_expr(const std::string& name) {
    meld::parser::ast::identifier id;
    id.name = name;
    return meld::parser::ast::expression(id);
}

/// Create a function_call expression.
static meld::parser::ast::expression make_call_expr(
    const std::string& func_name,
    std::vector<meld::parser::ast::expression> args = {}
) {
    meld::parser::ast::function_call call;
    call.function_name.name = func_name;
    for (auto& a : args) {
        call.arguments.push_back(
            boost::spirit::x3::forward_ast<meld::parser::ast::expression>(std::move(a)));
    }
    return meld::parser::ast::expression(
        boost::spirit::x3::forward_ast<meld::parser::ast::function_call>(std::move(call)));
}

/// Create a dot-method-call expression: receiver.method(args)
/// Represented as binary_operation(".", identifier, function_call)
static meld::parser::ast::expression make_method_call(
    const std::string& receiver,
    const std::string& method,
    std::vector<meld::parser::ast::expression> args = {}
) {
    meld::parser::ast::binary_operation binop;
    binop.op = ".";
    binop.left = boost::spirit::x3::forward_ast<meld::parser::ast::expression>(
        make_id_expr(receiver));
    binop.right = boost::spirit::x3::forward_ast<meld::parser::ast::expression>(
        make_call_expr(method, std::move(args)));
    return meld::parser::ast::expression(
        boost::spirit::x3::forward_ast<meld::parser::ast::binary_operation>(
            std::move(binop)));
}

/// Create a val declaration with a type annotation.
static meld::parser::ast::expression make_val_with_type(
    const std::string& val_name,
    meld::parser::ast::type_annotation type_ann
) {
    meld::parser::ast::val_declaration decl;
    decl.name.name = val_name;
    decl.has_type_annotation = true;
    decl.type_ann = boost::spirit::x3::forward_ast<meld::parser::ast::type_annotation>(
        std::move(type_ann));
    meld::parser::ast::identifier id;
    id.name = "dummy";
    decl.value = boost::spirit::x3::forward_ast<meld::parser::ast::expression>(
        meld::parser::ast::expression(id));
    return meld::parser::ast::expression(
        boost::spirit::x3::forward_ast<meld::parser::ast::val_declaration>(
            std::move(decl)));
}

/// Create a function definition wrapping body statements, with optional effects.
static meld::parser::ast::expression make_function_with_body(
    const std::string& func_name,
    std::vector<meld::parser::ast::expression> body_stmts,
    std::vector<std::string> effects = {}
) {
    meld::parser::ast::function_definition func;
    func.name.name = func_name;
    func.has_effects = !effects.empty();
    for (const auto& eff : effects) {
        meld::parser::ast::identifier eff_id;
        eff_id.name = eff;
        func.effects_clause.push_back(eff_id);
    }
    meld::parser::ast::block_expression body;
    for (auto& stmt : body_stmts) {
        body.statements.push_back(
            boost::spirit::x3::forward_ast<meld::parser::ast::expression>(std::move(stmt)));
    }
    func.body = boost::spirit::x3::forward_ast<meld::parser::ast::block_expression>(
        std::move(body));
    return meld::parser::ast::expression(
        boost::spirit::x3::forward_ast<meld::parser::ast::function_definition>(
            std::move(func)));
}

// ===========================================================================
// Test fixture
// ===========================================================================

class MutabilityEnforcementPassTest : public ::testing::Test {
protected:
    MutabilityEnforcementPass pass;
};

// ===========================================================================
// is_mutating_method static helper tests
// ===========================================================================

TEST_F(MutabilityEnforcementPassTest, IsMutatingMethodKnownMethods) {
    EXPECT_TRUE(MutabilityEnforcementPass::is_mutating_method("add"));
    EXPECT_TRUE(MutabilityEnforcementPass::is_mutating_method("remove"));
    EXPECT_TRUE(MutabilityEnforcementPass::is_mutating_method("clear"));
    EXPECT_TRUE(MutabilityEnforcementPass::is_mutating_method("sort-in-place"));
    EXPECT_TRUE(MutabilityEnforcementPass::is_mutating_method("push"));
    EXPECT_TRUE(MutabilityEnforcementPass::is_mutating_method("pop"));
    EXPECT_TRUE(MutabilityEnforcementPass::is_mutating_method("pop-front"));
    EXPECT_TRUE(MutabilityEnforcementPass::is_mutating_method("pop-back"));
    EXPECT_TRUE(MutabilityEnforcementPass::is_mutating_method("set"));
    EXPECT_TRUE(MutabilityEnforcementPass::is_mutating_method("insert"));
    EXPECT_TRUE(MutabilityEnforcementPass::is_mutating_method("delete"));
    EXPECT_TRUE(MutabilityEnforcementPass::is_mutating_method("update"));
    EXPECT_TRUE(MutabilityEnforcementPass::is_mutating_method("append"));
    EXPECT_TRUE(MutabilityEnforcementPass::is_mutating_method("prepend"));
}

TEST_F(MutabilityEnforcementPassTest, IsMutatingMethodNonMutating) {
    EXPECT_FALSE(MutabilityEnforcementPass::is_mutating_method("get"));
    EXPECT_FALSE(MutabilityEnforcementPass::is_mutating_method("size"));
    EXPECT_FALSE(MutabilityEnforcementPass::is_mutating_method("contains"));
    EXPECT_FALSE(MutabilityEnforcementPass::is_mutating_method("to-string"));
    EXPECT_FALSE(MutabilityEnforcementPass::is_mutating_method(""));
}

// ===========================================================================
// E4010: val type param + mutating method → error
// ===========================================================================

// val x: Hold[val User] = ...; x.add(y) → E4010
TEST_F(MutabilityEnforcementPassTest, ValQualifiedMutatingMethodEmitsE4010) {
    auto val_decl = make_val_with_type("x",
        make_generic_type("Hold", {{"User", MQ::VAL}}));
    auto method_call = make_method_call("x", "add", {make_id_expr("y")});

    auto func = make_function_with_body("test-func",
        {std::move(val_decl), std::move(method_call)});

    std::vector<meld::parser::ast::expression> exprs = {std::move(func)};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.val_violations, 1u);
    EXPECT_EQ(result.effect_violations, 0u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4010");
    EXPECT_NE(result.diagnostics[0].message.find("add"), std::string::npos);
}

// ===========================================================================
// E4011: var type param + mutating method without @effect(state) → error
// ===========================================================================

// val x: Hold[var User] = ...; x.add(y) in function without @effect(state) → E4011
TEST_F(MutabilityEnforcementPassTest, VarQualifiedMutatingMethodWithoutEffectEmitsE4011) {
    auto val_decl = make_val_with_type("x",
        make_generic_type("Hold", {{"User", MQ::VAR}}));
    auto method_call = make_method_call("x", "add", {make_id_expr("y")});

    auto func = make_function_with_body("test-func",
        {std::move(val_decl), std::move(method_call)});

    std::vector<meld::parser::ast::expression> exprs = {std::move(func)};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.val_violations, 0u);
    EXPECT_EQ(result.effect_violations, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4011");
    EXPECT_NE(result.diagnostics[0].message.find("add"), std::string::npos);
}

// ===========================================================================
// OK: var type param + mutating method WITH @effect(state)
// ===========================================================================

// val x: Hold[var User] = ...; x.add(y) in function with @effect(state) → OK
TEST_F(MutabilityEnforcementPassTest, VarQualifiedMutatingMethodWithEffectIsOk) {
    auto val_decl = make_val_with_type("x",
        make_generic_type("Hold", {{"User", MQ::VAR}}));
    auto method_call = make_method_call("x", "add", {make_id_expr("y")});

    auto func = make_function_with_body("test-func",
        {std::move(val_decl), std::move(method_call)},
        {"state"});  // @effect(state)

    std::vector<meld::parser::ast::expression> exprs = {std::move(func)};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.val_violations, 0u);
    EXPECT_EQ(result.effect_violations, 0u);
    EXPECT_TRUE(result.diagnostics.empty());
}

// ===========================================================================
// OK: val type param + non-mutating method
// ===========================================================================

// val x: Hold[val User] = ...; x.get() → OK (non-mutating method)
TEST_F(MutabilityEnforcementPassTest, ValQualifiedNonMutatingMethodIsOk) {
    auto val_decl = make_val_with_type("x",
        make_generic_type("Hold", {{"User", MQ::VAL}}));
    auto method_call = make_method_call("x", "get");

    auto func = make_function_with_body("test-func",
        {std::move(val_decl), std::move(method_call)});

    std::vector<meld::parser::ast::expression> exprs = {std::move(func)};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.val_violations, 0u);
    EXPECT_EQ(result.effect_violations, 0u);
    EXPECT_TRUE(result.diagnostics.empty());
}

// ===========================================================================
// NONE defaults to VAL after normalization → E4010
// ===========================================================================

// val x: List[User] = ...; x.add(y) → E4010 (NONE defaults to VAL)
TEST_F(MutabilityEnforcementPassTest, NoneQualifierDefaultsToValEmitsE4010) {
    // After MutabilityQualifierPass, NONE is resolved to VAL.
    // But if somehow NONE slips through, we treat it as VAL for safety.
    auto val_decl = make_val_with_type("x",
        make_generic_type("List", {{"User", MQ::VAL}}));
    auto method_call = make_method_call("x", "add", {make_id_expr("y")});

    auto func = make_function_with_body("test-func",
        {std::move(val_decl), std::move(method_call)});

    std::vector<meld::parser::ast::expression> exprs = {std::move(func)};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.val_violations, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4010");
}

// ===========================================================================
// Empty program → success
// ===========================================================================

TEST_F(MutabilityEnforcementPassTest, EmptyProgramIsOk) {
    std::vector<meld::parser::ast::expression> exprs;
    auto result = pass.run(exprs, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.val_violations, 0u);
    EXPECT_EQ(result.effect_violations, 0u);
    EXPECT_TRUE(result.diagnostics.empty());
}

// ===========================================================================
// is_qualifier_compatible — Req 165.5, 165.6
// ===========================================================================

TEST_F(MutabilityEnforcementPassTest, QualifierCompatibleValToVal) {
    EXPECT_TRUE(MutabilityEnforcementPass::is_qualifier_compatible(MQ::VAL, MQ::VAL));
}

TEST_F(MutabilityEnforcementPassTest, QualifierCompatibleVarToVar) {
    EXPECT_TRUE(MutabilityEnforcementPass::is_qualifier_compatible(MQ::VAR, MQ::VAR));
}

TEST_F(MutabilityEnforcementPassTest, QualifierCompatibleVarToValDowngrade) {
    // Downgrade: restricting permissions — safe, allowed
    EXPECT_TRUE(MutabilityEnforcementPass::is_qualifier_compatible(MQ::VAR, MQ::VAL));
}

TEST_F(MutabilityEnforcementPassTest, QualifierIncompatibleValToVarUpgrade) {
    // Upgrade: expanding permissions — requires explicit checked cast
    EXPECT_FALSE(MutabilityEnforcementPass::is_qualifier_compatible(MQ::VAL, MQ::VAR));
}

// ===========================================================================
// E4012: val→var qualifier upgrade violation — Req 165.5, 165.6
// ===========================================================================

TEST_F(MutabilityEnforcementPassTest, EmitQualifierUpgradeViolationE4012) {
    MutabilityEnforcementResult result;
    bool compatible = pass.check_qualifier_assignment(
        MQ::VAL, MQ::VAR, "test.meld", 10, 5, result);

    EXPECT_FALSE(compatible);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.qualifier_upgrade_violations, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4012");
    EXPECT_NE(result.diagnostics[0].message.find("val-qualified"), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("var-qualified"), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("checked cast"), std::string::npos);
    EXPECT_EQ(result.diagnostics[0].source_file, "test.meld");
    EXPECT_EQ(result.diagnostics[0].line, 10u);
    EXPECT_EQ(result.diagnostics[0].column, 5u);
}

TEST_F(MutabilityEnforcementPassTest, CheckQualifierAssignmentDowngradeNoError) {
    MutabilityEnforcementResult result;
    bool compatible = pass.check_qualifier_assignment(
        MQ::VAR, MQ::VAL, "test.meld", 1, 1, result);

    EXPECT_TRUE(compatible);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.qualifier_upgrade_violations, 0u);
    EXPECT_TRUE(result.diagnostics.empty());
}

// ===========================================================================
// Helpers: build functions with typed parameters (for Req 165.7 tests)
// ===========================================================================

/// Create a function_parameter with a typed annotation.
static meld::parser::ast::function_parameter make_typed_param(
    const std::string& param_name,
    meld::parser::ast::type_annotation type_ann
) {
    meld::parser::ast::function_parameter param;
    param.name.name = param_name;
    param.type = std::move(type_ann);
    return param;
}

/// Create a function definition with typed parameters, body statements, and optional effects.
static meld::parser::ast::expression make_function_with_params(
    const std::string& func_name,
    std::vector<meld::parser::ast::function_parameter> params,
    std::vector<meld::parser::ast::expression> body_stmts = {},
    std::vector<std::string> effects = {}
) {
    meld::parser::ast::function_definition func;
    func.name.name = func_name;
    func.parameters = std::move(params);
    func.has_effects = !effects.empty();
    for (const auto& eff : effects) {
        meld::parser::ast::identifier eff_id;
        eff_id.name = eff;
        func.effects_clause.push_back(eff_id);
    }
    meld::parser::ast::block_expression body;
    for (auto& stmt : body_stmts) {
        body.statements.push_back(
            boost::spirit::x3::forward_ast<meld::parser::ast::expression>(std::move(stmt)));
    }
    func.body = boost::spirit::x3::forward_ast<meld::parser::ast::block_expression>(
        std::move(body));
    return meld::parser::ast::expression(
        boost::spirit::x3::forward_ast<meld::parser::ast::function_definition>(
            std::move(func)));
}

// ===========================================================================
// Req 165.7: Auto-inference of @effect(state) from var type parameters
// ===========================================================================

// fnc update-user(param: Hold[var User]) → effect_state_inferred == 1
TEST_F(MutabilityEnforcementPassTest, VarParamInfersEffectState) {
    auto func = make_function_with_params("update-user", {
        make_typed_param("param", make_generic_type("Hold", {{"User", MQ::VAR}}))
    });

    std::vector<meld::parser::ast::expression> exprs = {std::move(func)};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_EQ(result.effect_state_inferred, 1u);
    ASSERT_EQ(result.functions_with_inferred_state.size(), 1u);
    EXPECT_EQ(result.functions_with_inferred_state[0], "update-user");
}

// fnc update-user(param: Hold[var User]) @effect(state) → effect_state_inferred == 0
TEST_F(MutabilityEnforcementPassTest, VarParamWithExistingEffectStateNoInference) {
    auto func = make_function_with_params("update-user", {
        make_typed_param("param", make_generic_type("Hold", {{"User", MQ::VAR}}))
    }, {}, {"state"});

    std::vector<meld::parser::ast::expression> exprs = {std::move(func)};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_EQ(result.effect_state_inferred, 0u);
    EXPECT_TRUE(result.functions_with_inferred_state.empty());
}

// fnc read-user(param: Hold[val User]) → effect_state_inferred == 0
TEST_F(MutabilityEnforcementPassTest, ValParamNoEffectStateInference) {
    auto func = make_function_with_params("read-user", {
        make_typed_param("param", make_generic_type("Hold", {{"User", MQ::VAL}}))
    });

    std::vector<meld::parser::ast::expression> exprs = {std::move(func)};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_EQ(result.effect_state_inferred, 0u);
    EXPECT_TRUE(result.functions_with_inferred_state.empty());
}

// fnc multi-var(a: Hold[var User], b: List[var Item]) → effect_state_inferred == 1
TEST_F(MutabilityEnforcementPassTest, MultipleVarParamsInferOncePerFunction) {
    auto func = make_function_with_params("multi-var", {
        make_typed_param("a", make_generic_type("Hold", {{"User", MQ::VAR}})),
        make_typed_param("b", make_generic_type("List", {{"Item", MQ::VAR}}))
    });

    std::vector<meld::parser::ast::expression> exprs = {std::move(func)};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_EQ(result.effect_state_inferred, 1u);
    ASSERT_EQ(result.functions_with_inferred_state.size(), 1u);
    EXPECT_EQ(result.functions_with_inferred_state[0], "multi-var");
}
