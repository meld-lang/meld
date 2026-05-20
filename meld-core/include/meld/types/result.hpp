#pragma once

#include "meld/kernel/primitives.hpp"
#include "meld/meta/metatype.hpp"
#include <expected>
#include <variant>
#include <functional>
#include <string>
#include <cassert>
#include <type_traits>

namespace meld::types {

// Forward declarations
template<typename T, typename E>
class Result;

// Success variant
template<typename T>
class Success {
public:
    explicit Success(T value) : value_(std::move(value)) {}
    
    const T& value() const { return value_; }
    T& value() { return value_; }
    
private:
    T value_;
};

// Error variant
template<typename E>
class Failure {
public:
    explicit Failure(E error) : error_(std::move(error)) {}
    
    const E& error() const { return error_; }
    E& error() { return error_; }
    
private:
    E error_;
};

// Result<T, E> type - represents either Success<T> or Failure<E>
template<typename T, typename E = std::string>
class Result {
public:
    // Constructors
    static Result success(T value) {
        return Result(Success<T>(std::move(value)));
    }
    
    static Result error(E err) {
        return Result(Failure<E>(std::move(err)));
    }
    
    // Type queries
    bool is_success() const {
        return std::holds_alternative<Success<T>>(variant_);
    }
    
    bool is_error() const {
        return std::holds_alternative<Failure<E>>(variant_);
    }
    
    // Value accessors (safe - returns optional-like behavior)
    const T& value() const {
        // In a full Meld implementation, this would use algebraic effects
        // For now, we use assertions for development safety
        assert(is_success() && "Called value() on Error variant");
        return std::get<Success<T>>(variant_).value();
    }
    
    T& value() {
        assert(is_success() && "Called value() on Error variant");
        return std::get<Success<T>>(variant_).value();
    }
    
    const E& error() const {
        assert(is_error() && "Called error() on Success variant");
        return std::get<Failure<E>>(variant_).error();
    }
    
    E& error() {
        assert(is_error() && "Called error() on Success variant");
        return std::get<Failure<E>>(variant_).error();
    }
    
    // Safe accessors
    T value_or(T default_value) const {
        return is_success() ? value() : std::move(default_value);
    }
    
    // Unwrap (asserts if error - in full Meld this would use effects)
    T unwrap() const {
        assert(is_success() && "Called unwrap() on Error variant");
        return value();
    }
    
    T unwrap_or(T default_value) const {
        return value_or(std::move(default_value));
    }
    
    // Combinators
    
    // Map: transform success value, leave error unchanged
    template<typename F>
    auto map(F&& func) const -> Result<decltype(func(std::declval<T>())), E> {
        using U = decltype(func(std::declval<T>()));
        
        if (is_success()) {
            return Result<U, E>::success(func(value()));
        } else {
            return Result<U, E>::error(error());
        }
    }
    
    // FlatMap: chain operations that return Result
    template<typename F>
    auto flat_map(F&& func) const -> decltype(func(std::declval<T>())) {
        if (is_success()) {
            return func(value());
        } else {
            using ResultType = decltype(func(std::declval<T>()));
            return ResultType::error(error());
        }
    }
    
    // MapError: transform error value, leave success unchanged
    template<typename F>
    auto map_error(F&& func) const -> Result<T, decltype(func(std::declval<E>()))> {
        using F2 = decltype(func(std::declval<E>()));
        
        if (is_error()) {
            return Result<T, F2>::error(func(error()));
        } else {
            return Result<T, F2>::success(value());
        }
    }
    
    // And: combine with another Result (returns second if both success)
    template<typename U>
    Result<U, E> and_then(const Result<U, E>& other) const {
        if (is_success()) {
            return other;
        } else {
            return Result<U, E>::error(error());
        }
    }
    
    // AndThen with function (alias for flat_map for consistency with Rust)
    template<typename F>
    auto and_then(F&& func) const -> decltype(func(std::declval<T>())) {
        return flat_map(std::forward<F>(func));
    }
    
    // Or: return this if success, otherwise return other
    Result or_else(const Result& other) const {
        return is_success() ? *this : other;
    }
    
    // OrElse with function
    template<typename F>
    Result or_else_with(F&& func) const {
        if (is_error()) {
            return func(error());
        }
        return *this;
    }
    
    // Inspect: call function with success value without consuming
    template<typename F>
    const Result& inspect(F&& func) const {
        if (is_success()) {
            func(value());
        }
        return *this;
    }
    
