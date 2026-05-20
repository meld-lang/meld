#include <gtest/gtest.h>
#include "meld/compiler/view_access_pass.hpp"
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

// Helper: build a binary operation (for member access via ".")
static meld::parser::ast::binary_operation make_dot_access(
    meld::parser::ast::expression lhs,
    meld::parser::ast::expression rhs
) {
    meld::parser::ast::binary_operation binop;
    binop.op = ".";
    binop.left = boost::spirit::x3::forward_ast<meld::parser::ast::expression>(std::move(lhs));
    binop.right = boost::spirit::x3::forward_ast<meld::parser::ast::expression>(std::move(rhs));
    return binop;
}

// Helper: build a binary operation for safe navigation via "?."
static meld::parser::ast::binary_operation make_safe_nav_access(
    meld::parser::ast::expression lhs,
    meld::parser::ast::expression rhs
) {
    meld::parser::ast::binary_operation binop;
    binop.op = "?.";
    binop.left = boost::spirit::x3::forward_ast<meld::parser::ast::expression>(std::move(lhs));
    binop.right = boost::spirit::x3::forward_ast<meld::parser::ast::expression>(std::move(rhs));
    return binop;
}

// Helper: build a type annotation
static meld::parser::ast::type_annotation make_type(const std::string& name) {
    meld::parser::ast::type_annotation type;
    type.type_name.name = name;
    return type;
}

// Helper: build a function parameter with type annotation
static meld::parser::ast::function_parameter make_param(
    const std::string& name,
    const std::string& type_name = ""
) {
    meld::parser::ast::function_parameter param;
    param.name.name = name;
    if (!type_name.empty()) {
        param.type = make_type(type_name);
    }
    return param;
}

// Helper: wrap statements in a function definition with typed parameters
static meld::parser::ast::expression make_function_with_params(
    const std::string& fname,
    std::vector<meld::parser::ast::function_parameter> params,
    std::vector<meld::parser::ast::expression> body_stmts
) {
    meld::parser::ast::function_definition func;
    func.name.name = fname;
    func.parameters = std::move(params);
    meld::parser::ast::block_expression body;
    for (auto& stmt : body_stmts) {
        body.statements.push_back(
            boost::spirit::x3::forward_ast<meld::parser::ast::expression>(std::move(stmt)));
    }
    func.body = boost::spirit::x3::forward_ast<meld::parser::ast::block_expression>(
        std::move(body));
    return meld::parser::ast::expression(
        boost::spirit::x3::forward_ast<meld::parser::ast::function_definition>(std::move(func)));
}

// Helper: wrap statements in a function definition with simple param names
static meld::parser::ast::expression make_function_with_body(
    const std::string& fname,
    std::vector<std::string> param_names,
    std::vector<meld::parser::ast::expression> body_stmts
) {
    std::vector<meld::parser::ast::function_parameter> params;
    for (const auto& pname : param_names) {
        params.push_back(make_param(pname));
    }
    return make_function_with_params(fname, std::move(params), std::move(body_stmts));
}

// ===========================================================================
// LinkBindingTracker — unit tests
// ===========================================================================

class LinkBindingTrackerTest : public ::testing::Test {
protected:
    LinkBindingTracker tracker;
};

TEST_F(LinkBindingTrackerTest, UnknownBindingIsNotLink) {
    EXPECT_FALSE(tracker.is_link_binding("unknown"));
}

TEST_F(LinkBindingTrackerTest, MarkedBindingIsLink) {
    tracker.mark_as_link("ref");
    EXPECT_TRUE(tracker.is_link_binding("ref"));
}

TEST_F(LinkBindingTrackerTest, PromotedBindingIsNotLink) {
    tracker.mark_as_link("ref");
    tracker.mark_as_promoted("ref");
    EXPECT_FALSE(tracker.is_link_binding("ref"));
    EXPECT_TRUE(tracker.is_promoted("ref"));
}

