#include "../../include/meld/effects/effect.hpp"
#include <gtest/gtest.h>
#include <string>

using namespace meld::effects;
using namespace meld::kernel;

// Test effect definition
TEST(EffectTest, CreateEffectDefinition) {
    auto effect = std::make_shared<EffectDefinition>("TestEffect");
    
    EXPECT_EQ(effect->name(), "TestEffect");
    EXPECT_EQ(effect->operations().size(), 0);
}

TEST(EffectTest, AddOperation) {
    auto effect = std::make_shared<EffectDefinition>("TestEffect");
    effect->add_operation("testOp", {"string", "int"}, "bool");
    
    EXPECT_EQ(effect->operations().size(), 1);
    EXPECT_TRUE(effect->has_operation("testOp"));
    EXPECT_FALSE(effect->has_operation("nonexistent"));
}

TEST(EffectTest, GetOperation) {
    auto effect = std::make_shared<EffectDefinition>("TestEffect");
    effect->add_operation("testOp", {"string"}, "int");
    
    const auto* op = effect->get_operation("testOp");
    ASSERT_NE(op, nullptr);
    EXPECT_EQ(op->name, "testOp");
    EXPECT_EQ(op->parameter_types.size(), 1);
    EXPECT_EQ(op->return_type, "int");
}

// Test effect handler
TEST(EffectTest, CreateEffectHandler) {
    auto effect = std::make_shared<EffectDefinition>("TestEffect");
    effect->add_operation("testOp", {}, "int");
    
    auto handler = std::make_shared<EffectHandler>(effect);
    
    EXPECT_EQ(handler->effect().name(), "TestEffect");
    EXPECT_FALSE(handler->has_handler("testOp"));
}

TEST(EffectTest, SetHandler) {
    auto effect = std::make_shared<EffectDefinition>("TestEffect");
    effect->add_operation("testOp", {}, "int");
    
    auto handler = std::make_shared<EffectHandler>(effect);
    
    handler->set_handler("testOp", [](const std::vector<Value>& args, Continuation& cont) {
        return Value::from_int(42);
    });
    
    EXPECT_TRUE(handler->has_handler("testOp"));
}

// Test continuation
TEST(EffectTest, ContinuationResume) {
    bool resumed = false;
    int result_value = 0;
    
    Continuation cont([&](Value v) {
        resumed = true;
        result_value = v.as_int();
        return v;
    });
    
    EXPECT_TRUE(cont.is_valid());
    
    cont.resume(Value::from_int(42));
    
    EXPECT_TRUE(resumed);
    EXPECT_EQ(result_value, 42);
    EXPECT_FALSE(cont.is_valid());  // One-shot
}

// Test effect runtime
TEST(EffectTest, EffectRuntimePushPop) {
    auto& runtime = EffectRuntime::instance();
    runtime.clear_handlers();
    
    EXPECT_EQ(runtime.handler_stack_size(), 0);
    
    auto effect = std::make_shared<EffectDefinition>("TestEffect");
    auto handler = std::make_shared<EffectHandler>(effect);
    
    runtime.push_handler(handler);
    EXPECT_EQ(runtime.handler_stack_size(), 1);
    
    runtime.pop_handler();
    EXPECT_EQ(runtime.handler_stack_size(), 0);
    
    runtime.clear_handlers();
}

// TASK 35.7: Test pushScope/popScope functionality
TEST(EffectTest, EffectRuntimePushPopScope) {
    auto& runtime = EffectRuntime::instance();
    runtime.clear_handlers();
    
    EXPECT_EQ(runtime.handler_stack_size(), 0);
    EXPECT_EQ(runtime.scope_depth(), 0);
    
    // Create test effects and handlers
    auto effect1 = std::make_shared<EffectDefinition>("Effect1");
    auto effect2 = std::make_shared<EffectDefinition>("Effect2");
    auto handler1 = std::make_shared<EffectHandler>(effect1);
    auto handler2 = std::make_shared<EffectHandler>(effect2);
    
    // Test single handler scope
    runtime.pushScope(handler1);
    EXPECT_EQ(runtime.handler_stack_size(), 1);
    EXPECT_EQ(runtime.scope_depth(), 1);
    
    runtime.popScope();
    EXPECT_EQ(runtime.handler_stack_size(), 0);
    EXPECT_EQ(runtime.scope_depth(), 0);
    
    // Test multiple handlers in one scope
    std::vector<std::shared_ptr<EffectHandler>> handlers = {handler1, handler2};
    runtime.pushScope(handlers);
    EXPECT_EQ(runtime.handler_stack_size(), 2);
    EXPECT_EQ(runtime.scope_depth(), 1);
    
    runtime.popScope();
    EXPECT_EQ(runtime.handler_stack_size(), 0);
    EXPECT_EQ(runtime.scope_depth(), 0);
    
    runtime.clear_handlers();
}

