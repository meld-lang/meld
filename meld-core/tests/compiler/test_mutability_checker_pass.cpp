/// @file test_mutability_checker_pass.cpp
/// @brief Unit tests for the Mutability Checker Pass
///
/// Tests that the pass correctly:
///   - Detects `this.field = value` assignments (E5001 when missing var fnc)
///   - Detects calls to sibling `var fnc` methods on `this`
///   - Warns on unnecessary `var fnc` declarations (W5001)
///   - Handles classes and structs
///   - Handles empty methods and nested expressions
///
/// Requirements: 57.2, 57.3

#include <gtest/gtest.h>
#include "meld/compiler/mutability_checker_pass.hpp"

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

/// Create a `this.field` dot-access expression.
static ast::expression make_this_dot(const std::string& field_name) {
    return make_binop(".", make_id_expr("this"), make_id_expr(field_name));
}

/// Create a `this.field = value` assignment expression.
static ast::expression make_this_field_assign(
    const std::string& field_name,
    ast::expression value
) {
    return make_binop("=", make_this_dot(field_name), std::move(value));
}

/// Create a function_call expression.
static ast::expression make_call(const std::string& fn_name) {
    ast::function_call call;
    call.function_name = make_id(fn_name);
    return ast::expression(
        boost::spirit::x3::forward_ast<ast::function_call>(std::move(call)));
}

/// Create a function_definition with a body containing the given statements.
static ast::function_definition make_method(
    const std::string& name,
    bool is_mutating,
    std::vector<ast::expression> body_stmts
) {
    ast::function_definition method;
    method.name = make_id(name);
    method.is_mutating = is_mutating;
    ast::block_expression body;
    for (auto& stmt : body_stmts) {
        body.statements.push_back(
            boost::spirit::x3::forward_ast<ast::expression>(std::move(stmt)));
    }
    method.body = boost::spirit::x3::forward_ast<ast::block_expression>(std::move(body));
    return method;
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

TEST(IsThisFieldAssignmentTest, DetectsThisDotFieldAssign) {
    // this.count = 1
    auto assign_expr = make_this_field_assign("count", make_id_expr("1"));
    // Extract the binary_operation from the expression
    const auto& binop = boost::get<
        boost::spirit::x3::forward_ast<ast::binary_operation>>(assign_expr).get();
    EXPECT_TRUE(MutabilityCheckerPass::is_this_field_assignment(binop));
}

TEST(IsThisFieldAssignmentTest, RejectsNonAssignment) {
    // this.count + 1 (not an assignment)
    auto add_expr = make_binop("+", make_this_dot("count"), make_id_expr("1"));
    const auto& binop = boost::get<
        boost::spirit::x3::forward_ast<ast::binary_operation>>(add_expr).get();
    EXPECT_FALSE(MutabilityCheckerPass::is_this_field_assignment(binop));
}

TEST(IsThisFieldAssignmentTest, RejectsNonThisAssignment) {
    // other.field = value (not this)
    auto assign_expr = make_binop("=",
        make_binop(".", make_id_expr("other"), make_id_expr("field")),
        make_id_expr("value"));
    const auto& binop = boost::get<
        boost::spirit::x3::forward_ast<ast::binary_operation>>(assign_expr).get();
    EXPECT_FALSE(MutabilityCheckerPass::is_this_field_assignment(binop));
}

TEST(IsThisFieldAssignmentTest, RejectsSimpleVarAssignment) {
    // x = 5 (no dot access at all)
    auto assign_expr = make_binop("=", make_id_expr("x"), make_id_expr("5"));
    const auto& binop = boost::get<
        boost::spirit::x3::forward_ast<ast::binary_operation>>(assign_expr).get();
    EXPECT_FALSE(MutabilityCheckerPass::is_this_field_assignment(binop));
}

TEST(IsThisVarFncCallTest, DetectsCallToSiblingVarFnc) {
    // Call to "increment" which is a var fnc sibling method
    ast::function_call call;
    call.function_name = make_id("increment");

    std::vector<ast::function_definition> siblings = {
        make_method("increment", true, {}),   // var fnc increment()
        make_method("current", false, {})      // fnc current()
    };

    EXPECT_TRUE(MutabilityCheckerPass::is_this_var_fnc_call(call, siblings));
}

TEST(IsThisVarFncCallTest, RejectsCallToNonVarFncSibling) {
    // Call to "current" which is NOT a var fnc method
    ast::function_call call;
    call.function_name = make_id("current");

    std::vector<ast::function_definition> siblings = {
        make_method("increment", true, {}),
        make_method("current", false, {})
    };

    EXPECT_FALSE(MutabilityCheckerPass::is_this_var_fnc_call(call, siblings));
}

TEST(IsThisVarFncCallTest, RejectsCallToUnknownMethod) {
    // Call to "unknown" which is not a sibling at all
    ast::function_call call;
    call.function_name = make_id("unknown");

    std::vector<ast::function_definition> siblings = {
        make_method("increment", true, {})
    };

    EXPECT_FALSE(MutabilityCheckerPass::is_this_var_fnc_call(call, siblings));
}

// ===========================================================================
// Integration tests — MutabilityCheckerPass::run()
// ===========================================================================

class MutabilityCheckerPassTest : public ::testing::Test {
protected:
    MutabilityCheckerPass pass;
};

// --- Empty / no-op cases ---

TEST_F(MutabilityCheckerPassTest, EmptyProgramNoDiagnostics) {
    std::vector<ast::expression> exprs;
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.methods_checked, 0u);
    EXPECT_EQ(result.mutations_detected, 0u);
    EXPECT_EQ(result.errors_emitted, 0u);
    EXPECT_EQ(result.warnings_emitted, 0u);
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST_F(MutabilityCheckerPassTest, ClassWithNoMethodsNoDiagnostics) {
    auto cls = make_class_with_methods("Empty", {});
    std::vector<ast::expression> exprs = {cls};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.methods_checked, 0u);
}

