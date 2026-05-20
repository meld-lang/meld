#include "meld/meta/trait_objects.hpp"
#include "meld/meta/advanced_traits.hpp"
#include <algorithm>
#include <ranges>
#include <format>

namespace meld::meta {

// VTable implementation
VTable::VTable(std::shared_ptr<AdvancedTraitMetaType> trait_type,
               std::shared_ptr<MetaType> concrete_type,
               std::shared_ptr<TraitImplementation> implementation)
    : trait_type_(std::move(trait_type))
    , concrete_type_(std::move(concrete_type))
    , implementation_(std::move(implementation)) {
    build_method_table();
}

std::expected<kernel::Value, std::string> VTable::get_method(const std::string& method_name) const {
    auto it = method_table_.find(method_name);
    if (it != method_table_.end()) {
        return it->second;
    }
    return std::unexpected(std::format("Method '{}' not found in vtable", method_name));
}

std::expected<kernel::Value, std::string> 
VTable::call_method(const std::string& method_name, 
                    const kernel::Value& self_object,
                    const std::vector<kernel::Value>& args) const {
    auto method_result = get_method(method_name);
    if (!method_result) {
        return method_result;
    }
    
    // In a full implementation, this would invoke the method function with self_object and args
    // For now, we'll use the implementation's method call mechanism
    return implementation_->get_method_implementation(method_name);
}

bool VTable::has_method(const std::string& method_name) const {
    return method_table_.find(method_name) != method_table_.end();
}

std::vector<std::string> VTable::get_method_names() const {
    std::vector<std::string> names;
    names.reserve(method_table_.size());
    for (const auto& [name, _] : method_table_) {
        names.push_back(name);
    }
    return names;
}

void VTable::build_method_table() {
    // Build the method table from the trait implementation
    auto all_methods = trait_type_->get_all_methods();
    for (const auto& method : all_methods) {
        auto impl_result = implementation_->get_method_implementation(method.name);
        if (impl_result) {
            method_table_[method.name] = impl_result.value();
        }
    }
}

// EnhancedTraitObject implementation
EnhancedTraitObject::EnhancedTraitObject(std::shared_ptr<AdvancedTraitMetaType> trait_type,
                                         kernel::Value object,
                                         std::shared_ptr<VTable> vtable)
    : trait_type_(std::move(trait_type))
    , object_(std::move(object))
    , vtable_(std::move(vtable)) {}

std::expected<kernel::Value, std::string>
EnhancedTraitObject::call_method(const std::string& method_name, 
                                 const std::vector<kernel::Value>& args) const {
    return vtable_->call_method(method_name, object_, args);
}

std::expected<std::shared_ptr<MetaType>, std::string>
EnhancedTraitObject::get_associated_type(const std::string& name) const {
    return vtable_->implementation()->get_associated_type_binding(name);
}

bool EnhancedTraitObject::is_object_safe() const {
    ObjectSafetyChecker checker;
    auto result = checker.is_object_safe(trait_type_);
    return result.value_or(false);
}

std::vector<std::string> EnhancedTraitObject::get_object_safety_violations() const {
    ObjectSafetyChecker checker;
    return checker.get_object_safety_violations(trait_type_);
}

std::expected<std::shared_ptr<EnhancedTraitObject>, std::string> 
EnhancedTraitObject::clone() const {
    // Check if the trait supports cloning
    if (!vtable_->has_method("clone")) {
        return std::unexpected("Trait object does not support cloning");
    }
    
    auto clone_result = vtable_->call_method("clone", object_, {});
    if (!clone_result) {
        return std::unexpected(clone_result.error());
    }
    
    return std::make_shared<EnhancedTraitObject>(trait_type_, clone_result.value(), vtable_);
}

std::expected<bool, std::string> 
EnhancedTraitObject::equals(const EnhancedTraitObject& other) const {
    // Check if both objects implement the same trait
    if (trait_type_ != other.trait_type_) {
        return std::unexpected("Cannot compare trait objects of different traits");
    }
    
    // Check if the trait supports equality
    if (!vtable_->has_method("eq")) {
        return std::unexpected("Trait object does not support equality comparison");
    }
    
    auto eq_result = vtable_->call_method("eq", object_, {other.object_});
    if (!eq_result) {
        return std::unexpected(eq_result.error());
    }
    
    // Convert result to boolean (simplified)
    return true; // Placeholder
}

// TraitObjectMetaType implementation
bool TraitObjectMetaType::can_convert_from(const std::shared_ptr<MetaType>& concrete_type) const {
    // Check if the concrete type implements the trait
    auto& registry = AdvancedTraitRegistry::instance();
    return registry.type_implements_trait(concrete_type, trait_type_);
}

std::expected<std::shared_ptr<EnhancedTraitObject>, std::string>
TraitObjectMetaType::create_trait_object(const kernel::Value& concrete_value) const {
    // This would need the concrete type of the value
    // For now, return an error as we need more context
    return std::unexpected("Cannot determine concrete type from value");
}

// DynamicDispatcher implementation
void DynamicDispatcher::register_vtable(std::shared_ptr<AdvancedTraitMetaType> trait_type,
                                         std::shared_ptr<MetaType> concrete_type,
                                         std::shared_ptr<VTable> vtable) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto key = std::make_pair(std::move(trait_type), std::move(concrete_type));
    vtables_[key] = std::move(vtable);
}

