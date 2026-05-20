#pragma once

#include "meld/kernel/primitives.hpp"
#include "meld/effects/effect.hpp"
#include "effects.hpp"
#include <string>
#include <memory>
#include <functional>
#include <stdexcept>

namespace meld::stdlib::exceptions {

// ============================================================================
// EXCEPTION TYPES AND STRUCTURES
// Task 35.14: Support custom exception types as effect data payloads
// Requirements: 41.22, 28.11
// ============================================================================

// Base exception type for Meld exceptions
// This represents the payload data for the Exception effect
struct MeldException {
    std::string message;
    std::string type_name;
    kernel::Value payload;  // Custom exception data
    
    MeldException(std::string msg, std::string type = "Exception", kernel::Value data = kernel::Value(kernel::Empty::instance()))
        : message(std::move(msg)), type_name(std::move(type)), payload(std::move(data)) {}
};

// Custom exception types can be created by extending MeldException
struct RuntimeException : public MeldException {
    RuntimeException(std::string msg, kernel::Value data = kernel::Value(kernel::Empty::instance()))
        : MeldException(std::move(msg), "RuntimeException", std::move(data)) {}
};

struct ValidationException : public MeldException {
    ValidationException(std::string msg, kernel::Value data = kernel::Value(kernel::Empty::instance()))
        : MeldException(std::move(msg), "ValidationException", std::move(data)) {}
};

struct NetworkException : public MeldException {
    NetworkException(std::string msg, kernel::Value data = kernel::Value(kernel::Empty::instance()))
        : MeldException(std::move(msg), "NetworkException", std::move(data)) {}
};

// ============================================================================
// LIBRARY FUNCTIONS FOR EXCEPTION HANDLING
// Task 35.14: Implement throw as library function performing Exception.raise
// Requirements: 41.22, 28.8
// ============================================================================

// throw - Library function that performs Exception.raise effect
// This is NOT a keyword - it's a library function that uses the effect system
//
// Usage:
//   throw_exception("Error message")
//   throw_exception(RuntimeException("Runtime error", data))
//
// This function:
// 1. Creates an exception payload with the provided message/data
// 2. Performs the Exception.raise effect with the payload
// 3. Never returns normally (the handler discards the continuation)
//
// Requirements implemented:
// - 28.8: throw as library function performing Exception.raise
// - 41.22: Exceptions implemented as effects where throw performs Exception.raise
// - 28.11: Support custom exception types as effect data payloads
inline void throw_exception(const std::string& message) {
    // Create exception payload
    MeldException exception(message);
    
    // Convert to kernel::Value for effect system
    // For now, we'll use the message as a string value
    // In a full implementation, this would serialize the entire exception structure
    kernel::Value exception_value = kernel::Value::from_string(message);
    
    // Perform Exception.raise effect - this will never return normally
    // The exception handler will discard the continuation to unwind the stack
    perform("Exception", "raise", {exception_value});
    
    // This line should never be reached if the handler is properly installed
    throw std::runtime_error("Unhandled exception: " + message);
}

// Overload for custom exception types
inline void throw_exception(const MeldException& exception) {
    // Create a formatted message that includes type information
    std::string formatted_message = exception.type_name + ": " + exception.message;
    
    // For now, use the formatted message as the payload
    // In a full implementation, this would serialize the entire exception structure
    kernel::Value exception_value = kernel::Value::from_string(formatted_message);
    
    // Perform Exception.raise effect
    perform("Exception", "raise", {exception_value});
    
    // This line should never be reached
    throw std::runtime_error("Unhandled exception: " + formatted_message);
}

// Convenience functions for specific exception types
inline void throw_runtime_exception(const std::string& message, kernel::Value data = kernel::Value(kernel::Empty::instance())) {
    throw_exception(RuntimeException(message, std::move(data)));
}

inline void throw_validation_exception(const std::string& message, kernel::Value data = kernel::Value(kernel::Empty::instance())) {
    throw_exception(ValidationException(message, std::move(data)));
}

inline void throw_network_exception(const std::string& message, kernel::Value data = kernel::Value(kernel::Empty::instance())) {
    throw_exception(NetworkException(message, std::move(data)));
}

// ============================================================================
// EXCEPTION HANDLER CREATION
// Task 35.14: Ensure handlers discard continuations to unwind stack
// Requirements: 41.22, 28.10
// ============================================================================

// Create an exception handler that discards continuations (stack unwinding)
// This handler implements the exception semantics where thrown exceptions
// unwind the stack by discarding the continuation instead of resuming it
//
// Requirements implemented:
// - 28.10: Exception handlers discard continuations to unwind stack
// - 41.22: Exceptions as effects where handlers discard continuations
// - 41B.7: Exception handlers discard continuations to implement stack unwinding
std::shared_ptr<effects::EffectHandler> create_unwinding_exception_handler();

// Create a catching exception handler that can catch and handle exceptions
// This handler allows implementing try/catch semantics by catching exceptions
// and resuming execution with a recovery value or re-throwing
//
// Usage:
//   auto handler = create_catching_exception_handler([](const MeldException& ex) -> kernel::Value {
//       // Handle the exception and return a recovery value
//       if (ex.type_name == "ValidationException") {
//           return kernel::Value::from_string("default_value");
//       }
//       // Re-throw if not handled
//       throw_exception(ex);
//   });
using ExceptionCatchHandler = std::function<kernel::Value(const MeldException&)>;
std::shared_ptr<effects::EffectHandler> create_catching_exception_handler(ExceptionCatchHandler catch_handler);

// ============================================================================
// TRY/CATCH LIBRARY MACROS
// Task 35.14: Implement try/catch as library macros expanding to handle blocks
// Requirements: 41.22, 28.9
// ============================================================================

// try_block - Library function that implements try semantics using handle blocks
// This is NOT a keyword - it's a library function that expands to effect handling
//
// Usage:
//   auto result = try_block([&]() -> kernel::Value {
//       // Code that might throw exceptions
//       throw_exception("Something went wrong");
//       return kernel::Value::from_string("success");
//   });
//
// This function:
// 1. Installs an unwinding exception handler
// 2. Executes the provided code block
// 3. If no exception is thrown, returns the result
// 4. If an exception is thrown, the handler discards the continuation (unwinds)
//
// Requirements implemented:
// - 28.9: try/catch as library macros expanding to handle blocks
// - 41.22: Exceptions implemented using handle blocks for Exception effect
// - 41B.4: try/catch as library macros expanding to handle blocks
template<typename F>
kernel::Value try_block(F&& body) {
    // Create an unwinding exception handler
    auto exception_handler = create_unwinding_exception_handler();
    
    // Use the effect system's handle mechanism
    return handle("Exception", exception_handler, [&]() -> kernel::Value {
        return body();
    });
}

// try_catch - Library function that implements try/catch semantics
// This provides full try/catch functionality with exception handling
//
// Usage:
//   auto result = try_catch([&]() -> kernel::Value {
//       // Code that might throw
//       throw_exception("Error");
//       return kernel::Value::from_string("success");
//   }, [](const MeldException& ex) -> kernel::Value {
//       // Catch handler
//       if (ex.type_name == "ValidationException") {
//           return kernel::Value::from_string("recovered");
//       }
//       throw_exception(ex);  // Re-throw if not handled
//   });
//
// This function:
// 1. Installs a catching exception handler with the provided catch logic
// 2. Executes the try block
// 3. If no exception is thrown, returns the result
// 4. If an exception is thrown, invokes the catch handler
// 5. The catch handler can return a recovery value or re-throw
//
// Requirements implemented:
// - 28.9: try/catch as library macros expanding to handle blocks
// - 28.12: Pattern matching on exception types in catch blocks
// - 41.22: Exceptions implemented using handle blocks
template<typename TryF, typename CatchF>
kernel::Value try_catch(TryF&& try_body, CatchF&& catch_handler) {
    // Create a catching exception handler with the provided catch logic
    auto exception_handler = create_catching_exception_handler(
        [catch_handler](const MeldException& ex) -> kernel::Value {
            return catch_handler(ex);
        });
    
    // Use the effect system's handle mechanism
    return handle("Exception", exception_handler, [&]() -> kernel::Value {
        return try_body();
    });
}

// Multiple catch handlers with type-based dispatch
// This allows implementing catch blocks for different exception types
//
// Usage:
//   auto result = try_catch_typed([&]() -> kernel::Value {
//       throw_validation_exception("Invalid input");
//       return kernel::Value::from_string("success");
//   })
//   .catch_type<ValidationException>([](const ValidationException& ex) -> kernel::Value {
//       return kernel::Value::from_string("validation_error_handled");
//   })
//   .catch_type<RuntimeException>([](const RuntimeException& ex) -> kernel::Value {
//       return kernel::Value::from_string("runtime_error_handled");
//   })
//   .catch_all([](const MeldException& ex) -> kernel::Value {
//       return kernel::Value::from_string("unknown_error_handled");
//   });
//
// This provides type-safe exception handling with pattern matching
class TryCatchBuilder {
public:
    template<typename F>
    explicit TryCatchBuilder(F&& try_body) : try_body_([try_body]() { return try_body(); }) {}
    
