/**
 * Tests for field-level @Getter and @Setter decorators.
 *
 * Validates that @Getter and @Setter, when applied to a single field,
 * generate proper function_definition AST nodes and inject them into
 * the parent class via add_method().
 *
 * Requirements: 25B.8, 25B.9, 25B.10, 25B.13
 */

#include <gtest/gtest.h>
#include "meld/macro/property_decorators.hpp"
#include "meld/macro/decorator.hpp"
#include "meld/macro/ast_abort.hpp"
#include "meld/parser/ast.hpp"
#include "meld/parser/ast_node.hpp"

using namespace meld::macro;
using namespace meld::parser::ast;

class FieldLevelGetterSetterTest : public ::testing::Test {
protected:
    void SetUp() override {
        DecoratorRegistry::instance().clear();
        ASTParentMap::instance().clear();
        register_property_decorators();
    }

    void TearDown() override {
        DecoratorRegistry::instance().clear();
        ASTParentMap::instance().clear();
    }

    // Helper: create a class with a single field, parent pointers wired
    void setup_class_with_field(class_definition& cls, const std::string& cls_name,
                                const std::string& field_name, const std::string& field_type,
                                bool is_mutable) {
        cls.name.name = cls_name;
        field_declaration field;
        field.name.name = field_name;
        field.type.type_name.name = field_type;
        field.is_mutable = is_mutable;
        cls.fields.push_back(std::move(field));
        cls.fields.back().set_parent(&cls);
    }
};

// =============================================================================
// @Getter field-level: injects getter into parent class (Requirement 25B.9)
// =============================================================================

TEST_F(FieldLevelGetterSetterTest, GetterFieldLevelInjectsMethodIntoParentClass) {
    class_definition cls;
    setup_class_with_field(cls, "User", "email", "string", false);

    MacroExpander expander;
    auto dec = DecoratorRegistry::instance().get_decorator("Getter");
    ASSERT_TRUE(dec.has_value());

    auto result = (*dec)->apply_to_field(cls.fields[0], expander);
    ASSERT_TRUE(result.has_value()) << "Field-level @Getter failed: " << result.error();

    // Getter method should be injected into the parent class
    EXPECT_TRUE(cls.has_method("email"));
}

TEST_F(FieldLevelGetterSetterTest, GetterFieldLevelMethodHasCorrectName) {
    class_definition cls;
    setup_class_with_field(cls, "User", "name", "string", true);

    MacroExpander expander;
    auto dec = DecoratorRegistry::instance().get_decorator("Getter");
    auto result = (*dec)->apply_to_field(cls.fields[0], expander);
    ASSERT_TRUE(result.has_value());

    auto* getter = cls.find_method("name");
    ASSERT_NE(getter, nullptr);
    EXPECT_EQ(getter->name.name, "name");
}

TEST_F(FieldLevelGetterSetterTest, GetterFieldLevelMethodHasReturnType) {
    class_definition cls;
    setup_class_with_field(cls, "User", "age", "int", false);

    MacroExpander expander;
    auto dec = DecoratorRegistry::instance().get_decorator("Getter");
    auto result = (*dec)->apply_to_field(cls.fields[0], expander);
    ASSERT_TRUE(result.has_value());

    auto* getter = cls.find_method("age");
    ASSERT_NE(getter, nullptr);
    EXPECT_TRUE(getter->has_return_type);
    EXPECT_EQ(getter->return_type.type_name.name, "int");
}

TEST_F(FieldLevelGetterSetterTest, GetterFieldLevelMethodHasNoParameters) {
    class_definition cls;
    setup_class_with_field(cls, "User", "score", "float", false);

    MacroExpander expander;
    auto dec = DecoratorRegistry::instance().get_decorator("Getter");
    auto result = (*dec)->apply_to_field(cls.fields[0], expander);
    ASSERT_TRUE(result.has_value());

    auto* getter = cls.find_method("score");
    ASSERT_NE(getter, nullptr);
    EXPECT_TRUE(getter->parameters.empty());
}

TEST_F(FieldLevelGetterSetterTest, GetterFieldLevelReturnsSymbolWithFieldName) {
    class_definition cls;
    setup_class_with_field(cls, "User", "email", "string", false);

    MacroExpander expander;
    auto dec = DecoratorRegistry::instance().get_decorator("Getter");
    auto result = (*dec)->apply_to_field(cls.fields[0], expander);
    ASSERT_TRUE(result.has_value());

    EXPECT_TRUE(result->is<meld::kernel::Symbol>());
    EXPECT_EQ(result->as<meld::kernel::Symbol>()->name(), "email");
}

