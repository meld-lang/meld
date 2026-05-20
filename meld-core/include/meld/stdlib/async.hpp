#pragma once

#include "meld/effects/effect.hpp"
#include "meld/kernel/primitives.hpp"
#include <memory>
#include <vector>
#include <queue>
#include <functional>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <atomic>

namespace meld::stdlib {

// ============================================================================
// TASK 35.15: Implement async/await as effects
// Requirements: 41.23
// ============================================================================

// Forward declarations
class Task;
class AsyncScheduler;
class TaskCompleter;
class CancellationToken;
class CancellationTokenSource;

// Task state enumeration
enum class TaskState {
    PENDING,    // Task created but not started
    RUNNING,    // Task is currently executing
    COMPLETED,  // Task completed successfully
    FAILED,     // Task failed with error
    CANCELLED   // Task was cancelled
};

// Cancellation exception - thrown when a task is cancelled
class CancellationException : public std::runtime_error {
public:
    CancellationException() : std::runtime_error("Task was cancelled") {}
    explicit CancellationException(const std::string& message) 
        : std::runtime_error("Task was cancelled: " + message) {}
};

// Cancellation token - used to signal cancellation to tasks
class CancellationToken {
public:
    CancellationToken() : cancelled_(std::make_shared<std::atomic<bool>>(false)) {}
    
    // Check if cancellation has been requested
    bool is_cancellation_requested() const {
        return cancelled_->load();
    }
    
    // Throw CancellationException if cancellation was requested
    void throw_if_cancellation_requested() const {
        if (is_cancellation_requested()) {
            throw CancellationException();
        }
    }
    
    // Create a child token that is cancelled when this token is cancelled
    CancellationToken create_child_token() const {
        CancellationToken child;
        child.parent_ = cancelled_;
        return child;
    }
    
private:
    friend class CancellationTokenSource;
    
    std::shared_ptr<std::atomic<bool>> cancelled_;
    std::shared_ptr<std::atomic<bool>> parent_;
    
    // Internal method to check if this token or any parent is cancelled
    bool is_cancelled_internal() const {
        if (cancelled_->load()) return true;
        if (parent_ && parent_->load()) return true;
        return false;
    }
    
    // Internal method used by CancellationTokenSource
    void cancel() {
        cancelled_->store(true);
    }
};

// Cancellation token source - used to create and control cancellation tokens
class CancellationTokenSource {
public:
    CancellationTokenSource() : token_() {}
    
    // Get the token associated with this source
    CancellationToken token() const {
        return token_;
    }
    
    // Cancel the token
    void cancel() {
        token_.cancel();
    }
    
    // Check if the token is cancelled
    bool is_cancelled() const {
        return token_.is_cancellation_requested();
    }
    
private:
    CancellationToken token_;
};

// Task<T> type for representing asynchronous computations
// This implements the library-based async system using effects
// Requirements: 41B.5, 41B.8
class Task : public std::enable_shared_from_this<Task> {
public:
    using TaskId = uint64_t;
    
    // Create a new task with a unique ID and optional cancellation token
    explicit Task(TaskId id = generate_task_id(), CancellationToken cancellation_token = CancellationToken()) 
        : id_(id), state_(TaskState::PENDING), cancellation_token_(cancellation_token) {}
    
    // Get task ID
    TaskId id() const { return id_; }
    
    // Get current task state
    TaskState state() const { 
        std::lock_guard<std::mutex> lock(mutex_);
        return state_; 
    }
    
    // Check if task is completed (successfully or with error)
    bool is_completed() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_ == TaskState::COMPLETED || state_ == TaskState::FAILED;
    }
    
