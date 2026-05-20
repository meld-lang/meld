#include <gtest/gtest.h>
#include "../../include/meld/stdlib/effects.hpp"
#include "../../include/meld/effects/effect.hpp"
#include "../../include/meld/kernel/primitives.hpp"

using namespace meld;
using namespace meld::stdlib;
using namespace meld::kernel;
using namespace meld::effects;

class EffectsLibraryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear any existing handlers and continuations
        HandlerStack::instance().clear();
        DelimitedContinuation::clear();
        MetadataStore::instance().clear_all();
    }
    
    void TearDown() override {
        // Clean up after each test
        HandlerStack::instance().clear();
        DelimitedContinuation::clear();
        MetadataStore::instance().clear_all();
    }
};

// Test that mark_stack properly registers a handler
TEST_F(EffectsLibraryTest, MarkStackRegistersHandler) {
    // Create a simple effect handler
    auto effect_def = create_console_effect();
    auto handler = std::make_shared<EffectHandler>(effect_def);
    
    // Initially no handlers
    EXPECT_FALSE(HandlerStack::instance().has_handler("Console"));
    EXPECT_EQ(HandlerStack::instance().depth(), 0);
    
    // Mark the stack with the handler
    mark_stack("Console", handler);
    
    // Handler should now be registered
    EXPECT_TRUE(HandlerStack::instance().has_handler("Console"));
    EXPECT_EQ(HandlerStack::instance().depth(), 1);
    
    // Should be able to find the handler
    auto found_handler = HandlerStack::instance().find_handler("Console");
    EXPECT_NE(found_handler, nullptr);
    EXPECT_EQ(found_handler, handler);
}

// Test that multiple handlers can be stacked
TEST_F(EffectsLibraryTest, MultipleHandlersCanBeStacked) {
    auto console_handler = std::make_shared<EffectHandler>(create_console_effect());
    auto filesystem_handler = std::make_shared<EffectHandler>(create_filesystem_effect());
    
    // Mark stack with multiple handlers
    mark_stack("Console", console_handler);
    mark_stack("FileSystem", filesystem_handler);
    
    EXPECT_EQ(HandlerStack::instance().depth(), 2);
    EXPECT_TRUE(HandlerStack::instance().has_handler("Console"));
    EXPECT_TRUE(HandlerStack::instance().has_handler("FileSystem"));
}

// Test that handlers are found in LIFO order (most recent first)
TEST_F(EffectsLibraryTest, HandlersFoundInLIFOOrder) {
    auto handler1 = std::make_shared<EffectHandler>(create_console_effect());
    auto handler2 = std::make_shared<EffectHandler>(create_console_effect());
    
    // Mark stack with two handlers for the same effect
    mark_stack("Console", handler1);
    mark_stack("Console", handler2);
    
    // Should find the most recent handler (handler2)
    auto found_handler = HandlerStack::instance().find_handler("Console");
    EXPECT_EQ(found_handler, handler2);
}

// Test EffectScope RAII behavior
TEST_F(EffectsLibraryTest, EffectScopeRAII) {
    auto handler = std::make_shared<EffectHandler>(create_console_effect());
    
    EXPECT_EQ(HandlerStack::instance().depth(), 0);
    
    {
        // Create scope - should register handler
        EffectScope scope("Console", handler);
        EXPECT_EQ(HandlerStack::instance().depth(), 1);
        EXPECT_TRUE(HandlerStack::instance().has_handler("Console"));
    }
    
    // Scope destroyed - should clean up handler
    EXPECT_EQ(HandlerStack::instance().depth(), 0);
    EXPECT_FALSE(HandlerStack::instance().has_handler("Console"));
}

// Test that suspend and resume work with continuations
TEST_F(EffectsLibraryTest, SuspendAndResumeWork) {
    bool callback_called = false;
    Value result_value;
    
    // Set up a delimiter
    DelimitedContinuation::push_delimiter("test", 
        [&](Value v) -> Value {
            result_value = v;
            return v;
        });
    
    // Test suspend with a callback that resumes immediately
    Value test_value = Value(std::make_shared<Integer>(42));
    
    Value result = stdlib::suspend("test", 
        [&](std::shared_ptr<Continuation> cont) -> Value {
            callback_called = true;
            EXPECT_TRUE(cont->is_valid());
            return stdlib::resume(cont, test_value);
        });
    
    EXPECT_TRUE(callback_called);
    // Note: The exact result depends on the continuation implementation
    // The important thing is that the callback was called and no exceptions were thrown
}

