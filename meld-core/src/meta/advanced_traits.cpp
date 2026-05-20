#include "meld/meta/advanced_traits.hpp"
#include <algorithm>
#include <ranges>
#include <format>

namespace meld::meta {

// AdvancedTraitMetaType implementation
std::expected<const AssociatedType*, std::string> 
AdvancedTraitMetaType::get_associated_type(const std::string& name) const {
    auto it = std::ranges::find_if(associated_types_, 
        [&name](const AssociatedType& at) { return at.name == name; });
    if (it != associated_types_.end()) {
        return &(*it);
    }
    return std::unexpected(std::format("Associated type '{}' not found in trait '{}'", name, this->name()));
}

bool AdvancedTraitMetaType::has_associated_type(const std::string& name) const {
    return std::ranges::any_of(associated_types_, 
        [&name](const AssociatedType& at) { return at.name == name; });
}

bool AdvancedTraitMetaType::extends_trait(const std::shared_ptr<MetaType>& other_trait) const {
    return std::ranges::any_of(super_traits_, 
        [&other_trait](const std::shared_ptr<MetaType>& trait) { 
            return trait.get() == other_trait.get(); 
        });
}

std::vector<Method> AdvancedTraitMetaType::get_all_methods() const {
    std::vector<Method> all_methods = methods();
    
    // Add methods from super traits
    for (const auto& super_trait : super_traits_) {
        if (auto advanced_super = std::dynamic_pointer_cast<AdvancedTraitMetaType>(super_trait)) {
            auto super_methods = advanced_super->get_all_methods();
            all_methods.insert(all_methods.end(), super_methods.begin(), super_methods.end());
        } else if (auto basic_super = std::dynamic_pointer_cast<TraitMetaType>(super_trait)) {
            const auto& super_methods = basic_super->methods();
            all_methods.insert(all_methods.end(), super_methods.begin(), super_methods.end());
        }
    }
    
    return all_methods;
}

// TraitImplementation implementation
std::expected<std::shared_ptr<MetaType>, std::string> 
TraitImplementation::get_associated_type_binding(const std::string& name) const {
    auto it = associated_type_bindings_.find(name);
    if (it != associated_type_bindings_.end()) {
        return it->second;
    }
    
    // Check if the trait has a default for this associated type
    if (auto associated_type = trait_type_->get_associated_type(name)) {
        if (associated_type.value()->default_type) {
            return associated_type.value()->default_type;
        }
    }
    
    return std::unexpected(std::format("Associated type '{}' not bound in implementation", name));
}

std::expected<kernel::Value, std::string> 
TraitImplementation::get_method_implementation(const std::string& name) const {
    auto it = method_implementations_.find(name);
    if (it != method_implementations_.end()) {
        return it->second;
    }
    
    // Check if the trait has a default implementation
    auto all_methods = trait_type_->get_all_methods();
    auto method_it = std::ranges::find_if(all_methods, 
        [&name](const Method& m) { return m.name == name; });
    
    if (method_it != all_methods.end() && method_it->implementation.is<kernel::Function>()) {
        return method_it->implementation;
    }
    
    return std::unexpected(std::format("Method '{}' not implemented", name));
}

std::expected<void, std::string> TraitImplementation::validate() const {
    // Check that all required associated types are bound
    for (const auto& associated_type : trait_type_->associated_types()) {
        auto binding_result = get_associated_type_binding(associated_type.name);
        if (!binding_result) {
            return std::unexpected(std::format("Associated type '{}' must be bound", associated_type.name));
        }
        
        // Check bounds
        auto bound_type = binding_result.value();
        for (const auto& bound : associated_type.bounds) {
            if (!bound_type->is_subtype_of(*bound)) {
                return std::unexpected(std::format("Associated type '{}' does not satisfy bound '{}'", 
                    associated_type.name, bound->name()));
            }
        }
    }
    
    // Check that all required methods are implemented
    auto all_methods = trait_type_->get_all_methods();
    for (const auto& method : all_methods) {
        if (!method.implementation.is<kernel::Function>()) {
            // Method requires implementation
            auto impl_result = get_method_implementation(method.name);
            if (!impl_result) {
                return std::unexpected(std::format("Method '{}' must be implemented", method.name));
            }
        }
    }
    
    return {};
}

// CoherenceChecker implementation
std::expected<void, std::string> 
CoherenceChecker::check_implementation_coherence(const TraitImplementation& new_impl) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto key = std::make_pair(new_impl.implementing_type(), new_impl.trait_type());
    auto existing = implementations_.find(key);
    
