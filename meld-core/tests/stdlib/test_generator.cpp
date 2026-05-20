#include <gtest/gtest.h>
#include "../../include/meld/stdlib/generator.hpp"
#include "../../include/meld/effects/builtin_effects.hpp"
#include "../../include/meld/effects/effect.hpp"
#include <vector>
#include <string>

using namespace meld::stdlib;
using namespace meld::effects;
using namespace meld::kernel;

// ============================================================================
// GENERATOR TESTS
// Task 35.16: Implement generators as effects
// Requirements: 41.24, 41B.3, 41B.6, 41B.9
// ============================================================================

class GeneratorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear any existing handlers
        EffectRuntime::instance().clear_handlers();
        
        // Install built-in effects including Generator
        install_builtin_effects();
    }
    
    void TearDown() override {
        // Clean up handlers
        EffectRuntime::instance().clear_handlers();
    }
};

// Test basic generator yield functionality
TEST_F(GeneratorTest, BasicYieldOperation) {
    // Test that yield performs Generator.yield effect
    // Requirement 41.24: Implement yield as library function performing Generator.yield
    
    bool yield_called = false;
    std::string yielded_value;
    
    // Create a custom generator handler to capture yields
    auto effect = create_generator_effect();
    auto handler = std::make_shared<EffectHandler>(effect);
    
    handler->set_enhanced_handler("yield", 
        [&yield_called, &yielded_value](const std::vector<Value>& args, EffectContinuation& cont) -> Value {
            yield_called = true;
            yielded_value = args[0].as_string();
            return cont.resume(Value::from_string("yield_handled"));
        });
    
    // Install the custom handler
    EffectRuntime::instance().pushScope(handler);
    
    try {
        // Perform a yield operation
        Value result = perform("Generator", "yield", {Value::from_string("test_value")});
        
        // Verify the yield was handled
        EXPECT_TRUE(yield_called);
        EXPECT_EQ(yielded_value, "test_value");
        EXPECT_EQ(result.as_string(), "yield_handled");
        
    } catch (const std::exception& e) {
        FAIL() << "Yield operation failed: " << e.what();
    }
    
    EffectRuntime::instance().popScope();
}

// Test generator effect definition
TEST_F(GeneratorTest, GeneratorEffectDefinition) {
    // Verify that Generator effect is properly defined
    auto effect = create_generator_effect();
    
    EXPECT_EQ(effect->name(), "Generator");
    EXPECT_TRUE(effect->has_operation("yield"));
    
    const auto* yield_op = effect->get_operation("yield");
    ASSERT_NE(yield_op, nullptr);
    EXPECT_EQ(yield_op->name, "yield");
    EXPECT_EQ(yield_op->parameter_types.size(), 1);
    EXPECT_EQ(yield_op->parameter_types[0], "string");
    EXPECT_EQ(yield_op->return_type, "void");
}

// Test generator handler creation
TEST_F(GeneratorTest, GeneratorHandlerCreation) {
    // Test that generator handler is properly created
    auto handler = create_generator_handler();
    
    EXPECT_NE(handler, nullptr);
    EXPECT_EQ(handler->effect().name(), "Generator");
    EXPECT_TRUE(handler->has_handler("yield"));
    EXPECT_TRUE(handler->is_complete());
}

// Test multiple yield operations
TEST_F(GeneratorTest, MultipleYieldOperations) {
    // Test that generators can yield multiple values
    // Requirement 41B.3: Implement generators as effects where handlers resume multiple times
    
    std::vector<std::string> yielded_values;
    int yield_count = 0;
    
    // Create a handler that captures multiple yields
    auto effect = create_generator_effect();
    auto handler = std::make_shared<EffectHandler>(effect);
    
    handler->set_enhanced_handler("yield", 
        [&yielded_values, &yield_count](const std::vector<Value>& args, EffectContinuation& cont) -> Value {
            yielded_values.push_back(args[0].as_string());
            yield_count++;
            return cont.resume(Value::from_string("yield_" + std::to_string(yield_count)));
        });
    
    EffectRuntime::instance().pushScope(handler);
    
    try {
        // Perform multiple yield operations
        Value result1 = perform("Generator", "yield", {Value::from_string("first")});
        Value result2 = perform("Generator", "yield", {Value::from_string("second")});
        Value result3 = perform("Generator", "yield", {Value::from_string("third")});
        
        // Verify all yields were captured
        EXPECT_EQ(yielded_values.size(), 3);
        EXPECT_EQ(yielded_values[0], "first");
        EXPECT_EQ(yielded_values[1], "second");
        EXPECT_EQ(yielded_values[2], "third");
        
        EXPECT_EQ(result1.as_string(), "yield_1");
        EXPECT_EQ(result2.as_string(), "yield_2");
        EXPECT_EQ(result3.as_string(), "yield_3");
        
    } catch (const std::exception& e) {
        FAIL() << "Multiple yield operations failed: " << e.what();
    }
    
    EffectRuntime::instance().popScope();
}