// TASK 35.7: Test nested scopes with proper precedence
TEST(EffectTest, NestedScopesWithPrecedence) {
    auto& runtime = EffectRuntime::instance();
    runtime.clear_handlers();
    
    // Create effect and handlers
    auto effect = std::make_shared<EffectDefinition>("TestEffect");
    effect->add_operation("getValue", {}, "int");
    
    // Outer handler
    auto outer_handler = std::make_shared<EffectHandler>(effect);
    outer_handler->set_handler("getValue", [](const std::vector<Value>& args, Continuation& cont) {
        return Value::from_int(100);
    });
    
    // Inner handler (should take precedence)
    auto inner_handler = std::make_shared<EffectHandler>(effect);
    inner_handler->set_handler("getValue", [](const std::vector<Value>& args, Continuation& cont) {
        return Value::from_int(200);
    });
    
    // Push outer scope
    runtime.pushScope(outer_handler);
    EXPECT_EQ(runtime.scope_depth(), 1);
    
    // Verify outer handler is used
    Value result = runtime.perform_effect("TestEffect", "getValue", {});
    EXPECT_EQ(result.as_int(), 100);
    
    // Push inner scope (should take precedence)
    runtime.pushScope(inner_handler);
    EXPECT_EQ(runtime.scope_depth(), 2);
    
    // Verify inner handler is used (innermost takes precedence)
    result = runtime.perform_effect("TestEffect", "getValue", {});
    EXPECT_EQ(result.as_int(), 200);
    
    // Pop inner scope
    runtime.popScope();
    EXPECT_EQ(runtime.scope_depth(), 1);
    
    // Verify outer handler is used again
    result = runtime.perform_effect("TestEffect", "getValue", {});
    EXPECT_EQ(result.as_int(), 100);
    
    // Pop outer scope
    runtime.popScope();
    EXPECT_EQ(runtime.scope_depth(), 0);
    
    runtime.clear_handlers();
}

// TASK 35.7: Test handler search functionality
TEST(EffectTest, HandlerSearchInnermost) {
    auto& runtime = EffectRuntime::instance();
    runtime.clear_handlers();
    
    // Create effects
    auto effect1 = std::make_shared<EffectDefinition>("Effect1");
    effect1->add_operation("op1", {}, "int");
    
    auto effect2 = std::make_shared<EffectDefinition>("Effect2");
    effect2->add_operation("op2", {}, "string");
    
    // Create handlers
    auto handler1 = std::make_shared<EffectHandler>(effect1);
    handler1->set_handler("op1", [](const std::vector<Value>& args, Continuation& cont) {
        return Value::from_int(42);
    });
    
    auto handler2 = std::make_shared<EffectHandler>(effect2);
    handler2->set_handler("op2", [](const std::vector<Value>& args, Continuation& cont) {
        return Value::from_string("test");
    });
    
    // Push handlers in different scopes
    runtime.pushScope(handler1);
    runtime.pushScope(handler2);
    
    // Test handler search
    EXPECT_TRUE(runtime.has_handler("Effect1"));
    EXPECT_TRUE(runtime.has_handler("Effect2"));
    EXPECT_FALSE(runtime.has_handler("NonexistentEffect"));
    
    EXPECT_TRUE(runtime.has_handler("Effect1", "op1"));
    EXPECT_TRUE(runtime.has_handler("Effect2", "op2"));
    EXPECT_FALSE(runtime.has_handler("Effect1", "nonexistent"));
    
    // Test find_handler
    auto found1 = runtime.find_handler("Effect1");
    auto found2 = runtime.find_handler("Effect2");
    auto not_found = runtime.find_handler("NonexistentEffect");
    
    EXPECT_NE(found1, nullptr);
    EXPECT_NE(found2, nullptr);
    EXPECT_EQ(not_found, nullptr);
    
    EXPECT_EQ(found1->effect().name(), "Effect1");
    EXPECT_EQ(found2->effect().name(), "Effect2");
    
    runtime.clear_handlers();
}

