/**
 * Property-Based Test: Handler Stack Isolation
 * 
 * Feature: effects-annotations, Property 4: Handler Stack Isolation
 * Validates: Requirements 4.6
 * 
 * Property: For any nested handle() calls, inner handlers must take precedence
 * over outer handlers for the same effect type.
 * 
 * This test verifies that when multiple handlers for the same effect are installed
 * in nested scopes, the innermost (most recently installed) handler is used.
 */

#include <gtest/gtest.h>
#include "../../include/meld/stdlib/effects.hpp"
#include "../../include/meld/effects/effect.hpp"
#include "../../include/meld/kernel/primitives.hpp"
#include <string>
#include <vector>
#include <random>

using namespace meld;
using namespace meld::stdlib;
using namespace meld::kernel;
using namespace meld::effects;

class HandlerStackIsolationTest : public ::testing::Test {
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
 * Test: Simple two-level nesting
 * 
 * Verifies that with two nested handlers for the same effect,
 * the inner handler takes precedence.
 */
TEST_F(HandlerStackIsolationTest, TwoLevelNesting) {
    const std::string effect_name = "TestEffect";
    const std::string operation_name = "testOp";
    
    // Create outer handler
    auto outer_effect_def = std::make_shared<EffectDefinition>(effect_name);
    auto outer_handler = std::make_shared<EffectHandler>(outer_effect_def);
    outer_handler->set_handler(operation_name,
        [](const std::vector<Value>& args, Continuation& cont) -> Value {
            return cont.resume(Value(std::make_shared<String>("outer_value")));
        });
    
    // Create inner handler
    auto inner_effect_def = std::make_shared<EffectDefinition>(effect_name);
    auto inner_handler = std::make_shared<EffectHandler>(inner_effect_def);
    inner_handler->set_handler(operation_name,
        [](const std::vector<Value>& args, Continuation& cont) -> Value {
            return cont.resume(Value(std::make_shared<String>("inner_value")));
        });
    
    // Test nested handlers
    std::string result = handle(effect_name, outer_handler, [&]() -> Value {
        return handle(effect_name, inner_handler, [&]() -> Value {
            // Perform effect - should use inner handler
            return effects::perform(effect_name, operation_name, {});
        });
    }).try_as<String>().value()->value();
    
    // Verify inner handler was used
    EXPECT_EQ(result, "inner_value");
}

/**
 * Test: Three-level nesting
 * 
 * Verifies that with three nested handlers for the same effect,
 * the innermost handler takes precedence.
 */
TEST_F(HandlerStackIsolationTest, ThreeLevelNesting) {
    const std::string effect_name = "TestEffect";
    const std::string operation_name = "testOp";
    
    // Create handlers
    auto outer_effect_def = std::make_shared<EffectDefinition>(effect_name);
    auto outer_handler = std::make_shared<EffectHandler>(outer_effect_def);
    outer_handler->set_handler(operation_name,
        [](const std::vector<Value>& args, Continuation& cont) -> Value {
            return cont.resume(Value(std::make_shared<String>("outer_value")));
        });
    
    auto middle_effect_def = std::make_shared<EffectDefinition>(effect_name);
    auto middle_handler = std::make_shared<EffectHandler>(middle_effect_def);
    middle_handler->set_handler(operation_name,
        [](const std::vector<Value>& args, Continuation& cont) -> Value {
            return cont.resume(Value(std::make_shared<String>("middle_value")));
        });
    
    auto inner_effect_def = std::make_shared<EffectDefinition>(effect_name);
    auto inner_handler = std::make_shared<EffectHandler>(inner_effect_def);
    inner_handler->set_handler(operation_name,
        [](const std::vector<Value>& args, Continuation& cont) -> Value {
            return cont.resume(Value(std::make_shared<String>("inner_value")));
        });
    
    // Test nested handlers
    std::string result = handle(effect_name, outer_handler, [&]() -> Value {
        return handle(effect_name, middle_handler, [&]() -> Value {
            return handle(effect_name, inner_handler, [&]() -> Value {
                // Perform effect - should use innermost handler
                return effects::perform(effect_name, operation_name, {});
            });
        });
    }).try_as<String>().value()->value();
    
    // Verify innermost handler was used
    EXPECT_EQ(result, "inner_value");
}

/**
 * Test: Handler isolation after inner scope exits
 * 
 * Verifies that after an inner handler scope exits, the outer handler
 * is used again.
 */
TEST_F(HandlerStackIsolationTest, HandlerIsolationAfterScopeExit) {
    const std::string effect_name = "TestEffect";
    const std::string operation_name = "testOp";
    
    // Create handlers
    auto outer_effect_def = std::make_shared<EffectDefinition>(effect_name);
    auto outer_handler = std::make_shared<EffectHandler>(outer_effect_def);
    outer_handler->set_handler(operation_name,
        [](const std::vector<Value>& args, Continuation& cont) -> Value {
            return cont.resume(Value(std::make_shared<String>("outer_value")));
        });
    
    auto inner_effect_def = std::make_shared<EffectDefinition>(effect_name);
    auto inner_handler = std::make_shared<EffectHandler>(inner_effect_def);
    inner_handler->set_handler(operation_name,
        [](const std::vector<Value>& args, Continuation& cont) -> Value {
            return cont.resume(Value(std::make_shared<String>("inner_value")));
        });
    
    std::vector<std::string> results;
    
    // Test handler isolation
    handle(effect_name, outer_handler, [&]() -> Value {
        // First perform - should use outer handler
        results.push_back(
            effects::perform(effect_name, operation_name, {})
                .try_as<String>().value()->value()
        );
        
        // Inner scope
        handle(effect_name, inner_handler, [&]() -> Value {
            // Second perform - should use inner handler
            results.push_back(
                effects::perform(effect_name, operation_name, {})
                    .try_as<String>().value()->value()
            );
            return Value(Empty::instance());
        });
        
        // Third perform - should use outer handler again
        results.push_back(
            effects::perform(effect_name, operation_name, {})
                .try_as<String>().value()->value()
        );
        
        return Value(Empty::instance());
    });
    
    // Verify handler isolation
    ASSERT_EQ(results.size(), 3);
    EXPECT_EQ(results[0], "outer_value");  // Before inner scope
    EXPECT_EQ(results[1], "inner_value");  // Inside inner scope
    EXPECT_EQ(results[2], "outer_value");  // After inner scope
}

/**
 * Test: Multiple different effects with nesting
 * 
 * Verifies that handlers for different effects don't interfere with each other,
 * even when nested.
 */
TEST_F(HandlerStackIsolationTest, MultipleDifferentEffects) {
    const std::string effect1_name = "Effect1";
    const std::string effect2_name = "Effect2";
    const std::string operation_name = "testOp";
    
    // Create handlers for Effect1
    auto effect1_def = std::make_shared<EffectDefinition>(effect1_name);
    auto effect1_handler = std::make_shared<EffectHandler>(effect1_def);
    effect1_handler->set_handler(operation_name,
        [](const std::vector<Value>& args, Continuation& cont) -> Value {
            return cont.resume(Value(std::make_shared<String>("effect1_value")));
        });
    
    // Create handlers for Effect2
    auto effect2_def = std::make_shared<EffectDefinition>(effect2_name);
    auto effect2_handler = std::make_shared<EffectHandler>(effect2_def);
    effect2_handler->set_handler(operation_name,
        [](const std::vector<Value>& args, Continuation& cont) -> Value {
            return cont.resume(Value(std::make_shared<String>("effect2_value")));
        });
    
    std::vector<std::string> results;
    
    // Test multiple effects
    handle(effect1_name, effect1_handler, [&]() -> Value {
        handle(effect2_name, effect2_handler, [&]() -> Value {
            // Perform both effects
            results.push_back(
                effects::perform(effect1_name, operation_name, {})
                    .try_as<String>().value()->value()
            );
            results.push_back(
                effects::perform(effect2_name, operation_name, {})
                    .try_as<String>().value()->value()
            );
            return Value(Empty::instance());
        });
        return Value(Empty::instance());
    });
    
    // Verify each effect used its own handler
    ASSERT_EQ(results.size(), 2);
    EXPECT_EQ(results[0], "effect1_value");
    EXPECT_EQ(results[1], "effect2_value");
}

/**
 * Property Test: Random nesting depths
 * 
 * Tests handler stack isolation with randomly generated nesting depths.
 * This is a property-based test that runs multiple iterations.
 */
TEST_F(HandlerStackIsolationTest, PropertyRandomNestingDepths) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> depth_dist(1, 5);
    std::uniform_int_distribution<> effect_dist(0, 2);
    
