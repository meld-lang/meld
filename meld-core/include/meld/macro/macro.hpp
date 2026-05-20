#pragma once

#include "meld/kernel/primitives.hpp"
#include "meld/parser/ast.hpp"
#include <string>
#include <memory>
#include <vector>
#include <map>
#include <expected>
#include <functional>
#include <mutex>
#include <set>

namespace meld::macro {

// Forward declarations
class MacroExpander;
class MacroRegistry;

// Macro transformer function type
// Takes AST node and returns transformed AST node
using MacroTransformer = std::function<std::expected<kernel::Value, std::string>(
    const kernel::Value& ast_node,
    MacroExpander& expander
)>;

// Macro definition
class Macro {
public:
    Macro(std::string name, 
          std::vector<std::string> params,
          MacroTransformer transformer)
        : name_(std::move(name))
        , params_(std::move(params))
        , transformer_(std::move(transformer)) {}
    
    const std::string& name() const { return name_; }
    const std::vector<std::string>& params() const { return params_; }
    
    // Apply macro transformation
    std::expected<kernel::Value, std::string> 
    apply(const kernel::Value& ast_node, MacroExpander& expander) const;
    
private:
    std::string name_;
    std::vector<std::string> params_;
    MacroTransformer transformer_;
};

// Macro registry - stores and looks up macros
class MacroRegistry {
public:
    static MacroRegistry& instance() {
        static MacroRegistry registry;
        return registry;
    }
    
    // Register a macro
    void register_macro(std::shared_ptr<Macro> macro);
    
    // Look up a macro by name
    std::expected<std::shared_ptr<Macro>, std::string> 
    get_macro(const std::string& name) const;
    
    // Check if a macro exists
    bool has_macro(const std::string& name) const;
    
    // Get all registered macros
    const std::map<std::string, std::shared_ptr<Macro>>& macros() const {
        return macros_;
    }
    
    // Clear all macros (useful for testing)
    void clear();
    
private:
    MacroRegistry() = default;
    
    std::map<std::string, std::shared_ptr<Macro>> macros_;
    mutable std::mutex mutex_;
};

// Scope for hygienic macro expansion
class MacroScope {
public:
    MacroScope() = default;
    
    // Enter a new scope
    void push_scope();
    
    // Exit current scope
    void pop_scope();
    
    // Register a symbol in current scope
    void register_symbol(const std::string& name, std::shared_ptr<kernel::Symbol> symbol);
    
    // Look up a symbol in current scope (returns nullptr if not found)
    std::shared_ptr<kernel::Symbol> lookup_symbol(const std::string& name) const;
    
    // Check if a symbol is in current scope
    bool has_symbol(const std::string& name) const;
    
private:
    std::vector<std::map<std::string, std::shared_ptr<kernel::Symbol>>> scopes_;
};

// Macro expander - transforms AST nodes by applying macros
class MacroExpander {
public:
    MacroExpander() = default;
    
    // Expand macros in an AST node
    std::expected<kernel::Value, std::string> 
    expand(const kernel::Value& ast_node);
    
    // Expand macros recursively in all child nodes
    std::expected<kernel::Value, std::string> 
    expand_recursive(const kernel::Value& ast_node);
    
    // Check if a value is a macro call
    bool is_macro_call(const kernel::Value& ast_node) const;
    
    // Extract macro name from a call
    std::expected<std::string, std::string> 
    get_macro_name(const kernel::Value& ast_node) const;
    
    // Extract arguments from a macro call
    std::expected<std::vector<kernel::Value>, std::string> 
    get_macro_args(const kernel::Value& ast_node) const;
    
    // Hygienic macro expansion support
    
    // Generate a fresh symbol (gensym) for hygienic expansion
    std::shared_ptr<kernel::Symbol> gensym(const std::string& prefix = "G");
    
    // Replace symbols in AST with fresh symbols (for hygiene)
    kernel::Value rename_symbols(const kernel::Value& ast_node, 
                                  const std::map<std::string, std::shared_ptr<kernel::Symbol>>& renames);
    
    // Mark a symbol as unhygienic (intentional capture)
    void mark_unhygienic(const std::string& symbol_name);
    
    // Check if a symbol is marked unhygienic
    bool is_unhygienic(const std::string& symbol_name) const;
    
    // Get current macro scope
    MacroScope& scope() { return scope_; }
    const MacroScope& scope() const { return scope_; }
    
private:
    // Expand a single macro call
    std::expected<kernel::Value, std::string> 
    expand_macro_call(const kernel::Value& ast_node);
    
    // Track expansion depth to prevent infinite recursion
    size_t expansion_depth_ = 0;
    static constexpr size_t MAX_EXPANSION_DEPTH = 100;
    
    // Scope for hygienic expansion
    MacroScope scope_;
    
    // Set of unhygienic symbols (intentional capture)
    std::set<std::string> unhygienic_symbols_;
};

// Helper functions for creating macros

// Create a simple macro from a transformer function
std::shared_ptr<Macro> make_macro(
    std::string name,
    std::vector<std::string> params,
    MacroTransformer transformer
);

// Create a macro from a lambda body (for simple transformations)
std::shared_ptr<Macro> make_simple_macro(
    std::string name,
    std::vector<std::string> params,
    std::function<kernel::Value(const std::vector<kernel::Value>&)> body
);

} // namespace meld::macro
