#include <gtest/gtest.h>
#include "meld/compiler/move_tracking_pass.hpp"
#include "meld/compiler/intrinsic_resolution_pass.hpp"
#include "meld/std/mem.hpp"

using namespace meld::compiler;
using namespace meld::std_mem;

// ===========================================================================
// Helper: build a seeded IntrinsicResolutionRegistry
// ===========================================================================

static IntrinsicResolutionRegistry make_seeded_registry() {
    IntrinsicResolutionPass ir_pass;
    std::vector<meld::parser::ast::expression> empty;
    auto result = ir_pass.run(empty);
    return std::move(result.registry);
}

// Helper: build a function_call AST node
static meld::parser::ast::function_call make_call(
    const std::string& name,
    std::vector<meld::parser::ast::expression> args = {}
) {
    meld::parser::ast::function_call call;
    call.function_name.name = name;
    for (auto& a : args) {
        call.arguments.push_back(
            boost::spirit::x3::forward_ast<meld::parser::ast::expression>(std::move(a)));
    }
    return call;
}

// Helper: build an identifier expression
static meld::parser::ast::expression make_id(const std::string& name) {
    meld::parser::ast::identifier id;
    id.name = name;
    return meld::parser::ast::expression(id);
}

// Helper: build a val declaration
static meld::parser::ast::val_declaration make_val(
    const std::string& name,
    meld::parser::ast::expression init
) {
    meld::parser::ast::val_declaration decl;
    decl.name.name = name;
    decl.value = boost::spirit::x3::forward_ast<meld::parser::ast::expression>(std::move(init));
    return decl;
}

