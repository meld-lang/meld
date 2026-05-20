#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>
#include <optional>
#include <functional>
#include <expected>
#include <format>
#include <ranges>
#include <concepts>
#include <unordered_map>
#include <stack>
#include <any>
#include <random>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <algorithm>

// Platform-specific includes for native library loading
#ifdef _WIN32
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

namespace meld::types { class StructInstance; }

namespace meld::kernel {

// Forward declarations
class Symbol;
class Cons;
class Vec;
class IntVec;
class Empty;
class Function;
class Integer;
class Float;
class Boolean;
class String;
class Placeholder;
template<typename T> class Optional;
class Continuation;
class DelimitedContinuation;
class MetadataStore;
class NativeHandle;
class NativeFunction;

// Concept for Meld value types
template<typename T>
concept MeldValueType = requires(T t) {
    { t.to_string() } -> std::convertible_to<std::string>;
};

// MetadataStore - Global storage for object metadata
// This enables meta_set and meta_get primitives without modifying object structure
class MetadataStore {
public:
    using MetadataMap = std::unordered_map<std::string, std::any>;
    
    // Get the global metadata store instance
    static MetadataStore& instance() {
        static MetadataStore store;
        return store;
    }
    
    // Attach metadata to an object (identified by pointer address)
    void set_metadata(const void* obj_ptr, const std::string& key, std::any value) {
        auto obj_id = reinterpret_cast<uintptr_t>(obj_ptr);
        metadata_[obj_id][key] = std::move(value);
    }
    
    // Retrieve metadata from an object
    std::optional<std::any> get_metadata(const void* obj_ptr, const std::string& key) const {
        auto obj_id = reinterpret_cast<uintptr_t>(obj_ptr);
        auto obj_it = metadata_.find(obj_id);
        if (obj_it == metadata_.end()) {
            return std::nullopt;
        }
        
        auto key_it = obj_it->second.find(key);
        if (key_it == obj_it->second.end()) {
            return std::nullopt;
        }
        
        return key_it->second;
    }
    
    // Check if metadata exists for an object
    bool has_metadata(const void* obj_ptr, const std::string& key) const {
        auto obj_id = reinterpret_cast<uintptr_t>(obj_ptr);
        auto obj_it = metadata_.find(obj_id);
        if (obj_it == metadata_.end()) {
            return false;
        }
        return obj_it->second.find(key) != obj_it->second.end();
    }
    
    // Clear all metadata for an object
    void clear_metadata(const void* obj_ptr) {
        auto obj_id = reinterpret_cast<uintptr_t>(obj_ptr);
        metadata_.erase(obj_id);
    }
    
    // Clear all metadata (for testing)
    void clear_all() {
        metadata_.clear();
    }
    
private:
    MetadataStore() = default;
    std::unordered_map<uintptr_t, MetadataMap> metadata_;
};

// Value variant - represents any Meld value (C++23 with deducing this)
class Value {
public:
    using ValueType = std::variant<
        std::shared_ptr<Symbol>,
        std::shared_ptr<Cons>,
        std::shared_ptr<Vec>,
        std::shared_ptr<IntVec>,
        std::shared_ptr<Empty>,
        std::shared_ptr<Function>,
        std::shared_ptr<Integer>,
        std::shared_ptr<Float>,
        std::shared_ptr<Boolean>,
        std::shared_ptr<String>,
        std::shared_ptr<Placeholder>,
        std::shared_ptr<Optional<Value>>,
        std::shared_ptr<Continuation>,
        std::shared_ptr<NativeHandle>,
        std::shared_ptr<NativeFunction>,
        std::shared_ptr<types::StructInstance>
    >;

    Value() = default;
    
    // Accept any shared_ptr that's a valid variant alternative
    template<typename T>
    Value(std::shared_ptr<T> ptr) : value_(std::move(ptr)) {}
    
    // C++23: Explicit object parameter (deducing this)
    // Note: Using traditional overloads for MSVC compatibility
    template<typename T>
    bool is() const {
        return std::holds_alternative<std::shared_ptr<T>>(value_);
    }
    
    template<typename T>
    std::shared_ptr<T> as() const {
        return std::get<std::shared_ptr<T>>(value_);
    }
    
    template<typename T>
    std::shared_ptr<T> as() {
        return std::get<std::shared_ptr<T>>(value_);
    }
    