    template<typename ExceptionType, typename F>
    TryCatchBuilder& catch_type(F&& handler) {
        catch_handlers_.emplace_back([handler](const MeldException& ex) -> std::optional<kernel::Value> {
            // Check if this is the right exception type
            if (ex.type_name == typeid(ExceptionType).name() || 
                (ex.type_name == "ValidationException" && std::is_same_v<ExceptionType, ValidationException>) ||
                (ex.type_name == "RuntimeException" && std::is_same_v<ExceptionType, RuntimeException>) ||
                (ex.type_name == "NetworkException" && std::is_same_v<ExceptionType, NetworkException>)) {
                
                // Create typed exception and call handler
                if constexpr (std::is_same_v<ExceptionType, ValidationException>) {
                    ValidationException typed_ex(ex.message, ex.payload);
                    return handler(typed_ex);
                } else if constexpr (std::is_same_v<ExceptionType, RuntimeException>) {
                    RuntimeException typed_ex(ex.message, ex.payload);
                    return handler(typed_ex);
                } else if constexpr (std::is_same_v<ExceptionType, NetworkException>) {
                    NetworkException typed_ex(ex.message, ex.payload);
                    return handler(typed_ex);
                } else {
                    // Generic MeldException
                    return handler(static_cast<const ExceptionType&>(ex));
                }
            }
            return std::nullopt;  // Not handled by this catch block
        });
        return *this;
    }
    
