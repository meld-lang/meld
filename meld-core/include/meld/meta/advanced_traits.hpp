#pragma once

#include "metatype.hpp"
#include <string>
#include <memory>
#include <vector>
#include <map>
#include <mutex>
#include <expected>
#include <unordered_set>
#include <functional>

namespace meld::meta {

// Forward declarations
class AssociatedType;
class TraitImplementation;
class CoherenceChecker;
class TraitObject;

// Associated type definition within a trait
struct AssociatedType {
    std::string name;
    std::shared_ptr<MetaType> default_type;  // Optional default implementation
    std::vector<std::shared_ptr<MetaType>> bounds;  // Type constraints
    
    AssociatedType(std::string n, 
                   std::shared_ptr<MetaType> default_impl = nullptr,
                   std::vector<std::shared_ptr<MetaType>> type_bounds = {})
        : name(std::move(n))
        , default_type(std::move(default_impl))
        , bounds(std::move(type_bounds)) {}
};

// Enhanced trait with associated types and type families
class AdvancedTraitMetaType : public TraitMetaType {
public:
    AdvancedTraitMetaType(std::string name, 
                          std::vector<Method> methods,
                          std::vector<AssociatedType> associated_types = {},
                          std::vector<std::shared_ptr<MetaType>> super_traits = {})
        : TraitMetaType(std::move(name), std::move(methods))
        , associated_types_(std::move(associated_types))
        , super_traits_(std::move(super_traits)) {}
    
    const std::vector<AssociatedType>& associated_types() const { return associated_types_; }
    const std::vector<std::shared_ptr<MetaType>>& super_traits() const { return super_traits_; }
    
    std::expected<const AssociatedType*, std::string> get_associated_type(const std::string& name) const;
    bool has_associated_type(const std::string& name) const;
    
    // Check if this trait extends another trait
    bool extends_trait(const std::shared_ptr<MetaType>& other_trait) const;
    
    // Get all methods including inherited ones
    std::vector<Method> get_all_methods() const;
    
private:
    std::vector<AssociatedType> associated_types_;
    std::vector<std::shared_ptr<MetaType>> super_traits_;  // Trait inheritance
};

// Trait implementation for a specific type
class TraitImplementation {
public:
    TraitImplementation(std::shared_ptr<MetaType> implementing_type,
                        std::shared_ptr<AdvancedTraitMetaType> trait_type,
                        std::map<std::string, std::shared_ptr<MetaType>> associated_type_bindings = {},
                        std::map<std::string, kernel::Value> method_implementations = {})
        : implementing_type_(std::move(implementing_type))
        , trait_type_(std::move(trait_type))
        , associated_type_bindings_(std::move(associated_type_bindings))
        , method_implementations_(std::move(method_implementations)) {}
    
    const std::shared_ptr<MetaType>& implementing_type() const { return implementing_type_; }
    const std::shared_ptr<AdvancedTraitMetaType>& trait_type() const { return trait_type_; }
    const std::map<std::string, std::shared_ptr<MetaType>>& associated_type_bindings() const { 
        return associated_type_bindings_; 
    }
    const std::map<std::string, kernel::Value>& method_implementations() const { 
        return method_implementations_; 
    }
    
    // Get the concrete type for an associated type
    std::expected<std::shared_ptr<MetaType>, std::string> 
    get_associated_type_binding(const std::string& name) const;
    
    // Get method implementation
    std::expected<kernel::Value, std::string> 
    get_method_implementation(const std::string& name) const;
    
    // Validate that this implementation satisfies the trait requirements
    std::expected<void, std::string> validate() const;
    
private:
    std::shared_ptr<MetaType> implementing_type_;
    std::shared_ptr<AdvancedTraitMetaType> trait_type_;
    std::map<std::string, std::shared_ptr<MetaType>> associated_type_bindings_;
    std::map<std::string, kernel::Value> method_implementations_;
};

// Coherence checker to prevent conflicting trait implementations
class CoherenceChecker {
public:
    // Check if a new trait implementation would conflict with existing ones
    std::expected<void, std::string> 
    check_implementation_coherence(const TraitImplementation& new_impl) const;
    