    std::string to_string() const;
    bool is_truthy() const;
    
    // C++23: std::expected for error handling
    template<typename T>
    std::expected<std::shared_ptr<T>, std::string> try_as() const {
        if (is<T>()) {
            return as<T>();
        }
        return std::unexpected(std::format("Expected type {}, got {}", 
            typeid(T).name(), to_string()));
    }
    
    // Get the raw pointer for metadata operations
    const void* get_ptr() const {
        return std::visit([](const auto& ptr) -> const void* {
            return ptr.get();
        }, value_);
    }
    
    // Factory methods for creating Values (defined after type definitions)
    static Value from_int(int64_t value);
    static Value from_string(const std::string& value);
    static Value from_bool(bool value);
    static Value from_unit();
    static Value from_symbol(const std::string& name);
    
    // Convenience accessors (defined after type definitions)
    int64_t as_int() const;
    std::string as_string() const;
    bool as_bool() const;

    bool operator==(const Value& other) const { return value_ == other.value_; }
    
private:
    ValueType value_;
};

// Symbol - Atomic identifiers (C++23 with std::format)
class Symbol {
public:
    explicit Symbol(std::string name) : name_(std::move(name)) {}
    
    // C++23: Explicit object parameter for const/non-const overloads
    template<typename Self>
    auto&& name(this Self&& self) { return std::forward<Self>(self).name_; }
    
    std::string to_string() const { 
        return std::format(":{}", name_); 
    }
    
    // C++23: Defaulted comparison operators
    auto operator<=>(const Symbol&) const = default;
    
private:
    std::string name_;
};

// Cons - Cons cell / Pair
class Cons {
public:
    Cons(Value car, Value cdr) : car_(std::move(car)), cdr_(std::move(cdr)) {}
    
    const Value& car() const { return car_; }
    const Value& cdr() const { return cdr_; }
    
    Value& car() { return car_; }
    Value& cdr() { return cdr_; }
    
    // Aliases for head/tail style access
    const Value& head() const { return car_; }
    const Value& tail() const { return cdr_; }
    Value& head() { return car_; }
    Value& tail() { return cdr_; }
    
    // Cons is never nil (nil is Empty)
    bool is_nil() const { return false; }
    
    std::string to_string() const;
    
private:
    Value car_;
    Value cdr_;
};

// Vec - Contiguous memory block for arrays, strings, buffers
class Vec {
public:
    Vec() = default;
    explicit Vec(std::vector<Value> elements) : elements_(std::move(elements)) {}
    
    const std::vector<Value>& elements() const { return elements_; }
    std::vector<Value>& elements() { return elements_; }
    
    size_t size() const { return elements_.size(); }
    bool empty() const { return elements_.empty(); }
    
    const Value& at(size_t index) const { return elements_.at(index); }
    Value& at(size_t index) { return elements_.at(index); }
    
    void push_back(Value val) { elements_.push_back(std::move(val)); }
    
    std::string to_string() const {
        std::string result = "[";
        for (size_t i = 0; i < elements_.size(); ++i) {
            if (i > 0) result += ", ";
            result += elements_[i].to_string();
        }
        result += "]";
        return result;
    }
    
private:
    std::vector<Value> elements_;
};

// IntVec - Compact mutable integer array for stdlib data structures.
class IntVec {
public:
    IntVec() = default;
    explicit IntVec(std::vector<int64_t> elements) : elements_(std::move(elements)) {}

    const std::vector<int64_t>& elements() const { return elements_; }
    std::vector<int64_t>& elements() { return elements_; }

    size_t size() const { return elements_.size(); }
    bool empty() const { return elements_.empty(); }

    int64_t at(size_t index) const { return elements_.at(index); }
    void set(size_t index, int64_t value) { elements_.at(index) = value; }
    void fill(int64_t value) { std::fill(elements_.begin(), elements_.end(), value); }

    std::string to_string() const {
        std::string result = "[";
        for (size_t i = 0; i < elements_.size(); ++i) {
            if (i > 0) result += ", ";
            result += std::to_string(elements_[i]);
        }
        result += "]";
        return result;
    }

private:
    std::vector<int64_t> elements_;
};

// Empty - Empty list
class Empty {
    struct PrivateTag {};
public:
    explicit Empty(PrivateTag) {}
    
    static std::shared_ptr<Empty> instance() {
        static auto inst = std::make_shared<Empty>(PrivateTag{});
        return inst;
    }
    