TEST_F(MutabilityCheckerPassTest, NonMutatingMethodNoDiagnostics) {
    // fnc current() { rtn count }  — no mutation, no var fnc → OK
    auto method = make_method("current", false, {make_id_expr("count")});
    auto cls = make_class_with_methods("Counter", {method});
    std::vector<ast::expression> exprs = {cls};
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.methods_checked, 1u);
    EXPECT_EQ(result.mutations_detected, 0u);
    EXPECT_TRUE(result.diagnostics.empty());
}

// --- E5001: mutates this without var fnc ---

TEST_F(MutabilityCheckerPassTest, MutatesThisWithoutVarFncEmitsE5001) {
    // fnc increment() { this.count = this.count + 1 }
    // Missing var fnc → E5001
    auto body_stmt = make_this_field_assign("count",
        make_binop("+", make_this_dot("count"), make_id_expr("1")));
    auto method = make_method("increment", false, {body_stmt});
    auto cls = make_class_with_methods("Counter", {method});

    std::vector<ast::expression> exprs = {cls};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.methods_checked, 1u);
    EXPECT_EQ(result.mutations_detected, 1u);
    EXPECT_EQ(result.errors_emitted, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E5001");
    EXPECT_EQ(result.diagnostics[0].level, MutabilityDiagnostic::Level::Error);
    EXPECT_NE(result.diagnostics[0].message.find("increment"), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("Counter"), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("var fnc"), std::string::npos);
    EXPECT_EQ(result.diagnostics[0].source_file, "test.meld");
}

// --- Correct var fnc usage: no diagnostics ---

TEST_F(MutabilityCheckerPassTest, VarFncWithMutationNoDiagnostics) {
    // var fnc increment() { this.count = this.count + 1 }
    auto body_stmt = make_this_field_assign("count",
        make_binop("+", make_this_dot("count"), make_id_expr("1")));
    auto method = make_method("increment", true, {body_stmt});
    auto cls = make_class_with_methods("Counter", {method});

    std::vector<ast::expression> exprs = {cls};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.methods_checked, 1u);
    EXPECT_EQ(result.mutations_detected, 1u);
    EXPECT_EQ(result.errors_emitted, 0u);
    EXPECT_EQ(result.warnings_emitted, 0u);
    EXPECT_TRUE(result.diagnostics.empty());
}

// --- W5001: var fnc but no mutation ---

TEST_F(MutabilityCheckerPassTest, VarFncWithoutMutationEmitsW5001) {
    // var fnc current() { rtn count }  — no mutation but declared var fnc → W5001
    auto method = make_method("current", true, {make_id_expr("count")});
    auto cls = make_class_with_methods("Counter", {method});

    std::vector<ast::expression> exprs = {cls};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_TRUE(result.success);  // Warnings don't block compilation
    EXPECT_EQ(result.methods_checked, 1u);
    EXPECT_EQ(result.mutations_detected, 0u);
    EXPECT_EQ(result.warnings_emitted, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "W5001");
    EXPECT_EQ(result.diagnostics[0].level, MutabilityDiagnostic::Level::Warning);
    EXPECT_NE(result.diagnostics[0].message.find("current"), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("Counter"), std::string::npos);
}

// --- Calling a sibling var fnc method counts as mutation ---

TEST_F(MutabilityCheckerPassTest, CallingVarFncSiblingCountsAsMutation) {
    // var fnc increment() { this.count = this.count + 1 }
    // fnc double_increment() { increment(); increment() }
    // double_increment calls a var fnc sibling → it mutates this → E5001
    auto inc_body = make_this_field_assign("count",
        make_binop("+", make_this_dot("count"), make_id_expr("1")));
    auto inc_method = make_method("increment", true, {inc_body});

    auto dbl_method = make_method("double_increment", false, {
        make_call("increment"),
        make_call("increment")
    });

    auto cls = make_class_with_methods("Counter", {inc_method, dbl_method});
    std::vector<ast::expression> exprs = {cls};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.methods_checked, 2u);
    // increment is correctly var fnc, double_increment is not
    EXPECT_EQ(result.errors_emitted, 1u);

    // Find the E5001 for double_increment
    bool found_e5001 = false;
    for (const auto& diag : result.diagnostics) {
        if (diag.code == "E5001" &&
            diag.message.find("double_increment") != std::string::npos) {
            found_e5001 = true;
        }
    }
    EXPECT_TRUE(found_e5001);
}