    const std::vector<std::string> effect_names = {"Effect1", "Effect2", "Effect3"};
    const std::string operation_name = "testOp";
    
    // Run 100 iterations as per requirements
    for (int iteration = 0; iteration < 100; ++iteration) {
        // Clear handler stack
        HandlerStack::instance().clear();
        
        // Generate random nesting depth
        int depth = depth_dist(gen);
        
        // Choose a random effect
        std::string effect_name = effect_names[effect_dist(gen)];
        
        // Create handlers for each level
        std::vector<std::shared_ptr<EffectHandler>> handlers;
        std::vector<std::string> expected_values;
        
        for (int level = 0; level < depth; ++level) {
            std::string value = "level_" + std::to_string(level);
            expected_values.push_back(value);
            
            auto effect_def = std::make_shared<EffectDefinition>(effect_name);
            auto handler = std::make_shared<EffectHandler>(effect_def);
            handler->set_handler(operation_name,
                [value](const std::vector<Value>& args, Continuation& cont) -> Value {
                    return cont.resume(Value(std::make_shared<String>(value)));
                });
            handlers.push_back(handler);
        }
        
        // Build nested handle() calls recursively
        std::function<Value(int)> build_nested = [&](int level) -> Value {
            if (level >= depth) {
                // Innermost level - perform the effect
                return effects::perform(effect_name, operation_name, {});
            } else {
                // Nest another handler
                return handle(effect_name, handlers[level], [&]() -> Value {
                    return build_nested(level + 1);
                });
            }
        };
        
        // Execute nested handlers
        std::string result = build_nested(0).try_as<String>().value()->value();
        
        // Verify innermost handler was used
        std::string expected = expected_values[depth - 1];
        EXPECT_EQ(result, expected) 
            << "Iteration " << iteration 
            << ": depth=" << depth 
            << ", effect=" << effect_name;
    }
}