    std::string to_string() const { return "empty"; }
};

// Convenience function to create nil/empty value
inline Value nil() { return Value(Empty::instance()); }

// Check if a Value is nil (Empty)
inline bool is_nil(const Value& v) { return v.is<Empty>(); }

// Function - Lambda abstractions with closure support
class Function {
public:
    using NativeImpl = std::function<Value(const std::vector<Value>&)>;
    using Environment = std::unordered_map<std::string, Value>;
    
    Function(std::vector<std::shared_ptr<Symbol>> params, 
             Value body,
             std::optional<NativeImpl> impl = std::nullopt,
             std::optional<std::string> name = std::nullopt,
             std::optional<Environment> closure_env = std::nullopt)
        : params_(std::move(params))
        , body_(std::move(body))
        , impl_(std::move(impl))
        , name_(std::move(name))
        , closure_env_(std::move(closure_env)) {}
    
    const std::vector<std::shared_ptr<Symbol>>& params() const { return params_; }
    const Value& body() const { return body_; }
    const std::optional<NativeImpl>& impl() const { return impl_; }
    const std::optional<std::string>& name() const { return name_; }
    const std::optional<Environment>& closure_env() const { return closure_env_; }
    
    // Check if this is a closure (has captured environment)
    bool is_closure() const { return closure_env_.has_value(); }
    
    // Note: .curry() method is available via the curry() function in partial_application.hpp
    
    std::string to_string() const {
        if (name_) {
            return "<function " + *name_ + ">";
        }
        if (is_closure()) {
            return "<closure>";
        }
        return "<lambda>";
    }
    
private:
    std::vector<std::shared_ptr<Symbol>> params_;
    Value body_;
    std::optional<NativeImpl> impl_;
    std::optional<std::string> name_;
    std::optional<Environment> closure_env_;  // Captured environment for closures
};

// Integer - Arbitrary precision integers (C++23)
class Integer {
public:
    explicit Integer(int64_t value) : value_(value) {}
    
    template<typename Self>
    auto value(this Self&& self) { return self.value_; }
    
    std::string to_string() const { 
        return std::format("{}", value_); 
    }
    
    // C++23: Defaulted spaceship operator
    auto operator<=>(const Integer&) const = default;
    
private:
    int64_t value_;
};

// Float - Floating point values
class Float {
public:
    explicit Float(double value) : value_(value) {}
    
    template<typename Self>
    auto value(this Self&& self) { return self.value_; }
    
    std::string to_string() const { 
        return std::format("{}", value_); 
    }
    
    auto operator<=>(const Float&) const = default;
    
private:
    double value_;
};

// Boolean - Boolean values
class Boolean {
public:
    static std::shared_ptr<Boolean> true_value() {
        static auto inst = std::make_shared<Boolean>(PrivateTag{}, true);
        return inst;
    }
    
    static std::shared_ptr<Boolean> false_value() {
        static auto inst = std::make_shared<Boolean>(PrivateTag{}, false);
        return inst;
    }
    
    static std::shared_ptr<Boolean> from(bool value) {
        return value ? true_value() : false_value();
    }
    
    template<typename Self>
    auto value(this Self&& self) { return self.value_; }
    
    std::string to_string() const { 
        return value_ ? "true" : "false"; 
    }
    
    auto operator<=>(const Boolean&) const = default;
    
    struct PrivateTag {};
    Boolean(PrivateTag, bool value) : value_(value) {}
    
private:
    bool value_;
};

// String - String values (C++23)
class String {
public:
    explicit String(std::string value) : value_(std::move(value)) {}
    
    template<typename Self>
    auto&& value(this Self&& self) { return std::forward<Self>(self).value_; }
    
    std::string to_string() const { 
        return std::format("\"{}\"", value_); 
    }
    
    auto operator<=>(const String&) const = default;
    
private:
    std::string value_;
};

// Placeholder - Placeholder for partial application
class Placeholder {
    struct PrivateTag {};
public:
    explicit Placeholder(PrivateTag) {}
    
    static std::shared_ptr<Placeholder> instance() {
        static auto inst = std::make_shared<Placeholder>(PrivateTag{});
        return inst;
    }
    
    std::string to_string() const { return "_"; }
    
