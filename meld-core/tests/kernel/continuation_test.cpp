#include <gtest/gtest.h>
#include "meld/kernel/primitives.hpp"
#include "meld/kernel/continuation.hpp"

using namespace meld::kernel;

class ContinuationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear continuation stack before each test
        DelimitedContinuation::clear();
    }
    
    void TearDown() override {
        // Clear continuation stack after each test
        DelimitedContinuation::clear();
    }
};

// Test 1: Basic continuation capture and resume
TEST_F(ContinuationTest, BasicCaptureAndResume) {
    bool handler_called = false;
    Value result;
    
    // Set up a delimiter
    DelimitedContinuation::push_delimiter(
        "test",
        [](Value v) { return v; }
    );
    
    // Suspend and capture continuation
    result = primitive_suspend("test", [&](std::shared_ptr<Continuation> cont) -> Value {
        handler_called = true;
        EXPECT_TRUE(cont->is_valid());
        EXPECT_EQ(cont->delimiter_id(), "test");
        
        // Resume immediately with a value
        return cont->resume(Value(std::make_shared<Integer>(42)));
    });
    
    EXPECT_TRUE(handler_called);
    EXPECT_TRUE(result.is<Integer>());
    EXPECT_EQ(result.as<Integer>()->value(), 42);
}

// Test 2: Continuation is one-shot (cannot resume twice)
TEST_F(ContinuationTest, ContinuationIsOneShot) {
    DelimitedContinuation::push_delimiter(
        "test",
        [](Value v) { return v; }
    );
    
    std::shared_ptr<Continuation> captured_cont;
    
    primitive_suspend("test", [&](std::shared_ptr<Continuation> cont) -> Value {
        captured_cont = cont;
        return cont->resume(Value(std::make_shared<Integer>(1)));
    });
    
    // Try to resume again - should throw
    EXPECT_THROW(
        captured_cont->resume(Value(std::make_shared<Integer>(2))),
        std::runtime_error
    );
}

// Test 3: Delimiter not found throws exception
TEST_F(ContinuationTest, DelimiterNotFoundThrows) {
    // No delimiter set up
    EXPECT_THROW(
        primitive_suspend("nonexistent", [](std::shared_ptr<Continuation> cont) -> Value {
            return Value(std::make_shared<Integer>(0));
        }),
        std::runtime_error
    );
}

// Test 4: Nested delimiters - inner takes precedence
TEST_F(ContinuationTest, NestedDelimiters) {
    std::string which_handler;
    
    // Outer delimiter
    DelimitedContinuation::push_delimiter(
        "outer",
        [&](Value v) { 
            which_handler = "outer";
            return v; 
        }
    );
    
    // Inner delimiter with same ID
    DelimitedContinuation::push_delimiter(
        "outer",
        [&](Value v) { 
            which_handler = "inner";
            return v; 
        }
    );
    
    // Suspend should capture to the inner (most recent) delimiter
    Value result = primitive_suspend("outer", [](std::shared_ptr<Continuation> cont) -> Value {
        return cont->resume(Value(std::make_shared<Integer>(99)));
    });
    
    EXPECT_EQ(which_handler, "inner");
    EXPECT_TRUE(result.is<Integer>());
    EXPECT_EQ(result.as<Integer>()->value(), 99);
}

// Test 5: Library function mark_stack
TEST_F(ContinuationTest, MarkStackFunction) {
    bool handler_invoked = false;
    
    mark_stack("effect", [&](std::shared_ptr<Continuation> cont) -> Value {
        handler_invoked = true;
        return cont->resume(Value(Boolean::from(true)));
    });
    
    Value result = suspend("effect", [](std::shared_ptr<Continuation> cont) -> Value {
        return cont->resume(Value(std::make_shared<Integer>(123)));
    });
    
    EXPECT_TRUE(handler_invoked);
    EXPECT_TRUE(result.is<Integer>());
}

// Test 6: Library function resume
TEST_F(ContinuationTest, ResumeFunction) {
    DelimitedContinuation::push_delimiter(
        "test",
        [](Value v) { return v; }
    );
    
    std::shared_ptr<Continuation> cont;
    
    primitive_suspend("test", [&](std::shared_ptr<Continuation> c) -> Value {
        cont = c;
        // Don't resume yet
        return Value(std::make_shared<Integer>(0));
    });
    
    // Use the library resume function
    Value result = resume(cont, Value(std::make_shared<String>("resumed")));
    
    EXPECT_TRUE(result.is<String>());
    EXPECT_EQ(result.as<String>()->value(), "resumed");
}