    // Register a new trait implementation
    std::expected<void, std::string> 
    register_implementation(std::shared_ptr<TraitImplementation> impl);
    
    // Check orphan rule: can only implement trait for type if you own either the trait or the type
    std::expected<void, std::string> 
    check_orphan_rule(const TraitImplementation& impl) const;
    
    // Find trait implementation for a specific type and trait
    std::expected<std::shared_ptr<TraitImplementation>, std::string>
    find_implementation(const std::shared_ptr<MetaType>& type,
                        const std::shared_ptr<AdvancedTraitMetaType>& trait) const;
    
    // Get all implementations for a trait
    std::vector<std::shared_ptr<TraitImplementation>>
    get_implementations_for_trait(const std::shared_ptr<AdvancedTraitMetaType>& trait) const;
    
    // Get all implementations for a type
    std::vector<std::shared_ptr<TraitImplementation>>
    get_implementations_for_type(const std::shared_ptr<MetaType>& type) const;
    
    // Check if a type implements a trait
    bool type_implements_trait(const std::shared_ptr<MetaType>& type,
                               const std::shared_ptr<AdvancedTraitMetaType>& trait) const;
    
private:
    // Map from (type, trait) -> implementation
    std::map<std::pair<std::shared_ptr<MetaType>, std::shared_ptr<AdvancedTraitMetaType>>, 
             std::shared_ptr<TraitImplementation>> implementations_;
    
    // Track which crate/module owns which types and traits for orphan rule
    std::map<std::shared_ptr<MetaType>, std::string> type_owners_;
    std::map<std::shared_ptr<AdvancedTraitMetaType>, std::string> trait_owners_;
    
    mutable std::mutex mutex_;
};

// Trait object for dynamic dispatch
class TraitObject {
public:
    TraitObject(std::shared_ptr<AdvancedTraitMetaType> trait_type,
                kernel::Value object,
                std::shared_ptr<TraitImplementation> implementation)
        : trait_type_(std::move(trait_type))
        , object_(std::move(object))
        , implementation_(std::move(implementation)) {}
    
    const std::shared_ptr<AdvancedTraitMetaType>& trait_type() const { return trait_type_; }
    const kernel::Value& object() const { return object_; }
    const std::shared_ptr<TraitImplementation>& implementation() const { return implementation_; }
    
    // Call a method on the trait object
    std::expected<kernel::Value, std::string>
    call_method(const std::string& method_name, 
                const std::vector<kernel::Value>& args) const;
    
    // Get associated type binding from the implementation
    std::expected<std::shared_ptr<MetaType>, std::string>
    get_associated_type(const std::string& name) const;
    
    // Check trait object safety rules
    bool is_object_safe() const;
    
private:
    std::shared_ptr<AdvancedTraitMetaType> trait_type_;
    kernel::Value object_;
    std::shared_ptr<TraitImplementation> implementation_;
};

// Higher-ranked trait bounds (for all lifetimes)
struct HigherRankedTraitBound {
    std::vector<std::string> lifetime_parameters;
    std::shared_ptr<AdvancedTraitMetaType> trait_type;
    std::map<std::string, std::shared_ptr<MetaType>> associated_type_constraints;
    
    HigherRankedTraitBound(std::vector<std::string> lifetimes,
                           std::shared_ptr<AdvancedTraitMetaType> trait,
                           std::map<std::string, std::shared_ptr<MetaType>> constraints = {})
        : lifetime_parameters(std::move(lifetimes))
        , trait_type(std::move(trait))
        , associated_type_constraints(std::move(constraints)) {}
};

// Blanket implementation support
class BlanketImplementation {
public:
    BlanketImplementation(std::shared_ptr<AdvancedTraitMetaType> trait_type,
                          std::function<bool(const std::shared_ptr<MetaType>&)> type_predicate,
                          std::map<std::string, kernel::Value> method_implementations)
        : trait_type_(std::move(trait_type))
        , type_predicate_(std::move(type_predicate))
        , method_implementations_(std::move(method_implementations)) {}
    
