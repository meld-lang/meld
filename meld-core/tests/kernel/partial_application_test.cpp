#include <gtest/gtest.h>
#include "meld/kernel/partial_application.hpp"
#include "meld/kernel/operations.hpp"
#include "meld/kernel/primitives.hpp"

using namespace meld::kernel;

class PartialApplicationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a simple add function for testing
        auto add_params = std::vector<std::shared_ptr<Symbol>>{
            std::make_shared<Symbol>("a"),
            std::make_shared<Symbol>("b")
        };
        
        auto add_impl = [](const std::vector<Value>& args) -> Value {
            if (args.size() != 2) {
                return Value(std::make_shared<String>("Error: add requires 2 arguments"));
            }
            if (!args[0].is<Integer>() || !args[1].is<Integer>()) {
                return Value(std::make_shared<String>("Error: add requires integer arguments"));
            }
            auto a = args[0].as<Integer>()->value();
            auto b = args[1].as<Integer>()->value();
            return Value(std::make_shared<Integer>(a + b));
        };
        
        add_fn = Value(std::make_shared<Function>(
            add_params,
            Value(),
            add_impl,
            std::make_optional(std::string("add"))
        ));
        
        // Create a three-parameter multiply function
        auto mult_params = std::vector<std::shared_ptr<Symbol>>{
            std::make_shared<Symbol>("a"),
            std::make_shared<Symbol>("b"),
            std::make_shared<Symbol>("c")
        };
        
        auto mult_impl = [](const std::vector<Value>& args) -> Value {
            if (args.size() != 3) {
                return Value(std::make_shared<String>("Error: multiply requires 3 arguments"));
            }
            if (!args[0].is<Integer>() || !args[1].is<Integer>() || !args[2].is<Integer>()) {
                return Value(std::make_shared<String>("Error: multiply requires integer arguments"));
            }
            auto a = args[0].as<Integer>()->value();
            auto b = args[1].as<Integer>()->value();
            auto c = args[2].as<Integer>()->value();
            return Value(std::make_shared<Integer>(a * b * c));
        };
        
        mult_fn = Value(std::make_shared<Function>(
            mult_params,
            Value(),
            mult_impl,
            std::make_optional(std::string("multiply"))
        ));
    }
    
    Value add_fn;
    Value mult_fn;
};

// Test placeholder creation
TEST_F(PartialApplicationTest, CreatePlaceholder) {
    auto placeholder = make_placeholder();
    EXPECT_TRUE(is_placeholder(placeholder));
    EXPECT_EQ(placeholder.to_string(), "_");
}

// Test placeholder detection
TEST_F(PartialApplicationTest, PlaceholderDetection) {
    auto placeholder = make_placeholder();
    auto number = Value(std::make_shared<Integer>(42));
    
    EXPECT_TRUE(is_placeholder(placeholder));
    EXPECT_FALSE(is_placeholder(number));
}

// Test partial application with first argument bound
TEST_F(PartialApplicationTest, PartialApplyFirstArgument) {
    // Create add(5, _)
    std::vector<Value> args = {
        Value(std::make_shared<Integer>(5)),
        make_placeholder()
    };
    
    auto result = partial_apply(add_fn, args);
    ASSERT_TRUE(result.has_value()) << result.error();
    
    auto partial_fn = result.value();
    EXPECT_TRUE(partial_fn.is<Function>());
    
    // The partially applied function should have 1 parameter
    auto func = partial_fn.as<Function>();
    EXPECT_EQ(func->params().size(), 1);
}

// Test partial application with second argument bound
TEST_F(PartialApplicationTest, PartialApplySecondArgument) {
    // Create add(_, 10)
    std::vector<Value> args = {
        make_placeholder(),
        Value(std::make_shared<Integer>(10))
    };
    
    auto result = partial_apply(add_fn, args);
    ASSERT_TRUE(result.has_value()) << result.error();
    
    auto partial_fn = result.value();
    EXPECT_TRUE(partial_fn.is<Function>());
    
    auto func = partial_fn.as<Function>();
    EXPECT_EQ(func->params().size(), 1);
}

