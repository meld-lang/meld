/**
 * Property-based test for @Property macro expansion correctness.
 *
 * Feature: meld-lang, Property 70: @Property Macro Expansion Correctness
 *
 * For any field `name: T` annotated with @Property inside a class,
 * the macro expansion should:
 *   (a) rename the backing field to `_name`
 *   (b) set the backing field visibility to package-private
 *   (c) generate a public getter `fnc name() -> T` returning `this._name`
 *   (d) if the field is mutable, generate a public setter
 *       `fnc set_name(v: T)` assigning `this._name = v`
 *
 * Uses rapidcheck for property-based testing.
 *
 * **Validates: Requirements 17.1, 17.2, 17.3, 17.4, 17.5, 25B.11**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "meld/macro/property_decorators.hpp"
#include "meld/macro/decorator.hpp"
#include "meld/parser/ast.hpp"
#include "meld/parser/ast_node.hpp"
#include <string>

using namespace meld::macro;
using namespace meld::parser;
using namespace meld::parser::ast;

namespace {

// ---------------------------------------------------------------------------
// Generators
// ---------------------------------------------------------------------------

/// Generate a valid Meld identifier (lowercase alpha + digits, no leading digit)
rc::Gen<std::string> genFieldName() {
    return rc::gen::map(
        rc::gen::inRange(0, 50),
        [](int n) { return "field_" + std::to_string(n); }
    );
}

/// Generate a type name
rc::Gen<std::string> genTypeName() {
    static const std::vector<std::string> types = {
        "int", "string", "float", "bool", "User", "Config", "List"
    };
    return rc::gen::map(
        rc::gen::inRange(0, static_cast<int>(types.size())),
        [&](int i) { return types[static_cast<size_t>(i)]; }
    );
}

/// Generate a class name
rc::Gen<std::string> genClassName() {
    return rc::gen::map(
        rc::gen::inRange(0, 30),
        [](int n) { return "Class_" + std::to_string(n); }
    );
}

} // anonymous namespace

// ===========================================================================
// Property 70: @Property Macro Expansion Correctness
//
// For any field with any name, type, and mutability applied with @Property
// inside a class, the macro must:
//   (a) rename field to _<original>
//   (b) set visibility to package-private
//   (c) inject getter method named <original> with correct return type
//   (d) inject setter method named set_<original> iff field is mutable
//
// **Validates: Requirements 17.1, 17.2, 17.3, 17.4, 17.5, 25B.11**
// ===========================================================================

TEST(PropertyMacroExpansionPropertyTest, PropertyMacroExpandsCorrectly) {
    rc::check("@Property renames field, sets visibility, generates getter/setter",
        []() {
            // --- Setup ---
            ASTParentMap::instance().clear();
            DecoratorRegistry::instance().clear();
            register_property_decorators();

            auto field_name = *genFieldName();
            auto type_name = *genTypeName();
            auto class_name = *genClassName();
            auto is_mutable = *rc::gen::arbitrary<bool>();

            // Build class with one field
            class_definition cls;
            cls.name.name = class_name;

            field_declaration field;
            field.name.name = field_name;
            field.type.type_name.name = type_name;
            field.is_mutable = is_mutable;
            field.visibility = FieldVisibility::DEFAULT;

            cls.fields.push_back(std::move(field));
            cls.fields.back().set_parent(&cls);

            // --- Apply @Property ---
            MacroExpander expander;
            DecoratorContext context;
            auto result = context.apply_decorator(cls.fields[0], "Property", expander);

            RC_ASSERT(result.has_value());

            // (a) Field renamed to _<original>
            std::string expected_backing = "_" + field_name;
            RC_ASSERT(cls.fields[0].name.name == expected_backing);

            // (b) Visibility changed to package-private
            RC_ASSERT(cls.fields[0].visibility == FieldVisibility::PACKAGE_PRIVATE);

            // (c) Getter method injected with original name
            RC_ASSERT(cls.has_method(field_name));
            auto* getter = cls.find_method(field_name);
            RC_ASSERT(getter != nullptr);
            RC_ASSERT(getter->name.name == field_name);
            RC_ASSERT(getter->has_return_type);
            RC_ASSERT(getter->return_type.type_name.name == type_name);

            // (d) Setter iff mutable
            std::string setter_name = "set_" + field_name;
            if (is_mutable) {
                RC_ASSERT(cls.has_method(setter_name));
                auto* setter = cls.find_method(setter_name);
                RC_ASSERT(setter != nullptr);
                RC_ASSERT(setter->name.name == setter_name);
                RC_ASSERT(setter->parameters.size() == 1);
                RC_ASSERT(setter->parameters[0].name.name == "v");
                RC_ASSERT(setter->parameters[0].type.type_name.name == type_name);
            } else {
                RC_ASSERT(!cls.has_method(setter_name));
            }
        }
    );
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