/**
 * Property Test: Handler isolation with exceptions
 * 
 * Tests that handler stack isolation is maintained even when exceptions occur.
 */
TEST_F(HandlerStackIsolationTest, PropertyHandlerIsolationWithExceptions) {
    const std::string effect_name = "TestEffect";
    const std::string operation_name = "testOp";
    
    // Run 50 iterations
    for (int iteration = 0; iteration < 50; ++iteration) {
        // Clear handler stack
        HandlerStack::instance().clear();
        
        // Create outer handler
        auto outer_effect_def = std::make_shared<EffectDefinition>(effect_name);
        auto outer_handler = std::make_shared<EffectHandler>(outer_effect_def);
        outer_handler->set_handler(operation_name,
            [](const std::vector<Value>& args, Continuation& cont) -> Value {
                return cont.resume(Value(std::make_shared<String>("outer_value")));
            });
        
        // Create inner handler that throws
        auto inner_effect_def = std::make_shared<EffectDefinition>(effect_name);
        auto inner_handler = std::make_shared<EffectHandler>(inner_effect_def);
        inner_handler->set_handler(operation_name,
            [](const std::vector<Value>& args, Continuation& cont) -> Value {
                throw std::runtime_error("Inner handler error");
            });
        
        // Test exception handling
        try {
            handle(effect_name, outer_handler, [&]() -> Value {
                try {
                    handle(effect_name, inner_handler, [&]() -> Value {
                        // This should throw
                        return effects::perform(effect_name, operation_name, {});
                    });
                } catch (const std::runtime_error&) {
                    // Inner handler threw - now outer handler should be active
                    std::string result = effects::perform(effect_name, operation_name, {})
                        .try_as<String>().value()->value();
                    EXPECT_EQ(result, "outer_value");
                }
                return Value(Empty::instance());
            });
        } catch (...) {
            FAIL() << "Unexpected exception in iteration " << iteration;
        }
    }
}

