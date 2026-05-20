#pragma once

#include "meld/kernel/primitives.hpp"
#include <string>
#include <memory>
#include <vector>
#include <map>
#include <mutex>
#include <expected>
#include <concepts>
#include <typeinfo>

namespace meld::meta {

// Forward declarations
class MetaType;
class PrimitiveMetaType;
class StructMetaType;
class ClassMetaType;
class EnumMetaType;
class NewtypeMetaType;
class TraitMetaType;
class UnionMetaType;
class IntersectionMetaType;
class GenericMetaType;
class FunctionMetaType;
class RefinementMetaType;

// Variance for generic type parameters
enum class Variance {
    Invariant,    // T
    Covariant,    // out T
    Contravariant // in T
};

// Field descriptor
struct Field {
    std::string name;
    std::shared_ptr<MetaType> type;
    bool is_mutable;
    
    Field(std::string n, std::shared_ptr<MetaType> t, bool mut = false)
        : name(std::move(n)), type(std::move(t)), is_mutable(mut) {}
};

// Enum variant descriptor
struct EnumVariant {
    std::string name;
    std::vector<Field> associated_fields;
    
    EnumVariant(std::string n, std::vector<Field> fields = {})
        : name(std::move(n)), associated_fields(std::move(fields)) {}
};

// Property accessor types
enum class AccessModifier {
    Public,
    Private,
    Protected,
    Internal
};

// Property descriptor
struct Property {
    std::string name;
    std::shared_ptr<MetaType> type;
    bool is_mutable;
    bool has_backing_field;
    std::string backing_field_name;
    
    // Custom accessors
    bool has_custom_getter;
    bool has_custom_setter;
    kernel::Value getter_impl;  // Function implementing getter
    kernel::Value setter_impl;  // Function implementing setter
    
    // Property delegation
    bool is_delegated;
    std::string delegate_name;  // Name of the delegate object/field
    kernel::Value delegate_impl;  // Delegate object implementing get/set
    
    // Access modifiers
    AccessModifier getter_access;
    AccessModifier setter_access;
    
    Property(std::string n, std::shared_ptr<MetaType> t, bool mut = false)
        : name(std::move(n))
        , type(std::move(t))
        , is_mutable(mut)
        , has_backing_field(true)
        , backing_field_name("_" + name)
        , has_custom_getter(false)
        , has_custom_setter(false)
        , is_delegated(false)
        , getter_access(AccessModifier::Public)
        , setter_access(AccessModifier::Public) {}
};

// Method descriptor
struct Method {
    std::string name;
    std::vector<std::shared_ptr<MetaType>> param_types;
    std::shared_ptr<MetaType> return_type;
    kernel::Value implementation;
    bool is_mutating = false;  // true when declared with 'var fnc' (Req 57.8)
    
    Method(std::string n, 
           std::vector<std::shared_ptr<MetaType>> params,
           std::shared_ptr<MetaType> ret,
           kernel::Value impl,
           bool mutating = false)
        : name(std::move(n))
        , param_types(std::move(params))
        , return_type(std::move(ret))
        , implementation(std::move(impl))
        , is_mutating(mutating) {}
};

// Base MetaType class - the type of all types
class MetaType : public std::enable_shared_from_this<MetaType> {
public:
    virtual ~MetaType() = default;
    
    // Type information
    virtual std::string name() const = 0;
    virtual size_t size() const = 0;
    virtual bool is_value_type() const = 0;
    
    // Annotation support for @transpile_as and other metadata
    void add_annotation(const std::string& name, const std::map<std::string, std::string>& values);
    void add_annotation(const std::string& name, const std::string& value = "");
    std::map<std::string, std::map<std::string, std::string>> get_annotations() const;
    bool has_annotation(const std::string& name) const;
    std::expected<std::map<std::string, std::string>, std::string> get_annotation(const std::string& name) const;
    
    // RTTI support - get runtime type information
    virtual const std::type_info& type_info() const {
        return typeid(*this);
    }
    
    // Get type name from RTTI
    std::string type_name() const {
        return type_info().name();
    }
    
    // Safe downcasting using dynamic_cast
    template<typename T>
    T* as() {
        return dynamic_cast<T*>(this);
    }
    
    template<typename T>
    const T* as() const {
        return dynamic_cast<const T*>(this);
    }
    
    // Type checking using dynamic_cast
    template<typename T>
    bool is() const {
        return dynamic_cast<const T*>(this) != nullptr;
    }
    
