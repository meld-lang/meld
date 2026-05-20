#include "meld/stdlib/async.hpp"
#include "meld/effects/effect.hpp"
#include "meld/effects/builtin_effects.hpp"
#include <iostream>

namespace meld::stdlib {

// Thread-local storage for current task's cancellation token
thread_local CancellationToken Task::current_cancellation_token_;

// ============================================================================
// ASYNC EFFECT HANDLER IMPLEMENTATION
// Task 35.15: Implement async/await as effects
// Requirements: 41.23
// ============================================================================

// Create enhanced async effect handler with continuation storage
// This is the async scheduler mentioned in the task requirements
std::shared_ptr<meld::effects::EffectHandler> create_async_scheduler_handler() {
    auto effect = meld::effects::create_async_effect();
    auto handler = std::make_shared<meld::effects::EffectHandler>(effect);
    
    // wait(task_id: int) -> string
    // This is the core implementation of await as an effect
    // Requirements: 41.23, 41B.2, 41B.8
    handler->set_enhanced_handler("wait", [](const std::vector<kernel::Value>& args, meld::effects::EffectContinuation& cont) -> kernel::Value {
        if (args.size() != 1) {
            throw std::runtime_error("Async.wait requires 1 argument (task_id)");
        }
        
        try {
            // Extract task ID from arguments
            Task::TaskId task_id = static_cast<Task::TaskId>(args[0].as_int());
            
            // Get the task from the scheduler
            auto& scheduler = AsyncScheduler::instance();
            auto task = scheduler.get_task(task_id);
            
            if (!task) {
                throw std::runtime_error("Task not found: " + std::to_string(task_id));
            }
            
            // Check for cancellation before waiting
            if (task->is_cancelled()) {
                throw CancellationException("Task was cancelled before await");
            }
            
            // Check if task is already completed
            if (task->is_completed()) {
                // Task is already done, return result immediately
                if (task->is_failed()) {
                    return cont.resume(kernel::Value::from_string("Task failed: " + task->get_error()));
                } else if (task->is_cancelled()) {
                    throw CancellationException("Task was cancelled");
                } else {
                    return cont.resume(task->get_result());
                }
            }
            
            // Task is not completed yet, store the continuation for later resumption
            // This is the key functionality: storing continuations for later resumption
            // Requirements: 41.23, 41B.8
            auto kernel_cont = cont.get_kernel_continuation();
            if (!kernel_cont) {
                throw std::runtime_error("Invalid continuation for async wait");
            }
            
            // Store the continuation in the scheduler
            scheduler.store_continuation(task_id, kernel_cont);
            
            // The continuation will be resumed when the task completes
            // We don't call cont.resume() here - the scheduler will do it later
            // This implements the "store continuations for later resumption" requirement
            
            // Return a placeholder value (this won't actually be used since we're not resuming)
            // The actual resumption happens in AsyncScheduler::resume_continuation
            return kernel::Value::from_string("async_wait_suspended");
            
        } catch (const CancellationException& e) {
            // Re-throw cancellation exceptions
            throw;
        } catch (const std::exception& e) {
            throw std::runtime_error("Async.wait failed: " + std::string(e.what()));
        }
    });
    
    return handler;
}

// Install the async scheduler in the effect runtime
void install_async_scheduler() {
    auto& runtime = meld::effects::EffectRuntime::instance();
    auto handler = create_async_scheduler_handler();
    runtime.push_handler(handler);
}

// Uninstall the async scheduler from the effect runtime
void uninstall_async_scheduler() {
    auto& runtime = meld::effects::EffectRuntime::instance();
    runtime.pop_handler();
}

// RAII helper for async scheduler
AsyncSchedulerScope::AsyncSchedulerScope() {
    install_async_scheduler();
}

AsyncSchedulerScope::~AsyncSchedulerScope() {
    uninstall_async_scheduler();
}

} // namespace meld::stdlib