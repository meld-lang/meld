#include <gtest/gtest.h>
#include "meld/compiler/migration_hints_pass.hpp"

using namespace meld::compiler;

// ===========================================================================
// Helper: build AST nodes
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
    return meld::parser::ast::expression(
        boost::spirit::x3::forward_ast<meld::parser::ast::class_definition>(
            std::move(cls)));
}

static meld::parser::ast::expression make_function_call_expr(
    const std::string& func_name
) {
    meld::parser::ast::function_call call;
    call.function_name.name = func_name;
    return meld::parser::ast::expression(
        boost::spirit::x3::forward_ast<meld::parser::ast::function_call>(
            std::move(call)));
}

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

static meld::parser::ast::expression make_val_with_type(
    const std::string& val_name,
    meld::parser::ast::type_annotation type_ann
) {
    meld::parser::ast::val_declaration decl;
    decl.name.name = val_name;
    decl.has_type_annotation = true;
    decl.type_ann = boost::spirit::x3::forward_ast<meld::parser::ast::type_annotation>(
        std::move(type_ann));
    // Dummy initializer
    meld::parser::ast::identifier id;
    id.name = "dummy";
    decl.value = boost::spirit::x3::forward_ast<meld::parser::ast::expression>(
        meld::parser::ast::expression(id));
    return meld::parser::ast::expression(
        boost::spirit::x3::forward_ast<meld::parser::ast::val_declaration>(
            std::move(decl)));
}

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

class MigrationHintsPassTest : public ::testing::Test {
protected:
    MigrationHintsPass pass;
};

// ===========================================================================
// Empty program
// ===========================================================================

TEST_F(MigrationHintsPassTest, EmptyProgramNoHints) {
    std::vector<meld::parser::ast::expression> exprs;
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.diagnostics.empty());
    EXPECT_EQ(result.own_hints, 0u);
    EXPECT_EQ(result.link_hints, 0u);
    EXPECT_EQ(result.link_call_hints, 0u);
}

// ===========================================================================
// I4010: Own → Hold hints
// ===========================================================================

TEST_F(MigrationHintsPassTest, OwnTypeInClassFieldEmitsI4010) {
    auto cls = make_class_with_fields("MyClass", {
        make_field("data", make_generic_type("Own", {"Widget"}))
    });
    std::vector<meld::parser::ast::expression> exprs = {cls};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.own_hints, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "I4010");
    EXPECT_EQ(result.diagnostics[0].level, MigrationHintDiagnostic::Level::Info);
    EXPECT_NE(result.diagnostics[0].message.find("Hold[T]"), std::string::npos);
}

TEST_F(MigrationHintsPassTest, StdMemOwnEmitsI4010) {
    auto cls = make_class_with_fields("MyClass", {
        make_field("data", make_generic_type("std.mem.Own", {"Widget"}))
    });
    std::vector<meld::parser::ast::expression> exprs = {cls};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_EQ(result.own_hints, 1u);
    EXPECT_EQ(result.diagnostics[0].code, "I4010");
}

TEST_F(MigrationHintsPassTest, MemOwnEmitsI4010) {
    auto cls = make_class_with_fields("MyClass", {
        make_field("data", make_generic_type("mem.Own", {"Widget"}))
    });
    std::vector<meld::parser::ast::expression> exprs = {cls};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_EQ(result.own_hints, 1u);
    EXPECT_EQ(result.diagnostics[0].code, "I4010");
}

// ===========================================================================
// I4011: Link → View hints
// ===========================================================================

TEST_F(MigrationHintsPassTest, LinkTypeInClassFieldEmitsI4011) {
    auto cls = make_class_with_fields("MyClass", {
        make_field("ref", make_generic_type("Link", {"Widget"}))
    });
    std::vector<meld::parser::ast::expression> exprs = {cls};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.link_hints, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "I4011");
    EXPECT_EQ(result.diagnostics[0].level, MigrationHintDiagnostic::Level::Info);
    EXPECT_NE(result.diagnostics[0].message.find("View[T]"), std::string::npos);
}