    template<typename F>
    TryCatchBuilder& catch_all(F&& handler) {
        catch_all_handler_ = [handler](const MeldException& ex) -> kernel::Value {
            return handler(ex);
        };
        return *this;
    }
    
    kernel::Value execute() {
        return try_catch(try_body_, [this](const MeldException& ex) -> kernel::Value {
            // Try each typed catch handler
            for (const auto& handler : catch_handlers_) {
                auto result = handler(ex);
                if (result.has_value()) {
                    return result.value();
                }
            }
            
            // Try catch-all handler
            if (catch_all_handler_) {
                return catch_all_handler_(ex);
            }
            
            // No handler matched - re-throw
            throw_exception(ex);
            return kernel::Value(kernel::Empty::instance());  // Never reached
        });
    }
    
    // Implicit conversion to execute the try-catch
    operator kernel::Value() {
        return execute();
    }
    
private:
    std::function<kernel::Value()> try_body_;
    std::vector<std::function<std::optional<kernel::Value>(const MeldException&)>> catch_handlers_;
    std::function<kernel::Value(const MeldException&)> catch_all_handler_;
};

// Factory function for type-safe try-catch
template<typename F>
TryCatchBuilder try_catch_typed(F&& try_body) {
    return TryCatchBuilder(std::forward<F>(try_body));
}

// ============================================================================
// EXCEPTION UTILITIES
// ============================================================================

// Check if an exception is of a specific type
template<typename ExceptionType>
bool is_exception_type(const MeldException& ex) {
    if constexpr (std::is_same_v<ExceptionType, ValidationException>) {
        return ex.type_name == "ValidationException";
    } else if constexpr (std::is_same_v<ExceptionType, RuntimeException>) {
        return ex.type_name == "RuntimeException";
    } else if constexpr (std::is_same_v<ExceptionType, NetworkException>) {
        return ex.type_name == "NetworkException";
    } else {
        return ex.type_name == typeid(ExceptionType).name();
    }
}

// Extract exception message
inline std::string get_exception_message(const MeldException& ex) {
    return ex.message;
}

// Extract exception type name
inline std::string get_exception_type(const MeldException& ex) {
    return ex.type_name;
}

// Extract exception payload
inline kernel::Value get_exception_payload(const MeldException& ex) {
    return ex.payload;
}

} // namespace meld::stdlib::exceptions