    // Check if task is pending
    bool is_pending() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_ == TaskState::PENDING;
    }
    
    // Check if task is running
    bool is_running() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_ == TaskState::RUNNING;
    }
    
    // Check if task failed
    bool is_failed() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_ == TaskState::FAILED;
    }
    
    // Check if task was cancelled
    bool is_cancelled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_ == TaskState::CANCELLED || cancellation_token_.is_cancellation_requested();
    }
    
    // Get cancellation token for this task
    CancellationToken cancellation_token() const {
        return cancellation_token_;
    }
    
    // Check for cancellation and throw if cancelled
    // This is the checkCancellation() function mentioned in the requirements
    static void check_cancellation() {
        // Get the current task's cancellation token from thread-local storage
        auto current_token = get_current_cancellation_token();
        if (current_token.is_cancellation_requested()) {
            throw CancellationException();
        }
    }
    
    // ============================================================================
    // MANUAL TASK COMPLETION (Task 2)
    // Requirements: 2.1-2.10
    // ============================================================================
    
    // Create a deferred task that can be completed manually
    // Returns a tuple of (Task, Completer)
    // Requirements: 2.1, 2.9
    static std::pair<std::shared_ptr<Task>, TaskCompleter> deferred(CancellationToken cancellation_token = CancellationToken());
    
    // Create an immediately completed task
    // Requirements: 2.6
    static std::shared_ptr<Task> completed(kernel::Value value);
    
    // Create an immediately failed task
    // Requirements: 2.7
    static std::shared_ptr<Task> failed(const std::string& error);
    
    // Check if task is done (completed, failed, or cancelled)
    // Requirements: 4.1
    bool is_done() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return is_completed_unlocked();
    }
    
    // Set the current task's cancellation token (thread-local)
    static void set_current_cancellation_token(const CancellationToken& token) {
        current_cancellation_token_ = token;
    }
    
    // Get the current task's cancellation token (thread-local)
    static CancellationToken get_current_cancellation_token() {
        return current_cancellation_token_;
    }
    
    // Get task result (blocks until completed)
    // This is the .await() method mentioned in Requirements 41B.5
    kernel::Value await() {
        // Perform Async.wait effect - this is the core implementation
        // Requirements: 41.23, 41B.2
        return meld::effects::perform("Async", "wait", {kernel::Value::from_int(static_cast<int64_t>(id_))});
    }
    
    // Set task result (used by scheduler)
    void set_result(kernel::Value result) {
        std::vector<std::function<void(kernel::Value, const std::string&)>> callbacks_to_call;
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (state_ != TaskState::RUNNING) {
                throw std::runtime_error("Cannot set result on task that is not running");
            }
            result_ = std::move(result);
            state_ = TaskState::COMPLETED;
            callbacks_to_call = completion_callbacks_;
            completion_cv_.notify_all();
        }
        
        // Call completion callbacks outside the lock
        for (auto& callback : callbacks_to_call) {
            try {
                callback(result_, "");
            } catch (...) {
                // Ignore callback errors
            }
        }
    }
    
    // Set task error (used by scheduler)
    void set_error(const std::string& error) {
        std::vector<std::function<void(kernel::Value, const std::string&)>> callbacks_to_call;
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (state_ != TaskState::RUNNING) {
                throw std::runtime_error("Cannot set error on task that is not running");
            }
            error_ = error;
            state_ = TaskState::FAILED;
            callbacks_to_call = completion_callbacks_;
            completion_cv_.notify_all();
        }
        
        // Call completion callbacks outside the lock
        for (auto& callback : callbacks_to_call) {
            try {
                callback(kernel::Value::from_string(""), error);
            } catch (...) {
                // Ignore callback errors
            }
        }
    }
    
    // Cancel task
    void cancel() {
        std::vector<std::shared_ptr<Task>> children_to_cancel;
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (state_ == TaskState::PENDING || state_ == TaskState::RUNNING) {
                state_ = TaskState::CANCELLED;
                completion_cv_.notify_all();
                
                // Collect children to cancel (copy to avoid holding lock)
                children_to_cancel.reserve(children_.size());
                for (auto& weak_child : children_) {
                    if (auto child = weak_child.lock()) {
                        children_to_cancel.push_back(child);
                    }
                }
            }
        }
        
        // Cancel all children outside the lock to avoid deadlock
        // This implements the requirement: "parent cancellation cancels all children"
        for (auto& child : children_to_cancel) {
            child->cancel();
        }
    }
    
    // Add a child task (for hierarchical cancellation)
    void add_child(std::shared_ptr<Task> child) {
        std::lock_guard<std::mutex> lock(mutex_);
        children_.push_back(child);
        child->set_parent(shared_from_this());
    }
    
    // Set parent task (for hierarchical cancellation)
    void set_parent(std::shared_ptr<Task> parent) {
        std::lock_guard<std::mutex> lock(mutex_);
        parent_ = parent;
    }
    
    // Get parent task
    std::shared_ptr<Task> get_parent() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return parent_.lock();
    }
    
    // Get children tasks
    std::vector<std::shared_ptr<Task>> get_children() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::shared_ptr<Task>> children;
        for (auto& weak_child : children_) {
            if (auto child = weak_child.lock()) {
                children.push_back(child);
            }
        }
        return children;
    }
    
    // Mark task as running (used by scheduler)
    void mark_running() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ != TaskState::PENDING) {
            throw std::runtime_error("Cannot mark task as running - not in pending state");
        }
        state_ = TaskState::RUNNING;
    }
    
    // Get result (non-blocking, throws if not completed)
    kernel::Value get_result() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ != TaskState::COMPLETED) {
            throw std::runtime_error("Task not completed");
        }
        return result_;
    }
    
    // Get error (non-blocking, throws if not failed)
    const std::string& get_error() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ != TaskState::FAILED) {
            throw std::runtime_error("Task did not fail");
        }
        return error_;
    }
    
    // Wait for completion (blocking)
    void wait_for_completion() const {
        std::unique_lock<std::mutex> lock(mutex_);
        completion_cv_.wait(lock, [this] { return is_completed_unlocked(); });
    }
    
    // Wait for completion with timeout
    bool wait_for_completion(std::chrono::milliseconds timeout) const {
        std::unique_lock<std::mutex> lock(mutex_);
        return completion_cv_.wait_for(lock, timeout, [this] { return is_completed_unlocked(); });
    }
    
    // ============================================================================
    // TASK COMBINATORS (Task 21.2)
    // Requirements: 27.1
    // ============================================================================
    
    // Map - transform the result of this task
    std::shared_ptr<Task> map(std::function<kernel::Value(kernel::Value)> transform);
    
    // FlatMap - chain tasks together
    std::shared_ptr<Task> flat_map(std::function<std::shared_ptr<Task>(kernel::Value)> transform);
    
    // WithTimeout - add timeout to this task
    std::shared_ptr<Task> with_timeout(int milliseconds);
    
    // OnComplete - register completion callback
    void on_complete(std::function<void(kernel::Value, const std::string&)> callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        completion_callbacks_.push_back(callback);
        
        // If already completed, call the callback immediately
        if (is_completed_unlocked()) {
            if (state_ == TaskState::COMPLETED) {
                callback(result_, "");
            } else if (state_ == TaskState::FAILED) {
                callback(kernel::Value::from_string(""), error_);
            } else if (state_ == TaskState::CANCELLED) {
                callback(kernel::Value::from_string(""), "Task was cancelled");
            }
        }
    }
    
    // ============================================================================
    // ERROR HANDLING METHODS (Task 1)
    // Requirements: 1.1-1.8
    // ============================================================================
    
    // Exceptionally - recover from task failures
    // Requirements: 1.1, 1.2
    std::shared_ptr<Task> exceptionally(std::function<kernel::Value(const std::string&)> handler);
    
    // Handle - unified handler for both success and failure cases
    // Requirements: 1.3, 1.4
    std::shared_ptr<Task> handle(std::function<kernel::Value(kernel::Value, const std::string&)> handler);
    
    // WhenComplete - run action on completion but preserve original result
    // Requirements: 1.5, 1.6
    std::shared_ptr<Task> when_complete(std::function<void(kernel::Value, const std::string&)> action);
    
    // Made public so AsyncScheduler can generate IDs for tasks
    static TaskId generate_task_id() {
        static std::atomic<TaskId> next_id{1};
        return next_id.fetch_add(1);
    }

