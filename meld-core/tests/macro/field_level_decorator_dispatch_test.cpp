#include <gtest/gtest.h>
#include "meld/macro/decorator.hpp"
#include "meld/parser/ast.hpp"
#include "meld/parser/ast_node.hpp"

using namespace meld::macro;
using namespace meld::parser::ast;

class FieldLevelDecoratorDispatchTest : public ::testing::Test {
protected:
    void SetUp() override {
        DecoratorRegistry::instance().clear();
        ASTParentMap::instance().clear();
    }

    void TearDown() override {
        DecoratorRegistry::instance().clear();
        ASTParentMap::instance().clear();
    }

    // Helper: create a class with one field, parent pointers wired
    void make_class_with_field(class_definition& cls, field_declaration& field,
                               const std::string& cls_name,
                               const std::string& field_name,
                               const std::string& field_type,
                               bool is_mutable = true) {
        cls.name.name = cls_name;
        field.name.name = field_name;
        field.type.type_name.name = field_type;
        field.is_mutable = is_mutable;
        cls.fields.push_back(field);
        // Wire parent pointer: field → class
        cls.fields.back().set_parent(&cls);
    }
};

// --- DecoratorTargetKind enum tests ---

TEST_F(FieldLevelDecoratorDispatchTest, TargetKindEnumValues) {
    // Verify the enum has the expected values
    EXPECT_NE(DecoratorTargetKind::ClassNode, DecoratorTargetKind::FieldNode);
}

// --- Decorator supports_* query tests ---

TEST_F(FieldLevelDecoratorDispatchTest, ClassOnlyDecoratorSupportsClassTarget) {
    auto dec = make_decorator("ClassOnly",
        [](const class_definition&, MacroExpander&)
            -> std::expected<meld::kernel::Value, std::string> {
        return meld::kernel::Value(std::make_shared<meld::kernel::Symbol>("class_result"));
    });

    EXPECT_TRUE(dec->supports_class_target());
    EXPECT_FALSE(dec->supports_field_target());
}

TEST_F(FieldLevelDecoratorDispatchTest, FieldOnlyDecoratorSupportsFieldTarget) {
    auto dec = make_field_decorator("FieldOnly",
        [](const field_declaration&, MacroExpander&)
            -> std::expected<meld::kernel::Value, std::string> {
        return meld::kernel::Value(std::make_shared<meld::kernel::Symbol>("field_result"));
    });

    EXPECT_FALSE(dec->supports_class_target());
    EXPECT_TRUE(dec->supports_field_target());
}

TEST_F(FieldLevelDecoratorDispatchTest, DualModeDecoratorSupportsBothTargets) {
    auto dec = make_dual_decorator("DualMode",
        [](const class_definition&, MacroExpander&)
            -> std::expected<meld::kernel::Value, std::string> {
        return meld::kernel::Value(std::make_shared<meld::kernel::Symbol>("class_result"));
    },
        [](const field_declaration&, MacroExpander&)
            -> std::expected<meld::kernel::Value, std::string> {
        return meld::kernel::Value(std::make_shared<meld::kernel::Symbol>("field_result"));
    });

    EXPECT_TRUE(dec->supports_class_target());
    EXPECT_TRUE(dec->supports_field_target());
}

// --- Direct apply / apply_to_field tests ---

TEST_F(FieldLevelDecoratorDispatchTest, ApplyClassLevelReturnsExpectedValue) {
    auto dec = make_decorator("ClassDec",
        [](const class_definition& cls, MacroExpander&)
            -> std::expected<meld::kernel::Value, std::string> {
        return meld::kernel::Value(
            std::make_shared<meld::kernel::Symbol>("class_" + cls.name.name));
    });

    class_definition cls;
    cls.name.name = "Person";
    MacroExpander expander;

    auto result = dec->apply(cls, expander);
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(result->is<meld::kernel::Symbol>());
    EXPECT_EQ(result->as<meld::kernel::Symbol>()->name(), "class_Person");
}

TEST_F(FieldLevelDecoratorDispatchTest, ApplyFieldLevelReturnsExpectedValue) {
    auto dec = make_field_decorator("FieldDec",
        [](const field_declaration& field, MacroExpander&)
            -> std::expected<meld::kernel::Value, std::string> {
        return meld::kernel::Value(
            std::make_shared<meld::kernel::Symbol>("field_" + field.name.name));
    });

    field_declaration field;
    field.name.name = "age";
    field.type.type_name.name = "Int";
    field.is_mutable = true;
    MacroExpander expander;

    auto result = dec->apply_to_field(field, expander);
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(result->is<meld::kernel::Symbol>());
    EXPECT_EQ(result->as<meld::kernel::Symbol>()->name(), "field_age");
}