// Test generator with continuation storage
TEST_F(GeneratorTest, GeneratorContinuationStorage) {
    // Test that generator handlers can store continuations for later resumption
    // Requirement 41B.9: When a generator yields, handler resumes continuation to produce next value
    
    std::shared_ptr<Continuation> stored_continuation;
    std::string stored_value;
    bool continuation_stored = false;
    
    // Create a handler that stores continuations
    auto effect = create_generator_effect();
    auto handler = std::make_shared<EffectHandler>(effect);
    
    handler->set_enhanced_handler("yield", 
        [&stored_continuation, &stored_value, &continuation_stored]
        (const std::vector<Value>& args, EffectContinuation& cont) -> Value {
            stored_value = args[0].as_string();
            stored_continuation = cont.get_kernel_continuation();
            continuation_stored = true;
            
            // Don't resume immediately - store for later
            // This simulates how real generators work
            return Value::from_string("suspended");
        });
    
    EffectRuntime::instance().pushScope(handler);
    
    try {
        // Perform yield operation
        Value result = perform("Generator", "yield", {Value::from_string("stored_value")});
        
        // Verify continuation was stored
        EXPECT_TRUE(continuation_stored);
        EXPECT_EQ(stored_value, "stored_value");
        EXPECT_NE(stored_continuation, nullptr);
        EXPECT_EQ(result.as_string(), "suspended");
        
        // Verify continuation is valid
        EXPECT_TRUE(stored_continuation->is_valid());
        
    } catch (const std::exception& e) {
        FAIL() << "Continuation storage failed: " << e.what();
    }
    
    EffectRuntime::instance().popScope();
}

// Test generator effect integration with runtime
TEST_F(GeneratorTest, GeneratorRuntimeIntegration) {
    // Test that generator effects integrate properly with the effect runtime
    
    // Verify generator handler is available
    EXPECT_TRUE(EffectRuntime::instance().has_handler("Generator"));
    EXPECT_TRUE(EffectRuntime::instance().has_handler("Generator", "yield"));
    
    // Test finding generator handler
    auto handler = EffectRuntime::instance().find_handler("Generator");
    EXPECT_NE(handler, nullptr);
    EXPECT_EQ(handler->effect().name(), "Generator");
    
    auto yield_handler = EffectRuntime::instance().find_handler("Generator", "yield");
    EXPECT_NE(yield_handler, nullptr);
    EXPECT_TRUE(yield_handler->has_handler("yield"));
}

// Test generator error handling
TEST_F(GeneratorTest, GeneratorErrorHandling) {
    // Test error handling in generator operations
    
    // Test yield with wrong number of arguments
    EXPECT_THROW({
        perform("Generator", "yield", {});  // No arguments
    }, std::runtime_error);
    
    EXPECT_THROW({
        perform("Generator", "yield", {
            Value::from_string("arg1"), 
            Value::from_string("arg2")
        });  // Too many arguments
    }, std::runtime_error);
    
    // Test invalid operation
    EXPECT_THROW({
        perform("Generator", "invalid_operation", {Value::from_string("test")});
    }, std::runtime_error);
}

// Test generator with nested handlers
TEST_F(GeneratorTest, GeneratorNestedHandlers) {
    // Test that generator handlers work correctly with nested scopes
    // Requirement 41.17: Support nested handlers with inner handlers taking precedence
    
    std::vector<std::string> outer_yields;
    std::vector<std::string> inner_yields;
    
    // Create outer handler
    auto outer_effect = create_generator_effect();
    auto outer_handler = std::make_shared<EffectHandler>(outer_effect);
    outer_handler->set_enhanced_handler("yield", 
        [&outer_yields](const std::vector<Value>& args, EffectContinuation& cont) -> Value {
            outer_yields.push_back(args[0].as_string());
            return cont.resume(Value::from_string("outer_handled"));
        });
    
    // Create inner handler
    auto inner_effect = create_generator_effect();
    auto inner_handler = std::make_shared<EffectHandler>(inner_effect);
    inner_handler->set_enhanced_handler("yield", 
        [&inner_yields](const std::vector<Value>& args, EffectContinuation& cont) -> Value {
            inner_yields.push_back(args[0].as_string());
            return cont.resume(Value::from_string("inner_handled"));
        });
    
    // Install outer handler
    EffectRuntime::instance().pushScope(outer_handler);
    
    try {
        // Yield should go to outer handler
        Value result1 = perform("Generator", "yield", {Value::from_string("outer_test")});
        EXPECT_EQ(result1.as_string(), "outer_handled");
        EXPECT_EQ(outer_yields.size(), 1);
        EXPECT_EQ(inner_yields.size(), 0);
        
        // Install inner handler (should take precedence)
        EffectRuntime::instance().pushScope(inner_handler);
        
        try {
            // Yield should go to inner handler
            Value result2 = perform("Generator", "yield", {Value::from_string("inner_test")});
            EXPECT_EQ(result2.as_string(), "inner_handled");
            EXPECT_EQ(outer_yields.size(), 1);  // Unchanged
            EXPECT_EQ(inner_yields.size(), 1);  // New yield
            
            // Remove inner handler
            EffectRuntime::instance().popScope();
            
            // Yield should go back to outer handler
            Value result3 = perform("Generator", "yield", {Value::from_string("outer_again")});
            EXPECT_EQ(result3.as_string(), "outer_handled");
            EXPECT_EQ(outer_yields.size(), 2);  // New yield
            EXPECT_EQ(inner_yields.size(), 1);  // Unchanged
            
        } catch (const std::exception& e) {
            EffectRuntime::instance().popScope();  // Clean up inner
            throw;
        }
        
    } catch (const std::exception& e) {
        EffectRuntime::instance().popScope();  // Clean up outer
        FAIL() << "Nested handler test failed: " << e.what();
    }
    
    EffectRuntime::instance().popScope();  // Clean up outer
}