    if (existing != implementations_.end()) {
        return std::unexpected(std::format("Conflicting implementation: trait '{}' is already implemented for type '{}'",
            new_impl.trait_type()->name(), new_impl.implementing_type()->name()));
    }
    
    return {};
}

std::expected<void, std::string> 
CoherenceChecker::register_implementation(std::shared_ptr<TraitImplementation> impl) {
    // Check coherence first
    auto coherence_result = check_implementation_coherence(*impl);
    if (!coherence_result) {
        return coherence_result;
    }
    
    // Check orphan rule
    auto orphan_result = check_orphan_rule(*impl);
    if (!orphan_result) {
        return orphan_result;
    }
    
    // Validate the implementation
    auto validation_result = impl->validate();
    if (!validation_result) {
        return validation_result;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    auto key = std::make_pair(impl->implementing_type(), impl->trait_type());
    implementations_[key] = std::move(impl);
    
    return {};
}

std::expected<void, std::string> 
CoherenceChecker::check_orphan_rule(const TraitImplementation& impl) const {
    // Orphan rule: You can only implement a trait for a type if you own either the trait or the type
    // For now, we'll be permissive and allow all implementations
    // In a full implementation, this would check crate/module ownership
    return {};
}

std::expected<std::shared_ptr<TraitImplementation>, std::string>
CoherenceChecker::find_implementation(const std::shared_ptr<MetaType>& type,
                                      const std::shared_ptr<AdvancedTraitMetaType>& trait) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto key = std::make_pair(type, trait);
    auto it = implementations_.find(key);
    
    if (it != implementations_.end()) {
        return it->second;
    }
    
    return std::unexpected(std::format("No implementation found for trait '{}' on type '{}'",
        trait->name(), type->name()));
}

std::vector<std::shared_ptr<TraitImplementation>>
CoherenceChecker::get_implementations_for_trait(const std::shared_ptr<AdvancedTraitMetaType>& trait) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<TraitImplementation>> result;
    
    for (const auto& [key, impl] : implementations_) {
        if (key.second == trait) {
            result.push_back(impl);
        }
    }
    
    return result;
}

std::vector<std::shared_ptr<TraitImplementation>>
CoherenceChecker::get_implementations_for_type(const std::shared_ptr<MetaType>& type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<TraitImplementation>> result;
    
    for (const auto& [key, impl] : implementations_) {
        if (key.first == type) {
            result.push_back(impl);
        }
    }
    
    return result;
}

bool CoherenceChecker::type_implements_trait(const std::shared_ptr<MetaType>& type,
                                             const std::shared_ptr<AdvancedTraitMetaType>& trait) const {
    auto result = find_implementation(type, trait);
    return result.has_value();
}

// TraitObject implementation
std::expected<kernel::Value, std::string>
TraitObject::call_method(const std::string& method_name, 
                         const std::vector<kernel::Value>& args) const {
    auto impl_result = implementation_->get_method_implementation(method_name);
    if (!impl_result) {
        return std::unexpected(impl_result.error());
    }
    
    // In a full implementation, this would invoke the method with the object and args
    // For now, we'll return a placeholder
    return kernel::Value(); // Placeholder
}

std::expected<std::shared_ptr<MetaType>, std::string>
TraitObject::get_associated_type(const std::string& name) const {
    return implementation_->get_associated_type_binding(name);
}

bool TraitObject::is_object_safe() const {
    // Object safety rules:
    // 1. No associated types (or all have defaults)
    // 2. No methods that take Self by value
    // 3. No generic methods
    // 4. No methods that return Self
    
    for (const auto& associated_type : trait_type_->associated_types()) {
        if (!associated_type.default_type) {
            return false; // Associated type without default
        }
    }
    
    auto all_methods = trait_type_->get_all_methods();
    for (const auto& method : all_methods) {
        // Check method signature for object safety
        // This is a simplified check - full implementation would be more thorough
        if (method.name == "clone" || method.name == "copy") {
            return false; // Methods that return Self are not object safe
        }
    }
    
    return true;
}

// BlanketImplementation implementation
bool BlanketImplementation::applies_to_type(const std::shared_ptr<MetaType>& type) const {
    return type_predicate_(type);
}

std::expected<std::shared_ptr<TraitImplementation>, std::string>
BlanketImplementation::instantiate_for_type(const std::shared_ptr<MetaType>& type) const {
    if (!applies_to_type(type)) {
        return std::unexpected(std::format("Blanket implementation does not apply to type '{}'", type->name()));
    }
    
    return std::make_shared<TraitImplementation>(
        type, trait_type_, std::map<std::string, std::shared_ptr<MetaType>>{}, method_implementations_);
}