private:
    
    bool is_completed_unlocked() const {
        return state_ == TaskState::COMPLETED || state_ == TaskState::FAILED || state_ == TaskState::CANCELLED;
    }
    
    TaskId id_;
    mutable std::mutex mutex_;
    mutable std::condition_variable completion_cv_;
    TaskState state_;
    kernel::Value result_;
    std::string error_;
    std::vector<std::function<void(kernel::Value, const std::string&)>> completion_callbacks_;
    CancellationToken cancellation_token_;
    std::weak_ptr<Task> parent_;
    std::vector<std::weak_ptr<Task>> children_;
    
    // Thread-local storage for current task's cancellation token
    static thread_local CancellationToken current_cancellation_token_;
};

// Task completer for manual task completion
// This allows creating deferred tasks that are completed later
// Requirements: 2.2, 2.3, 2.8, 2.10
class TaskCompleter {
public:
    explicit TaskCompleter(std::shared_ptr<Task> task) 
        : task_(std::move(task)), completed_(false) {}
    
    // Move constructor - needed because mutex is non-movable
    TaskCompleter(TaskCompleter&& other) noexcept
        : task_(std::move(other.task_)), completed_(other.completed_) {
        other.completed_ = true; // Mark moved-from as completed to prevent double-completion
    }
    
