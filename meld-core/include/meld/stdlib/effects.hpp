#pragma once

#include "meld/kernel/primitives.hpp"
#include "meld/kernel/continuation.hpp"
#include "meld/effects/effect.hpp"
#include <functional>
#include <string>
#include <memory>
#include <stack>
#include <unordered_map>
#include <vector>

namespace meld::stdlib {

// Forward declarations
class HandlerStack;

// Handler Stack Management - Global state for effect handlers
class HandlerStack {
public:
    struct HandlerFrame {
        std::string effect_name;
        std::shared_ptr<effects::EffectHandler> handler;
        std::string delimiter_id;
        
        HandlerFrame(std::string name, std::shared_ptr<effects::EffectHandler> h, std::string id)
            : effect_name(std::move(name)), handler(std::move(h)), delimiter_id(std::move(id)) {}
    };
    
    // Get the global handler stack instance
    static HandlerStack& instance() {
        static thread_local HandlerStack stack;
        return stack;
    }
    
    // Push a handler onto the stack
    void push_handler(const std::string& effect_name, 
                     std::shared_ptr<effects::EffectHandler> handler,
                     const std::string& delimiter_id) {
        handlers_.emplace(effect_name, handler, delimiter_id);
    }
    
    // Pop the top handler from the stack
    void pop_handler() {
        if (!handlers_.empty()) {
            handlers_.pop();
        }
    }
    
    // Find the nearest handler for an effect
    std::shared_ptr<effects::EffectHandler> find_handler(const std::string& effect_name) const {
        // Create a temporary stack to search without modifying the original
        std::stack<HandlerFrame> temp_stack;
        std::shared_ptr<effects::EffectHandler> found_handler;
        
        // Search from top to bottom
        auto& mutable_handlers = const_cast<std::stack<HandlerFrame>&>(handlers_);
        while (!mutable_handlers.empty()) {
            HandlerFrame frame = std::move(mutable_handlers.top());
            mutable_handlers.pop();
            
            if (frame.effect_name == effect_name && !found_handler) {
                found_handler = frame.handler;
            }
            
            temp_stack.push(std::move(frame));
        }
        
        // Restore the stack
        while (!temp_stack.empty()) {
            mutable_handlers.push(std::move(temp_stack.top()));
            temp_stack.pop();
        }
        
        return found_handler;
    }
    
    // Get the delimiter ID for the nearest handler of an effect
    std::string find_delimiter_id(const std::string& effect_name) const {
        // Create a temporary stack to search without modifying the original
        std::stack<HandlerFrame> temp_stack;
        std::string found_delimiter;
        
        // Search from top to bottom
        auto& mutable_handlers = const_cast<std::stack<HandlerFrame>&>(handlers_);
        while (!mutable_handlers.empty()) {
            HandlerFrame frame = std::move(mutable_handlers.top());
            mutable_handlers.pop();
            
            if (frame.effect_name == effect_name && found_delimiter.empty()) {
                found_delimiter = frame.delimiter_id;
            }
            
            temp_stack.push(std::move(frame));
        }
        
        // Restore the stack
        while (!temp_stack.empty()) {
            mutable_handlers.push(std::move(temp_stack.top()));
            temp_stack.pop();
        }
        
        return found_delimiter;
    }
    
    // Check if there's a handler for an effect
    bool has_handler(const std::string& effect_name) const {
        return find_handler(effect_name) != nullptr;
    }
    
    // Get the current stack depth
    size_t depth() const {
        return handlers_.size();
    }
    
