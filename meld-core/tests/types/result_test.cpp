#include "../../include/meld/types/result.hpp"
#include <gtest/gtest.h>
#include <string>

using namespace meld::types;

// Test Result creation
TEST(ResultTest, CreateSuccess) {
    auto result = Result<int, std::string>::success(42);
    
    EXPECT_TRUE(result.is_success());
    EXPECT_FALSE(result.is_error());
    EXPECT_EQ(result.value(), 42);
}

TEST(ResultTest, CreateError) {
    auto result = Result<int, std::string>::error("Something went wrong");
    
    EXPECT_FALSE(result.is_success());
    EXPECT_TRUE(result.is_error());
    EXPECT_EQ(result.error(), "Something went wrong");
}

// Test helper functions
TEST(ResultTest, OkHelper) {
    auto result = Ok<int>(42);
    
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(result.value(), 42);
}

TEST(ResultTest, ErrHelper) {
    auto result = Err<int, std::string>("Error message");
    
    EXPECT_TRUE(result.is_error());
    EXPECT_EQ(result.error(), "Error message");
}

// Test value accessors
TEST(ResultTest, ValueAccessor) {
    auto result = Ok<int>(42);
    EXPECT_EQ(result.value(), 42);
}

TEST(ResultTest, ErrorAccessor) {
    auto result = Err<int, std::string>("Error");
    EXPECT_EQ(result.error(), "Error");
}

TEST(ResultTest, ValueOrSuccess) {
    auto result = Ok<int>(42);
    EXPECT_EQ(result.value_or(0), 42);
}

TEST(ResultTest, ValueOrError) {
    auto result = Err<int, std::string>("Error");
    EXPECT_EQ(result.value_or(99), 99);
}

TEST(ResultTest, UnwrapSuccess) {
    auto result = Ok<int>(42);
    EXPECT_EQ(result.unwrap(), 42);
}

TEST(ResultTest, UnwrapError) {
    auto result = Err<int, std::string>("Error");
    EXPECT_THROW(result.unwrap(), std::runtime_error);
}

TEST(ResultTest, UnwrapOrSuccess) {
    auto result = Ok<int>(42);
    EXPECT_EQ(result.unwrap_or(0), 42);
}

TEST(ResultTest, UnwrapOrError) {
    auto result = Err<int, std::string>("Error");
    EXPECT_EQ(result.unwrap_or(99), 99);
}

// Test map combinator
TEST(ResultTest, MapSuccess) {
    auto result = Ok<int>(42);
    auto mapped = result.map([](int x) { return x * 2; });
    
    EXPECT_TRUE(mapped.is_success());
    EXPECT_EQ(mapped.value(), 84);
}

TEST(ResultTest, MapError) {
    auto result = Err<int, std::string>("Error");
    auto mapped = result.map([](int x) { return x * 2; });
    
    EXPECT_TRUE(mapped.is_error());
    EXPECT_EQ(mapped.error(), "Error");
}

TEST(ResultTest, MapChaining) {
    auto result = Ok<int>(5);
    auto final_result = result
        .map([](int x) { return x * 2; })
        .map([](int x) { return x + 10; })
        .map([](int x) { return x / 2; });
    
    EXPECT_TRUE(final_result.is_success());
    EXPECT_EQ(final_result.value(), 10);  // (5 * 2 + 10) / 2 = 10
}

// Test flatMap combinator
TEST(ResultTest, FlatMapSuccess) {
    auto result = Ok<int>(42);
    auto flat_mapped = result.flat_map([](int x) {
        return Ok<int>(x * 2);
    });
    
    EXPECT_TRUE(flat_mapped.is_success());
    EXPECT_EQ(flat_mapped.value(), 84);
}

TEST(ResultTest, FlatMapError) {
    auto result = Err<int, std::string>("Error");
    auto flat_mapped = result.flat_map([](int x) {
        return Ok<int>(x * 2);
    });
    
    EXPECT_TRUE(flat_mapped.is_error());
    EXPECT_EQ(flat_mapped.error(), "Error");
}

TEST(ResultTest, FlatMapReturnsError) {
    auto result = Ok<int>(42);
    auto flat_mapped = result.flat_map([](int x) {
        return Err<int, std::string>("Inner error");
    });
    
    EXPECT_TRUE(flat_mapped.is_error());
    EXPECT_EQ(flat_mapped.error(), "Inner error");
}

TEST(ResultTest, FlatMapChaining) {
    auto result = Ok<int>(10);
    auto final_result = result
        .flat_map([](int x) { return Ok<int>(x * 2); })
        .flat_map([](int x) { return Ok<int>(x + 5); })
        .flat_map([](int x) { return Ok<int>(x / 5); });
    
    EXPECT_TRUE(final_result.is_success());
    EXPECT_EQ(final_result.value(), 5);  // (10 * 2 + 5) / 5 = 5
}

// Test mapError combinator
TEST(ResultTest, MapErrorSuccess) {
    auto result = Ok<int>(42);
    auto mapped = result.map_error([](const std::string& e) {
        return "Transformed: " + e;
    });
    
    EXPECT_TRUE(mapped.is_success());
    EXPECT_EQ(mapped.value(), 42);
}

TEST(ResultTest, MapErrorError) {
    auto result = Err<int, std::string>("Original error");
    auto mapped = result.map_error([](const std::string& e) {
        return "Transformed: " + e;
    });
    
    EXPECT_TRUE(mapped.is_error());
    EXPECT_EQ(mapped.error(), "Transformed: Original error");
}

// Test and_then combinator
TEST(ResultTest, AndThenBothSuccess) {
    auto result1 = Ok<int>(42);
    auto result2 = Ok<std::string>("hello");
    auto combined = result1.and_then(result2);
    
    EXPECT_TRUE(combined.is_success());
    EXPECT_EQ(combined.value(), "hello");
}

