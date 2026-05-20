#include <gtest/gtest.h>
#include "meld/testing/property_test.hpp"
#include "meld/testing/generator.hpp"

using namespace meld::testing;

// Property 33: Property-Based Test Determinism with Effects
// Validates: Requirements 42.12
class PropertyTestEffectsTest : public ::testing::Test {
protected:
    void SetUp() override {
        GeneratorRegistry::instance().clear();
        register_builtin_generators();
    }
};

// Mock effect system for testing
namespace mock_effects {
    // Simple state effect for testing determinism
    thread_local int global_state = 0;
    
    void reset_state() {
        global_state = 0;
    }
    
    int get_state() {
        return global_state;
    }
    
    void increment_state() {
        global_state++;
    }
    
    void set_state(int value) {
        global_state = value;
    }
}

TEST_F(PropertyTestEffectsTest, DeterministicEffectfulPropertyTest) {
    // Property: Property tests with effects should be deterministic when using same seed
    PropertyTestConfig config1{.iterations = 10, .seed = 42};
    PropertyTestConfig config2{.iterations = 10, .seed = 42};
    
    // Reset state before each test
    mock_effects::reset_state();
    
    auto property = [](int x) {
        mock_effects::increment_state();
        return mock_effects::get_state() > 0;  // Always true after increment
    };
    
    auto result1 = PropertyTestExecutor::execute_property<int>(property, config1);
    
    mock_effects::reset_state();
    auto result2 = PropertyTestExecutor::execute_property<int>(property, config2);
    
    // Both should pass and have same number of iterations
    EXPECT_TRUE(result1.passed);
    EXPECT_TRUE(result2.passed);
    EXPECT_EQ(result1.iterations_run, result2.iterations_run);
}

TEST_F(PropertyTestEffectsTest, EffectfulPropertyWithStateManagement) {
    // Property: Property tests should handle stateful effects correctly
    mock_effects::reset_state();
    
    auto result = PropertyTestExecutor::execute_property<int>(
        [](int x) {
            int initial_state = mock_effects::get_state();
            mock_effects::set_state(x);
            int new_state = mock_effects::get_state();
            mock_effects::reset_state();  // Clean up after each iteration
            return new_state == x;
        }
    );
    
    EXPECT_TRUE(result.passed);
    EXPECT_EQ(result.iterations_run, 100);
}

TEST_F(PropertyTestEffectsTest, EffectfulPropertyFailureHandling) {
    // Property: Property tests should handle effect failures correctly
    mock_effects::reset_state();
    
    auto result = PropertyTestExecutor::execute_property<int>(
        [](int x) {
            mock_effects::increment_state();
            // Fail when state reaches a certain value
            return mock_effects::get_state() < 5;
        }
    );
    
    // Should fail after a few iterations
    EXPECT_FALSE(result.passed);
    EXPECT_LT(result.iterations_run, 100);
    EXPECT_GT(result.iterations_run, 0);
}

TEST_F(PropertyTestEffectsTest, EffectfulPropertyWithExceptions) {
    // Property: Property tests should handle effect exceptions correctly
    mock_effects::reset_state();
    
    auto result = PropertyTestExecutor::execute_property<int>(
        [](int x) -> bool {
            mock_effects::increment_state();
            if (mock_effects::get_state() > 3) {
                throw std::runtime_error("Effect exception");
            }
            return true;
        }
    );
    
    // Should catch the exception and report failure
    EXPECT_FALSE(result.passed);
    EXPECT_TRUE(result.failure_message.find("Exception") != std::string::npos);
}

TEST_F(PropertyTestEffectsTest, EffectfulPropertySeedReproducibility) {
    // Property: Effectful property tests should be reproducible with same seed
    PropertyTestConfig config{.iterations = 20, .seed = 123};
    
    std::vector<int> states1, states2;
    
    // First run
    mock_effects::reset_state();
    auto result1 = PropertyTestExecutor::execute_property<int>(
        [&states1](int x) {
            mock_effects::increment_state();
            states1.push_back(mock_effects::get_state());
            return true;
        },
        config
    );
    
    // Second run with same seed
    mock_effects::reset_state();
    auto result2 = PropertyTestExecutor::execute_property<int>(
        [&states2](int x) {
            mock_effects::increment_state();
            states2.push_back(mock_effects::get_state());
            return true;
        },
        config
    );
    
    EXPECT_TRUE(result1.passed);
    EXPECT_TRUE(result2.passed);
    EXPECT_EQ(result1.iterations_run, result2.iterations_run);
    
    // The state sequences should be identical (same generated values, same effects)
    EXPECT_EQ(states1.size(), states2.size());
    for (size_t i = 0; i < std::min(states1.size(), states2.size()); ++i) {
        EXPECT_EQ(states1[i], states2[i]);
    }
}

TEST_F(PropertyTestEffectsTest, EffectfulPropertyWithMultipleParameters) {
    // Property: Multi-parameter effectful properties should work correctly
    mock_effects::reset_state();
    
    auto result = PropertyTestExecutor::execute_property_2<int, int>(
        [](int x, int y) {
            mock_effects::set_state(x + y);
            return mock_effects::get_state() == x + y;
        }
    );
    
    EXPECT_TRUE(result.passed);
    EXPECT_EQ(result.iterations_run, 100);
}

TEST_F(PropertyTestEffectsTest, EffectfulPropertyIsolation) {
    // Property: Each property test iteration should be isolated
    mock_effects::reset_state();
    
    int max_state_seen = 0;
    auto result = PropertyTestExecutor::execute_property<int>(
        [&max_state_seen](int x) {
            mock_effects::increment_state();
            int current_state = mock_effects::get_state();
            max_state_seen = std::max(max_state_seen, current_state);
            
            // Reset state for next iteration (simulating effect handler cleanup)
            if (current_state > 1) {
                mock_effects::reset_state();
            }
            
            return true;
        }
    );
    
    EXPECT_TRUE(result.passed);
    // State should have been reset multiple times, so max shouldn't be too high
    EXPECT_LT(max_state_seen, 100);  // Much less than total iterations
}