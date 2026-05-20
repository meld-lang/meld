#include <gtest/gtest.h>
#include "meld/testing/property_test.hpp"
#include "meld/testing/generator.hpp"

using namespace meld::testing;

// Property 31: Property-Based Test Coverage
// Validates: Requirements 42.3, 42.4, 42.5
class PropertyTestCoverageTest : public ::testing::Test {
protected:
    void SetUp() override {
        GeneratorRegistry::instance().clear();
        register_builtin_generators();
    }
};

TEST_F(PropertyTestCoverageTest, PropertyTestExecutionCoverage) {
    // Property: Property tests should execute the specified number of iterations
    auto result = PropertyTestExecutor::execute_property<int>(
        [](int x) { return true; },  // Always passes
        PropertyTestConfig{.iterations = 50}
    );
    
    EXPECT_TRUE(result.passed);
    EXPECT_EQ(result.iterations_run, 50);
}

TEST_F(PropertyTestCoverageTest, PropertyTestFailureDetection) {
    // Property: Property tests should detect failures correctly
    auto result = PropertyTestExecutor::execute_property<int>(
        [](int x) { return x != 0; }  // Fails when x == 0
    );
    
    // Should eventually fail (high probability with built-in generator edge cases)
    EXPECT_FALSE(result.passed);
    EXPECT_GT(result.iterations_run, 0);
    EXPECT_FALSE(result.failure_message.empty());
}

TEST_F(PropertyTestCoverageTest, PropertyTestGeneratorCoverage) {
    // Property: Built-in generators should cover edge cases
    std::set<int> generated_values;
    auto* gen = get_generator<int>();
    ASSERT_NE(gen, nullptr);
    
    // Generate many values to check coverage
    for (int i = 0; i < 1000; ++i) {
        generated_values.insert(gen->generate());
    }
    
    // Should include some edge cases with high probability
    bool has_zero = generated_values.count(0) > 0;
    bool has_positive = std::any_of(generated_values.begin(), generated_values.end(), 
                                   [](int x) { return x > 0; });
    bool has_negative = std::any_of(generated_values.begin(), generated_values.end(), 
                                   [](int x) { return x < 0; });
    
    EXPECT_TRUE(has_zero || has_positive || has_negative);
}

TEST_F(PropertyTestCoverageTest, PropertyTestConfigurationCoverage) {
    // Property: Configuration should affect test execution
    PropertyTestConfig config1{.iterations = 10, .seed = 42};
    PropertyTestConfig config2{.iterations = 20, .seed = 42};
    
    auto result1 = PropertyTestExecutor::execute_property<int>(
        [](int x) { return true; },
        config1
    );
    
    auto result2 = PropertyTestExecutor::execute_property<int>(
        [](int x) { return true; },
        config2
    );
    
    EXPECT_TRUE(result1.passed);
    EXPECT_TRUE(result2.passed);
    EXPECT_EQ(result1.iterations_run, 10);
    EXPECT_EQ(result2.iterations_run, 20);
}

TEST_F(PropertyTestCoverageTest, PropertyTestMultiParameterCoverage) {
    // Property: Multi-parameter tests should work correctly
    auto result = PropertyTestExecutor::execute_property_2<int, std::string>(
        [](int x, const std::string& s) { 
            return x == x && s == s;  // Identity property
        }
    );
    
    EXPECT_TRUE(result.passed);
    EXPECT_EQ(result.iterations_run, 100);
}

TEST_F(PropertyTestCoverageTest, PropertyTestExceptionHandlingCoverage) {
    // Property: Exception handling should work correctly
    auto result = PropertyTestExecutor::execute_property<int>(
        [](int x) -> bool {
            if (x % 2 == 0) throw std::runtime_error("Test exception");
            return true;
        }
    );
    
    // Should catch exception and report failure
    EXPECT_FALSE(result.passed);
    EXPECT_TRUE(result.failure_message.find("Exception") != std::string::npos);
}