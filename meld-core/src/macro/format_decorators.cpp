#include "meld/macro/format_decorators.hpp"
#include "meld/kernel/operations.hpp"
#include <sstream>

namespace meld::macro {

// ---------------------------------------------------------------------------
// Code generation helpers
// ---------------------------------------------------------------------------

kernel::Value generate_debug(
    const std::string& class_name,
    const std::vector<parser::ast::field_declaration>& fields) {

    // Generate: fnc debug() -> string { rtn `ClassName { f1: ${:debug self.f1}, ... }` }
    std::ostringstream oss;
    oss << class_name << " { ";
    for (size_t i = 0; i < fields.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << fields[i].name.name << ": ${:debug self." << fields[i].name.name << "}";
    }
    oss << " }";

    auto method_name = std::make_shared<kernel::Symbol>("debug");
    auto return_type = std::make_shared<kernel::Symbol>("string");
    auto template_str = std::make_shared<kernel::Symbol>(oss.str());

    return kernel::cons(
        kernel::Value(method_name),
        kernel::cons(
            kernel::Value(return_type),
            kernel::cons(
                kernel::Value(template_str),
                kernel::Value(kernel::nil())
            )
        )
    );
}

kernel::Value generate_pretty(
    const std::string& class_name,
    const std::vector<parser::ast::field_declaration>& fields) {

    // Generate: fnc pretty() -> string { rtn pretty-print(self, indent: 0) }
    // For now, generate a multi-line debug representation
    std::ostringstream oss;
    oss << class_name << " {\n";
    for (size_t i = 0; i < fields.size(); ++i) {
        oss << "  " << fields[i].name.name << ": ${:pretty self." << fields[i].name.name << "}";
        if (i + 1 < fields.size()) oss << ",";
        oss << "\n";
    }
    oss << "}";

    auto method_name = std::make_shared<kernel::Symbol>("pretty");
    auto return_type = std::make_shared<kernel::Symbol>("string");
    auto template_str = std::make_shared<kernel::Symbol>(oss.str());

    return kernel::cons(
        kernel::Value(method_name),
        kernel::cons(
            kernel::Value(return_type),
            kernel::cons(
                kernel::Value(template_str),
                kernel::Value(kernel::nil())
            )
        )
    );
}

kernel::Value generate_meld_to_string(
    const std::string& class_name,
    const std::vector<parser::ast::field_declaration>& fields) {

    // Generate: fnc to-string() -> string { rtn `${self.f1} (${self.f2})` }
    // Simple user-facing format: first field as primary, rest in parens
    std::ostringstream oss;
    if (fields.empty()) {
        oss << class_name;
    } else if (fields.size() == 1) {
        oss << "${self." << fields[0].name.name << "}";
    } else {
        oss << "${self." << fields[0].name.name << "} (";
        for (size_t i = 1; i < fields.size(); ++i) {
            if (i > 1) oss << ", ";
            oss << "${self." << fields[i].name.name << "}";
        }
        oss << ")";
    }

    auto method_name = std::make_shared<kernel::Symbol>("to-string");
    auto return_type = std::make_shared<kernel::Symbol>("string");
    auto template_str = std::make_shared<kernel::Symbol>(oss.str());

    return kernel::cons(
        kernel::Value(method_name),
        kernel::cons(
            kernel::Value(return_type),
            kernel::cons(
                kernel::Value(template_str),
                kernel::Value(kernel::nil())
            )
        )
    );
}

// ---------------------------------------------------------------------------
// Decorator factories
// ---------------------------------------------------------------------------

std::shared_ptr<Decorator> create_debug_decorator() {
    auto transformer = [](const parser::ast::class_definition& class_def,
                         MacroExpander& expander)
        -> std::expected<kernel::Value, std::string> {

        std::vector<kernel::Value> methods;
        methods.push_back(generate_debug(class_def.name.name, class_def.fields));
        methods.push_back(generate_pretty(class_def.name.name, class_def.fields));

        return kernel::list(methods);
    };

    return make_decorator("debug", transformer);
}

std::shared_ptr<Decorator> create_stringify_decorator() {
    auto transformer = [](const parser::ast::class_definition& class_def,
                         MacroExpander& expander)
        -> std::expected<kernel::Value, std::string> {

        return generate_meld_to_string(class_def.name.name, class_def.fields);
    };

    return make_decorator("stringify", transformer);
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void register_format_decorators() {
    auto& registry = DecoratorRegistry::instance();
    registry.register_decorator(create_debug_decorator());
    registry.register_decorator(create_stringify_decorator());
}

} // namespace meld::macro
