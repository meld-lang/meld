#include "meld/effects/effect.hpp"
#include "meld/effects/builtin_effects.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <thread>
#include <random>
#include <sstream>
#include <stdexcept>
#include <algorithm>

namespace meld::effects {

// ============================================================================
// BUILT-IN EFFECT IMPLEMENTATIONS
// Task 35.12: Implement built-in effects
// Requirements: 41.19
// ============================================================================

// FileSystem Effect Implementation
std::shared_ptr<EffectHandler> create_filesystem_handler() {
    auto effect = create_filesystem_effect();
    auto handler = std::make_shared<EffectHandler>(effect);
    
    // read(path: string) -> string
    handler->set_enhanced_handler("read", [](const std::vector<kernel::Value>& args, EffectContinuation& cont) -> kernel::Value {
        if (args.size() != 1) {
            throw std::runtime_error("FileSystem.read requires 1 argument (path)");
        }
        
        try {
            std::string path = args[0].as_string();
            
            // Check if file exists
            if (!std::filesystem::exists(path)) {
                throw std::runtime_error("File does not exist: " + path);
            }
            
            // Read file contents
            std::ifstream file(path);
            if (!file.is_open()) {
                throw std::runtime_error("Cannot open file: " + path);
            }
            
            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string content = buffer.str();
            
            // Resume with the file content
            return cont.resume(kernel::Value::from_string(content));
        } catch (const std::exception& e) {
            throw std::runtime_error("FileSystem.read failed: " + std::string(e.what()));
        }
    });
    
    // write(path: string, data: string) -> void
    handler->set_enhanced_handler("write", [](const std::vector<kernel::Value>& args, EffectContinuation& cont) -> kernel::Value {
        if (args.size() != 2) {
            throw std::runtime_error("FileSystem.write requires 2 arguments (path, data)");
        }
        
        try {
            std::string path = args[0].as_string();
            std::string data = args[1].as_string();
            
            // Create parent directories if they don't exist
            std::filesystem::path file_path(path);
            if (file_path.has_parent_path()) {
                std::filesystem::create_directories(file_path.parent_path());
            }
            
            // Write file contents
            std::ofstream file(path);
            if (!file.is_open()) {
                throw std::runtime_error("Cannot create/open file: " + path);
            }
            
            file << data;
            file.close();
            
            // Resume with unit value
            return cont.resume(kernel::Value::from_unit());
        } catch (const std::exception& e) {
            throw std::runtime_error("FileSystem.write failed: " + std::string(e.what()));
        }
    });
    
    // delete(path: string) -> void
    handler->set_enhanced_handler("delete", [](const std::vector<kernel::Value>& args, EffectContinuation& cont) -> kernel::Value {
        if (args.size() != 1) {
            throw std::runtime_error("FileSystem.delete requires 1 argument (path)");
        }
        
        try {
            std::string path = args[0].as_string();
            
            // Check if file exists
            if (!std::filesystem::exists(path)) {
                throw std::runtime_error("File does not exist: " + path);
            }
            
            // Delete the file
            if (!std::filesystem::remove(path)) {
                throw std::runtime_error("Failed to delete file: " + path);
            }
            
            // Resume with unit value
            return cont.resume(kernel::Value::from_unit());
        } catch (const std::exception& e) {
            throw std::runtime_error("FileSystem.delete failed: " + std::string(e.what()));
        }
    });
    
    // exists(path: string) -> bool
    handler->set_enhanced_handler("exists", [](const std::vector<kernel::Value>& args, EffectContinuation& cont) -> kernel::Value {
        if (args.size() != 1) {
            throw std::runtime_error("FileSystem.exists requires 1 argument (path)");
        }
        
        try {
            std::string path = args[0].as_string();
            bool exists = std::filesystem::exists(path);
            
            // Resume with boolean result
            return cont.resume(kernel::Value::from_bool(exists));
        } catch (const std::exception& e) {
            throw std::runtime_error("FileSystem.exists failed: " + std::string(e.what()));
        }
    });
    
    return handler;
}

// Network Effect Implementation
std::shared_ptr<EffectHandler> create_network_handler() {
    auto effect = create_network_effect();
    auto handler = std::make_shared<EffectHandler>(effect);
    
    // get(url: string) -> string
    handler->set_enhanced_handler("get", [](const std::vector<kernel::Value>& args, EffectContinuation& cont) -> kernel::Value {
        if (args.size() != 1) {
            throw std::runtime_error("Network.get requires 1 argument (url)");
        }
        
        try {
            std::string url = args[0].as_string();
            
            // For this implementation, we'll simulate a network request
            // In a real implementation, this would use libcurl or similar
            std::string response = "HTTP response from " + url;
            
            // Simulate network delay
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            
            // Resume with the response
            return cont.resume(kernel::Value::from_string(response));
        } catch (const std::exception& e) {
            throw std::runtime_error("Network.get failed: " + std::string(e.what()));
        }
    });
    
    // post(url: string, body: string) -> string
    handler->set_enhanced_handler("post", [](const std::vector<kernel::Value>& args, EffectContinuation& cont) -> kernel::Value {
        if (args.size() != 2) {
            throw std::runtime_error("Network.post requires 2 arguments (url, body)");
        }
        
        try {
            std::string url = args[0].as_string();
            std::string body = args[1].as_string();
            
            // For this implementation, we'll simulate a network request
            // In a real implementation, this would use libcurl or similar
            std::string response = "POST response from " + url + " with body: " + body;
            
            // Simulate network delay
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
            
            // Resume with the response
            return cont.resume(kernel::Value::from_string(response));
        } catch (const std::exception& e) {
            throw std::runtime_error("Network.post failed: " + std::string(e.what()));
        }
    });
    
    return handler;
}

// Console Effect Implementation
std::shared_ptr<EffectHandler> create_console_handler() {
    auto effect = create_console_effect();
    auto handler = std::make_shared<EffectHandler>(effect);
    
    // print(message: string) -> void
    handler->set_enhanced_handler("print", [](const std::vector<kernel::Value>& args, EffectContinuation& cont) -> kernel::Value {
        if (args.size() != 1) {
            throw std::runtime_error("Console.print requires 1 argument (message)");
        }
        
        try {
            std::string message = args[0].as_string();
            
            // Print to stdout without newline
            std::cout << message << std::flush;
            
            // Resume with unit value
            return cont.resume(kernel::Value::from_unit());
        } catch (const std::exception& e) {
            throw std::runtime_error("Console.print failed: " + std::string(e.what()));
        }
    });
    
    // println(message: string) -> void (convenience method)
    handler->set_enhanced_handler("println", [](const std::vector<kernel::Value>& args, EffectContinuation& cont) -> kernel::Value {
        if (args.size() != 1) {
            throw std::runtime_error("Console.println requires 1 argument (message)");
        }
        
        try {
            std::string message = args[0].as_string();
            
            // Print to stdout with newline
            std::cout << message << std::endl;
            
            // Resume with unit value
            return cont.resume(kernel::Value::from_unit());
        } catch (const std::exception& e) {
            throw std::runtime_error("Console.println failed: " + std::string(e.what()));
        }
    });
    
    // readLine() -> string
    handler->set_enhanced_handler("readLine", [](const std::vector<kernel::Value>& args, EffectContinuation& cont) -> kernel::Value {
        if (args.size() != 0) {
            throw std::runtime_error("Console.readLine requires 0 arguments");
        }
        
        try {
            std::string line;
            std::getline(std::cin, line);
            
            // Resume with the input line
            return cont.resume(kernel::Value::from_string(line));
        } catch (const std::exception& e) {
            throw std::runtime_error("Console.readLine failed: " + std::string(e.what()));
        }
    });
    
    return handler;
}

// Random Effect Implementation
std::shared_ptr<EffectHandler> create_random_handler() {
    auto effect = create_random_effect();
    auto handler = std::make_shared<EffectHandler>(effect);
    
    // Thread-local random number generator for thread safety
    static thread_local std::random_device rd;
    static thread_local std::mt19937 gen(rd());
    
    // nextInt(max: int) -> int
    handler->set_enhanced_handler("nextInt", [](const std::vector<kernel::Value>& args, EffectContinuation& cont) -> kernel::Value {
        if (args.size() != 1) {
            throw std::runtime_error("Random.nextInt requires 1 argument (max)");
        }
        
        try {
            int64_t max_value = args[0].as_int();
            
            if (max_value <= 0) {
                throw std::runtime_error("Random.nextInt: max must be positive");
            }
            
            std::uniform_int_distribution<int64_t> dist(0, max_value - 1);
            int64_t result = dist(gen);
            
            // Resume with the random integer
            return cont.resume(kernel::Value::from_int(result));
        } catch (const std::exception& e) {
            throw std::runtime_error("Random.nextInt failed: " + std::string(e.what()));
        }
    });
    
    // nextFloat() -> float
    handler->set_enhanced_handler("nextFloat", [](const std::vector<kernel::Value>& args, EffectContinuation& cont) -> kernel::Value {
        if (args.size() != 0) {
            throw std::runtime_error("Random.nextFloat requires 0 arguments");
        }
        
        try {
            std::uniform_real_distribution<double> dist(0.0, 1.0);
            double result = dist(gen);
            
            // Note: We're using int64_t to store the float as bits
            // In a real implementation, we'd have a proper Float type
            // For now, we'll convert to string representation
            std::string float_str = std::to_string(result);
            
            // Resume with the random float (as string for now)
            return cont.resume(kernel::Value::from_string(float_str));
        } catch (const std::exception& e) {
            throw std::runtime_error("Random.nextFloat failed: " + std::string(e.what()));
        }
    });
    
    return handler;
}

// Seedable Random Effect Implementation (AI_DX Req 141)
// Deterministic random sequences from a given seed
std::shared_ptr<EffectHandler> create_seedable_random_handler(int64_t seed) {
    auto effect = create_random_effect();
    
    // Add reseed operation to the effect definition
    effect->add_operation("reseed", {"int"}, "void");
    
    auto handler = std::make_shared<EffectHandler>(effect);
    
    // Shared state: the seeded generator (captured by lambdas)
    auto gen = std::make_shared<std::mt19937>(static_cast<std::mt19937::result_type>(seed));
    
    // nextInt(max: int) -> int — deterministic from seed
    handler->set_enhanced_handler("nextInt", [gen](const std::vector<kernel::Value>& args, EffectContinuation& cont) -> kernel::Value {
        if (args.size() != 1) {
            throw std::runtime_error("Random.nextInt requires 1 argument (max)");
        }
        
        int64_t max_value = args[0].as_int();
        if (max_value <= 0) {
            throw std::runtime_error("Random.nextInt: max must be positive");
        }
        
        std::uniform_int_distribution<int64_t> dist(0, max_value - 1);
        int64_t result = dist(*gen);
        return cont.resume(kernel::Value::from_int(result));
    });
    
    // nextFloat() -> float — deterministic from seed
    handler->set_enhanced_handler("nextFloat", [gen](const std::vector<kernel::Value>& args, EffectContinuation& cont) -> kernel::Value {
        if (!args.empty()) {
            throw std::runtime_error("Random.nextFloat requires 0 arguments");
        }
        
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        double result = dist(*gen);
        return cont.resume(kernel::Value::from_string(std::to_string(result)));
    });
    
    // reseed(new_seed: int) -> void — re-seed the generator mid-execution
    handler->set_enhanced_handler("reseed", [gen](const std::vector<kernel::Value>& args, EffectContinuation& cont) -> kernel::Value {
        if (args.size() != 1) {
            throw std::runtime_error("Random.reseed requires 1 argument (new_seed)");
        }
        
        int64_t new_seed = args[0].as_int();
        gen->seed(static_cast<std::mt19937::result_type>(new_seed));
        return cont.resume(kernel::Value::from_unit());
    });
    
    return handler;
}

// Time Effect Implementation
std::shared_ptr<EffectHandler> create_time_handler() {
    auto effect = create_time_effect();
    auto handler = std::make_shared<EffectHandler>(effect);
    
    // now() -> int (timestamp in milliseconds)
    handler->set_enhanced_handler("now", [](const std::vector<kernel::Value>& args, EffectContinuation& cont) -> kernel::Value {
        if (args.size() != 0) {
            throw std::runtime_error("Time.now requires 0 arguments");
        }
        
        try {
            auto now = std::chrono::system_clock::now();
            auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()
            ).count();
            
            // Resume with the current timestamp
            return cont.resume(kernel::Value::from_int(timestamp));
        } catch (const std::exception& e) {
            throw std::runtime_error("Time.now failed: " + std::string(e.what()));
        }
    });
    
