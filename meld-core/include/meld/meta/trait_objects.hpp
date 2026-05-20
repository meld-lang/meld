#pragma once

#include "advanced_traits.hpp"
#include "meld/kernel/primitives.hpp"
#include <string>
#include <memory>
#include <vector>
#include <map>
#include <expected>
#include <functional>
#include <typeinfo>
#include <mutex>

namespace meld::meta {

// Forward declarations
class VTable;
class TraitObjectMetaType;
class DynamicDispatcher;
class HigherRankedTraitBounds;

// Virtual function table for trait objects
class VTable {
public:
    VTable(std::shared_ptr<AdvancedTraitMetaType> trait_type,
           std::shared_ptr<MetaType> concrete_type,
           std::shared_ptr<TraitImplementation> implementation);
    
    // Get method implementation from vtable
    std::expected<kernel::Value, std::string> get_method(const std::string& method_name) const;
    
    // Call method through vtable
    std::expected<kernel::Value, std::string> 
    call_method(const std::string& method_name, 
                const kernel::Value& self_object,
                const std::vector<kernel::Value>& args) const;
    
    // Get trait and concrete type information
    const std::shared_ptr<AdvancedTraitMetaType>& trait_type() const { return trait_type_; }
    const std::shared_ptr<MetaType>& concrete_type() const { return concrete_type_; }
    const std::shared_ptr<TraitImplementation>& implementation() const { return implementation_; }
    
    // Check if method exists in vtable
    bool has_method(const std::string& method_name) const;
    
    // Get all method names in vtable
    std::vector<std::string> get_method_names() const;
    
private:
    std::shared_ptr<AdvancedTraitMetaType> trait_type_;
    std::shared_ptr<MetaType> concrete_type_;
    std::shared_ptr<TraitImplementation> implementation_;
    std::map<std::string, kernel::Value> method_table_;
    
    void build_method_table();
};

// Enhanced trait object with full dynamic dispatch support
class EnhancedTraitObject {
public:
    EnhancedTraitObject(std::shared_ptr<AdvancedTraitMetaType> trait_type,
                        kernel::Value object,
                        std::shared_ptr<VTable> vtable);
    
    // Dynamic method dispatch
    std::expected<kernel::Value, std::string>
    call_method(const std::string& method_name, 
                const std::vector<kernel::Value>& args) const;
    
    // Get associated type from the concrete implementation
    std::expected<std::shared_ptr<MetaType>, std::string>
    get_associated_type(const std::string& name) const;
    
    // Type information
    const std::shared_ptr<AdvancedTraitMetaType>& trait_type() const { return trait_type_; }
    const std::shared_ptr<MetaType>& concrete_type() const { return vtable_->concrete_type(); }
    const kernel::Value& object() const { return object_; }
    const std::shared_ptr<VTable>& vtable() const { return vtable_; }
    
    // Object safety validation
    bool is_object_safe() const;
    std::vector<std::string> get_object_safety_violations() const;
    
    // Downcasting support
    template<typename T>
    std::expected<T*, std::string> downcast() const {
        // This would attempt to downcast the trait object to a concrete type
        // For now, we'll return an error as this requires runtime type information
        return std::unexpected("Downcasting not implemented");
    }
    
    // Clone support (if the trait supports cloning)
    std::expected<std::shared_ptr<EnhancedTraitObject>, std::string> clone() const;
    
    // Equality comparison (if the trait supports equality)
    std::expected<bool, std::string> equals(const EnhancedTraitObject& other) const;
    
private:
    std::shared_ptr<AdvancedTraitMetaType> trait_type_;
    kernel::Value object_;
    std::shared_ptr<VTable> vtable_;
};

// MetaType for trait objects
class TraitObjectMetaType : public MetaType {
public:
    TraitObjectMetaType(std::shared_ptr<AdvancedTraitMetaType> trait_type)
        : trait_type_(std::move(trait_type)) {}
    
    std::string name() const override { 
        return "dyn " + trait_type_->name(); 
    }
    
    size_t size() const override { 
        return sizeof(void*) * 2; // Object pointer + vtable pointer
    }
    
    bool is_value_type() const override { return false; }
    
    const std::shared_ptr<AdvancedTraitMetaType>& trait_type() const { return trait_type_; }
    