TEST_F(FieldLevelDecoratorDispatchTest, ClassOnlyDecoratorRejectsFieldApply) {
    auto dec = make_decorator("ClassOnly",
        [](const class_definition&, MacroExpander&)
            -> std::expected<meld::kernel::Value, std::string> {
        return meld::kernel::Value(std::make_shared<meld::kernel::Symbol>("ok"));
    });

    field_declaration field;
    field.name.name = "x";
    field.type.type_name.name = "Int";
    MacroExpander expander;

    auto result = dec->apply_to_field(field, expander);
    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().find("does not support field-level") != std::string::npos);
}

TEST_F(FieldLevelDecoratorDispatchTest, FieldOnlyDecoratorRejectsClassApply) {
    auto dec = make_field_decorator("FieldOnly",
        [](const field_declaration&, MacroExpander&)
            -> std::expected<meld::kernel::Value, std::string> {
        return meld::kernel::Value(std::make_shared<meld::kernel::Symbol>("ok"));
    });

    class_definition cls;
    cls.name.name = "Foo";
    MacroExpander expander;

    auto result = dec->apply(cls, expander);
    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().find("does not support class-level") != std::string::npos);
}

// --- DecoratorContext dispatch tests ---

TEST_F(FieldLevelDecoratorDispatchTest, ContextDispatchesToClassLevel) {
    auto dec = make_dual_decorator("Getter",
        [](const class_definition& cls, MacroExpander&)
            -> std::expected<meld::kernel::Value, std::string> {
        return meld::kernel::Value(
            std::make_shared<meld::kernel::Symbol>("class_getter_" + cls.name.name));
    },
        [](const field_declaration& field, MacroExpander&)
            -> std::expected<meld::kernel::Value, std::string> {
        return meld::kernel::Value(
            std::make_shared<meld::kernel::Symbol>("field_getter_" + field.name.name));
    });
    DecoratorRegistry::instance().register_decorator(dec);

    class_definition cls;
    cls.name.name = "User";
    MacroExpander expander;
    DecoratorContext context;

    auto result = context.dispatch_decorator(
        "Getter", DecoratorTargetKind::ClassNode, &cls, nullptr, expander);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->as<meld::kernel::Symbol>()->name(), "class_getter_User");
}

TEST_F(FieldLevelDecoratorDispatchTest, ContextDispatchesToFieldLevel) {
    auto dec = make_dual_decorator("Getter",
        [](const class_definition& cls, MacroExpander&)
            -> std::expected<meld::kernel::Value, std::string> {
        return meld::kernel::Value(
            std::make_shared<meld::kernel::Symbol>("class_getter_" + cls.name.name));
    },
        [](const field_declaration& field, MacroExpander&)
            -> std::expected<meld::kernel::Value, std::string> {
        return meld::kernel::Value(
            std::make_shared<meld::kernel::Symbol>("field_getter_" + field.name.name));
    });
    DecoratorRegistry::instance().register_decorator(dec);

    class_definition cls;
    field_declaration field;
    make_class_with_field(cls, field, "User", "email", "String");
    MacroExpander expander;
    DecoratorContext context;

    auto result = context.dispatch_decorator(
        "Getter", DecoratorTargetKind::FieldNode, nullptr, &cls.fields[0], expander);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->as<meld::kernel::Symbol>()->name(), "field_getter_email");
}

TEST_F(FieldLevelDecoratorDispatchTest, DispatchRejectsUnsupportedTargetKind) {
    // Register a class-only decorator, then try to dispatch as FieldNode
    auto dec = make_decorator("ToString",
        [](const class_definition&, MacroExpander&)
            -> std::expected<meld::kernel::Value, std::string> {
        return meld::kernel::Value(std::make_shared<meld::kernel::Symbol>("ok"));
    });
    DecoratorRegistry::instance().register_decorator(dec);

    field_declaration field;
    field.name.name = "x";
    field.type.type_name.name = "Int";
    MacroExpander expander;
    DecoratorContext context;

    auto result = context.dispatch_decorator(
        "ToString", DecoratorTargetKind::FieldNode, nullptr, &field, expander);

    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().find("does not support field-level") != std::string::npos);
}

TEST_F(FieldLevelDecoratorDispatchTest, DispatchRejectsNonExistentDecorator) {
    MacroExpander expander;
    DecoratorContext context;

    auto result = context.dispatch_decorator(
        "NonExistent", DecoratorTargetKind::ClassNode, nullptr, nullptr, expander);

    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().find("not found") != std::string::npos);
}

