#pragma once

#include "macro.hpp"
#include "meld/parser/ast.hpp"
#include <string>
#include <memory>
#include <vector>
#include <map>
#include <expected>
#include <functional>

namespace meld::macro {

// Forward declarations
class DecoratorRegistry;

/**
 * DecoratorTargetKind — Identifies the AST node kind a decorator is applied to.
 *
 * The decorator dispatch system inspects this to route to the correct
 * macro implementation path:
 *   - ClassNode  → class-level macro (top-down, iterates all fields)
 *   - FieldNode  → field-level macro (bottom-up, targets one field, uses .parent())
 *
 * Requirements: 25B.8
 */
enum class DecoratorTargetKind {
    ClassNode,   // Decorator applied to a class_definition
    FieldNode    // Decorator applied to a field_declaration
};

// --- Transformer function types ---

// Class-level transformer: receives the whole ClassNode (existing behavior)
using DecoratorTransformer = std::function<std::expected<kernel::Value, std::string>(
    const parser::ast::class_definition& class_def,
    MacroExpander& expander
)>;

// Field-level transformer: receives a single FieldNode (new for 7C.1)
// The macro implementation uses field.parent() to navigate to the enclosing class.
// Requirements: 25B.8
using FieldDecoratorTransformer = std::function<std::expected<kernel::Value, std::string>(
    const parser::ast::field_declaration& field,
    MacroExpander& expander
)>;

// Decorator definition
class Decorator {
public:
    // Construct a class-level-only decorator (backward compatible)
    Decorator(std::string name, DecoratorTransformer transformer)
        : name_(std::move(name))
        , class_transformer_(std::move(transformer)) {}

    // Construct a field-level-only decorator
    Decorator(std::string name, FieldDecoratorTransformer field_transformer)
        : name_(std::move(name))
        , field_transformer_(std::move(field_transformer)) {}

    // Construct a dual-mode decorator (supports both class-level and field-level)
    Decorator(std::string name,
              DecoratorTransformer class_transformer,
              FieldDecoratorTransformer field_transformer)
        : name_(std::move(name))
        , class_transformer_(std::move(class_transformer))
        , field_transformer_(std::move(field_transformer)) {}
    
    const std::string& name() const { return name_; }

    // Query which target kinds this decorator supports
    bool supports_class_target() const { return class_transformer_ != nullptr; }
    bool supports_field_target() const { return field_transformer_ != nullptr; }
    
    // Apply decorator transformation to a class definition (class-level path)
    std::expected<kernel::Value, std::string> 
    apply(const parser::ast::class_definition& class_def, MacroExpander& expander) const;

    // Apply decorator transformation to a field declaration (field-level path)
    // Requirements: 25B.8
    std::expected<kernel::Value, std::string>
    apply_to_field(const parser::ast::field_declaration& field, MacroExpander& expander) const;
    
private:
    std::string name_;
    DecoratorTransformer class_transformer_;
    FieldDecoratorTransformer field_transformer_;
};

// Decorator registry - stores and looks up decorators
class DecoratorRegistry {
public:
    static DecoratorRegistry& instance() {
        static DecoratorRegistry registry;
        return registry;
    }
    
    // Register a decorator
    void register_decorator(std::shared_ptr<Decorator> decorator);
    
    // Look up a decorator by name
    std::expected<std::shared_ptr<Decorator>, std::string> 
    get_decorator(const std::string& name) const;
    
    // Check if a decorator exists
    bool has_decorator(const std::string& name) const;
    
    // Get all registered decorators
    const std::map<std::string, std::shared_ptr<Decorator>>& decorators() const {
        return decorators_;
    }
    
    // Clear all decorators (useful for testing)
    void clear();
    
private:
    DecoratorRegistry() = default;
    
    std::map<std::string, std::shared_ptr<Decorator>> decorators_;
    mutable std::mutex mutex_;
};

// Decorator application context
// Tracks which decorators are applied to a class or field and manages their application.
// Implements dispatch logic that routes to class-level or field-level macro paths
// based on the target node kind.
// Requirements: 25B.8
class DecoratorContext {
public:
    DecoratorContext() = default;
    