TEST_F(LinkBindingTrackerTest, ChildInheritsParentLinkBindings) {
    tracker.mark_as_link("ref");
    LinkBindingTracker child(&tracker);
    EXPECT_TRUE(child.is_link_binding("ref"));
}

TEST_F(LinkBindingTrackerTest, ChildPromotionDoesNotAffectParent) {
    tracker.mark_as_link("ref");
    LinkBindingTracker child(&tracker);
    child.mark_as_promoted("ref");
    EXPECT_FALSE(child.is_link_binding("ref"));
    // Parent still sees it as Link
    EXPECT_TRUE(tracker.is_link_binding("ref"));
}

// ===========================================================================
// ViewAccessPass — static helper tests
// ===========================================================================

TEST(IsLinkCallTest, RecognizesLinkByName) {
    auto registry = make_seeded_registry();
    auto call = make_call("link", {make_id("owner")});
    EXPECT_TRUE(ViewAccessPass::is_link_call(call, registry));
}

TEST(IsLinkCallTest, RecognizesQualifiedLink) {
    auto registry = make_seeded_registry();
    auto call = make_call("std.mem.link", {make_id("owner")});
    EXPECT_TRUE(ViewAccessPass::is_link_call(call, registry));
}

TEST(IsLinkCallTest, RejectsNonLinkCall) {
    auto registry = make_seeded_registry();
    auto call = make_call("print", {make_id("x")});
    EXPECT_FALSE(ViewAccessPass::is_link_call(call, registry));
}

TEST(IsLinkTypeAnnotationTest, RecognizesLinkType) {
    auto type = make_type("Link");
    EXPECT_TRUE(ViewAccessPass::is_link_type_annotation(type));
}

TEST(IsLinkTypeAnnotationTest, RecognizesQualifiedLinkType) {
    auto type = make_type("std.mem.Link");
    EXPECT_TRUE(ViewAccessPass::is_link_type_annotation(type));
}

TEST(IsLinkTypeAnnotationTest, RejectsOwnType) {
    auto type = make_type("Own");
    EXPECT_FALSE(ViewAccessPass::is_link_type_annotation(type));
}

TEST(IsLinkTypeAnnotationTest, RejectsArbitraryType) {
    auto type = make_type("String");
    EXPECT_FALSE(ViewAccessPass::is_link_type_annotation(type));
}

// ===========================================================================
// ViewAccessPass — integration tests (Task 4.1)
// ===========================================================================

class ViewAccessPassTest : public ::testing::Test {
protected:
    ViewAccessPass pass;
    IntrinsicResolutionRegistry registry;

    void SetUp() override {
        registry = make_seeded_registry();
    }
};

TEST_F(ViewAccessPassTest, EmptyProgramNoErrors) {
    std::vector<meld::parser::ast::expression> exprs;
    auto result = pass.run(exprs, registry, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.direct_access_errors, 0u);
}

TEST_F(ViewAccessPassTest, DirectMemberAccessOnLinkEmitsE4001) {
    // fnc test() {
    //   val ref = link(owner)
    //   ref.name          // E4001: direct access on View[T]
    // }
    meld::parser::ast::expression link_call(
        boost::spirit::x3::forward_ast<meld::parser::ast::function_call>(
            make_call("link", {make_id("owner")})));

    meld::parser::ast::expression val_decl(
        boost::spirit::x3::forward_ast<meld::parser::ast::val_declaration>(
            make_val("ref", link_call)));

    meld::parser::ast::expression dot_access(
        boost::spirit::x3::forward_ast<meld::parser::ast::binary_operation>(
            make_dot_access(make_id("ref"), make_id("name"))));

    auto func = make_function_with_body("test", {}, {val_decl, dot_access});

    std::vector<meld::parser::ast::expression> exprs = {func};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.direct_access_errors, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4001");
    EXPECT_NE(result.diagnostics[0].message.find("View[T]"), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("?."), std::string::npos);
}