// Test partial application with multiple placeholders
TEST_F(PartialApplicationTest, PartialApplyMultiplePlaceholders) {
    // Create multiply(2, _, _)
    std::vector<Value> args = {
        Value(std::make_shared<Integer>(2)),
        make_placeholder(),
        make_placeholder()
    };
    
    auto result = partial_apply(mult_fn, args);
    ASSERT_TRUE(result.has_value()) << result.error();
    
    auto partial_fn = result.value();
    EXPECT_TRUE(partial_fn.is<Function>());
    
    auto func = partial_fn.as<Function>();
    EXPECT_EQ(func->params().size(), 2);
}

// Test partial application with no placeholders (all arguments bound)
TEST_F(PartialApplicationTest, PartialApplyNoPlaceholders) {
    // Create add(5, 10) - should just call the function
    std::vector<Value> args = {
        Value(std::make_shared<Integer>(5)),
        Value(std::make_shared<Integer>(10))
    };
    
    auto result = partial_apply(add_fn, args);
    ASSERT_TRUE(result.has_value()) << result.error();
    
    // Should return the result of the function call
    // Note: This test may need adjustment based on implementation details
}

// Test partial application with too many arguments
TEST_F(PartialApplicationTest, PartialApplyTooManyArguments) {
    std::vector<Value> args = {
        Value(std::make_shared<Integer>(1)),
        Value(std::make_shared<Integer>(2)),
        Value(std::make_shared<Integer>(3))
    };
    
    auto result = partial_apply(add_fn, args);
    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().find("too many arguments") != std::string::npos);
}

// Test partial application on non-function
TEST_F(PartialApplicationTest, PartialApplyNonFunction) {
    auto not_a_function = Value(std::make_shared<Integer>(42));
    std::vector<Value> args = { make_placeholder() };
    
    auto result = partial_apply(not_a_function, args);
    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().find("must be a function") != std::string::npos);
}

// Test currying a two-parameter function
TEST_F(PartialApplicationTest, CurryTwoParameterFunction) {
    auto result = curry(add_fn);
    ASSERT_TRUE(result.has_value()) << result.error();
    
    auto curried_fn = result.value();
    EXPECT_TRUE(curried_fn.is<Function>());
    EXPECT_TRUE(is_curried(curried_fn));
    
    // Curried function should have 1 parameter
    auto func = curried_fn.as<Function>();
    EXPECT_EQ(func->params().size(), 1);
}

// Test currying a three-parameter function
TEST_F(PartialApplicationTest, CurryThreeParameterFunction) {
    auto result = curry(mult_fn);
    ASSERT_TRUE(result.has_value()) << result.error();
    
    auto curried_fn = result.value();
    EXPECT_TRUE(curried_fn.is<Function>());
    EXPECT_TRUE(is_curried(curried_fn));
    
    auto func = curried_fn.as<Function>();
    EXPECT_EQ(func->params().size(), 1);
}

// Test currying a single-parameter function (already curried)
TEST_F(PartialApplicationTest, CurrySingleParameterFunction) {
    auto single_param = std::vector<std::shared_ptr<Symbol>>{
        std::make_shared<Symbol>("x")
    };
    
    auto single_fn = Value(std::make_shared<Function>(
        single_param,
        Value(),
        [](const std::vector<Value>& args) -> Value {
            return args[0];
        }
    ));
    
    auto result = curry(single_fn);
    ASSERT_TRUE(result.has_value()) << result.error();
    
    // Should return the same function
    EXPECT_TRUE(result.value().is<Function>());
}

// Test currying a zero-parameter function
TEST_F(PartialApplicationTest, CurryZeroParameterFunction) {
    auto zero_params = std::vector<std::shared_ptr<Symbol>>{};
    
    auto zero_fn = Value(std::make_shared<Function>(
        zero_params,
        Value(),
        [](const std::vector<Value>&) -> Value {
            return Value(std::make_shared<Integer>(42));
        }
    ));
    
    auto result = curry(zero_fn);
    ASSERT_TRUE(result.has_value()) << result.error();
    
    // Should return the same function
    EXPECT_TRUE(result.value().is<Function>());
}