std::expected<std::shared_ptr<VTable>, std::string>
DynamicDispatcher::get_vtable(const std::shared_ptr<AdvancedTraitMetaType>& trait_type,
                              const std::shared_ptr<MetaType>& concrete_type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto key = std::make_pair(trait_type, concrete_type);
    auto it = vtables_.find(key);
    
    if (it != vtables_.end()) {
        return it->second;
    }
    
    return std::unexpected(std::format("No vtable found for trait '{}' on type '{}'",
        trait_type->name(), concrete_type->name()));
}

std::expected<std::shared_ptr<EnhancedTraitObject>, std::string>
DynamicDispatcher::create_trait_object(const std::shared_ptr<AdvancedTraitMetaType>& trait_type,
                                       const kernel::Value& concrete_value,
                                       const std::shared_ptr<MetaType>& concrete_type) const {
    auto vtable_result = get_vtable(trait_type, concrete_type);
    if (!vtable_result) {
        return std::unexpected(vtable_result.error());
    }
    
    return std::make_shared<EnhancedTraitObject>(trait_type, concrete_value, vtable_result.value());
}

std::expected<kernel::Value, std::string>
DynamicDispatcher::dispatch_method(const std::shared_ptr<AdvancedTraitMetaType>& trait_type,
                                   const std::shared_ptr<MetaType>& concrete_type,
                                   const std::string& method_name,
                                   const kernel::Value& self_object,
                                   const std::vector<kernel::Value>& args) const {
    auto vtable_result = get_vtable(trait_type, concrete_type);
    if (!vtable_result) {
        return std::unexpected(vtable_result.error());
    }
    
    return vtable_result.value()->call_method(method_name, self_object, args);
}

std::vector<std::shared_ptr<VTable>> DynamicDispatcher::get_all_vtables() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<VTable>> result;
    result.reserve(vtables_.size());
    
    for (const auto& [key, vtable] : vtables_) {
        result.push_back(vtable);
    }
    
    return result;
}

bool DynamicDispatcher::type_implements_trait(const std::shared_ptr<MetaType>& type,
                                              const std::shared_ptr<AdvancedTraitMetaType>& trait) const {
    auto vtable_result = get_vtable(trait, type);
    return vtable_result.has_value();
}

