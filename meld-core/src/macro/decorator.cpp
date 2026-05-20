#include "meld/macro/decorator.hpp"
#include "meld/kernel/operations.hpp"
#include <format>
#include <sstream>

namespace meld::macro {

// Decorator implementation
std::expected<kernel::Value, std::string> 
Decorator::apply(const parser::ast::class_definition& class_def, MacroExpander& expander) const {
    if (!class_transformer_) {
        return std::unexpected(std::format("Decorator '{}' does not support class-level application", name_));
    }
    return class_transformer_(class_def, expander);
}

// Field-level decorator application
// Requirements: 25B.8
std::expected<kernel::Value, std::string>
Decorator::apply_to_field(const parser::ast::field_declaration& field, MacroExpander& expander) const {
    if (!field_transformer_) {
        return std::unexpected(std::format("Decorator '{}' does not support field-level application", name_));
    }
    return field_transformer_(field, expander);
}

// DecoratorRegistry implementation
void DecoratorRegistry::register_decorator(std::shared_ptr<Decorator> decorator) {
    std::lock_guard<std::mutex> lock(mutex_);
    decorators_[decorator->name()] = std::move(decorator);
}

std::expected<std::shared_ptr<Decorator>, std::string> 
DecoratorRegistry::get_decorator(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = decorators_.find(name);
    if (it != decorators_.end()) {
        return it->second;
    }
    return std::unexpected(std::format("Decorator '{}' not found", name));
}

bool DecoratorRegistry::has_decorator(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return decorators_.contains(name);
}

void DecoratorRegistry::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    decorators_.clear();
}

// DecoratorContext implementation
std::vector<std::string> DecoratorContext::parse_decorators(
    const parser::ast::class_definition& class_def) {
    
    // For now, we'll use a simple approach where decorators are stored
    // in a special field or comment. In a full implementation, this would
    // parse actual @Decorator annotations from the source.
    
    // This is a placeholder - in practice, decorators would be parsed
    // from the AST or from special annotation nodes
    std::vector<std::string> decorators;
    
    // TODO: Parse actual decorator annotations when parser supports them
    
    return decorators;
}

std::expected<kernel::Value, std::string> 
DecoratorContext::apply_decorators(
    const parser::ast::class_definition& class_def,
    const std::vector<std::string>& decorator_names,
    MacroExpander& expander) {
    
    // Start with the original class definition as a kernel Value
    // For now, we'll create a simple representation
    auto current_ast = kernel::Value(std::make_shared<kernel::Symbol>(class_def.name.name));
    
    // Apply each decorator in sequence
    for (const auto& decorator_name : decorator_names) {
        auto result = apply_decorator(class_def, decorator_name, expander);
        if (!result) {
            return result;
        }
        // In a full implementation, we would merge the generated code
        // For now, we just keep the last result
        current_ast = *result;
    }
    
    return current_ast;
}

std::expected<kernel::Value, std::string> 
DecoratorContext::apply_decorator(
    const parser::ast::class_definition& class_def,
    const std::string& decorator_name,
    MacroExpander& expander) {
    
    // Look up the decorator
    auto decorator_result = DecoratorRegistry::instance().get_decorator(decorator_name);
    if (!decorator_result) {
        return std::unexpected(decorator_result.error());
    }
    
    // Apply the decorator (class-level path)
    return (*decorator_result)->apply(class_def, expander);
}

// Field-level apply_decorator overload
// Requirements: 25B.8
std::expected<kernel::Value, std::string>
DecoratorContext::apply_decorator(
    const parser::ast::field_declaration& field,
    const std::string& decorator_name,
    MacroExpander& expander) {

    // Look up the decorator
    auto decorator_result = DecoratorRegistry::instance().get_decorator(decorator_name);
    if (!decorator_result) {
        return std::unexpected(decorator_result.error());
    }

    // Apply the decorator (field-level path)
    return (*decorator_result)->apply_to_field(field, expander);
}

