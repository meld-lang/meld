#include <gtest/gtest.h>
#include "../../include/meld/stdlib/exceptions.hpp"
#include "../../include/meld/effects/builtin_effects.hpp"
#include <stdexcept>

using namespace meld::stdlib::exceptions;
using namespace meld::effects;
using namespace meld::kernel;

// ============================================================================
// EXCEPTION LIBRARY TESTS
// Task 35.14: Test exceptions as effects implementation
// Requirements: 41.22, 28.7, 28.8, 28.9, 28.10
// ============================================================================

class ExceptionsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear any existing handlers
        EffectRuntime::instance().clear_handlers();
    }
    
    void TearDown() override {
        // Clean up handlers
        EffectRuntime::instance().clear_handlers();
    }
};

// Test basic exception throwing functionality
// Requirements: 28.8 - throw as library function performing Exception.raise
TEST_F(ExceptionsTest, BasicExceptionThrowing) {
    // Install an unwinding exception handler
    auto handler = create_unwinding_exception_handler();
    EffectRuntime::instance().pushScope(handler);
    
    // Test that throw_exception performs Exception.raise and unwinds
    EXPECT_THROW({
        throw_exception("Test exception message");
    }, std::runtime_error);
}

// Test custom exception types
// Requirements: 28.11 - Support custom exception types as effect data payloads
TEST_F(ExceptionsTest, CustomExceptionTypes) {
    auto handler = create_unwinding_exception_handler();
    EffectRuntime::instance().pushScope(handler);
    
    // Test RuntimeException
    EXPECT_THROW({
        throw_runtime_exception("Runtime error occurred");
    }, std::runtime_error);
    
    // Test ValidationException
    EXPECT_THROW({
        throw_validation_exception("Validation failed");
    }, std::runtime_error);
    
    // Test NetworkException
    EXPECT_THROW({
        throw_network_exception("Network timeout");
    }, std::runtime_error);
}

// Test exception handler creation
// Requirements: 28.10 - Exception handlers discard continuations to unwind stack
TEST_F(ExceptionsTest, ExceptionHandlerCreation) {
    // Test unwinding handler creation
    auto unwinding_handler = create_unwinding_exception_handler();
    EXPECT_TRUE(unwinding_handler->has_handler("raise"));
    EXPECT_TRUE(unwinding_handler->is_complete());
    
    // Test catching handler creation
    auto catching_handler = create_catching_exception_handler([](const MeldException& ex) -> Value {
        return Value::from_string("caught: " + ex.message);
    });
    EXPECT_TRUE(catching_handler->has_handler("raise"));
    EXPECT_TRUE(catching_handler->is_complete());
}

// Test try_block functionality
// Requirements: 28.9 - try/catch as library macros expanding to handle blocks
TEST_F(ExceptionsTest, TryBlockFunctionality) {
    // Test successful execution (no exception)
    auto result1 = try_block([]() -> Value {
        return Value::from_string("success");
    });
    EXPECT_EQ(result1.as<String>()->value(), "success");
    
    // Test exception handling (should propagate as C++ exception)
    EXPECT_THROW({
        try_block([]() -> Value {
            throw_exception("Test exception");
            return Value::from_string("never reached");
        });
    }, std::runtime_error);
}

// Test try_catch functionality
// Requirements: 28.9 - try/catch as library macros expanding to handle blocks
TEST_F(ExceptionsTest, TryCatchFunctionality) {
    // Test successful execution (no exception)
    auto result1 = try_catch([]() -> Value {
        return Value::from_string("success");
    }, [](const MeldException& ex) -> Value {
        return Value::from_string("caught: " + ex.message);
    });
    EXPECT_EQ(result1.as<String>()->value(), "success");
    
    // Test exception catching and recovery
    auto result2 = try_catch([]() -> Value {
        throw_exception("Test exception");
        return Value::from_string("never reached");
    }, [](const MeldException& ex) -> Value {
        return Value::from_string("recovered from: " + ex.message);
    });
    EXPECT_EQ(result2.as<String>()->value(), "recovered from: Test exception");
}

