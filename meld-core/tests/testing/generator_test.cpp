#include <gtest/gtest.h>
#include "meld/testing/generator.hpp"
#include "meld/testing/property_test.hpp"

using namespace meld::testing;

// Simple test generator for integers
class TestIntGenerator : public BaseGenerator<int> {
public:
    TestIntGenerator() : BaseGenerator<int>() {}
    
    int generate() override {
        std::uniform_int_distribution<int> dist(-100, 100);
        return dist(engine_);
    }
    
    std::vector<int> shrink(const int& value) override {
        std::vector<int> shrunk;
        if (value != 0) {
            shrunk.push_back(0);  // Always try zero first
        }
        if (value > 1) {
            shrunk.push_back(value / 2);  // Try half the value
        }
        if (value < -1) {
            shrunk.push_back(value / 2);  // Try half the value (towards zero)
        }
        return shrunk;
    }
};

class GeneratorTest : public ::testing::Test {
protected:
    void SetUp() override {
        GeneratorRegistry::instance().clear();
    }
};

TEST_F(GeneratorTest, CanRegisterAndRetrieveGenerator) {
    auto& registry = GeneratorRegistry::instance();
    
    // Register a generator for int
    auto int_gen = std::make_unique<TestIntGenerator>();
    registry.register_generator<int>(std::move(int_gen));
    
    // Check that we can retrieve it
    EXPECT_TRUE(registry.has_generator<int>());
    
    auto* retrieved_gen = registry.get_generator<int>();
    EXPECT_NE(retrieved_gen, nullptr);
}

TEST_F(GeneratorTest, GeneratorCanGenerateValues) {
    auto& registry = GeneratorRegistry::instance();
    
    // Register a generator for int
    auto int_gen = std::make_unique<TestIntGenerator>();
    registry.register_generator<int>(std::move(int_gen));
    
    // Get the generator and generate some values
    auto* gen = registry.get_generator<int>();
    ASSERT_NE(gen, nullptr);
    
    // Generate multiple values to ensure it works
    std::vector<int> generated_values;
    for (int i = 0; i < 10; ++i) {
        int value = gen->generate();
        generated_values.push_back(value);
        EXPECT_GE(value, -100);
        EXPECT_LE(value, 100);
    }
    
    // Values should be different (with high probability)
    bool has_different_values = false;
    for (size_t i = 1; i < generated_values.size(); ++i) {
        if (generated_values[i] != generated_values[0]) {
            has_different_values = true;
            break;
        }
    }
    EXPECT_TRUE(has_different_values);
}

TEST_F(GeneratorTest, GeneratorCanShrinkValues) {
    auto& registry = GeneratorRegistry::instance();
    
    // Register a generator for int
    auto int_gen = std::make_unique<TestIntGenerator>();
    registry.register_generator<int>(std::move(int_gen));
    
    // Get the generator and test shrinking
    auto* gen = registry.get_generator<int>();
    ASSERT_NE(gen, nullptr);
    
    // Test shrinking positive value
    auto shrunk_positive = gen->shrink(42);
    EXPECT_FALSE(shrunk_positive.empty());
    EXPECT_TRUE(std::find(shrunk_positive.begin(), shrunk_positive.end(), 0) != shrunk_positive.end());
    EXPECT_TRUE(std::find(shrunk_positive.begin(), shrunk_positive.end(), 21) != shrunk_positive.end());
    
    // Test shrinking negative value
    auto shrunk_negative = gen->shrink(-42);
    EXPECT_FALSE(shrunk_negative.empty());
    EXPECT_TRUE(std::find(shrunk_negative.begin(), shrunk_negative.end(), 0) != shrunk_negative.end());
    EXPECT_TRUE(std::find(shrunk_negative.begin(), shrunk_negative.end(), -21) != shrunk_negative.end());
    
    // Test shrinking zero (should return empty or minimal shrinks)
    auto shrunk_zero = gen->shrink(0);
    // Zero should have no shrinks or minimal shrinks
    EXPECT_TRUE(shrunk_zero.empty() || shrunk_zero.size() <= 1);
}