    auto operator<=>(const Placeholder&) const = default;
};

// ============================================================================
// Deferred inline definitions for Value factory methods and accessors
// (These require complete type definitions of Integer, Boolean, String, Empty, Symbol)
// ============================================================================

inline Value Value::from_int(int64_t value) {
    return Value(std::make_shared<Integer>(value));
}

inline Value Value::from_string(const std::string& value) {
    return Value(std::make_shared<String>(value));
}

inline Value Value::from_bool(bool value) {
    return Value(Boolean::from(value));
}

inline Value Value::from_unit() {
    return Value(Empty::instance());
}

inline Value Value::from_symbol(const std::string& name) {
    return Value(std::make_shared<Symbol>(name));
}

inline int64_t Value::as_int() const {
    auto result = try_as<Integer>();
    if (!result.has_value()) {
        throw std::runtime_error("Value is not an Integer");
    }
    return result.value()->value();
}

inline std::string Value::as_string() const {
    auto result = try_as<String>();
    if (!result.has_value()) {
        throw std::runtime_error("Value is not a String");
    }
    return result.value()->value();
}

inline bool Value::as_bool() const {
    auto result = try_as<Boolean>();
    if (!result.has_value()) {
        throw std::runtime_error("Value is not a Boolean");
    }
    return result.value()->value();
}

// Optional - Optional values (C++23 with monadic operations)
template<typename T>
class Optional {
public:
    static std::shared_ptr<Optional<T>> some(T value) {
        return std::make_shared<Optional<T>>(PrivateTag{}, std::move(value), false);
    }
    
    static std::shared_ptr<Optional<T>> none() {
        return std::make_shared<Optional<T>>(PrivateTag{}, T{}, true);
    }
    
    // Note: Using traditional methods for MSVC compatibility
    bool is_empty() const { return is_empty_; }
    
    bool is_some() const { return !is_empty_; }
    
    // std::expected for error handling (returns by value for MSVC compatibility)
    std::expected<T, std::string> get() const {
        if (is_empty_) {
            return std::unexpected("Cannot get value from empty Optional");
        }
        return value_;
    }
    
    T get_or_else(T default_value) const {
        return is_empty_ ? std::move(default_value) : value_;
    }
    
    // C++23: Monadic operations with concepts
    template<std::invocable<T> F>
    auto map(F&& fn) const -> std::shared_ptr<Optional<std::invoke_result_t<F, T>>> {
        using U = std::invoke_result_t<F, T>;
        if (is_empty_) {
            return Optional<U>::none();
        }
        return Optional<U>::some(std::invoke(std::forward<F>(fn), value_));
    }
    
    template<std::invocable<T> F>
        requires std::same_as<std::invoke_result_t<F, T>, std::shared_ptr<Optional<typename std::invoke_result_t<F, T>::element_type::value_type>>>
    auto and_then(F&& fn) const {
        if (is_empty_) {
            using U = typename std::invoke_result_t<F, T>::element_type::value_type;
            return Optional<U>::none();
        }
        return std::invoke(std::forward<F>(fn), value_);
    }
    
    std::string to_string() const {
        if (is_empty_) {
            return "None";
        }
        return std::format("Some({})", value_.to_string());
    }
    
    struct PrivateTag {};
    Optional(PrivateTag, T value, bool is_empty) 
        : value_(std::move(value)), is_empty_(is_empty) {}

private:
    T value_;
    bool is_empty_;
};

// Continuation - Captured execution context for delimited continuations
// This is the core primitive that enables algebraic effects, exceptions, async/await, and generators
class Continuation {
public:
    using ResumeFunc = std::function<Value(Value)>;
    
    explicit Continuation(ResumeFunc resume_func, std::string delimiter_id = "")
        : resume_func_(std::move(resume_func))
        , delimiter_id_(std::move(delimiter_id))
        , is_consumed_(false) {}
    
    // Resume execution with a value
    // This is a one-shot continuation - can only be resumed once
    Value resume(Value value) {
        if (is_consumed_) {
            throw std::runtime_error("Continuation already consumed");
        }
        if (!resume_func_) {
            throw std::runtime_error("Invalid continuation");
        }
        is_consumed_ = true;
        return resume_func_(std::move(value));
    }
    
    // Check if continuation is still valid (not consumed)
    bool is_valid() const {
        return !is_consumed_ && resume_func_ != nullptr;
    }
    
    // Get the delimiter ID this continuation is associated with
    const std::string& delimiter_id() const {
        return delimiter_id_;
    }
    