    // sleep(ms: int) -> void
    handler->set_enhanced_handler("sleep", [](const std::vector<kernel::Value>& args, EffectContinuation& cont) -> kernel::Value {
        if (args.size() != 1) {
            throw std::runtime_error("Time.sleep requires 1 argument (milliseconds)");
        }
        
        try {
            int64_t milliseconds = args[0].as_int();
            
            if (milliseconds < 0) {
                throw std::runtime_error("Time.sleep: milliseconds must be non-negative");
            }
            
            // Sleep for the specified duration
            std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
            
            // Resume with unit value
            return cont.resume(kernel::Value::from_unit());
        } catch (const std::exception& e) {
            throw std::runtime_error("Time.sleep failed: " + std::string(e.what()));
        }
    });
    
    return handler;
}

// Virtual Time Effect Implementation (AI_DX Req 142)
// Clock does not advance automatically — only via explicit advance() calls
std::shared_ptr<EffectHandler> create_virtual_time_handler(int64_t origin_ms) {
    auto effect = create_time_effect();
    
    // Add virtual-time-specific operations
    effect->add_operation("advance", {"int"}, "void");
    effect->add_operation("set_origin", {"int"}, "void");
    
    auto handler = std::make_shared<EffectHandler>(effect);
    
    // Shared virtual clock state
    struct VirtualClock {
        int64_t current_ms;
        // Pending timers: (fire_time_ms, callback_description)
        std::vector<std::pair<int64_t, std::string>> pending_timers;
    };
    auto clock = std::make_shared<VirtualClock>();
    clock->current_ms = origin_ms;
    
    // now() -> int — returns virtual clock value, not system clock
    handler->set_enhanced_handler("now", [clock](const std::vector<kernel::Value>& args, EffectContinuation& cont) -> kernel::Value {
        if (!args.empty()) {
            throw std::runtime_error("Time.now requires 0 arguments");
        }
        return cont.resume(kernel::Value::from_int(clock->current_ms));
    });
    
    // sleep(ms: int) -> void — records a pending timer instead of blocking
    handler->set_enhanced_handler("sleep", [clock](const std::vector<kernel::Value>& args, EffectContinuation& cont) -> kernel::Value {
        if (args.size() != 1) {
            throw std::runtime_error("Time.sleep requires 1 argument (milliseconds)");
        }
        
        int64_t ms = args[0].as_int();
        if (ms < 0) {
            throw std::runtime_error("Time.sleep: milliseconds must be non-negative");
        }
        
        // In virtual time, sleep is a no-op that records the timer
        // The timer fires when advance() moves the clock past the target
        int64_t fire_at = clock->current_ms + ms;
        clock->pending_timers.emplace_back(fire_at, "sleep(" + std::to_string(ms) + ")");
        
        // Resume immediately — the caller sees time as having passed
        return cont.resume(kernel::Value::from_unit());
    });
    
    // advance(duration_ms: int) -> void — advances virtual clock and fires pending timers
    handler->set_enhanced_handler("advance", [clock](const std::vector<kernel::Value>& args, EffectContinuation& cont) -> kernel::Value {
        if (args.size() != 1) {
            throw std::runtime_error("Time.advance requires 1 argument (duration_ms)");
        }
        
        int64_t duration_ms = args[0].as_int();
        if (duration_ms < 0) {
            throw std::runtime_error("Time.advance: duration must be non-negative");
        }
        
        int64_t target = clock->current_ms + duration_ms;
        
        // Sort pending timers by fire time for deterministic ordering
        std::sort(clock->pending_timers.begin(), clock->pending_timers.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });
        
        // Fire timers in order up to target time
        auto it = clock->pending_timers.begin();
        while (it != clock->pending_timers.end() && it->first <= target) {
            clock->current_ms = it->first;  // Step clock to timer fire time
            it = clock->pending_timers.erase(it);
        }
        
        // Advance clock to final target
        clock->current_ms = target;
        
        return cont.resume(kernel::Value::from_unit());
    });
    
    // set_origin(timestamp_ms: int) -> void — sets initial virtual time
    handler->set_enhanced_handler("set_origin", [clock](const std::vector<kernel::Value>& args, EffectContinuation& cont) -> kernel::Value {
        if (args.size() != 1) {
            throw std::runtime_error("Time.set_origin requires 1 argument (timestamp_ms)");
        }
        
        clock->current_ms = args[0].as_int();
        return cont.resume(kernel::Value::from_unit());
    });
    
    return handler;
}