    // Type checking
    virtual bool is_assignable_from(const MetaType& other) const;
    virtual bool is_subtype_of(const MetaType& other) const;
    
    // Type operations - typeof returns MetaType instances
    // MetaType is the type of all types, including itself
    static std::shared_ptr<MetaType> typeof_value(const MetaType& type);
    std::shared_ptr<MetaType> get_metatype() const;
    
    // Factory methods
    static std::shared_ptr<MetaType> create_primitive(std::string name, size_t size);
    static std::shared_ptr<MetaType> create_struct(std::string name, std::vector<Field> fields, std::vector<Property> properties = {});
    static std::shared_ptr<MetaType> create_class(std::string name, std::vector<Field> fields, std::vector<Method> methods, std::vector<Property> properties = {});
    static std::shared_ptr<MetaType> create_enum(std::string name, std::vector<EnumVariant> variants);
    static std::shared_ptr<MetaType> create_newtype(std::string name, std::shared_ptr<MetaType> wrapped_type, bool is_transparent = true);
    static std::shared_ptr<MetaType> create_trait(std::string name, std::vector<Method> methods);
    static std::shared_ptr<MetaType> create_union(std::vector<std::shared_ptr<MetaType>> types);
    static std::shared_ptr<MetaType> create_intersection(std::vector<std::shared_ptr<MetaType>> types);
    static std::shared_ptr<MetaType> create_refinement(std::string name, std::shared_ptr<MetaType> base_type, kernel::Value predicate);
    
    // Get the singleton MetaType instance (the type of all types)
    static std::shared_ptr<MetaType> get_metatype_instance();
    
    // C++23: Comparison
    virtual bool operator==(const MetaType& other) const {
        return name() == other.name();
    }
    
protected:
    MetaType() = default;
    
    // Annotation storage
    std::map<std::string, std::map<std::string, std::string>> annotations_;
};

// MetaTypeMetaType - the type of MetaType itself (self-referential)
class MetaTypeMetaType : public MetaType {
public:
    MetaTypeMetaType() = default;
    
    std::string name() const override { return "MetaType"; }
    size_t size() const override { return sizeof(void*); }
    bool is_value_type() const override { return false; }
    
    // MetaType's type is itself
    std::shared_ptr<MetaType> get_metatype() const;
};

// PrimitiveMetaType - for kernel primitives
class PrimitiveMetaType : public MetaType {
public:
    PrimitiveMetaType(std::string name, size_t size)
        : name_(std::move(name)), size_(size) {}
    
    std::string name() const override { return name_; }
    size_t size() const override { return size_; }
    bool is_value_type() const override { return true; }
    
private:
    std::string name_;
    size_t size_;
};

// StructMetaType - value types with copy-by-value semantics
// Structs are automatically Copyable
class StructMetaType : public MetaType {
public:
    StructMetaType(std::string name, std::vector<Field> fields, std::vector<Property> properties = {})
        : name_(std::move(name)), fields_(std::move(fields)), properties_(std::move(properties)) {}
    
    std::string name() const override { return name_; }
    size_t size() const override;
    bool is_value_type() const override { return true; }
    
    const std::vector<Field>& fields() const { return fields_; }
    const std::vector<Property>& properties() const { return properties_; }
    std::expected<const Field*, std::string> get_field(const std::string& name) const;
    std::expected<const Property*, std::string> get_property(const std::string& name) const;
    
    // Structs are automatically copyable
    bool is_copyable() const { return true; }
    
private:
    std::string name_;
    std::vector<Field> fields_;
    std::vector<Property> properties_;
};

// ClassMetaType - reference types with managed memory
class ClassMetaType : public MetaType {
public:
    ClassMetaType(std::string name, 
                  std::vector<Field> fields,
                  std::vector<Method> methods,
                  std::vector<Property> properties = {},
                  std::shared_ptr<ClassMetaType> base = nullptr)
        : name_(std::move(name))
        , fields_(std::move(fields))
        , methods_(std::move(methods))
        , properties_(std::move(properties))
        , base_class_(std::move(base)) {}
    
    std::string name() const override { return name_; }
    size_t size() const override { return sizeof(void*); } // Reference type
    bool is_value_type() const override { return false; }
    
    const std::vector<Field>& fields() const { return fields_; }
    const std::vector<Method>& methods() const { return methods_; }
    const std::vector<Property>& properties() const { return properties_; }
    const std::shared_ptr<ClassMetaType>& base_class() const { return base_class_; }
    