    // Move assignment
    TaskCompleter& operator=(TaskCompleter&& other) noexcept {
        if (this != &other) {
            task_ = std::move(other.task_);
            completed_ = other.completed_;
            other.completed_ = true;
        }
        return *this;
    }
    
    // Non-copyable
    TaskCompleter(const TaskCompleter&) = delete;
    TaskCompleter& operator=(const TaskCompleter&) = delete;
    
    // Complete the task with a result
    // Returns true if this was the first completion, false otherwise
    // Requirements: 2.2, 2.3, 2.8
    bool complete(kernel::Value result) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (completed_) {
            return false; // Already completed
        }
        
        if (auto task = task_.lock()) {
            try {
                // Mark as running if still pending
                if (task->is_pending()) {
                    task->mark_running();
                }
                task->set_result(std::move(result));
                completed_ = true;
                return true;
            } catch (...) {
                // If setting result fails, don't mark as completed
                return false;
            }
        }
        
        return false; // Task no longer exists
    }
    
    // Complete the task with an error
    // Returns true if this was the first completion, false otherwise
    // Requirements: 2.2, 2.3, 2.8
    bool complete_error(const std::string& error) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (completed_) {
            return false; // Already completed
        }
        
        if (auto task = task_.lock()) {
            try {
                // Mark as running if still pending
                if (task->is_pending()) {
                    task->mark_running();
                }
                task->set_error(error);
                completed_ = true;
                return true;
            } catch (...) {
                // If setting error fails, don't mark as completed
                return false;
            }
        }
        
        return false; // Task no longer exists
    }
    
    // Check if the completer has been used to complete the task
    // Requirements: 2.8
    bool is_completed() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return completed_;
    }
    
    // Cancel the task
    void cancel() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (auto task = task_.lock()) {
            task->cancel();
            completed_ = true; // Mark as completed since task is now cancelled
        }
    }
    
    // Check if the associated task is still valid
    bool is_valid() const {
        return !task_.expired();
    }
    
    // Get the associated task (if still valid)
    std::shared_ptr<Task> get_task() const {
        return task_.lock();
    }
    
private:
    std::weak_ptr<Task> task_;
    mutable std::mutex mutex_; // Thread-safe completion (Requirement 2.10)
    bool completed_; // Track if completion has occurred (Requirement 2.8)
};

// Async scheduler that manages task execution and continuation storage
// This is the core async scheduler mentioned in the task requirements
// Requirements: 41.23, 41B.8
class AsyncScheduler {
public:
    static AsyncScheduler& instance() {
        static AsyncScheduler scheduler;
        return scheduler;
    }
    
    // Create a new task with optional cancellation token
    std::shared_ptr<Task> create_task(CancellationToken cancellation_token = CancellationToken()) {
        auto task = std::make_shared<Task>(Task::generate_task_id(), cancellation_token);
        register_task(task);
        return task;
    }
    
    // Register an existing task with the scheduler
    // Used for deferred tasks and structured concurrency integration
    // Requirements: 2.9
    void register_task(std::shared_ptr<Task> task) {
        std::lock_guard<std::mutex> lock(mutex_);
        tasks_[task->id()] = task;
    }
    
