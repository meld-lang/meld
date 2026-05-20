/**
 * Tests for the node.parent() ?: ast.abort(...) pattern support.
 *
 * Validates that field-level macros can safely access parent nodes
 * via the require_parent helper, and that detached nodes produce
 * structured AstAbortError with clear messages.
 *
 * Requirements: 2.11
 */

#include <gtest/gtest.h>
#include "meld/macro/parent_access.hpp"
#include "meld/macro/ast_abort.hpp"
#include "meld/parser/ast.hpp"
#include "meld/parser/ast_node.hpp"
#include "meld/parser/parser.hpp"
#include "meld/parser/ast_parent_setter.hpp"

using namespace meld::macro;
using namespace meld::parser;
using namespace meld::parser::ast;

// =============================================================================
// require_parent succeeds when parent exists
// =============================================================================

TEST(ParentAccessTest, RequireParentReturnsParentWhenPresent) {
    class_definition cls;
    cls.name.name = "User";

    field_declaration field;
    field.name.name = "name";
    field.is_mutable = false;
    field.set_parent(&cls);

    // This is the C++ equivalent of: node.parent() ?: ast.abort("...")
    auto& parent = require_parent<class_definition>(
        field, "@Getter must be applied to a field inside a class");

    EXPECT_EQ(parent.name.name, "User");
}

TEST(ParentAccessTest, RequireParentReturnsCorrectClassForMultipleFields) {
    class_definition cls;
    cls.name.name = "Person";

    field_declaration name_field;
    name_field.name.name = "name";
    name_field.is_mutable = false;
    name_field.set_parent(&cls);

    field_declaration age_field;
    age_field.name.name = "age";
    age_field.is_mutable = true;
    age_field.set_parent(&cls);

    auto& parent1 = require_parent<class_definition>(
        name_field, "@Getter error");
    auto& parent2 = require_parent<class_definition>(
        age_field, "@Setter error");

    EXPECT_EQ(&parent1, &parent2);
    EXPECT_EQ(parent1.name.name, "Person");
}

// =============================================================================
// require_parent aborts when parent is absent (detached node)
// =============================================================================

TEST(ParentAccessTest, RequireParentAbortsWhenNoParent) {
    field_declaration detached;
    detached.name.name = "orphan";
    detached.is_mutable = false;

    EXPECT_THROW(
        require_parent<class_definition>(
            detached,
            "@Getter must be applied to a field inside a class"),
        AstAbortError);
}

TEST(ParentAccessTest, AbortMessageIsPreservedOnDetachedNode) {
    field_declaration detached;
    detached.name.name = "x";
    detached.is_mutable = false;

    try {
        require_parent<class_definition>(
            detached,
            "@Property must be applied to a field inside a class");
        FAIL() << "require_parent should have thrown AstAbortError";
    } catch (const AstAbortError& e) {
        EXPECT_EQ(e.abort_message(),
                  "@Property must be applied to a field inside a class");
    }
}

TEST(ParentAccessTest, AbortSourceLocationIsPreserved) {
    field_declaration detached;
    detached.name.name = "y";
    detached.is_mutable = false;

    try {
        require_parent<class_definition>(
            detached,
            "@Setter requires a class parent",
            AbortSourceLocation{"decorators.meld", 15, 5, 15, 30});
        FAIL() << "require_parent should have thrown AstAbortError";
    } catch (const AstAbortError& e) {
        EXPECT_EQ(e.source_location().file, "decorators.meld");
        EXPECT_EQ(e.source_location().line, 15u);
        EXPECT_EQ(e.source_location().column, 5u);
    }
}

// =============================================================================
// require_parent_node (non-template variant)
// =============================================================================

TEST(ParentAccessTest, RequireParentNodeReturnsRawPointer) {
    class_definition cls;
    cls.name.name = "Widget";

    field_declaration field;
    field.name.name = "label";
    field.is_mutable = false;
    field.set_parent(&cls);

    ASTNode* parent = require_parent_node(
        field, "@Getter must be applied to a field inside a class");

    EXPECT_EQ(parent, static_cast<ASTNode*>(&cls));
}

TEST(ParentAccessTest, RequireParentNodeAbortsWhenDetached) {
    field_declaration detached;
    detached.name.name = "z";
    detached.is_mutable = false;

    EXPECT_THROW(
        require_parent_node(detached, "no parent"),
        AstAbortError);
}

// =============================================================================
// Integration: parsed AST with set_parent_pointers
// =============================================================================

TEST(ParentAccessTest, WorksWithParsedClassFields) {
    Parser parser;
    expression result;

    std::string input = R"(class Account { val balance: int, var owner: string })";
    ASSERT_TRUE(parser.parse_expression(input, result))
        << "Parse error: " << parser.error_message();

    set_parent_pointers(result);

    auto* cls_ast = boost::get<x3::forward_ast<class_definition>>(&result);
    ASSERT_NE(cls_ast, nullptr);

    class_definition& cls = cls_ast->get();
    ASSERT_GE(cls.fields.size(), 2u);

    // Simulate a field-level macro using require_parent
    for (auto& field : cls.fields) {
        auto& parent = require_parent<class_definition>(
            field,
            "@Getter must be applied to a field inside a class");
        EXPECT_EQ(parent.name.name, "Account");
    }
}

TEST(ParentAccessTest, DetachedFieldAfterParseAborts) {
    Parser parser;
    expression result;

    std::string input = R"(class Temp { val x: int })";
    ASSERT_TRUE(parser.parse_expression(input, result))
        << "Parse error: " << parser.error_message();

    set_parent_pointers(result);

    auto* cls_ast = boost::get<x3::forward_ast<class_definition>>(&result);
    ASSERT_NE(cls_ast, nullptr);

    class_definition& cls = cls_ast->get();
    ASSERT_GE(cls.fields.size(), 1u);

    // Detach the field (simulates a top-level variable scenario)
    cls.fields[0].detach();

    EXPECT_THROW(
        require_parent<class_definition>(
            cls.fields[0],
            "@Getter must be applied to a field inside a class"),
        AstAbortError);
}

// =============================================================================
// AstAbortError is catchable as std::runtime_error (elvis fallback semantics)
// =============================================================================

TEST(ParentAccessTest, AbortIsCatchableAsRuntimeError) {
    field_declaration detached;
    detached.name.name = "w";
    detached.is_mutable = false;

    EXPECT_THROW(
        require_parent<class_definition>(detached, "test"),
        std::runtime_error);
}

// =============================================================================
// Struct parent access (not just class_definition)
// =============================================================================

TEST(ParentAccessTest, WorksWithStructParent) {
    struct_definition s;
    s.name.name = "Vec2";

    field_declaration field;
    field.name.name = "x";
    field.is_mutable = false;
    field.set_parent(&s);

    auto& parent = require_parent<struct_definition>(
        field, "@Property must be applied to a field inside a struct");

    EXPECT_EQ(parent.name.name, "Vec2");
}

// main() provided by gtest_main
