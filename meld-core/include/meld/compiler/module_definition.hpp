#pragma once

#include "meld/parser/ast.hpp"
#include "meld/kernel/namespace_registry.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <filesystem>

namespace meld::compiler {

// Visibility of a module-level symbol
enum class SymbolVisibility {
    PUBLIC,   // Default — exported to importers
    PRIVATE   // Annotated with @private — visible only within the defining module
};

// A single exported (or private) symbol within a module
struct ModuleSymbol {
    std::string name;
    SymbolVisibility visibility = SymbolVisibility::PUBLIC;
    
    // The kind of declaration that produced this symbol
    enum class Kind {
        VAL,
        VAR,
        FUNCTION,
        STRUCT,
        CLASS,
        ENUM,
        TRAIT
    };
    Kind kind;
};

// Represents a single .meld file as an implicit module.
//
// Per the spec (Requirement 31C):
//   - A .meld file IS a module; its name derives from the file path.
//   - All top-level val, var, fnc, struct, class, enum, and trait
//     declarations are exported by default.
//   - The @private annotation restricts visibility to the defining module.
class ModuleDefinition {
public:
    // Construct from a file path relative to the project root.
    // e.g. "app/services/auth.meld" → module name "app.services.auth"
    explicit ModuleDefinition(const std::string& file_path);

    // Module identity
    const std::string& file_path() const { return file_path_; }
    const std::string& module_name() const { return module_name_; }
    const std::vector<std::string>& module_path() const { return module_path_; }
    
    // Build the module definition from parsed top-level expressions.
    // Scans for val, var, fnc, struct, class, enum, trait declarations
    // and checks for @private annotations.
    void build_from_expressions(
        const std::vector<parser::ast::expression>& expressions,
        const std::unordered_set<std::string>& private_symbols = {}
    );
    
    // Query symbols
    const std::vector<ModuleSymbol>& symbols() const { return symbols_; }
    
    // Get only the publicly exported symbols
    std::vector<const ModuleSymbol*> public_symbols() const;
    
    // Get only the private symbols
    std::vector<const ModuleSymbol*> private_symbols() const;
    
    // Check if a symbol is exported (public)
    bool is_exported(const std::string& name) const;
    
    // Check if a symbol exists in this module (public or private)
    bool has_symbol(const std::string& name) const;
    
    // Look up a symbol by name (returns nullptr if not found)
    const ModuleSymbol* find_symbol(const std::string& name) const;
    
    // Register this module's public symbols into a NamespaceScope
    void register_in_namespace(std::shared_ptr<kernel::NamespaceScope> scope) const;
    
    // Derive module name from file path
    // "app/services/auth.meld" → "app.services.auth"
    static std::string derive_module_name(const std::string& file_path);
    
    // Derive module path segments from file path
    // "app/services/auth.meld" → ["app", "services", "auth"]
    static std::vector<std::string> derive_module_path(const std::string& file_path);

private:
    std::string file_path_;
    std::string module_name_;
    std::vector<std::string> module_path_;
    std::vector<ModuleSymbol> symbols_;
    std::unordered_map<std::string, size_t> symbol_index_;  // name → index into symbols_
    
    void add_symbol(const std::string& name, ModuleSymbol::Kind kind, SymbolVisibility visibility);
};

// Registry of all loaded modules in a compilation unit
class ModuleRegistry {
public:
    static ModuleRegistry& instance() {
        static ModuleRegistry registry;
        return registry;
    }
    
    // Register a module definition
    void register_module(std::shared_ptr<ModuleDefinition> module);
    
    // Look up a module by its dot-separated name (e.g. "app.services.auth")
    std::shared_ptr<ModuleDefinition> find_module(const std::string& module_name) const;
    
    // Look up a module by file path
    std::shared_ptr<ModuleDefinition> find_module_by_path(const std::string& file_path) const;
    
    // Get all registered modules
    const std::unordered_map<std::string, std::shared_ptr<ModuleDefinition>>& modules() const {
        return modules_;
    }
    
    // Clear all modules (useful for testing)
    void clear();

private:
    ModuleRegistry() = default;
    
    std::unordered_map<std::string, std::shared_ptr<ModuleDefinition>> modules_;       // name → module
    std::unordered_map<std::string, std::shared_ptr<ModuleDefinition>> path_to_module_; // path → module
    mutable std::mutex mutex_;
};

} // namespace meld::compiler
