/**
 * Tests for class_definition::add_method() and struct_definition::add_method()
 * member functions, plus find_method() and has_method() lookup APIs.
 *
 * These member functions are the primary API for field-level macros:
 *   parent_class.add_method(getter)
 *
 * Requirements: 25B.13
 */

#include <gtest/gtest.h>
#include "meld/parser/parser.hpp"
#include "meld/parser/ast.hpp"
#include "meld/parser/ast_node.hpp"
#include "meld/parser/ast_parent_setter.hpp"

using namespace meld::parser;
using namespace meld::parser::ast;

// =============================================================================
// class_definition::add_method() — parent pointer wiring
// =============================================================================

TEST(AddMethodMemberTest, ClassAddMethodWiresParentPointer) {
    ASTParentMap::instance().clear();

    class_definition cls;
    cls.name.name = "User";

    function_definition getter;
    getter.name.name = "name";
    getter.has_return_type = true;

    EXPECT_FALSE(getter.has_parent());

    cls.add_method(std::move(getter));

    ASSERT_EQ(cls.methods.size(), 1u);
    auto& added = cls.methods[0];

    // Method's parent should be the class
    ASSERT_TRUE(added.has_parent());
    EXPECT_EQ(added.parent().value(), static_cast<ASTNode*>(&cls));

    // Method name's parent should be the method
    ASSERT_TRUE(added.name.has_parent());
    EXPECT_EQ(added.name.parent().value(), static_cast<ASTNode*>(&added));
}

TEST(AddMethodMemberTest, ClassAddMethodWiresParameters) {
    ASTParentMap::instance().clear();

    class_definition cls;
    cls.name.name = "User";

    function_definition setter;
    setter.name.name = "set_name";

    function_parameter param;
    param.name.name = "v";
    setter.parameters.push_back(std::move(param));

    cls.add_method(std::move(setter));

    ASSERT_EQ(cls.methods.size(), 1u);
    auto& added = cls.methods[0];
    ASSERT_EQ(added.parameters.size(), 1u);

    // Parameter's parent should be the method
    ASSERT_TRUE(added.parameters[0].has_parent());
    EXPECT_EQ(added.parameters[0].parent().value(), static_cast<ASTNode*>(&added));

    // Parameter name's parent should be the parameter
    ASSERT_TRUE(added.parameters[0].name.has_parent());
    EXPECT_EQ(added.parameters[0].name.parent().value(),
              static_cast<ASTNode*>(&added.parameters[0]));
}

TEST(AddMethodMemberTest, ClassAddMultipleMethods) {
    ASTParentMap::instance().clear();

    class_definition cls;
    cls.name.name = "User";

    function_definition getter;
    getter.name.name = "name";
    cls.add_method(std::move(getter));

    function_definition setter;
    setter.name.name = "set_name";
    function_parameter param;
    param.name.name = "v";
    setter.parameters.push_back(std::move(param));
    cls.add_method(std::move(setter));

    ASSERT_EQ(cls.methods.size(), 2u);
    EXPECT_EQ(cls.methods[0].name.name, "name");
    EXPECT_EQ(cls.methods[1].name.name, "set_name");

    // Both methods should have the class as parent
    for (auto& m : cls.methods) {
        ASSERT_TRUE(m.has_parent());
        EXPECT_EQ(m.parent().value(), static_cast<ASTNode*>(&cls));
    }
}

// =============================================================================
// struct_definition::add_method() — parent pointer wiring
// =============================================================================

TEST(AddMethodMemberTest, StructAddMethodWiresParentPointer) {
    ASTParentMap::instance().clear();

    struct_definition s;
    s.name.name = "Point";

    function_definition method;
    method.name.name = "magnitude";

    s.add_method(std::move(method));

    ASSERT_EQ(s.methods.size(), 1u);
    auto& added = s.methods[0];
    ASSERT_TRUE(added.has_parent());
    EXPECT_EQ(added.parent().value(), static_cast<ASTNode*>(&s));
}

// =============================================================================
// find_method() — method table lookup
// =============================================================================

TEST(AddMethodMemberTest, FindMethodReturnsPointerWhenFound) {
    ASTParentMap::instance().clear();

    class_definition cls;
    cls.name.name = "User";

    function_definition getter;
    getter.name.name = "name";
    cls.add_method(std::move(getter));

    auto* found = cls.find_method("name");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->name.name, "name");
}

TEST(AddMethodMemberTest, FindMethodReturnsNullptrWhenNotFound) {
    class_definition cls;
    cls.name.name = "User";

    auto* found = cls.find_method("nonexistent");
    EXPECT_EQ(found, nullptr);
}

TEST(AddMethodMemberTest, FindMethodDistinguishesByName) {
    ASTParentMap::instance().clear();

    class_definition cls;
    cls.name.name = "User";

    function_definition getter;
    getter.name.name = "name";
    cls.add_method(std::move(getter));

    function_definition setter;
    setter.name.name = "set_name";
    cls.add_method(std::move(setter));

    auto* g = cls.find_method("name");
    auto* s = cls.find_method("set_name");
    ASSERT_NE(g, nullptr);
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(g->name.name, "name");
    EXPECT_EQ(s->name.name, "set_name");
    EXPECT_NE(g, s);
}