TEST_F(MigrationHintsPassTest, StdMemLinkEmitsI4011) {
    auto cls = make_class_with_fields("MyClass", {
        make_field("ref", make_generic_type("std.mem.Link", {"Widget"}))
    });
    std::vector<meld::parser::ast::expression> exprs = {cls};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_EQ(result.link_hints, 1u);
    EXPECT_EQ(result.diagnostics[0].code, "I4011");
}

TEST_F(MigrationHintsPassTest, MemLinkEmitsI4011) {
    auto cls = make_class_with_fields("MyClass", {
        make_field("ref", make_generic_type("mem.Link", {"Widget"}))
    });
    std::vector<meld::parser::ast::expression> exprs = {cls};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_EQ(result.link_hints, 1u);
    EXPECT_EQ(result.diagnostics[0].code, "I4011");
}

// ===========================================================================
// I4012: link() → view() hints
// ===========================================================================

TEST_F(MigrationHintsPassTest, LinkCallInFunctionBodyEmitsI4012) {
    auto func = make_function_with_body("example", {
        make_val_with_call_init("ref", "link")
    });
    std::vector<meld::parser::ast::expression> exprs = {func};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.link_call_hints, 1u);
    ASSERT_GE(result.diagnostics.size(), 1u);

    bool found_i4012 = false;
    for (const auto& diag : result.diagnostics) {
        if (diag.code == "I4012") {
            found_i4012 = true;
            EXPECT_EQ(diag.level, MigrationHintDiagnostic::Level::Info);
            EXPECT_NE(diag.message.find("view()"), std::string::npos);
        }
    }
    EXPECT_TRUE(found_i4012);
}

TEST_F(MigrationHintsPassTest, StdMemLinkCallEmitsI4012) {
    auto func = make_function_with_body("example", {
        make_val_with_call_init("ref", "std.mem.link")
    });
    std::vector<meld::parser::ast::expression> exprs = {func};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_EQ(result.link_call_hints, 1u);
}

TEST_F(MigrationHintsPassTest, MemLinkCallEmitsI4012) {
    auto func = make_function_with_body("example", {
        make_val_with_call_init("ref", "mem.link")
    });
    std::vector<meld::parser::ast::expression> exprs = {func};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_EQ(result.link_call_hints, 1u);
}

// ===========================================================================
// No false positives
// ===========================================================================

TEST_F(MigrationHintsPassTest, HoldTypeNoHint) {
    auto cls = make_class_with_fields("MyClass", {
        make_field("data", make_generic_type("Hold", {"Widget"}))
    });
    std::vector<meld::parser::ast::expression> exprs = {cls};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_TRUE(result.diagnostics.empty());
}

TEST_F(MigrationHintsPassTest, ViewTypeNoHint) {
    auto cls = make_class_with_fields("MyClass", {
        make_field("ref", make_generic_type("View", {"Widget"}))
    });
    std::vector<meld::parser::ast::expression> exprs = {cls};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_TRUE(result.diagnostics.empty());
}

TEST_F(MigrationHintsPassTest, ViewCallNoHint) {
    auto func = make_function_with_body("example", {
        make_val_with_call_init("ref", "view")
    });
    std::vector<meld::parser::ast::expression> exprs = {func};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_TRUE(result.diagnostics.empty());
}

// ===========================================================================
// Multiple hints in one program
// ===========================================================================

TEST_F(MigrationHintsPassTest, MultipleOldNamesEmitMultipleHints) {
    auto cls = make_class_with_fields("MyClass", {
        make_field("data", make_generic_type("Own", {"Widget"})),
        make_field("ref", make_generic_type("Link", {"Widget"}))
    });
    auto func = make_function_with_body("example", {
        make_val_with_call_init("r", "link")
    });
    std::vector<meld::parser::ast::expression> exprs = {cls, func};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_EQ(result.own_hints, 1u);
    EXPECT_EQ(result.link_hints, 1u);
    EXPECT_EQ(result.link_call_hints, 1u);
    EXPECT_EQ(result.diagnostics.size(), 3u);
}

// ===========================================================================
// Val declaration with type annotation
// ===========================================================================