// AdvancedTraitRegistry implementation
std::expected<void, std::string>
AdvancedTraitRegistry::register_trait(std::shared_ptr<AdvancedTraitMetaType> trait) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto name = trait->name();
    if (traits_.find(name) != traits_.end()) {
        return std::unexpected(std::format("Trait '{}' is already registered", name));
    }
    
    traits_[name] = std::move(trait);
    return {};
}

std::expected<void, std::string>
AdvancedTraitRegistry::register_implementation(std::shared_ptr<TraitImplementation> impl) {
    return coherence_checker_.register_implementation(std::move(impl));
}

std::expected<void, std::string>
AdvancedTraitRegistry::register_blanket_implementation(std::shared_ptr<BlanketImplementation> blanket_impl) {
    std::lock_guard<std::mutex> lock(mutex_);
    blanket_implementations_.push_back(std::move(blanket_impl));
    return {};
}

std::expected<std::shared_ptr<AdvancedTraitMetaType>, std::string>
AdvancedTraitRegistry::get_trait(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = traits_.find(name);
    if (it != traits_.end()) {
        return it->second;
    }
    return std::unexpected(std::format("Trait '{}' not found", name));
}

std::expected<std::shared_ptr<TraitImplementation>, std::string>
AdvancedTraitRegistry::find_implementation(const std::shared_ptr<MetaType>& type,
                                           const std::shared_ptr<AdvancedTraitMetaType>& trait) const {
    // First check explicit implementations
    auto explicit_result = coherence_checker_.find_implementation(type, trait);
    if (explicit_result) {
        return explicit_result;
    }
    
    // Check blanket implementations
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& blanket_impl : blanket_implementations_) {
        if (blanket_impl->trait_type() == trait && blanket_impl->applies_to_type(type)) {
            return blanket_impl->instantiate_for_type(type);
        }
    }
    
    return std::unexpected(std::format("No implementation found for trait '{}' on type '{}'",
        trait->name(), type->name()));
}

std::expected<std::shared_ptr<TraitObject>, std::string>
AdvancedTraitRegistry::create_trait_object(const std::shared_ptr<AdvancedTraitMetaType>& trait,
                                           const kernel::Value& object) const {
    // Find the implementation for the object's type
    // This is simplified - in practice we'd need to get the type from the object
    return std::unexpected("TraitObject creation not fully implemented");
}

bool AdvancedTraitRegistry::type_implements_trait(const std::shared_ptr<MetaType>& type,
                                                  const std::shared_ptr<AdvancedTraitMetaType>& trait) const {
    auto result = find_implementation(type, trait);
    return result.has_value();
}

std::vector<std::shared_ptr<AdvancedTraitMetaType>>
AdvancedTraitRegistry::get_implemented_traits(const std::shared_ptr<MetaType>& type) const {
    std::vector<std::shared_ptr<AdvancedTraitMetaType>> result;
    
    // Get explicit implementations
    auto explicit_impls = coherence_checker_.get_implementations_for_type(type);
    for (const auto& impl : explicit_impls) {
        result.push_back(impl->trait_type());
    }
    
    // Check blanket implementations
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& blanket_impl : blanket_implementations_) {
        if (blanket_impl->applies_to_type(type)) {
            result.push_back(blanket_impl->trait_type());
        }
    }
    
    return result;
}

// Factory functions
std::shared_ptr<AdvancedTraitMetaType> 
create_advanced_trait(std::string name,
                      std::vector<Method> methods,
                      std::vector<AssociatedType> associated_types,
                      std::vector<std::shared_ptr<MetaType>> super_traits) {
    return std::make_shared<AdvancedTraitMetaType>(
        std::move(name), std::move(methods), std::move(associated_types), std::move(super_traits));
}

std::shared_ptr<TraitImplementation>
create_trait_implementation(std::shared_ptr<MetaType> implementing_type,
                            std::shared_ptr<AdvancedTraitMetaType> trait_type,
                            std::map<std::string, std::shared_ptr<MetaType>> associated_type_bindings,
                            std::map<std::string, kernel::Value> method_implementations) {
    return std::make_shared<TraitImplementation>(
        std::move(implementing_type), std::move(trait_type), 
        std::move(associated_type_bindings), std::move(method_implementations));
}

std::shared_ptr<BlanketImplementation>
create_blanket_implementation(std::shared_ptr<AdvancedTraitMetaType> trait_type,
                              std::function<bool(const std::shared_ptr<MetaType>&)> type_predicate,
                              std::map<std::string, kernel::Value> method_implementations) {
    return std::make_shared<BlanketImplementation>(
        std::move(trait_type), std::move(type_predicate), std::move(method_implementations));
}

} // namespace meld::meta