// Exception Effect Implementation
std::shared_ptr<EffectDefinition> create_exception_effect() {
    auto effect = std::make_shared<EffectDefinition>("Exception");
    effect->add_operation("raise", {"string"}, "void");
    return effect;
}

std::shared_ptr<EffectHandler> create_exception_handler() {
    auto effect = create_exception_effect();
    auto handler = std::make_shared<EffectHandler>(effect);
    
    // raise(message: string) -> void (never returns normally)
    handler->set_enhanced_handler("raise", [](const std::vector<kernel::Value>& args, EffectContinuation& cont) -> kernel::Value {
        if (args.size() != 1) {
            throw std::runtime_error("Exception.raise requires 1 argument (message)");
        }
        
        try {
            std::string message = args[0].as_string();
            
            // For exceptions, we don't resume the continuation
            // Instead, we throw a C++ exception that will be caught by the handler
            throw std::runtime_error("Exception raised: " + message);
        } catch (const std::exception& e) {
            // Re-throw to propagate the exception
            throw;
        }
    });
    
    return handler;
}

// Async Effect Implementation
// Task 21.5: Integrate with algebraic effects
// Requirements: 27.7 - Use algebraic effects for async I/O operations
std::shared_ptr<EffectDefinition> create_async_effect() {
    auto effect = std::make_shared<EffectDefinition>("Async");
    
    // Core async operations for Task integration
    effect->add_operation("wait", {"int"}, "string");           // Wait for task completion
    effect->add_operation("delay", {"int"}, "void");            // Async delay operation
    effect->add_operation("yield", {}, "void");                 // Yield control to scheduler
    effect->add_operation("spawn", {"function"}, "int");        // Spawn new task
    effect->add_operation("cancel", {"int"}, "void");           // Cancel task by ID
    
    return effect;
}