    std::expected<const Field*, std::string> get_field(const std::string& name) const;
    std::expected<const Method*, std::string> get_method(const std::string& name) const;
    std::expected<const Property*, std::string> get_property(const std::string& name) const;
    
    bool is_subtype_of(const MetaType& other) const override;
    
private:
    std::string name_;
    std::vector<Field> fields_;
    std::vector<Method> methods_;
    std::vector<Property> properties_;
    std::shared_ptr<ClassMetaType> base_class_;
};

// EnumMetaType - algebraic data types with variants
class EnumMetaType : public MetaType {
public:
    EnumMetaType(std::string name, std::vector<EnumVariant> variants)
        : name_(std::move(name)), variants_(std::move(variants)) {}
    
    std::string name() const override { return name_; }
    size_t size() const override;
    bool is_value_type() const override { return true; }
    
    const std::vector<EnumVariant>& variants() const { return variants_; }
    std::expected<const EnumVariant*, std::string> get_variant(const std::string& name) const;
    
    // Check if a variant exists
    bool has_variant(const std::string& name) const;
    
    // Get variant by index
    std::expected<const EnumVariant*, std::string> get_variant_by_index(size_t index) const;
    
private:
    std::string name_;
    std::vector<EnumVariant> variants_;
};

// NewtypeMetaType - zero-cost type safety wrappers
class NewtypeMetaType : public MetaType {
public:
    NewtypeMetaType(std::string name, std::shared_ptr<MetaType> wrapped_type, bool is_transparent = true)
        : name_(std::move(name)), wrapped_type_(std::move(wrapped_type)), is_transparent_(is_transparent) {}
    
    std::string name() const override { return name_; }
    size_t size() const override { 
        // Zero-cost: same size as wrapped type
        return wrapped_type_->size(); 
    }
    bool is_value_type() const override { 
        // Inherit value semantics from wrapped type
        return wrapped_type_->is_value_type(); 
    }
    
    const std::shared_ptr<MetaType>& wrapped_type() const { return wrapped_type_; }
    bool is_transparent() const { return is_transparent_; }
    
    // Newtype provides type safety while maintaining zero-cost abstraction
    // At compile time: distinct type for safety
    // At runtime: same representation as wrapped type (if transparent)
    
private:
    std::string name_;
    std::shared_ptr<MetaType> wrapped_type_;
    bool is_transparent_;  // Zero-cost optimization flag
};

// TraitMetaType - interfaces with default implementations
class TraitMetaType : public MetaType {
public:
    TraitMetaType(std::string name, std::vector<Method> methods)
        : name_(std::move(name)), methods_(std::move(methods)) {}
    
    std::string name() const override { return name_; }
    size_t size() const override { return 0; } // Interface has no size
    bool is_value_type() const override { return false; }
    
    const std::vector<Method>& methods() const { return methods_; }
    std::expected<const Method*, std::string> get_method(const std::string& name) const;
    
private:
    std::string name_;
    std::vector<Method> methods_;
};

// UnionMetaType - Type | Type
class UnionMetaType : public MetaType {
public:
    explicit UnionMetaType(std::vector<std::shared_ptr<MetaType>> types)
        : types_(std::move(types)) {}
    
    std::string name() const override;
    size_t size() const override;
    bool is_value_type() const override { return false; }
    
    const std::vector<std::shared_ptr<MetaType>>& types() const { return types_; }
    bool contains_type(const MetaType& type) const;
    bool is_assignable_from(const MetaType& other) const override;
    bool is_subtype_of(const MetaType& other) const override;
    
private:
    std::vector<std::shared_ptr<MetaType>> types_;
};

// IntersectionMetaType - Type & Type
class IntersectionMetaType : public MetaType {
public:
    explicit IntersectionMetaType(std::vector<std::shared_ptr<MetaType>> types)
        : types_(std::move(types)) {}
    
    std::string name() const override;
    size_t size() const override;
    bool is_value_type() const override { return false; }
    
    const std::vector<std::shared_ptr<MetaType>>& types() const { return types_; }
    bool satisfies_all(const MetaType& type) const;
    bool is_assignable_from(const MetaType& other) const override;
    bool is_subtype_of(const MetaType& other) const override;
    
private:
    std::vector<std::shared_ptr<MetaType>> types_;
};

// GenericMetaType - Generic type parameters
class GenericMetaType : public MetaType {
public:
    GenericMetaType(std::string name, 
                    Variance variance,
                    std::shared_ptr<MetaType> bound = nullptr)
        : name_(std::move(name))
        , variance_(variance)
        , bound_(std::move(bound)) {}
    
