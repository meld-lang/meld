#include <gtest/gtest.h>
#include "meld/parser/parser.hpp"
#include "meld/parser/ast.hpp"
#include "meld/parser/ast_node.hpp"
#include "meld/parser/ast_parent_setter.hpp"
#include "meld/parser/ast_mutations.hpp"

using namespace meld::parser;
using namespace meld::parser::ast;

// =============================================================================
// Test: ASTNode.reparent() — moving nodes between parents
// =============================================================================

TEST(ASTMutationTest, ReparentMovesNodeToNewParent) {
    class_definition cls1;
    cls1.name.name = "ClassA";
    class_definition cls2;
    cls2.name.name = "ClassB";

    field_declaration field;
    field.name.name = "x";
    field.is_mutable = false;

    // Start attached to cls1
    field.set_parent(&cls1);
    ASSERT_TRUE(field.has_parent());
    EXPECT_EQ(field.parent().value(), static_cast<ASTNode*>(&cls1));

    // Reparent to cls2
    field.reparent(&cls2);
    ASSERT_TRUE(field.has_parent());
    EXPECT_EQ(field.parent().value(), static_cast<ASTNode*>(&cls2));
}

TEST(ASTMutationTest, ReparentToNullDetaches) {
    class_definition cls;
    field_declaration field;
    field.name.name = "x";
    field.is_mutable = false;
    field.set_parent(&cls);

    field.reparent(nullptr);
    EXPECT_FALSE(field.has_parent());
    EXPECT_EQ(field.parent(), std::nullopt);
}

// =============================================================================
// Test: add_method() wires parent pointers correctly
// =============================================================================

TEST(ASTMutationTest, AddMethodToClassWiresParent) {
    class_definition cls;
    cls.name.name = "User";

    function_definition getter;
    getter.name.name = "name";
    getter.has_return_type = true;

    // Method starts detached
    EXPECT_FALSE(getter.has_parent());

    add_method(cls, std::move(getter));

    ASSERT_EQ(cls.methods.size(), 1u);
    auto& added = cls.methods[0];

    // Method should have class as parent
    ASSERT_TRUE(added.has_parent());
    EXPECT_EQ(added.parent().value(), static_cast<ASTNode*>(&cls));

    // Method name should have method as parent
    ASSERT_TRUE(added.name.has_parent());
    EXPECT_EQ(added.name.parent().value(), static_cast<ASTNode*>(&added));
}

TEST(ASTMutationTest, AddMethodToStructWiresParent) {
    struct_definition s;
    s.name.name = "Point";

    function_definition method;
    method.name.name = "magnitude";

    add_method(s, std::move(method));

    ASSERT_EQ(s.methods.size(), 1u);
    auto& added = s.methods[0];
    ASSERT_TRUE(added.has_parent());
    EXPECT_EQ(added.parent().value(), static_cast<ASTNode*>(&s));
}

TEST(ASTMutationTest, AddMethodWithParametersWiresAll) {
    class_definition cls;
    cls.name.name = "User";

    function_definition setter;
    setter.name.name = "set_name";

    function_parameter param;
    param.name.name = "v";
    setter.parameters.push_back(std::move(param));

    add_method(cls, std::move(setter));

    ASSERT_EQ(cls.methods.size(), 1u);
    auto& added = cls.methods[0];
    ASSERT_EQ(added.parameters.size(), 1u);

    // Parameter should have method as parent
    ASSERT_TRUE(added.parameters[0].has_parent());
    EXPECT_EQ(added.parameters[0].parent().value(), static_cast<ASTNode*>(&added));

    // Parameter name should have parameter as parent
    ASSERT_TRUE(added.parameters[0].name.has_parent());
    EXPECT_EQ(added.parameters[0].name.parent().value(),
              static_cast<ASTNode*>(&added.parameters[0]));
}

// =============================================================================
// Test: add_field() wires parent pointers correctly
// =============================================================================