// TASK 35.7: Test stack info functionality
TEST(EffectTest, StackInfo) {
    auto& runtime = EffectRuntime::instance();
    runtime.clear_handlers();
    
    // Create effects and handlers
    auto effect1 = std::make_shared<EffectDefinition>("Effect1");
    auto effect2 = std::make_shared<EffectDefinition>("Effect2");
    auto handler1 = std::make_shared<EffectHandler>(effect1);
    auto handler2 = std::make_shared<EffectHandler>(effect2);
    
    // Initial state
    auto info = runtime.get_stack_info();
    EXPECT_EQ(info.total_handlers, 0);
    EXPECT_EQ(info.total_scopes, 0);
    EXPECT_EQ(info.effect_names.size(), 0);
    EXPECT_EQ(info.scope_sizes.size(), 0);
    
    // Push first scope with one handler
    runtime.pushScope(handler1);
    info = runtime.get_stack_info();
    EXPECT_EQ(info.total_handlers, 1);
    EXPECT_EQ(info.total_scopes, 1);
    EXPECT_EQ(info.effect_names.size(), 1);
    EXPECT_EQ(info.effect_names[0], "Effect1");
    EXPECT_EQ(info.scope_sizes.size(), 1);
    EXPECT_EQ(info.scope_sizes[0], 1);
    
    // Push second scope with multiple handlers
    std::vector<std::shared_ptr<EffectHandler>> handlers = {handler1, handler2};
    runtime.pushScope(handlers);
    info = runtime.get_stack_info();
    EXPECT_EQ(info.total_handlers, 3); // 1 from first scope + 2 from second scope
    EXPECT_EQ(info.total_scopes, 2);
    EXPECT_EQ(info.effect_names.size(), 3);
    EXPECT_EQ(info.scope_sizes.size(), 2);
    EXPECT_EQ(info.scope_sizes[1], 2); // Second scope has 2 handlers
    
    runtime.clear_handlers();
}

TEST(EffectTest, PerformEffect) {
    auto& runtime = EffectRuntime::instance();
    runtime.clear_handlers();
    
    // Create effect
    auto effect = std::make_shared<EffectDefinition>("TestEffect");
    effect->add_operation("getValue", {}, "int");
    
    // Create handler
    auto handler = std::make_shared<EffectHandler>(effect);
    handler->set_handler("getValue", [](const std::vector<Value>& args, Continuation& cont) {
        return Value::from_int(99);
    });
    
    // Install handler
    runtime.push_handler(handler);
    
    // Perform effect
    Value result = runtime.perform_effect("TestEffect", "getValue", {});
    
    EXPECT_EQ(result.as_int(), 99);
    
    runtime.clear_handlers();
}

TEST(EffectTest, UnhandledEffect) {
    auto& runtime = EffectRuntime::instance();
    runtime.clear_handlers();
    
    EXPECT_THROW(
        runtime.perform_effect("NonexistentEffect", "someOp", {}),
        std::runtime_error
    );
    
    runtime.clear_handlers();
}

// Test handler scope (RAII)
TEST(EffectTest, HandlerScope) {
    auto& runtime = EffectRuntime::instance();
    runtime.clear_handlers();
    
    EXPECT_EQ(runtime.handler_stack_size(), 0);
    
    {
        auto effect = std::make_shared<EffectDefinition>("TestEffect");
        auto handler = std::make_shared<EffectHandler>(effect);
        
        HandlerScope scope(handler);
        EXPECT_EQ(runtime.handler_stack_size(), 1);
    }
    
    // Handler should be popped automatically
    EXPECT_EQ(runtime.handler_stack_size(), 0);
    
    runtime.clear_handlers();
}

// Test nested handlers
TEST(EffectTest, NestedHandlers) {
    auto& runtime = EffectRuntime::instance();
    runtime.clear_handlers();
    
    // Create effect
    auto effect = std::make_shared<EffectDefinition>("TestEffect");
    effect->add_operation("getValue", {}, "int");
    
    // Outer handler
    auto outer_handler = std::make_shared<EffectHandler>(effect);
    outer_handler->set_handler("getValue", [](const std::vector<Value>& args, Continuation& cont) {
        return Value::from_int(1);
    });
    
    // Inner handler (should take precedence)
    auto inner_handler = std::make_shared<EffectHandler>(effect);
    inner_handler->set_handler("getValue", [](const std::vector<Value>& args, Continuation& cont) {
        return Value::from_int(2);
    });
    
    runtime.push_handler(outer_handler);
    runtime.push_handler(inner_handler);
    
    // Inner handler should be used
    Value result = runtime.perform_effect("TestEffect", "getValue", {});
    EXPECT_EQ(result.as_int(), 2);
    
    runtime.pop_handler();
    
    // Now outer handler should be used
    result = runtime.perform_effect("TestEffect", "getValue", {});
    EXPECT_EQ(result.as_int(), 1);
    
    runtime.clear_handlers();
}

