#include "../../include/meld/types/result.hpp"
#include "../../include/meld/types/option.hpp"
#include "../../include/meld/types/error_trait.hpp"
#include "meld/testing/property_test.hpp"
#include <gtest/gtest.h>
#include <string>
#include <functional>
#include <memory>
#include <vector>

using namespace meld::types;
using namespace meld::testing;

/**
 * Property-Based Tests for Explicit Error Management (Property 9)
 *
 * Feature: rust-inspired-meld-enhancements, Property 9: Explicit Error Management
 *
 * These tests validate that fallible operations use Result types for explicit
 * error handling, that error propagation works correctly, that combinator
 * methods chain properly, and that custom error traits are supported.
 *
 * **Validates: Requirements 5.1, 5.2, 5.4, 5.5**
 */

// ---------------------------------------------------------------------------
// Helpers: simulate the ? operator (early-return error propagation)
// ---------------------------------------------------------------------------

// Simulates the ? operator: propagates Err, unwraps Ok
template<typename T, typename E>
Result<T, E> try_propagate(const Result<T, E>& r) {
    if (r.is_error()) {
        return Result<T, E>::error(r.error());
    }
    return Result<T, E>::success(r.value());
}

// Chain of fallible operations using ? operator simulation
template<typename T, typename E>
Result<T, E> chain_with_propagation(
    const Result<T, E>& input,
    const std::vector<std::function<Result<T, E>(T)>>& steps)
{
    Result<T, E> current = input;
    for (const auto& step : steps) {
        if (current.is_error()) {
            return current;  // ? operator: early return on error
        }
        current = step(current.value());
    }
    return current;
}

// ---------------------------------------------------------------------------
// Requirement 5.1: Result[T, E] for explicit error modeling
// For any fallible operation, the type system uses Result[T, E]
// ---------------------------------------------------------------------------

/**
 * Property: Every Result is in exactly one of two states: Ok or Err.
 * For any generated value, constructing a Result via Ok or Err yields
 * a value where is_success() XOR is_error() is always true.
 *
 * **Validates: Requirements 5.1**
 */
TEST(ErrorHandlingPropertyTest, ResultIsExactlyOneVariant) {
    auto int_gen = Generators::integers();
    auto str_gen = Generators::strings();

    // Ok variant: is_success && !is_error
    bool ok_property = PropertyTest::forall<int>(
        int_gen,
        [](int value) {
            auto r = Ok<int>(value);
            return r.is_success() && !r.is_error();
        }
    );

    // Err variant: !is_success && is_error
    bool err_property = PropertyTest::forall<std::string>(
        str_gen,
        [](const std::string& err) {
            auto r = Err<int>(err);
            return !r.is_success() && r.is_error();
        }
    );

    EXPECT_TRUE(ok_property);
    EXPECT_TRUE(err_property);
}

/**
 * Property: Result preserves the value it was constructed with.
 * For any value v, Ok(v).value() == v and Err(e).error() == e.
 *
 * **Validates: Requirements 5.1**
 */
TEST(ErrorHandlingPropertyTest, ResultPreservesConstructedValue) {
    auto int_gen = Generators::integers();
    auto str_gen = Generators::strings();

    bool ok_property = PropertyTest::forall<int>(
        int_gen,
        [](int value) {
            auto r = Ok<int>(value);
            return r.value() == value;
        }
    );

    bool err_property = PropertyTest::forall<std::string>(
        str_gen,
        [](const std::string& err) {
            auto r = Err<int>(err);
            return r.error() == err;
        }
    );

    EXPECT_TRUE(ok_property);
    EXPECT_TRUE(err_property);
}

// ---------------------------------------------------------------------------
// Requirement 5.2: ? operator for early return / error propagation
// ---------------------------------------------------------------------------

/**
 * Property: Error propagation short-circuits on first Err.
 * Given a chain of fallible operations, if any step returns Err,
 * the final result is that Err and subsequent steps are not executed.
 *
 * **Validates: Requirements 5.2**
 */