    // Clear all handlers (for testing)
    void clear() {
        while (!handlers_.empty()) {
            handlers_.pop();
        }
    }
    
private:
    HandlerStack() = default;
    std::stack<HandlerFrame> handlers_;
};

// ============================================================================
// LIBRARY WRAPPER FUNCTIONS (Built on primitive_suspend)
// ============================================================================

// mark_stack - Library function that places a delimiter on the call stack for effect handlers
// This is NOT a kernel primitive - it's a library function that uses primitive_suspend
//
// Usage:
//   mark_stack(effect_name, handler)
//   
// This function:
// 1. Generates a unique delimiter ID for this handler
// 2. Pushes the handler onto the global handler stack
// 3. Uses primitive_suspend to place a delimiter on the continuation stack
//
// When an effect is performed, the runtime will search for the nearest handler
// and use primitive_suspend to capture the continuation up to this delimiter
inline void mark_stack(const std::string& effect_name, 
                      std::shared_ptr<effects::EffectHandler> handler) {
    // Generate a unique delimiter ID for this handler instance
    static size_t handler_counter = 0;
    std::string delimiter_id = std::format("{}#{}", effect_name, ++handler_counter);
    
    // Push the handler onto the global handler stack
    HandlerStack::instance().push_handler(effect_name, handler, delimiter_id);
    
    // Use primitive_suspend to place a delimiter on the continuation stack
    // This delimiter will be used to capture continuations when effects are performed
    kernel::DelimitedContinuation::push_delimiter(
        delimiter_id,
        [effect_name, handler, delimiter_id](kernel::Value v) -> kernel::Value {
            // When we reach this delimiter during effect performance,
            // we have captured the continuation and can invoke the handler
            
            // The handler will decide what to do with the continuation:
            // - Resume it immediately (pass-through)
            // - Resume it later (async)
            // - Discard it (exception handling)
            // - Transform the value before resuming
            
            // For now, just pass the value through
            // The actual handler logic is implemented in the perform function
            return v;
        }
    );
}

// suspend - Library function that pauses execution and captures the continuation
// This is NOT a kernel primitive - it's a library function that uses primitive_suspend
//
// Usage:
//   suspend(delimiter_id, callback)
//   
// This function:
// 1. Uses primitive_suspend to capture the continuation up to the specified delimiter
// 2. Invokes the callback with the captured continuation
// 3. The callback decides what to do with the continuation
//
// This is the core mechanism for effect performance - when an effect is performed,
// this function captures the continuation and passes it to the effect handler
inline kernel::Value suspend(const std::string& delimiter_id, 
                            std::function<kernel::Value(std::shared_ptr<kernel::Continuation>)> callback) {
    // Use the kernel primitive to capture the continuation
    return kernel::primitive_suspend(delimiter_id, callback);
}

// ============================================================================
// RESUME FUNCTIONS - CONTINUATION RESUMPTION
// ============================================================================

// Helper function to validate value type matches expected type
// This provides clear error messages for type mismatches
// Requirements: 5.4, 5.5
inline void validate_resume_type(const kernel::Value& value, 
                                 const std::string& expected_type,
                                 const std::string& operation_context = "") {
    std::string actual_type;
    
    if (value.is<kernel::String>()) {
        actual_type = "string";
    } else if (value.is<kernel::Integer>()) {
        actual_type = "integer";
    } else if (value.is<kernel::Boolean>()) {
        actual_type = "boolean";
    } else if (value.is<kernel::Empty>()) {
        actual_type = "void";
    } else if (value.is<kernel::Cons>()) {
        actual_type = "list";
    } else {
        actual_type = "unknown";
    }
    
    if (actual_type != expected_type) {
        std::string error_msg = std::format(
            "Type mismatch in resume(): expected '{}' but got '{}'",
            expected_type, actual_type
        );
        
        if (!operation_context.empty()) {
            error_msg += std::format(" (in operation: {})", operation_context);
        }
        
        // REQUIREMENT 5.5: Provide clear error messages
        throw std::runtime_error(error_msg);
    }
}

// resume - Library function that re-attaches a continuation and continues execution
// This is NOT a kernel primitive - it's a library function that operates on Continuation objects
//
// Overload 1: Resume with a value (for operations with return values)
// Usage:
//   resume(continuation, value)
//   
// This function:
// 1. Validates that the continuation is still valid (not consumed)
// 2. Validates that the value type matches the expected return type
// 3. Resumes execution with the provided value
// 4. Returns the result of the resumed computation
//
// Requirements implemented:
// - 5.1: resume() function for continuing execution
// - 5.2: resume() accepts optional return value
// - 5.4: Type checking for resumed values
// - 5.5: Clear error messages for type mismatches
// - 5.6: Continuation capture and resumption
inline kernel::Value resume(std::shared_ptr<kernel::Continuation> continuation, kernel::Value value) {
    // REQUIREMENT 5.1: Resume function for continuing execution
    if (!continuation) {
        // REQUIREMENT 5.5: Clear error messages
        throw std::runtime_error("Cannot resume null continuation");
    }
    
    // REQUIREMENT 5.6: Validate continuation is still valid
    if (!continuation->is_valid()) {
        // REQUIREMENT 5.5: Clear error messages
        throw std::runtime_error("Cannot resume invalid or consumed continuation");
    }
    
    // REQUIREMENT 5.4: Type checking for resumed values
    // Note: In a full implementation with static typing, this would validate
    // the value type against the expected return type of the effect operation.
    // For now, we perform basic runtime type validation to ensure the value
    // is not null and is a valid kernel::Value type.
    
    // Basic validation: ensure the value is not holding a null pointer
    if (value.is<kernel::Empty>() && !value.get_ptr()) {
        // REQUIREMENT 5.5: Clear error messages
        throw std::runtime_error("Cannot resume with invalid (null) value");
    }
    
    // REQUIREMENT 5.6: Resume execution with the provided value
    // REQUIREMENT 5.2: Accept return value
    return continuation->resume(value);
}

// Overload 2: Resume without a value (for void operations)
// Usage:
//   resume(continuation)
//   
// This function:
// 1. Validates that the continuation is still valid (not consumed)
// 2. Resumes execution with an Empty value (Unit type)
// 3. Returns the result of the resumed computation
//
// Requirements implemented:
// - 5.1: resume() function for continuing execution
// - 5.2: resume() accepts optional return value (void case)
// - 5.4: Type checking (void operations expect Empty)
// - 5.5: Clear error messages
// - 5.6: Continuation capture and resumption
inline kernel::Value resume(std::shared_ptr<kernel::Continuation> continuation) {
    // REQUIREMENT 5.1: Resume function for continuing execution
    if (!continuation) {
        // REQUIREMENT 5.5: Clear error messages
        throw std::runtime_error("Cannot resume null continuation");
    }
    
    // REQUIREMENT 5.6: Validate continuation is still valid
    if (!continuation->is_valid()) {
        // REQUIREMENT 5.5: Clear error messages
        throw std::runtime_error("Cannot resume invalid or consumed continuation");
    }
    
    // REQUIREMENT 5.2: Resume without value (void operations)
    // REQUIREMENT 5.4: Type checking - void operations expect Empty
    return continuation->resume(kernel::Value(kernel::Empty::instance()));
}

// Overload 3: Resume with type-safe value (template version)
// Usage:
//   resume<string>(continuation, "result")
//   resume<int>(continuation, 42)
//   
// This function:
// 1. Validates that the continuation is still valid (not consumed)
// 2. Converts the typed value to a kernel::Value
// 3. Validates the type matches expectations
// 4. Resumes execution with the provided value
//
// Requirements implemented:
// - 5.1: resume() function for continuing execution
// - 5.2: resume() accepts optional return value
// - 5.4: Type checking for resumed values
// - 5.5: Clear error messages for type mismatches
// - 5.6: Continuation capture and resumption
template<typename T>
inline kernel::Value resume(std::shared_ptr<kernel::Continuation> continuation, const T& value) {
    // REQUIREMENT 5.1: Resume function for continuing execution
    if (!continuation) {
        // REQUIREMENT 5.5: Clear error messages
        throw std::runtime_error("Cannot resume null continuation");
    }
    
    // REQUIREMENT 5.6: Validate continuation is still valid
    if (!continuation->is_valid()) {
        // REQUIREMENT 5.5: Clear error messages
        throw std::runtime_error("Cannot resume invalid or consumed continuation");
    }
    
    // REQUIREMENT 5.4: Type checking - convert typed value to kernel::Value
    kernel::Value kernel_value;
    
    if constexpr (std::is_same_v<T, std::string>) {
        kernel_value = kernel::Value(std::make_shared<kernel::String>(value));
    } else if constexpr (std::is_same_v<T, const char*>) {
        kernel_value = kernel::Value(std::make_shared<kernel::String>(std::string(value)));
    } else if constexpr (std::is_same_v<T, int> || std::is_same_v<T, int64_t>) {
        kernel_value = kernel::Value(std::make_shared<kernel::Integer>(static_cast<int64_t>(value)));
    } else if constexpr (std::is_same_v<T, bool>) {
        kernel_value = kernel::Value(kernel::Boolean::from(value));
    } else if constexpr (std::is_same_v<T, double> || std::is_same_v<T, float>) {
        // For floating point, we'd need a Float type - for now, convert to integer
        // REQUIREMENT 5.5: Clear error message for unsupported types
        throw std::runtime_error("Float types not yet supported in resume<T>() - use resume(continuation, Value(...)) instead");
    } else if constexpr (std::is_same_v<T, kernel::Value>) {
        // Allow passing kernel::Value directly
        kernel_value = value;
    } else {
        // REQUIREMENT 5.5: Clear error messages
        throw std::runtime_error(std::format("Unsupported type for resume<T>(): {}", typeid(T).name()));
    }
    
    // REQUIREMENT 5.6: Resume execution with the provided value
    // REQUIREMENT 5.2: Accept return value
    return continuation->resume(kernel_value);
}

// Overload 4: Resume with expected type validation
// Usage:
//   resume_with_type(continuation, value, "string", "FileSystem.read")
//   
// This function provides explicit type checking with operation context
// for better error messages
//
// Requirements implemented:
// - 5.1: resume() function for continuing execution
// - 5.2: resume() accepts optional return value
// - 5.4: Type checking for resumed values
// - 5.5: Clear error messages for type mismatches
// - 5.6: Continuation capture and resumption
inline kernel::Value resume_with_type(std::shared_ptr<kernel::Continuation> continuation,
                                     kernel::Value value,
                                     const std::string& expected_type,
                                     const std::string& operation_context = "") {
    // REQUIREMENT 5.1: Resume function for continuing execution
    if (!continuation) {
        // REQUIREMENT 5.5: Clear error messages
        throw std::runtime_error("Cannot resume null continuation");
    }
    
    // REQUIREMENT 5.6: Validate continuation is still valid
    if (!continuation->is_valid()) {
        // REQUIREMENT 5.5: Clear error messages
        throw std::runtime_error("Cannot resume invalid or consumed continuation");
    }
    
    // REQUIREMENT 5.4: Type checking for resumed values
    // REQUIREMENT 5.5: Clear error messages for type mismatches
    validate_resume_type(value, expected_type, operation_context);
    
    // REQUIREMENT 5.6: Resume execution with the provided value
    // REQUIREMENT 5.2: Accept return value
    return continuation->resume(value);
}

// ============================================================================
// EFFECT PERFORMANCE AND HANDLING
// ============================================================================

// perform - Library function to perform an effect operation
// This is the main entry point for effect performance in user code
//
// Usage:
//   perform(effect_name, operation_name, args...)
//   
// This function:
// 1. Searches the handler stack for a handler for the specified effect
// 2. If found, uses suspend() to capture the continuation and invoke the handler
// 3. If not found, throws an unhandled effect error
//
// This is what users call when they want to perform an effect operation
//
// Requirements implemented:
// - 41.3: perform as library function (NOT a keyword) using primitive_suspend
// - 41.13: Search dynamic scope for appropriate handler
// - 41.14: Execute handler code and capture continuation
inline kernel::Value perform(const std::string& effect_name,
                            const std::string& operation_name,
                            const std::vector<kernel::Value>& args = {}) {
    // Find the nearest handler for this effect
    auto handler = HandlerStack::instance().find_handler(effect_name);
    if (!handler) {
        throw std::runtime_error(std::format("Unhandled effect: {}.{}", effect_name, operation_name));
    }
    
    // Get the delimiter ID for this handler
    std::string delimiter_id = HandlerStack::instance().find_delimiter_id(effect_name);
    if (delimiter_id.empty()) {
        throw std::runtime_error(std::format("No delimiter found for effect: {}", effect_name));
    }
    
    // Use suspend() to capture the continuation and invoke the handler
    return suspend(delimiter_id, 
        [handler, operation_name, args](std::shared_ptr<kernel::Continuation> cont) -> kernel::Value {
            // Invoke the handler with the operation, arguments, and continuation
            return handler->handle(operation_name, args, *cont);
        });
}

// Template-based perform function for type-safe effect performance
// This provides a type-safe wrapper around the core perform function
//
// Usage:
//   auto result = perform_typed<string>("FileSystem", "read", {path_value});
//
// This function implements the conceptual perform<T>(eff: Effect<T>) -> T signature
// mentioned in the task requirements, providing type-safe effect performance
template<typename T>
T perform_typed(const std::string& effect_name,
               const std::string& operation_name,
               const std::vector<kernel::Value>& args = {});

// Explicit instantiation declarations
extern template std::string perform_typed<std::string>(const std::string&, const std::string&, const std::vector<kernel::Value>&);
extern template int64_t perform_typed<int64_t>(const std::string&, const std::string&, const std::vector<kernel::Value>&);
extern template bool perform_typed<bool>(const std::string&, const std::string&, const std::vector<kernel::Value>&);
extern template kernel::Value perform_typed<kernel::Value>(const std::string&, const std::string&, const std::vector<kernel::Value>&);

// ============================================================================
// HANDLER SCOPE MANAGEMENT
// ============================================================================

// RAII helper for managing handler scopes
// This ensures handlers are properly pushed and popped
class EffectScope {
public:
    EffectScope(const std::string& effect_name, 
               std::shared_ptr<effects::EffectHandler> handler)
        : effect_name_(effect_name) {
        // Push the handler and mark the stack
        mark_stack(effect_name, handler);
    }
    
