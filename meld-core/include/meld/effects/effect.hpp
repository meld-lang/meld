#pragma once

#include "meld/kernel/primitives.hpp"
#include "meld/kernel/continuation.hpp"
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <variant>

namespace meld::effects {

// Forward declarations
class EffectDefinition;
class EffectHandler;
class EffectRuntime;

// Effect operation signature
struct EffectOperation {
    std::string name;
    std::vector<std::string> parameter_types;
    std::string return_type;
    
    EffectOperation(std::string n, std::vector<std::string> params, std::string ret)
        : name(std::move(n)), parameter_types(std::move(params)), return_type(std::move(ret)) {}
};

// Effect definition - defines abstract operations
class EffectDefinition {
public:
    explicit EffectDefinition(std::string name) : name_(std::move(name)) {}
    
    // Add an operation to this effect
    void add_operation(const std::string& op_name, 
                      const std::vector<std::string>& param_types,
                      const std::string& return_type) {
        operations_.emplace_back(op_name, param_types, return_type);
    }
    
    // Get effect name
    const std::string& name() const { return name_; }
    
    // Get all operations
    const std::vector<EffectOperation>& operations() const { return operations_; }
    
    // Check if operation exists
    bool has_operation(const std::string& op_name) const {
        for (const auto& op : operations_) {
            if (op.name == op_name) {
                return true;
            }
        }
        return false;
    }
    
    // Get operation by name
    const EffectOperation* get_operation(const std::string& op_name) const {
        for (const auto& op : operations_) {
            if (op.name == op_name) {
                return &op;
            }
        }
        return nullptr;
    }
    
private:
    std::string name_;
    std::vector<EffectOperation> operations_;
};

// Use the kernel Continuation type
using Continuation = kernel::Continuation;

// Enhanced continuation wrapper for effects system
// Provides additional functionality for effect handlers
class EffectContinuation {
public:
    explicit EffectContinuation(std::shared_ptr<kernel::Continuation> cont)
        : continuation_(std::move(cont)), is_resumed_(false) {}
    
    // Resume execution without a value (uses Empty)
    // This is the basic resume() method - Requirement 41.5
    kernel::Value resume() {
        if (is_resumed_) {
            throw std::runtime_error("Continuation already resumed");
        }
        if (!continuation_ || !continuation_->is_valid()) {
            throw std::runtime_error("Invalid continuation");
        }
        
        is_resumed_ = true;
        return continuation_->resume(kernel::Value(kernel::Empty::instance()));
    }
    
    // Resume execution with a specific value - Requirement 41.16
    // This allows handlers to modify return values
    kernel::Value resume(kernel::Value value) {
        if (is_resumed_) {
            throw std::runtime_error("Continuation already resumed");
        }
        if (!continuation_ || !continuation_->is_valid()) {
            throw std::runtime_error("Invalid continuation");
        }
        
        is_resumed_ = true;
        return continuation_->resume(std::move(value));
    }
    
    // Check if continuation is still valid and not resumed
    bool is_valid() const {
        return !is_resumed_ && continuation_ && continuation_->is_valid();
    }
    
    // Check if continuation has been resumed
    bool is_resumed() const {
        return is_resumed_;
    }
    
    // Get the underlying kernel continuation (for advanced use cases)
    std::shared_ptr<kernel::Continuation> get_kernel_continuation() const {
        return continuation_;
    }
    
    // Get delimiter ID
    std::string delimiter_id() const {
        return continuation_ ? continuation_->delimiter_id() : "";
    }
    
private:
    std::shared_ptr<kernel::Continuation> continuation_;
    bool is_resumed_;
};

// Effect handler - implements effect operations
class EffectHandler {
public:
    // Handler function signature - takes effect parameters and continuation
    // The handler can inspect parameters (args) and decide how to handle the continuation
    using HandlerFunc = std::function<kernel::Value(const std::vector<kernel::Value>&, Continuation&)>;
    
    // Enhanced handler function signature with EffectContinuation wrapper
    using EnhancedHandlerFunc = std::function<kernel::Value(const std::vector<kernel::Value>&, EffectContinuation&)>;
    
    explicit EffectHandler(std::shared_ptr<EffectDefinition> effect)
        : effect_(std::move(effect)) {}
    
    // Set handler for an operation (traditional signature)
    void set_handler(const std::string& op_name, HandlerFunc handler) {
        handlers_[op_name] = std::move(handler);
    }
    