    // InspectErr: call function with error value without consuming
    template<typename F>
    const Result& inspect_err(F&& func) const {
        if (is_error()) {
            func(error());
        }
        return *this;
    }
    
    // Expect: unwrap with custom message
    T expect(const std::string& message) const {
        assert(is_success() && message.c_str());
        return value();
    }
    
    // ExpectErr: get error with custom message
    E expect_err(const std::string& message) const {
        assert(is_error() && message.c_str());
        return error();
    }
    
    // Contains: check if Result contains specific success value
    bool contains(const T& val) const {
        return is_success() && value() == val;
    }
    
    // ContainsErr: check if Result contains specific error value
    bool contains_err(const E& err) const {
        return is_error() && error() == err;
    }
    
    // Match pattern (visitor pattern)
    template<typename SuccessFunc, typename ErrorFunc>
    auto match(SuccessFunc&& on_success, ErrorFunc&& on_error) const 
        -> decltype(on_success(std::declval<T>())) 
    {
        if (is_success()) {
            return on_success(value());
        } else {
            return on_error(error());
        }
    }
    
private:
    explicit Result(Success<T> success) : variant_(std::move(success)) {}
    explicit Result(Failure<E> error) : variant_(std::move(error)) {}
    
    std::variant<Success<T>, Failure<E>> variant_;
};

// Result<void, E> specialization — represents success (no value) or error
template<typename E>
class Result<void, E> {
public:
    static Result success() { return Result(true); }
    static Result error(E err) { return Result(std::move(err)); }

    bool is_success() const { return !error_; }
    bool is_error() const { return !!error_; }

    const E& error() const { return *error_; }
    E& error() { return *error_; }

    void value() const { assert(is_success()); }

private:
    explicit Result(bool) : error_(std::nullopt) {}
    explicit Result(E err) : error_(std::move(err)) {}

    std::optional<E> error_;
};

// Attempt.run block helper for error recovery
template<typename T, typename E = std::string>
class AttemptResult {
public:
    explicit AttemptResult(Result<T, E> result) : result_(std::move(result)) {}
    
    // Chain with onFailure
    AttemptResult& on_failure(std::function<void(const E&)> handler) {
        if (result_.is_error()) {
            handler(result_.error());
        }
        return *this;
    }
    
    // Chain with onSuccess
    AttemptResult& on_success(std::function<void(const T&)> handler) {
        if (result_.is_success()) {
            handler(result_.value());
        }
        return *this;
    }
    
    // Get the underlying result
    Result<T, E> result() const { return result_; }
    
    // Implicit conversion to Result
    operator Result<T, E>() const { return result_; }
    
private:
    Result<T, E> result_;
};

// Helper trait to extract T and E from Result<T, E>
template<typename R> struct result_traits;
template<typename T, typename E>
struct result_traits<Result<T, E>> {
    using value_type = T;
    using error_type = E;
};

// Attempt namespace for error recovery
class Attempt {
public:
    // Create an attempt block for functions that return Result<T, E> (auto-deduced)
    template<typename F,
             typename R = std::invoke_result_t<F>,
             typename T = typename result_traits<R>::value_type,
             typename E = typename result_traits<R>::error_type>
    static auto run(F&& func) -> AttemptResult<T, E> {
        auto result = func();
        return AttemptResult<T, E>(result);
    }
    
    // Create an attempt block for functions that return plain values (wrapped in Result)
    template<typename F>
    static auto run_safe(F&& func) -> AttemptResult<decltype(func()), std::string> {
        using T = decltype(func());
        
        // In a full Meld implementation, this would use algebraic effects
        // to catch any effects performed by func() and convert them to Results
        // For now, we assume the function is safe and returns a value
        return AttemptResult<T, std::string>(Result<T, std::string>::success(func()));
    }
    
    // Create an attempt block with custom error type for safe functions
    template<typename E, typename F>
    static auto run_safe_with_error(F&& func) -> AttemptResult<decltype(func()), E> {
        using T = decltype(func());
        
        // In a full Meld implementation, this would use algebraic effects
        return AttemptResult<T, E>(Result<T, E>::success(func()));
    }
};

// Helper functions for creating Results
template<typename T, typename E = std::string>
Result<T, E> Ok(T value) {
    return Result<T, E>::success(std::move(value));
}

template<typename T, typename E = std::string>
Result<T, E> Err(E error) {
    return Result<T, E>::error(std::move(error));
}

} // namespace meld::types