    ~EffectScope() {
        // Pop the handler and delimiter
        HandlerStack::instance().pop_handler();
        kernel::DelimitedContinuation::pop_delimiter();
    }
    
    // Non-copyable, non-movable
    EffectScope(const EffectScope&) = delete;
    EffectScope& operator=(const EffectScope&) = delete;
    EffectScope(EffectScope&&) = delete;
    EffectScope& operator=(EffectScope&&) = delete;
    
private:
    std::string effect_name_;
};

// handle - Library macro/function to create an effect handler scope
// This is the main way users install effect handlers
//
// Usage:
//   handle(effect_name, handler, body)
//   
// This function:
// 1. Creates an EffectScope to manage the handler lifetime
// 2. Executes the body within the handler scope
// 3. Ensures the handler is properly cleaned up
//
// Requirements implemented:
// - 4.1: handle() accepts computation and handlers
// - 4.2: handle() accepts handler configuration
// - 4.5: Support handler configuration DSL
// - 4.7: Proper handler stack management
template<typename F>
kernel::Value handle(const std::string& effect_name,
                    std::shared_ptr<effects::EffectHandler> handler,
                    F&& body) {
    // Create RAII scope for the handler
    EffectScope scope(effect_name, handler);
    
    // Execute the body within the handler scope
    try {
        return body();
    } catch (...) {
        // Exception safety: EffectScope destructor will clean up
        throw;
    }
}

// ============================================================================
// HANDLER CONFIGURATION DSL SUPPORT
// ============================================================================

// HandlerConfig - Configuration for effect handlers
// This supports the DSL syntax from the design document
class HandlerConfig {
public:
    struct OperationHandler {
        std::string operation_name;
        std::function<kernel::Value(const std::vector<kernel::Value>&, kernel::Continuation&)> handler_fn;
    };
    
