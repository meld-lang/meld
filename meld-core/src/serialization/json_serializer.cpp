#include "meld/serialization/json_serializer.hpp"
#include "meld/types/dynamic_property_access.hpp"
#include "meld/meta/type_registration.hpp"
#include <sstream>

namespace meld::serialization {

std::string to_string(SerializationError error) {
    switch (error) {
        case SerializationError::InvalidInstance:
            return "Invalid instance";
        case SerializationError::TypeNotRegistered:
            return "Type not registered";
        case SerializationError::UnsupportedType:
            return "Unsupported type";
        case SerializationError::SerializationFailed:
            return "Serialization failed";
        case SerializationError::DeserializationFailed:
            return "Deserialization failed";
        case SerializationError::InvalidJson:
            return "Invalid JSON";
        case SerializationError::TypeMismatch:
            return "Type mismatch";
        default:
            return "Unknown error";
    }
}

std::expected<nlohmann::json, SerializationError>
JsonSerializer::serialize(const rttr::instance& instance) {
#ifdef __APPLE__
    return std::unexpected(SerializationError::UnsupportedType);
#else
    if (!instance.is_valid()) {
        return std::unexpected(SerializationError::InvalidInstance);
    }
    
    rttr::type type = instance.get_type();
    
    if (type.is_arithmetic() || type == rttr::type::get<std::string>()) {
        return serialize_atomic(instance.get_wrapped_instance());
    }
    
    return serialize_object(instance);
#endif
}

std::expected<nlohmann::json, SerializationError>
JsonSerializer::serialize_variant(const rttr::variant& variant) {
#ifdef __APPLE__
    return std::unexpected(SerializationError::UnsupportedType);
#else
    if (!variant.is_valid()) {
        return std::unexpected(SerializationError::InvalidInstance);
    }
    
    rttr::type type = variant.get_type();
    
    if (type.is_arithmetic() || type == rttr::type::get<std::string>()) {
        return serialize_atomic(variant);
    }
    
    if (type.is_sequential_container()) {
        return serialize_collection(variant);
    }
    
    rttr::instance instance = variant;
    return serialize_object(instance);
#endif
}

std::expected<nlohmann::json, SerializationError>
JsonSerializer::serialize_value(const kernel::Value& value) {
#ifdef __APPLE__
    return std::unexpected(SerializationError::UnsupportedType);
#else
    rttr::variant variant = types::DynamicPropertyAccess::value_to_variant(value);
    return serialize_variant(variant);
#endif
}

std::expected<nlohmann::json, SerializationError>
JsonSerializer::serialize_atomic(const rttr::variant& variant) {
#ifdef __APPLE__
    return std::unexpected(SerializationError::UnsupportedType);
#else
    rttr::type type = variant.get_type();
    
    if (type == rttr::type::get<bool>()) {
        return nlohmann::json(variant.get_value<bool>());
    }
    else if (type == rttr::type::get<int>()) {
        return nlohmann::json(variant.get_value<int>());
    }
    else if (type == rttr::type::get<int64_t>()) {
        return nlohmann::json(variant.get_value<int64_t>());
    }
    else if (type == rttr::type::get<size_t>()) {
        return nlohmann::json(variant.get_value<size_t>());
    }
    else if (type == rttr::type::get<double>()) {
        return nlohmann::json(variant.get_value<double>());
    }
    else if (type == rttr::type::get<std::string>()) {
        return nlohmann::json(variant.get_value<std::string>());
    }
    
    return std::unexpected(SerializationError::UnsupportedType);
#endif
}

std::expected<nlohmann::json, SerializationError>
JsonSerializer::serialize_object(const rttr::instance& instance) {
#ifdef __APPLE__
    return std::unexpected(SerializationError::UnsupportedType);
#else
    nlohmann::json json_obj = nlohmann::json::object();
    
    rttr::type type = instance.get_type();
    
    add_type_info(json_obj, type);
    
    auto props = type.get_properties();
    for (const auto& prop : props) {
        rttr::variant value = prop.get_value(instance);
        if (!value.is_valid()) {
            continue;
        }
        
        auto serialized = serialize_variant(value);
        if (serialized) {
            json_obj[prop.get_name().to_string()] = serialized.value();
        }
    }
    
    return json_obj;
#endif
}

std::expected<nlohmann::json, SerializationError>
JsonSerializer::serialize_collection(const rttr::variant& variant) {
#ifdef __APPLE__
    return std::unexpected(SerializationError::UnsupportedType);
#else
    nlohmann::json json_array = nlohmann::json::array();
    
    auto view = variant.create_sequential_view();
    if (!view.is_valid()) {
        return std::unexpected(SerializationError::SerializationFailed);
    }
    
    for (const auto& item : view) {
        auto serialized = serialize_variant(item);
        if (serialized) {
            json_array.push_back(serialized.value());
        }
    }
    
    return json_array;
#endif
}

std::expected<rttr::variant, SerializationError>
JsonSerializer::deserialize(const nlohmann::json& json, const rttr::type& type) {
#ifdef __APPLE__
    return std::unexpected(SerializationError::UnsupportedType);
#else
    if (!type.is_valid()) {
        return std::unexpected(SerializationError::TypeNotRegistered);
    }
    
    if (type.is_arithmetic() || type == rttr::type::get<std::string>()) {
        return deserialize_atomic(json, type);
    }
    
    if (type.is_sequential_container() && json.is_array()) {
        return deserialize_collection(json, type);
    }
    
    if (json.is_object()) {
        return deserialize_object(json, type);
    }
    
    return std::unexpected(SerializationError::InvalidJson);
#endif
}

std::expected<kernel::Value, SerializationError>
JsonSerializer::deserialize_value(const nlohmann::json& json) {
    // Try to infer type from JSON — this works on macOS too since it uses kernel types
    if (json.is_string()) {
        auto str = std::make_shared<kernel::String>(json.get<std::string>());
        return kernel::Value(str);
    }
    else if (json.is_number_integer()) {
        auto num = std::make_shared<kernel::Integer>(json.get<int64_t>());
        return kernel::Value(num);
    }
    else if (json.is_boolean()) {
        auto b = kernel::Boolean::from(json.get<bool>());
        return kernel::Value(b);
    }
#ifndef __APPLE__
    else if (json.is_object() && json.contains("__type__")) {
        std::string type_name = json["__type__"];
        rttr::type type = meta::TypeRegistry::instance().get_type(type_name);
        
        if (!type.is_valid()) {
            return std::unexpected(SerializationError::TypeNotRegistered);
        }
        
        auto variant = deserialize(json, type);
        if (!variant) {
            return std::unexpected(variant.error());
        }
        
        auto value = types::DynamicPropertyAccess::variant_to_value(variant.value());
        if (!value) {
            return std::unexpected(SerializationError::DeserializationFailed);
        }
        
        return value.value();
    }
#endif
    
    return std::unexpected(SerializationError::UnsupportedType);
}

std::expected<rttr::variant, SerializationError>
JsonSerializer::deserialize_atomic(const nlohmann::json& json, const rttr::type& type) {
#ifdef __APPLE__
    return std::unexpected(SerializationError::UnsupportedType);
#else
    try {
        if (type == rttr::type::get<bool>()) {
            return rttr::variant(json.get<bool>());
        }
        else if (type == rttr::type::get<int>()) {
            return rttr::variant(json.get<int>());
        }
        else if (type == rttr::type::get<int64_t>()) {
            return rttr::variant(json.get<int64_t>());
        }
        else if (type == rttr::type::get<size_t>()) {
            return rttr::variant(json.get<size_t>());
        }
        else if (type == rttr::type::get<double>()) {
            return rttr::variant(json.get<double>());
        }
        else if (type == rttr::type::get<std::string>()) {
            return rttr::variant(json.get<std::string>());
        }
    }
    catch (...) {
        return std::unexpected(SerializationError::DeserializationFailed);
    }
    
    return std::unexpected(SerializationError::UnsupportedType);
#endif
}

std::expected<rttr::variant, SerializationError>
JsonSerializer::deserialize_object(const nlohmann::json& json, const rttr::type& type) {
#ifdef __APPLE__
    return std::unexpected(SerializationError::UnsupportedType);
#else
    rttr::variant instance = type.create();
    if (!instance.is_valid()) {
        return std::unexpected(SerializationError::DeserializationFailed);
    }
    
    auto props = type.get_properties();
    for (const auto& prop : props) {
        std::string prop_name = prop.get_name().to_string();
        
        if (!json.contains(prop_name)) {
            continue;
        }
        
        if (prop.is_readonly()) {
            continue;
        }
        
        auto value = deserialize(json[prop_name], prop.get_type());
        if (value) {
            prop.set_value(instance, value.value());
        }
    }
    
    return instance;
#endif
}

std::expected<rttr::variant, SerializationError>
JsonSerializer::deserialize_collection(const nlohmann::json& json, const rttr::type& type) {
#ifdef __APPLE__
    return std::unexpected(SerializationError::UnsupportedType);
#else
    rttr::variant collection = type.create();
    if (!collection.is_valid()) {
        return std::unexpected(SerializationError::DeserializationFailed);
    }
    
    auto view = collection.create_sequential_view();
    if (!view.is_valid()) {
        return std::unexpected(SerializationError::DeserializationFailed);
    }
    
    rttr::type value_type = view.get_value_type();
    
    for (const auto& item : json) {
        auto value = deserialize(item, value_type);
        if (value) {
            view.insert(view.end(), value.value());
        }
    }
    
    return collection;
#endif
}

std::expected<std::string, SerializationError>
JsonSerializer::to_json_string(const rttr::instance& instance, int indent) {
#ifdef __APPLE__
    return std::unexpected(SerializationError::UnsupportedType);
#else
    auto json = serialize(instance);
    if (!json) {
        return std::unexpected(json.error());
    }
    
    return json.value().dump(indent);
#endif
}

std::expected<rttr::variant, SerializationError>
JsonSerializer::from_json_string(const std::string& json_str, const rttr::type& type) {
#ifdef __APPLE__
    return std::unexpected(SerializationError::UnsupportedType);
#else
    try {
        nlohmann::json json = nlohmann::json::parse(json_str);
        return deserialize(json, type);
    }
    catch (...) {
        return std::unexpected(SerializationError::InvalidJson);
    }
#endif
}

bool JsonSerializer::test_round_trip(const rttr::instance& instance) {
#ifdef __APPLE__
    return false;
#else
    auto serialized = serialize(instance);
    if (!serialized) {
        return false;
    }
    
    rttr::type type = instance.get_type();
    auto deserialized = deserialize(serialized.value(), type);
    if (!deserialized) {
        return false;
    }
    
    auto reserialized = serialize(deserialized.value());
    if (!reserialized) {
        return false;
    }
    
    return serialized.value() == reserialized.value();
#endif
}

bool JsonSerializer::is_serializable(const rttr::type& type) {
    if (!type.is_valid()) {
        return false;
    }
    
    if (type.is_arithmetic() || type == rttr::type::get<std::string>()) {
        return true;
    }
    
    if (type.is_sequential_container()) {
        return true;
    }
    
    if (type.is_class()) {
        return true;
    }
    
    return false;
}

std::optional<std::string> JsonSerializer::get_type_name(const nlohmann::json& json) {
    if (json.is_object() && json.contains("__type__")) {
        return json["__type__"].get<std::string>();
    }
    return std::nullopt;
}

void JsonSerializer::add_type_info(nlohmann::json& json, const rttr::type& type) {
#ifdef __APPLE__
    json["__type__"] = type.get_name();
#else
    json["__type__"] = type.get_name().to_string();
#endif
}

} // namespace meld::serialization
