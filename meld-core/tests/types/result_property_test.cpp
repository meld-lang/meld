#include "../../include/meld/types/result.hpp"
#include "meld/testing/property_test.hpp"
#include <gtest/gtest.h>
#include <string>
#include <functional>

using namespace meld::types;
using namespace meld::testing;

/**
 * Property-Based Tests for Result[T, E]
 * 
 * These tests validate the mathematical properties that Result must satisfy:
 * - Functor Laws (for map)
 * - Monad Laws (for flat_map)
 * - Result-specific properties
 * 
 * **Feature: meld-lang, Property: Result Functor Laws**
 * **Validates: Requirements 28.3**
 */

// Test Functor Identity Law: map(id) == id
TEST(ResultPropertyTest, FunctorIdentityLaw) {
    auto int_gen = Generators::integers();
    auto string_gen = Generators::strings();
    
    // Property: For all Result[int], map(identity) should equal the original result
    bool success_property = PropertyTest::forall<int>(
        int_gen,
        [](int value) {
            auto result = Ok<int>(value);
            auto mapped = result.map([](int x) { return x; }); // identity function
            return result.is_success() == mapped.is_success() && 
                   result.value() == mapped.value();
        }
    );
    
    // Property: For all Result[int, string] errors, map(identity) should equal the original result
    bool error_property = PropertyTest::forall<std::string>(
        string_gen,
        [](const std::string& error) {
            auto result = Err<int>(error);
            auto mapped = result.map([](int x) { return x; }); // identity function
            return result.is_error() == mapped.is_error() && 
                   result.error() == mapped.error();
        }
    );
    
    EXPECT_TRUE(success_property);
    EXPECT_TRUE(error_property);
}

// Test Functor Composition Law: map(f . g) == map(f) . map(g)
TEST(ResultPropertyTest, FunctorCompositionLaw) {
    auto int_gen = Generators::integers();
    
    auto f = [](int x) { return x * 2; };
    auto g = [](int x) { return x + 10; };
    
    // Property: For all Result[int], map(f . g) should equal map(f) . map(g)
    bool property = PropertyTest::forall<int>(
        int_gen,
        [&](int value) {
            auto result = Ok<int>(value);
            
            // Left side: map(f . g)
            auto left = result.map([&](int x) { return f(g(x)); });
            
            // Right side: map(f) . map(g)
            auto right = result.map(g).map(f);
            
            return left.is_success() == right.is_success() && 
                   left.value() == right.value();
        }
    );
    
    EXPECT_TRUE(property);
}

/**
 * **Feature: meld-lang, Property: Result Monad Laws**
 * **Validates: Requirements 28.4**
 *
 * The three monad laws for Result[T, E] with flat_map:
 *   1. Left Identity:  Result.success(a).flat_map(f) == f(a)
 *   2. Right Identity:  m.flat_map(Result.success) == m
 *   3. Associativity:  m.flat_map(f).flat_map(g) == m.flat_map(x => f(x).flat_map(g))
 *
 * Each law is tested with:
 *   - Functions that always succeed
 *   - Functions that may fail (returning Error)
 *   - Both Success and Error inputs where applicable
 */

// ---------------------------------------------------------------------------
// Monad Law 1: Left Identity
// Result.success(a).flat_map(f) == f(a)
// ---------------------------------------------------------------------------

/**
 * Left identity with a succeeding function.
 * For all a, Ok(a).flat_map(f) == f(a) when f always returns Ok.
 *
 * **Validates: Requirements 28.4**
 */
TEST(ResultPropertyTest, MonadLeftIdentityLaw) {
    auto int_gen = Generators::integers();
    
    auto f = [](int x) { return Ok<int>(x * 2); };
    
    bool property = PropertyTest::forall<int>(
        int_gen,
        [&](int a) {
            auto left = Ok<int>(a).flat_map(f);
            auto right = f(a);
            
            return left.is_success() == right.is_success() && 
                   left.value() == right.value();
        }
    );
    
    EXPECT_TRUE(property);
}

/**
 * Left identity with a function that may fail.
 * For all a, Ok(a).flat_map(f) == f(a) even when f returns Err for some inputs.
 *
 * **Validates: Requirements 28.4**
 */
TEST(ResultPropertyTest, MonadLeftIdentityWithFailingFunction) {
    auto int_gen = Generators::integers();
    
    // f returns Err for negative values, Ok for non-negative
    auto f = [](int x) -> Result<int, std::string> {
        if (x < 0) return Err<int>(std::string("negative: ") + std::to_string(x));
        return Ok<int>(x * 3 + 1);
    };
    
    bool property = PropertyTest::forall<int>(
        int_gen,
        [&](int a) {
            auto left = Ok<int>(a).flat_map(f);
            auto right = f(a);
            
            if (left.is_success() != right.is_success()) return false;
            if (left.is_success()) return left.value() == right.value();
            return left.error() == right.error();
        }
    );
    
    EXPECT_TRUE(property);
}

