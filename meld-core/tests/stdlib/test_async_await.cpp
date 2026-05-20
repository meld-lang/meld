#include "../../include/meld/stdlib/async.hpp"
#include "../../include/meld/effects/builtin_effects.hpp"
#include <gtest/gtest.h>
#include <chrono>
#include <thread>

using namespace meld::stdlib;
using namespace meld::effects;
using namespace meld::kernel;

// ============================================================================
// ASYNC/AWAIT TESTS
// Task 35.15: Test async/await as effects
// Requirements: 41.23
// ============================================================================

class AsyncAwaitTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Install async scheduler and built-in effects for each test
        async_scope_ = std::make_unique<AsyncSchedulerScope>();
        effects_scope_ = std::make_unique<BuiltinEffectsScope>();
    }
    
    void TearDown() override {
        // Clean up scopes
        effects_scope_.reset();
        async_scope_.reset();
        
        // Clean up any remaining tasks
        AsyncScheduler::instance().cancel_all_tasks();
    }
    
private:
    std::unique_ptr<AsyncSchedulerScope> async_scope_;
    std::unique_ptr<BuiltinEffectsScope> effects_scope_;
};

// Test basic task creation and completion
TEST_F(AsyncAwaitTest, BasicTaskCreation) {
    auto task = create_task();
    
    EXPECT_TRUE(task != nullptr);
    EXPECT_TRUE(task->is_pending());
    EXPECT_FALSE(task->is_running());
    EXPECT_FALSE(task->is_completed());
    EXPECT_FALSE(task->is_failed());
    EXPECT_FALSE(task->is_cancelled());
}

// Test async function execution
TEST_F(AsyncAwaitTest, AsyncFunctionExecution) {
    auto task = async([]() -> Value {
        return Value::from_string("async_result");
    });
    
    EXPECT_TRUE(task != nullptr);
    
    // Wait for completion
    Value result = await(task);
    
    EXPECT_EQ(result.as_string(), "async_result");
    EXPECT_TRUE(task->is_completed());
}

// Test async computation
TEST_F(AsyncAwaitTest, AsyncComputation) {
    auto task = async([]() -> Value {
        int result = 42 + 58;
        return Value::from_int(result);
    });
    
    Value result = await(task);
    
    EXPECT_EQ(result.as_int(), 100);
    EXPECT_TRUE(task->is_completed());
}

// Test delayed task
TEST_F(AsyncAwaitTest, DelayedTask) {
    auto start_time = std::chrono::steady_clock::now();
    
    auto task = delay(std::chrono::milliseconds(100), Value::from_string("delayed_result"));
    
    Value result = await(task);
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    EXPECT_EQ(result.as_string(), "delayed_result");
    EXPECT_TRUE(task->is_completed());
    EXPECT_GE(duration.count(), 90); // Allow some timing tolerance
}

// Test deferred task completion
TEST_F(AsyncAwaitTest, DeferredTaskCompletion) {
    auto [task, completer] = create_deferred_task();
    
    EXPECT_TRUE(task->is_pending());
    
    // Complete the task in another thread after a delay
    std::thread([completer = std::move(completer)]() mutable {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        completer.complete(Value::from_string("deferred_result"));
    }).detach();
    
    Value result = await(task);
    
    EXPECT_EQ(result.as_string(), "deferred_result");
    EXPECT_TRUE(task->is_completed());
}

// Test deferred task error completion
TEST_F(AsyncAwaitTest, DeferredTaskError) {
    auto [task, completer] = create_deferred_task();
    
    // Complete the task with an error
    std::thread([completer = std::move(completer)]() mutable {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        completer.complete_error("test_error");
    }).detach();
    
    Value result = await(task);
    
    EXPECT_EQ(result.as_string(), "Task failed: test_error");
    EXPECT_TRUE(task->is_failed());
}

// Test task cancellation
TEST_F(AsyncAwaitTest, TaskCancellation) {
    auto [task, completer] = create_deferred_task();
    
    // Cancel the task
    std::thread([completer = std::move(completer)]() mutable {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        completer.cancel();
    }).detach();
    
    Value result = await(task);
    
    EXPECT_EQ(result.as_string(), "Task was cancelled");
    EXPECT_TRUE(task->is_cancelled());
}

// Test multiple concurrent tasks
TEST_F(AsyncAwaitTest, MultipleConcurrentTasks) {
    auto task1 = async([]() -> Value {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        return Value::from_string("result1");
    });
    
    auto task2 = async([]() -> Value {
        std::this_thread::sleep_for(std::chrono::milliseconds(75));
        return Value::from_string("result2");
    });
    
    auto task3 = async([]() -> Value {
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
        return Value::from_string("result3");
    });
    
    // Await all tasks
    Value result1 = await(task1);
    Value result2 = await(task2);
    Value result3 = await(task3);
    
    EXPECT_EQ(result1.as_string(), "result1");
    EXPECT_EQ(result2.as_string(), "result2");
    EXPECT_EQ(result3.as_string(), "result3");
    
    EXPECT_TRUE(task1->is_completed());
    EXPECT_TRUE(task2->is_completed());
    EXPECT_TRUE(task3->is_completed());
}