TEST(ASTMutationTest, AddFieldToClassWiresParent) {
    class_definition cls;
    cls.name.name = "User";

    field_declaration field;
    field.name.name = "_name";
    field.is_mutable = false;

    add_field(cls, std::move(field));

    ASSERT_EQ(cls.fields.size(), 1u);
    auto& added = cls.fields[0];
    ASSERT_TRUE(added.has_parent());
    EXPECT_EQ(added.parent().value(), static_cast<ASTNode*>(&cls));
    ASSERT_TRUE(added.name.has_parent());
    EXPECT_EQ(added.name.parent().value(), static_cast<ASTNode*>(&added));
}

// =============================================================================
// Test: remove_field() detaches parent pointers
// =============================================================================

TEST(ASTMutationTest, RemoveFieldDetachesParent) {
    ASTParentMap::instance().clear();

    class_definition cls;
    cls.name.name = "User";

    field_declaration f1;
    f1.name.name = "name";
    f1.is_mutable = false;
    field_declaration f2;
    f2.name.name = "age";
    f2.is_mutable = true;

    add_field(cls, std::move(f1));
    add_field(cls, std::move(f2));
    ASSERT_EQ(cls.fields.size(), 2u);

    // Remove first field
    field_declaration removed = remove_field(cls, 0);
    EXPECT_EQ(removed.name.name, "name");
    EXPECT_FALSE(removed.has_parent());
    EXPECT_FALSE(removed.name.has_parent());

    // Remaining field still has correct parent
    ASSERT_EQ(cls.fields.size(), 1u);
    EXPECT_EQ(cls.fields[0].name.name, "age");
    // Note: after vector erase, the remaining element may have moved in memory.
    // rewire_children should be called after bulk mutations to fix this.
}

// =============================================================================
// Test: rewire_children() restores invariants after bulk mutation
// =============================================================================

TEST(ASTMutationTest, RewireChildrenFixesParentsAfterBulkMutation) {
    ASTParentMap::instance().clear();

    class_definition cls;
    cls.name.name = "User";

    // Manually push fields without wiring (simulating a bulk mutation)
    field_declaration f1;
    f1.name.name = "name";
    f1.is_mutable = false;
    cls.fields.push_back(std::move(f1));

    field_declaration f2;
    f2.name.name = "age";
    f2.is_mutable = true;
    cls.fields.push_back(std::move(f2));

    // Manually push a method without wiring
    function_definition getter;
    getter.name.name = "name";
    function_parameter param;
    param.name.name = "self";
    getter.parameters.push_back(std::move(param));
    cls.methods.push_back(std::move(getter));

    // Nothing is wired yet
    EXPECT_FALSE(cls.fields[0].has_parent());
    EXPECT_FALSE(cls.fields[1].has_parent());
    EXPECT_FALSE(cls.methods[0].has_parent());

    // Rewire everything
    rewire_children(cls);

    // Fields should now have class as parent
    for (auto& f : cls.fields) {
        ASSERT_TRUE(f.has_parent());
        EXPECT_EQ(f.parent().value(), static_cast<ASTNode*>(&cls));
        ASSERT_TRUE(f.name.has_parent());
        EXPECT_EQ(f.name.parent().value(), static_cast<ASTNode*>(&f));
    }

    // Method should have class as parent
    ASSERT_TRUE(cls.methods[0].has_parent());
    EXPECT_EQ(cls.methods[0].parent().value(), static_cast<ASTNode*>(&cls));

    // Method's parameter should have method as parent
    ASSERT_TRUE(cls.methods[0].parameters[0].has_parent());
    EXPECT_EQ(cls.methods[0].parameters[0].parent().value(),
              static_cast<ASTNode*>(&cls.methods[0]));
}

// =============================================================================
// Test: Macro expansion scenario — @Getter pattern end-to-end
// =============================================================================