// HigherRankedTraitBounds implementation
std::expected<bool, std::string>
HigherRankedTraitBounds::satisfies_hrtb(const std::shared_ptr<MetaType>& type, const HRTBound& bound) const {
    // Check if the type implements the trait
    auto& registry = AdvancedTraitRegistry::instance();
    if (!registry.type_implements_trait(type, bound.trait_type)) {
        return false;
    }
    
    // Check associated type constraints
    auto impl_result = registry.find_implementation(type, bound.trait_type);
    if (!impl_result) {
        return std::unexpected(impl_result.error());
    }
    
    auto implementation = impl_result.value();
    for (const auto& [assoc_name, constraint_type] : bound.associated_type_constraints) {
        auto binding_result = implementation->get_associated_type_binding(assoc_name);
        if (!binding_result) {
            return std::unexpected(binding_result.error());
        }
        
        auto bound_type = binding_result.value();
        if (!bound_type->is_subtype_of(*constraint_type)) {
            return false;
        }
    }
    
    // Check lifetime constraints (simplified for now)
    if (!check_lifetime_constraints(bound.lifetime_params)) {
        return false;
    }
    
    return true;
}

std::expected<std::shared_ptr<EnhancedTraitObject>, std::string>
HigherRankedTraitBounds::create_hrtb_trait_object(const HRTBound& bound, 
                                                   const kernel::Value& concrete_value) const {
    // This would need the concrete type of the value
    return std::unexpected("HRTB trait object creation not fully implemented");
}

std::expected<void, std::string> 
HigherRankedTraitBounds::validate_hrtb(const HRTBound& bound) const {
    // Validate that the bound is well-formed
    if (!bound.trait_type) {
        return std::unexpected("HRTB must have a valid trait type");
    }
    
    // Check that all associated type constraints reference valid associated types
    for (const auto& [assoc_name, _] : bound.associated_type_constraints) {
        if (!bound.trait_type->has_associated_type(assoc_name)) {
            return std::unexpected(std::format("Associated type '{}' not found in trait '{}'",
                assoc_name, bound.trait_type->name()));
        }
    }
    
    // Validate lifetime parameters
    if (!check_lifetime_constraints(bound.lifetime_params)) {
        return std::unexpected("Invalid lifetime constraints in HRTB");
    }
    
    return {};
}

std::vector<std::string> 
HigherRankedTraitBounds::get_lifetime_parameter_names(const HRTBound& bound) const {
    std::vector<std::string> names;
    names.reserve(bound.lifetime_params.size());
    
    for (const auto& param : bound.lifetime_params) {
        names.push_back(param.name);
    }
    
    return names;
}

bool HigherRankedTraitBounds::check_lifetime_constraints(const std::vector<LifetimeParameter>& params) const {
    // Simplified lifetime constraint checking
    // In a full implementation, this would check for cycles and validity
    for (const auto& param : params) {
        for (const auto& bound : param.bounds) {
            // Check that bound lifetime exists
            auto bound_it = std::ranges::find_if(params, 
                [&bound](const LifetimeParameter& p) { return p.name == bound; });
            if (bound_it == params.end()) {
                return false; // Bound lifetime not found
            }
        }
    }
    return true;
}

// ObjectSafetyChecker implementation
std::expected<bool, std::string> 
ObjectSafetyChecker::is_object_safe(const std::shared_ptr<AdvancedTraitMetaType>& trait) const {
    auto violations = analyze_object_safety(trait);
    return violations.empty();
}

std::vector<std::string> 
ObjectSafetyChecker::get_object_safety_violations(const std::shared_ptr<AdvancedTraitMetaType>& trait) const {
    auto violations = analyze_object_safety(trait);
    std::vector<std::string> descriptions;
    descriptions.reserve(violations.size());
    
    for (const auto& violation : violations) {
        descriptions.push_back(violation.description);
    }
    
    return descriptions;
}