    std::string name() const override { return name_; }
    size_t size() const override { return 0; } // Type parameter has no size
    bool is_value_type() const override { return false; }
    
    Variance variance() const { return variance_; }
    const std::shared_ptr<MetaType>& bound() const { return bound_; }
    
    // Instantiate with concrete type
    std::expected<std::shared_ptr<MetaType>, std::string> 
    instantiate(std::shared_ptr<MetaType> concrete_type) const;
    
private:
    std::string name_;
    Variance variance_;
    std::shared_ptr<MetaType> bound_; // Upper bound constraint
};

// FunctionMetaType - Function types with parameter and return types
class FunctionMetaType : public MetaType {
public:
    FunctionMetaType(std::vector<std::shared_ptr<MetaType>> param_types,
                     std::shared_ptr<MetaType> return_type,
                     std::vector<std::string> named_returns = {})
        : param_types_(std::move(param_types))
        , return_type_(std::move(return_type))
        , named_returns_(std::move(named_returns)) {}
    
    std::string name() const override {
        std::string result = "(";
        for (size_t i = 0; i < param_types_.size(); ++i) {
            if (i > 0) result += ", ";
            result += param_types_[i]->name();
        }
        result += ") => ";
        
        if (!named_returns_.empty()) {
            result += "(";
            for (size_t i = 0; i < named_returns_.size(); ++i) {
                if (i > 0) result += ", ";
                result += named_returns_[i] + ": " + return_type_->name();
            }
            result += ")";
        } else {
            result += return_type_->name();
        }
        
        return result;
    }
    
    size_t size() const override { return sizeof(void*); } // Function pointer size
    bool is_value_type() const override { return false; }
    
    const std::vector<std::shared_ptr<MetaType>>& param_types() const { return param_types_; }
    const std::shared_ptr<MetaType>& return_type() const { return return_type_; }
    const std::vector<std::string>& named_returns() const { return named_returns_; }
    
    // Check if this function type is compatible with another
    bool is_compatible_with(const FunctionMetaType& other) const;
    
    // Check if this function can be called with given argument types
    bool can_call_with(const std::vector<std::shared_ptr<MetaType>>& arg_types) const;
    
private:
    std::vector<std::shared_ptr<MetaType>> param_types_;
    std::shared_ptr<MetaType> return_type_;
    std::vector<std::string> named_returns_;
};

// RefinementMetaType - Types with logical constraints (type Name -> BaseType where { predicate })
class RefinementMetaType : public MetaType {
public:
    // Composition type for combining multiple refinements
    enum class CompositionType {
        AND,  // All predicates must be true
        OR    // At least one predicate must be true
    };

    RefinementMetaType(std::string name, 
                       std::shared_ptr<MetaType> base_type,
                       kernel::Value predicate)
        : name_(std::move(name))
        , base_type_(std::move(base_type))
        , predicate_(std::move(predicate)) {}
    
    // Constructor for inheritance (extends another refinement type)
    RefinementMetaType(std::string name,
                       std::shared_ptr<RefinementMetaType> parent_refinement,
                       kernel::Value additional_predicate)
        : name_(std::move(name))
        , base_type_(parent_refinement->base_type_)
        , predicate_(combine_predicates_and(parent_refinement->predicate_, additional_predicate))
        , parent_refinement_(std::move(parent_refinement)) {}
    
    // Constructor for composition (combines multiple refinement types)
    RefinementMetaType(std::string name,
                       std::shared_ptr<MetaType> base_type,
                       const std::vector<std::shared_ptr<RefinementMetaType>>& composed_refinements,
                       CompositionType composition_type = CompositionType::AND)
        : name_(std::move(name))
        , base_type_(std::move(base_type))
        , predicate_(combine_multiple_predicates(composed_refinements, composition_type))
        , composed_refinements_(composed_refinements)
        , composition_type_(composition_type) {}
    
    std::string name() const override { return name_; }
    size_t size() const override { return base_type_->size(); }
    bool is_value_type() const override { return base_type_->is_value_type(); }
    
    const std::shared_ptr<MetaType>& base_type() const { return base_type_; }
    const kernel::Value& predicate() const { return predicate_; }
    
    // Composition and inheritance support
    const std::shared_ptr<RefinementMetaType>& parent_refinement() const { return parent_refinement_; }
    const std::vector<std::shared_ptr<RefinementMetaType>>& composed_refinements() const { return composed_refinements_; }
    bool is_inherited() const { return parent_refinement_ != nullptr; }
    bool is_composed() const { return !composed_refinements_.empty(); }
    