// Dispatch decorator based on target node kind
// Requirements: 25B.8
std::expected<kernel::Value, std::string>
DecoratorContext::dispatch_decorator(
    const std::string& decorator_name,
    DecoratorTargetKind target_kind,
    const parser::ast::class_definition* class_def,
    const parser::ast::field_declaration* field,
    MacroExpander& expander) {

    // Look up the decorator
    auto decorator_result = DecoratorRegistry::instance().get_decorator(decorator_name);
    if (!decorator_result) {
        return std::unexpected(decorator_result.error());
    }

    const auto& decorator = *decorator_result;

    switch (target_kind) {
        case DecoratorTargetKind::ClassNode: {
            if (!class_def) {
                return std::unexpected(std::format(
                    "Decorator '{}' dispatched as ClassNode but no class_definition provided",
                    decorator_name));
            }
            if (!decorator->supports_class_target()) {
                return std::unexpected(std::format(
                    "Decorator '{}' does not support class-level application",
                    decorator_name));
            }
            return decorator->apply(*class_def, expander);
        }
        case DecoratorTargetKind::FieldNode: {
            if (!field) {
                return std::unexpected(std::format(
                    "Decorator '{}' dispatched as FieldNode but no field_declaration provided",
                    decorator_name));
            }
            if (!decorator->supports_field_target()) {
                return std::unexpected(std::format(
                    "Decorator '{}' does not support field-level application",
                    decorator_name));
            }
            return decorator->apply_to_field(*field, expander);
        }
    }

    return std::unexpected(std::format("Unknown target kind for decorator '{}'", decorator_name));
}

// Helper functions
std::shared_ptr<Decorator> make_decorator(
    std::string name,
    DecoratorTransformer transformer) {
    
    return std::make_shared<Decorator>(
        std::move(name),
        std::move(transformer)
    );
}

std::shared_ptr<Decorator> make_field_decorator(
    std::string name,
    FieldDecoratorTransformer field_transformer) {

    return std::make_shared<Decorator>(
        std::move(name),
        std::move(field_transformer)
    );
}

std::shared_ptr<Decorator> make_dual_decorator(
    std::string name,
    DecoratorTransformer class_transformer,
    FieldDecoratorTransformer field_transformer) {

    return std::make_shared<Decorator>(
        std::move(name),
        std::move(class_transformer),
        std::move(field_transformer)
    );
}

// Code generation helpers

kernel::Value generate_getter(
    const std::string& class_name,
    const parser::ast::field_declaration& field) {
    
    // Generate: func get<FieldName>(): <Type> { return this.<fieldName> }
    std::string getter_name = "get" + field.name.name;
    getter_name[3] = std::toupper(getter_name[3]); // Capitalize first letter after "get"
    
    // Create a simple AST representation
    // In a full implementation, this would create proper AST nodes
    auto getter_symbol = std::make_shared<kernel::Symbol>(getter_name);
    return kernel::Value(getter_symbol);
}

kernel::Value generate_setter(
    const std::string& class_name,
    const parser::ast::field_declaration& field) {
    
    // Generate: func set<FieldName>(value: <Type>) { this.<fieldName> = value }
    std::string setter_name = "set" + field.name.name;
    setter_name[3] = std::toupper(setter_name[3]); // Capitalize first letter after "set"
    
    auto setter_symbol = std::make_shared<kernel::Symbol>(setter_name);
    return kernel::Value(setter_symbol);
}

kernel::Value generate_to_string(
    const std::string& class_name,
    const std::vector<parser::ast::field_declaration>& fields) {
    
    // Generate: func toString(): String { return "<ClassName>(<field1>=${this.<field1>}, ...)" }
    
    // Build the string template
    std::ostringstream oss;
    oss << class_name << "(";
    
    for (size_t i = 0; i < fields.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << fields[i].name.name << "=${this." << fields[i].name.name << "}";
    }
    
    oss << ")";
    
    // Create AST representation for the toString method
    // func toString(): String { return "<template>" }
    
    // For now, create a simple symbol representation
    // In a full implementation, this would create proper function AST nodes
    auto method_name = std::make_shared<kernel::Symbol>("toString");
    auto return_type = std::make_shared<kernel::Symbol>("String");
    auto template_str = std::make_shared<kernel::Symbol>(oss.str());
    
    // Create a cons cell representing the method: (toString String template)
    auto method_cons = kernel::cons(
        kernel::Value(method_name),
        kernel::cons(
            kernel::Value(return_type),
            kernel::cons(
                kernel::Value(template_str),
                kernel::Value(kernel::nil())
            )
        )
    );
    
    return method_cons;
}

