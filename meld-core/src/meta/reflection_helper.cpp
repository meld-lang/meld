#include "meld/meta/reflection_helper.hpp"
#include <sstream>
#include <algorithm>

namespace meld::meta {

#ifdef __APPLE__
// Stub implementations for macOS where RTTR is not available

rttr::type ReflectionHelper::type_by_name(const std::string&) {
    return {};
}

std::vector<rttr::property> ReflectionHelper::get_properties(const rttr::type&) {
    return {};
}

std::optional<rttr::property> ReflectionHelper::get_property(const rttr::type&, const std::string&) {
    return std::nullopt;
}

std::vector<rttr::method> ReflectionHelper::get_methods(const rttr::type&) {
    return {};
}

std::optional<rttr::method> ReflectionHelper::get_method(const rttr::type&, const std::string&) {
    return std::nullopt;
}

std::vector<rttr::constructor> ReflectionHelper::get_constructors(const rttr::type&) {
    return {};
}

bool ReflectionHelper::has_property(const rttr::type&, const std::string&) {
    return false;
}

bool ReflectionHelper::has_method(const rttr::type&, const std::string&) {
    return false;
}

std::vector<rttr::type> ReflectionHelper::get_base_classes(const rttr::type&) {
    return {};
}

std::vector<rttr::type> ReflectionHelper::get_derived_classes(const rttr::type&) {
    return {};
}

bool ReflectionHelper::is_derived_from(const rttr::type&, const rttr::type&) {
    return false;
}

rttr::variant ReflectionHelper::get_property_value(const rttr::instance&, const std::string&) {
    return {};
}

bool ReflectionHelper::set_property_value(rttr::instance&, const std::string&, const rttr::variant&) {
    return false;
}

rttr::variant ReflectionHelper::invoke_method(rttr::instance&, const std::string&, 
                                              const std::vector<rttr::variant>&) {
    return {};
}

rttr::variant ReflectionHelper::create_instance(const rttr::type&) {
    return {};
}

rttr::variant ReflectionHelper::create_instance(const rttr::type&, const std::vector<rttr::variant>&) {
    return {};
}

std::optional<rttr::variant> ReflectionHelper::get_metadata(const rttr::type&, const std::string&) {
    return std::nullopt;
}

std::optional<rttr::variant> ReflectionHelper::get_metadata(const rttr::property&, const std::string&) {
    return std::nullopt;
}

std::optional<rttr::variant> ReflectionHelper::get_metadata(const rttr::method&, const std::string&) {
    return std::nullopt;
}

std::string ReflectionHelper::describe_type(const rttr::type&) {
    return "[RTTR not available on macOS]";
}

std::string ReflectionHelper::describe_property(const rttr::property&) {
    return "[RTTR not available on macOS]";
}

std::string ReflectionHelper::describe_method(const rttr::method&) {
    return "[RTTR not available on macOS]";
}

#else // !__APPLE__

rttr::type ReflectionHelper::type_by_name(const std::string& name) {
    return rttr::type::get_by_name(name);
}

std::vector<rttr::property> ReflectionHelper::get_properties(const rttr::type& type) {
    std::vector<rttr::property> result;
    auto props = type.get_properties();
    result.assign(props.begin(), props.end());
    return result;
}

std::optional<rttr::property> ReflectionHelper::get_property(const rttr::type& type, const std::string& name) {
    rttr::property prop = type.get_property(name);
    if (prop.is_valid()) {
        return prop;
    }
    return std::nullopt;
}

std::vector<rttr::method> ReflectionHelper::get_methods(const rttr::type& type) {
    std::vector<rttr::method> result;
    auto methods = type.get_methods();
    result.assign(methods.begin(), methods.end());
    return result;
}

std::optional<rttr::method> ReflectionHelper::get_method(const rttr::type& type, const std::string& name) {
    rttr::method method = type.get_method(name);
    if (method.is_valid()) {
        return method;
    }
    return std::nullopt;
}

std::vector<rttr::constructor> ReflectionHelper::get_constructors(const rttr::type& type) {
    std::vector<rttr::constructor> result;
    auto ctors = type.get_constructors();
    result.assign(ctors.begin(), ctors.end());
    return result;
}

bool ReflectionHelper::has_property(const rttr::type& type, const std::string& name) {
    return type.get_property(name).is_valid();
}

bool ReflectionHelper::has_method(const rttr::type& type, const std::string& name) {
    return type.get_method(name).is_valid();
}

std::vector<rttr::type> ReflectionHelper::get_base_classes(const rttr::type& type) {
    std::vector<rttr::type> result;
    auto bases = type.get_base_classes();
    result.assign(bases.begin(), bases.end());
    return result;
}

std::vector<rttr::type> ReflectionHelper::get_derived_classes(const rttr::type& type) {
    std::vector<rttr::type> result;
    auto derived = type.get_derived_classes();
    result.assign(derived.begin(), derived.end());
    return result;
}

bool ReflectionHelper::is_derived_from(const rttr::type& derived, const rttr::type& base) {
    return derived.is_derived_from(base);
}

rttr::variant ReflectionHelper::get_property_value(const rttr::instance& instance, const std::string& property_name) {
    rttr::type type = instance.get_type();
    rttr::property prop = type.get_property(property_name);
    if (!prop.is_valid()) {
        return rttr::variant();
    }
    return prop.get_value(instance);
}

bool ReflectionHelper::set_property_value(rttr::instance& instance, const std::string& property_name, const rttr::variant& value) {
    rttr::type type = instance.get_type();
    rttr::property prop = type.get_property(property_name);
    if (!prop.is_valid()) {
        return false;
    }
    if (prop.is_readonly()) {
        return false;
    }
    return prop.set_value(instance, value);
}

rttr::variant ReflectionHelper::invoke_method(rttr::instance& instance, const std::string& method_name, 
                                              const std::vector<rttr::variant>& args) {
    rttr::type type = instance.get_type();
    rttr::method method = type.get_method(method_name);
    if (!method.is_valid()) {
        return rttr::variant();
    }
    return method.invoke(instance, args);
}

rttr::variant ReflectionHelper::create_instance(const rttr::type& type) {
    for (auto& ctor : type.get_constructors()) {
        if (ctor.get_parameter_infos().empty()) {
            return ctor.invoke();
        }
    }
    return rttr::variant();
}

rttr::variant ReflectionHelper::create_instance(const rttr::type& type, const std::vector<rttr::variant>& args) {
    for (auto& ctor : type.get_constructors()) {
        auto params = ctor.get_parameter_infos();
        if (params.size() != args.size()) {
            continue;
        }
        bool matches = true;
        auto params_vec = std::vector<rttr::parameter_info>(params.begin(), params.end());
        for (size_t i = 0; i < params_vec.size(); ++i) {
            if (!args[i].can_convert(params_vec[i].get_type())) {
                matches = false;
                break;
            }
        }
        if (matches) {
            return ctor.invoke(args);
        }
    }
    return rttr::variant();
}

std::optional<rttr::variant> ReflectionHelper::get_metadata(const rttr::type& type, const std::string& key) {
    rttr::variant metadata = type.get_metadata(key);
    if (metadata.is_valid()) {
        return metadata;
    }
    return std::nullopt;
}

std::optional<rttr::variant> ReflectionHelper::get_metadata(const rttr::property& property, const std::string& key) {
    rttr::variant metadata = property.get_metadata(key);
    if (metadata.is_valid()) {
        return metadata;
    }
    return std::nullopt;
}

std::optional<rttr::variant> ReflectionHelper::get_metadata(const rttr::method& method, const std::string& key) {
    rttr::variant metadata = method.get_metadata(key);
    if (metadata.is_valid()) {
        return metadata;
    }
    return std::nullopt;
}

std::string ReflectionHelper::describe_type(const rttr::type& type) {
    std::ostringstream oss;
    oss << "Type: " << type.get_name().to_string() << "\n";
    oss << "  Is class: " << (type.is_class() ? "yes" : "no") << "\n";
    oss << "  Is pointer: " << (type.is_pointer() ? "yes" : "no") << "\n";
    oss << "  Is array: " << (type.is_array() ? "yes" : "no") << "\n";
    auto bases = get_base_classes(type);
    if (!bases.empty()) {
        oss << "  Base classes: ";
        for (size_t i = 0; i < bases.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << bases[i].get_name().to_string();
        }
        oss << "\n";
    }
    auto props = get_properties(type);
    oss << "  Properties: " << props.size() << "\n";
    for (const auto& prop : props) {
        oss << "    - " << prop.get_name().to_string() 
            << " (" << prop.get_type().get_name().to_string() << ")"
            << (prop.is_readonly() ? " [readonly]" : "")
            << "\n";
    }
    auto methods = get_methods(type);
    oss << "  Methods: " << methods.size() << "\n";
    for (const auto& method : methods) {
        oss << "    - " << method.get_name().to_string() << "(";
        auto params = method.get_parameter_infos();
        auto params_vec = std::vector<rttr::parameter_info>(params.begin(), params.end());
        for (size_t i = 0; i < params_vec.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << params_vec[i].get_type().get_name().to_string();
        }
        oss << ") -> " << method.get_return_type().get_name().to_string() << "\n";
    }
    return oss.str();
}

std::string ReflectionHelper::describe_property(const rttr::property& property) {
    std::ostringstream oss;
    oss << "Property: " << property.get_name().to_string() << "\n";
    oss << "  Type: " << property.get_type().get_name().to_string() << "\n";
    oss << "  Readonly: " << (property.is_readonly() ? "yes" : "no") << "\n";
    oss << "  Declaring type: " << property.get_declaring_type().get_name().to_string() << "\n";
    return oss.str();
}

std::string ReflectionHelper::describe_method(const rttr::method& method) {
    std::ostringstream oss;
    oss << "Method: " << method.get_name().to_string() << "\n";
    oss << "  Return type: " << method.get_return_type().get_name().to_string() << "\n";
    oss << "  Parameters: ";
    auto params = method.get_parameter_infos();
    if (params.empty()) {
        oss << "none";
    } else {
        oss << "\n";
        auto params_vec = std::vector<rttr::parameter_info>(params.begin(), params.end());
        for (size_t i = 0; i < params_vec.size(); ++i) {
            oss << "    " << i << ": " << params_vec[i].get_type().get_name().to_string() << "\n";
        }
    }
    oss << "  Declaring type: " << method.get_declaring_type().get_name().to_string() << "\n";
    return oss.str();
}

#endif // !__APPLE__

} // namespace meld::meta