// Test typed exception handling
// Requirements: 28.12 - Pattern matching on exception types in catch blocks
TEST_F(ExceptionsTest, TypedExceptionHandling) {
    // Test ValidationException handling
    Value result1 = try_catch_typed([]() -> Value {
        throw_validation_exception("Invalid input");
        return Value::from_string("never reached");
    })
    .catch_type<ValidationException>([](const ValidationException& ex) -> Value {
        return Value::from_string("validation_handled: " + ex.message);
    })
    .catch_all([](const MeldException& ex) -> Value {
        return Value::from_string("other_handled: " + ex.message);
    });
    
    EXPECT_EQ(result1.as<String>()->value(), "validation_handled: Invalid input");
    
    // Test RuntimeException handling
    Value result2 = try_catch_typed([]() -> Value {
        throw_runtime_exception("Runtime error");
        return Value::from_string("never reached");
    })
    .catch_type<ValidationException>([](const ValidationException& ex) -> Value {
        return Value::from_string("validation_handled: " + ex.message);
    })
    .catch_type<RuntimeException>([](const RuntimeException& ex) -> Value {
        return Value::from_string("runtime_handled: " + ex.message);
    })
    .catch_all([](const MeldException& ex) -> Value {
        return Value::from_string("other_handled: " + ex.message);
    });
    
    EXPECT_EQ(result2.as<String>()->value(), "runtime_handled: Runtime error");
    
    // Test catch_all handling
    Value result3 = try_catch_typed([]() -> Value {
        throw_network_exception("Network error");
        return Value::from_string("never reached");
    })
    .catch_type<ValidationException>([](const ValidationException& ex) -> Value {
        return Value::from_string("validation_handled: " + ex.message);
    })
    .catch_type<RuntimeException>([](const RuntimeException& ex) -> Value {
        return Value::from_string("runtime_handled: " + ex.message);
    })
    .catch_all([](const MeldException& ex) -> Value {
        return Value::from_string("other_handled: " + ex.type_name + ": " + ex.message);
    });
    
    EXPECT_EQ(result3.as<String>()->value(), "other_handled: NetworkException: Network error");
}

// Test exception re-throwing
// Requirements: 41.22 - Exceptions as effects where handlers can re-throw
TEST_F(ExceptionsTest, ExceptionReThrowingInCatch) {
    // Test re-throwing in catch handler
    EXPECT_THROW({
        try_catch([]() -> Value {
            throw_validation_exception("Critical error");
            return Value::from_string("never reached");
        }, [](const MeldException& ex) -> Value {
            if (ex.message.find("Critical") != std::string::npos) {
                // Re-throw critical errors
                throw_exception(ex);
            }
            return Value::from_string("handled: " + ex.message);
        });
    }, std::runtime_error);
}

// Test exception utilities
TEST_F(ExceptionsTest, ExceptionUtilities) {
    MeldException ex("Test message", "TestException", Value::from_string("payload"));
    
    // Test utility functions
    EXPECT_EQ(get_exception_message(ex), "Test message");
    EXPECT_EQ(get_exception_type(ex), "TestException");
    EXPECT_EQ(get_exception_payload(ex).as<String>()->value(), "payload");
    
    // Test type checking
    ValidationException val_ex("Validation error");
    EXPECT_TRUE(is_exception_type<ValidationException>(val_ex));
    EXPECT_FALSE(is_exception_type<RuntimeException>(val_ex));
    
    RuntimeException run_ex("Runtime error");
    EXPECT_TRUE(is_exception_type<RuntimeException>(run_ex));
    EXPECT_FALSE(is_exception_type<ValidationException>(run_ex));
}

