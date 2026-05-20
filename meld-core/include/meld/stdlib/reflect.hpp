#pragma once

#include "meld/meta/reflection_helper.hpp"
#include "meld/meta/metatype.hpp"
#include "meld/kernel/primitives.hpp"
#include <memory>
#include <string>
#include <vector>

namespace meld::stdlib {

/**
 * Reflect - High-level reflection API for Meld programs
 * 
 * This namespace provides the public reflection API that Meld programs
 * can use to introspect types at runtime.
 */
namespace reflect {

/**
 * Get type information for a C++ type
 * 
 * @tparam T The type to reflect on
 * @return RTTR type object
 */
template<typename T>
rttr::type typeOf() {
    return meta::ReflectionHelper::type_of<T>();
}

/**
 * Get type information by name
 * 
 * @param name Type name
 * @return RTTR type object
 */
rttr::type typeOf(const std::string& name);

/**
 * Get type information from a Meld Value
 * 
 * @param value Meld value
 * @return RTTR type object
 */
rttr::type typeOf(const kernel::Value& value);

/**
 * Get type information from a MetaType
 * 
 * @param metatype MetaType instance
 * @return RTTR type object
 */
rttr::type typeOf(const std::shared_ptr<meta::MetaType>& metatype);

/**
 * Get all properties of a type
 * 
 * @param type_name Type name
 * @return Vector of property names
 */
std::vector<std::string> getProperties(const std::string& type_name);

/**
 * Get all methods of a type
 * 
 * @param type_name Type name
 * @return Vector of method names
 */
std::vector<std::string> getMethods(const std::string& type_name);

/**
 * Get all constructors of a type
 * 
 * @param type_name Type name
 * @return Number of constructors
 */
size_t getConstructorCount(const std::string& type_name);

/**
 * Check if a type has a specific property
 * 
 * @param type_name Type name
 * @param property_name Property name
 * @return true if property exists
 */
bool hasProperty(const std::string& type_name, const std::string& property_name);

/**
 * Check if a type has a specific method
 * 
 * @param type_name Type name
 * @param method_name Method name
 * @return true if method exists
 */
bool hasMethod(const std::string& type_name, const std::string& method_name);

/**
 * Get the base classes of a type
 * 
 * @param type_name Type name
 * @return Vector of base class names
 */
std::vector<std::string> getBaseClasses(const std::string& type_name);

/**
 * Get the derived classes of a type
 * 
 * @param type_name Type name
 * @return Vector of derived class names
 */
std::vector<std::string> getDerivedClasses(const std::string& type_name);

/**
 * Check if a type is derived from another type
 * 
 * @param derived_name Potentially derived type name
 * @param base_name Potential base type name
 * @return true if derived is derived from base
 */
bool isDerivedFrom(const std::string& derived_name, const std::string& base_name);

/**
 * Get a human-readable description of a type
 * 
 * @param type_name Type name
 * @return Type description string
 */
std::string describeType(const std::string& type_name);

/**
 * Get a human-readable description of a property
 * 
 * @param type_name Type name
 * @param property_name Property name
 * @return Property description string
 */
std::string describeProperty(const std::string& type_name, const std::string& property_name);

/**
 * Get a human-readable description of a method
 * 
 * @param type_name Type name
 * @param method_name Method name
 * @return Method description string
 */
std::string describeMethod(const std::string& type_name, const std::string& method_name);

/**
 * Get all registered type names
 * 
 * @return Vector of all type names
 */
std::vector<std::string> getAllTypes();

/**
 * Check if a type is registered
 * 
 * @param type_name Type name
 * @return true if type is registered
 */
bool isTypeRegistered(const std::string& type_name);

/**
 * Get property value from an instance
 * 
 * @param instance RTTR instance wrapper
 * @param property_name Property name
 * @return Property value as variant
 */
rttr::variant getPropertyValue(rttr::instance instance, const std::string& property_name);

/**
 * Set property value on an instance
 * 
 * @param instance RTTR instance wrapper
 * @param property_name Property name
 * @param value New value
 * @return true if successful
 */
bool setPropertyValue(rttr::instance instance, const std::string& property_name, const rttr::variant& value);

/**
 * Invoke a method on an instance
 * 
 * @param instance RTTR instance wrapper
 * @param method_name Method name
 * @param args Method arguments
 * @return Method return value as variant
 */
rttr::variant invokeMethod(rttr::instance instance, const std::string& method_name, 
                          const std::vector<rttr::variant>& args = {});

/**
 * Create an instance of a type using its default constructor
 * 
 * @param type_name Type name
 * @return New instance as variant
 */
rttr::variant createInstance(const std::string& type_name);

/**
 * Create an instance of a type using a specific constructor
 * 
 * @param type_name Type name
 * @param args Constructor arguments
 * @return New instance as variant
 */
rttr::variant createInstance(const std::string& type_name, const std::vector<rttr::variant>& args);

} // namespace reflect

} // namespace meld::stdlib