// Helper: wrap statements in a function definition
static meld::parser::ast::expression make_function_with_body(
    const std::string& fname,
    std::vector<std::string> param_names,
    std::vector<meld::parser::ast::expression> body_stmts
) {
    meld::parser::ast::function_definition func;
    func.name.name = fname;
    for (const auto& pname : param_names) {
        meld::parser::ast::function_parameter param;
        param.name.name = pname;
        func.parameters.push_back(param);
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
// BindingScope — unit tests
// ===========================================================================

class BindingScopeTest : public ::testing::Test {
protected:
    BindingScope scope;
};

TEST_F(BindingScopeTest, NewBindingIsLive) {
    scope.declare("x");
    EXPECT_EQ(scope.get_state("x"), BindingState::LIVE);
    EXPECT_TRUE(scope.has_binding("x"));
}

TEST_F(BindingScopeTest, UnknownBindingTreatedAsLive) {
    EXPECT_EQ(scope.get_state("unknown"), BindingState::LIVE);
    EXPECT_FALSE(scope.has_binding("unknown"));
}

TEST_F(BindingScopeTest, MarkMoved) {
    scope.declare("x");
    scope.mark_moved("x");
    EXPECT_EQ(scope.get_state("x"), BindingState::MOVED);
}

TEST_F(BindingScopeTest, MarkConditionallyMoved) {
    scope.declare("x");
    scope.mark_conditionally_moved("x");
    EXPECT_EQ(scope.get_state("x"), BindingState::CONDITIONALLY_MOVED);
}

TEST_F(BindingScopeTest, ChildScopeInheritsParent) {
    scope.declare("x");
    scope.mark_moved("x");

    BindingScope child(&scope);
    EXPECT_EQ(child.get_state("x"), BindingState::MOVED);
    EXPECT_TRUE(child.has_binding("x"));
}

TEST_F(BindingScopeTest, ChildScopeCanShadow) {
    scope.declare("x");
    scope.mark_moved("x");

    BindingScope child(&scope);
    child.declare("x");  // shadows parent's x
    EXPECT_EQ(child.get_state("x"), BindingState::LIVE);
}

TEST_F(BindingScopeTest, LocalStatesReturnsOnlyLocal) {
    scope.declare("x");
    scope.declare("y");
    scope.mark_moved("x");

    auto locals = scope.local_states();
    EXPECT_EQ(locals.size(), 2u);
    EXPECT_EQ(locals["x"], BindingState::MOVED);
    EXPECT_EQ(locals["y"], BindingState::LIVE);
}

// ===========================================================================
// MoveTrackingPass — static helper tests
// ===========================================================================

TEST(IsMoveCallTest, RecognizesMoveByName) {
    auto registry = make_seeded_registry();

    auto call = make_call("move", {make_id("x")});
    EXPECT_TRUE(MoveTrackingPass::is_move_call(call, registry));
}

TEST(IsMoveCallTest, RecognizesMemMoveByName) {
    auto registry = make_seeded_registry();

    auto call = make_call("mem_move", {make_id("x")});
    EXPECT_TRUE(MoveTrackingPass::is_move_call(call, registry));
}

TEST(IsMoveCallTest, RecognizesQualifiedMove) {
    auto registry = make_seeded_registry();

    auto call = make_call("std.mem.move", {make_id("x")});
    EXPECT_TRUE(MoveTrackingPass::is_move_call(call, registry));
}

TEST(IsMoveCallTest, RejectsNonMoveCall) {
    auto registry = make_seeded_registry();

    auto call = make_call("print", {make_id("x")});
    EXPECT_FALSE(MoveTrackingPass::is_move_call(call, registry));
}

TEST(ExtractMoveTargetTest, SingleIdentifierArg) {
    auto call = make_call("move", {make_id("my_var")});
    EXPECT_EQ(MoveTrackingPass::extract_move_target(call), "my_var");
}

TEST(ExtractMoveTargetTest, NoArgsReturnsEmpty) {
    auto call = make_call("move");
    EXPECT_EQ(MoveTrackingPass::extract_move_target(call), "");
}

TEST(ExtractMoveTargetTest, NonIdentifierArgReturnsEmpty) {
    meld::parser::ast::integer_literal lit;
    lit.value = 42;
    meld::parser::ast::expression lit_expr(lit);

    auto call = make_call("move", {lit_expr});
    EXPECT_EQ(MoveTrackingPass::extract_move_target(call), "");
}

// ===========================================================================
// MoveTrackingPass — integration tests
// ===========================================================================

class MoveTrackingPassTest : public ::testing::Test {
protected:
    MoveTrackingPass pass;
    IntrinsicResolutionRegistry registry;

    void SetUp() override {
        registry = make_seeded_registry();
    }
};

TEST_F(MoveTrackingPassTest, EmptyProgramNoErrors) {
    std::vector<meld::parser::ast::expression> exprs;
    auto result = pass.run(exprs, registry, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.errors_emitted, 0u);
}

TEST_F(MoveTrackingPassTest, SimpleMoveNoUseAfter) {
    // fnc test(x) { move(x) }
    // No use after move — should be clean
    meld::parser::ast::expression move_call(
        boost::spirit::x3::forward_ast<meld::parser::ast::function_call>(
            make_call("move", {make_id("x")})));

    auto func = make_function_with_body("test", {"x"}, {move_call});

    std::vector<meld::parser::ast::expression> exprs = {func};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.moves_detected, 1u);
    EXPECT_EQ(result.errors_emitted, 0u);
}

TEST_F(MoveTrackingPassTest, UseAfterMoveEmitsE4002) {
    // fnc test(x) { move(x); x }
    // Use of x after move — should emit E4002
    meld::parser::ast::expression move_call(
        boost::spirit::x3::forward_ast<meld::parser::ast::function_call>(
            make_call("move", {make_id("x")})));
    meld::parser::ast::expression use_x = make_id("x");

    auto func = make_function_with_body("test", {"x"}, {move_call, use_x});

    std::vector<meld::parser::ast::expression> exprs = {func};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errors_emitted, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4002");
    EXPECT_NE(result.diagnostics[0].message.find("use of moved value 'x'"),
              std::string::npos);
}

TEST_F(MoveTrackingPassTest, UseBeforeMoveIsOk) {
    // fnc test(x) { print(x); move(x) }
    // Use of x before move — should be fine
    meld::parser::ast::expression print_call(
        boost::spirit::x3::forward_ast<meld::parser::ast::function_call>(
            make_call("print", {make_id("x")})));
    meld::parser::ast::expression move_call(
        boost::spirit::x3::forward_ast<meld::parser::ast::function_call>(
            make_call("move", {make_id("x")})));

    auto func = make_function_with_body("test", {"x"}, {print_call, move_call});

    std::vector<meld::parser::ast::expression> exprs = {func};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.errors_emitted, 0u);
}