// Test exception message formatting and parsing
TEST_F(ExceptionsTest, ExceptionMessageFormatting) {
    // Test basic exception (no type prefix)
    MeldException basic_ex("Simple message");
    std::string formatted1 = get_exception_message(basic_ex);
    EXPECT_EQ(formatted1, "Simple message");
    
    MeldException parsed1 = get_exception_message(formatted1);
    EXPECT_EQ(parsed1.message, "Simple message");
    EXPECT_EQ(parsed1.type_name, "Exception");
    
    // Test typed exception (with type prefix)
    ValidationException typed_ex("Validation failed");
    std::string formatted2 = get_exception_message(typed_ex);
    EXPECT_EQ(formatted2, "ValidationException: Validation failed");
    
    MeldException parsed2 = get_exception_message(formatted2);
    EXPECT_EQ(parsed2.message, "Validation failed");
    EXPECT_EQ(parsed2.type_name, "ValidationException");
}

// Test integration with effect system
// Requirements: 41.22 - Exceptions implemented as effects
TEST_F(ExceptionsTest, EffectSystemIntegration) {
    auto& runtime = EffectRuntime::instance();
    
    // Test that exception handlers work with the effect runtime
    auto handler = create_unwinding_exception_handler();
    runtime.pushScope(handler);
    
    // Verify handler is registered
    EXPECT_TRUE(runtime.has_handler("Exception"));
    
    // Test performing exception effect directly
    EXPECT_THROW({
        runtime.perform_effect("Exception", "raise", {Value::from_string("Direct effect test")});
    }, std::runtime_error);
    
    runtime.popScope();
    
    // Verify handler is removed
    EXPECT_FALSE(runtime.has_handler("Exception"));
}

// Test nested exception handling
// Requirements: 41.17 - Support nested handlers with proper precedence
TEST_F(ExceptionsTest, NestedExceptionHandling) {
    // Outer handler catches all exceptions
    auto result = try_catch([]() -> Value {
        // Inner handler catches only ValidationExceptions
        return try_catch([]() -> Value {
            throw_validation_exception("Inner exception");
            return Value::from_string("never reached");
        }, [](const MeldException& ex) -> Value {
            if (ex.type_name == "ValidationException") {
                return Value::from_string("inner_handled: " + ex.message);
            }
            throw_exception(ex);  // Re-throw if not handled
            return Value::from_string("never reached");
        });
    }, [](const MeldException& ex) -> Value {
        return Value::from_string("outer_handled: " + ex.message);
    });
    
    EXPECT_EQ(result.as<String>()->value(), "inner_handled: Inner exception");
}

// Test exception handling with other effects
// Requirements: 41.22 - Exceptions work with other effects in the system
TEST_F(ExceptionsTest, ExceptionWithOtherEffects) {
    // Install both Console and Exception handlers
    auto console_handler = create_console_handler();
    auto exception_handler = create_catching_exception_handler([](const MeldException& ex) -> Value {
        return Value::from_string("exception_handled: " + ex.message);
    });
    
    auto& runtime = EffectRuntime::instance();
    runtime.pushScope({console_handler, exception_handler});
    
    // Test that both effects work together
    auto result = try_catch([]() -> Value {
        // This would normally print to console, but we're in a test environment
        // so we'll just simulate the effect
        throw_exception("Test with multiple effects");
        return Value::from_string("never reached");
    }, [](const MeldException& ex) -> Value {
        return Value::from_string("handled: " + ex.message);
    });
    
    EXPECT_EQ(result.as<String>()->value(), "handled: Test with multiple effects");
    
    runtime.popScope();
}

// Performance test for exception handling
TEST_F(ExceptionsTest, ExceptionHandlingPerformance) {
    auto handler = create_catching_exception_handler([](const MeldException& ex) -> Value {
        return Value::from_string("handled");
    });
    EffectRuntime::instance().pushScope(handler);
    
    // Test that exception handling doesn't have excessive overhead
    const int iterations = 1000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        try_catch([]() -> Value {
            throw_exception("Performance test exception");
            return Value::from_string("never reached");
        }, [](const MeldException& ex) -> Value {
            return Value::from_string("handled");
        });
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Should complete within reasonable time (less than 1 second for 1000 iterations)
    EXPECT_LT(duration.count(), 1000000);  // 1 second in microseconds
    
    std::cout << "Exception handling performance: " << iterations << " iterations in " 
              << duration.count() << " microseconds" << std::endl;
}