TEST_F(ViewAccessPassTest, NonLinkBindingAccessIsOk) {
    // fnc test() {
    //   val obj = create()
    //   obj.name          // OK: not a Hold[T]
    // }
    meld::parser::ast::expression create_call(
        boost::spirit::x3::forward_ast<meld::parser::ast::function_call>(
            make_call("create", {})));

    meld::parser::ast::expression val_decl(
        boost::spirit::x3::forward_ast<meld::parser::ast::val_declaration>(
            make_val("obj", create_call)));

    meld::parser::ast::expression dot_access(
        boost::spirit::x3::forward_ast<meld::parser::ast::binary_operation>(
            make_dot_access(make_id("obj"), make_id("name"))));

    auto func = make_function_with_body("test", {}, {val_decl, dot_access});

    std::vector<meld::parser::ast::expression> exprs = {func};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.direct_access_errors, 0u);
}

TEST_F(ViewAccessPassTest, LinkParamDirectAccessEmitsE4001) {
    // fnc test(ref: Link) {
    //   ref.name          // E4001
    // }
    meld::parser::ast::expression dot_access(
        boost::spirit::x3::forward_ast<meld::parser::ast::binary_operation>(
            make_dot_access(make_id("ref"), make_id("name"))));

    auto func = make_function_with_params("test",
        {make_param("ref", "Link")},
        {dot_access});

    std::vector<meld::parser::ast::expression> exprs = {func};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.direct_access_errors, 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4001");
}

TEST_F(ViewAccessPassTest, MethodCallOnLinkEmitsE4001) {
    // fnc test() {
    //   val ref = link(owner)
    //   ref.method()      // E4001: method call on View[T]
    // }
    meld::parser::ast::expression link_call(
        boost::spirit::x3::forward_ast<meld::parser::ast::function_call>(
            make_call("link", {make_id("owner")})));

    meld::parser::ast::expression val_decl(
        boost::spirit::x3::forward_ast<meld::parser::ast::val_declaration>(
            make_val("ref", link_call)));

    // Method call represented as function_call with qualified name "ref.method"
    meld::parser::ast::expression method_call(
        boost::spirit::x3::forward_ast<meld::parser::ast::function_call>(
            make_call("ref.method", {})));

    auto func = make_function_with_body("test", {}, {val_decl, method_call});

    std::vector<meld::parser::ast::expression> exprs = {func};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.direct_access_errors, 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4001");
}

TEST_F(ViewAccessPassTest, MultipleDirectAccessesEmitMultipleE4001) {
    // fnc test() {
    //   val ref = link(owner)
    //   ref.name
    //   ref.age
    // }
    meld::parser::ast::expression link_call(
        boost::spirit::x3::forward_ast<meld::parser::ast::function_call>(
            make_call("link", {make_id("owner")})));

    meld::parser::ast::expression val_decl(
        boost::spirit::x3::forward_ast<meld::parser::ast::val_declaration>(
            make_val("ref", link_call)));

    meld::parser::ast::expression access1(
        boost::spirit::x3::forward_ast<meld::parser::ast::binary_operation>(
            make_dot_access(make_id("ref"), make_id("name"))));

    meld::parser::ast::expression access2(
        boost::spirit::x3::forward_ast<meld::parser::ast::binary_operation>(
            make_dot_access(make_id("ref"), make_id("age"))));

    auto func = make_function_with_body("test", {}, {val_decl, access1, access2});

    std::vector<meld::parser::ast::expression> exprs = {func};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.direct_access_errors, 2u);
    EXPECT_EQ(result.diagnostics.size(), 2u);
}

// ===========================================================================
// ViewAccessPass — upgrade scope tests (Task 4.2)
// ===========================================================================