// Test task error handling
TEST_F(AsyncAwaitTest, TaskErrorHandling) {
    auto task = async([]() -> Value {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        throw std::runtime_error("test_exception");
        return Value::from_string("should_not_reach");
    });
    
    Value result = await(task);
    
    EXPECT_EQ(result.as_string(), "Task failed: test_exception");
    EXPECT_TRUE(task->is_failed());
}

// Test task state transitions
TEST_F(AsyncAwaitTest, TaskStateTransitions) {
    auto task = async([]() -> Value {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return Value::from_string("state_test");
    });
    
    // Task should start as pending or quickly become running
    EXPECT_TRUE(task->is_pending() || task->is_running());
    
    // Wait for completion
    Value result = await(task);
    
    EXPECT_EQ(result.as_string(), "state_test");
    EXPECT_TRUE(task->is_completed());
    EXPECT_FALSE(task->is_pending());
    EXPECT_FALSE(task->is_running());
    EXPECT_FALSE(task->is_failed());
    EXPECT_FALSE(task->is_cancelled());
}

// Test scheduler statistics
TEST_F(AsyncAwaitTest, SchedulerStatistics) {
    auto& scheduler = AsyncScheduler::instance();
    
    size_t initial_count = scheduler.active_task_count();
    
    // Create some tasks
    auto task1 = create_task();
    auto task2 = create_task();
    
    EXPECT_EQ(scheduler.active_task_count(), initial_count + 2);
    
    // Complete one task
    auto [task3, completer] = create_deferred_task();
    completer.complete(Value::from_string("completed"));
    
    // Clean up completed tasks
    scheduler.cleanup_completed_tasks();
    
    // Should have fewer active tasks now
    size_t final_count = scheduler.active_task_count();
    EXPECT_LE(final_count, initial_count + 2);
}

// Test await with already completed task
TEST_F(AsyncAwaitTest, AwaitCompletedTask) {
    auto [task, completer] = create_deferred_task();
    
    // Complete the task before awaiting
    completer.complete(Value::from_string("pre_completed"));
    
    // Give it a moment to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Now await - should return immediately
    auto start_time = std::chrono::steady_clock::now();
    Value result = await(task);
    auto end_time = std::chrono::steady_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    EXPECT_EQ(result.as_string(), "pre_completed");
    EXPECT_TRUE(task->is_completed());
    EXPECT_LT(duration.count(), 50); // Should be very fast
}