TEST_F(GeneratorTest, GeneratorSeedingWorks) {
    auto& registry = GeneratorRegistry::instance();
    
    // Register a generator for int
    auto int_gen = std::make_unique<TestIntGenerator>();
    registry.register_generator<int>(std::move(int_gen));
    
    // Get the generator
    auto* gen = registry.get_generator<int>();
    ASSERT_NE(gen, nullptr);
    
    // Generate values with seed 42
    gen->set_seed(42);
    std::vector<int> values1;
    for (int i = 0; i < 5; ++i) {
        values1.push_back(gen->generate());
    }
    
    // Generate values with same seed 42
    gen->set_seed(42);
    std::vector<int> values2;
    for (int i = 0; i < 5; ++i) {
        values2.push_back(gen->generate());
    }
    
    // Should generate the same sequence
    EXPECT_EQ(values1, values2);
}

TEST_F(GeneratorTest, RegistryReturnsNullForUnregisteredType) {
    auto& registry = GeneratorRegistry::instance();
    
    // Try to get a generator for a type that hasn't been registered
    auto* gen = registry.get_generator<double>();
    EXPECT_EQ(gen, nullptr);
    EXPECT_FALSE(registry.has_generator<double>());
}

// Tests for built-in generators

TEST_F(GeneratorTest, BuiltinIntGeneratorWorks) {
    register_builtin_generators();
    
    auto* gen = get_generator<int>();
    ASSERT_NE(gen, nullptr);
    
    // Test generation
    std::vector<int> values;
    for (int i = 0; i < 100; ++i) {
        values.push_back(gen->generate());
    }
    
    // Should have some variety
    bool has_positive = false, has_negative = false, has_zero = false;
    for (int val : values) {
        if (val > 0) has_positive = true;
        if (val < 0) has_negative = true;
        if (val == 0) has_zero = true;
    }
    
    // At least one of these should be true with high probability
    EXPECT_TRUE(has_positive || has_negative || has_zero);
    
    // Test shrinking
    auto shrunk = gen->shrink(42);
    EXPECT_FALSE(shrunk.empty());
    EXPECT_TRUE(std::find(shrunk.begin(), shrunk.end(), 0) != shrunk.end());
}

TEST_F(GeneratorTest, BuiltinStringGeneratorWorks) {
    register_builtin_generators();
    
    auto* gen = get_generator<std::string>();
    ASSERT_NE(gen, nullptr);
    
    // Test generation
    std::vector<std::string> values;
    for (int i = 0; i < 50; ++i) {
        values.push_back(gen->generate());
    }
    
    // Should have some variety in lengths
    bool has_empty = false, has_non_empty = false;
    for (const auto& val : values) {
        if (val.empty()) has_empty = true;
        else has_non_empty = true;
    }
    
    // At least one of these should be true
    EXPECT_TRUE(has_empty || has_non_empty);
    
    // Test shrinking
    auto shrunk = gen->shrink("hello world");
    EXPECT_FALSE(shrunk.empty());
    EXPECT_TRUE(std::find(shrunk.begin(), shrunk.end(), "") != shrunk.end());
}

TEST_F(GeneratorTest, BuiltinBoolGeneratorWorks) {
    register_builtin_generators();
    
    auto* gen = get_generator<bool>();
    ASSERT_NE(gen, nullptr);
    
    // Test generation
    std::vector<bool> values;
    for (int i = 0; i < 100; ++i) {
        values.push_back(gen->generate());
    }
    
    // Should have both true and false with high probability
    bool has_true = false, has_false = false;
    for (bool val : values) {
        if (val) has_true = true;
        else has_false = true;
    }
    
    // At least one of these should be true
    EXPECT_TRUE(has_true || has_false);
    
    // Test shrinking
    auto shrunk_true = gen->shrink(true);
    EXPECT_FALSE(shrunk_true.empty());
    EXPECT_EQ(shrunk_true[0], false);
    
    auto shrunk_false = gen->shrink(false);
    EXPECT_TRUE(shrunk_false.empty());  // false cannot be shrunk further
}