// =============================================================================
// @Setter field-level: injects setter into parent class (Requirement 25B.10)
// =============================================================================

TEST_F(FieldLevelGetterSetterTest, SetterFieldLevelInjectsMethodIntoParentClass) {
    class_definition cls;
    setup_class_with_field(cls, "User", "email", "string", true);

    MacroExpander expander;
    auto dec = DecoratorRegistry::instance().get_decorator("Setter");
    ASSERT_TRUE(dec.has_value());

    auto result = (*dec)->apply_to_field(cls.fields[0], expander);
    ASSERT_TRUE(result.has_value()) << "Field-level @Setter failed: " << result.error();

    // Setter method should be injected into the parent class
    EXPECT_TRUE(cls.has_method("set_email"));
}

TEST_F(FieldLevelGetterSetterTest, SetterFieldLevelMethodHasCorrectName) {
    class_definition cls;
    setup_class_with_field(cls, "User", "name", "string", true);

    MacroExpander expander;
    auto dec = DecoratorRegistry::instance().get_decorator("Setter");
    auto result = (*dec)->apply_to_field(cls.fields[0], expander);
    ASSERT_TRUE(result.has_value());

    auto* setter = cls.find_method("set_name");
    ASSERT_NE(setter, nullptr);
    EXPECT_EQ(setter->name.name, "set_name");
}

TEST_F(FieldLevelGetterSetterTest, SetterFieldLevelMethodHasOneParameter) {
    class_definition cls;
    setup_class_with_field(cls, "User", "age", "int", true);

    MacroExpander expander;
    auto dec = DecoratorRegistry::instance().get_decorator("Setter");
    auto result = (*dec)->apply_to_field(cls.fields[0], expander);
    ASSERT_TRUE(result.has_value());

    auto* setter = cls.find_method("set_age");
    ASSERT_NE(setter, nullptr);
    ASSERT_EQ(setter->parameters.size(), 1);
    EXPECT_EQ(setter->parameters[0].name.name, "v");
    EXPECT_EQ(setter->parameters[0].type.type_name.name, "int");
}

TEST_F(FieldLevelGetterSetterTest, SetterFieldLevelMethodHasNoReturnType) {
    class_definition cls;
    setup_class_with_field(cls, "User", "score", "float", true);

    MacroExpander expander;
    auto dec = DecoratorRegistry::instance().get_decorator("Setter");
    auto result = (*dec)->apply_to_field(cls.fields[0], expander);
    ASSERT_TRUE(result.has_value());

    auto* setter = cls.find_method("set_score");
    ASSERT_NE(setter, nullptr);
    EXPECT_FALSE(setter->has_return_type);
}

TEST_F(FieldLevelGetterSetterTest, SetterFieldLevelReturnsSymbolWithSetterName) {
    class_definition cls;
    setup_class_with_field(cls, "User", "email", "string", true);

    MacroExpander expander;
    auto dec = DecoratorRegistry::instance().get_decorator("Setter");
    auto result = (*dec)->apply_to_field(cls.fields[0], expander);
    ASSERT_TRUE(result.has_value());

    EXPECT_TRUE(result->is<meld::kernel::Symbol>());
    EXPECT_EQ(result->as<meld::kernel::Symbol>()->name(), "set_email");
}

// =============================================================================
// Dual-mode dispatch: class-level vs field-level (Requirement 25B.8)
// =============================================================================

TEST_F(FieldLevelGetterSetterTest, GetterClassLevelStillWorksAfterFieldLevelUpdate) {
    class_definition cls;
    cls.name.name = "Person";

    field_declaration f1;
    f1.name.name = "name";
    f1.type.type_name.name = "string";
    f1.is_mutable = false;
    cls.fields.push_back(f1);

    field_declaration f2;
    f2.name.name = "age";
    f2.type.type_name.name = "int";
    f2.is_mutable = true;
    cls.fields.push_back(f2);

    MacroExpander expander;
    DecoratorContext context;
    auto result = context.apply_decorator(cls, "Getter", expander);

    ASSERT_TRUE(result.has_value());
    // Class-level should return a list of generated symbols
    if (result->is<meld::kernel::Cons>()) {
        auto list_result = meld::kernel::list_to_array(*result);
        ASSERT_TRUE(list_result.has_value());
        EXPECT_EQ(list_result->size(), 2);
    }
}

