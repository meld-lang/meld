/**
 * Tests for @debug and @stringify format decorators (Task 55.4, 55.5).
 *
 * Validates that the decorators are registered and generate correct
 * method representations for struct/class types.
 */

#include <gtest/gtest.h>
#include <meld/macro/format_decorators.hpp>
#include <meld/macro/decorator.hpp>
#include <meld/kernel/primitives.hpp>
#include <meld/parser/ast.hpp>
#include <string>

using namespace meld::macro;
using namespace meld::kernel;
using namespace meld::parser::ast;

namespace {

// Helper: create a simple class_definition with fields
class_definition make_class(const std::string& name,
                            std::vector<std::pair<std::string, std::string>> field_specs) {
    class_definition def;
    def.name.name = name;
    for (const auto& [fname, ftype] : field_specs) {
        field_declaration field;
        field.name.name = fname;
        field.type.type_name.name = ftype;
        field.is_mutable = false;
        field.has_explicit_val = true;
        def.fields.push_back(field);
    }
    return def;
}

// Helper: extract symbol name from a cons cell
std::string get_symbol_name(const Value& val) {
    if (auto sym = val.as<Symbol>()) {
        return sym->name();
    }
    return "";
}

// Helper: car/cdr accessors for cons cells
Value car(const Value& val) {
    if (auto cons = val.as<Cons>()) {
        return cons->car();
    }
    return Value(Empty::instance());
}

Value cdr(const Value& val) {
    if (auto cons = val.as<Cons>()) {
        return cons->cdr();
    }
    return Value(Empty::instance());
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

TEST(FormatDecoratorsTest, DebugDecoratorRegistered) {
    register_format_decorators();
    auto& registry = DecoratorRegistry::instance();
    EXPECT_TRUE(registry.has_decorator("debug"));
}

TEST(FormatDecoratorsTest, ToStringDecoratorRegistered) {
    register_format_decorators();
    auto& registry = DecoratorRegistry::instance();
    EXPECT_TRUE(registry.has_decorator("stringify"));
}

// ---------------------------------------------------------------------------
// generate_debug
// ---------------------------------------------------------------------------

TEST(FormatDecoratorsTest, GenerateDebugSingleField) {
    auto def = make_class("Point", {{"x", "int"}, {"y", "int"}});
    auto result = generate_debug("Point", def.fields);

    // Result is a cons cell: (debug string template)
    auto method_name = get_symbol_name(car(result));
    EXPECT_EQ(method_name, "debug");

    // The template should contain the class name and field references
    auto return_type = get_symbol_name(car(cdr(result)));
    EXPECT_EQ(return_type, "string");

    auto tmpl = get_symbol_name(car(cdr(cdr(result))));
    EXPECT_NE(tmpl.find("Point"), std::string::npos);
    EXPECT_NE(tmpl.find("${:debug self.x}"), std::string::npos);
    EXPECT_NE(tmpl.find("${:debug self.y}"), std::string::npos);
}

TEST(FormatDecoratorsTest, GenerateDebugNoFields) {
    auto def = make_class("Empty", {});
    auto result = generate_debug("Empty", def.fields);

    auto tmpl = get_symbol_name(car(cdr(cdr(result))));
    EXPECT_NE(tmpl.find("Empty"), std::string::npos);
}

// ---------------------------------------------------------------------------
// generate_pretty
// ---------------------------------------------------------------------------

TEST(FormatDecoratorsTest, GeneratePrettyMultiline) {
    auto def = make_class("User", {{"name", "string"}, {"age", "int"}});
    auto result = generate_pretty("User", def.fields);

    auto method_name = get_symbol_name(car(result));
    EXPECT_EQ(method_name, "pretty");

    auto tmpl = get_symbol_name(car(cdr(cdr(result))));
    EXPECT_NE(tmpl.find("User"), std::string::npos);
    EXPECT_NE(tmpl.find("${:pretty self.name}"), std::string::npos);
    EXPECT_NE(tmpl.find("${:pretty self.age}"), std::string::npos);
    // Should contain newlines for multi-line output
    EXPECT_NE(tmpl.find("\n"), std::string::npos);
}

// ---------------------------------------------------------------------------
// generate_meld_to_string
// ---------------------------------------------------------------------------

TEST(FormatDecoratorsTest, GenerateToStringSingleField) {
    auto def = make_class("Wrapper", {{"value", "int"}});
    auto result = generate_meld_to_string("Wrapper", def.fields);

    auto method_name = get_symbol_name(car(result));
    EXPECT_EQ(method_name, "to-string");

    auto tmpl = get_symbol_name(car(cdr(cdr(result))));
    EXPECT_NE(tmpl.find("${self.value}"), std::string::npos);
}

TEST(FormatDecoratorsTest, GenerateToStringMultipleFields) {
    auto def = make_class("User", {{"name", "string"}, {"age", "int"}});
    auto result = generate_meld_to_string("User", def.fields);

    auto tmpl = get_symbol_name(car(cdr(cdr(result))));
    // First field as primary, rest in parens
    EXPECT_NE(tmpl.find("${self.name}"), std::string::npos);
    EXPECT_NE(tmpl.find("${self.age}"), std::string::npos);
    EXPECT_NE(tmpl.find("("), std::string::npos);
}

TEST(FormatDecoratorsTest, GenerateToStringNoFields) {
    auto def = make_class("Empty", {});
    auto result = generate_meld_to_string("Empty", def.fields);

    auto tmpl = get_symbol_name(car(cdr(cdr(result))));
    EXPECT_EQ(tmpl, "Empty");
}