// Test generator library functions
TEST_F(GeneratorTest, GeneratorLibraryFunctions) {
    // Test the library functions for generator operations
    
    // Test that we can create generator effects and handlers
    auto effect = create_generator_effect();
    EXPECT_NE(effect, nullptr);
    EXPECT_EQ(effect->name(), "Generator");
    
    auto handler = create_generator_handler();
    EXPECT_NE(handler, nullptr);
    EXPECT_TRUE(handler->is_complete());
    
    // Test that built-in effects include generator
    auto all_handlers = create_all_builtin_handlers();
    bool found_generator = false;
    for (const auto& h : all_handlers) {
        if (h->effect().name() == "Generator") {
            found_generator = true;
            break;
        }
    }
    EXPECT_TRUE(found_generator);
}

// Test generator effect with different value types
TEST_F(GeneratorTest, GeneratorValueTypes) {
    // Test that generators can handle different value types
    
    std::vector<std::string> captured_values;
    
    auto effect = create_generator_effect();
    auto handler = std::make_shared<EffectHandler>(effect);
    
    handler->set_enhanced_handler("yield", 
        [&captured_values](const std::vector<Value>& args, EffectContinuation& cont) -> Value {
            captured_values.push_back(args[0].as_string());
            return cont.resume(Value::from_string("handled"));
        });
    
    EffectRuntime::instance().pushScope(handler);
    
    try {
        // Test string values
        perform("Generator", "yield", {Value::from_string("hello")});
        
        // Test numeric values (converted to string)
        perform("Generator", "yield", {Value::from_int(42)});
        
        // Test boolean values (converted to string)
        perform("Generator", "yield", {Value::from_bool(true)});
        
        // Verify all values were captured
        EXPECT_EQ(captured_values.size(), 3);
        EXPECT_EQ(captured_values[0], "hello");
        EXPECT_EQ(captured_values[1], "42");
        EXPECT_EQ(captured_values[2], "1");  // true as string
        
    } catch (const std::exception& e) {
        FAIL() << "Value type test failed: " << e.what();
    }
    
    EffectRuntime::instance().popScope();
}

// Performance test for generator operations
TEST_F(GeneratorTest, GeneratorPerformance) {
    // Test that generator operations perform reasonably well
    
    int yield_count = 0;
    
    auto effect = create_generator_effect();
    auto handler = std::make_shared<EffectHandler>(effect);
    
    handler->set_enhanced_handler("yield", 
        [&yield_count](const std::vector<Value>& args, EffectContinuation& cont) -> Value {
            yield_count++;
            return cont.resume(Value::from_string("handled"));
        });
    
    EffectRuntime::instance().pushScope(handler);
    
    try {
        // Perform many yield operations
        const int num_yields = 1000;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < num_yields; ++i) {
            perform("Generator", "yield", {Value::from_string("value_" + std::to_string(i))});
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        // Verify all yields were processed
        EXPECT_EQ(yield_count, num_yields);
        
        // Performance should be reasonable (less than 1ms per yield on average)
        double avg_time_per_yield = static_cast<double>(duration.count()) / num_yields;
        EXPECT_LT(avg_time_per_yield, 1000.0);  // Less than 1000 microseconds per yield
        
        std::cout << "Generator performance: " << avg_time_per_yield 
                  << " microseconds per yield (total: " << duration.count() 
                  << " microseconds for " << num_yields << " yields)" << std::endl;
        
    } catch (const std::exception& e) {
        FAIL() << "Performance test failed: " << e.what();
    }
    
    EffectRuntime::instance().popScope();
}