TEST(ErrorHandlingPropertyTest, ErrorPropagationShortCircuits) {
    auto int_gen = Generators::integers();
    auto str_gen = Generators::strings();

    bool property = PropertyTest::forall<int, std::string>(
        int_gen, str_gen,
        [](int value, const std::string& err_msg) {
            int steps_executed = 0;

            // Step 1: succeeds
            auto step1 = [&](int x) -> Result<int, std::string> {
                steps_executed++;
                return Ok<int>(x + 1);
            };
            // Step 2: fails
            auto step2 = [&](int) -> Result<int, std::string> {
                steps_executed++;
                return Err<int>(err_msg);
            };
            // Step 3: should never execute
            auto step3 = [&](int x) -> Result<int, std::string> {
                steps_executed++;
                return Ok<int>(x * 2);
            };

            std::vector<std::function<Result<int, std::string>(int)>> steps = {step1, step2, step3};
            auto result = chain_with_propagation(Ok<int>(value), steps);

            // Must be error, must carry the error message, step3 must not run
            return result.is_error() &&
                   result.error() == err_msg &&
                   steps_executed == 2;
        }
    );

    EXPECT_TRUE(property);
}

/**
 * Property: Propagation of Ok passes through unchanged.
 * If all steps succeed, the final result is Ok with the composed value.
 *
 * **Validates: Requirements 5.2**
 */
TEST(ErrorHandlingPropertyTest, SuccessfulPropagationPassesThrough) {
    auto int_gen = Generators::integers(-100, 100);

    bool property = PropertyTest::forall<int>(
        int_gen,
        [](int value) {
            auto step1 = [](int x) -> Result<int, std::string> {
                return Ok<int>(x + 1);
            };
            auto step2 = [](int x) -> Result<int, std::string> {
                return Ok<int>(x * 2);
            };

            std::vector<std::function<Result<int, std::string>(int)>> steps = {step1, step2};
            auto result = chain_with_propagation(Ok<int>(value), steps);

            return result.is_success() &&
                   result.value() == (value + 1) * 2;
        }
    );

    EXPECT_TRUE(property);
}

// ---------------------------------------------------------------------------
// Requirement 5.4: Combinator methods for error handling (chaining)
// ---------------------------------------------------------------------------

/**
 * Property: map on Ok transforms the value; map on Err preserves the error.
 *
 * **Validates: Requirements 5.4**
 */
TEST(ErrorHandlingPropertyTest, MapCombinatorProperty) {
    auto int_gen = Generators::integers();
    auto str_gen = Generators::strings();

    // map on Ok applies the function
    bool ok_property = PropertyTest::forall<int>(
        int_gen,
        [](int value) {
            auto r = Ok<int>(value);
            auto mapped = r.map([](int x) { return x + 42; });
            return mapped.is_success() && mapped.value() == value + 42;
        }
    );

    // map on Err preserves the error untouched
    bool err_property = PropertyTest::forall<std::string>(
        str_gen,
        [](const std::string& err) {
            auto r = Err<int>(err);
            auto mapped = r.map([](int x) { return x + 42; });
            return mapped.is_error() && mapped.error() == err;
        }
    );

    EXPECT_TRUE(ok_property);
    EXPECT_TRUE(err_property);
}

/**
 * Property: flat_map chains fallible operations correctly.
 * flat_map on Ok applies the function (which itself returns Result).
 * flat_map on Err short-circuits with the original error.
 *
 * **Validates: Requirements 5.4**
 */
TEST(ErrorHandlingPropertyTest, FlatMapCombinatorProperty) {
    auto int_gen = Generators::integers();
    auto str_gen = Generators::strings();

    // flat_map on Ok with a succeeding function
    bool ok_ok = PropertyTest::forall<int>(
        int_gen,
        [](int value) {
            auto r = Ok<int>(value);
            auto result = r.flat_map([](int x) { return Ok<int>(x * 3); });
            return result.is_success() && result.value() == value * 3;
        }
    );

    // flat_map on Ok with a failing function
    bool ok_err = PropertyTest::forall<int>(
        int_gen,
        [](int value) {
            auto r = Ok<int>(value);
            auto result = r.flat_map([](int) -> Result<int, std::string> {
                return Err<int>(std::string("inner failure"));
            });
            return result.is_error() && result.error() == "inner failure";
        }
    );

    // flat_map on Err never calls the function
    bool err_property = PropertyTest::forall<std::string>(
        str_gen,
        [](const std::string& err) {
            bool fn_called = false;
            auto r = Err<int>(err);
            auto result = r.flat_map([&](int x) -> Result<int, std::string> {
                fn_called = true;
                return Ok<int>(x);
            });
            return result.is_error() && result.error() == err && !fn_called;
        }
    );

    EXPECT_TRUE(ok_ok);
    EXPECT_TRUE(ok_err);
    EXPECT_TRUE(err_property);
}

