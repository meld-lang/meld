#include "meld/macro/method_decorators.hpp"
#include "meld/kernel/operations.hpp"
#include <format>

namespace meld::macro {

std::shared_ptr<Decorator> create_to_string_decorator() {
    auto transformer = [](const parser::ast::class_definition& class_def,
                         MacroExpander& expander)
        -> std::expected<kernel::Value, std::string> {
        
        // Generate toString method
        auto to_string = generate_to_string(class_def.name.name, class_def.fields);
        
        // Return the generated method
        return to_string;
    };
    
    return make_decorator("ToString", transformer);
}

std::shared_ptr<Decorator> create_equals_and_hash_code_decorator() {
    auto transformer = [](const parser::ast::class_definition& class_def,
                         MacroExpander& expander)
        -> std::expected<kernel::Value, std::string> {
        
        // Generate both equals and hashCode methods
        std::vector<kernel::Value> generated_methods;
        
        auto equals = generate_equals(class_def.name.name, class_def.fields);
        generated_methods.push_back(equals);
        
        auto hash_code = generate_hash_code(class_def.name.name, class_def.fields);
        generated_methods.push_back(hash_code);
        
        // Return a list of both methods
        return kernel::list(generated_methods);
    };
    
    return make_decorator("EqualsAndHashCode", transformer);
}

void register_method_decorators() {
    auto& registry = DecoratorRegistry::instance();
    
    registry.register_decorator(create_to_string_decorator());
    registry.register_decorator(create_equals_and_hash_code_decorator());
}

} // namespace meld::macro