// Test 7: with_delimiter helper
TEST_F(ContinuationTest, WithDelimiterHelper) {
    Value result = with_delimiter("scope", []() -> Value {
        // Inside the delimited scope
        return primitive_suspend("scope", [](std::shared_ptr<Continuation> cont) -> Value {
            return cont->resume(Value(std::make_shared<Integer>(777)));
        });
    });
    
    EXPECT_TRUE(result.is<Integer>());
    EXPECT_EQ(result.as<Integer>()->value(), 777);
}

// Test 8: Exception-like behavior (discard continuation)
TEST_F(ContinuationTest, ExceptionLikeBehavior) {
    DelimitedContinuation::push_delimiter(
        "exception",
        [](Value v) { return v; }
    );
    
    // Simulate throwing an exception by NOT resuming the continuation
    Value result = primitive_suspend("exception", [](std::shared_ptr<Continuation> cont) -> Value {
        // Discard the continuation (don't call resume)
        // Return an error value instead
        return Value(std::make_shared<String>("error: something went wrong"));
    });
    
    EXPECT_TRUE(result.is<String>());
    EXPECT_EQ(result.as<String>()->value(), "error: something went wrong");
}

// Test 9: Generator-like behavior (resume multiple times conceptually)
TEST_F(ContinuationTest, GeneratorLikeBehavior) {
    std::vector<int64_t> yielded_values;
    
    DelimitedContinuation::push_delimiter(
        "generator",
        [](Value v) { return v; }
    );
    
    // Simulate yielding a value
    auto yield_value = [&](int64_t val) -> Value {
        return primitive_suspend("generator", [&, val](std::shared_ptr<Continuation> cont) -> Value {
            yielded_values.push_back(val);
            // In a real generator, we'd store the continuation and resume it later
            // For this test, we'll just resume immediately
            return cont->resume(Value(std::make_shared<Integer>(val)));
        });
    };
    
    yield_value(1);
    yield_value(2);
    yield_value(3);
    
    EXPECT_EQ(yielded_values.size(), 3);
    EXPECT_EQ(yielded_values[0], 1);
    EXPECT_EQ(yielded_values[1], 2);
    EXPECT_EQ(yielded_values[2], 3);
}

// Test 10: Async-like behavior (store continuation for later)
TEST_F(ContinuationTest, AsyncLikeBehavior) {
    std::shared_ptr<Continuation> stored_continuation;
    
    DelimitedContinuation::push_delimiter(
        "async",
        [](Value v) { return v; }
    );
    
    // Simulate an async operation that stores the continuation
    Value initial_result = primitive_suspend("async", [&](std::shared_ptr<Continuation> cont) -> Value {
        // Store the continuation for later resumption
        stored_continuation = cont;
        // Return a "pending" marker
        return Value(std::make_shared<String>("pending"));
    });
    
    EXPECT_TRUE(initial_result.is<String>());
    EXPECT_EQ(initial_result.as<String>()->value(), "pending");
    EXPECT_TRUE(stored_continuation != nullptr);
    EXPECT_TRUE(stored_continuation->is_valid());
    
    // Later, resume the continuation with the actual result
    Value final_result = stored_continuation->resume(Value(std::make_shared<Integer>(42)));
    
    EXPECT_TRUE(final_result.is<Integer>());
    EXPECT_EQ(final_result.as<Integer>()->value(), 42);
}

// Test 11: Stack depth tracking
TEST_F(ContinuationTest, StackDepthTracking) {
    EXPECT_EQ(DelimitedContinuation::depth(), 0);
    
    DelimitedContinuation::push_delimiter("d1", [](Value v) { return v; });
    EXPECT_EQ(DelimitedContinuation::depth(), 1);
    
    DelimitedContinuation::push_delimiter("d2", [](Value v) { return v; });
    EXPECT_EQ(DelimitedContinuation::depth(), 2);
    
    DelimitedContinuation::pop_delimiter();
    EXPECT_EQ(DelimitedContinuation::depth(), 1);
    
    DelimitedContinuation::pop_delimiter();
    EXPECT_EQ(DelimitedContinuation::depth(), 0);
}

// Test 12: Continuation to_string
TEST_F(ContinuationTest, ContinuationToString) {
    auto cont = std::make_shared<Continuation>(
        [](Value v) { return v; },
        "test-effect"
    );
    
    std::string str = cont->to_string();
    EXPECT_TRUE(str.find("continuation") != std::string::npos);
    EXPECT_TRUE(str.find("test-effect") != std::string::npos);
    
    // After consuming
    cont->resume(Value(std::make_shared<Integer>(1)));
    str = cont->to_string();
    EXPECT_TRUE(str.find("consumed") != std::string::npos);
}