TEST_F(ViewAccessPassTest, MatchUpgradeScopeAllowsAccess) {
    // fnc test() {
    //   val ref = link(owner)
    //   match ref {
    //     case obj => obj.name   // OK: obj is promoted to T
    //   }
    // }
    meld::parser::ast::expression link_call(
        boost::spirit::x3::forward_ast<meld::parser::ast::function_call>(
            make_call("link", {make_id("owner")})));

    meld::parser::ast::expression val_decl(
        boost::spirit::x3::forward_ast<meld::parser::ast::val_declaration>(
            make_val("ref", link_call)));

    // Build match expression
    meld::parser::ast::match_expression match;
    match.matched_value = boost::spirit::x3::forward_ast<meld::parser::ast::expression>(
        make_id("ref"));

    // on<some> branch: binds "obj" and accesses obj.name
    meld::parser::ast::match_case some_case;
    some_case.case_pattern.type = meld::parser::ast::PatternType::TYPE;
    some_case.case_pattern.binding_name.name = "obj";

    meld::parser::ast::expression obj_access(
        boost::spirit::x3::forward_ast<meld::parser::ast::binary_operation>(
            make_dot_access(make_id("obj"), make_id("name"))));
    some_case.result_expression = boost::spirit::x3::forward_ast<meld::parser::ast::expression>(
        std::move(obj_access));
    match.cases.push_back(std::move(some_case));

    meld::parser::ast::expression match_expr(
        boost::spirit::x3::forward_ast<meld::parser::ast::match_expression>(std::move(match)));

    auto func = make_function_with_body("test", {}, {val_decl, match_expr});

    std::vector<meld::parser::ast::expression> exprs = {func};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.direct_access_errors, 0u);
    EXPECT_EQ(result.upgrade_scopes_entered, 1u);
}

TEST_F(ViewAccessPassTest, MatchOnNoneBranchNoPromotion) {
    // fnc test() {
    //   val ref = link(owner)
    //   match ref {
    //     case _ => print("dead")   // on<none>: no binding, no promotion
    //   }
    // }
    meld::parser::ast::expression link_call(
        boost::spirit::x3::forward_ast<meld::parser::ast::function_call>(
            make_call("link", {make_id("owner")})));

    meld::parser::ast::expression val_decl(
        boost::spirit::x3::forward_ast<meld::parser::ast::val_declaration>(
            make_val("ref", link_call)));

    meld::parser::ast::match_expression match;
    match.matched_value = boost::spirit::x3::forward_ast<meld::parser::ast::expression>(
        make_id("ref"));

    // Wildcard case — no binding name
    meld::parser::ast::match_case none_case;
    none_case.case_pattern.type = meld::parser::ast::PatternType::WILDCARD;
    meld::parser::ast::expression print_call(
        boost::spirit::x3::forward_ast<meld::parser::ast::function_call>(
            make_call("print", {make_id("dead")})));
    none_case.result_expression = boost::spirit::x3::forward_ast<meld::parser::ast::expression>(
        std::move(print_call));
    match.cases.push_back(std::move(none_case));

    meld::parser::ast::expression match_expr(
        boost::spirit::x3::forward_ast<meld::parser::ast::match_expression>(std::move(match)));

    auto func = make_function_with_body("test", {}, {val_decl, match_expr});

    std::vector<meld::parser::ast::expression> exprs = {func};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.upgrade_scopes_entered, 0u);
}