    std::string to_string() const {
        if (is_consumed_) {
            return "<consumed-continuation>";
        }
        if (delimiter_id_.empty()) {
            return "<continuation>";
        }
        return std::format("<continuation:{}>", delimiter_id_);
    }
    
private:
    ResumeFunc resume_func_;
    std::string delimiter_id_;
    bool is_consumed_;
};

// DelimitedContinuation - Manages delimited continuation capture
// This is the runtime support for primitive_suspend
class DelimitedContinuation {
public:
    struct Frame {
        std::string delimiter_id;
        std::function<Value(Value)> continuation;
        std::any context;  // Additional context data
        
        Frame(std::string id, std::function<Value(Value)> cont, std::any ctx = {})
            : delimiter_id(std::move(id))
            , continuation(std::move(cont))
            , context(std::move(ctx)) {}
    };
    
    // Get the global continuation stack
    static std::stack<Frame>& stack() {
        static thread_local std::stack<Frame> continuation_stack;
        return continuation_stack;
    }
    
    // Push a delimiter onto the stack
    static void push_delimiter(const std::string& delimiter_id, 
                               std::function<Value(Value)> continuation,
                               std::any context = {}) {
        stack().emplace(delimiter_id, std::move(continuation), std::move(context));
    }
    
    // Pop a delimiter from the stack
    static void pop_delimiter() {
        if (!stack().empty()) {
            stack().pop();
        }
    }
    
    // Find the nearest delimiter with the given ID
    static std::optional<Frame> find_delimiter(const std::string& delimiter_id) {
        // We need to search the stack without modifying it
        // Create a temporary stack to search
        std::stack<Frame> temp_stack;
        std::optional<Frame> found;
        
        while (!stack().empty()) {
            Frame frame = std::move(stack().top());
            stack().pop();
            
            if (frame.delimiter_id == delimiter_id && !found.has_value()) {
                found = frame;
            }
            
            temp_stack.push(std::move(frame));
        }
        
        // Restore the stack
        while (!temp_stack.empty()) {
            stack().push(std::move(temp_stack.top()));
            temp_stack.pop();
        }
        
        return found;
    }
    
    // Capture continuation up to the nearest delimiter
    static std::shared_ptr<Continuation> capture(const std::string& delimiter_id) {
        // Build the continuation by composing all frames up to the delimiter
        std::vector<Frame> captured_frames;
        
        // Pop frames until we find the delimiter
        while (!stack().empty()) {
            Frame frame = std::move(stack().top());
            stack().pop();
            
            if (frame.delimiter_id == delimiter_id) {
                // Found the delimiter - create the continuation
                auto resume_func = [captured_frames = std::move(captured_frames)](Value v) mutable -> Value {
                    // Restore the frames in reverse order
                    for (auto it = captured_frames.rbegin(); it != captured_frames.rend(); ++it) {
                        DelimitedContinuation::push_delimiter(it->delimiter_id, it->continuation, it->context);
                    }
                    return v;
                };
                
                return std::make_shared<Continuation>(resume_func, delimiter_id);
            }
            
            captured_frames.push_back(std::move(frame));
        }
        
        // Delimiter not found - restore the stack and throw
        for (auto it = captured_frames.rbegin(); it != captured_frames.rend(); ++it) {
            stack().push(std::move(*it));
        }
        
        throw std::runtime_error("Delimiter not found: " + delimiter_id);
    }
    
    // Clear the continuation stack (for testing)
    static void clear() {
        while (!stack().empty()) {
            stack().pop();
        }
    }
    