    // Set enhanced handler for an operation (with EffectContinuation wrapper)
    void set_enhanced_handler(const std::string& op_name, EnhancedHandlerFunc handler) {
        // Wrap the enhanced handler to work with the traditional signature
        handlers_[op_name] = [handler](const std::vector<kernel::Value>& args, Continuation& cont) -> kernel::Value {
            // Create an EffectContinuation wrapper by sharing the continuation
            // We need to create a shared_ptr from the reference
            auto shared_cont = std::shared_ptr<Continuation>(&cont, [](Continuation*) {
                // Custom deleter that does nothing since we don't own the continuation
            });
            EffectContinuation effect_cont(shared_cont);
            return handler(args, effect_cont);
        };
    }
    
    // Handle an operation - CORE IMPLEMENTATION FOR TASK 35.6
    // This method implements the handler execution mechanism that:
    // 1. Validates the operation exists and has a handler
    // 2. Allows handlers to inspect effect parameters (args)
    // 3. Executes the handler code with the captured continuation
    // 4. Enables handlers to modify return values through resume()
    // 5. Supports both resume() and resume(value) patterns
    //
    // Requirements implemented:
    // - 41.5: Handler execution with continuation support
    // - 41.14: Execute handler code and capture continuation
    // - 41.15: Enable handlers to inspect effect parameters
    // - 41.16: Enable handlers to modify return values
    kernel::Value handle(const std::string& op_name, 
                        const std::vector<kernel::Value>& args,
                        Continuation& cont) {
        // Validate that the operation exists in the effect definition
        if (!effect_->has_operation(op_name)) {
            throw std::runtime_error("Operation '" + op_name + "' not defined in effect '" + effect_->name() + "'");
        }
        
        // Find the handler for this operation
        auto it = handlers_.find(op_name);
        if (it == handlers_.end()) {
            throw std::runtime_error("No handler for operation: " + effect_->name() + "." + op_name);
        }
        
        // Execute the handler with the effect parameters and continuation
        // The handler can:
        // 1. Inspect the effect parameters (args) - Requirement 41.15
        // 2. Call cont.resume() to continue execution - Requirement 41.5
        // 3. Call cont.resume(value) to continue with a specific return value - Requirement 41.16
        // 4. Discard the continuation (for exceptions)
        // 5. Store the continuation for later resumption (for async)
        // 6. Resume the continuation multiple times (for generators)
        try {
            return it->second(args, cont);
        } catch (const std::exception& e) {
            throw std::runtime_error("Handler execution failed for " + effect_->name() + "." + op_name + ": " + e.what());
        }
    }
    
    // Check if operation is handled
    bool has_handler(const std::string& op_name) const {
        return handlers_.find(op_name) != handlers_.end();
    }
    
    // Get effect definition
    const EffectDefinition& effect() const { return *effect_; }
    
    // Get all handler names (for debugging and introspection)
    std::vector<std::string> get_handler_names() const {
        std::vector<std::string> names;
        for (const auto& [name, _] : handlers_) {
            names.push_back(name);
        }
        return names;
    }
    
    // Check if all operations in the effect definition have handlers
    bool is_complete() const {
        for (const auto& op : effect_->operations()) {
            if (!has_handler(op.name)) {
                return false;
            }
        }
        return true;
    }
    
    // Get missing handlers (operations without implementations)
    std::vector<std::string> get_missing_handlers() const {
        std::vector<std::string> missing;
        for (const auto& op : effect_->operations()) {
            if (!has_handler(op.name)) {
                missing.push_back(op.name);
            }
        }
        return missing;
    }
    
private:
    std::shared_ptr<EffectDefinition> effect_;
    std::map<std::string, HandlerFunc> handlers_;
};

// Effect runtime - manages handler stack
class EffectRuntime {
public:
    static EffectRuntime& instance() {
        static EffectRuntime runtime;
        return runtime;
    }
    
    // Push handler onto stack
    void push_handler(std::shared_ptr<EffectHandler> handler) {
        handler_stack_.push_back(std::move(handler));
    }
    
    // Pop handler from stack
    void pop_handler() {
        if (!handler_stack_.empty()) {
            handler_stack_.pop_back();
        }
    }
    
    // TASK 35.7: Handler registration/deregistration (pushScope/popScope)
    // These functions implement the scope-based handler management required by the task
    
