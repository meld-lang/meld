#pragma once

#include "decorator.hpp"
#include "meld/effects/effect_types.hpp"
#include "meld/parser/ast.hpp"
#include <string>
#include <memory>
#include <vector>
#include <set>
#include <expected>

namespace meld::macro {

// Imposes annotation decorator
// Transforms function definitions marked with @imposes to include effect metadata
// and validates that the function only performs declared effects
class ImposesAnnotation {
public:
    // Register the @imposes annotation with the decorator registry
    static void register_annotation();
    
    // Apply @imposes transformation to a function definition
    static std::expected<kernel::Value, std::string> 
    transform_imposes_function(const parser::ast::function_definition& func_def, 
                              const std::vector<std::string>& effect_types,
                              MacroExpander& expander);
    
    // Parse @imposes annotation parameters from AST
    static std::expected<std::vector<std::string>, std::string>
    parse_imposes_parameters(const kernel::Value& annotation_ast);
    
private:
    // Validate effect usage against @imposes declaration
    static std::expected<void, std::string> 
    validate_effect_usage(const parser::ast::function_definition& func_def,
                          const std::vector<std::string>& declared_effects);
    
    // Extract performed effect types from function body
    static std::set<std::string> 
    extract_performed_effects(const parser::ast::block_expression& body);
    
    // Generate effect metadata for function
    static kernel::Value generate_effect_metadata(
        const std::string& function_name,
        const std::vector<std::string>& effect_types
    );
    
    // Check if expression is a perform() call
    static bool is_perform_call(const parser::ast::expression& expr);
    
    // Extract effect type from perform() call
    static std::expected<std::string, std::string>
    extract_effect_from_perform(const parser::ast::expression& expr);
    
    // Recursively search for perform() calls in expressions
    static void find_perform_calls_in_expression(
        const parser::ast::expression& expr,
        std::set<std::string>& performed_effects
    );
    
    // Search for perform() calls in statements
    static void find_perform_calls_in_statement(
        const parser::ast::expression& stmt,
        std::set<std::string>& performed_effects
    );
    
    // Search for perform() calls in block expressions
    static void find_perform_calls_in_block(
        const parser::ast::block_expression& block,
        std::set<std::string>& performed_effects
    );
};

// Function effect metadata for runtime querying
struct FunctionEffectMetadata {
    std::string function_name;
    std::vector<std::string> declared_effects;
    
    FunctionEffectMetadata(std::string name, std::vector<std::string> effects)
        : function_name(std::move(name)), declared_effects(std::move(effects)) {}
};

// Registry for function effect metadata
class FunctionEffectRegistry {
public:
    // Singleton instance
    static FunctionEffectRegistry& instance() {
        static FunctionEffectRegistry registry;
        return registry;
    }
    
    // Register function effect metadata
    void register_function_effects(const std::string& function_name, 
                                  const std::vector<std::string>& effects);
    
    // Look up function effects by name
    std::expected<std::vector<std::string>, std::string> 
    get_function_effects(const std::string& function_name) const;
    
    // Check if function has effect metadata
    bool has_function_effects(const std::string& function_name) const;
    
    // Get all registered function effects
    const std::map<std::string, std::vector<std::string>>& function_effects() const {
        return function_effects_;
    }
    
    // Clear all function effects (useful for testing)
    void clear();
    
private:
    FunctionEffectRegistry() = default;
    
    std::map<std::string, std::vector<std::string>> function_effects_;
    mutable std::mutex mutex_;
};

} // namespace meld::macro