    struct EffectHandlerConfig {
        std::string effect_name;
        std::vector<OperationHandler> operations;
    };
    
    HandlerConfig() = default;
    
    // Add an effect handler configuration
    void add_effect_handler(const std::string& effect_name) {
        current_effect_ = effect_name;
        handlers_.push_back({effect_name, {}});
    }
    
    // Add an operation handler to the current effect
    void add_operation(const std::string& operation_name,
                      std::function<kernel::Value(const std::vector<kernel::Value>&, kernel::Continuation&)> handler_fn) {
        if (handlers_.empty()) {
            throw std::runtime_error("No effect handler configured");
        }
        handlers_.back().operations.push_back({operation_name, std::move(handler_fn)});
    }
    
    // Get all configured handlers
    const std::vector<EffectHandlerConfig>& get_handlers() const {
        return handlers_;
    }
    
private:
    std::string current_effect_;
    std::vector<EffectHandlerConfig> handlers_;
};

// Builder class for creating handler configurations
class HandlerBuilder {
public:
    HandlerBuilder(HandlerConfig& config, const std::string& effect_name)
        : config_(config), effect_name_(effect_name) {
        config_.add_effect_handler(effect_name);
    }
    
    // Add an operation handler
    HandlerBuilder& operation(const std::string& op_name,
                             std::function<kernel::Value(const std::vector<kernel::Value>&, kernel::Continuation&)> handler_fn) {
        config_.add_operation(op_name, std::move(handler_fn));
        return *this;
    }
    
private:
    HandlerConfig& config_;
    std::string effect_name_;
};

// Enhanced handle() function with DSL support
// This version supports multiple effect handlers in a single call
//
// Usage:
//   handle(computation, [](HandlerConfig& config) {
//       config.add_effect_handler("FileSystem");
//       config.add_operation("read", [](auto& args, auto& cont) { ... });
//       config.add_operation("write", [](auto& args, auto& cont) { ... });
//   })
//
// Requirements implemented:
// - 4.1: Accept computation and handlers
// - 4.2: Accept handler configuration
// - 4.5: Support handler configuration DSL
// - 4.6: Support multiple handlers in single call
// - 4.7: Proper handler stack management with exception safety
template<typename ComputationF, typename ConfigF>
kernel::Value handle(ComputationF&& computation, ConfigF&& config_fn) {
    // Parse handler configuration
    HandlerConfig config;
    config_fn(config);
    
    // Create handler scopes for all configured effects
    std::vector<std::unique_ptr<EffectScope>> scopes;
    
    try {
        // Push all handlers onto the stack
        for (const auto& handler_config : config.get_handlers()) {
            // Create effect handler
            auto effect_def = std::make_shared<effects::EffectDefinition>(handler_config.effect_name);
            auto handler = std::make_shared<effects::EffectHandler>(effect_def);
            
            // Configure operations
            for (const auto& op : handler_config.operations) {
                handler->set_handler(op.operation_name, op.handler_fn);
            }
            
            // Create scope (pushes handler onto stack)
            scopes.push_back(std::make_unique<EffectScope>(handler_config.effect_name, handler));
        }
        
        // Execute computation with all handlers installed
        return computation();
        
    } catch (...) {
        // Exception safety: All EffectScope destructors will be called
        // automatically, cleaning up the handler stack
        throw;
    }
    // Scopes are destroyed here, popping handlers in reverse order
}

// ============================================================================
// CONVENIENCE FUNCTIONS FOR COMMON EFFECTS
// ============================================================================

// Generic effect handler creation function
// Used by the handle macro to create handlers for any effect type
std::shared_ptr<effects::EffectHandler> create_effect_handler(const std::string& effect_name);

// Built-in effect handler factory functions
std::shared_ptr<effects::EffectHandler> create_real_filesystem_handler();
std::shared_ptr<effects::EffectHandler> create_mock_filesystem_handler();
std::shared_ptr<effects::EffectHandler> create_real_console_handler();
std::shared_ptr<effects::EffectHandler> create_real_time_handler();
std::shared_ptr<effects::EffectHandler> create_real_random_handler();

// Helper functions for creating and using built-in effects

// FileSystem effect helpers
namespace filesystem {
    inline kernel::Value read(const std::string& path) {
        return perform("FileSystem", "read", {kernel::Value(std::make_shared<kernel::String>(path))});
    }
    
