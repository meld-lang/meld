#pragma once

#include "meld/kernel/primitives.hpp"
#include "meld/meta/metatype.hpp"
#include <expected>
#include <optional>
#include <functional>

namespace meld::types {

// Nullable value wrapper
class Nullable {
public:
    // Create null value
    static Nullable null();
    
    // Create non-null value
    static Nullable of(kernel::Value value);
    
    // Check if value is null
    bool is_null() const { return !value_.has_value(); }
    bool has_value() const { return value_.has_value(); }
    
    // Get the value (throws if null)
    kernel::Value value() const;
    
    // Get value or default
    kernel::Value value_or(kernel::Value default_value) const;
    
    // Safe navigation: apply function if not null
    template<typename F>
    Nullable map(F&& func) const {
        if (is_null()) {
            return Nullable::null();
        }
        return Nullable::of(func(value_.value()));
    }
    
    // Flat map for chaining nullable operations
    template<typename F>
    Nullable flat_map(F&& func) const {
        if (is_null()) {
            return Nullable::null();
        }
        return func(value_.value());
    }
    
    // Null coalescing operator: return this if not null, otherwise other
    Nullable operator||(const Nullable& other) const {
        return has_value() ? *this : other;
    }
    
    // Null coalescing with value
    kernel::Value operator||(const kernel::Value& default_value) const {
        return value_or(default_value);
    }
    
private:
    explicit Nullable(std::optional<kernel::Value> value) : value_(std::move(value)) {}
    
    std::optional<kernel::Value> value_;
};

// Safe navigation helper
class SafeNavigator {
public:
    explicit SafeNavigator(Nullable value) : current_(std::move(value)) {}
    
    // Navigate to field (returns null if current is null or field doesn't exist)
    SafeNavigator field(const std::string& field_name);
    
    // Call method (returns null if current is null or method fails)
    SafeNavigator method(const std::string& method_name, std::vector<kernel::Value> args = {});
    
    // Get the final result
    Nullable result() const { return current_; }
    
private:
    Nullable current_;
};

// Smart cast helper - checks if value is non-null and casts to specific type
class SmartCast {
public:
    // Check if value is non-null
    static bool is_non_null(const Nullable& value);
    
    // Check if value is null
    static bool is_null(const Nullable& value);
    
    // Cast to non-nullable type after null check
    static std::expected<kernel::Value, std::string> 
    cast_non_null(const Nullable& value);
    
    // Smart cast with type narrowing - after null check, value is known to be non-null
    // This enables the compiler to treat the value as non-nullable in the scope
    template<typename F>
    static auto with_non_null(const Nullable& value, F&& func) 
        -> std::expected<decltype(func(std::declval<kernel::Value>())), std::string> 
    {
        if (value.is_null()) {
            return std::unexpected("Value is null");
        }
        return func(value.value());
    }
    
    // Smart cast for conditional execution - only executes if non-null
    template<typename F>
    static void if_non_null(const Nullable& value, F&& func) {
        if (!value.is_null()) {
            func(value.value());
        }
    }
};

// Operators for nullable types

// Safe navigation operator: value?.field
Nullable safe_navigate_field(const Nullable& value, const std::string& field_name);

// Safe navigation operator: value?.method()
Nullable safe_navigate_method(const Nullable& value, const std::string& method_name, 
                              std::vector<kernel::Value> args = {});

// Null coalescing operator: value ?? default
kernel::Value null_coalesce(const Nullable& value, const kernel::Value& default_value);

// Null coalescing with nullable: value ?? other_nullable
Nullable null_coalesce_nullable(const Nullable& value, const Nullable& other);

} // namespace meld::types