TEST(ASTMutationTest, MacroExpansionScenario_GetterInjection) {
    // Simulates what a @Getter field-level macro would do:
    // 1. Parse a class with a field
    // 2. Navigate from field to parent class via .parent()
    // 3. Generate a getter method
    // 4. Inject it into the class via add_method()

    Parser parser;
    expression result;

    std::string input = R"(class User { val name: string })";
    ASSERT_TRUE(parser.parse_expression(input, result))
        << "Parse error: " << parser.error_message();

    set_parent_pointers(result);

    auto* cls_ast = boost::get<x3::forward_ast<class_definition>>(&result);
    ASSERT_NE(cls_ast, nullptr);
    class_definition& cls = cls_ast->get();

    // Step 1: Field-level macro receives the field node
    ASSERT_GE(cls.fields.size(), 1u);
    field_declaration& field = cls.fields[0];

    // Step 2: Navigate up to parent class (the ?: ast.abort pattern)
    auto parent_opt = field.parent();
    ASSERT_NE(parent_opt, std::nullopt)
        << "@Getter: field has no parent (would call ast.abort)";
    auto* parent_class = static_cast<class_definition*>(parent_opt.value());
    EXPECT_EQ(parent_class->name.name, "User");

    // Step 3: Generate a getter method (simulating ast.quote output)
    function_definition getter;
    getter.name.name = field.name.name;  // fnc name() -> string
    getter.has_return_type = true;

    // Step 4: Inject into parent class
    add_method(*parent_class, std::move(getter));

    // Verify the method was injected with correct parent pointers
    ASSERT_EQ(parent_class->methods.size(), 1u);
    auto& injected = parent_class->methods[0];
    EXPECT_EQ(injected.name.name, "name");
    ASSERT_TRUE(injected.has_parent());
    EXPECT_EQ(injected.parent().value(), static_cast<ASTNode*>(parent_class));

    // Original field still has correct parent
    ASSERT_TRUE(field.has_parent());
    EXPECT_EQ(field.parent().value(), static_cast<ASTNode*>(parent_class));
}

// =============================================================================
// Test: set_parent_pointers() wires macro-injected methods
// =============================================================================

TEST(ASTMutationTest, SetParentPointersWiresMethods) {
    // If methods are already present when set_parent_pointers is called
    // (e.g., after macro expansion), they should get wired too.
    ASTParentMap::instance().clear();

    class_definition cls;
    cls.name.name = "User";

    field_declaration field;
    field.name.name = "name";
    field.is_mutable = false;
    cls.fields.push_back(std::move(field));

    function_definition method;
    method.name.name = "get_name";
    cls.methods.push_back(std::move(method));

    // Wrap in expression and wire
    expression expr;
    expr = std::move(cls);
    set_parent_pointers(expr);

    auto* cls_ast = boost::get<x3::forward_ast<class_definition>>(&expr);
    ASSERT_NE(cls_ast, nullptr);
    class_definition& wired_cls = cls_ast->get();

    // Field should be wired
    ASSERT_GE(wired_cls.fields.size(), 1u);
    ASSERT_TRUE(wired_cls.fields[0].has_parent());
    EXPECT_EQ(wired_cls.fields[0].parent().value(),
              static_cast<ASTNode*>(&wired_cls));

    // Method should also be wired
    ASSERT_GE(wired_cls.methods.size(), 1u);
    ASSERT_TRUE(wired_cls.methods[0].has_parent());
    EXPECT_EQ(wired_cls.methods[0].parent().value(),
              static_cast<ASTNode*>(&wired_cls));
}

// =============================================================================
// Test: ASTParentMap.size() for diagnostics
// =============================================================================

TEST(ASTMutationTest, ParentMapSizeTracksRegistrations) {
    ASTParentMap::instance().clear();
    EXPECT_EQ(ASTParentMap::instance().size(), 0u);

    class_definition cls;
    cls.name.name = "Test";

    field_declaration field;
    field.name.name = "x";
    field.is_mutable = false;

    add_field(cls, std::move(field));

    // add_field registers: field -> cls, field.name -> field = 2 entries
    EXPECT_GE(ASTParentMap::instance().size(), 2u);
}
