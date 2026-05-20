#include "meld/kernel/namespace_registry.hpp"
#include "meld/kernel/primitives.hpp"
#include <sstream>
#include <algorithm>

namespace meld::kernel {

// NamespaceScope implementation

std::string NamespaceScope::qualified_name() const {
    if (path_.empty()) {
        return "";
    }
    
    std::ostringstream oss;
    for (size_t i = 0; i < path_.size(); ++i) {
        if (i > 0) oss << ".";
        oss << path_[i];
    }
    return oss.str();
}

void NamespaceScope::register_symbol(const std::string& name, std::shared_ptr<Symbol> symbol) {
    symbols_[name] = symbol;
}

std::shared_ptr<Symbol> NamespaceScope::lookup_local(const std::string& name) const {
    auto it = symbols_.find(name);
    if (it != symbols_.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<Symbol> NamespaceScope::lookup(const std::string& name) const {
    // First check local scope
    auto symbol = lookup_local(name);
    if (symbol) {
        return symbol;
    }
    
    // Then check parent scopes
    if (parent_) {
        return parent_->lookup(name);
    }
    
    return nullptr;
}

std::shared_ptr<NamespaceScope> NamespaceScope::get_or_create_child(const std::string& name) {
    auto it = children_.find(name);
    if (it != children_.end()) {
        return it->second;
    }
    
    // Create new child namespace
    std::vector<std::string> child_path = path_;
    child_path.push_back(name);
    auto child = std::make_shared<NamespaceScope>(child_path, this);
    children_[name] = child;
    return child;
}

// NamespaceRegistry implementation

NamespaceRegistry::NamespaceRegistry() {
    // Create root namespace
    root_ = std::make_shared<NamespaceScope>(std::vector<std::string>{}, nullptr);
    current_ = root_;
}

std::shared_ptr<NamespaceScope> NamespaceRegistry::get_or_create_namespace(
    const std::vector<std::string>& path) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (path.empty()) {
        return root_;
    }
    
    auto current = root_;
    for (const auto& part : path) {
        current = current->get_or_create_child(part);
    }
    
    return current;
}

void NamespaceRegistry::push_namespace(std::shared_ptr<NamespaceScope> ns) {
    std::lock_guard<std::mutex> lock(mutex_);
    namespace_stack_.push_back(current_);
    current_ = ns;
}

void NamespaceRegistry::pop_namespace() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!namespace_stack_.empty()) {
        current_ = namespace_stack_.back();
        namespace_stack_.pop_back();
    }
}

std::shared_ptr<Symbol> NamespaceRegistry::resolve_qualified_name(
    const std::string& qualified_name) const {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Split the qualified name by '.'
    std::vector<std::string> parts;
    std::istringstream iss(qualified_name);
    std::string part;
    while (std::getline(iss, part, '.')) {
        parts.push_back(part);
    }
    
    if (parts.empty()) {
        return nullptr;
    }
    
    // Navigate to the namespace
    auto ns = root_;
    for (size_t i = 0; i < parts.size() - 1; ++i) {
        auto child_it = ns->symbols().find(parts[i]);
        if (child_it == ns->symbols().end()) {
            return nullptr;
        }
        // In a real implementation, we'd need to check if this is a namespace
        // For now, we'll just try to get the child namespace
        ns = ns->get_or_create_child(parts[i]);
    }
    
    // Look up the final symbol
    return ns->lookup_local(parts.back());
}

std::shared_ptr<Symbol> NamespaceRegistry::resolve_name(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // First check if it's a qualified name
    if (name.find('.') != std::string::npos) {
        return resolve_qualified_name(name);
    }
    
    // Check current namespace and parent scopes
    auto symbol = current_->lookup(name);
    if (symbol) {
        return symbol;
    }
    
    // Check imports
    auto qualified_current = current_->qualified_name();
    auto imports_it = imports_.find(qualified_current);
    if (imports_it != imports_.end()) {
        for (const auto& import : imports_it->second) {
            if (import.is_wildcard) {
                // Try to resolve from the imported namespace
                auto imported_ns = get_or_create_namespace(import.path);
                auto imported_symbol = imported_ns->lookup_local(name);
                if (imported_symbol) {
                    return imported_symbol;
                }
            } else if (import.has_alias && import.alias == name) {
                // Resolve the aliased namespace
                return resolve_qualified_name(
                    [&]() {
                        std::ostringstream oss;
                        for (size_t i = 0; i < import.path.size(); ++i) {
                            if (i > 0) oss << ".";
                            oss << import.path[i];
                        }
                        return oss.str();
                    }()
                );
            } else {
                // Check if this is a specific import matching the name
                if (!import.path.empty() && import.path.back() == name) {
                    return resolve_qualified_name(
                        [&]() {
                            std::ostringstream oss;
                            for (size_t i = 0; i < import.path.size(); ++i) {
                                if (i > 0) oss << ".";
                                oss << import.path[i];
                            }
                            return oss.str();
                        }()
                    );
                }
            }
        }
    }
    
    return nullptr;
}

void NamespaceRegistry::register_import(const std::vector<std::string>& path, 
                                       bool is_wildcard, const std::string& alias) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto qualified_current = current_->qualified_name();
    Import import;
    import.path = path;
    import.is_wildcard = is_wildcard;
    import.alias = alias;
    import.has_alias = !alias.empty();
    
    imports_[qualified_current].push_back(import);
}

const std::vector<NamespaceRegistry::Import>& NamespaceRegistry::current_imports() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto qualified_current = current_->qualified_name();
    auto it = imports_.find(qualified_current);
    if (it != imports_.end()) {
        return it->second;
    }
    
    static std::vector<Import> empty;
    return empty;
}

} // namespace meld::kernel