TEST_F(MigrationHintsPassTest, ValWithOwnTypeAnnotationEmitsI4010) {
    auto func = make_function_with_body("example", {
        make_val_with_type("x", make_generic_type("Own", {"Widget"}))
    });
    std::vector<meld::parser::ast::expression> exprs = {func};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_EQ(result.own_hints, 1u);
    EXPECT_EQ(result.diagnostics[0].code, "I4010");
}

// ===========================================================================
// I4013: Dict → Map hints
// ===========================================================================

TEST_F(MigrationHintsPassTest, DictTypeInClassFieldEmitsI4013) {
    auto cls = make_class_with_fields("MyClass", {
        make_field("data", make_generic_type("Dict", {"string", "int"}))
    });
    std::vector<meld::parser::ast::expression> exprs = {cls};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.dict_hints, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "I4013");
    EXPECT_EQ(result.diagnostics[0].level, MigrationHintDiagnostic::Level::Info);
    EXPECT_NE(result.diagnostics[0].message.find("Map"), std::string::npos);
}

TEST_F(MigrationHintsPassTest, LowercaseDictEmitsI4013) {
    auto cls = make_class_with_fields("MyClass", {
        make_field("data", make_generic_type("dict", {"string", "int"}))
    });
    std::vector<meld::parser::ast::expression> exprs = {cls};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_EQ(result.dict_hints, 1u);
    EXPECT_EQ(result.diagnostics[0].code, "I4013");
}

// ===========================================================================
// I4014: Deque → Queue hints
// ===========================================================================

TEST_F(MigrationHintsPassTest, DequeTypeInClassFieldEmitsI4014) {
    auto cls = make_class_with_fields("MyClass", {
        make_field("q", make_generic_type("Deque", {"int"}))
    });
    std::vector<meld::parser::ast::expression> exprs = {cls};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.deque_hints, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "I4014");
    EXPECT_EQ(result.diagnostics[0].level, MigrationHintDiagnostic::Level::Info);
    EXPECT_NE(result.diagnostics[0].message.find("Queue"), std::string::npos);
}

TEST_F(MigrationHintsPassTest, LowercaseDequeEmitsI4014) {
    auto cls = make_class_with_fields("MyClass", {
        make_field("q", make_generic_type("deque", {"int"}))
    });
    std::vector<meld::parser::ast::expression> exprs = {cls};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_EQ(result.deque_hints, 1u);
    EXPECT_EQ(result.diagnostics[0].code, "I4014");
}

// ===========================================================================
// I4015: vec → List hints
// ===========================================================================

TEST_F(MigrationHintsPassTest, VecTypeInClassFieldEmitsI4015) {
    auto cls = make_class_with_fields("MyClass", {
        make_field("items", make_generic_type("vec", {"int"}))
    });
    std::vector<meld::parser::ast::expression> exprs = {cls};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.vec_hints, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "I4015");
    EXPECT_EQ(result.diagnostics[0].level, MigrationHintDiagnostic::Level::Info);
    EXPECT_NE(result.diagnostics[0].message.find("List"), std::string::npos);
}

// ===========================================================================
// No false positives for new collection names
// ===========================================================================

TEST_F(MigrationHintsPassTest, MapTypeNoHint) {
    auto cls = make_class_with_fields("MyClass", {
        make_field("data", make_generic_type("Map", {"string", "int"}))
    });
    std::vector<meld::parser::ast::expression> exprs = {cls};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_EQ(result.dict_hints, 0u);
    EXPECT_EQ(result.deque_hints, 0u);
    EXPECT_EQ(result.vec_hints, 0u);
}

TEST_F(MigrationHintsPassTest, QueueTypeNoHint) {
    auto cls = make_class_with_fields("MyClass", {
        make_field("q", make_generic_type("Queue", {"int"}))
    });
    std::vector<meld::parser::ast::expression> exprs = {cls};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_EQ(result.deque_hints, 0u);
}

TEST_F(MigrationHintsPassTest, ListTypeNoHint) {
    auto cls = make_class_with_fields("MyClass", {
        make_field("items", make_generic_type("List", {"int"}))
    });
    std::vector<meld::parser::ast::expression> exprs = {cls};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_EQ(result.vec_hints, 0u);
}