TEST_F(ViewAccessPassTest, AccessAfterMatchScopeExitEmitsE4001) {
    // fnc test() {
    //   val ref = link(owner)
    //   match ref {
    //     case obj => obj.name   // OK inside upgrade scope
    //   }
    //   ref.name               // E4001: outside upgrade scope
    // }
    meld::parser::ast::expression link_call(
        boost::spirit::x3::forward_ast<meld::parser::ast::function_call>(
            make_call("link", {make_id("owner")})));

    meld::parser::ast::expression val_decl(
        boost::spirit::x3::forward_ast<meld::parser::ast::val_declaration>(
            make_val("ref", link_call)));

    // Match with upgrade
    meld::parser::ast::match_expression match;
    match.matched_value = boost::spirit::x3::forward_ast<meld::parser::ast::expression>(
        make_id("ref"));

    meld::parser::ast::match_case some_case;
    some_case.case_pattern.type = meld::parser::ast::PatternType::TYPE;
    some_case.case_pattern.binding_name.name = "obj";
    meld::parser::ast::expression obj_access(
        boost::spirit::x3::forward_ast<meld::parser::ast::binary_operation>(
            make_dot_access(make_id("obj"), make_id("name"))));
    some_case.result_expression = boost::spirit::x3::forward_ast<meld::parser::ast::expression>(
        std::move(obj_access));
    match.cases.push_back(std::move(some_case));

    meld::parser::ast::expression match_expr(
        boost::spirit::x3::forward_ast<meld::parser::ast::match_expression>(std::move(match)));

    // Direct access after match — should fail
    meld::parser::ast::expression direct_access(
        boost::spirit::x3::forward_ast<meld::parser::ast::binary_operation>(
            make_dot_access(make_id("ref"), make_id("name"))));

    auto func = make_function_with_body("test", {}, {val_decl, match_expr, direct_access});

    std::vector<meld::parser::ast::expression> exprs = {func};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.direct_access_errors, 1u);
    EXPECT_EQ(result.upgrade_scopes_entered, 1u);
}

TEST_F(ViewAccessPassTest, MultipleFunctionsAnalyzedIndependently) {
    // fnc f1() { val r = link(o); r.x }  // E4001
    // fnc f2() { val s = link(o); s.y }  // E4001
    auto make_link_access_func = [&](const std::string& fname,
                                      const std::string& binding,
                                      const std::string& field) {
        meld::parser::ast::expression link_call(
            boost::spirit::x3::forward_ast<meld::parser::ast::function_call>(
                make_call("link", {make_id("o")})));
        meld::parser::ast::expression val_decl(
            boost::spirit::x3::forward_ast<meld::parser::ast::val_declaration>(
                make_val(binding, link_call)));
        meld::parser::ast::expression dot_access(
            boost::spirit::x3::forward_ast<meld::parser::ast::binary_operation>(
                make_dot_access(make_id(binding), make_id(field))));
        return make_function_with_body(fname, {}, {val_decl, dot_access});
    };

    auto f1 = make_link_access_func("f1", "r", "x");
    auto f2 = make_link_access_func("f2", "s", "y");

    std::vector<meld::parser::ast::expression> exprs = {f1, f2};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.direct_access_errors, 2u);
    EXPECT_EQ(result.link_bindings_found, 2u);
}

TEST_F(ViewAccessPassTest, LinkBindingsTrackedCount) {
    // fnc test() {
    //   val a = link(x)
    //   val b = link(y)
    //   val c = create()   // not a link
    // }
    meld::parser::ast::expression link1(
        boost::spirit::x3::forward_ast<meld::parser::ast::function_call>(
            make_call("link", {make_id("x")})));
    meld::parser::ast::expression link2(
        boost::spirit::x3::forward_ast<meld::parser::ast::function_call>(
            make_call("link", {make_id("y")})));
    meld::parser::ast::expression create(
        boost::spirit::x3::forward_ast<meld::parser::ast::function_call>(
            make_call("create", {})));

    meld::parser::ast::expression val_a(
        boost::spirit::x3::forward_ast<meld::parser::ast::val_declaration>(
            make_val("a", link1)));
    meld::parser::ast::expression val_b(
        boost::spirit::x3::forward_ast<meld::parser::ast::val_declaration>(
            make_val("b", link2)));
    meld::parser::ast::expression val_c(
        boost::spirit::x3::forward_ast<meld::parser::ast::val_declaration>(
            make_val("c", create)));

    auto func = make_function_with_body("test", {}, {val_a, val_b, val_c});

    std::vector<meld::parser::ast::expression> exprs = {func};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.link_bindings_found, 2u);
}

// ===========================================================================
// ViewAccessPass — safe navigation (?.) tests (Task 58.5)
// ===========================================================================

