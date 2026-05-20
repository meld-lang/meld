#include "meld/macro/decorators.hpp"
#include "meld/macro/imposes_annotation.hpp"
#include "meld/macro/effect_annotation.hpp"
#include <algorithm>

namespace meld::macro {

void register_all_decorators() {
    // Register all decorator categories
    register_property_decorators();
    register_method_decorators();
    register_constructor_decorators();
    register_composite_decorators();
    register_builder_decorator();
    register_format_decorators();
    
    // Register effect-related annotations
    EffectAnnotation::register_annotation();
    ImposesAnnotation::register_annotation();
}

bool all_decorators_registered() {
    const auto& registry = DecoratorRegistry::instance();
    
    // Check for all required decorators from Requirement 25
    std::vector<std::string> required_decorators = {
        // Property decorators (Requirement 25.1)
        "Getter",
        "Setter",
        
        // Method generation decorators (Requirement 25.2)
        "ToString",
        "EqualsAndHashCode",
        
        // Constructor decorators (Requirement 25.3)
        "NoArgsConstructor",
        "RequiredArgsConstructor",
        "AllArgsConstructor",
        
        // Composite decorators (Requirement 25.4, 25.5)
        "Data",
        "Value",
        
        // Builder decorator (Requirement 25.6)
        "Builder",
        
        // Format decorators (Task 55.4, 55.5)
        "debug",
        "stringify"
    };
    
    // Check if all required decorators are registered
    for (const auto& decorator_name : required_decorators) {
        if (!registry.has_decorator(decorator_name)) {
            return false;
        }
    }
    
    return true;
}

std::vector<std::string> get_registered_decorator_names() {
    const auto& registry = DecoratorRegistry::instance();
    const auto& decorators = registry.decorators();
    
    std::vector<std::string> names;
    names.reserve(decorators.size());
    
    for (const auto& [name, decorator] : decorators) {
        names.push_back(name);
    }
    
    // Sort for consistent output
    std::sort(names.begin(), names.end());
    
    return names;
}

} // namespace meld::macro