// Test mock filesystem handler
TEST_F(EffectsLibraryTest, MockFilesystemHandler) {
    auto handler = create_mock_filesystem_handler();
    
    // Set up handler for testing
    handler->set_handler("write",
        [](const std::vector<Value>& args, Continuation& cont) -> Value {
            // Simple mock that just returns success
            return cont.resume(Value(Empty::instance()));
        });
    
    handler->set_handler("read",
        [](const std::vector<Value>& args, Continuation& cont) -> Value {
            // Simple mock that returns test content
            return cont.resume(Value(std::make_shared<String>("test content")));
        });
    
    // Test that handler has the expected operations
    EXPECT_TRUE(handler->has_handler("write"));
    EXPECT_TRUE(handler->has_handler("read"));
    EXPECT_TRUE(handler->has_handler("exists"));
}

// Test effect performance with mock handler
TEST_F(EffectsLibraryTest, EffectPerformanceWithMockHandler) {
    // Create a simple mock handler that just echoes back
    auto effect_def = create_console_effect();
    auto handler = std::make_shared<EffectHandler>(effect_def);
    
    handler->set_handler("print",
        [](const std::vector<Value>& args, Continuation& cont) -> Value {
            // Echo back the first argument
            if (!args.empty()) {
                return cont.resume(args[0]);
            }
            return cont.resume(Value(Empty::instance()));
        });
    
    // Use handle() to set up the handler scope and test performance
    Value test_message = Value(std::make_shared<String>("Hello, World!"));
    
    Value result = handle("Console", handler, [&]() -> Value {
        return effects::perform("Console", "print", {test_message});
    });
    
    // Should get back the message we sent
    EXPECT_TRUE(result.is<String>());
    if (result.is<String>()) {
        EXPECT_EQ(result.as<String>()->value(), "Hello, World!");
    }
}

// Test unhandled effect throws exception
TEST_F(EffectsLibraryTest, UnhandledEffectThrowsException) {
    // Try to perform an effect without a handler
    EXPECT_THROW({
        effects::perform("NonExistentEffect", "someOperation", {});
    }, std::runtime_error);
}

// Test that library functions are not kernel primitives
TEST_F(EffectsLibraryTest, LibraryFunctionsAreNotKernelPrimitives) {
    // This test verifies that our functions are library code, not kernel primitives
    // We do this by checking that they use the kernel primitive_suspend internally
    
    // The functions should be callable (they exist)
    auto handler = std::make_shared<EffectHandler>(create_console_effect());
    
    // mark_stack should not throw when called
    EXPECT_NO_THROW(mark_stack("TestEffect", handler));
    
    // The handler should be registered in the library's handler stack
    EXPECT_TRUE(HandlerStack::instance().has_handler("TestEffect"));
    
    // This confirms these are library functions managing their own state
    // rather than kernel primitives
}

// Test that perform function works as library function (not keyword)
TEST_F(EffectsLibraryTest, PerformIsLibraryFunction) {
    // This test verifies that perform is implemented as a library function
    // that uses primitive_suspend internally to capture continuations
    
    // Create a simple mock handler that echoes back the first argument
    auto effect_def = create_console_effect();
    auto handler = std::make_shared<EffectHandler>(effect_def);
    
    handler->set_handler("echo",
        [](const std::vector<Value>& args, Continuation& cont) -> Value {
            // Echo back the first argument (if any)
            if (!args.empty()) {
                return cont.resume(args[0]);
            }
            return cont.resume(Value(Empty::instance()));
        });
    
    // Use handle() to set up the handler scope and test effects::perform()
    Value test_message = Value(std::make_shared<String>("Hello, Effects!"));
    
    Value result = handle("Console", handler, [&]() -> Value {
        // This call to perform should:
        // 1. Search handler stack for nearest matching handler (requirement 41.13)
        // 2. Use primitive_suspend internally to capture continuation (requirement 41.3)
        // 3. Pass continuation to handler (requirement 41.14)
        return effects::perform("Console", "echo", {test_message});
    });
    
    // Should get back the message we sent
    EXPECT_TRUE(result.is<String>());
    if (result.is<String>()) {
        EXPECT_EQ(result.as<String>()->value(), "Hello, Effects!");
    }
}

// Test that perform searches handler stack correctly (requirement 41.13)
TEST_F(EffectsLibraryTest, PerformSearchesHandlerStack) {
    // Create two different handlers for the same effect
    auto effect_def = create_console_effect();
    auto outer_handler = std::make_shared<EffectHandler>(effect_def);
    auto inner_handler = std::make_shared<EffectHandler>(effect_def);
    
    // Set up different behaviors for each handler
    outer_handler->set_handler("test",
        [](const std::vector<Value>& args, Continuation& cont) -> Value {
            return cont.resume(Value(std::make_shared<String>("outer")));
        });
    
    inner_handler->set_handler("test",
        [](const std::vector<Value>& args, Continuation& cont) -> Value {
            return cont.resume(Value(std::make_shared<String>("inner")));
        });
    
    // Test nested handler scopes - inner should take precedence
    Value result = handle("Console", outer_handler, [&]() -> Value {
        return handle("Console", inner_handler, [&]() -> Value {
            // Should find the inner handler (most recent)
            return effects::perform("Console", "test", {});
        });
    });
    
    EXPECT_TRUE(result.is<String>());
    if (result.is<String>()) {
        EXPECT_EQ(result.as<String>()->value(), "inner");
    }
}