std::vector<ObjectSafetyChecker::SafetyViolation> 
ObjectSafetyChecker::analyze_object_safety(const std::shared_ptr<AdvancedTraitMetaType>& trait) const {
    std::vector<SafetyViolation> violations;
    
    check_associated_types(trait, violations);
    check_method_signatures(trait, violations);
    check_generic_methods(trait, violations);
    
    return violations;
}

bool ObjectSafetyChecker::check_associated_types(const std::shared_ptr<AdvancedTraitMetaType>& trait, 
                                                 std::vector<SafetyViolation>& violations) const {
    bool is_safe = true;
    
    for (const auto& assoc_type : trait->associated_types()) {
        if (!assoc_type.default_type) {
            violations.emplace_back(
                ObjectSafetyViolation::AssociatedTypeWithoutDefault,
                "",
                std::format("Associated type '{}' has no default implementation", assoc_type.name)
            );
            is_safe = false;
        }
    }
    
    return is_safe;
}

bool ObjectSafetyChecker::check_method_signatures(const std::shared_ptr<AdvancedTraitMetaType>& trait, 
                                                  std::vector<SafetyViolation>& violations) const {
    bool is_safe = true;
    auto all_methods = trait->get_all_methods();
    
    for (const auto& method : all_methods) {
        // Check for methods that take Self by value
        // This is a simplified check - full implementation would parse method signatures
        if (method.name.find("self") != std::string::npos) {
            // Simplified heuristic - in practice would check actual parameter types
            violations.emplace_back(
                ObjectSafetyViolation::MethodTakesSelfByValue,
                method.name,
                std::format("Method '{}' may take Self by value", method.name)
            );
            is_safe = false;
        }
        
        // Check for methods that return Self
        if (method.name == "clone" || method.name == "new") {
            violations.emplace_back(
                ObjectSafetyViolation::MethodReturnsSelf,
                method.name,
                std::format("Method '{}' returns Self", method.name)
            );
            is_safe = false;
        }
    }
    
    return is_safe;
}

bool ObjectSafetyChecker::check_generic_methods(const std::shared_ptr<AdvancedTraitMetaType>& trait, 
                                                std::vector<SafetyViolation>& violations) const {
    bool is_safe = true;
    auto all_methods = trait->get_all_methods();
    
    for (const auto& method : all_methods) {
        // Check for generic methods (simplified)
        if (method.name.find("<") != std::string::npos) {
            violations.emplace_back(
                ObjectSafetyViolation::GenericMethod,
                method.name,
                std::format("Method '{}' is generic", method.name)
            );
            is_safe = false;
        }
    }
    
    return is_safe;
}

// TraitObjectFactory implementation
std::expected<std::shared_ptr<EnhancedTraitObject>, std::string>
TraitObjectFactory::create_trait_object(const std::shared_ptr<AdvancedTraitMetaType>& trait_type,
                                        const kernel::Value& concrete_value,
                                        const std::shared_ptr<MetaType>& concrete_type) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check object safety
    if (!safety_checker_.is_object_safe(trait_type).value_or(false)) {
        auto violations = safety_checker_.get_object_safety_violations(trait_type);
        std::string error_msg = "Trait is not object-safe: ";
        for (const auto& violation : violations) {
            error_msg += violation + "; ";
        }
        return std::unexpected(error_msg);
    }
    
    // Get or create vtable
    auto vtable_result = dispatcher_.get_vtable(trait_type, concrete_type);
    if (!vtable_result) {
        // Try to create vtable from registry
        auto& registry = AdvancedTraitRegistry::instance();
        auto impl_result = registry.find_implementation(concrete_type, trait_type);
        if (!impl_result) {
            return std::unexpected(impl_result.error());
        }
        
        auto vtable = std::make_shared<VTable>(trait_type, concrete_type, impl_result.value());
        dispatcher_.register_vtable(trait_type, concrete_type, vtable);
        vtable_result = vtable;
    }
    
    return std::make_shared<EnhancedTraitObject>(trait_type, concrete_value, vtable_result.value());
}