/**
 * Property: or_else provides fallback on Err, passes through Ok.
 *
 * **Validates: Requirements 5.4**
 */
TEST(ErrorHandlingPropertyTest, OrElseCombinatorProperty) {
    auto int_gen = Generators::integers();
    auto str_gen = Generators::strings();

    // or_else on Ok returns the original Ok
    bool ok_property = PropertyTest::forall<int, int>(
        int_gen, int_gen,
        [](int value, int fallback) {
            auto r = Ok<int>(value);
            auto alt = Ok<int>(fallback);
            auto result = r.or_else(alt);
            return result.is_success() && result.value() == value;
        }
    );

    // or_else on Err returns the fallback
    bool err_property = PropertyTest::forall<std::string, int>(
        str_gen, int_gen,
        [](const std::string& err, int fallback) {
            auto r = Err<int>(err);
            auto alt = Ok<int>(fallback);
            auto result = r.or_else(alt);
            return result.is_success() && result.value() == fallback;
        }
    );

    EXPECT_TRUE(ok_property);
    EXPECT_TRUE(err_property);
}

/**
 * Property: unwrap_or returns the value for Ok, the default for Err.
 *
 * **Validates: Requirements 5.4**
 */
TEST(ErrorHandlingPropertyTest, UnwrapOrCombinatorProperty) {
    auto int_gen = Generators::integers();
    auto str_gen = Generators::strings();

    bool property = PropertyTest::forall<int, int, std::string>(
        int_gen, int_gen, str_gen,
        [](int value, int default_val, const std::string& err) {
            auto ok_result = Ok<int>(value);
            auto err_result = Err<int>(err);

            return ok_result.unwrap_or(default_val) == value &&
                   err_result.unwrap_or(default_val) == default_val;
        }
    );

    EXPECT_TRUE(property);
}

/**
 * Property: map_error transforms the error on Err, passes through Ok.
 *
 * **Validates: Requirements 5.4**
 */
TEST(ErrorHandlingPropertyTest, MapErrorCombinatorProperty) {
    auto int_gen = Generators::integers();
    auto str_gen = Generators::strings();

    // map_error on Ok preserves the value
    bool ok_property = PropertyTest::forall<int>(
        int_gen,
        [](int value) {
            auto r = Ok<int, std::string>(value);
            auto mapped = r.map_error([](const std::string& e) {
                return "wrapped: " + e;
            });
            return mapped.is_success() && mapped.value() == value;
        }
    );

    // map_error on Err transforms the error
    bool err_property = PropertyTest::forall<std::string>(
        str_gen,
        [](const std::string& err) {
            auto r = Err<int>(err);
            auto mapped = r.map_error([](const std::string& e) {
                return "wrapped: " + e;
            });
            return mapped.is_error() && mapped.error() == "wrapped: " + err;
        }
    );

    EXPECT_TRUE(ok_property);
    EXPECT_TRUE(err_property);
}

// ---------------------------------------------------------------------------
// Requirement 5.5: Error trait implementations for custom errors
// ---------------------------------------------------------------------------

/**
 * Property: All Error trait implementations provide a non-empty message.
 * For any custom error constructed with a non-empty message, message()
 * returns a non-empty string and category() returns a non-empty string.
 *
 * **Validates: Requirements 5.5**
 */
TEST(ErrorHandlingPropertyTest, ErrorTraitProvidesMessageAndCategory) {
    auto str_gen = Generators::strings(30);

    bool property = PropertyTest::forall<std::string>(
        str_gen,
        [](const std::string& msg) {
            if (msg.empty()) return true;  // skip empty — trivial case

            auto generic = std::make_shared<GenericError>(msg);
            auto io = std::make_shared<IOError>(IOError::Kind::NotFound, msg);
            auto parse = std::make_shared<ParseError>(msg, 1, 1);
            auto validation = std::make_shared<ValidationError>("field", msg);

            // All must return non-empty message and category
            return !generic->message().empty() && !generic->category().empty() &&
                   !io->message().empty() && !io->category().empty() &&
                   !parse->message().empty() && !parse->category().empty() &&
                   !validation->message().empty() && !validation->category().empty();
        }
    );

    EXPECT_TRUE(property);
}

/**
 * Property: Error chaining preserves the full chain.
 * When errors are chained via source(), chain_string() contains all
 * messages in order and chain() returns the correct number of sources.
 *
 * **Validates: Requirements 5.5**
 */
