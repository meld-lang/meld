#include "meld/stdlib/exceptions.hpp"
#include "meld/effects/builtin_effects.hpp"
#include <iostream>
#include <sstream>

namespace meld::stdlib::exceptions {

// ============================================================================
// EXCEPTION HANDLER IMPLEMENTATIONS
// Task 35.14: Ensure handlers discard continuations to unwind stack
// Requirements: 41.22, 28.10
// ============================================================================

// Create an unwinding exception handler that discards continuations
// This implements the core exception semantics where exceptions unwind the stack
// by discarding the continuation instead of resuming it
//
// Requirements implemented:
// - 28.10: Exception handlers discard continuations to unwind stack
// - 41.22: Exceptions as effects where handlers discard continuations
// - 41B.7: Exception handlers discard continuations to implement stack unwinding
std::shared_ptr<effects::EffectHandler> create_unwinding_exception_handler() {
    auto effect = effects::create_exception_effect();
    auto handler = std::make_shared<effects::EffectHandler>(effect);
    
    // raise operation - implements stack unwinding by discarding continuation
    handler->set_enhanced_handler("raise", 
        [](const std::vector<kernel::Value>& args, effects::EffectContinuation& cont) -> kernel::Value {
            if (args.size() != 1) {
                throw std::runtime_error("Exception.raise requires 1 argument (message)");
            }
            
            // Extract exception message from arguments
            std::string message;
            try {
                message = args[0].as_string();
            } catch (const std::exception& e) {
                message = "Exception with non-string payload";
            }
            
            // CRITICAL: For exceptions, we DO NOT call cont.resume()
            // This discards the continuation, which implements stack unwinding
            // The exception propagates up the call stack until caught by a handler
            
            std::cout << "[EXCEPTION] Unwinding stack due to exception: " << message << std::endl;
            
            // Throw a C++ exception to propagate the error
            // This will be caught by the effect runtime and handled appropriately
            throw std::runtime_error("Exception raised: " + message);
        });
    
    return handler;
}

// Create a catching exception handler that can catch and handle exceptions
// This handler allows implementing try/catch semantics by catching exceptions
// and either resuming with a recovery value or re-throwing
//
// Requirements implemented:
// - 28.9: try/catch as library macros expanding to handle blocks
// - 28.12: Pattern matching on exception types in catch blocks
// - 41.22: Exceptions implemented using handle blocks
std::shared_ptr<effects::EffectHandler> create_catching_exception_handler(ExceptionCatchHandler catch_handler) {
    auto effect = effects::create_exception_effect();
    auto handler = std::make_shared<effects::EffectHandler>(effect);
    
    // raise operation - implements exception catching and handling
    handler->set_enhanced_handler("raise",
        [catch_handler](const std::vector<kernel::Value>& args, effects::EffectContinuation& cont) -> kernel::Value {
            if (args.size() != 1) {
                throw std::runtime_error("Exception.raise requires 1 argument (message)");
            }
            
            // Extract exception message from arguments
            std::string message;
            try {
                message = args[0].as_string();
            } catch (const std::exception& e) {
                message = "Exception with non-string payload";
            }
            
            // Parse exception type and message from formatted string
            // Format: "ExceptionType: message" or just "message"
            std::string type_name = "Exception";
            std::string actual_message = message;
            
            size_t colon_pos = message.find(": ");
            if (colon_pos != std::string::npos) {
                type_name = message.substr(0, colon_pos);
                actual_message = message.substr(colon_pos + 2);
            }
            
            // Create MeldException from the parsed information
            MeldException exception(actual_message, type_name, kernel::Value(kernel::Empty::instance()));
            
            std::cout << "[EXCEPTION] Caught exception: " << type_name << ": " << actual_message << std::endl;
            
            try {
                // Invoke the catch handler to process the exception
                kernel::Value recovery_value = catch_handler(exception);
                
                // If the catch handler returns normally, resume with the recovery value
                std::cout << "[EXCEPTION] Exception handled, resuming with recovery value" << std::endl;
                return cont.resume(recovery_value);
                
            } catch (const std::runtime_error& re_throw) {
                // If the catch handler re-throws, discard the continuation (unwind)
                std::cout << "[EXCEPTION] Exception re-thrown, unwinding stack" << std::endl;
                throw re_throw;
            } catch (const std::exception& e) {
                // If the catch handler throws a different exception, propagate it
                std::cout << "[EXCEPTION] New exception thrown in catch handler: " << e.what() << std::endl;
                throw;
            }
        });
    
    return handler;
}

// ============================================================================
// EXCEPTION PARSING AND UTILITIES
// ============================================================================

// Parse exception information from a formatted message
// This handles the serialization format used by the exception system
MeldException parse_exception_message(const std::string& formatted_message) {
    // Format: "ExceptionType: message" or just "message"
    std::string type_name = "Exception";
    std::string message = formatted_message;
    
    size_t colon_pos = formatted_message.find(": ");
    if (colon_pos != std::string::npos) {
        type_name = formatted_message.substr(0, colon_pos);
        message = formatted_message.substr(colon_pos + 2);
    }
    
    return MeldException(message, type_name, kernel::Value(kernel::Empty::instance()));
}

// Format exception information into a serialized message
// This creates the format used by the exception system
std::string format_exception_message(const MeldException& exception) {
    if (exception.type_name == "Exception") {
        return exception.message;
    } else {
        return exception.type_name + ": " + exception.message;
    }
}

// ============================================================================
// DEMONSTRATION AND TESTING FUNCTIONS
// ============================================================================

// Demonstrate exception handling with various scenarios
void demonstrate_exception_handling() {
    std::cout << "\n=== Exception Handling Demonstration ===" << std::endl;
    
    // Test 1: Basic exception throwing and catching
    std::cout << "\nTest 1: Basic exception handling" << std::endl;
    try {
        auto result = try_catch([]() -> kernel::Value {
            std::cout << "  Executing try block..." << std::endl;
            throw_exception("This is a test exception");
            return kernel::Value::from_string("success");  // Never reached
        }, [](const MeldException& ex) -> kernel::Value {
            std::cout << "  Caught exception: " << ex.message << std::endl;
            return kernel::Value::from_string("recovered");
        });
        
        std::cout << "  Result: " << result.as_string() << std::endl;
    } catch (const std::exception& e) {
        std::cout << "  Unexpected C++ exception: " << e.what() << std::endl;
    }
    
    // Test 2: Exception unwinding (no catch handler)
    std::cout << "\nTest 2: Exception unwinding" << std::endl;
    try {
        auto result = try_block([]() -> kernel::Value {
            std::cout << "  Executing try block..." << std::endl;
            throw_exception("Unhandled exception");
            return kernel::Value::from_string("success");  // Never reached
        });
        
        std::cout << "  This should not be printed" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "  Exception unwound to C++ level: " << e.what() << std::endl;
    }
    
    // Test 3: Custom exception types
    std::cout << "\nTest 3: Custom exception types" << std::endl;
    try {
        auto result = try_catch_typed([]() -> kernel::Value {
            std::cout << "  Executing try block..." << std::endl;
            throw_validation_exception("Invalid input data");
            return kernel::Value::from_string("success");  // Never reached
        })
        .catch_type<ValidationException>([](const ValidationException& ex) -> kernel::Value {
            std::cout << "  Caught ValidationException: " << ex.message << std::endl;
            return kernel::Value::from_string("validation_handled");
        })
        .catch_type<RuntimeException>([](const RuntimeException& ex) -> kernel::Value {
            std::cout << "  Caught RuntimeException: " << ex.message << std::endl;
            return kernel::Value::from_string("runtime_handled");
        })
        .catch_all([](const MeldException& ex) -> kernel::Value {
            std::cout << "  Caught unknown exception: " << ex.type_name << ": " << ex.message << std::endl;
            return kernel::Value::from_string("unknown_handled");
        })
        .execute();
        
        std::cout << "  Result: " << result.as_string() << std::endl;
    } catch (const std::exception& e) {
        std::cout << "  Unexpected C++ exception: " << e.what() << std::endl;
    }
    
    // Test 4: Re-throwing exceptions
    std::cout << "\nTest 4: Re-throwing exceptions" << std::endl;
    try {
        auto result = try_catch([]() -> kernel::Value {
            std::cout << "  Executing try block..." << std::endl;
            throw_runtime_exception("Critical error");
            return kernel::Value::from_string("success");  // Never reached
        }, [](const MeldException& ex) -> kernel::Value {
            std::cout << "  Caught exception: " << ex.type_name << ": " << ex.message << std::endl;
            if (ex.type_name == "RuntimeException") {
                std::cout << "  Re-throwing critical error..." << std::endl;
                throw_exception(ex);  // Re-throw
            }
            return kernel::Value::from_string("handled");
        });
        
        std::cout << "  This should not be printed" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "  Re-thrown exception caught at C++ level: " << e.what() << std::endl;
    }
    
    std::cout << "\n=== Exception Handling Demonstration Complete ===" << std::endl;
}

// Test exception handler behavior in isolation
void test_exception_handlers() {
    std::cout << "\n=== Exception Handler Testing ===" << std::endl;
    
    // Test unwinding handler
    std::cout << "\nTesting unwinding handler..." << std::endl;
    auto unwinding_handler = create_unwinding_exception_handler();
    
    std::cout << "Handler created successfully" << std::endl;
    std::cout << "Handler has raise operation: " << unwinding_handler->has_handler("raise") << std::endl;
    std::cout << "Handler is complete: " << unwinding_handler->is_complete() << std::endl;
    
    // Test catching handler
    std::cout << "\nTesting catching handler..." << std::endl;
    auto catching_handler = create_catching_exception_handler([](const MeldException& ex) -> kernel::Value {
        return kernel::Value::from_string("caught: " + ex.message);
    });
    
    std::cout << "Catching handler created successfully" << std::endl;
    std::cout << "Catching handler has raise operation: " << catching_handler->has_handler("raise") << std::endl;
    std::cout << "Catching handler is complete: " << catching_handler->is_complete() << std::endl;
    
    std::cout << "\n=== Exception Handler Testing Complete ===" << std::endl;
}

} // namespace meld::stdlib::exceptions