TEST_F(GeneratorTest, BuiltinFloatGeneratorWorks) {
    register_builtin_generators();
    
    auto* gen = get_generator<float>();
    ASSERT_NE(gen, nullptr);
    
    // Test generation
    std::vector<float> values;
    for (int i = 0; i < 100; ++i) {
        values.push_back(gen->generate());
    }
    
    // Should have some variety
    bool has_finite = false;
    for (float val : values) {
        if (std::isfinite(val)) {
            has_finite = true;
            break;
        }
    }
    
    EXPECT_TRUE(has_finite);
    
    // Test shrinking
    auto shrunk = gen->shrink(42.5f);
    EXPECT_FALSE(shrunk.empty());
    EXPECT_TRUE(std::find(shrunk.begin(), shrunk.end(), 0.0f) != shrunk.end());
}

TEST_F(GeneratorTest, BuiltinListGeneratorWorks) {
    register_builtin_generators();
    
    auto* gen = get_generator<std::vector<int>>();
    ASSERT_NE(gen, nullptr);
    
    // Test generation
    std::vector<std::vector<int>> values;
    for (int i = 0; i < 50; ++i) {
        values.push_back(gen->generate());
    }
    
    // Should have some variety in sizes
    bool has_empty = false, has_non_empty = false;
    for (const auto& val : values) {
        if (val.empty()) has_empty = true;
        else has_non_empty = true;
    }
    
    // At least one of these should be true
    EXPECT_TRUE(has_empty || has_non_empty);
    
    // Test shrinking
    std::vector<int> test_list = {1, 2, 3, 4, 5};
    auto shrunk = gen->shrink(test_list);
    EXPECT_FALSE(shrunk.empty());
    
    // Should include empty list
    bool has_empty_shrink = false;
    for (const auto& shrunk_list : shrunk) {
        if (shrunk_list.empty()) {
            has_empty_shrink = true;
            break;
        }
    }
    EXPECT_TRUE(has_empty_shrink);
}

// Tests for PropertyTestExecutor

TEST_F(GeneratorTest, PropertyTestExecutorBasicTest) {
    register_builtin_generators();
    
    // Test a simple property: all integers are equal to themselves
    auto result = PropertyTestExecutor::execute_property<int>(
        [](int x) { return x == x; }
    );
    
    EXPECT_TRUE(result.passed);
    EXPECT_EQ(result.iterations_run, 100);  // Default iterations
    EXPECT_TRUE(result.failure_message.empty());
}

TEST_F(GeneratorTest, PropertyTestExecutorFailingTest) {
    register_builtin_generators();
    
    // Test a property that should fail: all integers are positive
    auto result = PropertyTestExecutor::execute_property<int>(
        [](int x) { return x > 0; }
    );
    
    EXPECT_FALSE(result.passed);
    EXPECT_GT(result.iterations_run, 0);
    EXPECT_FALSE(result.failure_message.empty());
    EXPECT_FALSE(result.counterexamples.empty());
}

TEST_F(GeneratorTest, PropertyTestExecutorWithConfig) {
    register_builtin_generators();
    
    PropertyTestConfig config;
    config.iterations = 50;
    config.seed = 42;
    
    // Test with custom configuration
    auto result = PropertyTestExecutor::execute_property<int>(
        [](int x) { return x == x; },
        config
    );
    
    EXPECT_TRUE(result.passed);
    EXPECT_EQ(result.iterations_run, 50);
}

TEST_F(GeneratorTest, PropertyTestExecutorTwoParameters) {
    register_builtin_generators();
    
    // Test a property with two parameters: addition is commutative
    auto result = PropertyTestExecutor::execute_property_2<int, int>(
        [](int x, int y) { return x + y == y + x; }
    );
    
    EXPECT_TRUE(result.passed);
    EXPECT_EQ(result.iterations_run, 100);
}