TEST_F(ViewAccessPassTest, SafeNavigationOnLinkDoesNotEmitE4001) {
    // fnc test() {
    //   val ref = link(owner)
    //   ref?.name          // OK: safe navigation on View[T]
    // }
    meld::parser::ast::expression link_call(
        boost::spirit::x3::forward_ast<meld::parser::ast::function_call>(
            make_call("link", {make_id("owner")})));

    meld::parser::ast::expression val_decl(
        boost::spirit::x3::forward_ast<meld::parser::ast::val_declaration>(
            make_val("ref", link_call)));

    meld::parser::ast::expression safe_access(
        boost::spirit::x3::forward_ast<meld::parser::ast::binary_operation>(
            make_safe_nav_access(make_id("ref"), make_id("name"))));

    auto func = make_function_with_body("test", {}, {val_decl, safe_access});

    std::vector<meld::parser::ast::expression> exprs = {func};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.direct_access_errors, 0u);
    EXPECT_EQ(result.link_bindings_found, 1u);
}

TEST_F(ViewAccessPassTest, SafeNavigationOnLinkParamDoesNotEmitE4001) {
    // fnc test(ref: Link) {
    //   ref?.name          // OK: safe navigation
    // }
    meld::parser::ast::expression safe_access(
        boost::spirit::x3::forward_ast<meld::parser::ast::binary_operation>(
            make_safe_nav_access(make_id("ref"), make_id("name"))));

    auto func = make_function_with_params("test",
        {make_param("ref", "Link")},
        {safe_access});

    std::vector<meld::parser::ast::expression> exprs = {func};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.direct_access_errors, 0u);
}

TEST_F(ViewAccessPassTest, DirectAccessStillEmitsE4001WithUpdatedMessage) {
    // fnc test(ref: Link) {
    //   ref.name          // E4001 with updated message
    // }
    meld::parser::ast::expression dot_access(
        boost::spirit::x3::forward_ast<meld::parser::ast::binary_operation>(
            make_dot_access(make_id("ref"), make_id("name"))));

    auto func = make_function_with_params("test",
        {make_param("ref", "Link")},
        {dot_access});

    std::vector<meld::parser::ast::expression> exprs = {func};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.direct_access_errors, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4001");
    // Verify updated message mentions all three access patterns
    EXPECT_NE(result.diagnostics[0].message.find("?."), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("if val"), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("match"), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("View[T]"), std::string::npos);
}

TEST_F(ViewAccessPassTest, MixedSafeNavAndDirectAccessOnlyFlagsDirect) {
    // fnc test() {
    //   val ref = link(owner)
    //   ref?.name          // OK
    //   ref.age            // E4001
    // }
    meld::parser::ast::expression link_call(
        boost::spirit::x3::forward_ast<meld::parser::ast::function_call>(
            make_call("link", {make_id("owner")})));

    meld::parser::ast::expression val_decl(
        boost::spirit::x3::forward_ast<meld::parser::ast::val_declaration>(
            make_val("ref", link_call)));

    meld::parser::ast::expression safe_access(
        boost::spirit::x3::forward_ast<meld::parser::ast::binary_operation>(
            make_safe_nav_access(make_id("ref"), make_id("name"))));

    meld::parser::ast::expression direct_access(
        boost::spirit::x3::forward_ast<meld::parser::ast::binary_operation>(
            make_dot_access(make_id("ref"), make_id("age"))));

    auto func = make_function_with_body("test", {}, {val_decl, safe_access, direct_access});

    std::vector<meld::parser::ast::expression> exprs = {func};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.direct_access_errors, 1u);
}

TEST_F(ViewAccessPassTest, SafeNavigationOnNonLinkIsOk) {
    // fnc test() {
    //   val obj = create()
    //   obj?.name          // OK: not a View[T], no error
    // }
    meld::parser::ast::expression create_call(
        boost::spirit::x3::forward_ast<meld::parser::ast::function_call>(
            make_call("create", {})));

    meld::parser::ast::expression val_decl(
        boost::spirit::x3::forward_ast<meld::parser::ast::val_declaration>(
            make_val("obj", create_call)));

    meld::parser::ast::expression safe_access(
        boost::spirit::x3::forward_ast<meld::parser::ast::binary_operation>(
            make_safe_nav_access(make_id("obj"), make_id("name"))));

    auto func = make_function_with_body("test", {}, {val_decl, safe_access});

    std::vector<meld::parser::ast::expression> exprs = {func};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.direct_access_errors, 0u);
}

