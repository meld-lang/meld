/**
 * Tests for field-level decorator error handling.
 *
 * Validates that @Getter, @Setter, and @Property decorators call ast.abort()
 * with descriptive error messages when applied to a field with no parent class
 * (e.g., a top-level variable).
 *
 * Requirements: 25B.12
 */

#include <gtest/gtest.h>
#include "meld/macro/property_decorators.hpp"
#include "meld/macro/decorator.hpp"
#include "meld/macro/ast_abort.hpp"
#include "meld/parser/ast.hpp"
#include "meld/parser/ast_node.hpp"
#include <nlohmann/json.hpp>
#include <string>

using namespace meld::macro;
using namespace meld::parser::ast;

class FieldLevelDecoratorErrorTest : public ::testing::Test {
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

    // Helper: create a detached field (no parent — simulates top-level variable)
    field_declaration make_detached_field(const std::string& name,
                                          const std::string& type_name,
                                          bool is_mutable = true) {
        field_declaration field;
        field.name.name = name;
        field.type.type_name.name = type_name;
        field.is_mutable = is_mutable;
        // Deliberately NOT setting parent — field is detached
        return field;
    }

    // Helper: create a field inside a class (has parent)
    void make_class_with_field(class_definition& cls,
                               field_declaration& field,
                               const std::string& cls_name,
                               const std::string& field_name,
                               const std::string& field_type,
                               bool is_mutable = true) {
        cls.name.name = cls_name;
        field.name.name = field_name;
        field.type.type_name.name = field_type;
        field.is_mutable = is_mutable;
        cls.fields.push_back(field);
        cls.fields.back().set_parent(&cls);
    }
};

// =============================================================================
// @Getter on top-level field produces AstAbortError
// =============================================================================

TEST_F(FieldLevelDecoratorErrorTest, GetterOnTopLevelFieldThrowsAstAbortError) {
    auto field = make_detached_field("count", "int");
    MacroExpander expander;

    auto dec = DecoratorRegistry::instance().get_decorator("Getter");
    ASSERT_TRUE(dec.has_value());

    EXPECT_THROW((*dec)->apply_to_field(field, expander), AstAbortError);
}

TEST_F(FieldLevelDecoratorErrorTest, GetterErrorMessageIncludesDecoratorName) {
    auto field = make_detached_field("count", "int");
    MacroExpander expander;

    auto dec = DecoratorRegistry::instance().get_decorator("Getter");
    ASSERT_TRUE(dec.has_value());

    try {
        (*dec)->apply_to_field(field, expander);
        FAIL() << "apply_to_field should have thrown AstAbortError";
    } catch (const AstAbortError& e) {
        EXPECT_NE(std::string(e.abort_message()).find("@Getter"), std::string::npos)
            << "Error message should include '@Getter', got: " << e.abort_message();
    }
}

TEST_F(FieldLevelDecoratorErrorTest, GetterErrorMessageIncludesFieldName) {
    auto field = make_detached_field("count", "int");
    MacroExpander expander;

    auto dec = DecoratorRegistry::instance().get_decorator("Getter");
    ASSERT_TRUE(dec.has_value());

    try {
        (*dec)->apply_to_field(field, expander);
        FAIL() << "apply_to_field should have thrown AstAbortError";
    } catch (const AstAbortError& e) {
        EXPECT_NE(std::string(e.abort_message()).find("count"), std::string::npos)
            << "Error message should include field name 'count', got: " << e.abort_message();
    }
}

// =============================================================================
// @Setter on top-level field produces AstAbortError
// =============================================================================

TEST_F(FieldLevelDecoratorErrorTest, SetterOnTopLevelFieldThrowsAstAbortError) {
    auto field = make_detached_field("total", "int");
    MacroExpander expander;

    auto dec = DecoratorRegistry::instance().get_decorator("Setter");
    ASSERT_TRUE(dec.has_value());

    EXPECT_THROW((*dec)->apply_to_field(field, expander), AstAbortError);
}

TEST_F(FieldLevelDecoratorErrorTest, SetterErrorMessageIncludesDecoratorName) {
    auto field = make_detached_field("total", "int");
    MacroExpander expander;

    auto dec = DecoratorRegistry::instance().get_decorator("Setter");
    ASSERT_TRUE(dec.has_value());

    try {
        (*dec)->apply_to_field(field, expander);
        FAIL() << "apply_to_field should have thrown AstAbortError";
    } catch (const AstAbortError& e) {
        EXPECT_NE(std::string(e.abort_message()).find("@Setter"), std::string::npos)
            << "Error message should include '@Setter', got: " << e.abort_message();
    }
}