// ---------------------------------------------------------------------------
// Monad Law 2: Right Identity
// m.flat_map(Result.success) == m
// ---------------------------------------------------------------------------

/**
 * Right identity for Success values.
 * For all v, Ok(v).flat_map(Ok) == Ok(v).
 *
 * **Validates: Requirements 28.4**
 */
TEST(ResultPropertyTest, MonadRightIdentityLaw) {
    auto int_gen = Generators::integers();
    auto string_gen = Generators::strings();
    
    bool success_property = PropertyTest::forall<int>(
        int_gen,
        [](int value) {
            auto result = Ok<int>(value);
            auto bound = result.flat_map([](int x) { return Ok<int>(x); });
            
            return result.is_success() == bound.is_success() && 
                   result.value() == bound.value();
        }
    );
    
    // Right identity for Error values: Err(e).flat_map(Ok) == Err(e)
    bool error_property = PropertyTest::forall<std::string>(
        string_gen,
        [](const std::string& error) {
            auto result = Err<int>(error);
            auto bound = result.flat_map([](int x) { return Ok<int>(x); });
            
            return result.is_error() == bound.is_error() && 
                   result.error() == bound.error();
        }
    );
    
    EXPECT_TRUE(success_property);
    EXPECT_TRUE(error_property);
}

// ---------------------------------------------------------------------------
// Monad Law 3: Associativity
// m.flat_map(f).flat_map(g) == m.flat_map(x => f(x).flat_map(g))
// ---------------------------------------------------------------------------

/**
 * Associativity with two succeeding functions.
 *
 * **Validates: Requirements 28.4**
 */
TEST(ResultPropertyTest, MonadAssociativityLaw) {
    auto int_gen = Generators::integers();
    
    auto f = [](int x) { return Ok<int>(x * 2); };
    auto g = [](int x) { return Ok<int>(x + 10); };
    
    bool property = PropertyTest::forall<int>(
        int_gen,
        [&](int value) {
            auto m = Ok<int>(value);
            
            auto left = m.flat_map(f).flat_map(g);
            auto right = m.flat_map([&](int x) {
                return f(x).flat_map(g);
            });
            
            return left.is_success() == right.is_success() && 
                   left.value() == right.value();
        }
    );
    
    EXPECT_TRUE(property);
}

/**
 * Associativity with functions that may fail.
 * Both sides must produce the same result (success or error) regardless
 * of which function in the chain fails.
 *
 * **Validates: Requirements 28.4**
 */
TEST(ResultPropertyTest, MonadAssociativityWithFailingFunctions) {
    auto int_gen = Generators::integers();
    
    // f fails on negative inputs
    auto f = [](int x) -> Result<int, std::string> {
        if (x < 0) return Err<int, std::string>("f: negative");
        return Ok<int>(x + 5);
    };
    // g fails on even inputs
    auto g = [](int x) -> Result<int, std::string> {
        if (x % 2 == 0) return Err<int, std::string>("g: even");
        return Ok<int>(x * 2);
    };
    
    bool property = PropertyTest::forall<int>(
        int_gen,
        [&](int value) {
            auto m = Ok<int>(value);
            
            auto left = m.flat_map(f).flat_map(g);
            auto right = m.flat_map([&](int x) {
                return f(x).flat_map(g);
            });
            
            if (left.is_success() != right.is_success()) return false;
            if (left.is_success()) return left.value() == right.value();
            return left.error() == right.error();
        }
    );
    
    EXPECT_TRUE(property);
}

/**
 * Associativity when the initial Result is an Error.
 * Error should propagate identically through both sides of the law.
 *
 * **Validates: Requirements 28.4**
 */
TEST(ResultPropertyTest, MonadAssociativityWithErrorInput) {
    auto str_gen = Generators::strings();
    
    auto f = [](int x) { return Ok<int>(x * 2); };
    auto g = [](int x) { return Ok<int>(x + 10); };
    
    bool property = PropertyTest::forall<std::string>(
        str_gen,
        [&](const std::string& err) {
            auto m = Err<int>(err);
            
            auto left = m.flat_map(f).flat_map(g);
            auto right = m.flat_map([&](int x) {
                return f(x).flat_map(g);
            });
            
            return left.is_error() && right.is_error() &&
                   left.error() == right.error() &&
                   left.error() == err;
        }
    );
    
    EXPECT_TRUE(property);
}