TEST(IsSafeNavigationAccessTest, DetectsSafeNavOperator) {
    auto binop = make_safe_nav_access(make_id("x"), make_id("y"));
    EXPECT_TRUE(ViewAccessPass::is_safe_navigation_access(binop));
}

TEST(IsSafeNavigationAccessTest, RejectsDotOperator) {
    auto binop = make_dot_access(make_id("x"), make_id("y"));
    EXPECT_FALSE(ViewAccessPass::is_safe_navigation_access(binop));
}

// ===========================================================================
// Property-based tests (Tasks 4.3 and 4.4)
// ===========================================================================

#include <rapidcheck.h>

// ---------------------------------------------------------------------------
// Property 5: View[T] direct access rejection
// Feature: hold-view-tenancy-model, Property 5: View[T] direct access rejection
// **Validates: Requirements 3.1, 7.1**
//
// For any View[T] binding and for any member access expression on that
// binding outside an upgrade scope, the Semantic Analyzer shall emit E4001.
// ---------------------------------------------------------------------------

TEST_F(ViewAccessPassTest, Property5_LinkDirectAccessRejection) {
    // Feature: hold-view-tenancy-model, Property 5: View[T] direct access rejection
    rc::check("Any member access on View[T] outside upgrade scope emits E4001",
        [this]() {
            // Generate random binding and field names
            auto binding_name = *rc::gen::suchThat(
                rc::gen::string<std::string>(),
                [](const std::string& s) {
                    return !s.empty() && s.size() < 20 &&
                           std::isalpha(s[0]) &&
                           std::all_of(s.begin(), s.end(),
                               [](char c) { return std::isalnum(c) || c == '_'; });
                });
            auto field_name = *rc::gen::suchThat(
                rc::gen::string<std::string>(),
                [](const std::string& s) {
                    return !s.empty() && s.size() < 20 &&
                           std::isalpha(s[0]) &&
                           std::all_of(s.begin(), s.end(),
                               [](char c) { return std::isalnum(c) || c == '_'; });
                });

            // Build: fnc test() { val <binding> = link(owner); <binding>.<field> }
            meld::parser::ast::expression link_call(
                boost::spirit::x3::forward_ast<meld::parser::ast::function_call>(
                    make_call("link", {make_id("owner")})));

            meld::parser::ast::expression val_decl(
                boost::spirit::x3::forward_ast<meld::parser::ast::val_declaration>(
                    make_val(binding_name, link_call)));

            meld::parser::ast::expression dot_access(
                boost::spirit::x3::forward_ast<meld::parser::ast::binary_operation>(
                    make_dot_access(make_id(binding_name), make_id(field_name))));

            auto func = make_function_with_body("test", {}, {val_decl, dot_access});

            std::vector<meld::parser::ast::expression> exprs = {func};
            auto result = pass.run(exprs, registry, "test.meld");

            RC_ASSERT(!result.success);
            RC_ASSERT(result.direct_access_errors >= 1u);
            RC_ASSERT(result.diagnostics.size() >= 1u);
            RC_ASSERT(result.diagnostics[0].code == "E4001");
            RC_ASSERT(result.diagnostics[0].message.find("View[T]") != std::string::npos);
        }
    );
}

// ---------------------------------------------------------------------------
// Property 6: View[T] upgrade round trip
// Feature: hold-view-tenancy-model, Property 6: View[T] upgrade round trip
// **Validates: Requirements 3.2, 3.3, 3.4, 3.5, 3.6**
//
// For any View[T] reference to a live object, upgrading via match shall
// yield a valid strong reference of type T in the on<some> branch, and
// the promoted binding shall be accessible without E4001. After the
// upgrade scope exits, direct access on the original View[T] binding
// shall again emit E4001.
// ---------------------------------------------------------------------------

