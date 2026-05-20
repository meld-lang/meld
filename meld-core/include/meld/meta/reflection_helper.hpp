#pragma once

#ifdef __APPLE__
// RTTR has circular header dependencies that break on Apple Clang's libc++.
// Provide a minimal stub interface on macOS.
#include <string>
#include <vector>
#include <optional>
#include <memory>

namespace rttr {
    class type;
    class parameter_info {
    public:
        parameter_info() = default;
        std::string get_name() const { return ""; }
        type get_type() const;
    };
    class variant {
    public:
        variant() = default;
        template<typename T> variant(const T&) {}
        bool is_valid() const { return false; }
        template<typename T> bool is_type() const { return false; }
        template<typename T> T get_value() const { return T{}; }
        bool convert(variant&) const { return false; }
        bool can_convert(const type&) const { return false; }
        type get_type() const;
        variant create_sequential_view() const { return {}; }
        variant get_wrapped_instance() const { return {}; }
    };
    class method {
    public:
        method() = default;
        bool is_valid() const { return false; }
        std::string get_name() const { return ""; }
        variant invoke(variant) const { return {}; }
        variant invoke(variant, variant) const { return {}; }
        variant invoke(variant, variant, variant) const { return {}; }
        template<typename... Args>
        variant invoke(variant, Args...) const { return {}; }
        std::vector<parameter_info> get_parameter_infos() const { return {}; }
        type get_return_type() const;
        type get_declaring_type() const;
        variant get_metadata(const std::string&) const { return {}; }
    };
    class property {
    public:
        property() = default;
        bool is_valid() const { return false; }
        std::string get_name() const { return ""; }
        variant get_value(variant) const { return {}; }
        bool set_value(variant, variant) const { return false; }
        type get_type() const;
        type get_declaring_type() const;
        bool is_readonly() const { return false; }
        variant get_metadata(const std::string&) const { return {}; }
    };
    class constructor {
    public:
        constructor() = default;
        bool is_valid() const { return false; }
        variant invoke() const { return {}; }
        template<typename... Args>
        variant invoke(Args...) const { return {}; }
    };
    class instance {
    public:
        instance() = default;
        template<typename T> instance(T&&) {}
        bool is_valid() const { return false; }
        type get_type() const;
    };
    class type {
    public:
        type() = default;
        static type get_by_name(const std::string&) { return {}; }
        bool is_valid() const { return false; }
        bool is_class() const { return false; }
        bool is_pointer() const { return false; }
        bool is_array() const { return false; }
        bool is_arithmetic() const { return false; }
        bool is_enumeration() const { return false; }
        bool is_sequential_container() const { return false; }
        bool is_associative_container() const { return false; }
        struct string_view_stub {
            std::string to_string() const { return ""; }
            operator std::string() const { return ""; }
        };
        string_view_stub get_name() const { return {}; }
        static std::vector<type> get_types() { return {}; }
        template<typename T> static type get() { return {}; }
        std::vector<property> get_properties() const { return {}; }
        property get_property(const std::string&) const { return {}; }
        std::vector<method> get_methods() const { return {}; }
        method get_method(const std::string&) const { return {}; }
        std::vector<constructor> get_constructors() const { return {}; }
        std::vector<type> get_base_classes() const { return {}; }
        std::vector<type> get_derived_classes() const { return {}; }
        bool is_derived_from(const type&) const { return false; }
        variant get_metadata(const std::string&) const { return {}; }
        
        bool operator==(const type&) const { return false; }
        bool operator!=(const type&) const { return true; }
    };
    inline type property::get_type() const { return {}; }
    inline type property::get_declaring_type() const { return {}; }
    inline type method::get_return_type() const { return {}; }
    inline type method::get_declaring_type() const { return {}; }
    inline type instance::get_type() const { return {}; }
    inline type variant::get_type() const { return {}; }
    inline type parameter_info::get_type() const { return {}; }
}
#else
#include <rttr/type>
#endif
#include <string>
#include <vector>
#include <optional>
#include <memory>

namespace meld::meta {

/**
 * ReflectionHelper - Provides utilities for runtime type introspection
 * 
 * This class wraps RTTR functionality to provide convenient reflection
 * operations for Meld types.
 */
class ReflectionHelper {
public:
    /**
     * Get type information for a C++ type
     * 
     * @tparam T The type to reflect on
     * @return RTTR type object
     */
    template<typename T>
    static rttr::type type_of() {
        return rttr::type::get<T>();
    }
    
    /**
     * Get type information by name
     * 
     * @param name Type name
     * @return RTTR type object, invalid if not found
     */
    static rttr::type type_by_name(const std::string& name);
    
