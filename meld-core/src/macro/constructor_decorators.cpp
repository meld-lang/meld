#include "meld/macro/constructor_decorators.hpp"
#include "meld/kernel/operations.hpp"
#include <format>

namespace meld::macro {

std::shared_ptr<Decorator> create_no_args_constructor_decorator() {
    auto transformer = [](const parser::ast::class_definition& class_def,
                         MacroExpander& expander)
        -> std::expected<kernel::Value, std::string> {
        
        // Generate no-args constructor
        auto constructor = generate_no_args_constructor(class_def.name.name);
        
        return constructor;
    };
    
    return make_decorator("NoArgsConstructor", transformer);
}

std::shared_ptr<Decorator> create_required_args_constructor_decorator() {
    auto transformer = [](const parser::ast::class_definition& class_def,
                         MacroExpander& expander)
        -> std::expected<kernel::Value, std::string> {
        
        // Identify required fields (non-nullable without defaults)
        std::vector<parser::ast::field_declaration> required_fields;
        
        for (const auto& field : class_def.fields) {
            // A field is required if:
            // 1. It's not nullable (no ? in type)
            // 2. It doesn't have a default value (we'd need to track this in AST)
            // For now, we'll consider all non-nullable fields as required
            if (!field.type.is_nullable) {
                required_fields.push_back(field);
            }
        }
        
        // Generate constructor with required fields
        auto constructor = generate_required_args_constructor(
            class_def.name.name, 
            required_fields
        );
        
        return constructor;
    };
    
    return make_decorator("RequiredArgsConstructor", transformer);
}

std::shared_ptr<Decorator> create_all_args_constructor_decorator() {
    auto transformer = [](const parser::ast::class_definition& class_def,
                         MacroExpander& expander)
        -> std::expected<kernel::Value, std::string> {
        
        // Generate constructor with all fields
        auto constructor = generate_all_args_constructor(
            class_def.name.name,
            class_def.fields
        );
        
        return constructor;
    };
    
    return make_decorator("AllArgsConstructor", transformer);
}

void register_constructor_decorators() {
    auto& registry = DecoratorRegistry::instance();
    
    registry.register_decorator(create_no_args_constructor_decorator());
    registry.register_decorator(create_required_args_constructor_decorator());
    registry.register_decorator(create_all_args_constructor_decorator());
}

} // namespace meld::macro

