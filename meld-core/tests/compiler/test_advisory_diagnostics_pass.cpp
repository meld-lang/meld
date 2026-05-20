#include <gtest/gtest.h>
#include "meld/compiler/advisory_diagnostics_pass.hpp"
#include "meld/compiler/intrinsic_resolution_pass.hpp"
#include "meld/std/mem.hpp"

using namespace meld::compiler;
using namespace meld::std_mem;

// ===========================================================================
// Helper: build AST nodes (same pattern as cycle detection tests)
// ===========================================================================

static meld::parser::ast::type_annotation make_type(const std::string& name) {
    meld::parser::ast::type_annotation type;
    type.type_name.name = name;
    return type;
}

static meld::parser::ast::type_annotation make_generic_type(
    const std::string& outer_name,
    std::vector<std::string> arg_names
) {
    meld::parser::ast::type_annotation type;
    type.type_name.name = outer_name;
    type.has_type_arguments = true;
    for (const auto& arg : arg_names) {
        type.type_arguments.push_back(
            boost::spirit::x3::forward_ast<meld::parser::ast::type_annotation>(
                make_type(arg)));
    }
    return type;
}

static meld::parser::ast::field_declaration make_field(
    const std::string& name,
    meld::parser::ast::type_annotation type
) {
    meld::parser::ast::field_declaration field;
    field.name.name = name;
    field.type = std::move(type);
    field.is_mutable = false;
    return field;
}

static meld::parser::ast::expression make_class_with_fields(
    const std::string& class_name,
    std::vector<meld::parser::ast::field_declaration> fields
) {
    meld::parser::ast::class_definition cls;
    cls.name.name = class_name;
    cls.fields = std::move(fields);
    meld::parser::ast::block_expression body;
    cls.body = boost::spirit::x3::forward_ast<meld::parser::ast::block_expression>(
        std::move(body));
    return meld::parser::ast::expression(
        boost::spirit::x3::forward_ast<meld::parser::ast::class_definition>(
            std::move(cls)));
}

// Helper: build a function with a body containing statements
static meld::parser::ast::expression make_function_with_body(
    const std::string& func_name,
    std::vector<meld::parser::ast::expression> body_stmts
) {
    meld::parser::ast::function_definition func;
    func.name.name = func_name;
    meld::parser::ast::block_expression body;
    for (auto& stmt : body_stmts) {
        body.statements.push_back(
            boost::spirit::x3::forward_ast<meld::parser::ast::expression>(
                std::move(stmt)));
    }
    func.body = boost::spirit::x3::forward_ast<meld::parser::ast::block_expression>(
        std::move(body));
    return meld::parser::ast::expression(
        boost::spirit::x3::forward_ast<meld::parser::ast::function_definition>(
            std::move(func)));
}

// Helper: build a val declaration with an identifier initializer
static meld::parser::ast::expression make_val_with_id_init(
    const std::string& val_name,
    const std::string& init_id
) {
    meld::parser::ast::val_declaration decl;
    decl.name.name = val_name;
    meld::parser::ast::identifier id;
    id.name = init_id;
    decl.value = boost::spirit::x3::forward_ast<meld::parser::ast::expression>(
        meld::parser::ast::expression(id));
    return meld::parser::ast::expression(
        boost::spirit::x3::forward_ast<meld::parser::ast::val_declaration>(
            std::move(decl)));
}

// Helper: build a val declaration with a function call initializer
static meld::parser::ast::expression make_val_with_call_init(
    const std::string& val_name,
    const std::string& func_name
) {
    meld::parser::ast::val_declaration decl;
    decl.name.name = val_name;
    meld::parser::ast::function_call call;
    call.function_name.name = func_name;
    decl.value = boost::spirit::x3::forward_ast<meld::parser::ast::expression>(
        meld::parser::ast::expression(
            boost::spirit::x3::forward_ast<meld::parser::ast::function_call>(
                std::move(call))));
    return meld::parser::ast::expression(
        boost::spirit::x3::forward_ast<meld::parser::ast::val_declaration>(
            std::move(decl)));
}

// ===========================================================================
// Test fixture
// ===========================================================================

class AdvisoryDiagnosticsPassTest : public ::testing::Test {
protected:
    AdvisoryDiagnosticsPass pass;
    IntrinsicResolutionRegistry registry;
};

// ===========================================================================
// W4002: Move suggestion tests
// ===========================================================================

TEST_F(AdvisoryDiagnosticsPassTest, EmptyProgramNoAdvisories) {
    std::vector<meld::parser::ast::expression> exprs;
    auto result = pass.run(exprs, registry, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.diagnostics.empty());
    EXPECT_EQ(result.move_suggestions, 0u);
    EXPECT_EQ(result.back_reference_suggestions, 0u);
}