TEST(ErrorHandlingPropertyTest, ErrorChainingPreservesFullChain) {
    auto str_gen = Generators::strings(20);

    bool property = PropertyTest::forall<std::string, std::string>(
        str_gen, str_gen,
        [](const std::string& root_msg, const std::string& outer_msg) {
            if (root_msg.empty() || outer_msg.empty()) return true;

            auto root_err = std::make_shared<GenericError>(root_msg);
            auto outer_err = std::make_shared<IOError>(
                IOError::Kind::Other, outer_msg, root_err);

            // source() of outer should be root
            bool source_correct = outer_err->source() != nullptr &&
                                  outer_err->source()->message() == root_msg;

            // chain_string() should contain both messages
            std::string chain_str = outer_err->chain_string();
            bool chain_str_correct = chain_str.find(outer_msg) != std::string::npos &&
                                     chain_str.find(root_msg) != std::string::npos;

            // chain() should have exactly 1 element (the root)
            auto chain_vec = outer_err->chain();
            bool chain_len_correct = chain_vec.size() == 1;

            return source_correct && chain_str_correct && chain_len_correct;
        }
    );

    EXPECT_TRUE(property);
}

/**
 * Property: Error type downcasting works correctly.
 * is<T>() and as<T>() correctly identify the concrete error type.
 *
 * **Validates: Requirements 5.5**
 */
TEST(ErrorHandlingPropertyTest, ErrorTypeDowncastingWorks) {
    auto str_gen = Generators::strings(20);

    bool property = PropertyTest::forall<std::string>(
        str_gen,
        [](const std::string& msg) {
            if (msg.empty()) return true;

            std::shared_ptr<Error> generic = std::make_shared<GenericError>(msg);
            std::shared_ptr<Error> io = std::make_shared<IOError>(
                IOError::Kind::Timeout, msg);
            std::shared_ptr<Error> parse = std::make_shared<ParseError>(msg, 5, 10);

            // is<T>() correctly identifies the type
            bool generic_is = generic->is<GenericError>() &&
                              !generic->is<IOError>() &&
                              !generic->is<ParseError>();

            bool io_is = io->is<IOError>() &&
                         !io->is<GenericError>() &&
                         !io->is<ParseError>();

            bool parse_is = parse->is<ParseError>() &&
                            !parse->is<GenericError>() &&
                            !parse->is<IOError>();

            // as<T>() returns non-null for correct type, null for wrong type
            bool generic_as = generic->as<GenericError>() != nullptr &&
                              generic->as<IOError>() == nullptr;

            bool io_as = io->as<IOError>() != nullptr &&
                         io->as<GenericError>() == nullptr;

            bool parse_as = parse->as<ParseError>() != nullptr &&
                            parse->as<GenericError>() == nullptr;

            return generic_is && io_is && parse_is &&
                   generic_as && io_as && parse_as;
        }
    );

    EXPECT_TRUE(property);
}

/**
 * Property: Result integrates with custom Error trait types.
 * Result<T, shared_ptr<Error>> works correctly with the error trait
 * hierarchy, preserving error type information through combinators.
 *
 * **Validates: Requirements 5.1, 5.5**
 */
TEST(ErrorHandlingPropertyTest, ResultIntegratesWithErrorTrait) {
    auto int_gen = Generators::integers();
    auto str_gen = Generators::strings(20);

    using ErrorPtr = std::shared_ptr<Error>;

    bool property = PropertyTest::forall<int, std::string>(
        int_gen, str_gen,
        [](int value, const std::string& msg) {
            if (msg.empty()) return true;

            // Create Result with custom error type
            auto ok_result = Result<int, ErrorPtr>::success(value);
            auto err_result = Result<int, ErrorPtr>::error(
                std::make_shared<IOError>(IOError::Kind::NotFound, msg));

            // Ok result: map works, value preserved
            auto mapped_ok = ok_result.map([](int x) { return x + 1; });
            bool ok_correct = mapped_ok.is_success() &&
                              mapped_ok.value() == value + 1;

            // Err result: map preserves error, error type is recoverable
            auto mapped_err = err_result.map([](int x) { return x + 1; });
            bool err_correct = mapped_err.is_error() &&
                               mapped_err.error()->is<IOError>() &&
                               mapped_err.error()->message() == msg;

            return ok_correct && err_correct;
        }
    );

    EXPECT_TRUE(property);
}

