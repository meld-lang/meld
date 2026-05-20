#include "meld/kernel/extension_registry.hpp"

namespace meld::kernel {

void ExtensionRegistry::register_extension(
    const std::string& type_name,
    const std::string& method_name,
    std::function<Value(const std::vector<Value>&)> implementation,
    const std::vector<std::string>& parameter_types,
    const std::string& return_type
) {
    ExtensionMethod method(method_name, type_name, implementation, parameter_types, return_type);
    extensions_[type_name].insert_or_assign(method_name, method);
}

const ExtensionMethod* ExtensionRegistry::lookup_extension(
    const std::string& type_name,
    const std::string& method_name
) const {
    auto type_it = extensions_.find(type_name);
    if (type_it == extensions_.end()) {
        return nullptr;
    }
    
    auto method_it = type_it->second.find(method_name);
    if (method_it == type_it->second.end()) {
        return nullptr;
    }
    
    return &method_it->second;
}

bool ExtensionRegistry::has_extension(
    const std::string& type_name,
    const std::string& method_name
) const {
    return lookup_extension(type_name, method_name) != nullptr;
}

std::vector<const ExtensionMethod*> ExtensionRegistry::get_extensions_for_type(
    const std::string& type_name
) const {
    std::vector<const ExtensionMethod*> result;
    
    auto type_it = extensions_.find(type_name);
    if (type_it != extensions_.end()) {
        for (const auto& [method_name, method] : type_it->second) {
            result.push_back(&method);
        }
    }
    
    return result;
}

void ExtensionRegistry::clear() {
    extensions_.clear();
}

} // namespace meld::kernel