    // Get the current stack depth
    static size_t depth() {
        return stack().size();
    }
};

// primitive_suspend - THE SINGLE KERNEL CONTROL FLOW PRIMITIVE
// This is the foundation for all control flow: exceptions, async/await, generators, effects
// 
// Usage:
//   primitive_suspend(delimiter_id, callback)
//   
// The callback receives a Continuation object that can be used to resume execution
// The continuation captures the execution context up to the nearest delimiter with the given ID
//
// This primitive enables:
// - Exceptions: throw performs suspend, catch handler discards continuation
// - Async/await: await performs suspend, scheduler resumes later
// - Generators: yield performs suspend, iterator resumes multiple times
// - Algebraic effects: perform suspends, handler decides when/if to resume
inline Value primitive_suspend(const std::string& delimiter_id, 
                               std::function<Value(std::shared_ptr<Continuation>)> callback) {
    // Capture the continuation up to the delimiter
    auto continuation = DelimitedContinuation::capture(delimiter_id);
    
    // Invoke the callback with the captured continuation
    // The callback decides what to do with the continuation:
    // - Resume it immediately (generators)
    // - Resume it later (async)
    // - Discard it (exceptions)
    // - Clone it (backtracking)
    return callback(continuation);
}

// meta_set - AI & METADATA PRIMITIVE
// Attaches hidden metadata to any object or AST node without changing its value
// 
// Usage:
//   meta_set(obj, key, value) -> obj
//   
// The metadata is stored separately from the object and doesn't affect:
// - Object equality comparisons
// - Object string representation
// - Object behavior
//
// This primitive enables:
// - Code provenance tracking (Origin.Human, Origin.Agent, Origin.Verified)
// - Documentation attachment to AST nodes
// - Type annotations for dynamic typing
// - Debug information storage
// - AI conversation history linking
inline Value meta_set(const Value& obj, const std::string& key, const Value& metadata_value) {
    // Get the pointer to the underlying object
    const void* obj_ptr = obj.get_ptr();
    
    // Store the metadata in the global metadata store
    // We store the Value directly as std::any
    MetadataStore::instance().set_metadata(obj_ptr, key, metadata_value);
    
    // Return the original object unchanged
    return obj;
}

// meta_get - AI & METADATA PRIMITIVE
// Retrieves hidden metadata from any object or AST node
// 
// Usage:
//   meta_get(obj, key) -> value | nil
//   
// Returns the metadata value if it exists, or nil if not found
//
// This primitive enables:
// - Querying code provenance (checking if code is Origin.Human)
// - Retrieving documentation from AST nodes
// - Accessing type information
// - Reading debug information
// - Accessing AI conversation history
inline Value meta_get(const Value& obj, const std::string& key) {
    // Get the pointer to the underlying object
    const void* obj_ptr = obj.get_ptr();
    
    // Retrieve the metadata from the global metadata store
    auto metadata_opt = MetadataStore::instance().get_metadata(obj_ptr, key);
    
    if (metadata_opt.has_value()) {
        // Extract the Value from std::any
        try {
            return std::any_cast<Value>(metadata_opt.value());
        } catch (const std::bad_any_cast&) {
            // If the cast fails, return nil
            return Value(Empty::instance());
        }
    }
    
    // Return nil if metadata not found
    return Value(Empty::instance());
}

// meta_has - Helper function to check if metadata exists
// Not a kernel primitive, but useful for testing and library code
inline bool meta_has(const Value& obj, const std::string& key) {
    const void* obj_ptr = obj.get_ptr();
    return MetadataStore::instance().has_metadata(obj_ptr, key);
}

// ============================================================================
// INTEROP PRIMITIVES (The Bridge)
// ============================================================================

// NativeHandle - Represents a handle to a loaded native library
class NativeHandle {
public:
    explicit NativeHandle(void* handle, std::string path)
        : handle_(handle), path_(std::move(path)) {}
    
    void* handle() const { return handle_; }
    const std::string& path() const { return path_; }
    
    std::string to_string() const {
        return std::format("<native-handle:{}>", path_);
    }
    
private:
    void* handle_;
    std::string path_;
};

// NativeFunction - Represents a native function pointer with signature
class NativeFunction {
public:
    using FunctionPtr = void*;
    
    explicit NativeFunction(FunctionPtr ptr, std::string signature, std::string name = "")
        : ptr_(ptr), signature_(std::move(signature)), name_(std::move(name)) {}
    
    FunctionPtr ptr() const { return ptr_; }
    const std::string& signature() const { return signature_; }
    const std::string& name() const { return name_; }
    