TEST_F(MoveTrackingPassTest, DoubleMoveEmitsE4002) {
    // fnc test(x) { move(x); move(x) }
    // Second move of already-moved x — should emit E4002
    meld::parser::ast::expression move1(
        boost::spirit::x3::forward_ast<meld::parser::ast::function_call>(
            make_call("move", {make_id("x")})));
    meld::parser::ast::expression move2(
        boost::spirit::x3::forward_ast<meld::parser::ast::function_call>(
            make_call("move", {make_id("x")})));

    auto func = make_function_with_body("test", {"x"}, {move1, move2});

    std::vector<meld::parser::ast::expression> exprs = {func};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_FALSE(result.success);
    EXPECT_GE(result.errors_emitted, 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4002");
}

TEST_F(MoveTrackingPassTest, ValDeclarationTracked) {
    // fnc test() { val y = 1; move(y); y }
    meld::parser::ast::integer_literal one;
    one.value = 1;
    meld::parser::ast::expression one_expr(one);

    meld::parser::ast::expression val_decl(
        boost::spirit::x3::forward_ast<meld::parser::ast::val_declaration>(
            make_val("y", one_expr)));
    meld::parser::ast::expression move_call(
        boost::spirit::x3::forward_ast<meld::parser::ast::function_call>(
            make_call("move", {make_id("y")})));
    meld::parser::ast::expression use_y = make_id("y");

    auto func = make_function_with_body("test", {}, {val_decl, move_call, use_y});

    std::vector<meld::parser::ast::expression> exprs = {func};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errors_emitted, 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4002");
    EXPECT_NE(result.diagnostics[0].message.find("'y'"), std::string::npos);
}

TEST_F(MoveTrackingPassTest, ConditionalMoveInMatchBranch) {
    // fnc test(x, cond) {
    //   match cond {
    //     case _ => move(x)   // branch 1: moves x
    //   }
    //   // Only one branch, so x is MOVED after match
    //   x  // should emit E4002
    // }
    //
    // With a single-branch match, x is moved in all branches → MOVED

    // Build the match expression with one case that moves x
    meld::parser::ast::match_expression match;
    match.matched_value = boost::spirit::x3::forward_ast<meld::parser::ast::expression>(
        make_id("cond"));

    meld::parser::ast::match_case case1;
    case1.case_pattern.type = meld::parser::ast::PatternType::WILDCARD;
    meld::parser::ast::expression move_in_branch(
        boost::spirit::x3::forward_ast<meld::parser::ast::function_call>(
            make_call("move", {make_id("x")})));
    case1.result_expression = boost::spirit::x3::forward_ast<meld::parser::ast::expression>(
        std::move(move_in_branch));
    match.cases.push_back(std::move(case1));

    meld::parser::ast::expression match_expr(
        boost::spirit::x3::forward_ast<meld::parser::ast::match_expression>(std::move(match)));

    meld::parser::ast::expression use_x = make_id("x");

    auto func = make_function_with_body("test", {"x", "cond"}, {match_expr, use_x});

    std::vector<meld::parser::ast::expression> exprs = {func};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_FALSE(result.success);
    EXPECT_GE(result.errors_emitted, 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4002");
}

TEST_F(MoveTrackingPassTest, ConditionalMoveInSomeBranches) {
    // fnc test(x, cond) {
    //   match cond {
    //     case a => move(x)   // branch 1: moves x
    //     case b => x         // branch 2: does NOT move x (just reads)
    //   }
    //   x  // should emit E4002 (conditionally moved)
    // }

    meld::parser::ast::match_expression match;
    match.matched_value = boost::spirit::x3::forward_ast<meld::parser::ast::expression>(
        make_id("cond"));

    // Branch 1: moves x
    meld::parser::ast::match_case case1;
    case1.case_pattern.type = meld::parser::ast::PatternType::TYPE;
    case1.case_pattern.binding_name.name = "a";
    meld::parser::ast::expression move_in_branch(
        boost::spirit::x3::forward_ast<meld::parser::ast::function_call>(
            make_call("move", {make_id("x")})));
    case1.result_expression = boost::spirit::x3::forward_ast<meld::parser::ast::expression>(
        std::move(move_in_branch));
    match.cases.push_back(std::move(case1));

    // Branch 2: just reads x (no move)
    meld::parser::ast::match_case case2;
    case2.case_pattern.type = meld::parser::ast::PatternType::TYPE;
    case2.case_pattern.binding_name.name = "b";
    case2.result_expression = boost::spirit::x3::forward_ast<meld::parser::ast::expression>(
        make_id("x"));
    match.cases.push_back(std::move(case2));

    meld::parser::ast::expression match_expr(
        boost::spirit::x3::forward_ast<meld::parser::ast::match_expression>(std::move(match)));

    meld::parser::ast::expression use_x = make_id("x");

    auto func = make_function_with_body("test", {"x", "cond"}, {match_expr, use_x});

    std::vector<meld::parser::ast::expression> exprs = {func};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_FALSE(result.success);
    EXPECT_GE(result.errors_emitted, 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4002");
    EXPECT_NE(result.diagnostics[0].message.find("possibly moved"), std::string::npos);
}

TEST_F(MoveTrackingPassTest, MoveInAllBranchesIsMoved) {
    // fnc test(x, cond) {
    //   match cond {
    //     case a => move(x)
    //     case b => move(x)
    //   }
    //   x  // should emit E4002 (moved, not conditionally)
    // }

    meld::parser::ast::match_expression match;
    match.matched_value = boost::spirit::x3::forward_ast<meld::parser::ast::expression>(
        make_id("cond"));

    // Branch 1: moves x
    meld::parser::ast::match_case case1;
    case1.case_pattern.type = meld::parser::ast::PatternType::TYPE;
    case1.case_pattern.binding_name.name = "a";
    meld::parser::ast::expression move1(
        boost::spirit::x3::forward_ast<meld::parser::ast::function_call>(
            make_call("move", {make_id("x")})));
    case1.result_expression = boost::spirit::x3::forward_ast<meld::parser::ast::expression>(
        std::move(move1));
    match.cases.push_back(std::move(case1));

    // Branch 2: also moves x
    meld::parser::ast::match_case case2;
    case2.case_pattern.type = meld::parser::ast::PatternType::TYPE;
    case2.case_pattern.binding_name.name = "b";
    meld::parser::ast::expression move2(
        boost::spirit::x3::forward_ast<meld::parser::ast::function_call>(
            make_call("move", {make_id("x")})));
    case2.result_expression = boost::spirit::x3::forward_ast<meld::parser::ast::expression>(
        std::move(move2));
    match.cases.push_back(std::move(case2));

    meld::parser::ast::expression match_expr(
        boost::spirit::x3::forward_ast<meld::parser::ast::match_expression>(std::move(match)));

    meld::parser::ast::expression use_x = make_id("x");

    auto func = make_function_with_body("test", {"x", "cond"}, {match_expr, use_x});

    std::vector<meld::parser::ast::expression> exprs = {func};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_FALSE(result.success);
    EXPECT_GE(result.errors_emitted, 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4002");
    EXPECT_NE(result.diagnostics[0].message.find("use of moved value"), std::string::npos);
}

TEST_F(MoveTrackingPassTest, UnrelatedBindingNotAffected) {
    // fnc test(x, y) { move(x); y }
    // y is not moved — should be fine
    meld::parser::ast::expression move_call(
        boost::spirit::x3::forward_ast<meld::parser::ast::function_call>(
            make_call("move", {make_id("x")})));
    meld::parser::ast::expression use_y = make_id("y");

    auto func = make_function_with_body("test", {"x", "y"}, {move_call, use_y});

    std::vector<meld::parser::ast::expression> exprs = {func};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.errors_emitted, 0u);
}

TEST_F(MoveTrackingPassTest, MultipleFunctionsAnalyzedIndependently) {
    // fnc f1(x) { move(x) }
    // fnc f2(x) { x }  // x in f2 is a different binding — should be fine
    auto f1 = make_function_with_body("f1", {"x"}, {
        meld::parser::ast::expression(
            boost::spirit::x3::forward_ast<meld::parser::ast::function_call>(
                make_call("move", {make_id("x")})))
    });
    auto f2 = make_function_with_body("f2", {"x"}, {make_id("x")});

    std::vector<meld::parser::ast::expression> exprs = {f1, f2};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.errors_emitted, 0u);
}

// ===========================================================================
// Compiler integration test
// ===========================================================================

TEST_F(MoveTrackingPassTest, BindingsTrackedCount) {
    // fnc test(a, b, c) { move(a) }
    // Should track 3 parameter bindings
    meld::parser::ast::expression move_call(
        boost::spirit::x3::forward_ast<meld::parser::ast::function_call>(
            make_call("move", {make_id("a")})));

    auto func = make_function_with_body("test", {"a", "b", "c"}, {move_call});

    std::vector<meld::parser::ast::expression> exprs = {func};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.bindings_tracked, 3u);
    EXPECT_EQ(result.moves_detected, 1u);
}