TEST_F(ViewAccessPassTest, Property6_LinkUpgradeRoundTrip) {
    // Feature: hold-view-tenancy-model, Property 6: View[T] upgrade round trip
    rc::check("Upgrade via match allows access inside scope, rejects after exit",
        [this]() {
            // Generate random names
            auto link_name = *rc::gen::suchThat(
                rc::gen::string<std::string>(),
                [](const std::string& s) {
                    return !s.empty() && s.size() < 15 &&
                           std::isalpha(s[0]) &&
                           std::all_of(s.begin(), s.end(),
                               [](char c) { return std::isalnum(c) || c == '_'; });
                });
            auto promoted_name = *rc::gen::suchThat(
                rc::gen::string<std::string>(),
                [&link_name](const std::string& s) {
                    return !s.empty() && s.size() < 15 && s != link_name &&
                           std::isalpha(s[0]) &&
                           std::all_of(s.begin(), s.end(),
                               [](char c) { return std::isalnum(c) || c == '_'; });
                });
            auto field_name = *rc::gen::suchThat(
                rc::gen::string<std::string>(),
                [](const std::string& s) {
                    return !s.empty() && s.size() < 15 &&
                           std::isalpha(s[0]) &&
                           std::all_of(s.begin(), s.end(),
                               [](char c) { return std::isalnum(c) || c == '_'; });
                });

            // Build: fnc test() {
            //   val <link_name> = link(owner)
            //   match <link_name> {
            //     case <promoted_name> => <promoted_name>.<field_name>  // OK
            //   }
            //   <link_name>.<field_name>  // E4001
            // }
            meld::parser::ast::expression link_call(
                boost::spirit::x3::forward_ast<meld::parser::ast::function_call>(
                    make_call("link", {make_id("owner")})));

            meld::parser::ast::expression val_decl(
                boost::spirit::x3::forward_ast<meld::parser::ast::val_declaration>(
                    make_val(link_name, link_call)));

            // Match with upgrade scope
            meld::parser::ast::match_expression match;
            match.matched_value =
                boost::spirit::x3::forward_ast<meld::parser::ast::expression>(
                    make_id(link_name));

            meld::parser::ast::match_case some_case;
            some_case.case_pattern.type = meld::parser::ast::PatternType::TYPE;
            some_case.case_pattern.binding_name.name = promoted_name;

            meld::parser::ast::expression promoted_access(
                boost::spirit::x3::forward_ast<meld::parser::ast::binary_operation>(
                    make_dot_access(make_id(promoted_name), make_id(field_name))));
            some_case.result_expression =
                boost::spirit::x3::forward_ast<meld::parser::ast::expression>(
                    std::move(promoted_access));
            match.cases.push_back(std::move(some_case));

            meld::parser::ast::expression match_expr(
                boost::spirit::x3::forward_ast<meld::parser::ast::match_expression>(
                    std::move(match)));

            // Direct access after scope exit
            meld::parser::ast::expression direct_access(
                boost::spirit::x3::forward_ast<meld::parser::ast::binary_operation>(
                    make_dot_access(make_id(link_name), make_id(field_name))));

            auto func = make_function_with_body("test", {},
                {val_decl, match_expr, direct_access});

            std::vector<meld::parser::ast::expression> exprs = {func};
            auto result = pass.run(exprs, registry, "test.meld");

            // The match-internal access should be fine (upgrade scope)
            // but the post-match access should emit E4001
            RC_ASSERT(!result.success);
            RC_ASSERT(result.direct_access_errors == 1u);
            RC_ASSERT(result.upgrade_scopes_entered == 1u);
            RC_ASSERT(result.diagnostics[0].code == "E4001");
            RC_ASSERT(result.diagnostics[0].message.find("View[T]") != std::string::npos);
        }
    );
}