    // Push a new handler scope with multiple handlers
    // This allows installing multiple effect handlers at once in a single scope
    // Requirement 41.13: Search dynamic scope for appropriate handler
    // Requirement 41.17: Support nested handlers with proper precedence
    void pushScope(const std::vector<std::shared_ptr<EffectHandler>>& handlers) {
        // Create a new scope frame
        ScopeFrame frame;
        frame.handlers = handlers;
        frame.start_index = handler_stack_.size();
        
        // Add all handlers to the stack (innermost handlers take precedence)
        for (auto& handler : handlers) {
            handler_stack_.push_back(handler);
        }
        
        // Track the scope for proper cleanup
        scope_stack_.push_back(std::move(frame));
    }
    
    // Push a single handler as a new scope
    void pushScope(std::shared_ptr<EffectHandler> handler) {
        pushScope(std::vector<std::shared_ptr<EffectHandler>>{std::move(handler)});
    }
    
    // Pop the most recent handler scope
    // This removes all handlers that were added in the most recent pushScope call
    // Ensures proper nested scope cleanup (LIFO order)
    void popScope() {
        if (scope_stack_.empty()) {
            return; // No scopes to pop
        }
        
        // Get the most recent scope
        const ScopeFrame& frame = scope_stack_.back();
        
        // Remove all handlers from this scope
        // We need to remove exactly the number of handlers that were added
        size_t handlers_to_remove = frame.handlers.size();
        for (size_t i = 0; i < handlers_to_remove && !handler_stack_.empty(); ++i) {
            handler_stack_.pop_back();
        }
        
        // Remove the scope frame
        scope_stack_.pop_back();
    }
    
    // TASK 35.7: Implement handler search (innermost first)
    // This method implements the core handler search algorithm
    // Requirement 41.13: Search dynamic scope for appropriate handler
    // Requirement 41.17: Support nested handlers with inner handlers taking precedence
    std::shared_ptr<EffectHandler> find_handler(const std::string& effect_name) const {
        // Search from top (innermost/most recent) to bottom (outermost/oldest)
        // This ensures that inner handlers take precedence over outer handlers
        for (auto it = handler_stack_.rbegin(); it != handler_stack_.rend(); ++it) {
            if ((*it)->effect().name() == effect_name) {
                return *it;
            }
        }
        return nullptr; // No handler found
    }
    
    // Find handler for a specific operation
    std::shared_ptr<EffectHandler> find_handler(const std::string& effect_name, 
                                               const std::string& operation_name) const {
        // Search from top (innermost) to bottom (outermost)
        for (auto it = handler_stack_.rbegin(); it != handler_stack_.rend(); ++it) {
            if ((*it)->effect().name() == effect_name && 
                (*it)->has_handler(operation_name)) {
                return *it;
            }
        }
        return nullptr; // No handler found
    }
    
    // Check if there's a handler for an effect
    bool has_handler(const std::string& effect_name) const {
        return find_handler(effect_name) != nullptr;
    }
    
    // Check if there's a handler for a specific operation
    bool has_handler(const std::string& effect_name, const std::string& operation_name) const {
        return find_handler(effect_name, operation_name) != nullptr;
    }
    
    // Get all handlers for an effect (from innermost to outermost)
    std::vector<std::shared_ptr<EffectHandler>> get_handlers(const std::string& effect_name) const {
        std::vector<std::shared_ptr<EffectHandler>> handlers;
        for (auto it = handler_stack_.rbegin(); it != handler_stack_.rend(); ++it) {
            if ((*it)->effect().name() == effect_name) {
                handlers.push_back(*it);
            }
        }
        return handlers;
    }
    // Perform an effect operation using primitive_suspend
    // TASK 35.7: Updated to use the new handler search functionality
    // Requirement 41.13: Search dynamic scope for appropriate handler
    // Requirement 41.17: Support nested handlers with inner handlers taking precedence
    kernel::Value perform_effect(const std::string& effect_name,
                                 const std::string& operation_name,
                                 const std::vector<kernel::Value>& args) {
        // Use the new handler search method (innermost first)
        auto handler = find_handler(effect_name, operation_name);
        if (!handler) {
            throw std::runtime_error("Unhandled effect: " + effect_name + "." + operation_name);
        }
        
        // Build delimiter ID from effect and operation
        std::string delimiter_id = effect_name + "." + operation_name;
        
        // Use primitive_suspend to capture continuation
        return kernel::primitive_suspend(delimiter_id, 
            [handler, operation_name, args](std::shared_ptr<kernel::Continuation> cont) -> kernel::Value {
                // Invoke handler with captured continuation
                return handler->handle(operation_name, args, *cont);
            });
    }
    