std::shared_ptr<EffectHandler> create_async_handler() {
    auto effect = create_async_effect();
    auto handler = std::make_shared<EffectHandler>(effect);
    
    // wait(task_id: int) -> string
    // This is a basic async handler - the enhanced scheduler handler in async.cpp provides full integration
    handler->set_enhanced_handler("wait", [](const std::vector<kernel::Value>& args, EffectContinuation& cont) -> kernel::Value {
        if (args.size() != 1) {
            throw std::runtime_error("Async.wait requires 1 argument (task_id or milliseconds)");
        }
        
        try {
            int64_t value = args[0].as_int();
            
            if (value < 0) {
                throw std::runtime_error("Async.wait: value must be non-negative");
            }
            
            // For basic handler, treat as milliseconds delay
            std::this_thread::sleep_for(std::chrono::milliseconds(value));
            
            std::string result = "Async operation completed after " + std::to_string(value) + "ms";
            return cont.resume(kernel::Value::from_string(result));
        } catch (const std::exception& e) {
            throw std::runtime_error("Async.wait failed: " + std::string(e.what()));
        }
    });
    
    // delay(ms: int) -> void
    // Async delay operation using effects
    handler->set_enhanced_handler("delay", [](const std::vector<kernel::Value>& args, EffectContinuation& cont) -> kernel::Value {
        if (args.size() != 1) {
            throw std::runtime_error("Async.delay requires 1 argument (milliseconds)");
        }
        
        try {
            int64_t milliseconds = args[0].as_int();
            
            if (milliseconds < 0) {
                throw std::runtime_error("Async.delay: milliseconds must be non-negative");
            }
            
            // Perform async delay
            std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
            
            // Resume with void result
            return cont.resume(kernel::Value(kernel::Empty::instance()));
        } catch (const std::exception& e) {
            throw std::runtime_error("Async.delay failed: " + std::string(e.what()));
        }
    });
    
    // yield() -> void
    // Yield control to other tasks
    handler->set_enhanced_handler("yield", [](const std::vector<kernel::Value>& args, EffectContinuation& cont) -> kernel::Value {
        if (!args.empty()) {
            throw std::runtime_error("Async.yield takes no arguments");
        }
        
        try {
            // Yield control to scheduler
            std::this_thread::yield();
            
            // Resume immediately with void result
            return cont.resume(kernel::Value(kernel::Empty::instance()));
        } catch (const std::exception& e) {
            throw std::runtime_error("Async.yield failed: " + std::string(e.what()));
        }
    });
    
    // spawn(function) -> int (task_id)
    // Spawn a new task (basic implementation)
    handler->set_enhanced_handler("spawn", [](const std::vector<kernel::Value>& args, EffectContinuation& cont) -> kernel::Value {
        if (args.size() != 1) {
            throw std::runtime_error("Async.spawn requires 1 argument (function)");
        }
        
        try {
            // For basic handler, just return a mock task ID
            // The enhanced scheduler handler provides full task spawning
            static std::atomic<int64_t> next_task_id{1000};
            int64_t task_id = next_task_id.fetch_add(1);
            
            return cont.resume(kernel::Value::from_int(task_id));
        } catch (const std::exception& e) {
            throw std::runtime_error("Async.spawn failed: " + std::string(e.what()));
        }
    });
    
    // cancel(task_id: int) -> void
    // Cancel a task by ID (basic implementation)
    handler->set_enhanced_handler("cancel", [](const std::vector<kernel::Value>& args, EffectContinuation& cont) -> kernel::Value {
        if (args.size() != 1) {
            throw std::runtime_error("Async.cancel requires 1 argument (task_id)");
        }
        
        try {
            int64_t task_id = args[0].as_int();
            
            // For basic handler, just acknowledge the cancellation
            // The enhanced scheduler handler provides full task cancellation
            
            return cont.resume(kernel::Value(kernel::Empty::instance()));
        } catch (const std::exception& e) {
            throw std::runtime_error("Async.cancel failed: " + std::string(e.what()));
        }
    });
    
    return handler;
}

