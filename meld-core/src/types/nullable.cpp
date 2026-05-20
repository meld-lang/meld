#include "meld/types/nullable.hpp"
#include "meld/types/instance.hpp"
#include <stdexcept>
#include <format>

namespace meld::types {

// Nullable implementation

Nullable Nullable::null() {
    return Nullable(std::nullopt);
}

Nullable Nullable::of(kernel::Value value) {
    return Nullable(std::move(value));
}

kernel::Value Nullable::value() const {
    if (!value_.has_value()) {
        throw std::runtime_error("Attempted to access value of null");
    }
    return value_.value();
}

kernel::Value Nullable::value_or(kernel::Value default_value) const {
    return value_.value_or(std::move(default_value));
}

// SafeNavigator implementation

SafeNavigator SafeNavigator::field(const std::string& field_name) {
    if (current_.is_null()) {
        return SafeNavigator(Nullable::null());
    }
    
    try {
        auto value = current_.value();
        
        // Try to get field from struct instance
        auto struct_inst = as_struct_instance(value);
        if (struct_inst.has_value()) {
            auto field_result = (*struct_inst)->get_field(field_name);
            if (field_result.has_value()) {
                return SafeNavigator(Nullable::of(*field_result));
            }
        }
        
        // Try to get field from class instance
        auto class_inst = as_class_instance(value);
        if (class_inst.has_value()) {
            auto field_result = (*class_inst)->get_field(field_name);
            if (field_result.has_value()) {
                return SafeNavigator(Nullable::of(*field_result));
            }
        }
        
        // Field not found or not accessible
        return SafeNavigator(Nullable::null());
    } catch (...) {
        return SafeNavigator(Nullable::null());
    }
}

SafeNavigator SafeNavigator::method(const std::string& method_name, std::vector<kernel::Value> args) {
    if (current_.is_null()) {
        return SafeNavigator(Nullable::null());
    }
    
    // Method invocation would be implemented here
    // For now, return null as methods aren't fully implemented yet
    return SafeNavigator(Nullable::null());
}

// SmartCast implementation

bool SmartCast::is_non_null(const Nullable& value) {
    return value.has_value();
}

bool SmartCast::is_null(const Nullable& value) {
    return value.is_null();
}

std::expected<kernel::Value, std::string> 
SmartCast::cast_non_null(const Nullable& value) {
    if (value.is_null()) {
        return std::unexpected(std::string("Cannot cast null value to non-nullable type"));
    }
    return value.value();
}

// Operator functions

Nullable safe_navigate_field(const Nullable& value, const std::string& field_name) {
    return SafeNavigator(value).field(field_name).result();
}

Nullable safe_navigate_method(const Nullable& value, const std::string& method_name, 
                              std::vector<kernel::Value> args) {
    return SafeNavigator(value).method(method_name, std::move(args)).result();
}

kernel::Value null_coalesce(const Nullable& value, const kernel::Value& default_value) {
    return value.value_or(default_value);
}

Nullable null_coalesce_nullable(const Nullable& value, const Nullable& other) {
    return value || other;
}

} // namespace meld::types
