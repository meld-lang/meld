#include <gtest/gtest.h>
#include "meld/parser/parser.hpp"
#include "meld/parser/ast.hpp"
#include "meld/parser/ast_node.hpp"
#include "meld/parser/ast_parent_setter.hpp"

using namespace meld::parser;
using namespace meld::parser::ast;

// =============================================================================
// Test: ASTNode base class basics
// =============================================================================

TEST(ASTParentPointerTest, NewNodeHasNoParent) {
    identifier id;
    id.name = "test";

    EXPECT_FALSE(id.has_parent());
    EXPECT_EQ(id.parent(), std::nullopt);
}

TEST(ASTParentPointerTest, SetParentMakesItAccessible) {
    class_definition cls;
    cls.name.name = "MyClass";

    field_declaration field;
    field.name.name = "x";
    field.is_mutable = false;

    field.set_parent(&cls);

    EXPECT_TRUE(field.has_parent());
    ASSERT_NE(field.parent(), std::nullopt);
    EXPECT_EQ(field.parent().value(), &cls);
}

TEST(ASTParentPointerTest, DetachClearsParent) {
    class_definition cls;
    field_declaration field;
    field.set_parent(&cls);

    EXPECT_TRUE(field.has_parent());

    field.detach();

    EXPECT_FALSE(field.has_parent());
    EXPECT_EQ(field.parent(), std::nullopt);
}

TEST(ASTParentPointerTest, SetParentToNullDetaches) {
    class_definition cls;
    field_declaration field;
    field.set_parent(&cls);
    EXPECT_TRUE(field.has_parent());

    field.set_parent(nullptr);
    EXPECT_FALSE(field.has_parent());
}

// =============================================================================
// Test: Copy/move semantics — parent is NOT transferred
// =============================================================================

TEST(ASTParentPointerTest, CopyDoesNotTransferParent) {
    class_definition cls;
    field_declaration original;
    original.name.name = "x";
    original.is_mutable = false;
    original.set_parent(&cls);

    // Copy the node
    field_declaration copy = original;

    // Original still has parent
    EXPECT_TRUE(original.has_parent());
    // Copy starts detached
    EXPECT_FALSE(copy.has_parent());
    EXPECT_EQ(copy.parent(), std::nullopt);
}

TEST(ASTParentPointerTest, MoveDoesNotTransferParent) {
    class_definition cls;
    field_declaration original;
    original.name.name = "x";
    original.is_mutable = false;
    original.set_parent(&cls);

    // Move the node
    field_declaration moved = std::move(original);

    // Moved node starts detached
    EXPECT_FALSE(moved.has_parent());
    EXPECT_EQ(moved.parent(), std::nullopt);
}

// =============================================================================
// Test: Parent pointer is a weak (non-owning) reference
// =============================================================================

TEST(ASTParentPointerTest, ParentPointerIsWeakReference) {
    // The parent pointer is a raw pointer (non-owning).
    // This test verifies the pointer doesn't extend lifetime.
    field_declaration field;
    field.name.name = "x";
    field.is_mutable = false;

    {
        class_definition cls;
        cls.name.name = "TempClass";
        field.set_parent(&cls);
        EXPECT_TRUE(field.has_parent());
    }
    // cls is destroyed here. The parent pointer is now dangling.
    // In real usage, the tree structure ensures parent outlives children.
    // This test just verifies the pointer doesn't prevent destruction
    // (i.e., it's truly a weak/non-owning reference, not a shared_ptr).
    // We can't safely dereference it, but we can verify it was set.
}

// =============================================================================
// Test: Parent pointer wiring via set_parent_pointers()
// =============================================================================

TEST(ASTParentPointerTest, ClassFieldsGetParentSet) {
    Parser parser;
    expression result;

    std::string input = R"(class Point { val x: int, val y: int })";
    ASSERT_TRUE(parser.parse_expression(input, result))
        << "Parse error: " << parser.error_message();

    // Wire parent pointers
    set_parent_pointers(result);

    auto* cls_ast = boost::get<x3::forward_ast<class_definition>>(&result);
    ASSERT_NE(cls_ast, nullptr) << "Expected class_definition";

    class_definition& cls = cls_ast->get();

    // The class itself should have no parent (it's the root)
    // (set_parent_pointers with nullptr parent means root is detached)
    EXPECT_FALSE(cls.has_parent());

    // Each field should have the class as parent
    ASSERT_GE(cls.fields.size(), 2u);
    for (auto& field : cls.fields) {
        ASSERT_TRUE(field.has_parent()) << "Field '" << field.name.name << "' should have a parent";
        EXPECT_EQ(field.parent().value(), static_cast<ASTNode*>(&cls))
            << "Field '" << field.name.name << "' parent should be the class";
    }
}

