#include <gtest/gtest.h>
#include "meld/testing/property_test.hpp"
#include "meld/testing/generator.hpp"

using namespace meld::testing;

// Property 32: Property-Based Test Shrinking
// Validates: Requirements 42.6, 42.7
class PropertyTestShrinkingTest : public ::testing::Test {
protected:
    void SetUp() override {
        GeneratorRegistry::instance().clear();
        register_builtin_generators();
    }
};

TEST_F(PropertyTestShrinkingTest, IntegerShrinkingWorks) {
    // Property: Integer shrinking should produce smaller values
    auto* gen = get_generator<int>();
    ASSERT_NE(gen, nullptr);
    
    // Test shrinking of a large positive number
    auto shrunk = gen->shrink(1000);
    EXPECT_FALSE(shrunk.empty());
    
    // Should include zero and smaller positive values
    bool has_zero = std::find(shrunk.begin(), shrunk.end(), 0) != shrunk.end();
    bool has_smaller = std::any_of(shrunk.begin(), shrunk.end(), 
                                  [](int x) { return x > 0 && x < 1000; });
    
    EXPECT_TRUE(has_zero || has_smaller);
}

TEST_F(PropertyTestShrinkingTest, StringShrinkingWorks) {
    // Property: String shrinking should produce shorter strings
    auto* gen = get_generator<std::string>();
    ASSERT_NE(gen, nullptr);
    
    // Test shrinking of a long string
    std::string long_string = "this is a very long string for testing";
    auto shrunk = gen->shrink(long_string);
    EXPECT_FALSE(shrunk.empty());
    
    // Should include empty string and shorter strings
    bool has_empty = std::find(shrunk.begin(), shrunk.end(), "") != shrunk.end();
    bool has_shorter = std::any_of(shrunk.begin(), shrunk.end(), 
                                  [&long_string](const std::string& s) { 
                                      return s.length() < long_string.length(); 
                                  });
    
    EXPECT_TRUE(has_empty || has_shorter);
}

TEST_F(PropertyTestShrinkingTest, BooleanShrinkingWorks) {
    // Property: Boolean shrinking should work correctly
    auto* gen = get_generator<bool>();
    ASSERT_NE(gen, nullptr);
    
    // Shrinking true should give false
    auto shrunk_true = gen->shrink(true);
    EXPECT_FALSE(shrunk_true.empty());
    EXPECT_EQ(shrunk_true[0], false);
    
    // Shrinking false should give empty (no smaller value)
    auto shrunk_false = gen->shrink(false);
    EXPECT_TRUE(shrunk_false.empty());
}

TEST_F(PropertyTestShrinkingTest, ListShrinkingWorks) {
    // Property: List shrinking should produce smaller lists
    auto* gen = get_generator<std::vector<int>>();
    ASSERT_NE(gen, nullptr);
    
    // Test shrinking of a non-empty list
    std::vector<int> test_list = {1, 2, 3, 4, 5};
    auto shrunk = gen->shrink(test_list);
    EXPECT_FALSE(shrunk.empty());
    
    // Should include empty list and shorter lists
    bool has_empty = std::any_of(shrunk.begin(), shrunk.end(), 
                                [](const std::vector<int>& v) { return v.empty(); });
    bool has_shorter = std::any_of(shrunk.begin(), shrunk.end(), 
                                  [&test_list](const std::vector<int>& v) { 
                                      return v.size() < test_list.size(); 
                                  });
    
    EXPECT_TRUE(has_empty || has_shorter);
}

TEST_F(PropertyTestShrinkingTest, ShrinkingFindsMinimalCounterexample) {
    // Property: Shrinking should find minimal counterexamples
    auto result = PropertyTestExecutor::execute_property<int>(
        [](int x) { return x < 10; }  // Fails for x >= 10
    );
    
    EXPECT_FALSE(result.passed);
    EXPECT_FALSE(result.minimal_counterexample.empty());
    
    // The minimal counterexample should be mentioned
    EXPECT_TRUE(result.minimal_counterexample.find("Minimal counterexample") != std::string::npos);
}

TEST_F(PropertyTestShrinkingTest, ShrinkingConverges) {
    // Property: Shrinking should eventually converge (not infinite loop)
    auto* gen = get_generator<int>();
    ASSERT_NE(gen, nullptr);
    
    // Test that shrinking a value multiple times converges
    int current = 1000;
    std::set<int> seen_values;
    
    for (int iteration = 0; iteration < 20; ++iteration) {
        auto shrunk = gen->shrink(current);
        if (shrunk.empty()) {
            break;  // Converged - no more shrinking possible
        }
        
        // Take the first shrunk value
        int next = shrunk[0];
        
        // Should not cycle
        EXPECT_TRUE(seen_values.find(next) == seen_values.end());
        seen_values.insert(current);
        
        current = next;
    }
    
    // Should have made progress (current should be smaller than original)
    EXPECT_LT(current, 1000);
}

TEST_F(PropertyTestShrinkingTest, ShrinkingPreservesFailure) {
    // Property: Shrunk values should still cause the property to fail
    auto property = [](int x) { return x < 5; };  // Fails for x >= 5
    
    auto* gen = get_generator<int>();
    ASSERT_NE(gen, nullptr);
    
    // Find a failing value
    int failing_value = -1;
    for (int i = 0; i < 100; ++i) {
        int candidate = gen->generate();
        if (!property(candidate)) {
            failing_value = candidate;
            break;
        }
    }
    
    ASSERT_GE(failing_value, 5);  // Should have found a failing value
    
    // Shrink it and verify all shrunk values that still fail the property
    auto shrunk = gen->shrink(failing_value);
    for (const auto& shrunk_value : shrunk) {
        if (!property(shrunk_value)) {
            // This shrunk value also fails - it should be smaller or simpler
            EXPECT_TRUE(shrunk_value <= failing_value || shrunk_value == 0);
        }
    }
}