    // Get current handler stack size
    size_t handler_stack_size() const {
        return handler_stack_.size();
    }
    
    // Get current scope depth
    size_t scope_depth() const {
        return scope_stack_.size();
    }
    
    // Clear all handlers and scopes (for testing)
    void clear_handlers() {
        handler_stack_.clear();
        scope_stack_.clear();
    }
    
    // Get debug information about the current handler stack
    struct StackInfo {
        size_t total_handlers;
        size_t total_scopes;
        std::vector<std::string> effect_names;
        std::vector<size_t> scope_sizes;
    };
    
    StackInfo get_stack_info() const {
        StackInfo info;
        info.total_handlers = handler_stack_.size();
        info.total_scopes = scope_stack_.size();
        
        // Collect effect names
        for (const auto& handler : handler_stack_) {
            info.effect_names.push_back(handler->effect().name());
        }
        
        // Collect scope sizes
        for (const auto& frame : scope_stack_) {
            info.scope_sizes.push_back(frame.handlers.size());
        }
        
        return info;
    }
    
private:
    EffectRuntime() = default;
    
    // Scope frame for tracking handler scopes
    struct ScopeFrame {
        std::vector<std::shared_ptr<EffectHandler>> handlers;
        size_t start_index; // Index in handler_stack_ where this scope starts
    };
    
    std::vector<std::shared_ptr<EffectHandler>> handler_stack_;
    std::vector<ScopeFrame> scope_stack_;
};

// RAII helper for handler scope
class HandlerScope {
public:
    explicit HandlerScope(std::shared_ptr<EffectHandler> handler) {
        EffectRuntime::instance().push_handler(std::move(handler));
    }
    
    ~HandlerScope() {
        EffectRuntime::instance().pop_handler();
    }
    
    // Non-copyable, non-movable
    HandlerScope(const HandlerScope&) = delete;
    HandlerScope& operator=(const HandlerScope&) = delete;
    HandlerScope(HandlerScope&&) = delete;
    HandlerScope& operator=(HandlerScope&&) = delete;
};

// Built-in effect definitions

// FileSystem effect
inline std::shared_ptr<EffectDefinition> create_filesystem_effect() {
    auto effect = std::make_shared<EffectDefinition>("FileSystem");
    effect->add_operation("read", {"string"}, "string");
    effect->add_operation("write", {"string", "string"}, "void");
    effect->add_operation("delete", {"string"}, "void");
    effect->add_operation("exists", {"string"}, "bool");
    return effect;
}

// Network effect
inline std::shared_ptr<EffectDefinition> create_network_effect() {
    auto effect = std::make_shared<EffectDefinition>("Network");
    effect->add_operation("get", {"string"}, "string");
    effect->add_operation("post", {"string", "string"}, "string");
    return effect;
}

// Console effect
inline std::shared_ptr<EffectDefinition> create_console_effect() {
    auto effect = std::make_shared<EffectDefinition>("Console");
    effect->add_operation("print", {"string"}, "void");
    effect->add_operation("println", {"string"}, "void");
    effect->add_operation("readLine", {}, "string");
    return effect;
}

// Random effect
inline std::shared_ptr<EffectDefinition> create_random_effect() {
    auto effect = std::make_shared<EffectDefinition>("Random");
    effect->add_operation("nextInt", {"int"}, "int");
    effect->add_operation("nextFloat", {}, "float");
    return effect;
}

// Time effect
inline std::shared_ptr<EffectDefinition> create_time_effect() {
    auto effect = std::make_shared<EffectDefinition>("Time");
    effect->add_operation("now", {}, "int");
    effect->add_operation("sleep", {"int"}, "void");
    return effect;
}

// Helper function to perform an effect
inline kernel::Value perform(const std::string& effect_name,
                            const std::string& operation_name,
                            const std::vector<kernel::Value>& args = {}) {
    return EffectRuntime::instance().perform_effect(effect_name, operation_name, args);
}

// Helper function to create and install a handler
template<typename F>
void handle(std::shared_ptr<EffectDefinition> effect, F&& body) {
    auto handler = std::make_shared<EffectHandler>(effect);
    
    // Install handler
    HandlerScope scope(handler);
    
    // Execute body
    body(*handler);
}

} // namespace meld::effects