TEST(ASTParentPointerTest, FieldNameHasFieldAsParent) {
    Parser parser;
    expression result;

    std::string input = R"(class Widget { val label: string })";
    ASSERT_TRUE(parser.parse_expression(input, result))
        << "Parse error: " << parser.error_message();

    set_parent_pointers(result);

    auto* cls_ast = boost::get<x3::forward_ast<class_definition>>(&result);
    ASSERT_NE(cls_ast, nullptr);

    class_definition& cls = cls_ast->get();
    ASSERT_GE(cls.fields.size(), 1u);

    field_declaration& field = cls.fields[0];
    EXPECT_TRUE(field.name.has_parent());
    EXPECT_EQ(field.name.parent().value(), static_cast<ASTNode*>(&field));
}

TEST(ASTParentPointerTest, StructFieldsGetParentSet) {
    Parser parser;
    expression result;

    std::string input = R"(struct Vec2 { val x: float, val y: float })";
    ASSERT_TRUE(parser.parse_expression(input, result))
        << "Parse error: " << parser.error_message();

    set_parent_pointers(result);

    auto* struct_ast = boost::get<x3::forward_ast<struct_definition>>(&result);
    ASSERT_NE(struct_ast, nullptr) << "Expected struct_definition";

    struct_definition& s = struct_ast->get();
    ASSERT_GE(s.fields.size(), 2u);

    for (auto& field : s.fields) {
        ASSERT_TRUE(field.has_parent());
        EXPECT_EQ(field.parent().value(), static_cast<ASTNode*>(&s));
    }
}

TEST(ASTParentPointerTest, FunctionParametersGetParentSet) {
    Parser parser;
    expression result;

    std::string input = R"(fnc add(a: int, b: int) -> int { rtn a })";
    ASSERT_TRUE(parser.parse_expression(input, result))
        << "Parse error: " << parser.error_message();

    set_parent_pointers(result);

    auto* fn_ast = boost::get<x3::forward_ast<function_definition>>(&result);
    ASSERT_NE(fn_ast, nullptr) << "Expected function_definition";

    function_definition& fn = fn_ast->get();
    ASSERT_GE(fn.parameters.size(), 2u);

    for (auto& param : fn.parameters) {
        ASSERT_TRUE(param.has_parent());
        EXPECT_EQ(param.parent().value(), static_cast<ASTNode*>(&fn));
    }
}

// =============================================================================
// Test: Bottom-up traversal pattern (the key use case for macros)
// =============================================================================

TEST(ASTParentPointerTest, BottomUpTraversalFromFieldToClass) {
    // This tests the exact pattern used by field-level macros like @Getter:
    //   val parent_class = node.parent() ?: ast.abort("...")
    Parser parser;
    expression result;

    std::string input = R"(class User { val name: string, var age: int })";
    ASSERT_TRUE(parser.parse_expression(input, result))
        << "Parse error: " << parser.error_message();

    set_parent_pointers(result);

    auto* cls_ast = boost::get<x3::forward_ast<class_definition>>(&result);
    ASSERT_NE(cls_ast, nullptr);

    class_definition& cls = cls_ast->get();
    ASSERT_GE(cls.fields.size(), 1u);

    // Simulate what a field-level macro would do:
    // Start from a field, navigate up to the class
    field_declaration& field = cls.fields[0];
    auto parent_opt = field.parent();

    // The ?: pattern: parent must exist
    ASSERT_NE(parent_opt, std::nullopt)
        << "Field-level macro: .parent() returned nil (node is detached)";

    // Cast to class_definition to inject methods
    auto* parent_class = static_cast<class_definition*>(parent_opt.value());
    ASSERT_NE(parent_class, nullptr)
        << "Field's parent should be a class_definition";

    EXPECT_EQ(parent_class->name.name, "User");
}

// =============================================================================
// Test: Detached node returns nullopt (nil equivalent)
// =============================================================================

TEST(ASTParentPointerTest, DetachedNodeParentReturnsNullopt) {
    // A node not attached to any tree should return nullopt
    field_declaration detached_field;
    detached_field.name.name = "orphan";
    detached_field.is_mutable = false;

    EXPECT_EQ(detached_field.parent(), std::nullopt);
}
