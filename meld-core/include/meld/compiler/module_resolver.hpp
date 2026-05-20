#pragma once

#include "meld/compiler/module_definition.hpp"
#include "meld/parser/ast.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <filesystem>
#include <memory>
#include <functional>

namespace meld::compiler {

// Error information for module resolution failures
struct ModuleResolutionError {
    enum class Kind {
        NOT_FOUND,
        CIRCULAR_IMPORT,
        PRIVATE_SYMBOL,
        SYMBOL_NOT_EXPORTED,
        AMBIGUOUS_MODULE,
        PARSE_ERROR
    };
    
    Kind kind;
    std::string message;
    std::string module_path;           // The module path that caused the error
    std::vector<std::string> cycle;    // For CIRCULAR_IMPORT: the full cycle path
    
    // CAP-compatible structured error output
    std::string to_cap_json() const;
};

// Configuration for module resolution search paths
struct ModuleResolverConfig {
    std::filesystem::path project_root;                    // Project root directory
    std::filesystem::path stdlib_root;                     // Standard library root
    std::vector<std::filesystem::path> additional_roots;   // Additional search paths
    std::unordered_map<std::string, std::filesystem::path> package_roots;  // package prefix → path
};

// Resolves dot-separated module paths to file system locations.
//
// Resolution order (per Requirement 31D):
//   1. std.* prefix → standard library directory
//   2. Project-relative paths → project_root/path/to/module.meld
//   3. Registered package prefix → package manager directory
//
// Dots map to directory separators:
//   app.services.auth → app/services/auth.meld
class ModuleResolver {
public:
    explicit ModuleResolver(const ModuleResolverConfig& config);
    
    // Resolve a dot-separated module path to a filesystem path.
    // Returns the resolved path or an error.
    std::variant<std::filesystem::path, ModuleResolutionError>
    resolve(const std::string& module_path) const;
    
    // Resolve a module path and load/parse the module definition.
    // Registers the module in the ModuleRegistry if not already loaded.
    std::variant<std::shared_ptr<ModuleDefinition>, ModuleResolutionError>
    resolve_and_load(const std::string& module_path);
    
    // Check if a module path would resolve to the standard library
    bool is_stdlib_path(const std::string& module_path) const;
    
    // Check if a module path matches a registered package prefix
    bool is_package_path(const std::string& module_path) const;
    
    // Get the package prefix for a module path, if any
    std::optional<std::string> get_package_prefix(const std::string& module_path) const;
    
    // Register a package prefix → root directory mapping
    void register_package(const std::string& prefix, const std::filesystem::path& root);
    
    // Get the resolver configuration
    const ModuleResolverConfig& config() const { return config_; }

private:
    ModuleResolverConfig config_;
    
    // Convert dot-separated path to filesystem path segments
    static std::vector<std::string> split_module_path(const std::string& module_path);
    
    // Build a filesystem path from module path segments and a root directory
    static std::filesystem::path build_file_path(
        const std::vector<std::string>& segments,
        const std::filesystem::path& root);
};

// Tracks module dependencies and detects circular imports.
//
// Per Requirement 31E:
//   - Enforces DAG constraint on module dependencies
//   - Rejects any import cycle at compile time
//   - Emits structured error identifying the full cycle path (CAP-compatible)
class ModuleDependencyGraph {
public:
    // Add a dependency edge: importer depends on imported
    void add_dependency(const std::string& importer, const std::string& imported);
    
    // Check if adding a dependency would create a cycle.
    // Returns the cycle path if a cycle would be created, empty otherwise.
    std::vector<std::string> would_create_cycle(
        const std::string& importer, const std::string& imported) const;
    
    // Validate the entire graph is a DAG.
    // Returns the first cycle found, or empty if the graph is acyclic.
    std::vector<std::string> find_cycle() const;
    
    // Get all direct dependencies of a module
    std::vector<std::string> dependencies_of(const std::string& module) const;
    
    // Get all modules that depend on a given module
    std::vector<std::string> dependents_of(const std::string& module) const;
    
    // Get topological ordering of all modules (empty if cycle exists)
    std::vector<std::string> topological_order() const;
    
    // Clear all dependency information
    void clear();

private:
    // Adjacency list: module → set of modules it imports
    std::unordered_map<std::string, std::unordered_set<std::string>> edges_;
    
    // Reverse adjacency list: module → set of modules that import it
    std::unordered_map<std::string, std::unordered_set<std::string>> reverse_edges_;
    
    // DFS helper for cycle detection
    bool dfs_has_cycle(
        const std::string& node,
        std::unordered_set<std::string>& visiting,
        std::unordered_set<std::string>& visited,
        std::vector<std::string>& path) const;
};

// Manages re-exports: symbols imported via `imp` that become part of
// the re-exporting module's public API.
//
// Per Requirement 31F:
//   - A module can re-export symbols from another module
//   - Importers gain access without knowing origin modules
class ReExportManager {
public:
    // Mark a symbol as re-exported from a module
    void add_re_export(
        const std::string& re_exporting_module,
        const std::string& symbol_name,
        const std::string& origin_module);
    
    // Get all re-exported symbols for a module
    struct ReExportedSymbol {
        std::string symbol_name;
        std::string origin_module;
    };
    std::vector<ReExportedSymbol> get_re_exports(const std::string& module) const;
    
    // Check if a symbol is re-exported by a module
    bool is_re_exported(const std::string& module, const std::string& symbol_name) const;
    
    // Clear all re-export information
    void clear();

private:
    // module → list of re-exported symbols
    std::unordered_map<std::string, std::vector<ReExportedSymbol>> re_exports_;
};

} // namespace meld::compiler