kernel::Value generate_equals(
    const std::string& class_name,
    const std::vector<parser::ast::field_declaration>& fields) {
    
    // Generate: func equals(other: Any): Bool { 
    //   if (!(other is <ClassName>)) return false
    //   val other<ClassName> = other as <ClassName>
    //   return this.<field1> == other<ClassName>.<field1> && ...
    // }
    
    // Build the field comparison logic
    std::ostringstream comparison_oss;
    for (size_t i = 0; i < fields.size(); ++i) {
        if (i > 0) comparison_oss << " && ";
        comparison_oss << "this." << fields[i].name.name 
                      << " == other" << class_name << "." << fields[i].name.name;
    }
    
    // If no fields, objects are equal if they're the same type
    std::string comparison_logic = fields.empty() ? "true" : comparison_oss.str();
    
    // Create AST representation for the equals method
    auto method_name = std::make_shared<kernel::Symbol>("equals");
    auto param_name = std::make_shared<kernel::Symbol>("other");
    auto param_type = std::make_shared<kernel::Symbol>("Any");
    auto return_type = std::make_shared<kernel::Symbol>("Bool");
    auto class_name_sym = std::make_shared<kernel::Symbol>(class_name);
    auto comparison_sym = std::make_shared<kernel::Symbol>(comparison_logic);
    
    // Create a cons cell representing the method: (equals (other Any) Bool class_name comparison)
    auto method_cons = kernel::cons(
        kernel::Value(method_name),
        kernel::cons(
            kernel::cons(
                kernel::Value(param_name),
                kernel::cons(
                    kernel::Value(param_type),
                    kernel::Value(kernel::nil())
                )
            ),
            kernel::cons(
                kernel::Value(return_type),
                kernel::cons(
                    kernel::Value(class_name_sym),
                    kernel::cons(
                        kernel::Value(comparison_sym),
                        kernel::Value(kernel::nil())
                    )
                )
            )
        )
    );
    
    return method_cons;
}

kernel::Value generate_hash_code(
    const std::string& class_name,
    const std::vector<parser::ast::field_declaration>& fields) {
    
    // Generate: func hashCode(): Int {
    //   var result = 17
    //   result = 31 * result + this.<field1>.hashCode()
    //   result = 31 * result + this.<field2>.hashCode()
    //   ...
    //   return result
    // }
    
    // Build the hash code computation logic
    std::ostringstream hash_oss;
    hash_oss << "var result = 17";
    
    for (const auto& field : fields) {
        hash_oss << "; result = 31 * result + this." << field.name.name << ".hashCode()";
    }
    
    hash_oss << "; return result";
    
    // Create AST representation for the hashCode method
    auto method_name = std::make_shared<kernel::Symbol>("hashCode");
    auto return_type = std::make_shared<kernel::Symbol>("Int");
    auto hash_logic = std::make_shared<kernel::Symbol>(hash_oss.str());
    
    // Create a cons cell representing the method: (hashCode Int hash_logic)
    auto method_cons = kernel::cons(
        kernel::Value(method_name),
        kernel::cons(
            kernel::Value(return_type),
            kernel::cons(
                kernel::Value(hash_logic),
                kernel::Value(kernel::nil())
            )
        )
    );
    
    return method_cons;
}

kernel::Value generate_copy(
    const std::string& class_name,
    const std::vector<parser::ast::field_declaration>& fields) {
    
    // Generate: func copy(...): <ClassName> { ... }
    // Create a new instance with optionally modified fields
    
    auto copy_symbol = std::make_shared<kernel::Symbol>("copy");
    return kernel::Value(copy_symbol);
}

kernel::Value generate_no_args_constructor(
    const std::string& class_name) {
    
    // Generate: fnc <ClassName>(): <ClassName> { return <ClassName> { <field> = default, ... } }
    // A no-args constructor initializes all fields with type-appropriate defaults.
    
    auto method_name = std::make_shared<kernel::Symbol>(class_name);
    auto return_type = std::make_shared<kernel::Symbol>(class_name);
    auto kind = std::make_shared<kernel::Symbol>("no_args_constructor");
    
    // Encode: (constructor_name return_type kind)
    auto method_cons = kernel::cons(
        kernel::Value(method_name),
        kernel::cons(
            kernel::Value(return_type),
            kernel::cons(
                kernel::Value(kind),
                kernel::Value(kernel::nil())
            )
        )
    );
    
    return method_cons;
}

kernel::Value generate_required_args_constructor(
    const std::string& class_name,
    const std::vector<parser::ast::field_declaration>& required_fields) {
    
    // Generate: fnc <ClassName>(<required_fields>): <ClassName> { ... }
    // Only non-nullable fields without defaults become parameters.
    
    auto method_name = std::make_shared<kernel::Symbol>(class_name);
    auto return_type = std::make_shared<kernel::Symbol>(class_name);
    auto kind = std::make_shared<kernel::Symbol>("required_args_constructor");
    
    // Build parameter list as nested cons cells: ((name type) (name type) ...)
    kernel::Value params = kernel::Value(kernel::nil());
    for (auto it = required_fields.rbegin(); it != required_fields.rend(); ++it) {
        auto param_name = std::make_shared<kernel::Symbol>(it->name.name);
        auto param_type = std::make_shared<kernel::Symbol>(it->type.type_name.name);
        auto param_pair = kernel::cons(
            kernel::Value(param_name),
            kernel::cons(
                kernel::Value(param_type),
                kernel::Value(kernel::nil())
            )
        );
        params = kernel::cons(param_pair, params);
    }
    
    // Encode: (constructor_name return_type kind params)
    auto method_cons = kernel::cons(
        kernel::Value(method_name),
        kernel::cons(
            kernel::Value(return_type),
            kernel::cons(
                kernel::Value(kind),
                kernel::cons(
                    params,
                    kernel::Value(kernel::nil())
                )
            )
        )
    );
    
    return method_cons;
}

