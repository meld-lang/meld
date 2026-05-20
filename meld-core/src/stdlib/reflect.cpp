#include "meld/stdlib/reflect.hpp"
#include "meld/meta/type_registration.hpp"
#include <algorithm>

namespace meld::stdlib::reflect {

rttr::type typeOf(const std::string& name) {
    return meta::ReflectionHelper::type_by_name(name);
}

rttr::type typeOf(const kernel::Value& value) {
    // On macOS, RTTR is stubbed out, so just return an invalid type
    // On other platforms, determine the type from the Value variant
    return rttr::type();
}

rttr::type typeOf(const std::shared_ptr<meta::MetaType>& metatype) {
    if (!metatype) {
        return rttr::type();
    }
    
    // Get the type by the MetaType's name
    return meta::ReflectionHelper::type_by_name(metatype->name());
}

std::vector<std::string> getProperties(const std::string& type_name) {
    rttr::type type = typeOf(type_name);
    if (!type.is_valid()) {
        return {};
    }
    
    auto props = meta::ReflectionHelper::get_properties(type);
    std::vector<std::string> result;
    result.reserve(props.size());
    
    for (const auto& prop : props) {
        result.push_back(prop.get_name());
    }
    
    return result;
}

std::vector<std::string> getMethods(const std::string& type_name) {
    rttr::type type = typeOf(type_name);
    if (!type.is_valid()) {
        return {};
    }
    
    auto methods = meta::ReflectionHelper::get_methods(type);
    std::vector<std::string> result;
    result.reserve(methods.size());
    
    for (const auto& method : methods) {
        result.push_back(method.get_name());
    }
    
    return result;
}

size_t getConstructorCount(const std::string& type_name) {
    rttr::type type = typeOf(type_name);
    if (!type.is_valid()) {
        return 0;
    }
    
    return meta::ReflectionHelper::get_constructors(type).size();
}

bool hasProperty(const std::string& type_name, const std::string& property_name) {
    rttr::type type = typeOf(type_name);
    if (!type.is_valid()) {
        return false;
    }
    
    return meta::ReflectionHelper::has_property(type, property_name);
}

bool hasMethod(const std::string& type_name, const std::string& method_name) {
    rttr::type type = typeOf(type_name);
    if (!type.is_valid()) {
        return false;
    }
    
    return meta::ReflectionHelper::has_method(type, method_name);
}

std::vector<std::string> getBaseClasses(const std::string& type_name) {
    rttr::type type = typeOf(type_name);
    if (!type.is_valid()) {
        return {};
    }
    
    auto bases = meta::ReflectionHelper::get_base_classes(type);
    std::vector<std::string> result;
    result.reserve(bases.size());
    
    for (const auto& base : bases) {
        result.push_back(base.get_name());
    }
    
    return result;
}

std::vector<std::string> getDerivedClasses(const std::string& type_name) {
    rttr::type type = typeOf(type_name);
    if (!type.is_valid()) {
        return {};
    }
    
    auto derived = meta::ReflectionHelper::get_derived_classes(type);
    std::vector<std::string> result;
    result.reserve(derived.size());
    
    for (const auto& d : derived) {
        result.push_back(d.get_name());
    }
    
    return result;
}

bool isDerivedFrom(const std::string& derived_name, const std::string& base_name) {
    rttr::type derived = typeOf(derived_name);
    rttr::type base = typeOf(base_name);
    
    if (!derived.is_valid() || !base.is_valid()) {
        return false;
    }
    
    return meta::ReflectionHelper::is_derived_from(derived, base);
}

std::string describeType(const std::string& type_name) {
    rttr::type type = typeOf(type_name);
    if (!type.is_valid()) {
        return "Type not found: " + type_name;
    }
    
    return meta::ReflectionHelper::describe_type(type);
}

std::string describeProperty(const std::string& type_name, const std::string& property_name) {
    rttr::type type = typeOf(type_name);
    if (!type.is_valid()) {
        return "Type not found: " + type_name;
    }
    
    auto prop = meta::ReflectionHelper::get_property(type, property_name);
    if (!prop) {
        return "Property not found: " + property_name;
    }
    
    return meta::ReflectionHelper::describe_property(*prop);
}

std::string describeMethod(const std::string& type_name, const std::string& method_name) {
    rttr::type type = typeOf(type_name);
    if (!type.is_valid()) {
        return "Type not found: " + type_name;
    }
    
    auto method = meta::ReflectionHelper::get_method(type, method_name);
    if (!method) {
        return "Method not found: " + method_name;
    }
    
    return meta::ReflectionHelper::describe_method(*method);
}

std::vector<std::string> getAllTypes() {
    return meta::TypeRegistry::instance().get_all_type_names();
}

bool isTypeRegistered(const std::string& type_name) {
    return meta::TypeRegistry::instance().is_registered(type_name);
}

rttr::variant getPropertyValue(rttr::instance instance, const std::string& property_name) {
    return meta::ReflectionHelper::get_property_value(instance, property_name);
}

bool setPropertyValue(rttr::instance instance, const std::string& property_name, const rttr::variant& value) {
    return meta::ReflectionHelper::set_property_value(instance, property_name, value);
}

rttr::variant invokeMethod(rttr::instance instance, const std::string& method_name, 
                          const std::vector<rttr::variant>& args) {
    return meta::ReflectionHelper::invoke_method(instance, method_name, args);
}

rttr::variant createInstance(const std::string& type_name) {
    rttr::type type = typeOf(type_name);
    if (!type.is_valid()) {
        return rttr::variant();
    }
    
    return meta::ReflectionHelper::create_instance(type);
}

rttr::variant createInstance(const std::string& type_name, const std::vector<rttr::variant>& args) {
    rttr::type type = typeOf(type_name);
    if (!type.is_valid()) {
        return rttr::variant();
    }
    
    return meta::ReflectionHelper::create_instance(type, args);
}

} // namespace meld::stdlib::reflect
