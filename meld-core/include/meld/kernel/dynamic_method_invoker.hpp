#pragma once

#include "meld/kernel/primitives.hpp"
#include "meld/meta/reflection_helper.hpp"
#include <string>
#include <vector>
#include <optional>
#include <expected>

namespace meld::kernel {

/**
 * MethodInvocationError - Errors that can occur during method invocation
 */
enum class MethodInvocationError {
    TypeNotFound,
    MethodNotFound,
    InvalidInstance,
    ArgumentCountMismatch,
    ArgumentTypeMismatch,
    ConversionFailed,
    InvocationFailed,
    AmbiguousOverload
};

/**
 * Convert MethodInvocationError to string
 */
std::string to_string(MethodInvocationError error);

/**
 * DynamicMethodInvoker - Enables runtime method invocation
 * 
 * This class provides dynamic method invocation for Meld objects using RTTR.
 * It handles argument conversion, overload resolution, and result conversion.
 */
class DynamicMethodInvoker {
public:
    /**
     * Invoke a method on an instance
     * 
     * @param instance RTTR instance wrapper
     * @param method_name Method name
     * @param args Method arguments
     * @return Method result or error
     */
    static std::expected<rttr::variant, MethodInvocationError>
    invoke(rttr::instance& instance, const std::string& method_name,
           const std::vector<rttr::variant>& args = {});
    
    /**
     * Invoke a method on an instance with type validation
     * 
     * @param instance RTTR instance wrapper
     * @param type_name Expected type name
     * @param method_name Method name
     * @param args Method arguments
     * @return Method result or error
     */
    static std::expected<rttr::variant, MethodInvocationError>
    invoke(rttr::instance& instance, const std::string& type_name,
           const std::string& method_name, const std::vector<rttr::variant>& args = {});
    
    /**
     * Check if a method exists on an instance
     * 
     * @param instance RTTR instance wrapper
     * @param method_name Method name
     * @return true if method exists
     */
    static bool has_method(const rttr::instance& instance, const std::string& method_name);
    
    /**
     * Check if a method with specific signature exists
     * 
     * @param instance RTTR instance wrapper
     * @param method_name Method name
     * @param arg_types Argument types
     * @return true if matching method exists
     */
    static bool has_method(const rttr::instance& instance, const std::string& method_name,
                          const std::vector<rttr::type>& arg_types);
    
    /**
     * Get the return type of a method
     * 
     * @param instance RTTR instance wrapper
     * @param method_name Method name
     * @return Return type or nullopt if not found
     */
    static std::optional<rttr::type> get_return_type(const rttr::instance& instance,
                                                      const std::string& method_name);
    
    /**
     * Get all overloads of a method
     * 
     * @param instance RTTR instance wrapper
     * @param method_name Method name
     * @return Vector of method objects
     */
    static std::vector<rttr::method> get_overloads(const rttr::instance& instance,
                                                    const std::string& method_name);
    
    /**
     * Get parameter types for a method
     * 
     * @param instance RTTR instance wrapper
     * @param method_name Method name
     * @return Vector of parameter types or nullopt if not found
     */
    static std::optional<std::vector<rttr::type>> 
    get_parameter_types(const rttr::instance& instance, const std::string& method_name);
    
    /**
     * Find best matching method overload
     * 
     * @param methods Available method overloads
     * @param args Arguments to match
     * @return Best matching method or nullopt
     */
    static std::optional<rttr::method> 
    find_best_overload(const std::vector<rttr::method>& methods,
                      const std::vector<rttr::variant>& args);
    
    /**
     * Convert arguments to match parameter types
     * 
     * @param args Original arguments
     * @param param_types Target parameter types
     * @return Converted arguments or error
     */
    static std::expected<std::vector<rttr::variant>, MethodInvocationError>
    convert_arguments(const std::vector<rttr::variant>& args,
                     const std::vector<rttr::type>& param_types);
    
    /**
     * Invoke a method with Meld Values as arguments
     * 
     * @param instance RTTR instance wrapper
     * @param method_name Method name
     * @param args Meld Value arguments
     * @return Method result as Value or error
     */
    static std::expected<Value, MethodInvocationError>
    invoke_with_values(rttr::instance& instance, const std::string& method_name,
                      const std::vector<Value>& args = {});
    
    /**
     * Get all methods of an instance
     * 
     * @param instance RTTR instance wrapper
     * @return Vector of method names
     */
    static std::vector<std::string> get_all_methods(const rttr::instance& instance);
    
    /**
     * Get method signature as string
     * 
     * @param method RTTR method object
     * @return Method signature string
     */
    static std::string get_method_signature(const rttr::method& method);
    
    /**
     * Check if arguments match parameter types
     * 
     * @param args Arguments
     * @param param_types Parameter types
     * @return true if arguments match
     */
    static bool arguments_match(const std::vector<rttr::variant>& args,
                               const std::vector<rttr::type>& param_types);
    
    /**
     * Calculate match score for overload resolution
     * 
     * @param args Arguments
     * @param param_types Parameter types
     * @return Match score (higher is better), -1 if no match
     */
    static int calculate_match_score(const std::vector<rttr::variant>& args,
                                     const std::vector<rttr::type>& param_types);
};

} // namespace meld::kernel