TEST_F(FieldLevelGetterSetterTest, SetterClassLevelStillWorksAfterFieldLevelUpdate) {
    class_definition cls;
    cls.name.name = "Person";

    field_declaration f1;
    f1.name.name = "name";
    f1.type.type_name.name = "string";
    f1.is_mutable = true;
    cls.fields.push_back(f1);

    field_declaration f2;
    f2.name.name = "birthYear";
    f2.type.type_name.name = "int";
    f2.is_mutable = false;
    cls.fields.push_back(f2);

    MacroExpander expander;
    DecoratorContext context;
    auto result = context.apply_decorator(cls, "Setter", expander);

    ASSERT_TRUE(result.has_value());
    // Only 1 setter for the mutable field
    if (result->is<meld::kernel::Cons>()) {
        auto list_result = meld::kernel::list_to_array(*result);
        ASSERT_TRUE(list_result.has_value());
        EXPECT_EQ(list_result->size(), 1);
    }
}

// =============================================================================
// Multiple fields: independent field-level application
// =============================================================================

TEST_F(FieldLevelGetterSetterTest, GetterOnMultipleFieldsIndependently) {
    class_definition cls;
    cls.name.name = "User";

    field_declaration f1;
    f1.name.name = "name";
    f1.type.type_name.name = "string";
    f1.is_mutable = false;
    cls.fields.push_back(std::move(f1));
    cls.fields.back().set_parent(&cls);

    field_declaration f2;
    f2.name.name = "age";
    f2.type.type_name.name = "int";
    f2.is_mutable = true;
    cls.fields.push_back(std::move(f2));
    cls.fields.back().set_parent(&cls);

    MacroExpander expander;
    auto dec = DecoratorRegistry::instance().get_decorator("Getter");

    auto r1 = (*dec)->apply_to_field(cls.fields[0], expander);
    ASSERT_TRUE(r1.has_value());
    auto r2 = (*dec)->apply_to_field(cls.fields[1], expander);
    ASSERT_TRUE(r2.has_value());

    // Both getters injected
    EXPECT_TRUE(cls.has_method("name"));
    EXPECT_TRUE(cls.has_method("age"));
}

TEST_F(FieldLevelGetterSetterTest, SetterOnMultipleFieldsIndependently) {
    class_definition cls;
    cls.name.name = "User";

    field_declaration f1;
    f1.name.name = "name";
    f1.type.type_name.name = "string";
    f1.is_mutable = true;
    cls.fields.push_back(std::move(f1));
    cls.fields.back().set_parent(&cls);

    field_declaration f2;
    f2.name.name = "age";
    f2.type.type_name.name = "int";
    f2.is_mutable = true;
    cls.fields.push_back(std::move(f2));
    cls.fields.back().set_parent(&cls);

    MacroExpander expander;
    auto dec = DecoratorRegistry::instance().get_decorator("Setter");

    auto r1 = (*dec)->apply_to_field(cls.fields[0], expander);
    ASSERT_TRUE(r1.has_value());
    auto r2 = (*dec)->apply_to_field(cls.fields[1], expander);
    ASSERT_TRUE(r2.has_value());

    // Both setters injected
    EXPECT_TRUE(cls.has_method("set_name"));
    EXPECT_TRUE(cls.has_method("set_age"));
}

// =============================================================================
// Combined @Getter + @Setter field-level on same field
// =============================================================================

TEST_F(FieldLevelGetterSetterTest, GetterAndSetterOnSameFieldBothInject) {
    class_definition cls;
    setup_class_with_field(cls, "Account", "balance", "float", true);

    MacroExpander expander;
    auto getter_dec = DecoratorRegistry::instance().get_decorator("Getter");
    auto setter_dec = DecoratorRegistry::instance().get_decorator("Setter");

    auto r1 = (*getter_dec)->apply_to_field(cls.fields[0], expander);
    ASSERT_TRUE(r1.has_value());
    auto r2 = (*setter_dec)->apply_to_field(cls.fields[0], expander);
    ASSERT_TRUE(r2.has_value());

    EXPECT_TRUE(cls.has_method("balance"));
    EXPECT_TRUE(cls.has_method("set_balance"));

    // Verify getter details
    auto* getter = cls.find_method("balance");
    ASSERT_NE(getter, nullptr);
    EXPECT_TRUE(getter->has_return_type);
    EXPECT_EQ(getter->return_type.type_name.name, "float");
    EXPECT_TRUE(getter->parameters.empty());

    // Verify setter details
    auto* setter = cls.find_method("set_balance");
    ASSERT_NE(setter, nullptr);
    EXPECT_FALSE(setter->has_return_type);
    ASSERT_EQ(setter->parameters.size(), 1);
    EXPECT_EQ(setter->parameters[0].name.name, "v");
    EXPECT_EQ(setter->parameters[0].type.type_name.name, "float");
}

// main() provided by gtest_main