TEST(ResultTest, AndThenFirstError) {
    auto result1 = Err<int, std::string>("First error");
    auto result2 = Ok<std::string>("hello");
    auto combined = result1.and_then(result2);
    
    EXPECT_TRUE(combined.is_error());
    EXPECT_EQ(combined.error(), "First error");
}

// Test or_else combinator
TEST(ResultTest, OrElseFirstSuccess) {
    auto result1 = Ok<int>(42);
    auto result2 = Ok<int>(99);
    auto combined = result1.or_else(result2);
    
    EXPECT_TRUE(combined.is_success());
    EXPECT_EQ(combined.value(), 42);
}

TEST(ResultTest, OrElseFirstError) {
    auto result1 = Err<int, std::string>("Error");
    auto result2 = Ok<int>(99);
    auto combined = result1.or_else(result2);
    
    EXPECT_TRUE(combined.is_success());
    EXPECT_EQ(combined.value(), 99);
}

// Test match pattern
TEST(ResultTest, MatchSuccess) {
    auto result = Ok<int>(42);
    
    std::string output = result.match(
        [](int value) { return "Success: " + std::to_string(value); },
        [](const std::string& error) { return "Error: " + error; }
    );
    
    EXPECT_EQ(output, "Success: 42");
}

TEST(ResultTest, MatchError) {
    auto result = Err<int, std::string>("Something failed");
    
    std::string output = result.match(
        [](int value) { return "Success: " + std::to_string(value); },
        [](const std::string& error) { return "Error: " + error; }
    );
    
    EXPECT_EQ(output, "Error: Something failed");
}

// Test complex scenarios
TEST(ResultTest, DivisionExample) {
    auto divide = [](int a, int b) -> Result<int, std::string> {
        if (b == 0) {
            return Err<int, std::string>("Division by zero");
        }
        return Ok<int>(a / b);
    };
    
    auto result1 = divide(10, 2);
    EXPECT_TRUE(result1.is_success());
    EXPECT_EQ(result1.value(), 5);
    
    auto result2 = divide(10, 0);
    EXPECT_TRUE(result2.is_error());
    EXPECT_EQ(result2.error(), "Division by zero");
}

TEST(ResultTest, ChainedOperations) {
    auto parse_int = [](const std::string& s) -> Result<int, std::string> {
        try {
            return Ok<int>(std::stoi(s));
        } catch (...) {
            return Err<int, std::string>("Parse error");
        }
    };
    
    auto validate_positive = [](int x) -> Result<int, std::string> {
        if (x > 0) {
            return Ok<int>(x);
        }
        return Err<int, std::string>("Not positive");
    };
    
    auto result = parse_int("42")
        .flat_map(validate_positive)
        .map([](int x) { return x * 2; });
    
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(result.value(), 84);
    
    auto result2 = parse_int("-5")
        .flat_map(validate_positive)
        .map([](int x) { return x * 2; });
    
    EXPECT_TRUE(result2.is_error());
    EXPECT_EQ(result2.error(), "Not positive");
}

// Test with custom error types
TEST(ResultTest, CustomErrorType) {
    enum class ErrorCode {
        NotFound,
        PermissionDenied,
        InvalidInput
    };
    
    auto result1 = Result<int, ErrorCode>::success(42);
    EXPECT_TRUE(result1.is_success());
    
    auto result2 = Result<int, ErrorCode>::error(ErrorCode::NotFound);
    EXPECT_TRUE(result2.is_error());
    EXPECT_EQ(result2.error(), ErrorCode::NotFound);
}

// Test Functor Laws
TEST(ResultTest, FunctorIdentityLaw) {
    // map(id) == id
    auto result = Ok<int>(42);
    auto mapped = result.map([](int x) { return x; });
    
    EXPECT_EQ(result.value(), mapped.value());
}

TEST(ResultTest, FunctorCompositionLaw) {
    // map(f . g) == map(f) . map(g)
    auto f = [](int x) { return x * 2; };
    auto g = [](int x) { return x + 10; };
    
    auto result = Ok<int>(5);
    
    // Left side: map(f . g)
    auto left = result.map([&](int x) { return f(g(x)); });
    
    // Right side: map(f) . map(g)
    auto right = result.map(g).map(f);
    
    EXPECT_EQ(left.value(), right.value());
}

// Test Monad Laws
TEST(ResultTest, MonadLeftIdentityLaw) {
    // return a >>= f == f a
    auto f = [](int x) { return Ok<int>(x * 2); };
    int a = 42;
    
    auto left = Ok<int>(a).flat_map(f);
    auto right = f(a);
    
    EXPECT_EQ(left.value(), right.value());
}

TEST(ResultTest, MonadRightIdentityLaw) {
    // m >>= return == m
    auto result = Ok<int>(42);
    auto bound = result.flat_map([](int x) { return Ok<int>(x); });
    
    EXPECT_EQ(result.value(), bound.value());
}

TEST(ResultTest, MonadAssociativityLaw) {
    // (m >>= f) >>= g == m >>= (\x -> f x >>= g)
    auto f = [](int x) { return Ok<int>(x * 2); };
    auto g = [](int x) { return Ok<int>(x + 10); };
    
    auto m = Ok<int>(5);
    
    // Left side: (m >>= f) >>= g
    auto left = m.flat_map(f).flat_map(g);
    
    // Right side: m >>= (\x -> f x >>= g)
    auto right = m.flat_map([&](int x) {
        return f(x).flat_map(g);
    });
    
    EXPECT_EQ(left.value(), right.value());
}