TEST(AddMethodMemberTest, ConstFindMethodWorks) {
    ASTParentMap::instance().clear();

    class_definition cls;
    cls.name.name = "User";

    function_definition getter;
    getter.name.name = "name";
    cls.add_method(std::move(getter));

    const class_definition& const_cls = cls;
    const auto* found = const_cls.find_method("name");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->name.name, "name");
}

// =============================================================================
// has_method() — existence check
// =============================================================================

TEST(AddMethodMemberTest, HasMethodReturnsTrueWhenPresent) {
    ASTParentMap::instance().clear();

    class_definition cls;
    cls.name.name = "User";

    function_definition getter;
    getter.name.name = "name";
    cls.add_method(std::move(getter));

    EXPECT_TRUE(cls.has_method("name"));
}

TEST(AddMethodMemberTest, HasMethodReturnsFalseWhenAbsent) {
    class_definition cls;
    cls.name.name = "User";

    EXPECT_FALSE(cls.has_method("name"));
}

// =============================================================================
// struct find_method / has_method
// =============================================================================

TEST(AddMethodMemberTest, StructFindMethodWorks) {
    ASTParentMap::instance().clear();

    struct_definition s;
    s.name.name = "Vec2";

    function_definition method;
    method.name.name = "length";
    s.add_method(std::move(method));

    EXPECT_TRUE(s.has_method("length"));
    EXPECT_FALSE(s.has_method("width"));

    auto* found = s.find_method("length");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->name.name, "length");
}

// =============================================================================
// Integration: field-level macro pattern using member add_method()
// =============================================================================

TEST(AddMethodMemberTest, FieldLevelMacroPatternEndToEnd) {
    // Simulates the @Getter macro pattern using the member API:
    //   val parent_class = node.parent() ?: ast.abort("...")
    //   parent_class.add_method(getter)

    Parser parser;
    expression result;

    std::string input = R"(class User { val name: string })";
    ASSERT_TRUE(parser.parse_expression(input, result))
        << "Parse error: " << parser.error_message();

    set_parent_pointers(result);

    auto* cls_ast = boost::get<x3::forward_ast<class_definition>>(&result);
    ASSERT_NE(cls_ast, nullptr);
    class_definition& cls = cls_ast->get();

    ASSERT_GE(cls.fields.size(), 1u);
    field_declaration& field = cls.fields[0];

    // Step 1: Navigate to parent class via .parent()
    auto parent_opt = field.parent();
    ASSERT_NE(parent_opt, std::nullopt);
    auto* parent_class = static_cast<class_definition*>(parent_opt.value());
    EXPECT_EQ(parent_class->name.name, "User");

    // Step 2: Generate getter (simulating ast.quote output)
    function_definition getter;
    getter.name.name = field.name.name;
    getter.has_return_type = true;

    // Step 3: Inject via member add_method()
    parent_class->add_method(std::move(getter));

    // Verify: method is registered and findable
    ASSERT_EQ(parent_class->methods.size(), 1u);
    EXPECT_TRUE(parent_class->has_method("name"));

    auto* injected = parent_class->find_method("name");
    ASSERT_NE(injected, nullptr);
    EXPECT_EQ(injected->name.name, "name");

    // Verify: parent pointer is correct
    ASSERT_TRUE(injected->has_parent());
    EXPECT_EQ(injected->parent().value(), static_cast<ASTNode*>(parent_class));

    // Verify: original field still has correct parent
    ASSERT_TRUE(field.has_parent());
    EXPECT_EQ(field.parent().value(), static_cast<ASTNode*>(parent_class));
}

TEST(AddMethodMemberTest, PropertyMacroPatternGetterAndSetter) {
    // Simulates @Property generating both getter and setter

    ASTParentMap::instance().clear();

    class_definition cls;
    cls.name.name = "User";

    field_declaration field;
    field.name.name = "email";
    field.is_mutable = true;
    cls.fields.push_back(std::move(field));

    // Wire parent pointers for the field
    for (auto& f : cls.fields) {
        f.set_parent(&cls);
        f.name.set_parent(&f);
    }

    // Generate getter
    function_definition getter;
    getter.name.name = "email";
    getter.has_return_type = true;
    cls.add_method(std::move(getter));

    // Generate setter
    function_definition setter;
    setter.name.name = "set_email";
    function_parameter param;
    param.name.name = "v";
    setter.parameters.push_back(std::move(param));
    cls.add_method(std::move(setter));

    // Both methods should be findable
    EXPECT_TRUE(cls.has_method("email"));
    EXPECT_TRUE(cls.has_method("set_email"));
    EXPECT_FALSE(cls.has_method("nonexistent"));

    EXPECT_EQ(cls.methods.size(), 2u);
}

// main() provided by gtest_main