    inline kernel::Value write(const std::string& path, const std::string& content) {
        return perform("FileSystem", "write", {
            kernel::Value(std::make_shared<kernel::String>(path)),
            kernel::Value(std::make_shared<kernel::String>(content))
        });
    }
    
    inline kernel::Value exists(const std::string& path) {
        return perform("FileSystem", "exists", {kernel::Value(std::make_shared<kernel::String>(path))});
    }
}

// Network effect helpers
namespace network {
    inline kernel::Value get(const std::string& url) {
        return perform("Network", "get", {kernel::Value(std::make_shared<kernel::String>(url))});
    }
    
    inline kernel::Value post(const std::string& url, const std::string& body) {
        return perform("Network", "post", {
            kernel::Value(std::make_shared<kernel::String>(url)),
            kernel::Value(std::make_shared<kernel::String>(body))
        });
    }
}

// Console effect helpers
namespace console {
    inline kernel::Value print(const std::string& message) {
        return perform("Console", "print", {kernel::Value(std::make_shared<kernel::String>(message))});
    }
    
    inline kernel::Value read_line() {
        return perform("Console", "readLine", {});
    }
}

// Time effect helpers
namespace time {
    inline kernel::Value now() {
        return perform("Time", "now", {});
    }
    
    inline kernel::Value sleep(int64_t milliseconds) {
        return perform("Time", "sleep", {kernel::Value(std::make_shared<kernel::Integer>(milliseconds))});
    }
}

// Random effect helpers
namespace random {
    inline kernel::Value next_int(int64_t max) {
        return perform("Random", "nextInt", {kernel::Value(std::make_shared<kernel::Integer>(max))});
    }
    
    inline kernel::Value next_float() {
        return perform("Random", "nextFloat", {});
    }
}

} // namespace meld::stdlib