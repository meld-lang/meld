#pragma once

#include "meld/kernel/primitives.hpp"
#include "meld/meta/reflection_helper.hpp"
#include <string>
#include <optional>
#include <expected>

namespace meld::types {

/**
 * PropertyAccessError - Errors that can occur during property access
 */
enum class PropertyAccessError {
    TypeNotFound,
    PropertyNotFound,
    PropertyReadOnly,
    TypeMismatch,
    ConversionFailed,
    InvalidInstance
};

/**
 * Convert PropertyAccessError to string
 */
std::string to_string(PropertyAccessError error);

/**
 * DynamicPropertyAccess - Enables runtime property get/set operations
 * 
 * This class provides dynamic property access for Meld objects using RTTR.
 * It handles conversion between Meld Values and RTTR variants.
 */
class DynamicPropertyAccess {
public:
    /**
     * Get a property value from an instance
     * 
     * @param instance RTTR instance wrapper
     * @param property_name Property name
     * @return Property value or error
     */
    static std::expected<rttr::variant, PropertyAccessError> 
    get_property(const rttr::instance& instance, const std::string& property_name);
    
    /**
     * Set a property value on an instance
     * 
     * @param instance RTTR instance wrapper
     * @param property_name Property name
     * @param value New value
     * @return Success or error
     */
    static std::expected<void, PropertyAccessError> 
    set_property(rttr::instance& instance, const std::string& property_name, const rttr::variant& value);
    
    /**
     * Get a property value from an instance by type name
     * 
     * @param instance RTTR instance wrapper
     * @param type_name Type name
     * @param property_name Property name
     * @return Property value or error
     */
    static std::expected<rttr::variant, PropertyAccessError> 
    get_property(const rttr::instance& instance, const std::string& type_name, const std::string& property_name);
    
    /**
     * Set a property value on an instance by type name
     * 
     * @param instance RTTR instance wrapper
     * @param type_name Type name
     * @param property_name Property name
     * @param value New value
     * @return Success or error
     */
    static std::expected<void, PropertyAccessError> 
    set_property(rttr::instance& instance, const std::string& type_name, 
                const std::string& property_name, const rttr::variant& value);
    
    /**
     * Check if a property exists on an instance
     * 
     * @param instance RTTR instance wrapper
     * @param property_name Property name
     * @return true if property exists
     */
    static bool has_property(const rttr::instance& instance, const std::string& property_name);
    
    /**
     * Check if a property is read-only
     * 
     * @param instance RTTR instance wrapper
     * @param property_name Property name
     * @return true if property is read-only
     */
    static bool is_readonly(const rttr::instance& instance, const std::string& property_name);
    
    /**
     * Get the type of a property
     * 
     * @param instance RTTR instance wrapper
     * @param property_name Property name
     * @return Property type or nullopt if not found
     */
    static std::optional<rttr::type> get_property_type(const rttr::instance& instance, 
                                                        const std::string& property_name);
    
    /**
     * Convert Meld Value to RTTR variant
     * 
     * @param value Meld value
     * @return RTTR variant
     */
    static rttr::variant value_to_variant(const kernel::Value& value);
    
    /**
     * Convert RTTR variant to Meld Value
     * 
     * @param variant RTTR variant
     * @return Meld value or error
     */
    static std::expected<kernel::Value, PropertyAccessError> variant_to_value(const rttr::variant& variant);
    
    /**
     * Get all properties of an instance
     * 
     * @param instance RTTR instance wrapper
     * @return Vector of property names
     */
    static std::vector<std::string> get_all_properties(const rttr::instance& instance);
    
    /**
     * Get all property values of an instance
     * 
     * @param instance RTTR instance wrapper
     * @return Map of property names to values
     */
    static std::unordered_map<std::string, rttr::variant> 
    get_all_property_values(const rttr::instance& instance);
    
    /**
     * Set multiple properties on an instance
     * 
     * @param instance RTTR instance wrapper
     * @param properties Map of property names to values
     * @return Success or first error encountered
     */
    static std::expected<void, PropertyAccessError> 
    set_properties(rttr::instance& instance, const std::unordered_map<std::string, rttr::variant>& properties);
};

} // namespace meld::types