TEST_F(FieldLevelDecoratorErrorTest, SetterErrorMessageIncludesFieldName) {
    auto field = make_detached_field("total", "int");
    MacroExpander expander;

    auto dec = DecoratorRegistry::instance().get_decorator("Setter");
    ASSERT_TRUE(dec.has_value());

    try {
        (*dec)->apply_to_field(field, expander);
        FAIL() << "apply_to_field should have thrown AstAbortError";
    } catch (const AstAbortError& e) {
        EXPECT_NE(std::string(e.abort_message()).find("total"), std::string::npos)
            << "Error message should include field name 'total', got: " << e.abort_message();
    }
}

// =============================================================================
// @Property on top-level field produces AstAbortError
// =============================================================================

TEST_F(FieldLevelDecoratorErrorTest, PropertyOnTopLevelFieldThrowsAstAbortError) {
    auto field = make_detached_field("name", "string");
    MacroExpander expander;

    auto dec = DecoratorRegistry::instance().get_decorator("Property");
    ASSERT_TRUE(dec.has_value());

    EXPECT_THROW((*dec)->apply_to_field(field, expander), AstAbortError);
}

TEST_F(FieldLevelDecoratorErrorTest, PropertyErrorMessageIncludesDecoratorName) {
    auto field = make_detached_field("name", "string");
    MacroExpander expander;

    auto dec = DecoratorRegistry::instance().get_decorator("Property");
    ASSERT_TRUE(dec.has_value());

    try {
        (*dec)->apply_to_field(field, expander);
        FAIL() << "apply_to_field should have thrown AstAbortError";
    } catch (const AstAbortError& e) {
        EXPECT_NE(std::string(e.abort_message()).find("@Property"), std::string::npos)
            << "Error message should include '@Property', got: " << e.abort_message();
    }
}

TEST_F(FieldLevelDecoratorErrorTest, PropertyErrorMessageIncludesFieldName) {
    auto field = make_detached_field("name", "string");
    MacroExpander expander;

    auto dec = DecoratorRegistry::instance().get_decorator("Property");
    ASSERT_TRUE(dec.has_value());

    try {
        (*dec)->apply_to_field(field, expander);
        FAIL() << "apply_to_field should have thrown AstAbortError";
    } catch (const AstAbortError& e) {
        EXPECT_NE(std::string(e.abort_message()).find("name"), std::string::npos)
            << "Error message should include field name 'name', got: " << e.abort_message();
    }
}

// =============================================================================
// Error messages mention "class" context requirement
// =============================================================================

TEST_F(FieldLevelDecoratorErrorTest, GetterErrorMentionsClassContext) {
    auto field = make_detached_field("x", "int");
    MacroExpander expander;

    auto dec = DecoratorRegistry::instance().get_decorator("Getter");
    try {
        (*dec)->apply_to_field(field, expander);
        FAIL();
    } catch (const AstAbortError& e) {
        std::string msg = e.abort_message();
        EXPECT_NE(msg.find("class"), std::string::npos)
            << "Error should mention 'class', got: " << msg;
    }
}

TEST_F(FieldLevelDecoratorErrorTest, SetterErrorMentionsClassContext) {
    auto field = make_detached_field("y", "int");
    MacroExpander expander;

    auto dec = DecoratorRegistry::instance().get_decorator("Setter");
    try {
        (*dec)->apply_to_field(field, expander);
        FAIL();
    } catch (const AstAbortError& e) {
        std::string msg = e.abort_message();
        EXPECT_NE(msg.find("class"), std::string::npos)
            << "Error should mention 'class', got: " << msg;
    }
}

TEST_F(FieldLevelDecoratorErrorTest, PropertyErrorMentionsClassContext) {
    auto field = make_detached_field("z", "int");
    MacroExpander expander;

    auto dec = DecoratorRegistry::instance().get_decorator("Property");
    try {
        (*dec)->apply_to_field(field, expander);
        FAIL();
    } catch (const AstAbortError& e) {
        std::string msg = e.abort_message();
        EXPECT_NE(msg.find("class"), std::string::npos)
            << "Error should mention 'class', got: " << msg;
    }
}

// =============================================================================
// CAP integration — AstAbortError produces structured JSON
// =============================================================================

TEST_F(FieldLevelDecoratorErrorTest, GetterErrorProducesValidCapJson) {
    auto field = make_detached_field("score", "int");
    MacroExpander expander;

    auto dec = DecoratorRegistry::instance().get_decorator("Getter");
    try {
        (*dec)->apply_to_field(field, expander);
        FAIL();
    } catch (const AstAbortError& e) {
        std::string json_str = e.to_cap_json();
        auto j = nlohmann::json::parse(json_str);

        EXPECT_EQ(j["severity"], "error");
        EXPECT_EQ(j["code"], "E_MACRO_ABORT");
        EXPECT_NE(j["message"].get<std::string>().find("@Getter"), std::string::npos);
    }
}