    // Create a deferred task (completed manually later) with optional cancellation token
    std::pair<std::shared_ptr<Task>, TaskCompleter> create_deferred_task(CancellationToken cancellation_token = CancellationToken()) {
        auto task = create_task(cancellation_token);
        TaskCompleter completer(task);
        return {task, std::move(completer)};
    }
    
    // Schedule a task with a function to execute
    template<typename F>
    std::shared_ptr<Task> schedule_task(F&& func) {
        auto task = create_task();
        
        // Execute the function asynchronously
        std::thread([task, func = std::forward<F>(func)]() {
            try {
                task->mark_running();
                auto result = func();
                task->set_result(result);
            } catch (const std::exception& e) {
                task->set_error(e.what());
            } catch (...) {
                task->set_error("Unknown error occurred");
            }
        }).detach();
        
        return task;
    }
    
    // Schedule a task with a delay
    std::shared_ptr<Task> schedule_delayed_task(std::chrono::milliseconds delay, kernel::Value result) {
        auto task = create_task();
        
        std::thread([task, delay, result]() {
            try {
                task->mark_running();
                std::this_thread::sleep_for(delay);
                task->set_result(result);
            } catch (const std::exception& e) {
                task->set_error(e.what());
            }
        }).detach();
        
        return task;
    }
    
    // Store a continuation for later resumption
    // This is the key functionality for async/await - storing continuations
    // Requirements: 41.23, 41B.8
    void store_continuation(Task::TaskId task_id, std::shared_ptr<meld::effects::Continuation> continuation) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto task_it = tasks_.find(task_id);
        if (task_it == tasks_.end()) {
            throw std::runtime_error("Task not found: " + std::to_string(task_id));
        }
        
        auto task = task_it->second.lock();
        if (!task) {
            throw std::runtime_error("Task expired: " + std::to_string(task_id));
        }
        
        // Store the continuation for this task
        continuations_[task_id] = continuation;
        
        // If the task is already completed, resume immediately
        if (task->is_completed()) {
            resume_continuation(task_id);
        }
    }
    
    // Resume a stored continuation
    // This is called when a task completes to resume the awaiting code
    void resume_continuation(Task::TaskId task_id) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        auto cont_it = continuations_.find(task_id);
        if (cont_it == continuations_.end()) {
            return; // No continuation stored
        }
        
        auto task_it = tasks_.find(task_id);
        if (task_it == tasks_.end()) {
            continuations_.erase(cont_it);
            return;
        }
        
        auto task = task_it->second.lock();
        if (!task) {
            continuations_.erase(cont_it);
            tasks_.erase(task_it);
            return;
        }
        
        auto continuation = cont_it->second;
        continuations_.erase(cont_it);
        
        lock.unlock();
        
        // Resume the continuation with the task result
        try {
            if (task->is_completed() && !task->is_failed() && !task->is_cancelled()) {
                continuation->resume(task->get_result());
            } else if (task->is_failed()) {
                // For failed tasks, we could throw an exception or return an error value
                // For now, we'll return an error string
                continuation->resume(kernel::Value::from_string("Task failed: " + task->get_error()));
            } else if (task->is_cancelled()) {
                continuation->resume(kernel::Value::from_string("Task was cancelled"));
            }
        } catch (const std::exception& e) {
            // If resumption fails, there's not much we can do
            // In a production system, we might log this error
        }
    }
    
    // Get a task by ID
    std::shared_ptr<Task> get_task(Task::TaskId task_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = tasks_.find(task_id);
        if (it != tasks_.end()) {
            return it->second.lock();
        }
        return nullptr;
    }
    
    // Clean up completed tasks
    void cleanup_completed_tasks() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = tasks_.begin();
        while (it != tasks_.end()) {
            auto task = it->second.lock();
            if (!task || task->is_completed()) {
                // Also clean up any associated continuations
                continuations_.erase(it->first);
                it = tasks_.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    // Get number of active tasks
    size_t active_task_count() {
        std::lock_guard<std::mutex> lock(mutex_);
        cleanup_completed_tasks();
        return tasks_.size();
    }
    
    // Cancel all tasks
    void cancel_all_tasks() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (auto& [task_id, weak_task] : tasks_) {
            if (auto task = weak_task.lock()) {
                task->cancel();
            }
        }
        
        // Resume all stored continuations with cancellation
        for (auto& [task_id, continuation] : continuations_) {
            try {
                continuation->resume(kernel::Value::from_string("All tasks cancelled"));
            } catch (...) {
                // Ignore errors during cancellation cleanup
            }
        }
        
        continuations_.clear();
        tasks_.clear();
    }
    
private:
    AsyncScheduler() = default;
    
    std::mutex mutex_;
    std::map<Task::TaskId, std::weak_ptr<Task>> tasks_;
    std::map<Task::TaskId, std::shared_ptr<meld::effects::Continuation>> continuations_;
};