    /**
     * Get all properties of a type
     * 
     * @param type RTTR type object
     * @return Vector of property objects
     */
    static std::vector<rttr::property> get_properties(const rttr::type& type);
    
    /**
     * Get a specific property by name
     * 
     * @param type RTTR type object
     * @param name Property name
     * @return Property object if found
     */
    static std::optional<rttr::property> get_property(const rttr::type& type, const std::string& name);
    
    /**
     * Get all methods of a type
     * 
     * @param type RTTR type object
     * @return Vector of method objects
     */
    static std::vector<rttr::method> get_methods(const rttr::type& type);
    
    /**
     * Get a specific method by name
     * 
     * @param type RTTR type object
     * @param name Method name
     * @return Method object if found
     */
    static std::optional<rttr::method> get_method(const rttr::type& type, const std::string& name);
    
    /**
     * Get all constructors of a type
     * 
     * @param type RTTR type object
     * @return Vector of constructor objects
     */
    static std::vector<rttr::constructor> get_constructors(const rttr::type& type);
    
    /**
     * Check if a type has a specific property
     * 
     * @param type RTTR type object
     * @param name Property name
     * @return true if property exists
     */
    static bool has_property(const rttr::type& type, const std::string& name);
    
    /**
     * Check if a type has a specific method
     * 
     * @param type RTTR type object
     * @param name Method name
     * @return true if method exists
     */
    static bool has_method(const rttr::type& type, const std::string& name);
    
    /**
     * Get the base classes of a type
     * 
     * @param type RTTR type object
     * @return Vector of base class types
     */
    static std::vector<rttr::type> get_base_classes(const rttr::type& type);
    
    /**
     * Get the derived classes of a type
     * 
     * @param type RTTR type object
     * @return Vector of derived class types
     */
    static std::vector<rttr::type> get_derived_classes(const rttr::type& type);
    
    /**
     * Check if a type is derived from another type
     * 
     * @param derived The potentially derived type
     * @param base The potential base type
     * @return true if derived is derived from base
     */
    static bool is_derived_from(const rttr::type& derived, const rttr::type& base);
    
    /**
     * Get property value from an instance
     * 
     * @param instance RTTR instance wrapper
     * @param property_name Property name
     * @return Property value as variant
     */
    static rttr::variant get_property_value(const rttr::instance& instance, const std::string& property_name);
    
    /**
     * Set property value on an instance
     * 
     * @param instance RTTR instance wrapper
     * @param property_name Property name
     * @param value New value
     * @return true if successful
     */
    static bool set_property_value(rttr::instance& instance, const std::string& property_name, const rttr::variant& value);
    
    /**
     * Invoke a method on an instance
     * 
     * @param instance RTTR instance wrapper
     * @param method_name Method name
     * @param args Method arguments
     * @return Method return value as variant
     */
    static rttr::variant invoke_method(rttr::instance& instance, const std::string& method_name, 
                                       const std::vector<rttr::variant>& args = {});
    
    /**
     * Create an instance of a type using its default constructor
     * 
     * @param type RTTR type object
     * @return New instance as variant
     */
    static rttr::variant create_instance(const rttr::type& type);
    
    /**
     * Create an instance of a type using a specific constructor
     * 
     * @param type RTTR type object
     * @param args Constructor arguments
     * @return New instance as variant
     */
    static rttr::variant create_instance(const rttr::type& type, const std::vector<rttr::variant>& args);
    
    /**
     * Get metadata associated with a type
     * 
     * @param type RTTR type object
     * @param key Metadata key
     * @return Metadata value if found
     */
    static std::optional<rttr::variant> get_metadata(const rttr::type& type, const std::string& key);
    
    /**
     * Get metadata associated with a property
     * 
     * @param property RTTR property object
     * @param key Metadata key
     * @return Metadata value if found
     */
    static std::optional<rttr::variant> get_metadata(const rttr::property& property, const std::string& key);
    
    /**
     * Get metadata associated with a method
     * 
     * @param method RTTR method object
     * @param key Metadata key
     * @return Metadata value if found
     */
    static std::optional<rttr::variant> get_metadata(const rttr::method& method, const std::string& key);
    
    /**
     * Get a human-readable description of a type
     * 
     * @param type RTTR type object
     * @return Type description string
     */
    static std::string describe_type(const rttr::type& type);
    
    /**
     * Get a human-readable description of a property
     * 
     * @param property RTTR property object
     * @return Property description string
     */
    static std::string describe_property(const rttr::property& property);
    
    /**
     * Get a human-readable description of a method
     * 
     * @param method RTTR method object
     * @return Method description string
     */
    static std::string describe_method(const rttr::method& method);
};

} // namespace meld::meta