std::expected<std::shared_ptr<EnhancedTraitObject>, std::string>
TraitObjectFactory::create_trait_object_with_impl(const std::shared_ptr<AdvancedTraitMetaType>& trait_type,
                                                  const kernel::Value& concrete_value,
                                                  const std::shared_ptr<TraitImplementation>& implementation) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check object safety
    if (!safety_checker_.is_object_safe(trait_type).value_or(false)) {
        auto violations = safety_checker_.get_object_safety_violations(trait_type);
        std::string error_msg = "Trait is not object-safe: ";
        for (const auto& violation : violations) {
            error_msg += violation + "; ";
        }
        return std::unexpected(error_msg);
    }
    
    auto vtable = std::make_shared<VTable>(trait_type, implementation->implementing_type(), implementation);
    return std::make_shared<EnhancedTraitObject>(trait_type, concrete_value, vtable);
}

void TraitObjectFactory::register_trait_object_type(std::shared_ptr<TraitObjectMetaType> trait_object_type) {
    std::lock_guard<std::mutex> lock(mutex_);
    trait_object_types_[trait_object_type->trait_type()] = std::move(trait_object_type);
}

std::expected<std::shared_ptr<TraitObjectMetaType>, std::string>
TraitObjectFactory::get_trait_object_type(const std::shared_ptr<AdvancedTraitMetaType>& trait_type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = trait_object_types_.find(trait_type);
    
    if (it != trait_object_types_.end()) {
        return it->second;
    }
    
    return std::unexpected(std::format("No trait object type registered for trait '{}'", trait_type->name()));
}

bool TraitObjectFactory::can_create_trait_object(const std::shared_ptr<AdvancedTraitMetaType>& trait_type,
                                                 const std::shared_ptr<MetaType>& concrete_type) const {
    // Check object safety
    if (!safety_checker_.is_object_safe(trait_type).value_or(false)) {
        return false;
    }
    
    // Check if type implements trait
    return dispatcher_.type_implements_trait(concrete_type, trait_type);
}

// Utility functions
namespace trait_objects {

std::shared_ptr<TraitObjectMetaType> 
create_trait_object_type(std::shared_ptr<AdvancedTraitMetaType> trait_type) {
    return std::make_shared<TraitObjectMetaType>(std::move(trait_type));
}

bool is_object_safe(const std::shared_ptr<AdvancedTraitMetaType>& trait) {
    ObjectSafetyChecker checker;
    return checker.is_object_safe(trait).value_or(false);
}

std::vector<std::string> 
get_object_safety_violations(const std::shared_ptr<AdvancedTraitMetaType>& trait) {
    ObjectSafetyChecker checker;
    return checker.get_object_safety_violations(trait);
}

HigherRankedTraitBounds::HRTBound
create_hrtb(std::vector<std::string> lifetime_names,
            std::shared_ptr<AdvancedTraitMetaType> trait_type,
            std::map<std::string, std::shared_ptr<MetaType>> associated_type_constraints) {
    std::vector<HigherRankedTraitBounds::LifetimeParameter> lifetime_params;
    lifetime_params.reserve(lifetime_names.size());
    
    for (const auto& name : lifetime_names) {
        lifetime_params.emplace_back(name);
    }
    
    return HigherRankedTraitBounds::HRTBound(
        std::move(lifetime_params), 
        std::move(trait_type), 
        std::move(associated_type_constraints)
    );
}

std::expected<std::shared_ptr<EnhancedTraitObject>, std::string>
box_as_trait_object(const std::shared_ptr<AdvancedTraitMetaType>& trait_type,
                    const kernel::Value& value,
                    const std::shared_ptr<MetaType>& concrete_type) {
    return TraitObjectFactory::instance().create_trait_object(trait_type, value, concrete_type);
}

} // namespace trait_objects

} // namespace meld::meta