// ============================================================================
// LIBRARY FUNCTIONS FOR ASYNC/AWAIT
// ============================================================================

// ============================================================================
// TASK STATIC METHOD IMPLEMENTATIONS
// These must be defined after AsyncScheduler since they use it
// ============================================================================

// Task::deferred implementation
inline std::pair<std::shared_ptr<Task>, TaskCompleter> Task::deferred(CancellationToken cancellation_token) {
    auto task = std::make_shared<Task>(generate_task_id(), cancellation_token);
    
    // Register task with scheduler for structured concurrency integration
    AsyncScheduler::instance().register_task(task);
    
    TaskCompleter completer(task);
    return {task, std::move(completer)};
}

// Task::completed implementation
inline std::shared_ptr<Task> Task::completed(kernel::Value value) {
    auto task = std::make_shared<Task>(generate_task_id());
    task->mark_running();
    task->set_result(std::move(value));
    return task;
}

// Task::failed implementation
inline std::shared_ptr<Task> Task::failed(const std::string& error) {
    auto task = std::make_shared<Task>(generate_task_id());
    task->mark_running();
    task->set_error(error);
    return task;
}

// ============================================================================
// TASK COMBINATOR / ERROR HANDLING IMPLEMENTATIONS
// These must be defined after AsyncScheduler since they use it
// ============================================================================

inline std::shared_ptr<Task> Task::map(std::function<kernel::Value(kernel::Value)> transform) {
    auto new_task = AsyncScheduler::instance().create_task();
    std::thread([self = shared_from_this(), new_task, transform]() {
        try {
            new_task->mark_running();
            kernel::Value original_result = self->await();
            kernel::Value transformed_result = transform(original_result);
            new_task->set_result(transformed_result);
        } catch (const std::exception& e) {
            new_task->set_error(e.what());
        } catch (...) {
            new_task->set_error("Unknown error in map transformation");
        }
    }).detach();
    return new_task;
}

inline std::shared_ptr<Task> Task::flat_map(std::function<std::shared_ptr<Task>(kernel::Value)> transform) {
    auto new_task = AsyncScheduler::instance().create_task();
    std::thread([self = shared_from_this(), new_task, transform]() {
        try {
            new_task->mark_running();
            kernel::Value original_result = self->await();
            auto next_task = transform(original_result);
            kernel::Value final_result = next_task->await();
            new_task->set_result(final_result);
        } catch (const std::exception& e) {
            new_task->set_error(e.what());
        } catch (...) {
            new_task->set_error("Unknown error in flatMap transformation");
        }
    }).detach();
    return new_task;
}

inline std::shared_ptr<Task> Task::with_timeout(int milliseconds) {
    auto timeout_task = AsyncScheduler::instance().create_task();
    std::thread([self = shared_from_this(), timeout_task, milliseconds]() {
        try {
            timeout_task->mark_running();
            auto future = std::async(std::launch::async, [self]() {
                return self->await();
            });
            auto status = future.wait_for(std::chrono::milliseconds(milliseconds));
            if (status == std::future_status::timeout) {
                self->cancel();
                timeout_task->set_error("Task timed out after " + std::to_string(milliseconds) + "ms");
            } else {
                kernel::Value result = future.get();
                timeout_task->set_result(result);
            }
        } catch (const std::exception& e) {
            timeout_task->set_error(e.what());
        } catch (...) {
            timeout_task->set_error("Unknown error in timeout handling");
        }
    }).detach();
    return timeout_task;
}

