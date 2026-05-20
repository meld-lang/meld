#pragma once

#include "meld/kernel/primitives.hpp"
#include "meld/meta/metatype.hpp"
#include <variant>
#include <functional>
#include <string>
#include <cassert>
#include <type_traits>

namespace meld::types {

// Forward declarations
template<typename T>
class Option;

// Some variant - represents a value
template<typename T>
class Some {
public:
    explicit Some(T value) : value_(std::move(value)) {}
    
    const T& value() const { return value_; }
    T& value() { return value_; }
    
private:
    T value_;
};

// None variant - represents absence of value
struct None {
    // Empty struct to represent absence
};

// Option<T> type - represents either Some<T> or None
template<typename T>
class Option {
public:
    // Constructors
    static Option some(T value) {
        return Option(Some<T>(std::move(value)));
    }
    
    static Option none() {
        return Option(None{});
    }
    
    // Type queries
    bool is_some() const {
        return std::holds_alternative<Some<T>>(variant_);
    }
    
    bool is_none() const {
        return std::holds_alternative<None>(variant_);
    }
    
    // Value accessors (safe - returns optional-like behavior)
    const T& value() const {
        assert(is_some() && "Called value() on None variant");
        return std::get<Some<T>>(variant_).value();
    }
    
    T& value() {
        assert(is_some() && "Called value() on None variant");
        return std::get<Some<T>>(variant_).value();
    }
    
    // Safe accessors
    T value_or(T default_value) const {
        return is_some() ? value() : std::move(default_value);
    }
    
    // Unwrap (asserts if none - in full Meld this would use effects)
    T unwrap() const {
        assert(is_some() && "Called unwrap() on None variant");
        return value();
    }
    
    T unwrap_or(T default_value) const {
        return value_or(std::move(default_value));
    }
    
    T unwrap_or_else(std::function<T()> func) const {
        return is_some() ? value() : func();
    }
    
    // Expect with custom message
    T expect(const std::string& message) const {
        assert(is_some() && message.c_str());
        return value();
    }
    
    // Combinators
    
    // Map: transform Some value, leave None unchanged
    template<typename F>
    auto map(F&& func) const -> Option<decltype(func(std::declval<T>()))> {
        using U = decltype(func(std::declval<T>()));
        
        if (is_some()) {
            return Option<U>::some(func(value()));
        } else {
            return Option<U>::none();
        }
    }
    
    // FlatMap (and_then): chain operations that return Option
    template<typename F>
    auto and_then(F&& func) const -> decltype(func(std::declval<T>())) {
        if (is_some()) {
            return func(value());
        } else {
            using OptionType = decltype(func(std::declval<T>()));
            return OptionType::none();
        }
    }
    
    // Filter: keep Some if predicate is true, otherwise None
    template<typename F>
    Option filter(F&& predicate) const {
        if (is_some() && predicate(value())) {
            return *this;
        } else {
            return Option::none();
        }
    }
    
    // Or: return this if Some, otherwise return other
    Option or_else(const Option& other) const {
        return is_some() ? *this : other;
    }
    
    // Or with function
    template<typename F>
    Option or_else_with(F&& func) const {
        return is_some() ? *this : func();
    }
    
    // Xor: Some if exactly one is Some, otherwise None
    Option xor_with(const Option& other) const {
        if (is_some() && other.is_none()) {
            return *this;
        } else if (is_none() && other.is_some()) {
            return other;
        } else {
            return Option::none();
        }
    }
    
    // Zip: combine two Options into Option of pair
    template<typename U>
    Option<std::pair<T, U>> zip(const Option<U>& other) const {
        if (is_some() && other.is_some()) {
            return Option<std::pair<T, U>>::some(std::make_pair(value(), other.value()));
        } else {
            return Option<std::pair<T, U>>::none();
        }
    }
    
    // Zip with function
    template<typename U, typename F>
    auto zip_with(const Option<U>& other, F&& func) const 
        -> Option<decltype(func(std::declval<T>(), std::declval<U>()))> 
    {
        using R = decltype(func(std::declval<T>(), std::declval<U>()));
        
        if (is_some() && other.is_some()) {
            return Option<R>::some(func(value(), other.value()));
        } else {
            return Option<R>::none();
        }
    }
    
    // Match pattern (visitor pattern)
    template<typename SomeFunc, typename NoneFunc>
    auto match(SomeFunc&& on_some, NoneFunc&& on_none) const 
        -> decltype(on_some(std::declval<T>())) 
    {
        if (is_some()) {
            return on_some(value());
        } else {
            return on_none();
        }
    }
    
    // Take: replace with None and return the original value
    Option take() {
        if (is_some()) {
            Option result = *this;
            *this = Option::none();
            return result;
        }
        return Option::none();
    }
    
    // Replace: replace with new value and return old value
    Option replace(T new_value) {
        Option old = *this;
        *this = Option::some(std::move(new_value));
        return old;
    }
    
    // Contains: check if Option contains specific value
    bool contains(const T& value) const {
        return is_some() && this->value() == value;
    }
    
private:
    explicit Option(Some<T> some) : variant_(std::move(some)) {}
    explicit Option(None none) : variant_(std::move(none)) {}
    
    std::variant<Some<T>, None> variant_;
};

// Helper functions for creating Options
template<typename T>
Option<T> make_some(T value) {
    return Option<T>::some(std::move(value));
}

inline None NoneValue() {
    return None{};
}

// Transpose: convert Option<Result<T, E>> to Result<Option<T>, E>
template<typename T, typename E>
class Result; // Forward declaration

template<typename T, typename E>
Result<Option<T>, E> transpose(const Option<Result<T, E>>& opt);

} // namespace meld::types