    // Parse decorator annotations from class definition
    // Looks for @DecoratorName annotations in comments or metadata
    std::vector<std::string> parse_decorators(const parser::ast::class_definition& class_def);
    
    // Apply all decorators to a class definition (class-level path)
    std::expected<kernel::Value, std::string> 
    apply_decorators(const parser::ast::class_definition& class_def, 
                     const std::vector<std::string>& decorator_names,
                     MacroExpander& expander);
    
    // Apply a single decorator to a class definition (class-level path)
    std::expected<kernel::Value, std::string> 
    apply_decorator(const parser::ast::class_definition& class_def,
                    const std::string& decorator_name,
                    MacroExpander& expander);

    // Apply a single decorator to a field declaration (field-level path)
    // Dispatches to the field-level transformer if the decorator supports it.
    // Requirements: 25B.8
    std::expected<kernel::Value, std::string>
    apply_decorator(const parser::ast::field_declaration& field,
                    const std::string& decorator_name,
                    MacroExpander& expander);

    /**
     * Dispatch a decorator to the correct implementation path based on target kind.
     *
     * When target_kind == ClassNode: routes to class-level macro (top-down).
     * When target_kind == FieldNode: routes to field-level macro (bottom-up).
     *
     * Returns an error if the decorator does not support the given target kind.
     * Requirements: 25B.8
     */
    std::expected<kernel::Value, std::string>
    dispatch_decorator(const std::string& decorator_name,
                       DecoratorTargetKind target_kind,
                       const parser::ast::class_definition* class_def,
                       const parser::ast::field_declaration* field,
                       MacroExpander& expander);
};

// Helper functions for creating decorators

// Create a class-level decorator from a transformer function
std::shared_ptr<Decorator> make_decorator(
    std::string name,
    DecoratorTransformer transformer
);

// Create a field-level decorator from a field transformer function
// Requirements: 25B.8
std::shared_ptr<Decorator> make_field_decorator(
    std::string name,
    FieldDecoratorTransformer field_transformer
);

// Create a dual-mode decorator that supports both class-level and field-level targets
// Requirements: 25B.8
std::shared_ptr<Decorator> make_dual_decorator(
    std::string name,
    DecoratorTransformer class_transformer,
    FieldDecoratorTransformer field_transformer
);

// Helper functions for code generation

// Generate a getter method for a field
kernel::Value generate_getter(
    const std::string& class_name,
    const parser::ast::field_declaration& field
);

// Generate a setter method for a field
kernel::Value generate_setter(
    const std::string& class_name,
    const parser::ast::field_declaration& field
);

// Generate a toString method
kernel::Value generate_to_string(
    const std::string& class_name,
    const std::vector<parser::ast::field_declaration>& fields
);

// Generate an equals method
kernel::Value generate_equals(
    const std::string& class_name,
    const std::vector<parser::ast::field_declaration>& fields
);

// Generate a hashCode method
kernel::Value generate_hash_code(
    const std::string& class_name,
    const std::vector<parser::ast::field_declaration>& fields
);

// Generate a copy method
kernel::Value generate_copy(
    const std::string& class_name,
    const std::vector<parser::ast::field_declaration>& fields
);

// Generate a no-args constructor
kernel::Value generate_no_args_constructor(
    const std::string& class_name
);

// Generate a constructor with required fields
kernel::Value generate_required_args_constructor(
    const std::string& class_name,
    const std::vector<parser::ast::field_declaration>& required_fields
);

// Generate a constructor with all fields
kernel::Value generate_all_args_constructor(
    const std::string& class_name,
    const std::vector<parser::ast::field_declaration>& fields
);

// Generate a builder class
kernel::Value generate_builder(
    const std::string& class_name,
    const std::vector<parser::ast::field_declaration>& fields
);

} // namespace meld::macro