// Generator Effect Implementation
std::shared_ptr<EffectDefinition> create_generator_effect() {
    auto effect = std::make_shared<EffectDefinition>("Generator");
    effect->add_operation("yield", {"string"}, "void");
    return effect;
}

std::shared_ptr<EffectHandler> create_generator_handler() {
    auto effect = create_generator_effect();
    auto handler = std::make_shared<EffectHandler>(effect);
    
    // yield(value: string) -> void
    // This is the core generator implementation that enables multiple resumptions
    // Requirements: 41.24 - Implement generators as effects where yield performs Generator.yield
    //               41B.3 - Implement generators as effects where handlers resume multiple times
    //               41B.6 - Provide generator functions using the Generator effect
    //               41B.9 - When a generator yields, handler resumes continuation to produce next value
    handler->set_enhanced_handler("yield", [](const std::vector<kernel::Value>& args, EffectContinuation& cont) -> kernel::Value {
        if (args.size() != 1) {
            throw std::runtime_error("Generator.yield requires 1 argument (value)");
        }
        
        try {
            std::string value = args[0].as_string();
            
            // For generators, the key insight is that we need to:
            // 1. Store the continuation for later resumption (not resume immediately)
            // 2. Return the yielded value to the generator caller
            // 3. Allow the continuation to be resumed multiple times
            
            // In a full implementation, we would store the continuation in a generator object
            // and allow it to be resumed multiple times by calling next()
            
            // For this basic implementation, we'll demonstrate the concept by:
            // 1. Logging the yielded value
            // 2. Storing continuation metadata
            // 3. Resuming with a marker that indicates suspension
            
            static thread_local std::vector<std::string> yielded_values;
            static thread_local std::shared_ptr<kernel::Continuation> stored_continuation;
            
            // Store the yielded value
            yielded_values.push_back(value);
            
            // Store the continuation for potential multiple resumptions
            stored_continuation = cont.get_kernel_continuation();
            
            // Log the yield operation
            std::cout << "[Generator] Yielded value #" << yielded_values.size() 
                      << ": '" << value << "'" << std::endl;
            
            // For generators, we typically don't resume immediately
            // Instead, we would store the continuation and resume it when next() is called
            // For this demonstration, we'll resume with a special marker
            std::string result = "Generator suspended after yielding: " + value;
            return cont.resume(kernel::Value::from_string(result));
            
        } catch (const std::exception& e) {
            throw std::runtime_error("Generator.yield failed: " + std::string(e.what()));
        }
    });
    
    return handler;
}

// ============================================================================
// CONVENIENCE FUNCTIONS FOR CREATING ALL BUILT-IN HANDLERS
// ============================================================================

// Create all built-in effect handlers
std::vector<std::shared_ptr<EffectHandler>> create_all_builtin_handlers() {
    return {
        create_filesystem_handler(),
        create_network_handler(),
        create_console_handler(),
        create_random_handler(),
        create_time_handler(),
        create_exception_handler(),
        create_async_handler(),
        create_generator_handler()
    };
}

// Install all built-in effects in the runtime
void install_builtin_effects() {
    auto& runtime = EffectRuntime::instance();
    auto handlers = create_all_builtin_handlers();
    
    // Push all handlers as a single scope
    runtime.pushScope(handlers);
}

// Uninstall all built-in effects from the runtime
void uninstall_builtin_effects() {
    auto& runtime = EffectRuntime::instance();
    runtime.popScope();
}

// RAII helper for built-in effects — implementation of header-declared class
BuiltinEffectsScope::BuiltinEffectsScope() {
    install_builtin_effects();
}

BuiltinEffectsScope::~BuiltinEffectsScope() {
    uninstall_builtin_effects();
}

} // namespace meld::effects