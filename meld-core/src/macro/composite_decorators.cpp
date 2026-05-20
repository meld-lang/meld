#include "meld/macro/composite_decorators.hpp"
#include "meld/kernel/operations.hpp"
#include <format>

namespace meld::macro {

std::shared_ptr<Decorator> create_data_decorator() {
    auto transformer = [](const parser::ast::class_definition& class_def,
                         MacroExpander& expander)
        -> std::expected<kernel::Value, std::string> {
        
        // @Data combines multiple decorators:
        // - Getters for all fields
        // - Setters for all mutable fields
        // - toString method
        // - equals and hashCode methods
        // - All-args constructor
        
        std::vector<kernel::Value> generated_code;
        
        // Generate getters
        for (const auto& field : class_def.fields) {
            auto getter = generate_getter(class_def.name.name, field);
            generated_code.push_back(getter);
        }
        
        // Generate setters for mutable fields
        for (const auto& field : class_def.fields) {
            if (field.is_mutable) {
                auto setter = generate_setter(class_def.name.name, field);
                generated_code.push_back(setter);
            }
        }
        
        // Generate toString
        auto to_string = generate_to_string(class_def.name.name, class_def.fields);
        generated_code.push_back(to_string);
        
        // Generate equals
        auto equals = generate_equals(class_def.name.name, class_def.fields);
        generated_code.push_back(equals);
        
        // Generate hashCode
        auto hash_code = generate_hash_code(class_def.name.name, class_def.fields);
        generated_code.push_back(hash_code);
        
        // Generate all-args constructor
        auto constructor = generate_all_args_constructor(class_def.name.name, class_def.fields);
        generated_code.push_back(constructor);
        
        // Return list of all generated code
        return kernel::list(generated_code);
    };
    
    return make_decorator("Data", transformer);
}

std::shared_ptr<Decorator> create_value_decorator() {
    auto transformer = [](const parser::ast::class_definition& class_def,
                         MacroExpander& expander)
        -> std::expected<kernel::Value, std::string> {
        
        // @Value combines multiple decorators for immutable types:
        // - Getters for all fields
        // - toString method
        // - equals and hashCode methods
        // - All-args constructor
        // Note: No setters since @Value is for immutable types
        
        std::vector<kernel::Value> generated_code;
        
        // Generate getters
        for (const auto& field : class_def.fields) {
            auto getter = generate_getter(class_def.name.name, field);
            generated_code.push_back(getter);
        }
        
        // Generate toString
        auto to_string = generate_to_string(class_def.name.name, class_def.fields);
        generated_code.push_back(to_string);
        
        // Generate equals
        auto equals = generate_equals(class_def.name.name, class_def.fields);
        generated_code.push_back(equals);
        
        // Generate hashCode
        auto hash_code = generate_hash_code(class_def.name.name, class_def.fields);
        generated_code.push_back(hash_code);
        
        // Generate all-args constructor
        auto constructor = generate_all_args_constructor(class_def.name.name, class_def.fields);
        generated_code.push_back(constructor);
        
        // Generate copy method (useful for immutable types)
        auto copy = generate_copy(class_def.name.name, class_def.fields);
        generated_code.push_back(copy);
        
        // Return list of all generated code
        return kernel::list(generated_code);
    };
    
    return make_decorator("Value", transformer);
}

void register_composite_decorators() {
    auto& registry = DecoratorRegistry::instance();
    
    registry.register_decorator(create_data_decorator());
    registry.register_decorator(create_value_decorator());
}

} // namespace meld::macro