    // Check if a concrete type can be converted to this trait object
    bool can_convert_from(const std::shared_ptr<MetaType>& concrete_type) const;
    
    // Create trait object from concrete value
    std::expected<std::shared_ptr<EnhancedTraitObject>, std::string>
    create_trait_object(const kernel::Value& concrete_value) const;
    
private:
    std::shared_ptr<AdvancedTraitMetaType> trait_type_;
};

// Dynamic dispatcher for trait object method calls
class DynamicDispatcher {
public:
    // Register a vtable for a specific trait-type combination
    void register_vtable(std::shared_ptr<AdvancedTraitMetaType> trait_type,
                         std::shared_ptr<MetaType> concrete_type,
                         std::shared_ptr<VTable> vtable);
    
    // Get vtable for a trait-type combination
    std::expected<std::shared_ptr<VTable>, std::string>
    get_vtable(const std::shared_ptr<AdvancedTraitMetaType>& trait_type,
               const std::shared_ptr<MetaType>& concrete_type) const;
    
    // Create trait object with automatic vtable lookup
    std::expected<std::shared_ptr<EnhancedTraitObject>, std::string>
    create_trait_object(const std::shared_ptr<AdvancedTraitMetaType>& trait_type,
                        const kernel::Value& concrete_value,
                        const std::shared_ptr<MetaType>& concrete_type) const;
    
    // Dispatch method call to appropriate implementation
    std::expected<kernel::Value, std::string>
    dispatch_method(const std::shared_ptr<AdvancedTraitMetaType>& trait_type,
                    const std::shared_ptr<MetaType>& concrete_type,
                    const std::string& method_name,
                    const kernel::Value& self_object,
                    const std::vector<kernel::Value>& args) const;
    
    // Get all registered vtables
    std::vector<std::shared_ptr<VTable>> get_all_vtables() const;
    
    // Check if a type implements a trait (for trait object creation)
    bool type_implements_trait(const std::shared_ptr<MetaType>& type,
                               const std::shared_ptr<AdvancedTraitMetaType>& trait) const;
    
private:
    std::map<std::pair<std::shared_ptr<AdvancedTraitMetaType>, std::shared_ptr<MetaType>>, 
             std::shared_ptr<VTable>> vtables_;
    mutable std::mutex mutex_;
};

// Higher-ranked trait bounds support
class HigherRankedTraitBounds {
public:
    // Lifetime parameter information
    struct LifetimeParameter {
        std::string name;
        std::vector<std::string> bounds;  // Lifetime bounds
        
        LifetimeParameter(std::string n, std::vector<std::string> b = {})
            : name(std::move(n)), bounds(std::move(b)) {}
    };
    
    // Higher-ranked trait bound
    struct HRTBound {
        std::vector<LifetimeParameter> lifetime_params;
        std::shared_ptr<AdvancedTraitMetaType> trait_type;
        std::map<std::string, std::shared_ptr<MetaType>> associated_type_constraints;
        
        HRTBound(std::vector<LifetimeParameter> lifetimes,
                 std::shared_ptr<AdvancedTraitMetaType> trait,
                 std::map<std::string, std::shared_ptr<MetaType>> constraints = {})
            : lifetime_params(std::move(lifetimes))
            , trait_type(std::move(trait))
            , associated_type_constraints(std::move(constraints)) {}
    };
    
    // Check if a type satisfies a higher-ranked trait bound
    std::expected<bool, std::string>
    satisfies_hrtb(const std::shared_ptr<MetaType>& type, const HRTBound& bound) const;
    
    // Create trait object with higher-ranked trait bounds
    std::expected<std::shared_ptr<EnhancedTraitObject>, std::string>
    create_hrtb_trait_object(const HRTBound& bound, const kernel::Value& concrete_value) const;
    
    // Validate higher-ranked trait bound
    std::expected<void, std::string> validate_hrtb(const HRTBound& bound) const;
    
    // Get all lifetime parameters from a bound
    std::vector<std::string> get_lifetime_parameter_names(const HRTBound& bound) const;
    
private:
    // Helper to check lifetime constraints
    bool check_lifetime_constraints(const std::vector<LifetimeParameter>& params) const;
};

// Object safety checker
class ObjectSafetyChecker {
public:
    // Check if a trait is object-safe
    std::expected<bool, std::string> is_object_safe(const std::shared_ptr<AdvancedTraitMetaType>& trait) const;
    