TEST_F(FieldLevelDecoratorErrorTest, SetterErrorProducesValidCapJson) {
    auto field = make_detached_field("score", "int");
    MacroExpander expander;

    auto dec = DecoratorRegistry::instance().get_decorator("Setter");
    try {
        (*dec)->apply_to_field(field, expander);
        FAIL();
    } catch (const AstAbortError& e) {
        std::string json_str = e.to_cap_json();
        auto j = nlohmann::json::parse(json_str);

        EXPECT_EQ(j["severity"], "error");
        EXPECT_EQ(j["code"], "E_MACRO_ABORT");
        EXPECT_NE(j["message"].get<std::string>().find("@Setter"), std::string::npos);
    }
}

TEST_F(FieldLevelDecoratorErrorTest, PropertyErrorProducesValidCapJson) {
    auto field = make_detached_field("score", "int");
    MacroExpander expander;

    auto dec = DecoratorRegistry::instance().get_decorator("Property");
    try {
        (*dec)->apply_to_field(field, expander);
        FAIL();
    } catch (const AstAbortError& e) {
        std::string json_str = e.to_cap_json();
        auto j = nlohmann::json::parse(json_str);

        EXPECT_EQ(j["severity"], "error");
        EXPECT_EQ(j["code"], "E_MACRO_ABORT");
        EXPECT_NE(j["message"].get<std::string>().find("@Property"), std::string::npos);
    }
}

// =============================================================================
// AstAbortError is catchable as std::runtime_error
// =============================================================================

TEST_F(FieldLevelDecoratorErrorTest, ErrorIsCatchableAsRuntimeError) {
    auto field = make_detached_field("x", "int");
    MacroExpander expander;

    auto dec = DecoratorRegistry::instance().get_decorator("Getter");
    EXPECT_THROW((*dec)->apply_to_field(field, expander), std::runtime_error);
}

// =============================================================================
// Positive case: field inside a class does NOT throw
// =============================================================================

TEST_F(FieldLevelDecoratorErrorTest, GetterOnClassFieldSucceeds) {
    class_definition cls;
    field_declaration field;
    make_class_with_field(cls, field, "User", "email", "string");
    MacroExpander expander;

    auto dec = DecoratorRegistry::instance().get_decorator("Getter");
    ASSERT_TRUE(dec.has_value());

    auto result = (*dec)->apply_to_field(cls.fields[0], expander);
    EXPECT_TRUE(result.has_value())
        << "Getter on class field should succeed, got error: " << result.error();
}

TEST_F(FieldLevelDecoratorErrorTest, SetterOnClassFieldSucceeds) {
    class_definition cls;
    field_declaration field;
    make_class_with_field(cls, field, "User", "email", "string", /*is_mutable=*/true);
    MacroExpander expander;

    auto dec = DecoratorRegistry::instance().get_decorator("Setter");
    ASSERT_TRUE(dec.has_value());

    auto result = (*dec)->apply_to_field(cls.fields[0], expander);
    EXPECT_TRUE(result.has_value())
        << "Setter on class field should succeed, got error: " << result.error();
}

TEST_F(FieldLevelDecoratorErrorTest, PropertyOnClassFieldSucceeds) {
    class_definition cls;
    field_declaration field;
    make_class_with_field(cls, field, "User", "email", "string", /*is_mutable=*/true);
    MacroExpander expander;

    auto dec = DecoratorRegistry::instance().get_decorator("Property");
    ASSERT_TRUE(dec.has_value());

    auto result = (*dec)->apply_to_field(cls.fields[0], expander);
    EXPECT_TRUE(result.has_value())
        << "Property on class field should succeed, got error: " << result.error();
}

// =============================================================================
// @Property is registered as field-level only (not class-level)
// =============================================================================

TEST_F(FieldLevelDecoratorErrorTest, PropertyIsFieldLevelOnly) {
    auto dec = DecoratorRegistry::instance().get_decorator("Property");
    ASSERT_TRUE(dec.has_value());

    EXPECT_TRUE((*dec)->supports_field_target());
    EXPECT_FALSE((*dec)->supports_class_target());
}

// =============================================================================
// @Getter and @Setter are dual-mode (support both class and field level)
// =============================================================================

TEST_F(FieldLevelDecoratorErrorTest, GetterIsDualMode) {
    auto dec = DecoratorRegistry::instance().get_decorator("Getter");
    ASSERT_TRUE(dec.has_value());

    EXPECT_TRUE((*dec)->supports_class_target());
    EXPECT_TRUE((*dec)->supports_field_target());
}

TEST_F(FieldLevelDecoratorErrorTest, SetterIsDualMode) {
    auto dec = DecoratorRegistry::instance().get_decorator("Setter");
    ASSERT_TRUE(dec.has_value());

    EXPECT_TRUE((*dec)->supports_class_target());
    EXPECT_TRUE((*dec)->supports_field_target());
}

// main() provided by gtest_main
