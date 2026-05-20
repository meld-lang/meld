/**
 * Property-Based Test: Handler Cleanup on Exception
 * 
 * Feature: effects-annotations, Property 8: Handler Cleanup on Exception
 * Validates: Requirements 4.7
 * 
 * Property: For any handle() call, if the computation throws an exception,
 * all pushed handlers must be popped from the stack before the exception propagates.
 * 
 * This test verifies that the handler stack is properly cleaned up even when
 * exceptions occur, ensuring no handler leaks.
 */

#include <gtest/gtest.h>
#include "../../include/meld/stdlib/effects.hpp"
#include "../../include/meld/effects/effect.hpp"
#include "../../include/meld/kernel/primitives.hpp"
#include <string>
#include <vector>
#include <random>
#include <stdexcept>

using namespace meld;
using namespace meld::stdlib;
using namespace meld::kernel;
using namespace meld::effects;

class HandlerCleanupExceptionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear handler stack before each test
        HandlerStack::instance().clear();
    }
    
    void TearDown() override {
        // Clean up after each test
        HandlerStack::instance().clear();
    }
};

/**
 * Test: Simple exception in computation
 * 
 * Verifies that when a computation throws an exception,
 * the handler is properly cleaned up.
 */
TEST_F(HandlerCleanupExceptionTest, SimpleExceptionCleanup) {
    const std::string effect_name = "TestEffect";
    
    // Record initial stack depth
    size_t initial_depth = HandlerStack::instance().depth();
    EXPECT_EQ(initial_depth, 0);
    
    // Create handler
    auto effect_def = std::make_shared<EffectDefinition>(effect_name);
    auto handler = std::make_shared<EffectHandler>(effect_def);
    
    // Test exception handling
    try {
        handle(effect_name, handler, [&]() -> Value {
            // Verify handler is on stack
            EXPECT_EQ(HandlerStack::instance().depth(), 1);
            
            // Throw exception
            throw std::runtime_error("Test exception");
            
            return Value(Empty::instance());
        });
        FAIL() << "Expected exception to be thrown";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Test exception");
    }
    
    // Verify handler was cleaned up
    EXPECT_EQ(HandlerStack::instance().depth(), initial_depth);
}

/**
 * Test: Exception in nested handlers
 * 
 * Verifies that when an exception occurs in nested handlers,
 * all handlers are properly cleaned up.
 */
TEST_F(HandlerCleanupExceptionTest, NestedHandlerExceptionCleanup) {
    const std::string effect1_name = "Effect1";
    const std::string effect2_name = "Effect2";
    
    // Record initial stack depth
    size_t initial_depth = HandlerStack::instance().depth();
    EXPECT_EQ(initial_depth, 0);
    
    // Create handlers
    auto effect1_def = std::make_shared<EffectDefinition>(effect1_name);
    auto handler1 = std::make_shared<EffectHandler>(effect1_def);
    
    auto effect2_def = std::make_shared<EffectDefinition>(effect2_name);
    auto handler2 = std::make_shared<EffectHandler>(effect2_def);
    
    // Test nested exception handling
    try {
        handle(effect1_name, handler1, [&]() -> Value {
            EXPECT_EQ(HandlerStack::instance().depth(), 1);
            
            handle(effect2_name, handler2, [&]() -> Value {
                EXPECT_EQ(HandlerStack::instance().depth(), 2);
                
                // Throw exception from innermost scope
                throw std::runtime_error("Nested exception");
                
                return Value(Empty::instance());
            });
            
            return Value(Empty::instance());
        });
        FAIL() << "Expected exception to be thrown";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Nested exception");
    }
    
    // Verify all handlers were cleaned up
    EXPECT_EQ(HandlerStack::instance().depth(), initial_depth);
}

/**
 * Test: Exception in handler operation
 * 
 * Verifies that when a handler operation throws an exception,
 * the handler is still properly cleaned up.
 */
