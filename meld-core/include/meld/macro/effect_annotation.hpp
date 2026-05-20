#pragma once

#include "decorator.hpp"
#include "meld/effects/effect_types.hpp"
#include "meld/parser/ast.hpp"
#include <string>
#include <memory>
#include <vector>
#include <expected>

namespace meld::macro {

// Forward declarations for types used in EffectRegistry
struct Parameter;
struct EffectOperation;
struct EffectDescriptor;

// Effect annotation decorator
// Transforms class definitions marked with @effect into effect infrastructure
// Note: In MELD, traits are represented as classes with abstract methods
class EffectAnnotation {
public:
    // Register the @effect annotation with the decorator registry
    static void register_annotation();
    
    // Apply @effect transformation to a class definition (representing a trait)
    static std::expected<kernel::Value, std::string> 
    transform_effect_class(const parser::ast::class_definition& class_def, 
                          MacroExpander& expander);
    
private:
    // Generate effect registration code
    static kernel::Value generate_effect_registration(
        const std::string& effect_name,
        const std::vector<parser::ast::function_definition>& operations
    );
    
    // Generate effect handler infrastructure
    static kernel::Value generate_effect_handler_class(
        const std::string& effect_name,
        const std::vector<parser::ast::function_definition>& operations
    );
    
    // Generate effect operation descriptors
    static kernel::Value generate_effect_operations(
        const std::vector<parser::ast::function_definition>& operations
    );
    
    // Validate that @effect is applied to trait-like class definitions only
    static std::expected<void, std::string> 
    validate_effect_class(const parser::ast::class_definition& class_def);
    
    // Extract class methods as effect operations (abstract functions)
    static std::vector<parser::ast::function_definition> 
    extract_effect_operations(const parser::ast::class_definition& class_def);
};

// Effect registry for managing effect types at runtime
class EffectRegistry {
public:
    // Singleton instance
    static EffectRegistry& instance() {
        static EffectRegistry registry;
        return registry;
    }
    
    // Register an effect type
    void register_effect(const std::string& name, 
                        const std::vector<EffectOperation>& operations);
    
    // Look up an effect by name
    std::expected<EffectDescriptor, std::string> 
    get_effect(const std::string& name) const;
    
    // Check if an effect is registered
    bool has_effect(const std::string& name) const;
    
    // Get all registered effects
    const std::map<std::string, EffectDescriptor>& effects() const {
        return effects_;
    }
    
    // Clear all effects (useful for testing)
    void clear();
    
private:
    EffectRegistry() = default;
    
    std::map<std::string, EffectDescriptor> effects_;
    mutable std::mutex mutex_;
};

// Parameter descriptor
struct Parameter {
    std::string name;
    std::string type;
    
    Parameter(std::string n, std::string t)
        : name(std::move(n)), type(std::move(t)) {}
};

// Effect operation descriptor
struct EffectOperation {
    std::string name;
    std::vector<Parameter> params;
    std::string return_type;
    
    EffectOperation(std::string n, std::vector<Parameter> p, std::string r)
        : name(std::move(n)), params(std::move(p)), return_type(std::move(r)) {}
};

// Effect descriptor
struct EffectDescriptor {
    std::string name;
    std::vector<EffectOperation> operations;
    
    EffectDescriptor() = default;
    EffectDescriptor(std::string n, std::vector<EffectOperation> ops)
        : name(std::move(n)), operations(std::move(ops)) {}
};

} // namespace meld::macro