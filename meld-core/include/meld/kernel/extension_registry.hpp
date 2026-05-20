#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

namespace meld::kernel {

// Forward declarations
class Value;
class MetaType;

// Extension method signature
struct ExtensionMethod {
    std::string method_name;
    std::string target_type_name;
    std::function<Value(const std::vector<Value>&)> implementation;
    std::vector<std::string> parameter_types;
    std::string return_type;
    
    ExtensionMethod(
        const std::string& name,
        const std::string& target_type,
        std::function<Value(const std::vector<Value>&)> impl,
        const std::vector<std::string>& param_types = {},
        const std::string& ret_type = "Unit"
    ) : method_name(name),
        target_type_name(target_type),
        implementation(impl),
        parameter_types(param_types),
        return_type(ret_type) {}
};

// Extension method registry
class ExtensionRegistry {
public:
    static ExtensionRegistry& instance() {
        static ExtensionRegistry registry;
        return registry;
    }
    
    // Register an extension method for a type
    void register_extension(
        const std::string& type_name,
        const std::string& method_name,
        std::function<Value(const std::vector<Value>&)> implementation,
        const std::vector<std::string>& parameter_types = {},
        const std::string& return_type = "Unit"
    );
    
    // Look up an extension method for a type
    // Returns nullptr if not found
    const ExtensionMethod* lookup_extension(
        const std::string& type_name,
        const std::string& method_name
    ) const;
    
    // Check if an extension method exists
    bool has_extension(
        const std::string& type_name,
        const std::string& method_name
    ) const;
    
    // Get all extension methods for a type
    std::vector<const ExtensionMethod*> get_extensions_for_type(
        const std::string& type_name
    ) const;
    
    // Clear all registered extensions (useful for testing)
    void clear();
    
private:
    ExtensionRegistry() = default;
    ExtensionRegistry(const ExtensionRegistry&) = delete;
    ExtensionRegistry& operator=(const ExtensionRegistry&) = delete;
    
    // Map from type name to map of method name to extension method
    std::unordered_map<std::string, std::unordered_map<std::string, ExtensionMethod>> extensions_;
};

} // namespace meld::kernel
