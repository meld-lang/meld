#pragma once

#include "primitives.hpp"
#include <functional>
#include <string>
#include <memory>

namespace meld::kernel {

// Library functions built on primitive_suspend
// These are NOT kernel primitives - they are standard library functions

// mark_stack - Places a delimiter on the call stack for effect handlers
// This is a library function that uses primitive_suspend internally
inline void mark_stack(const std::string& delimiter_id, 
                      std::function<Value(std::shared_ptr<Continuation>)> handler) {
    // Push the delimiter onto the continuation stack
    DelimitedContinuation::push_delimiter(
        delimiter_id,
        [handler, delimiter_id](Value v) -> Value {
            // When we reach this delimiter, invoke the handler
            // The handler receives a continuation that can resume execution
            auto cont = std::make_shared<Continuation>(
                [v](Value resume_val) { return resume_val; },
                delimiter_id
            );
            return handler(cont);
        }
    );
}

// suspend - Pauses execution and captures the continuation
// This is a library function that uses primitive_suspend internally
inline Value suspend(const std::string& delimiter_id, 
                    std::function<Value(std::shared_ptr<Continuation>)> callback) {
    return primitive_suspend(delimiter_id, callback);
}

// resume - Re-attaches a continuation and continues execution
// This is a library function that operates on Continuation objects
inline Value resume(std::shared_ptr<Continuation> continuation, Value value) {
    if (!continuation || !continuation->is_valid()) {
        throw std::runtime_error("Cannot resume invalid continuation");
    }
    return continuation->resume(value);
}

// Helper: Execute code within a delimited scope
// This is the foundation for handle blocks in the effects system
template<typename F>
Value with_delimiter(const std::string& delimiter_id, F&& body) {
    // Push delimiter
    DelimitedContinuation::push_delimiter(
        delimiter_id,
        [](Value v) { return v; }  // Default: just pass through
    );
    
    try {
        // Execute body
        Value result = body();
        
        // Pop delimiter
        DelimitedContinuation::pop_delimiter();
        
        return result;
    } catch (...) {
        // Ensure delimiter is popped even on exception
        DelimitedContinuation::pop_delimiter();
        throw;
    }
}

// Helper: Create an effect handler scope
// This combines mark_stack with a body of code
template<typename F>
Value handle_effect(const std::string& effect_id,
                   std::function<Value(std::shared_ptr<Continuation>)> handler,
                   F&& body) {
    // Mark the stack with the effect handler
    mark_stack(effect_id, handler);
    
    try {
        // Execute the body
        Value result = body();
        
        // Pop the delimiter
        DelimitedContinuation::pop_delimiter();
        
        return result;
    } catch (...) {
        // Ensure delimiter is popped even on exception
        DelimitedContinuation::pop_delimiter();
        throw;
    }
}

} // namespace meld::kernel
