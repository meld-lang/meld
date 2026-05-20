#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>

namespace meld::kernel {

// Forward declaration
class Symbol;

// Represents a namespace scope
class NamespaceScope {
public:
    NamespaceScope(const std::vector<std::string>& path, NamespaceScope* parent = nullptr)
        : path_(path), parent_(parent) {}
    
    // Get the full namespace path (e.g., ["com", "example", "myapp"])
    const std::vector<std::string>& path() const { return path_; }
    
    // Get the fully qualified name (e.g., "com.example.myapp")
    std::string qualified_name() const;
    
    // Register a symbol in this namespace
    void register_symbol(const std::string& name, std::shared_ptr<Symbol> symbol);
    
    // Look up a symbol in this namespace (does not search parent scopes)
    std::shared_ptr<Symbol> lookup_local(const std::string& name) const;
    
    // Look up a symbol in this namespace and parent scopes
    std::shared_ptr<Symbol> lookup(const std::string& name) const;
    
    // Get or create a child namespace
    std::shared_ptr<NamespaceScope> get_or_create_child(const std::string& name);
    
    // Get parent namespace
    NamespaceScope* parent() const { return parent_; }
    
    // Get all symbols in this namespace
    const std::unordered_map<std::string, std::shared_ptr<Symbol>>& symbols() const {
        return symbols_;
    }
    
private:
    std::vector<std::string> path_;
    NamespaceScope* parent_;
    std::unordered_map<std::string, std::shared_ptr<Symbol>> symbols_;
    std::unordered_map<std::string, std::shared_ptr<NamespaceScope>> children_;
};

// Manages namespace registration and resolution
class NamespaceRegistry {
public:
    static NamespaceRegistry& instance() {
        static NamespaceRegistry registry;
        return registry;
    }
    
    // Get or create a namespace by path
    std::shared_ptr<NamespaceScope> get_or_create_namespace(const std::vector<std::string>& path);
    
    // Get the root namespace
    std::shared_ptr<NamespaceScope> root_namespace() const { return root_; }
    
    // Get the current namespace (for scoped operations)
    std::shared_ptr<NamespaceScope> current_namespace() const { return current_; }
    
    // Set the current namespace
    void set_current_namespace(std::shared_ptr<NamespaceScope> ns) { current_ = ns; }
    
    // Push a namespace onto the stack (for nested namespace support)
    void push_namespace(std::shared_ptr<NamespaceScope> ns);
    
    // Pop the current namespace from the stack
    void pop_namespace();
    
    // Resolve a fully qualified name (e.g., "com.example.User")
    std::shared_ptr<Symbol> resolve_qualified_name(const std::string& qualified_name) const;
    
    // Resolve a name in the current namespace context
    std::shared_ptr<Symbol> resolve_name(const std::string& name);
    
    // Register an import in the current namespace
    void register_import(const std::vector<std::string>& path, bool is_wildcard, 
                        const std::string& alias = "");
    
    // Get all imports for the current namespace
    struct Import {
        std::vector<std::string> path;
        bool is_wildcard;
        std::string alias;
        bool has_alias;
    };
    const std::vector<Import>& current_imports() const;
    
private:
    NamespaceRegistry();
    
    std::shared_ptr<NamespaceScope> root_;
    std::shared_ptr<NamespaceScope> current_;
    std::vector<std::shared_ptr<NamespaceScope>> namespace_stack_;
    
    // Track imports per namespace
    std::unordered_map<std::string, std::vector<Import>> imports_;
    
    mutable std::mutex mutex_;
};

} // namespace meld::kernel