    // Check if a value satisfies the refinement predicate
    std::expected<bool, std::string> validate_value(const kernel::Value& value) const;
    
    // Create a validated instance of this refinement type
    std::expected<kernel::Value, std::string> from_value(const kernel::Value& value) const;
    
    // Static factory method for creating validated instances
    static std::expected<kernel::Value, std::string> create_validated(
        std::shared_ptr<RefinementMetaType> refinement_type,
        const kernel::Value& value
    );
    
    // Static factory methods for composition and inheritance
    static std::shared_ptr<RefinementMetaType> create_inherited(
        const std::string& name,
        std::shared_ptr<RefinementMetaType> parent,
        kernel::Value additional_predicate
    );
    
    static std::shared_ptr<RefinementMetaType> create_composed_and(
        const std::string& name,
        std::shared_ptr<MetaType> base_type,
        const std::vector<std::shared_ptr<RefinementMetaType>>& refinements
    );
    
    static std::shared_ptr<RefinementMetaType> create_composed_or(
        const std::string& name,
        std::shared_ptr<MetaType> base_type,
        const std::vector<std::shared_ptr<RefinementMetaType>>& refinements
    );
    
    // Type compatibility - refinement types are subtypes of their base types
    bool is_assignable_from(const MetaType& other) const override;
    bool is_subtype_of(const MetaType& other) const override;
    
private:
    std::string name_;
    std::shared_ptr<MetaType> base_type_;
    kernel::Value predicate_;  // Function that takes a value and returns bool
    
    // Inheritance support
    std::shared_ptr<RefinementMetaType> parent_refinement_;
    
    // Composition support
    std::vector<std::shared_ptr<RefinementMetaType>> composed_refinements_;
    CompositionType composition_type_ = CompositionType::AND;
    
    // Helper methods for predicate combination
    static kernel::Value combine_predicates_and(const kernel::Value& pred1, const kernel::Value& pred2);
    static kernel::Value combine_predicates_or(const kernel::Value& pred1, const kernel::Value& pred2);
    static kernel::Value combine_multiple_predicates(
        const std::vector<std::shared_ptr<RefinementMetaType>>& refinements,
        CompositionType composition_type
    );
};

// Type registry - singleton for managing all types
class TypeRegistry {
public:
    static TypeRegistry& instance() {
        static TypeRegistry registry;
        return registry;
    }
    
    // Register types
    void register_type(const std::string& name, std::shared_ptr<MetaType> type);
    std::expected<std::shared_ptr<MetaType>, std::string> get_type(const std::string& name) const;
    
    // Type aliases
    void register_type_alias(const std::string& alias_name, const std::string& target_type_name);
    void register_type_alias(const std::string& alias_name, std::shared_ptr<MetaType> target_type);
    std::expected<std::shared_ptr<MetaType>, std::string> resolve_alias(const std::string& name) const;
    bool is_alias(const std::string& name) const;
    
    // Built-in types
    std::shared_ptr<MetaType> get_int_type() const;
    std::shared_ptr<MetaType> get_bool_type() const;
    std::shared_ptr<MetaType> get_string_type() const;
    std::shared_ptr<MetaType> get_unit_type() const;
    std::shared_ptr<MetaType> get_null_type() const;
    std::shared_ptr<MetaType> get_float_type() const;
    
    // Type operations
    std::shared_ptr<MetaType> create_optional_type(std::shared_ptr<MetaType> inner);
    std::shared_ptr<MetaType> create_array_type(std::shared_ptr<MetaType> element);
    std::shared_ptr<MetaType> create_function_type(
        std::vector<std::shared_ptr<MetaType>> params,
        std::shared_ptr<MetaType> return_type
    );
    
    // Query operations
    bool is_registered(const std::string& name) const;
    std::vector<std::string> get_all_type_names() const;
    
    // Nullability helpers
    bool is_nullable_type(const MetaType& type) const;
    std::expected<std::shared_ptr<MetaType>, std::string> get_inner_type(const MetaType& nullable_type) const;
    
    // Built-in traits
    std::shared_ptr<TraitMetaType> get_copyable_trait() const;
    
private:
    TypeRegistry();
    
    std::map<std::string, std::shared_ptr<MetaType>> types_;
    std::map<std::string, std::shared_ptr<MetaType>> type_aliases_;
    std::shared_ptr<MetaType> null_type_;
    std::shared_ptr<TraitMetaType> copyable_trait_;
    mutable std::mutex mutex_;
};

} // namespace meld::meta