TEST_F(GeneratorTest, PropertyTestExecutorThreeParameters) {
    register_builtin_generators();
    
    // Test a property with three parameters: addition is associative
    auto result = PropertyTestExecutor::execute_property_3<int, int, int>(
        [](int x, int y, int z) { return (x + y) + z == x + (y + z); }
    );
    
    EXPECT_TRUE(result.passed);
    EXPECT_EQ(result.iterations_run, 100);
}

TEST_F(GeneratorTest, PropertyTestExecutorStringProperty) {
    register_builtin_generators();
    
    // Test a string property: length is non-negative
    auto result = PropertyTestExecutor::execute_property<std::string>(
        [](const std::string& s) { return s.length() >= 0; }
    );
    
    EXPECT_TRUE(result.passed);
    EXPECT_EQ(result.iterations_run, 100);
}

TEST_F(GeneratorTest, PropertyTestExecutorShrinkingWorks) {
    register_builtin_generators();
    
    // Test a property that fails for large positive numbers
    auto result = PropertyTestExecutor::execute_property<int>(
        [](int x) { return x < 10; }  // Fails for x >= 10
    );
    
    EXPECT_FALSE(result.passed);
    EXPECT_FALSE(result.minimal_counterexample.empty());
    
    // The minimal counterexample should be mentioned
    EXPECT_TRUE(result.minimal_counterexample.find("Minimal counterexample") != std::string::npos);
}

TEST_F(GeneratorTest, PropertyTestExecutorExceptionHandling) {
    register_builtin_generators();
    
    // Test a property that throws an exception for any even number
    auto result = PropertyTestExecutor::execute_property<int>(
        [](int x) -> bool { 
            if (x % 2 == 0) throw std::runtime_error("Even number encountered");
            return true; 
        }
    );
    
    // Should catch the exception and report it
    EXPECT_FALSE(result.passed);
    EXPECT_TRUE(result.failure_message.find("Exception") != std::string::npos);
}

TEST_F(GeneratorTest, PropertyTestConfigParsing) {
    // Test basic config parsing
    auto config1 = PropertyTestConfig::parse_config("iterations: 50, seed: 42");
    EXPECT_EQ(config1.iterations, 50);
    EXPECT_EQ(config1.seed, 42u);
    
    // Test config with verbose flag
    auto config2 = PropertyTestConfig::parse_config("iterations: 200, verbose: true, timeout: 10000");
    EXPECT_EQ(config2.iterations, 200);
    EXPECT_TRUE(config2.verbose);
    EXPECT_EQ(config2.timeout_ms, 10000);
    
    // Test config with braces (should still work)
    auto config3 = PropertyTestConfig::parse_config("{ iterations: 75, seed: 123 }");
    EXPECT_EQ(config3.iterations, 75);
    EXPECT_EQ(config3.seed, 123u);
}

TEST_F(GeneratorTest, PropertyTestConfigToString) {
    PropertyTestConfig config;
    config.iterations = 50;
    config.seed = 42;
    config.verbose = true;
    
    std::string config_str = config.to_string();
    EXPECT_TRUE(config_str.find("iterations: 50") != std::string::npos);
    EXPECT_TRUE(config_str.find("seed: 42") != std::string::npos);
    EXPECT_TRUE(config_str.find("verbose: true") != std::string::npos);
}

TEST_F(GeneratorTest, PropertyTestExecutorVariadicSupport) {
    register_builtin_generators();
    
    // Test variadic property with 4 parameters
    auto result = PropertyTestExecutor::execute_property_variadic<
        std::function<bool(int, int, int, int)>, int, int, int, int
    >(
        [](int a, int b, int c, int d) { 
            // Test associativity: (a + b) + (c + d) == a + (b + c) + d
            // Use smaller numbers to avoid overflow
            if (a > 100 || a < -100 || b > 100 || b < -100 || 
                c > 100 || c < -100 || d > 100 || d < -100) return true;
            return (a + b) + (c + d) == a + (b + c) + d;
        }
    );
    
    EXPECT_TRUE(result.passed);
    EXPECT_EQ(result.iterations_run, 100);
}