TEST_F(HandlerCleanupExceptionTest, HandlerOperationExceptionCleanup) {
    const std::string effect_name = "TestEffect";
    const std::string operation_name = "testOp";
    
    // Record initial stack depth
    size_t initial_depth = HandlerStack::instance().depth();
    
    // Create handler that throws
    auto effect_def = std::make_shared<EffectDefinition>(effect_name);
    auto handler = std::make_shared<EffectHandler>(effect_def);
    handler->set_handler(operation_name,
        [](const std::vector<Value>& args, Continuation& cont) -> Value {
            throw std::runtime_error("Handler exception");
        });
    
    // Test exception in handler
    try {
        handle(effect_name, handler, [&]() -> Value {
            EXPECT_EQ(HandlerStack::instance().depth(), 1);
            
            // Perform effect - handler will throw
            return effects::perform(effect_name, operation_name, {});
        });
        FAIL() << "Expected exception to be thrown";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Handler exception");
    }
    
    // Verify handler was cleaned up
    EXPECT_EQ(HandlerStack::instance().depth(), initial_depth);
}


/**
 * Property Test: Random exception scenarios
 * 
 * Tests handler cleanup with randomly generated exception scenarios.
 * This is a property-based test that runs multiple iterations.
 */
TEST_F(HandlerCleanupExceptionTest, PropertyRandomExceptionScenarios) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> depth_dist(1, 5);
    std::uniform_int_distribution<> throw_level_dist(0, 4);
    
    // Run 100 iterations as per requirements
    for (int iteration = 0; iteration < 100; ++iteration) {
        // Clear handler stack
        HandlerStack::instance().clear();
        size_t initial_depth = HandlerStack::instance().depth();
        
        // Generate random nesting depth
        int depth = depth_dist(gen);
        
        // Generate random level at which to throw
        int throw_level = throw_level_dist(gen) % depth;
        
        // Create handlers
        std::vector<std::shared_ptr<EffectHandler>> handlers;
        for (int level = 0; level < depth; ++level) {
            std::string effect_name = "Effect" + std::to_string(level);
            auto effect_def = std::make_shared<EffectDefinition>(effect_name);
            auto handler = std::make_shared<EffectHandler>(effect_def);
            handlers.push_back(handler);
        }
        
        // Build nested handle() calls recursively
        std::function<Value(int)> build_nested = [&](int level) -> Value {
            if (level >= depth) {
                // Innermost level - check if we should throw here
                if (throw_level == level - 1) {
                    throw std::runtime_error("Random exception");
                }
                return Value(Empty::instance());
            } else {
                std::string effect_name = "Effect" + std::to_string(level);
                return handle(effect_name, handlers[level], [&]() -> Value {
                    // Check if we should throw at this level
                    if (throw_level == level) {
                        throw std::runtime_error("Random exception");
                    }
                    return build_nested(level + 1);
                });
            }
        };
        
        // Execute nested handlers with exception
        try {
            build_nested(0);
            // If we get here, no exception was thrown (shouldn't happen)
            FAIL() << "Expected exception in iteration " << iteration;
        } catch (const std::runtime_error&) {
            // Expected exception
        }
        
        // Verify all handlers were cleaned up
        EXPECT_EQ(HandlerStack::instance().depth(), initial_depth)
            << "Iteration " << iteration
            << ": depth=" << depth
            << ", throw_level=" << throw_level;
    }
}


/**
 * Property Test: Exception types
 * 
 * Tests handler cleanup with different exception types.
 */