TEST_F(FieldLevelDecoratorDispatchTest, DispatchClassNodeWithNullClassDefErrors) {
    auto dec = make_decorator("Getter",
        [](const class_definition&, MacroExpander&)
            -> std::expected<meld::kernel::Value, std::string> {
        return meld::kernel::Value(std::make_shared<meld::kernel::Symbol>("ok"));
    });
    DecoratorRegistry::instance().register_decorator(dec);

    MacroExpander expander;
    DecoratorContext context;

    auto result = context.dispatch_decorator(
        "Getter", DecoratorTargetKind::ClassNode, nullptr, nullptr, expander);

    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().find("no class_definition provided") != std::string::npos);
}

TEST_F(FieldLevelDecoratorDispatchTest, DispatchFieldNodeWithNullFieldErrors) {
    auto dec = make_field_decorator("FieldDec",
        [](const field_declaration&, MacroExpander&)
            -> std::expected<meld::kernel::Value, std::string> {
        return meld::kernel::Value(std::make_shared<meld::kernel::Symbol>("ok"));
    });
    DecoratorRegistry::instance().register_decorator(dec);

    MacroExpander expander;
    DecoratorContext context;

    auto result = context.dispatch_decorator(
        "FieldDec", DecoratorTargetKind::FieldNode, nullptr, nullptr, expander);

    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().find("no field_declaration provided") != std::string::npos);
}

// --- Context apply_decorator overload for field_declaration ---

TEST_F(FieldLevelDecoratorDispatchTest, ContextApplyDecoratorFieldOverload) {
    auto dec = make_field_decorator("FieldGetter",
        [](const field_declaration& field, MacroExpander&)
            -> std::expected<meld::kernel::Value, std::string> {
        return meld::kernel::Value(
            std::make_shared<meld::kernel::Symbol>("getter_" + field.name.name));
    });
    DecoratorRegistry::instance().register_decorator(dec);

    field_declaration field;
    field.name.name = "score";
    field.type.type_name.name = "Int";
    field.is_mutable = false;
    MacroExpander expander;
    DecoratorContext context;

    auto result = context.apply_decorator(field, "FieldGetter", expander);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->as<meld::kernel::Symbol>()->name(), "getter_score");
}

// --- Backward compatibility: existing class-level tests still work ---

TEST_F(FieldLevelDecoratorDispatchTest, ExistingClassLevelApplyStillWorks) {
    auto dec = make_decorator("OldStyleDecorator",
        [](const class_definition& cls, MacroExpander&)
            -> std::expected<meld::kernel::Value, std::string> {
        return meld::kernel::Value(
            std::make_shared<meld::kernel::Symbol>("decorated_" + cls.name.name));
    });
    DecoratorRegistry::instance().register_decorator(dec);

    class_definition cls;
    cls.name.name = "Widget";
    MacroExpander expander;
    DecoratorContext context;

    auto result = context.apply_decorator(cls, "OldStyleDecorator", expander);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->as<meld::kernel::Symbol>()->name(), "decorated_Widget");
}

// --- Dual-mode decorator routes correctly based on target ---

TEST_F(FieldLevelDecoratorDispatchTest, DualModeRoutesCorrectlyForBothTargets) {
    std::string invoked_path;

    auto dec = make_dual_decorator("Setter",
        [&invoked_path](const class_definition& cls, MacroExpander&)
            -> std::expected<meld::kernel::Value, std::string> {
        invoked_path = "class";
        return meld::kernel::Value(std::make_shared<meld::kernel::Symbol>("class_setter"));
    },
        [&invoked_path](const field_declaration& field, MacroExpander&)
            -> std::expected<meld::kernel::Value, std::string> {
        invoked_path = "field";
        return meld::kernel::Value(std::make_shared<meld::kernel::Symbol>("field_setter"));
    });
    DecoratorRegistry::instance().register_decorator(dec);

    MacroExpander expander;
    DecoratorContext context;

    // Dispatch as ClassNode
    class_definition cls;
    cls.name.name = "Account";
    context.dispatch_decorator("Setter", DecoratorTargetKind::ClassNode,
                               &cls, nullptr, expander);
    EXPECT_EQ(invoked_path, "class");

    // Dispatch as FieldNode
    field_declaration field;
    field.name.name = "balance";
    field.type.type_name.name = "Int";
    field.is_mutable = true;
    context.dispatch_decorator("Setter", DecoratorTargetKind::FieldNode,
                               nullptr, &field, expander);
    EXPECT_EQ(invoked_path, "field");
}