// Test Result-specific properties

// Property: map preserves error values
TEST(ResultPropertyTest, MapPreservesErrors) {
    auto string_gen = Generators::strings();
    
    // Property: For all error values, map should preserve the error unchanged
    bool property = PropertyTest::forall<std::string>(
        string_gen,
        [](const std::string& error) {
            auto result = Err<int>(error);
            auto mapped = result.map([](int x) { return x * 100; });
            
            return mapped.is_error() && mapped.error() == error;
        }
    );
    
    EXPECT_TRUE(property);
}

// Property: flat_map preserves error values
TEST(ResultPropertyTest, FlatMapPreservesErrors) {
    auto string_gen = Generators::strings();
    
    // Property: For all error values, flat_map should preserve the error unchanged
    bool property = PropertyTest::forall<std::string>(
        string_gen,
        [](const std::string& error) {
            auto result = Err<int>(error);
            auto flat_mapped = result.flat_map([](int x) { return Ok<int>(x * 100); });
            
            return flat_mapped.is_error() && flat_mapped.error() == error;
        }
    );
    
    EXPECT_TRUE(property);
}

// Property: map_error preserves success values
TEST(ResultPropertyTest, MapErrorPreservesSuccess) {
    auto int_gen = Generators::integers();
    
    // Property: For all success values, map_error should preserve the value unchanged
    bool property = PropertyTest::forall<int>(
        int_gen,
        [](int value) {
            auto result = Ok<int, std::string>(value);
            auto mapped = result.map_error([](const std::string& e) { return "transformed: " + e; });
            
            return mapped.is_success() && mapped.value() == value;
        }
    );
    
    EXPECT_TRUE(property);
}

// Property: unwrap_or returns value for success, default for error
TEST(ResultPropertyTest, UnwrapOrBehavior) {
    auto int_gen = Generators::integers();
    auto string_gen = Generators::strings();
    
    // Property: For all success values, unwrap_or should return the value
    bool success_property = PropertyTest::forall(
        int_gen, int_gen,
        [](int value, int default_val) {
            auto result = Ok<int>(value);
            return result.unwrap_or(default_val) == value;
        }
    );
    
    // Property: For all error values, unwrap_or should return the default
    bool error_property = PropertyTest::forall(
        string_gen, int_gen,
        [](const std::string& error, int default_val) {
            auto result = Err<int>(error);
            return result.unwrap_or(default_val) == default_val;
        }
    );
    
    EXPECT_TRUE(success_property);
    EXPECT_TRUE(error_property);
}

// Property: and_then behavior
TEST(ResultPropertyTest, AndThenBehavior) {
    auto int_gen = Generators::integers();
    auto string_gen = Generators::strings();
    
    // Property: For all success values, and_then should return the second result
    bool success_property = PropertyTest::forall(
        int_gen, string_gen,
        [](int value1, const std::string& value2) {
            auto result1 = Ok<int>(value1);
            auto result2 = Ok<std::string>(value2);
            auto combined = result1.and_then(result2);
            
            return combined.is_success() && combined.value() == value2;
        }
    );
    
    // Property: For all error values, and_then should return the first error
    bool error_property = PropertyTest::forall(
        string_gen, string_gen,
        [](const std::string& error1, const std::string& value2) {
            auto result1 = Err<int>(error1);
            auto result2 = Ok<std::string>(value2);
            auto combined = result1.and_then(result2);
            
            return combined.is_error() && combined.error() == error1;
        }
    );
    
    EXPECT_TRUE(success_property);
    EXPECT_TRUE(error_property);
}

// Property: or_else behavior
TEST(ResultPropertyTest, OrElseBehavior) {
    auto int_gen = Generators::integers();
    auto string_gen = Generators::strings();
    
    // Property: For all success values, or_else should return the first result
    bool success_property = PropertyTest::forall(
        int_gen, int_gen,
        [](int value1, int value2) {
            auto result1 = Ok<int>(value1);
            auto result2 = Ok<int>(value2);
            auto combined = result1.or_else(result2);
            
            return combined.is_success() && combined.value() == value1;
        }
    );
    
    // Property: For all error values, or_else should return the second result
    bool error_property = PropertyTest::forall(
        string_gen, int_gen,
        [](const std::string& error, int value2) {
            auto result1 = Err<int>(error);
            auto result2 = Ok<int>(value2);
            auto combined = result1.or_else(result2);
            
            return combined.is_success() && combined.value() == value2;
        }
    );
    
    EXPECT_TRUE(success_property);
    EXPECT_TRUE(error_property);
}