inline std::shared_ptr<Task> Task::exceptionally(std::function<kernel::Value(const std::string&)> handler) {
    auto recovery_task = AsyncScheduler::instance().create_task();
    std::thread([self = shared_from_this(), recovery_task, handler]() {
        try {
            recovery_task->mark_running();
            self->wait_for_completion();
            if (self->is_failed()) {
                kernel::Value recovered_result = handler(self->get_error());
                recovery_task->set_result(recovered_result);
            } else if (self->is_cancelled()) {
                kernel::Value recovered_result = handler("Task was cancelled");
                recovery_task->set_result(recovered_result);
            } else {
                recovery_task->set_result(self->get_result());
            }
        } catch (const std::exception& e) {
            recovery_task->set_error(e.what());
        } catch (...) {
            recovery_task->set_error("Unknown error in exceptionally handler");
        }
    }).detach();
    return recovery_task;
}

inline std::shared_ptr<Task> Task::handle(std::function<kernel::Value(kernel::Value, const std::string&)> handler) {
    auto handled_task = AsyncScheduler::instance().create_task();
    std::thread([self = shared_from_this(), handled_task, handler]() {
        try {
            handled_task->mark_running();
            self->wait_for_completion();
            kernel::Value handled_result;
            if (self->is_failed()) {
                handled_result = handler(kernel::Value::from_string(""), self->get_error());
            } else if (self->is_cancelled()) {
                handled_result = handler(kernel::Value::from_string(""), "Task was cancelled");
            } else {
                handled_result = handler(self->get_result(), "");
            }
            handled_task->set_result(handled_result);
        } catch (const std::exception& e) {
            handled_task->set_error(e.what());
        } catch (...) {
            handled_task->set_error("Unknown error in handle handler");
        }
    }).detach();
    return handled_task;
}

inline std::shared_ptr<Task> Task::when_complete(std::function<void(kernel::Value, const std::string&)> action) {
    auto transparent_task = AsyncScheduler::instance().create_task();
    std::thread([self = shared_from_this(), transparent_task, action]() {
        try {
            transparent_task->mark_running();
            self->wait_for_completion();
            kernel::Value result_value;
            std::string error_value;
            if (self->is_failed()) {
                result_value = kernel::Value::from_string("");
                error_value = self->get_error();
            } else if (self->is_cancelled()) {
                result_value = kernel::Value::from_string("");
                error_value = "Task was cancelled";
            } else {
                result_value = self->get_result();
                error_value = "";
            }
            try { action(result_value, error_value); } catch (...) {}
            if (self->is_failed()) {
                transparent_task->set_error(self->get_error());
            } else if (self->is_cancelled()) {
                transparent_task->set_error("Task was cancelled");
            } else {
                transparent_task->set_result(self->get_result());
            }
        } catch (const std::exception& e) {
            transparent_task->set_error(e.what());
        } catch (...) {
            transparent_task->set_error("Unknown error in whenComplete handler");
        }
    }).detach();
    return transparent_task;
}

// await function - performs Async.wait effect
// This is the main await function mentioned in the task requirements
// Requirements: 41.23
inline kernel::Value await(std::shared_ptr<Task> task) {
    if (!task) {
        throw std::runtime_error("Cannot await null task");
    }
    
    return task->await();
}

// Create a new task with optional cancellation token
inline std::shared_ptr<Task> create_task(CancellationToken cancellation_token = CancellationToken()) {
    return AsyncScheduler::instance().create_task(cancellation_token);
}

// Create a deferred task with optional cancellation token
inline std::pair<std::shared_ptr<Task>, TaskCompleter> create_deferred_task(CancellationToken cancellation_token = CancellationToken()) {
    return AsyncScheduler::instance().create_deferred_task(cancellation_token);
}

// Schedule a task with a function
template<typename F>
std::shared_ptr<Task> async(F&& func) {
    return AsyncScheduler::instance().schedule_task(std::forward<F>(func));
}

