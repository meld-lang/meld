#pragma once

#include "meld/meta/metatype.hpp"
#include "meld/kernel/primitives.hpp"
#include "meld/types/instance.hpp"
#include <functional>
#include <expected>

namespace meld::types {

// Access context - information about who is accessing the property
struct AccessContext {
    meta::AccessModifier caller_access;  // Access level of the caller
    bool is_same_class;  // Is the caller in the same class?
    bool is_subclass;    // Is the caller in a subclass?
    bool is_same_module; // Is the caller in the same module?
};

// Forward declarations
class StructInstance;
class ClassInstance;

// Property accessor context - holds the object instance and property metadata
struct PropertyContext {
    std::shared_ptr<TypeInstance> instance;  // The object instance (struct or class)
    const meta::Property* property;  // Property metadata
    AccessContext access_context;  // Who is accessing the property
};

// Property accessor - handles getting and setting property values
class PropertyAccessor {
public:
    // Get property value
    static std::expected<kernel::Value, std::string> 
    get_value(const PropertyContext& context);
    
    // Set property value
    static std::expected<void, std::string> 
    set_value(const PropertyContext& context, const kernel::Value& value);
    
    // Execute custom getter
    static std::expected<kernel::Value, std::string>
    execute_custom_getter(const PropertyContext& context);
    
    // Execute custom setter
    static std::expected<void, std::string>
    execute_custom_setter(const PropertyContext& context, const kernel::Value& value);
    
    // Execute delegated getter
    static std::expected<kernel::Value, std::string>
    execute_delegated_getter(const PropertyContext& context);
    
    // Execute delegated setter
    static std::expected<void, std::string>
    execute_delegated_setter(const PropertyContext& context, const kernel::Value& value);
    
    // Check access permissions
    static bool can_access_getter(const PropertyContext& context);
    static bool can_access_setter(const PropertyContext& context);
    
private:
    // Get backing field value
    static std::expected<kernel::Value, std::string>
    get_backing_field(const PropertyContext& context);
    
    // Set backing field value
    static std::expected<void, std::string>
    set_backing_field(const PropertyContext& context, const kernel::Value& value);
    
    // Check if access is allowed based on modifier
    static bool is_access_allowed(meta::AccessModifier required_access, 
                                  const AccessContext& context);
};

// Property builder - fluent API for creating properties
class PropertyBuilder {
public:
    explicit PropertyBuilder(std::string name, std::shared_ptr<meta::MetaType> type)
        : property_(std::move(name), std::move(type), false) {}
    
    PropertyBuilder& mutable_property() {
        property_.is_mutable = true;
        return *this;
    }
    
    PropertyBuilder& immutable_property() {
        property_.is_mutable = false;
        return *this;
    }
    
    PropertyBuilder& with_backing_field(std::string field_name) {
        property_.has_backing_field = true;
        property_.backing_field_name = std::move(field_name);
        return *this;
    }
    
    PropertyBuilder& without_backing_field() {
        property_.has_backing_field = false;
        return *this;
    }
    
    PropertyBuilder& with_custom_getter(kernel::Value getter_impl) {
        property_.has_custom_getter = true;
        property_.getter_impl = std::move(getter_impl);
        return *this;
    }
    
    PropertyBuilder& with_custom_setter(kernel::Value setter_impl) {
        property_.has_custom_setter = true;
        property_.setter_impl = std::move(setter_impl);
        return *this;
    }
    
    PropertyBuilder& getter_access(meta::AccessModifier access) {
        property_.getter_access = access;
        return *this;
    }
    
    PropertyBuilder& setter_access(meta::AccessModifier access) {
        property_.setter_access = access;
        return *this;
    }
    
    PropertyBuilder& delegated_to(std::string delegate_name, kernel::Value delegate_impl) {
        property_.is_delegated = true;
        property_.delegate_name = std::move(delegate_name);
        property_.delegate_impl = std::move(delegate_impl);
        // Delegated properties typically don't have backing fields
        property_.has_backing_field = false;
        return *this;
    }
    
    meta::Property build() {
        return std::move(property_);
    }
    
private:
    meta::Property property_;
};

} // namespace meld::types