    // Get detailed object safety violations
    std::vector<std::string> get_object_safety_violations(const std::shared_ptr<AdvancedTraitMetaType>& trait) const;
    
    // Object safety rules
    enum class ObjectSafetyViolation {
        AssociatedTypeWithoutDefault,
        MethodTakesSelfByValue,
        GenericMethod,
        MethodReturnsSelf,
        AssociatedConstant,
        StaticMethod
    };
    
    struct SafetyViolation {
        ObjectSafetyViolation type;
        std::string method_name;
        std::string description;
        
        SafetyViolation(ObjectSafetyViolation t, std::string method = "", std::string desc = "")
            : type(t), method_name(std::move(method)), description(std::move(desc)) {}
    };
    
    // Get detailed safety violations
    std::vector<SafetyViolation> analyze_object_safety(const std::shared_ptr<AdvancedTraitMetaType>& trait) const;
    
private:
    // Check individual object safety rules
    bool check_associated_types(const std::shared_ptr<AdvancedTraitMetaType>& trait, std::vector<SafetyViolation>& violations) const;
    bool check_method_signatures(const std::shared_ptr<AdvancedTraitMetaType>& trait, std::vector<SafetyViolation>& violations) const;
    bool check_generic_methods(const std::shared_ptr<AdvancedTraitMetaType>& trait, std::vector<SafetyViolation>& violations) const;
};

// Trait object factory
class TraitObjectFactory {
public:
    static TraitObjectFactory& instance() {
        static TraitObjectFactory factory;
        return factory;
    }
    
    // Create trait object from concrete value
    std::expected<std::shared_ptr<EnhancedTraitObject>, std::string>
    create_trait_object(const std::shared_ptr<AdvancedTraitMetaType>& trait_type,
                        const kernel::Value& concrete_value,
                        const std::shared_ptr<MetaType>& concrete_type);
    
    // Create trait object with explicit implementation
    std::expected<std::shared_ptr<EnhancedTraitObject>, std::string>
    create_trait_object_with_impl(const std::shared_ptr<AdvancedTraitMetaType>& trait_type,
                                  const kernel::Value& concrete_value,
                                  const std::shared_ptr<TraitImplementation>& implementation);
    
    // Register trait object type
    void register_trait_object_type(std::shared_ptr<TraitObjectMetaType> trait_object_type);
    
    // Get trait object type
    std::expected<std::shared_ptr<TraitObjectMetaType>, std::string>
    get_trait_object_type(const std::shared_ptr<AdvancedTraitMetaType>& trait_type) const;
    
    // Check if trait object creation is possible
    bool can_create_trait_object(const std::shared_ptr<AdvancedTraitMetaType>& trait_type,
                                 const std::shared_ptr<MetaType>& concrete_type) const;
    
private:
    TraitObjectFactory() = default;
    
    DynamicDispatcher dispatcher_;
    ObjectSafetyChecker safety_checker_;
    std::map<std::shared_ptr<AdvancedTraitMetaType>, std::shared_ptr<TraitObjectMetaType>> trait_object_types_;
    mutable std::mutex mutex_;
};

// Utility functions for trait objects
namespace trait_objects {

// Create a trait object type
std::shared_ptr<TraitObjectMetaType> 
create_trait_object_type(std::shared_ptr<AdvancedTraitMetaType> trait_type);

// Check if a trait is object-safe
bool is_object_safe(const std::shared_ptr<AdvancedTraitMetaType>& trait);

// Get object safety violations
std::vector<std::string> 
get_object_safety_violations(const std::shared_ptr<AdvancedTraitMetaType>& trait);

// Create higher-ranked trait bound
HigherRankedTraitBounds::HRTBound
create_hrtb(std::vector<std::string> lifetime_names,
            std::shared_ptr<AdvancedTraitMetaType> trait_type,
            std::map<std::string, std::shared_ptr<MetaType>> associated_type_constraints = {});

// Box a value into a trait object (similar to Rust's Box<dyn Trait>)
std::expected<std::shared_ptr<EnhancedTraitObject>, std::string>
box_as_trait_object(const std::shared_ptr<AdvancedTraitMetaType>& trait_type,
                    const kernel::Value& value,
                    const std::shared_ptr<MetaType>& concrete_type);

} // namespace trait_objects

} // namespace meld::meta