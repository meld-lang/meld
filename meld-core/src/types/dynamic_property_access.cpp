#include "meld/types/dynamic_property_access.hpp"
#include "meld/meta/type_registration.hpp"
#include <sstream>

namespace meld::types {

std::string to_string(PropertyAccessError error) {
    switch (error) {
        case PropertyAccessError::TypeNotFound:
            return "Type not found";
        case PropertyAccessError::PropertyNotFound:
            return "Property not found";
        case PropertyAccessError::PropertyReadOnly:
            return "Property is read-only";
        case PropertyAccessError::TypeMismatch:
            return "Type mismatch";
        case PropertyAccessError::ConversionFailed:
            return "Conversion failed";
        case PropertyAccessError::InvalidInstance:
            return "Invalid instance";
        default:
            return "Unknown error";
    }
}

std::expected<rttr::variant, PropertyAccessError> 
DynamicPropertyAccess::get_property(const rttr::instance& instance, const std::string& property_name) {
#ifdef __APPLE__
    // RTTR stubs on macOS don't support full property access via rttr::instance
    return std::unexpected(PropertyAccessError::ConversionFailed);
#else
    if (!instance.is_valid()) {
        return std::unexpected(PropertyAccessError::InvalidInstance);
    }
    
    rttr::type type = instance.get_type();
    rttr::property prop = type.get_property(property_name);
    
    if (!prop.is_valid()) {
        return std::unexpected(PropertyAccessError::PropertyNotFound);
    }
    
    rttr::variant value = prop.get_value(instance);
    if (!value.is_valid()) {
        return std::unexpected(PropertyAccessError::ConversionFailed);
    }
    
    return value;
#endif
}

std::expected<void, PropertyAccessError> 
DynamicPropertyAccess::set_property(rttr::instance& instance, const std::string& property_name, 
                                    const rttr::variant& value) {
#ifdef __APPLE__
    return std::unexpected(PropertyAccessError::ConversionFailed);
#else
    if (!instance.is_valid()) {
        return std::unexpected(PropertyAccessError::InvalidInstance);
    }
    
    rttr::type type = instance.get_type();
    rttr::property prop = type.get_property(property_name);
    
    if (!prop.is_valid()) {
        return std::unexpected(PropertyAccessError::PropertyNotFound);
    }
    
    if (prop.is_readonly()) {
        return std::unexpected(PropertyAccessError::PropertyReadOnly);
    }
    
    // Try to convert value to property type
    rttr::variant converted_value = value;
    if (!converted_value.convert(prop.get_type())) {
        return std::unexpected(PropertyAccessError::TypeMismatch);
    }
    
    bool success = prop.set_value(instance, converted_value);
    if (!success) {
        return std::unexpected(PropertyAccessError::ConversionFailed);
    }
    
    return {};
#endif
}

std::expected<rttr::variant, PropertyAccessError> 
DynamicPropertyAccess::get_property(const rttr::instance& instance, const std::string& type_name, 
                                    const std::string& property_name) {
#ifdef __APPLE__
    return std::unexpected(PropertyAccessError::ConversionFailed);
#else
    rttr::type expected_type = meta::TypeRegistry::instance().get_type(type_name);
    if (!expected_type.is_valid()) {
        return std::unexpected(PropertyAccessError::TypeNotFound);
    }
    
    rttr::type actual_type = instance.get_type();
    if (actual_type != expected_type && !actual_type.is_derived_from(expected_type)) {
        return std::unexpected(PropertyAccessError::TypeMismatch);
    }
    
    return get_property(instance, property_name);
#endif
}

std::expected<void, PropertyAccessError> 
DynamicPropertyAccess::set_property(rttr::instance& instance, const std::string& type_name, 
                                    const std::string& property_name, const rttr::variant& value) {
#ifdef __APPLE__
    return std::unexpected(PropertyAccessError::ConversionFailed);
#else
    rttr::type expected_type = meta::TypeRegistry::instance().get_type(type_name);
    if (!expected_type.is_valid()) {
        return std::unexpected(PropertyAccessError::TypeNotFound);
    }
    
    rttr::type actual_type = instance.get_type();
    if (actual_type != expected_type && !actual_type.is_derived_from(expected_type)) {
        return std::unexpected(PropertyAccessError::TypeMismatch);
    }
    
    return set_property(instance, property_name, value);
#endif
}

bool DynamicPropertyAccess::has_property(const rttr::instance& instance, const std::string& property_name) {
#ifdef __APPLE__
    return false;
#else
    if (!instance.is_valid()) {
        return false;
    }
    
    rttr::type type = instance.get_type();
    return type.get_property(property_name).is_valid();
#endif
}

bool DynamicPropertyAccess::is_readonly(const rttr::instance& instance, const std::string& property_name) {
#ifdef __APPLE__
    return true;
#else
    if (!instance.is_valid()) {
        return true;
    }
    
    rttr::type type = instance.get_type();
    rttr::property prop = type.get_property(property_name);
    
    if (!prop.is_valid()) {
        return true;
    }
    
    return prop.is_readonly();
#endif
}

std::optional<rttr::type> DynamicPropertyAccess::get_property_type(const rttr::instance& instance, 
                                                                     const std::string& property_name) {
#ifdef __APPLE__
    return std::nullopt;
#else
    if (!instance.is_valid()) {
        return std::nullopt;
    }
    
    rttr::type type = instance.get_type();
    rttr::property prop = type.get_property(property_name);
    
    if (!prop.is_valid()) {
        return std::nullopt;
    }
    
    return prop.get_type();
#endif
}

rttr::variant DynamicPropertyAccess::value_to_variant(const kernel::Value& value) {
#ifdef __APPLE__
    // RTTR stubs on macOS don't support full variant conversion
    return rttr::variant();
#else
    // Convert Meld Value to RTTR variant
    return std::visit([](const auto& v) -> rttr::variant {
        return rttr::variant(v);
    }, value);
#endif
}

std::expected<kernel::Value, PropertyAccessError> 
DynamicPropertyAccess::variant_to_value(const rttr::variant& variant) {
    if (!variant.is_valid()) {
        return std::unexpected(PropertyAccessError::ConversionFailed);
    }
    
#ifdef __APPLE__
    // RTTR stubs on macOS don't support is_type/get_value — return conversion error
    return std::unexpected(PropertyAccessError::ConversionFailed);
#else
    // Try to extract known Meld types
    if (variant.is_type<std::shared_ptr<kernel::Symbol>>()) {
        return kernel::Value(variant.get_value<std::shared_ptr<kernel::Symbol>>());
    }
    else if (variant.is_type<std::shared_ptr<kernel::Integer>>()) {
        return kernel::Value(variant.get_value<std::shared_ptr<kernel::Integer>>());
    }
    else if (variant.is_type<std::shared_ptr<kernel::Boolean>>()) {
        return kernel::Value(variant.get_value<std::shared_ptr<kernel::Boolean>>());
    }
    else if (variant.is_type<std::shared_ptr<kernel::String>>()) {
        return kernel::Value(variant.get_value<std::shared_ptr<kernel::String>>());
    }
    else if (variant.is_type<std::shared_ptr<kernel::Function>>()) {
        return kernel::Value(variant.get_value<std::shared_ptr<kernel::Function>>());
    }
    
    // Try to convert basic types
    if (variant.is_type<std::string>()) {
        auto str = std::make_shared<kernel::String>(variant.get_value<std::string>());
        return kernel::Value(str);
    }
    else if (variant.is_type<int64_t>()) {
        auto num = std::make_shared<kernel::Integer>(variant.get_value<int64_t>());
        return kernel::Value(num);
    }
    else if (variant.is_type<int>()) {
        auto num = std::make_shared<kernel::Integer>(static_cast<int64_t>(variant.get_value<int>()));
        return kernel::Value(num);
    }
    else if (variant.is_type<bool>()) {
        auto b = std::make_shared<kernel::Boolean>(variant.get_value<bool>());
        return kernel::Value(b);
    }
    
    return std::unexpected(PropertyAccessError::ConversionFailed);
#endif
}

std::vector<std::string> DynamicPropertyAccess::get_all_properties(const rttr::instance& instance) {
    std::vector<std::string> result;
#ifdef __APPLE__
    return result;
#else
    if (!instance.is_valid()) {
        return result;
    }
    
    rttr::type type = instance.get_type();
    auto props = type.get_properties();
    
    result.reserve(props.size());
    for (const auto& prop : props) {
        result.push_back(prop.get_name());
    }
    
    return result;
#endif
}

std::unordered_map<std::string, rttr::variant> 
DynamicPropertyAccess::get_all_property_values(const rttr::instance& instance) {
    std::unordered_map<std::string, rttr::variant> result;
#ifdef __APPLE__
    return result;
#else
    if (!instance.is_valid()) {
        return result;
    }
    
    rttr::type type = instance.get_type();
    auto props = type.get_properties();
    
    for (const auto& prop : props) {
        rttr::variant value = prop.get_value(instance);
        if (value.is_valid()) {
            result[prop.get_name()] = value;
        }
    }
    
    return result;
#endif
}

std::expected<void, PropertyAccessError> 
DynamicPropertyAccess::set_properties(rttr::instance& instance, 
                                      const std::unordered_map<std::string, rttr::variant>& properties) {
#ifdef __APPLE__
    return std::unexpected(PropertyAccessError::ConversionFailed);
#else
    if (!instance.is_valid()) {
        return std::unexpected(PropertyAccessError::InvalidInstance);
    }
    
    for (const auto& [name, value] : properties) {
        auto result = set_property(instance, name, value);
        if (!result) {
            return result;
        }
    }
    
    return {};
#endif
}

} // namespace meld::types