// Test currying on non-function
TEST_F(PartialApplicationTest, CurryNonFunction) {
    auto not_a_function = Value(std::make_shared<Integer>(42));
    
    auto result = curry(not_a_function);
    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().find("must be a function") != std::string::npos);
}

// Test is_curried on regular function
TEST_F(PartialApplicationTest, IsCurriedRegularFunction) {
    EXPECT_FALSE(is_curried(add_fn));
}

// Test is_curried on non-function
TEST_F(PartialApplicationTest, IsCurriedNonFunction) {
    auto not_a_function = Value(std::make_shared<Integer>(42));
    EXPECT_FALSE(is_curried(not_a_function));
}

// Property-based test: Partial application preserves function behavior
TEST_F(PartialApplicationTest, PartialApplicationPreservesBehavior) {
    // Property: For any function f(a, b) and values x, y:
    // partial_apply(f, [x, _])(y) should equal f(x, y)
    
    for (int i = 0; i < 50; ++i) {
        int x = (i * 17) % 100;  // Generate test values
        int y = (i * 23) % 100;
        
        // Apply original function
        auto original_result = apply(add_fn, {
            Value(std::make_shared<Integer>(x)),
            Value(std::make_shared<Integer>(y))
        });
        
        // Apply partial function
        std::vector<Value> partial_args = {
            Value(std::make_shared<Integer>(x)),
            make_placeholder()
        };
        
        auto partial_result = partial_apply(add_fn, partial_args);
        ASSERT_TRUE(partial_result.has_value()) << "Partial application failed for x=" << x;
        
        auto partial_fn = partial_result.value();
        auto partial_fn_result = apply(partial_fn, {Value(std::make_shared<Integer>(y))});
        
        ASSERT_TRUE(original_result.has_value()) << "Original function failed for x=" << x << ", y=" << y;
        ASSERT_TRUE(partial_fn_result.has_value()) << "Partial function failed for x=" << x << ", y=" << y;
        
        // Both should produce the same result
        EXPECT_TRUE(equal(original_result.value(), partial_fn_result.value()))
            << "Results differ for x=" << x << ", y=" << y
            << " original=" << original_result.value().to_string()
            << " partial=" << partial_fn_result.value().to_string();
    }
}

// Property-based test: Currying preserves function behavior
TEST_F(PartialApplicationTest, CurryingPreservesBehavior) {
    // Property: For any function f(a, b) and values x, y:
    // curry(f)(x)(y) should equal f(x, y)
    
    for (int i = 0; i < 50; ++i) {
        int x = (i * 13) % 100;  // Generate test values
        int y = (i * 19) % 100;
        
        // Apply original function
        auto original_result = apply(add_fn, {
            Value(std::make_shared<Integer>(x)),
            Value(std::make_shared<Integer>(y))
        });
        
        // Apply curried function
        auto curry_result = curry(add_fn);
        ASSERT_TRUE(curry_result.has_value()) << "Currying failed";
        
        auto curried_fn = curry_result.value();
        auto step1_result = apply(curried_fn, {Value(std::make_shared<Integer>(x))});
        ASSERT_TRUE(step1_result.has_value()) << "First curry step failed for x=" << x;
        
        auto step2_result = apply(step1_result.value(), {Value(std::make_shared<Integer>(y))});
        
        ASSERT_TRUE(original_result.has_value()) << "Original function failed for x=" << x << ", y=" << y;
        ASSERT_TRUE(step2_result.has_value()) << "Curried function failed for x=" << x << ", y=" << y;
        
        // Both should produce the same result
        EXPECT_TRUE(equal(original_result.value(), step2_result.value()))
            << "Results differ for x=" << x << ", y=" << y
            << " original=" << original_result.value().to_string()
            << " curried=" << step2_result.value().to_string();
    }
}