    const std::shared_ptr<AdvancedTraitMetaType>& trait_type() const { return trait_type_; }
    
    // Check if this blanket implementation applies to a type
    bool applies_to_type(const std::shared_ptr<MetaType>& type) const;
    
    // Create a concrete implementation for a specific type
    std::expected<std::shared_ptr<TraitImplementation>, std::string>
    instantiate_for_type(const std::shared_ptr<MetaType>& type) const;
    
private:
    std::shared_ptr<AdvancedTraitMetaType> trait_type_;
    std::function<bool(const std::shared_ptr<MetaType>&)> type_predicate_;
    std::map<std::string, kernel::Value> method_implementations_;
};

// Advanced trait registry with coherence checking
class AdvancedTraitRegistry {
public:
    static AdvancedTraitRegistry& instance() {
        static AdvancedTraitRegistry registry;
        return registry;
    }
    
    // Register a trait
    std::expected<void, std::string>
    register_trait(std::shared_ptr<AdvancedTraitMetaType> trait);
    
    // Register a trait implementation
    std::expected<void, std::string>
    register_implementation(std::shared_ptr<TraitImplementation> impl);
    
    // Register a blanket implementation
    std::expected<void, std::string>
    register_blanket_implementation(std::shared_ptr<BlanketImplementation> blanket_impl);
    
    // Find trait by name
    std::expected<std::shared_ptr<AdvancedTraitMetaType>, std::string>
    get_trait(const std::string& name) const;
    
    // Find implementation for type and trait
    std::expected<std::shared_ptr<TraitImplementation>, std::string>
    find_implementation(const std::shared_ptr<MetaType>& type,
                        const std::shared_ptr<AdvancedTraitMetaType>& trait) const;
    
    // Create trait object
    std::expected<std::shared_ptr<TraitObject>, std::string>
    create_trait_object(const std::shared_ptr<AdvancedTraitMetaType>& trait,
                        const kernel::Value& object) const;
    
    // Check if type implements trait (including blanket implementations)
    bool type_implements_trait(const std::shared_ptr<MetaType>& type,
                               const std::shared_ptr<AdvancedTraitMetaType>& trait) const;
    
    // Get all traits implemented by a type
    std::vector<std::shared_ptr<AdvancedTraitMetaType>>
    get_implemented_traits(const std::shared_ptr<MetaType>& type) const;
    
private:
    AdvancedTraitRegistry() = default;
    
    std::map<std::string, std::shared_ptr<AdvancedTraitMetaType>> traits_;
    CoherenceChecker coherence_checker_;
    std::vector<std::shared_ptr<BlanketImplementation>> blanket_implementations_;
    mutable std::mutex mutex_;
};

// Factory functions for creating advanced traits
std::shared_ptr<AdvancedTraitMetaType> 
create_advanced_trait(std::string name,
                      std::vector<Method> methods,
                      std::vector<AssociatedType> associated_types = {},
                      std::vector<std::shared_ptr<MetaType>> super_traits = {});

std::shared_ptr<TraitImplementation>
create_trait_implementation(std::shared_ptr<MetaType> implementing_type,
                            std::shared_ptr<AdvancedTraitMetaType> trait_type,
                            std::map<std::string, std::shared_ptr<MetaType>> associated_type_bindings = {},
                            std::map<std::string, kernel::Value> method_implementations = {});

std::shared_ptr<BlanketImplementation>
create_blanket_implementation(std::shared_ptr<AdvancedTraitMetaType> trait_type,
                              std::function<bool(const std::shared_ptr<MetaType>&)> type_predicate,
                              std::map<std::string, kernel::Value> method_implementations);

} // namespace meld::meta