TEST_F(AdvisoryDiagnosticsPassTest, ValCopyFromIdentifierEmitsW4002) {
    // fnc example() { val y = x }
    // Copying an identifier into a val → suggest move
    auto func = make_function_with_body("example", {
        make_val_with_id_init("y", "x")
    });

    std::vector<meld::parser::ast::expression> exprs = {func};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.move_suggestions, 1u);
    ASSERT_GE(result.diagnostics.size(), 1u);

    bool found_w4002 = false;
    for (const auto& diag : result.diagnostics) {
        if (diag.code == "W4002") {
            found_w4002 = true;
            EXPECT_EQ(diag.level, AdvisoryDiagnostic::Level::Warning);
            EXPECT_NE(diag.message.find("std.mem.move"), std::string::npos);
            EXPECT_NE(diag.message.find("x"), std::string::npos);
        }
    }
    EXPECT_TRUE(found_w4002);
}

TEST_F(AdvisoryDiagnosticsPassTest, ValFromFunctionCallNoW4002) {
    // fnc example() { val y = create_thing() }
    // Function call initializer → no move suggestion
    auto func = make_function_with_body("example", {
        make_val_with_call_init("y", "create_thing")
    });

    std::vector<meld::parser::ast::expression> exprs = {func};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.move_suggestions, 0u);
}

// ===========================================================================
// I4001: Back-reference pattern tests
// ===========================================================================

TEST_F(AdvisoryDiagnosticsPassTest, ParentFieldWithOwnEmitsI4001) {
    // class Child { val parent: Hold[Parent] }
    // class Parent { val name: string }
    // "parent" field name is a back-reference pattern → suggest View[T]
    auto child_cls = make_class_with_fields("Child", {
        make_field("parent", make_generic_type("Own", {"Parent"}))
    });
    auto parent_cls = make_class_with_fields("Parent", {
        make_field("name", make_type("string"))
    });

    std::vector<meld::parser::ast::expression> exprs = {child_cls, parent_cls};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.back_reference_suggestions, 1u);
    ASSERT_GE(result.diagnostics.size(), 1u);

    bool found_i4001 = false;
    for (const auto& diag : result.diagnostics) {
        if (diag.code == "I4001") {
            found_i4001 = true;
            EXPECT_EQ(diag.level, AdvisoryDiagnostic::Level::Info);
            EXPECT_NE(diag.message.find("View[Parent]"), std::string::npos);
            EXPECT_NE(diag.message.find("parent"), std::string::npos);
        }
    }
    EXPECT_TRUE(found_i4001);
}

TEST_F(AdvisoryDiagnosticsPassTest, MutualOwnBackRefEmitsI4001) {
    // class A { val b_ref: Hold[B] }
    // class B { val a_ref: Hold[A] }
    // B has Hold[A] and A has Hold[B] → B.a_ref looks like a back-reference
    auto a_cls = make_class_with_fields("A", {
        make_field("b_ref", make_generic_type("Own", {"B"}))
    });
    auto b_cls = make_class_with_fields("B", {
        make_field("a_ref", make_generic_type("Own", {"A"}))
    });

    std::vector<meld::parser::ast::expression> exprs = {a_cls, b_cls};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_TRUE(result.success);
    // Both directions detected as back-references (mutual Own)
    EXPECT_GE(result.back_reference_suggestions, 1u);

    bool found_i4001 = false;
    for (const auto& diag : result.diagnostics) {
        if (diag.code == "I4001") {
            found_i4001 = true;
            EXPECT_EQ(diag.level, AdvisoryDiagnostic::Level::Info);
        }
    }
    EXPECT_TRUE(found_i4001);
}

TEST_F(AdvisoryDiagnosticsPassTest, LinkFieldNoI4001) {
    // class Child { val parent: View[Parent] }
    // class Parent { val name: string }
    // Already using View[T] → no suggestion
    auto child_cls = make_class_with_fields("Child", {
        make_field("parent", make_generic_type("Link", {"Parent"}))
    });
    auto parent_cls = make_class_with_fields("Parent", {
        make_field("name", make_type("string"))
    });

    std::vector<meld::parser::ast::expression> exprs = {child_cls, parent_cls};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.back_reference_suggestions, 0u);
}

TEST_F(AdvisoryDiagnosticsPassTest, NonBackRefFieldNameNoI4001) {
    // class Node { val child: Hold[Other] }
    // class Other { val name: string }
    // "child" is not a back-reference pattern name, and Other doesn't own Node
    auto node_cls = make_class_with_fields("Node", {
        make_field("child", make_generic_type("Own", {"Other"}))
    });
    auto other_cls = make_class_with_fields("Other", {
        make_field("name", make_type("string"))
    });

    std::vector<meld::parser::ast::expression> exprs = {node_cls, other_cls};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.back_reference_suggestions, 0u);
}

TEST_F(AdvisoryDiagnosticsPassTest, OwnerFieldNameEmitsI4001) {
    // class Widget { val owner: Hold[Container] }
    // class Container { val name: string }
    // "owner" is a back-reference pattern name
    auto widget_cls = make_class_with_fields("Widget", {
        make_field("owner", make_generic_type("Own", {"Container"}))
    });
    auto container_cls = make_class_with_fields("Container", {
        make_field("name", make_type("string"))
    });

    std::vector<meld::parser::ast::expression> exprs = {widget_cls, container_cls};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.back_reference_suggestions, 1u);
}
