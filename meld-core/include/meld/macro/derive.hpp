#pragma once

#include "macro.hpp"
#include "decorator.hpp"
#include "meld/parser/ast.hpp"
#include <string>
#include <memory>
#include <vector>
#include <map>
#include <expected>
#include <functional>

namespace meld::macro {

// ============================================================================
// DERIVE MACRO SYSTEM
// Task 9.1: Create derive macro system
// Requirements: 6.1, 6.2
// ============================================================================

// Derive trait transformer function type
// Takes class definition and generates trait implementation
using DeriveTraitTransformer = std::function<std::expected<kernel::Value, std::string>(
    const parser::ast::class_definition& class_def,
    MacroExpander& expander
)>;

// Derive trait definition
// Represents a trait that can be automatically derived (e.g., Debug, Clone, PartialEq)
class DeriveTrait {
public:
    DeriveTrait(std::string name, DeriveTraitTransformer transformer)
        : name_(std::move(name))
        , transformer_(std::move(transformer)) {}
    
    const std::string& name() const { return name_; }
    
    // Generate trait implementation for a class
    std::expected<kernel::Value, std::string> 
    derive(const parser::ast::class_definition& class_def, MacroExpander& expander) const;
    
private:
    std::string name_;
    DeriveTraitTransformer transformer_;
};

// Derive trait registry - stores and looks up derivable traits
class DeriveTraitRegistry {
public:
    static DeriveTraitRegistry& instance() {
        static DeriveTraitRegistry registry;
        return registry;
    }
    
    // Register a derivable trait
    void register_trait(std::shared_ptr<DeriveTrait> trait);
    
    // Look up a derivable trait by name
    std::expected<std::shared_ptr<DeriveTrait>, std::string> 
    get_trait(const std::string& name) const;
    
    // Check if a trait is derivable
    bool has_trait(const std::string& name) const;
    
    // Get all registered derivable traits
    const std::map<std::string, std::shared_ptr<DeriveTrait>>& traits() const {
        return traits_;
    }
    
    // Clear all traits (useful for testing)
    void clear();
    
private:
    DeriveTraitRegistry() = default;
    
    std::map<std::string, std::shared_ptr<DeriveTrait>> traits_;
    mutable std::mutex mutex_;
};

// Derive macro implementation
// Handles @derive(Trait1, Trait2, ...) annotations
class DeriveMacro {
public:
    // Parse @derive annotation and extract trait names
    static std::vector<std::string> parse_derive_annotation(
        const parser::ast::class_definition& class_def
    );
    
    // Apply @derive macro to a class definition
    // Generates implementations for all specified traits
    static std::expected<kernel::Value, std::string> 
    apply(const parser::ast::class_definition& class_def, MacroExpander& expander);
    
    // Derive a single trait for a class
    static std::expected<kernel::Value, std::string> 
    derive_trait(const parser::ast::class_definition& class_def,
                 const std::string& trait_name,
                 MacroExpander& expander);
    
    // Derive multiple traits for a class
    static std::expected<kernel::Value, std::string> 
    derive_traits(const parser::ast::class_definition& class_def,
                  const std::vector<std::string>& trait_names,
                  MacroExpander& expander);
};

// ============================================================================
// COMMON TRAIT DERIVATIONS
// ============================================================================

// Debug trait - generates debug string representation
// Generates: func debug(): String { return "<ClassName> { field1: <value1>, ... }" }
std::expected<kernel::Value, std::string> 
derive_debug(const parser::ast::class_definition& class_def, MacroExpander& expander);

// Clone trait - generates deep copy implementation
// Generates: func clone(): <ClassName> { return <ClassName> { field1: this.field1.clone(), ... } }
std::expected<kernel::Value, std::string> 
derive_clone(const parser::ast::class_definition& class_def, MacroExpander& expander);

// PartialEq trait - generates equality comparison
// Generates: func equals(other: <ClassName>): Bool { return this.field1 == other.field1 && ... }
std::expected<kernel::Value, std::string> 
derive_partial_eq(const parser::ast::class_definition& class_def, MacroExpander& expander);

// Eq trait - generates full equality (requires PartialEq)
// Generates: marker trait implementation (no additional methods)
std::expected<kernel::Value, std::string> 
derive_eq(const parser::ast::class_definition& class_def, MacroExpander& expander);

// Hash trait - generates hash code computation
// Generates: func hash(): Int { var h = 0; h = 31 * h + field1.hash(); ...; return h }
std::expected<kernel::Value, std::string> 
derive_hash(const parser::ast::class_definition& class_def, MacroExpander& expander);

// Default trait - generates default value constructor
// Generates: func default(): <ClassName> { return <ClassName> { field1: default(), ... } }
std::expected<kernel::Value, std::string> 
derive_default(const parser::ast::class_definition& class_def, MacroExpander& expander);

// Copy trait - generates bitwise copy (for simple types)
// Generates: marker trait implementation (compiler handles copying)
std::expected<kernel::Value, std::string> 
derive_copy(const parser::ast::class_definition& class_def, MacroExpander& expander);

// Send trait - marks type as safe to send between threads
// Generates: marker trait implementation (compiler checks fields)
std::expected<kernel::Value, std::string> 
derive_send(const parser::ast::class_definition& class_def, MacroExpander& expander);

// Sync trait - marks type as safe to share between threads
// Generates: marker trait implementation (compiler checks fields)
std::expected<kernel::Value, std::string> 
derive_sync(const parser::ast::class_definition& class_def, MacroExpander& expander);

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

// Create a derivable trait
std::shared_ptr<DeriveTrait> make_derive_trait(
    std::string name,
    DeriveTraitTransformer transformer
);

// Register all standard derivable traits
void register_standard_derive_traits();

// Generate trait implementation AST
kernel::Value generate_trait_impl(
    const std::string& trait_name,
    const std::string& class_name,
    const std::vector<kernel::Value>& methods
);

// Generate method AST
kernel::Value generate_method(
    const std::string& method_name,
    const std::vector<std::pair<std::string, std::string>>& params,
    const std::string& return_type,
    const kernel::Value& body
);

// Check if all fields of a class implement a trait
bool all_fields_implement_trait(
    const parser::ast::class_definition& class_def,
    const std::string& trait_name
);

} // namespace meld::macro
