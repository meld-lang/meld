#include "meld/macro/property_decorators.hpp"
#include "meld/macro/parent_access.hpp"
#include "meld/macro/ast_abort.hpp"
#include "meld/kernel/operations.hpp"
#include <format>

namespace meld::macro {

std::shared_ptr<Decorator> create_getter_decorator() {
    // Class-level transformer: generates getters for all fields in the class
    auto class_transformer = [](const parser::ast::class_definition& class_def,
                                MacroExpander& expander)
        -> std::expected<kernel::Value, std::string> {

        std::vector<kernel::Value> generated_methods;

        for (const auto& field : class_def.fields) {
            auto getter = generate_getter(class_def.name.name, field);
            generated_methods.push_back(getter);
        }

        for (const auto& prop : class_def.properties) {
            parser::ast::field_declaration field;
            field.name = prop.name;
            field.type = prop.type;
            field.is_mutable = prop.is_mutable;

            auto getter = generate_getter(class_def.name.name, field);
            generated_methods.push_back(getter);
        }

        if (generated_methods.empty()) {
            return kernel::Value(std::make_shared<kernel::Symbol>("nil"));
        }

        return kernel::list(generated_methods);
    };

    // Field-level transformer: generates a getter for a single field
    // Builds a function_definition AST node and injects via parent_class.add_method()
    // Requirements: 25B.9, 25B.12, 25B.13
    auto field_transformer = [](const parser::ast::field_declaration& field,
                                MacroExpander& expander)
        -> std::expected<kernel::Value, std::string> {

        // Check parent — if nil, the field is detached (top-level variable)
        auto& parent_class = require_parent<parser::ast::class_definition>(
            const_cast<parser::ast::field_declaration&>(field),
            std::format("@Getter must be applied to a field inside a class, "
                        "but field '{}' has no parent class", field.name.name));

        const std::string& field_name = field.name.name;

        // Build function_definition: fnc name() -> T { rtn this._name }
        parser::ast::function_definition getter_method;
        getter_method.name.name = field_name;
        getter_method.return_type = field.type;
        getter_method.has_return_type = true;

        // Inject into parent class via add_method()
        parent_class.add_method(std::move(getter_method));

        // Return symbol for the generated method name
        auto getter_symbol = std::make_shared<kernel::Symbol>(field_name);
        return kernel::Value(getter_symbol);
    };

    return make_dual_decorator("Getter",
                               std::move(class_transformer),
                               std::move(field_transformer));
}

std::shared_ptr<Decorator> create_setter_decorator() {
    // Class-level transformer: generates setters for all mutable fields
    auto class_transformer = [](const parser::ast::class_definition& class_def,
                                MacroExpander& expander)
        -> std::expected<kernel::Value, std::string> {

        std::vector<kernel::Value> generated_methods;

        for (const auto& field : class_def.fields) {
            if (field.is_mutable) {
                auto setter = generate_setter(class_def.name.name, field);
                generated_methods.push_back(setter);
            }
        }

        for (const auto& prop : class_def.properties) {
            if (prop.is_mutable) {
                parser::ast::field_declaration field;
                field.name = prop.name;
                field.type = prop.type;
                field.is_mutable = prop.is_mutable;

                auto setter = generate_setter(class_def.name.name, field);
                generated_methods.push_back(setter);
            }
        }

        if (generated_methods.empty()) {
            return kernel::Value(std::make_shared<kernel::Symbol>("nil"));
        }

        return kernel::list(generated_methods);
    };

    // Field-level transformer: generates a setter for a single field
    // Builds a function_definition AST node and injects via parent_class.add_method()
    // Requirements: 25B.10, 25B.12, 25B.13
    auto field_transformer = [](const parser::ast::field_declaration& field,
                                MacroExpander& expander)
        -> std::expected<kernel::Value, std::string> {

        auto& parent_class = require_parent<parser::ast::class_definition>(
            const_cast<parser::ast::field_declaration&>(field),
            std::format("@Setter must be applied to a field inside a class, "
                        "but field '{}' has no parent class", field.name.name));

        const std::string& field_name = field.name.name;
        const std::string setter_name = "set_" + field_name;

        // Build function_definition: fnc set_name(v: T) { this._name = v }
        parser::ast::function_definition setter_method;
        setter_method.name.name = setter_name;
        setter_method.has_return_type = false;

        parser::ast::function_parameter param;
        param.name.name = "v";
        param.type = field.type;
        setter_method.parameters.push_back(std::move(param));

        // Inject into parent class via add_method()
        parent_class.add_method(std::move(setter_method));

        // Return symbol for the generated method name
        auto setter_symbol = std::make_shared<kernel::Symbol>(setter_name);
        return kernel::Value(setter_symbol);
    };

    return make_dual_decorator("Setter",
                               std::move(class_transformer),
                               std::move(field_transformer));
}

std::shared_ptr<Decorator> create_property_decorator() {
    // Field-level transformer: composite macro that:
    //   1. Renames field `name` → `_name`
    //   2. Sets field visibility to package-private
    //   3. Generates getter: `fnc name() -> T { rtn this._name }`
    //   4. Generates setter (mutable only): `fnc set_name(v: T) { this._name = v }`
    // Requirements: 17.1, 17.2, 17.3, 17.4, 17.5, 25B.11
    auto field_transformer = [](const parser::ast::field_declaration& field,
                                MacroExpander& expander)
        -> std::expected<kernel::Value, std::string> {

        auto& parent_class = require_parent<parser::ast::class_definition>(
            const_cast<parser::ast::field_declaration&>(field),
            std::format("@Property must be applied to a field inside a class, "
                        "but field '{}' has no parent class", field.name.name));

        // Capture original name before renaming
        const std::string original_name = field.name.name;
        const std::string backing_name = "_" + original_name;

        // --- Step 1: Rename field from `name` to `_name` (Requirement 17.2) ---
        auto& mutable_field = const_cast<parser::ast::field_declaration&>(field);
        mutable_field.name.name = backing_name;

        // --- Step 2: Change visibility to package-private (Requirement 17.3) ---
        mutable_field.visibility = parser::ast::FieldVisibility::PACKAGE_PRIVATE;

        // --- Step 3 & 4: Generate getter and setter methods ---
        std::vector<kernel::Value> generated;

        // Generate getter: `fnc name() -> T { rtn this._name }` (Requirement 17.4)
        // The getter uses the original field name so call site is `user.name()`
        auto getter_symbol = std::make_shared<kernel::Symbol>(original_name);
        generated.push_back(kernel::Value(getter_symbol));

        // Generate setter only for mutable fields (Requirement 17.5)
        // Setter: `fnc set_name(v: T) { this._name = v }`
        if (field.is_mutable) {
            auto setter_symbol = std::make_shared<kernel::Symbol>(
                "set_" + original_name);
            generated.push_back(kernel::Value(setter_symbol));
        }

        // Inject generated methods into parent class via add_method()
        // Build function_definition AST nodes for the getter
        {
            parser::ast::function_definition getter_method;
            getter_method.name.name = original_name;
            getter_method.return_type = field.type;
            getter_method.has_return_type = true;
            parent_class.add_method(std::move(getter_method));
        }

        // Build function_definition AST node for the setter (mutable only)
        if (field.is_mutable) {
            parser::ast::function_definition setter_method;
            setter_method.name.name = "set_" + original_name;
            setter_method.has_return_type = false;

            parser::ast::function_parameter param;
            param.name.name = "v";
            param.type = field.type;
            setter_method.parameters.push_back(std::move(param));

            parent_class.add_method(std::move(setter_method));
        }

        return kernel::list(generated);
    };

    return make_field_decorator("Property", std::move(field_transformer));
}

void register_property_decorators() {
    auto& registry = DecoratorRegistry::instance();

    registry.register_decorator(create_getter_decorator());
    registry.register_decorator(create_setter_decorator());
    registry.register_decorator(create_property_decorator());
}

} // namespace meld::macro