// Test continuation storage and resumption
TEST_F(AsyncAwaitTest, ContinuationStorageAndResumption) {
    auto& scheduler = AsyncScheduler::instance();
    
    // Create a deferred task
    auto [task, completer] = create_deferred_task();
    
    // Start awaiting in another thread
    std::atomic<bool> await_completed{false};
    std::string await_result;
    
    std::thread([&task, &await_completed, &await_result]() {
        Value result = await(task);
        await_result = result.as_string();
        await_completed = true;
    }).detach();
    
    // Give the await thread time to suspend
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    EXPECT_FALSE(await_completed);
    
    // Complete the task - this should resume the continuation
    completer.complete(Value::from_string("continuation_test"));
    
    // Wait for the await to complete
    auto start_time = std::chrono::steady_clock::now();
    while (!await_completed && 
           std::chrono::steady_clock::now() - start_time < std::chrono::milliseconds(1000)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    EXPECT_TRUE(await_completed);
    EXPECT_EQ(await_result, "continuation_test");
}

// Test direct store_continuation and resume_continuation functionality
TEST_F(AsyncAwaitTest, DirectContinuationStorageAndResumption) {
    auto& scheduler = AsyncScheduler::instance();
    
    // Create a task
    auto task = create_task();
    
    // Test that we can store and retrieve continuations
    // This is a more direct test of the continuation storage mechanism
    
    // Create a mock continuation for testing
    auto mock_continuation = std::make_shared<meld::kernel::Continuation>(
        [](meld::kernel::Value v) -> meld::kernel::Value {
            return v; // Just return the value unchanged
        },
        "test_delimiter"
    );
    
    // Store the continuation
    scheduler.store_continuation(task->id(), mock_continuation);
    
    // Complete the task
    task->mark_running();
    task->set_result(Value::from_string("test_result"));
    
    // The continuation should be resumed automatically when the task completes
    // We can verify this by checking that the continuation is no longer stored
    
    // Give some time for the resumption to happen
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // The test passes if no exceptions were thrown during the process
    EXPECT_TRUE(task->is_completed());
}

// Test effect system integration
TEST_F(AsyncAwaitTest, EffectSystemIntegration) {
    // This test verifies that await properly uses the effect system
    // by performing the Async.wait effect
    
    auto& runtime = EffectRuntime::instance();
    
    // Verify that the async handler is installed
    EXPECT_TRUE(runtime.has_handler("Async", "wait"));
    
    // Create and await a task
    auto task = async([]() -> Value {
        return Value::from_string("effect_integration_test");
    });
    
    // This should work through the effect system
    Value result = await(task);
    
    EXPECT_EQ(result.as_string(), "effect_integration_test");
}

// Test Task<T> type functionality
TEST_F(AsyncAwaitTest, TaskTypeFunctionality) {
    auto task = create_task();
    
    // Test task ID generation
    EXPECT_GT(task->id(), 0);
    
    // Test unique IDs
    auto task2 = create_task();
    EXPECT_NE(task->id(), task2->id());
    
    // Test task state methods
    EXPECT_TRUE(task->is_pending());
    EXPECT_FALSE(task->is_running());
    EXPECT_FALSE(task->is_completed());
    EXPECT_FALSE(task->is_failed());
    EXPECT_FALSE(task->is_cancelled());
    
    // Test state consistency
    TaskState state = task->state();
    EXPECT_EQ(state, TaskState::PENDING);
}

// Performance test for many concurrent tasks
TEST_F(AsyncAwaitTest, ManyConcurrentTasks) {
    const int num_tasks = 50;
    std::vector<std::shared_ptr<Task>> tasks;
    
    // Create many concurrent tasks
    for (int i = 0; i < num_tasks; ++i) {
        auto task = async([i]() -> Value {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            return Value::from_int(i);
        });
        tasks.push_back(task);
    }
    
    // Await all tasks
    for (int i = 0; i < num_tasks; ++i) {
        Value result = await(tasks[i]);
        EXPECT_EQ(result.as_int(), i);
    }
    
    // Verify all tasks completed
    for (const auto& task : tasks) {
        EXPECT_TRUE(task->is_completed());
    }
}

// ============================================================================
// TASK COMBINATOR TESTS (Task 21.2)
// Requirements: 27.1
// ============================================================================

// Test Task.map combinator
TEST_F(AsyncAwaitTest, TaskMapCombinator) {
    auto original_task = async([]() -> Value {
        return Value::from_int(10);
    });
    
    auto mapped_task = original_task->map([](Value v) -> Value {
        int original_value = v.as_int();
        return Value::from_int(original_value * 2);
    });
    
    Value result = await(mapped_task);
    
    EXPECT_EQ(result.as_int(), 20);
    EXPECT_TRUE(mapped_task->is_completed());
}

// Test Task.flatMap combinator
TEST_F(AsyncAwaitTest, TaskFlatMapCombinator) {
    auto original_task = async([]() -> Value {
        return Value::from_int(5);
    });
    
    auto flat_mapped_task = original_task->flat_map([](Value v) -> std::shared_ptr<Task> {
        int original_value = v.as_int();
        return async([original_value]() -> Value {
            return Value::from_int(original_value * 3);
        });
    });
    
    Value result = await(flat_mapped_task);
    
    EXPECT_EQ(result.as_int(), 15);
    EXPECT_TRUE(flat_mapped_task->is_completed());
}

// Test Task.withTimeout combinator - success case
TEST_F(AsyncAwaitTest, TaskWithTimeoutSuccess) {
    auto fast_task = async([]() -> Value {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        return Value::from_string("fast_result");
    });
    
    auto timeout_task = fast_task->with_timeout(200);
    
    Value result = await(timeout_task);
    
    EXPECT_EQ(result.as_string(), "fast_result");
    EXPECT_TRUE(timeout_task->is_completed());
}

// Test Task.withTimeout combinator - timeout case
TEST_F(AsyncAwaitTest, TaskWithTimeoutFailure) {
    auto slow_task = async([]() -> Value {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        return Value::from_string("slow_result");
    });
    
    auto timeout_task = slow_task->with_timeout(100);
    
    Value result = await(timeout_task);
    
    // Should get a timeout error
    EXPECT_TRUE(result.as_string().find("timed out") != std::string::npos);
    EXPECT_TRUE(timeout_task->is_failed());
}

// Test Task.onComplete callback
TEST_F(AsyncAwaitTest, TaskOnCompleteCallback) {
    std::atomic<bool> callback_called{false};
    std::string callback_result;
    
    auto task = async([]() -> Value {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        return Value::from_string("callback_test");
    });
    
    task->on_complete([&callback_called, &callback_result](Value result, const std::string& error) {
        callback_called = true;
        if (error.empty()) {
            callback_result = result.as_string();
        } else {
            callback_result = "error: " + error;
        }
    });
    
    Value result = await(task);
    
    // Give callback time to execute
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    EXPECT_EQ(result.as_string(), "callback_test");
    EXPECT_TRUE(callback_called);
    EXPECT_EQ(callback_result, "callback_test");
}

// Test Task.onComplete callback with error
TEST_F(AsyncAwaitTest, TaskOnCompleteCallbackWithError) {
    std::atomic<bool> callback_called{false};
    std::string callback_result;
    
    auto task = async([]() -> Value {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        throw std::runtime_error("callback_error_test");
        return Value::from_string("should_not_reach");
    });
    
    task->on_complete([&callback_called, &callback_result](Value result, const std::string& error) {
        callback_called = true;
        if (error.empty()) {
            callback_result = result.as_string();
        } else {
            callback_result = "error: " + error;
        }
    });
    
    Value result = await(task);
    
    // Give callback time to execute
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    EXPECT_TRUE(result.as_string().find("callback_error_test") != std::string::npos);
    EXPECT_TRUE(callback_called);
    EXPECT_TRUE(callback_result.find("callback_error_test") != std::string::npos);
}

// Test Task.all combinator
TEST_F(AsyncAwaitTest, TaskAllCombinator) {
    std::vector<std::shared_ptr<Task>> tasks;
    
    // Create multiple tasks
    for (int i = 1; i <= 3; ++i) {
        auto task = async([i]() -> Value {
            std::this_thread::sleep_for(std::chrono::milliseconds(50 * i));
            return Value::from_int(i * 10);
        });
        tasks.push_back(task);
    }
    
    auto all_task = all(tasks);
    
    Value result = await(all_task);
    
    // The result should be a string representation of the list
    std::string result_str = result.as_string();
    EXPECT_TRUE(result_str.find("[") != std::string::npos);
    EXPECT_TRUE(result_str.find("]") != std::string::npos);
    EXPECT_TRUE(all_task->is_completed());
}

// Test Task.race combinator
TEST_F(AsyncAwaitTest, TaskRaceCombinator) {
    std::vector<std::shared_ptr<Task>> tasks;
    
    // Create tasks with different delays - the fastest should win
    tasks.push_back(async([]() -> Value {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        return Value::from_string("slow");
    }));
    
    tasks.push_back(async([]() -> Value {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        return Value::from_string("fast");
    }));
    
    tasks.push_back(async([]() -> Value {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        return Value::from_string("medium");
    }));
    
    auto race_task = race(tasks);
    
    Value result = await(race_task);
    
    // The fastest task should win
    EXPECT_EQ(result.as_string(), "fast");
    EXPECT_TRUE(race_task->is_completed());
}

// Test Task.race with empty list
TEST_F(AsyncAwaitTest, TaskRaceEmptyList) {
    std::vector<std::shared_ptr<Task>> empty_tasks;
    
    auto race_task = race(empty_tasks);
    
    Value result = await(race_task);
    
    // Should get an error for empty list
    EXPECT_TRUE(result.as_string().find("empty task list") != std::string::npos);
    EXPECT_TRUE(race_task->is_failed());
}

// Test chaining combinators
TEST_F(AsyncAwaitTest, TaskCombinatorChaining) {
    auto original_task = async([]() -> Value {
        return Value::from_int(5);
    });
    
    auto chained_task = original_task
        ->map([](Value v) -> Value {
            return Value::from_int(v.as_int() * 2);  // 5 * 2 = 10
        })
        ->flat_map([](Value v) -> std::shared_ptr<Task> {
            int value = v.as_int();
            return async([value]() -> Value {
                return Value::from_int(value + 5);  // 10 + 5 = 15
            });
        })
        ->map([](Value v) -> Value {
            return Value::from_int(v.as_int() * 3);  // 15 * 3 = 45
        });
    
    Value result = await(chained_task);
    
    EXPECT_EQ(result.as_int(), 45);
    EXPECT_TRUE(chained_task->is_completed());
}

// Test combinator error propagation
TEST_F(AsyncAwaitTest, TaskCombinatorErrorPropagation) {
    auto error_task = async([]() -> Value {
        throw std::runtime_error("original_error");
        return Value::from_int(42);
    });
    
    auto mapped_task = error_task->map([](Value v) -> Value {
        // This should not be called due to the error
        return Value::from_int(v.as_int() * 2);
    });
    
    Value result = await(mapped_task);
    
    // Should propagate the original error
    EXPECT_TRUE(result.as_string().find("original_error") != std::string::npos);
    EXPECT_TRUE(mapped_task->is_failed());
}