// Test built-in effects
TEST(EffectTest, FileSystemEffect) {
    auto fs_effect = create_filesystem_effect();
    
    EXPECT_EQ(fs_effect->name(), "FileSystem");
    EXPECT_TRUE(fs_effect->has_operation("read"));
    EXPECT_TRUE(fs_effect->has_operation("write"));
    EXPECT_TRUE(fs_effect->has_operation("delete"));
    EXPECT_TRUE(fs_effect->has_operation("exists"));
}

TEST(EffectTest, NetworkEffect) {
    auto net_effect = create_network_effect();
    
    EXPECT_EQ(net_effect->name(), "Network");
    EXPECT_TRUE(net_effect->has_operation("get"));
    EXPECT_TRUE(net_effect->has_operation("post"));
}

TEST(EffectTest, ConsoleEffect) {
    auto console_effect = create_console_effect();
    
    EXPECT_EQ(console_effect->name(), "Console");
    EXPECT_TRUE(console_effect->has_operation("print"));
    EXPECT_TRUE(console_effect->has_operation("readLine"));
}

TEST(EffectTest, RandomEffect) {
    auto random_effect = create_random_effect();
    
    EXPECT_EQ(random_effect->name(), "Random");
    EXPECT_TRUE(random_effect->has_operation("nextInt"));
    EXPECT_TRUE(random_effect->has_operation("nextFloat"));
}

TEST(EffectTest, TimeEffect) {
    auto time_effect = create_time_effect();
    
    EXPECT_EQ(time_effect->name(), "Time");
    EXPECT_TRUE(time_effect->has_operation("now"));
    EXPECT_TRUE(time_effect->has_operation("sleep"));
}

// Test sandboxing scenario
TEST(EffectTest, SandboxFileSystem) {
    auto& runtime = EffectRuntime::instance();
    runtime.clear_handlers();
    
    // Create FileSystem effect
    auto fs_effect = create_filesystem_effect();
    
    // Create sandboxing handler
    auto sandbox_handler = std::make_shared<EffectHandler>(fs_effect);
    
    std::vector<std::string> logged_operations;
    
    sandbox_handler->set_handler("write", [&](const std::vector<Value>& args, Continuation& cont) {
        // Log the operation instead of actually writing
        logged_operations.push_back("write: " + args[0].as_string());
        return Value::from_unit();
    });
    
    sandbox_handler->set_handler("read", [&](const std::vector<Value>& args, Continuation& cont) {
        // Return mock data
        logged_operations.push_back("read: " + args[0].as_string());
        return Value::from_string("mocked content");
    });
    
    runtime.push_handler(sandbox_handler);
    
    // Simulate code that performs file operations
    runtime.perform_effect("FileSystem", "write", {Value::from_string("/tmp/test.txt")});
    Value content = runtime.perform_effect("FileSystem", "read", {Value::from_string("/tmp/test.txt")});
    
    EXPECT_EQ(logged_operations.size(), 2);
    EXPECT_EQ(logged_operations[0], "write: /tmp/test.txt");
    EXPECT_EQ(logged_operations[1], "read: /tmp/test.txt");
    EXPECT_EQ(content.as_string(), "mocked content");
    
    runtime.clear_handlers();
}

// Test deterministic testing with Time effect
TEST(EffectTest, DeterministicTime) {
    auto& runtime = EffectRuntime::instance();
    runtime.clear_handlers();
    
    // Create Time effect
    auto time_effect = create_time_effect();
    
    // Create deterministic handler
    auto det_handler = std::make_shared<EffectHandler>(time_effect);
    
    int fixed_time = 1234567890;
    
    det_handler->set_handler("now", [&](const std::vector<Value>& args, Continuation& cont) {
        return Value::from_int(fixed_time);
    });
    
    det_handler->set_handler("sleep", [&](const std::vector<Value>& args, Continuation& cont) {
        // No-op in tests
        return Value::from_unit();
    });
    
    runtime.push_handler(det_handler);
    
    // Get "current" time (always returns fixed value)
    Value time1 = runtime.perform_effect("Time", "now", {});
    Value time2 = runtime.perform_effect("Time", "now", {});
    
    EXPECT_EQ(time1.as_int(), fixed_time);
    EXPECT_EQ(time2.as_int(), fixed_time);
    
    runtime.clear_handlers();
}

// int main(int argc, char** argv) {
//     testing::InitGoogleTest(&argc, argv);
//     return RUN_ALL_TESTS();
// }