kernel::Value generate_all_args_constructor(
    const std::string& class_name,
    const std::vector<parser::ast::field_declaration>& fields) {
    
    // Generate: fnc <ClassName>(<all_fields>): <ClassName> { ... }
    // Every field becomes a constructor parameter.
    
    auto method_name = std::make_shared<kernel::Symbol>(class_name);
    auto return_type = std::make_shared<kernel::Symbol>(class_name);
    auto kind = std::make_shared<kernel::Symbol>("all_args_constructor");
    
    // Build parameter list as nested cons cells: ((name type) (name type) ...)
    kernel::Value params = kernel::Value(kernel::nil());
    for (auto it = fields.rbegin(); it != fields.rend(); ++it) {
        auto param_name = std::make_shared<kernel::Symbol>(it->name.name);
        std::string type_str = it->type.type_name.name;
        if (it->type.is_nullable) type_str += "?";
        auto param_type = std::make_shared<kernel::Symbol>(type_str);
        auto param_pair = kernel::cons(
            kernel::Value(param_name),
            kernel::cons(
                kernel::Value(param_type),
                kernel::Value(kernel::nil())
            )
        );
        params = kernel::cons(param_pair, params);
    }
    
    // Encode: (constructor_name return_type kind params)
    auto method_cons = kernel::cons(
        kernel::Value(method_name),
        kernel::cons(
            kernel::Value(return_type),
            kernel::cons(
                kernel::Value(kind),
                kernel::cons(
                    params,
                    kernel::Value(kernel::nil())
                )
            )
        )
    );
    
    return method_cons;
}

kernel::Value generate_builder(
    const std::string& class_name,
    const std::vector<parser::ast::field_declaration>& fields) {
    
    // Generate a builder class with fluent API:
    //   class <ClassName>Builder {
    //     var <field>: <Type> = default
    //     ...
    //     fnc <field>(value: <Type>): <ClassName>Builder { this.<field> = value; rtn this }
    //     ...
    //     fnc build(): <ClassName> { rtn <ClassName> { <field> = this.<field>, ... } }
    //   }
    
    std::string builder_name = class_name + "Builder";
    auto builder_sym = std::make_shared<kernel::Symbol>(builder_name);
    auto target_class_sym = std::make_shared<kernel::Symbol>(class_name);
    auto kind = std::make_shared<kernel::Symbol>("builder");
    
    // Build field list as cons cells: ((name type nullable?) ...)
    kernel::Value field_list = kernel::Value(kernel::nil());
    for (auto it = fields.rbegin(); it != fields.rend(); ++it) {
        auto fname = std::make_shared<kernel::Symbol>(it->name.name);
        std::string type_str = it->type.type_name.name;
        if (it->type.is_nullable) type_str += "?";
        auto ftype = std::make_shared<kernel::Symbol>(type_str);
        auto nullable_flag = std::make_shared<kernel::Symbol>(
            it->type.is_nullable ? "nullable" : "required"
        );
        auto field_entry = kernel::cons(
            kernel::Value(fname),
            kernel::cons(
                kernel::Value(ftype),
                kernel::cons(
                    kernel::Value(nullable_flag),
                    kernel::Value(kernel::nil())
                )
            )
        );
        field_list = kernel::cons(field_entry, field_list);
    }
    
    // Build fluent setter method names list
    kernel::Value setter_list = kernel::Value(kernel::nil());
    for (auto it = fields.rbegin(); it != fields.rend(); ++it) {
        auto setter_name = std::make_shared<kernel::Symbol>(it->name.name);
        auto setter_return = std::make_shared<kernel::Symbol>(builder_name);
        auto setter_entry = kernel::cons(
            kernel::Value(setter_name),
            kernel::cons(
                kernel::Value(setter_return),
                kernel::Value(kernel::nil())
            )
        );
        setter_list = kernel::cons(setter_entry, setter_list);
    }
    
    // Encode: (builder_name target_class kind field_list setter_list)
    auto builder_cons = kernel::cons(
        kernel::Value(builder_sym),
        kernel::cons(
            kernel::Value(target_class_sym),
            kernel::cons(
                kernel::Value(kind),
                kernel::cons(
                    field_list,
                    kernel::cons(
                        setter_list,
                        kernel::Value(kernel::nil())
                    )
                )
            )
        )
    );
    
    return builder_cons;
}

} // namespace meld::macro

