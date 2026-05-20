#pragma once

#include "meld/kernel/primitives.hpp"
#include "meld/meta/reflection_helper.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <expected>

namespace meld::serialization {

/**
 * SerializationError - Errors that can occur during serialization
 */
enum class SerializationError {
    InvalidInstance,
    TypeNotRegistered,
    UnsupportedType,
    SerializationFailed,
    DeserializationFailed,
    InvalidJson,
    TypeMismatch
};

/**
 * Convert SerializationError to string
 */
std::string to_string(SerializationError error);

/**
 * JsonSerializer - Serializes and deserializes Meld objects to/from JSON
 * 
 * This class uses RTTR reflection to automatically serialize and deserialize
 * Meld objects, including nested objects and collections.
 */
class JsonSerializer {
public:
    /**
     * Serialize an RTTR instance to JSON
     * 
     * @param instance RTTR instance to serialize
     * @return JSON object or error
     */
    static std::expected<nlohmann::json, SerializationError>
    serialize(const rttr::instance& instance);
    
    /**
     * Serialize an RTTR variant to JSON
     * 
     * @param variant RTTR variant to serialize
     * @return JSON value or error
     */
    static std::expected<nlohmann::json, SerializationError>
    serialize_variant(const rttr::variant& variant);
    
    /**
     * Serialize a Meld Value to JSON
     * 
     * @param value Meld Value to serialize
     * @return JSON value or error
     */
    static std::expected<nlohmann::json, SerializationError>
    serialize_value(const kernel::Value& value);
    
    /**
     * Deserialize JSON to an RTTR instance
     * 
     * @param json JSON object
     * @param type Target type
     * @return RTTR variant containing the instance or error
     */
    static std::expected<rttr::variant, SerializationError>
    deserialize(const nlohmann::json& json, const rttr::type& type);
    
    /**
     * Deserialize JSON to a Meld Value
     * 
     * @param json JSON value
     * @return Meld Value or error
     */
    static std::expected<kernel::Value, SerializationError>
    deserialize_value(const nlohmann::json& json);
    
    /**
     * Serialize to JSON string
     * 
     * @param instance RTTR instance to serialize
     * @param indent Indentation for pretty printing (0 = compact)
     * @return JSON string or error
     */
    static std::expected<std::string, SerializationError>
    to_json_string(const rttr::instance& instance, int indent = 2);
    
    /**
     * Deserialize from JSON string
     * 
     * @param json_str JSON string
     * @param type Target type
     * @return RTTR variant containing the instance or error
     */
    static std::expected<rttr::variant, SerializationError>
    from_json_string(const std::string& json_str, const rttr::type& type);
    
    /**
     * Round-trip test: serialize then deserialize
     * 
     * @param instance Original instance
     * @return true if round-trip preserves data
     */
    static bool test_round_trip(const rttr::instance& instance);
    
    /**
     * Serialize a collection to JSON array
     * 
     * @param variant Collection variant
     * @return JSON array or error
     */
    static std::expected<nlohmann::json, SerializationError>
    serialize_collection(const rttr::variant& variant);
    
    /**
     * Deserialize JSON array to collection
     * 
     * @param json JSON array
     * @param type Target collection type
     * @return Collection variant or error
     */
    static std::expected<rttr::variant, SerializationError>
    deserialize_collection(const nlohmann::json& json, const rttr::type& type);
    
    /**
     * Check if a type is serializable
     * 
     * @param type Type to check
     * @return true if type can be serialized
     */
    static bool is_serializable(const rttr::type& type);
    
    /**
     * Get type name from JSON object
     * 
     * @param json JSON object
     * @return Type name if present
     */
    static std::optional<std::string> get_type_name(const nlohmann::json& json);
    
    /**
     * Add type information to JSON object
     * 
     * @param json JSON object to modify
     * @param type Type to add
     */
    static void add_type_info(nlohmann::json& json, const rttr::type& type);

private:
    /**
     * Serialize atomic types (primitives)
     */
    static std::expected<nlohmann::json, SerializationError>
    serialize_atomic(const rttr::variant& variant);
    
    /**
     * Serialize object with properties
     */
    static std::expected<nlohmann::json, SerializationError>
    serialize_object(const rttr::instance& instance);
    
    /**
     * Deserialize atomic types
     */
    static std::expected<rttr::variant, SerializationError>
    deserialize_atomic(const nlohmann::json& json, const rttr::type& type);
    
    /**
     * Deserialize object with properties
     */
    static std::expected<rttr::variant, SerializationError>
    deserialize_object(const nlohmann::json& json, const rttr::type& type);
};

} // namespace meld::serialization