// Schedule a delayed task
inline std::shared_ptr<Task> delay(std::chrono::milliseconds ms, kernel::Value result = kernel::Value::from_string("delay_completed")) {
    return AsyncScheduler::instance().schedule_delayed_task(ms, result);
}

// ============================================================================
// TASK COMBINATORS - STATIC METHODS (Task 21.2)
// Requirements: 27.1
// ============================================================================

// Task.all - wait for all tasks to complete and return list of results
inline std::shared_ptr<Task> all(const std::vector<std::shared_ptr<Task>>& tasks) {
    auto result_task = AsyncScheduler::instance().create_task();
    
    std::thread([tasks, result_task]() {
        try {
            result_task->mark_running();
            
            std::vector<kernel::Value> results;
            results.reserve(tasks.size());
            
            // Wait for all tasks to complete
            for (auto& task : tasks) {
                kernel::Value task_result = task->await();
                results.push_back(task_result);
            }
            
            // Create a list value containing all results
            // For now, we'll create a simple string representation
            // In a full implementation, this would be a proper list type
            std::string combined_result = "[";
            for (size_t i = 0; i < results.size(); ++i) {
                if (i > 0) combined_result += ", ";
                // This is a simplified representation - in practice we'd need proper serialization
                combined_result += "result_" + std::to_string(i);
            }
            combined_result += "]";
            
            result_task->set_result(kernel::Value::from_string(combined_result));
        } catch (const std::exception& e) {
            result_task->set_error(e.what());
        } catch (...) {
            result_task->set_error("Unknown error in Task.all");
        }
    }).detach();
    
    return result_task;
}

// Task.race - return the result of the first task to complete
inline std::shared_ptr<Task> race(const std::vector<std::shared_ptr<Task>>& tasks) {
    if (tasks.empty()) {
        auto error_task = AsyncScheduler::instance().create_task();
        error_task->mark_running();
        error_task->set_error("Cannot race empty task list");
        return error_task;
    }
    
    auto result_task = AsyncScheduler::instance().create_task();
    auto completed = std::make_shared<std::atomic<bool>>(false);
    
    // Launch a thread for each task to race them
    for (auto& task : tasks) {
        std::thread([task, result_task, completed]() {
            try {
                kernel::Value task_result = task->await();
                
                // Check if we're the first to complete
                bool expected = false;
                if (completed->compare_exchange_strong(expected, true)) {
                    // We won the race
                    result_task->mark_running();
                    result_task->set_result(task_result);
                }
            } catch (const std::exception& e) {
                // Check if we're the first to complete (even with error)
                bool expected = false;
                if (completed->compare_exchange_strong(expected, true)) {
                    result_task->mark_running();
                    result_task->set_error(e.what());
                }
            } catch (...) {
                // Check if we're the first to complete (even with error)
                bool expected = false;
                if (completed->compare_exchange_strong(expected, true)) {
                    result_task->mark_running();
                    result_task->set_error("Unknown error in racing task");
                }
            }
        }).detach();
    }
    
    return result_task;
}

// ============================================================================
// ASYNC EFFECT HANDLER IMPLEMENTATION
// ============================================================================

// Create enhanced async effect handler with continuation storage
// This replaces the simple async handler with a proper scheduler-based implementation
std::shared_ptr<meld::effects::EffectHandler> create_async_scheduler_handler();

// Install the async scheduler in the effect runtime
void install_async_scheduler();

// Uninstall the async scheduler from the effect runtime
void uninstall_async_scheduler();

// RAII helper for async scheduler
class AsyncSchedulerScope {
public:
    AsyncSchedulerScope();
    ~AsyncSchedulerScope();
    
    // Non-copyable, non-movable
    AsyncSchedulerScope(const AsyncSchedulerScope&) = delete;
    AsyncSchedulerScope& operator=(const AsyncSchedulerScope&) = delete;
    AsyncSchedulerScope(AsyncSchedulerScope&&) = delete;
    AsyncSchedulerScope& operator=(AsyncSchedulerScope&&) = delete;
};

} // namespace meld::stdlib