    std::string to_string() const {
        if (!name_.empty()) {
            return std::format("<native-function:{}:{}>", name_, signature_);
        }
        return std::format("<native-function:{}>", signature_);
    }
    
private:
    FunctionPtr ptr_;
    std::string signature_;
    std::string name_;
};

// native_load - INTEROP PRIMITIVE
// Dynamically loads a shared library (.dll, .so, .dylib)
// 
// Usage:
//   native_load(path) -> NativeHandle
//   
// The path should be a string pointing to the shared library file
// Returns a handle that can be used to look up functions in the library
//
// This primitive enables:
// - Loading C libraries at runtime
// - FFI (Foreign Function Interface) support
// - Platform-specific extensions
// - Plugin systems
//
// Platform-specific behavior:
// - Windows: Loads .dll files using LoadLibrary
// - Linux/Unix: Loads .so files using dlopen
// - macOS: Loads .dylib files using dlopen
inline Value native_load(const std::string& path) {
    // Platform-specific library loading
    void* handle = nullptr;
    
#ifdef _WIN32
    // Windows: Use LoadLibrary
    handle = LoadLibraryA(path.c_str());
    if (!handle) {
        throw std::runtime_error(std::format("Failed to load library: {}", path));
    }
#else
    // Unix/Linux/macOS: Use dlopen
    handle = dlopen(path.c_str(), RTLD_LAZY | RTLD_LOCAL);
    if (!handle) {
        const char* error = dlerror();
        throw std::runtime_error(std::format("Failed to load library: {} - {}", 
            path, error ? error : "unknown error"));
    }
#endif
    
    return Value(std::make_shared<NativeHandle>(handle, path));
}

// native_call - INTEROP PRIMITIVE
// Calls a function in the host environment (C, JVM, JS)
// 
// Usage:
//   native_call(function_name, args...) -> Value
//   native_call(native_handle, function_name, args...) -> Value
//   
// This is a simplified implementation that supports:
// - Calling built-in native functions by name
// - Calling functions from loaded libraries via handle
//
// The signature is inferred from the arguments
// Returns the result as a Meld Value
//
// This primitive enables:
// - FFI (Foreign Function Interface)
// - Calling C functions from Meld
// - Platform-specific operations
// - Integration with existing libraries
//
// Example:
//   val handle = native_load("libmath.so")
//   val result = native_call(handle, "sqrt", 16.0)
//
// Built-in functions (no handle required):
//   val result = native_call("logical_shift_right", value, shift)
inline Value native_call(const std::string& function_name, const std::vector<Value>& args) {
    // This is a simplified implementation that supports built-in native functions
    // A full implementation would use libffi or similar to handle arbitrary C function calls
    
    // Built-in native functions for common operations
    if (function_name == "logical_shift_right") {
        // Unsigned right shift for implementing unsigned integer types
        if (args.size() != 2) {
            throw std::runtime_error("logical_shift_right requires 2 arguments");
        }
        
        auto value_result = args[0].try_as<Integer>();
        auto shift_result = args[1].try_as<Integer>();
        
        if (!value_result.has_value()) {
            throw std::runtime_error("logical_shift_right: first argument must be Integer");
        }
        if (!shift_result.has_value()) {
            throw std::runtime_error("logical_shift_right: second argument must be Integer");
        }
        
        uint64_t unsigned_value = static_cast<uint64_t>(value_result.value()->value());
        int64_t shift_amount = shift_result.value()->value();
        uint64_t result = unsigned_value >> shift_amount;
        
        return Value(std::make_shared<Integer>(static_cast<int64_t>(result)));
    }
    
    // Add more built-in functions as needed
    if (function_name == "bitwise_and") {
        if (args.size() != 2) {
            throw std::runtime_error("bitwise_and requires 2 arguments");
        }
        
        auto a_result = args[0].try_as<Integer>();
        auto b_result = args[1].try_as<Integer>();
        
        if (!a_result.has_value() || !b_result.has_value()) {
            throw std::runtime_error("bitwise_and: arguments must be Integers");
        }
        
        int64_t result = a_result.value()->value() & b_result.value()->value();
        return Value(std::make_shared<Integer>(result));
    }
    
    if (function_name == "bitwise_or") {
        if (args.size() != 2) {
            throw std::runtime_error("bitwise_or requires 2 arguments");
        }
        
        auto a_result = args[0].try_as<Integer>();
        auto b_result = args[1].try_as<Integer>();
        
        if (!a_result.has_value() || !b_result.has_value()) {
            throw std::runtime_error("bitwise_or: arguments must be Integers");
        }
        
        int64_t result = a_result.value()->value() | b_result.value()->value();
        return Value(std::make_shared<Integer>(result));
    }
    
    if (function_name == "bitwise_xor") {
        if (args.size() != 2) {
            throw std::runtime_error("bitwise_xor requires 2 arguments");
        }
        
        auto a_result = args[0].try_as<Integer>();
        auto b_result = args[1].try_as<Integer>();
        
        if (!a_result.has_value() || !b_result.has_value()) {
            throw std::runtime_error("bitwise_xor: arguments must be Integers");
        }
        
        int64_t result = a_result.value()->value() ^ b_result.value()->value();
        return Value(std::make_shared<Integer>(result));
    }
    
    // ─── Random ─────────────────────────────────────────────────────
    if (function_name == "random_int") {
        static std::mt19937_64 rng(std::random_device{}());
        return Value(std::make_shared<Integer>(static_cast<int64_t>(rng())));
    }
    if (function_name == "random_float") {
        static std::mt19937_64 rng(std::random_device{}());
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        return Value(std::make_shared<Float>(dist(rng)));
    }

    // ─── Filesystem ─────────────────────────────────────────────────
    if (function_name == "fs_read") {
        if (args.empty()) throw std::runtime_error("fs_read requires path argument");
        std::string path = args[0].is<String>() ? args[0].as<String>()->value() : args[0].to_string();
        std::ifstream f(path);
        if (!f.is_open()) throw std::runtime_error("fs_read: cannot open " + path);
        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        return Value::from_string(content);
    }
    if (function_name == "fs_write") {
        if (args.size() < 2) throw std::runtime_error("fs_write requires path and content");
        std::string path = args[0].is<String>() ? args[0].as<String>()->value() : args[0].to_string();
        std::string content = args[1].is<String>() ? args[1].as<String>()->value() : args[1].to_string();
        std::ofstream f(path);
        if (!f.is_open()) throw std::runtime_error("fs_write: cannot open " + path);
        f << content;
        return Value{};
    }
    if (function_name == "fs_exists") {
        if (args.empty()) throw std::runtime_error("fs_exists requires path argument");
        std::string path = args[0].is<String>() ? args[0].as<String>()->value() : args[0].to_string();
        return Value::from_bool(std::filesystem::exists(path));
    }

    // ─── Time ───────────────────────────────────────────────────────
    if (function_name == "timer_now_ms") {
        auto now = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        return Value(std::make_shared<Integer>(static_cast<int64_t>(ms)));
    }

    // ─── String utilities ───────────────────────────────────────────
    if (function_name == "char_from_code") {
        if (args.empty()) return Value::from_string("");
        int64_t code = args[0].is<Integer>() ? args[0].as<Integer>()->value() : 0;
        return Value::from_string(std::string(1, static_cast<char>(code)));
    }

    throw std::runtime_error(std::format("Unknown native function: {}", function_name));
}

// Overload for calling functions from a loaded library
inline Value native_call(const Value& handle, const std::string& function_name, const std::vector<Value>& args) {
    auto handle_result = handle.try_as<NativeHandle>();
    if (!handle_result.has_value()) {
        throw std::runtime_error("native_call: first argument must be NativeHandle");
    }
    
    void* lib_handle = handle_result.value()->handle();
    
    // Platform-specific function lookup
    void* func_ptr = nullptr;
    
#ifdef _WIN32
    // Windows: Use GetProcAddress
    func_ptr = GetProcAddress(static_cast<HMODULE>(lib_handle), function_name.c_str());
    if (!func_ptr) {
        throw std::runtime_error(std::format("Function not found: {}", function_name));
    }
#else
    // Unix/Linux/macOS: Use dlsym
    func_ptr = dlsym(lib_handle, function_name.c_str());
    if (!func_ptr) {
        const char* error = dlerror();
        throw std::runtime_error(std::format("Function not found: {} - {}", 
            function_name, error ? error : "unknown error"));
    }
#endif
    
    // For now, we return a NativeFunction object that wraps the pointer
    // A full implementation would use libffi to actually call the function
    // with the provided arguments and return the result
    return Value(std::make_shared<NativeFunction>(func_ptr, "unknown", function_name));
}

// Convenience factory functions
inline Value make_int(int64_t v) { return Value(std::make_shared<Integer>(v)); }
inline Value make_float(double v) { return Value(std::make_shared<Float>(v)); }
inline Value make_bool(bool v) { return Value(Boolean::from(v)); }
inline Value make_string(const std::string& v) { return Value(std::make_shared<String>(v)); }
inline Value createSymbol(const std::string& v) { return Value(std::make_shared<Symbol>(v)); }

} // namespace meld::kernel