// --- Struct support ---

TEST_F(MutabilityCheckerPassTest, StructMethodMutationDetected) {
    // struct Point { var fnc move_x() { this.x = this.x + 1 } }
    auto body_stmt = make_this_field_assign("x",
        make_binop("+", make_this_dot("x"), make_id_expr("1")));
    auto method = make_method("move_x", true, {body_stmt});
    auto s = make_struct_with_methods("Point", {method});

    std::vector<ast::expression> exprs = {s};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.methods_checked, 1u);
    EXPECT_EQ(result.mutations_detected, 1u);
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST_F(MutabilityCheckerPassTest, StructMethodMissingVarFncEmitsE5001) {
    // struct Point { fnc move_x() { this.x = this.x + 1 } }  — missing var fnc
    auto body_stmt = make_this_field_assign("x",
        make_binop("+", make_this_dot("x"), make_id_expr("1")));
    auto method = make_method("move_x", false, {body_stmt});
    auto s = make_struct_with_methods("Point", {method});

    std::vector<ast::expression> exprs = {s};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errors_emitted, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E5001");
    EXPECT_NE(result.diagnostics[0].message.find("Point"), std::string::npos);
}

// --- Multiple methods, mixed diagnostics ---

TEST_F(MutabilityCheckerPassTest, MultipleMethodsMixedDiagnostics) {
    // class Widget {
    //   fnc get_name() { rtn name }           — OK
    //   fnc set_name() { this.name = "x" }    — E5001 (missing var fnc)
    //   var fnc reset() { this.name = "" }     — OK
    //   var fnc noop() { rtn nil }             — W5001 (unnecessary var fnc)
    // }
    auto get_name = make_method("get_name", false, {make_id_expr("name")});
    auto set_name = make_method("set_name", false, {
        make_this_field_assign("name", make_id_expr("x"))
    });
    auto reset = make_method("reset", true, {
        make_this_field_assign("name", make_id_expr(""))
    });
    auto noop = make_method("noop", true, {make_id_expr("nil")});

    auto cls = make_class_with_methods("Widget", {get_name, set_name, reset, noop});
    std::vector<ast::expression> exprs = {cls};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_FALSE(result.success);  // E5001 makes it fail
    EXPECT_EQ(result.methods_checked, 4u);
    EXPECT_EQ(result.mutations_detected, 2u);  // set_name and reset
    EXPECT_EQ(result.errors_emitted, 1u);       // set_name
    EXPECT_EQ(result.warnings_emitted, 1u);     // noop

    // Verify specific diagnostics
    bool found_e5001_set_name = false;
    bool found_w5001_noop = false;
    for (const auto& diag : result.diagnostics) {
        if (diag.code == "E5001" && diag.message.find("set_name") != std::string::npos) {
            found_e5001_set_name = true;
        }
        if (diag.code == "W5001" && diag.message.find("noop") != std::string::npos) {
            found_w5001_noop = true;
        }
    }
    EXPECT_TRUE(found_e5001_set_name);
    EXPECT_TRUE(found_w5001_noop);
}

// --- Multiple classes in one program ---

TEST_F(MutabilityCheckerPassTest, MultipleClassesAnalyzedIndependently) {
    // class A { var fnc inc() { this.x = 1 } }  — OK
    // class B { fnc dec() { this.y = 0 } }       — E5001
    auto a_method = make_method("inc", true, {
        make_this_field_assign("x", make_id_expr("1"))
    });
    auto b_method = make_method("dec", false, {
        make_this_field_assign("y", make_id_expr("0"))
    });

    auto cls_a = make_class_with_methods("A", {a_method});
    auto cls_b = make_class_with_methods("B", {b_method});

    std::vector<ast::expression> exprs = {cls_a, cls_b};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.methods_checked, 2u);
    EXPECT_EQ(result.errors_emitted, 1u);
    ASSERT_GE(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E5001");
    EXPECT_NE(result.diagnostics[0].message.find("dec"), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("B"), std::string::npos);
}

// --- Source file propagation ---

TEST_F(MutabilityCheckerPassTest, SourceFileInDiagnostics) {
    auto method = make_method("bad", false, {
        make_this_field_assign("x", make_id_expr("1"))
    });
    auto cls = make_class_with_methods("Foo", {method});
    std::vector<ast::expression> exprs = {cls};
    auto result = pass.run(exprs, "src/foo.meld");

    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].source_file, "src/foo.meld");
}
