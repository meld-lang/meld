#include "meld/macro/builder_decorator.hpp"
#include "meld/kernel/operations.hpp"
#include <format>

namespace meld::macro {

std::shared_ptr<Decorator> create_builder_decorator() {
    auto transformer = [](const parser::ast::class_definition& class_def,
                         MacroExpander& expander)
        -> std::expected<kernel::Value, std::string> {
        
        // Generate a builder class with fluent API
        auto builder = generate_builder(class_def.name.name, class_def.fields);
        
        return builder;
    };
    
    return make_decorator("Builder", transformer);
}

void register_builder_decorator() {
    auto& registry = DecoratorRegistry::instance();
    
    registry.register_decorator(create_builder_decorator());
}

} // namespace meld::macro