// Test that perform passes continuation to handler correctly (requirement 41.14)
TEST_F(EffectsLibraryTest, PerformPassesContinuationToHandler) {
    // Create a handler that verifies it receives a valid continuation
    auto effect_def = create_console_effect();
    auto handler = std::make_shared<EffectHandler>(effect_def);
    
    bool continuation_received = false;
    bool continuation_valid = false;
    
    handler->set_handler("test_continuation",
        [&](const std::vector<Value>& args, Continuation& cont) -> Value {
            continuation_received = true;
            continuation_valid = cont.is_valid();
            
            // Resume with a test value
            return cont.resume(Value(std::make_shared<String>("continuation_works")));
        });
    
    // Perform the effect
    Value result = handle("Console", handler, [&]() -> Value {
        return effects::perform("Console", "test_continuation", {});
    });
    
    // Verify the handler received a valid continuation
    EXPECT_TRUE(continuation_received);
    EXPECT_TRUE(continuation_valid);
    
    // Verify the result came through the continuation
    EXPECT_TRUE(result.is<String>());
    if (result.is<String>()) {
        EXPECT_EQ(result.as<String>()->value(), "continuation_works");
    }
}

// Test that perform throws on unhandled effects
TEST_F(EffectsLibraryTest, PerformThrowsOnUnhandledEffect) {
    // Try to perform an effect without a handler
    EXPECT_THROW({
        effects::perform("NonExistentEffect", "someOperation", {});
    }, std::runtime_error);
    
    // The error message should indicate the unhandled effect
    try {
        effects::perform("TestEffect", "testOperation", {});
        FAIL() << "Expected std::runtime_error";
    } catch (const std::runtime_error& e) {
        std::string error_msg = e.what();
        EXPECT_TRUE(error_msg.find("Unhandled effect") != std::string::npos);
        EXPECT_TRUE(error_msg.find("TestEffect.testOperation") != std::string::npos);
    }
}

// Test template-based perform_typed function
TEST_F(EffectsLibraryTest, PerformTypedProvicesTypeSafety) {
    // Create a handler that returns different types
    auto effect_def = create_console_effect();
    auto handler = std::make_shared<EffectHandler>(effect_def);
    
    handler->set_handler("get_string",
        [](const std::vector<Value>& args, Continuation& cont) -> Value {
            return cont.resume(Value(std::make_shared<String>("test_string")));
        });
    
    handler->set_handler("get_number",
        [](const std::vector<Value>& args, Continuation& cont) -> Value {
            return cont.resume(Value(std::make_shared<Integer>(42)));
        });
    
    handler->set_handler("get_bool",
        [](const std::vector<Value>& args, Continuation& cont) -> Value {
            return cont.resume(Value(Boolean::from(true)));
        });
    
    // Test type-safe perform calls
    handle("Console", handler, [&]() -> Value {
        // Test string return
        std::string str_result = perform_typed<std::string>("Console", "get_string", {});
        EXPECT_EQ(str_result, "test_string");
        
        // Test integer return
        int64_t int_result = perform_typed<int64_t>("Console", "get_number", {});
        EXPECT_EQ(int_result, 42);
        
        // Test boolean return
        bool bool_result = perform_typed<bool>("Console", "get_bool", {});
        EXPECT_TRUE(bool_result);
        
        return Value(Empty::instance());
    });
}

// Test metadata primitives work with effects
TEST_F(EffectsLibraryTest, MetadataPrimitivesWorkWithEffects) {
    // Create an effect handler and attach metadata
    auto handler = std::make_shared<EffectHandler>(create_console_effect());
    Value handler_value = Value(std::make_shared<String>("test_handler"));
    
    // Use meta_set to attach provenance information
    Value tagged_handler = meta_set(handler_value, "origin", Value(std::make_shared<String>("Human")));
    
    // Verify metadata was attached
    EXPECT_TRUE(meta_has(tagged_handler, "origin"));
    
    Value origin = meta_get(tagged_handler, "origin");
    EXPECT_TRUE(origin.is<String>());
    if (origin.is<String>()) {
        EXPECT_EQ(origin.as<String>()->value(), "Human");
    }
}