TEST_F(HandlerCleanupExceptionTest, PropertyDifferentExceptionTypes) {
    const std::string effect_name = "TestEffect";
    
    // Test with different exception types
    std::vector<std::function<void()>> exception_throwers = {
        []() { throw std::runtime_error("runtime_error"); },
        []() { throw std::logic_error("logic_error"); },
        []() { throw std::invalid_argument("invalid_argument"); },
        []() { throw std::out_of_range("out_of_range"); },
        []() { throw std::overflow_error("overflow_error"); },
        []() { throw std::underflow_error("underflow_error"); },
        []() { throw std::range_error("range_error"); },
        []() { throw std::domain_error("domain_error"); },
        []() { throw std::length_error("length_error"); },
        []() { throw std::bad_alloc(); },
        []() { throw std::bad_cast(); },
        []() { throw 42; },  // Non-standard exception
        []() { throw "string exception"; }  // C-string exception
    };
    
    for (size_t i = 0; i < exception_throwers.size(); ++i) {
        // Clear handler stack
        HandlerStack::instance().clear();
        size_t initial_depth = HandlerStack::instance().depth();
        
        // Create handler
        auto effect_def = std::make_shared<EffectDefinition>(effect_name);
        auto handler = std::make_shared<EffectHandler>(effect_def);
        
        // Test exception handling
        try {
            handle(effect_name, handler, [&]() -> Value {
                EXPECT_EQ(HandlerStack::instance().depth(), 1);
                exception_throwers[i]();
                return Value(Empty::instance());
            });
            FAIL() << "Expected exception in iteration " << i;
        } catch (...) {
            // Catch any exception type
        }
        
        // Verify handler was cleaned up
        EXPECT_EQ(HandlerStack::instance().depth(), initial_depth)
            << "Exception type iteration " << i;
    }
}


/**
 * Property Test: Stack depth consistency
 * 
 * Verifies that the handler stack depth is always consistent,
 * even with complex exception scenarios.
 */
TEST_F(HandlerCleanupExceptionTest, PropertyStackDepthConsistency) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> operation_dist(0, 3);
    
    // Run 100 iterations
    for (int iteration = 0; iteration < 100; ++iteration) {
        // Clear handler stack
        HandlerStack::instance().clear();
        size_t initial_depth = HandlerStack::instance().depth();
        EXPECT_EQ(initial_depth, 0);
        
        // Perform random operations
        for (int op = 0; op < 10; ++op) {
            int operation = operation_dist(gen);
            
            try {
                switch (operation) {
                    case 0: {
                        // Install handler and throw immediately
                        auto effect_def = std::make_shared<EffectDefinition>("TestEffect");
                        auto handler = std::make_shared<EffectHandler>(effect_def);
                        handle("TestEffect", handler, [&]() -> Value {
                            throw std::runtime_error("Immediate throw");
                            return Value(Empty::instance());
                        });
                        break;
                    }
                    case 1: {
                        // Install nested handlers and throw
                        auto effect1_def = std::make_shared<EffectDefinition>("Effect1");
                        auto handler1 = std::make_shared<EffectHandler>(effect1_def);
                        handle("Effect1", handler1, [&]() -> Value {
                            auto effect2_def = std::make_shared<EffectDefinition>("Effect2");
                            auto handler2 = std::make_shared<EffectHandler>(effect2_def);
                            handle("Effect2", handler2, [&]() -> Value {
                                throw std::runtime_error("Nested throw");
                                return Value(Empty::instance());
                            });
                            return Value(Empty::instance());
                        });
                        break;
                    }
                    case 2: {
                        // Install handler, perform effect, throw
                        auto effect_def = std::make_shared<EffectDefinition>("TestEffect");
                        auto handler = std::make_shared<EffectHandler>(effect_def);
                        handler->set_handler("testOp",
                            [](const std::vector<Value>& args, Continuation& cont) -> Value {
                                return cont.resume(Value(Empty::instance()));
                            });
                        handle("TestEffect", handler, [&]() -> Value {
                            effects::perform("TestEffect", "testOp", {});
                            throw std::runtime_error("After perform");
                            return Value(Empty::instance());
                        });
                        break;
                    }
                    case 3: {
                        // Install handler successfully (no exception)
                        auto effect_def = std::make_shared<EffectDefinition>("TestEffect");
                        auto handler = std::make_shared<EffectHandler>(effect_def);
                        handle("TestEffect", handler, [&]() -> Value {
                            return Value(Empty::instance());
                        });
                        break;
                    }
                }
            } catch (...) {
                // Ignore exceptions
            }
            
            // Verify stack is always clean after each operation
            EXPECT_EQ(HandlerStack::instance().depth(), initial_depth)
                << "Iteration " << iteration
                << ", operation " << op
                << ", type " << operation;
        }
    }
}


