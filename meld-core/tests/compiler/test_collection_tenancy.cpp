/// @file test_collection_tenancy.cpp
/// @brief Tests for tenancy-aware collection API enforcement
///
/// Verifies that the MutabilityEnforcementPass correctly enforces
/// val/var qualifiers on collection type parameters.
///
/// Requirements: 167.1-167.7

#include <gtest/gtest.h>
#include "meld/compiler/mutability_enforcement_pass.hpp"

using namespace meld::compiler;
using MQ = meld::parser::ast::type_annotation::MutabilityQualifier;

// ===========================================================================
// Helpers
// ===========================================================================

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

static meld::parser::ast::expression make_id_expr(const std::string& name) {
    meld::parser::ast::identifier id;
    id.name = name;
    return meld::parser::ast::expression(id);
}

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

static meld::parser::ast::expression make_method_call(
    const std::string& receiver, const std::string& method,
    std::vector<meld::parser::ast::expression> args = {}
) {
    meld::parser::ast::binary_operation binop;
    binop.op = ".";
    binop.left = boost::spirit::x3::forward_ast<meld::parser::ast::expression>(make_id_expr(receiver));
    binop.right = boost::spirit::x3::forward_ast<meld::parser::ast::expression>(
        make_call_expr(method, std::move(args)));
    return meld::parser::ast::expression(
        boost::spirit::x3::forward_ast<meld::parser::ast::binary_operation>(std::move(binop)));
}

static meld::parser::ast::expression make_val_with_type(
    const std::string& name, meld::parser::ast::type_annotation type_ann
) {
    meld::parser::ast::val_declaration decl;
    decl.name.name = name;
    decl.has_type_annotation = true;
    decl.type_ann = boost::spirit::x3::forward_ast<meld::parser::ast::type_annotation>(
        std::move(type_ann));
    decl.value = boost::spirit::x3::forward_ast<meld::parser::ast::expression>(make_id_expr("dummy"));
    return meld::parser::ast::expression(
        boost::spirit::x3::forward_ast<meld::parser::ast::val_declaration>(std::move(decl)));
}

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
    func.body = boost::spirit::x3::forward_ast<meld::parser::ast::block_expression>(std::move(body));
    return meld::parser::ast::expression(
        boost::spirit::x3::forward_ast<meld::parser::ast::function_definition>(std::move(func)));
}

// ===========================================================================
// Tests
// ===========================================================================

class CollectionTenancyTest : public ::testing::Test {
protected:
    MutabilityEnforcementPass pass;
};

// List[val T].add() → E4010 (val type param, mutating method)
TEST_F(CollectionTenancyTest, ListValAddRejected) {
    auto val_decl = make_val_with_type("items",
        make_generic_type("List", {{"User", MQ::VAL}}));
    auto call = make_method_call("items", "add", {make_id_expr("u")});
    auto func = make_function_with_body("test", {std::move(val_decl), std::move(call)});

    std::vector<meld::parser::ast::expression> exprs = {std::move(func)};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.val_violations, 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4010");
}

// List[var T].add() without @effect(state) → E4011
TEST_F(CollectionTenancyTest, ListVarAddWithoutEffectRejected) {
    auto val_decl = make_val_with_type("items",
        make_generic_type("List", {{"User", MQ::VAR}}));
    auto call = make_method_call("items", "add", {make_id_expr("u")});
    auto func = make_function_with_body("test", {std::move(val_decl), std::move(call)});

    std::vector<meld::parser::ast::expression> exprs = {std::move(func)};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.effect_violations, 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4011");
}

// List[var T].add() with @effect(state) → OK
TEST_F(CollectionTenancyTest, ListVarAddWithEffectOk) {
    auto val_decl = make_val_with_type("items",
        make_generic_type("List", {{"User", MQ::VAR}}));
    auto call = make_method_call("items", "add", {make_id_expr("u")});
    auto func = make_function_with_body("test",
        {std::move(val_decl), std::move(call)}, {"state"});

    std::vector<meld::parser::ast::expression> exprs = {std::move(func)};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.diagnostics.empty());
}

// List[val T].size() → OK (non-mutating)
TEST_F(CollectionTenancyTest, ListValSizeOk) {
    auto val_decl = make_val_with_type("items",
        make_generic_type("List", {{"User", MQ::VAL}}));
    auto call = make_method_call("items", "size");
    auto func = make_function_with_body("test", {std::move(val_decl), std::move(call)});

    std::vector<meld::parser::ast::expression> exprs = {std::move(func)};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.diagnostics.empty());
}

// Map[val K, val V].clear() → E4010
TEST_F(CollectionTenancyTest, MapValClearRejected) {
    auto val_decl = make_val_with_type("m",
        make_generic_type("Map", {{"K", MQ::VAL}, {"V", MQ::VAL}}));
    auto call = make_method_call("m", "clear");
    auto func = make_function_with_body("test", {std::move(val_decl), std::move(call)});

    std::vector<meld::parser::ast::expression> exprs = {std::move(func)};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.val_violations, 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4010");
}

// Queue[var T].push() with @effect(state) → OK
TEST_F(CollectionTenancyTest, QueueVarPushWithEffectOk) {
    auto val_decl = make_val_with_type("q",
        make_generic_type("Queue", {{"Task", MQ::VAR}}));
    auto call = make_method_call("q", "push", {make_id_expr("t")});
    auto func = make_function_with_body("test",
        {std::move(val_decl), std::move(call)}, {"state"});

    std::vector<meld::parser::ast::expression> exprs = {std::move(func)};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.diagnostics.empty());
}

// Set[val T].remove() → E4010
TEST_F(CollectionTenancyTest, SetValRemoveRejected) {
    auto val_decl = make_val_with_type("s",
        make_generic_type("Set", {{"int", MQ::VAL}}));
    auto call = make_method_call("s", "remove", {make_id_expr("x")});
    auto func = make_function_with_body("test", {std::move(val_decl), std::move(call)});

    std::vector<meld::parser::ast::expression> exprs = {std::move(func)};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.val_violations, 1u);
}
