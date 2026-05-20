#include "meld/stdlib/effects.hpp"
#include "meld/effects/effect.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <random>
#include <thread>

namespace meld::stdlib {

// ============================================================================
// BUILT-IN EFFECT HANDLERS
// ============================================================================

// Generic effect handler creation function
// This is used by the handle macro to create handlers for any effect type
std::shared_ptr<effects::EffectHandler> create_effect_handler(const std::string& effect_name) {
    // For now, create a basic handler that can be configured with operations
    // In a full implementation, this would look up the effect definition
    // and create an appropriate handler instance
    
    // Create a basic effect definition for the named effect
    auto effect_def = std::make_shared<effects::EffectDefinition>(effect_name);
    
    // Create and return the handler
    return std::make_shared<effects::EffectHandler>(effect_def);
}

// Create a real FileSystem handler that performs actual file operations
std::shared_ptr<effects::EffectHandler> create_real_filesystem_handler() {
    auto effect_def = effects::create_filesystem_effect();
    auto handler = std::make_shared<effects::EffectHandler>(effect_def);
    
    // read operation - demonstrates parameter inspection and return value modification
    handler->set_handler("read", 
        [](const std::vector<kernel::Value>& args, kernel::Continuation& cont) -> kernel::Value {
            // REQUIREMENT 41.15: Inspect effect parameters
            if (args.size() != 1) {
                throw std::runtime_error("FileSystem.read requires 1 argument");
            }
            
            auto path_result = args[0].try_as<kernel::String>();
            if (!path_result.has_value()) {
                throw std::runtime_error("FileSystem.read: path must be a string");
            }
            
            const std::string& path = path_result.value()->value();
            
            try {
                std::ifstream file(path);
                if (!file.is_open()) {
                    throw std::runtime_error("Failed to open file: " + path);
                }
                
                std::string content((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
                
                // REQUIREMENT 41.16: Modify return values through resume(value)
                // The handler can inspect the content and modify it before resuming
                // For example, we could filter sensitive information, add metadata, etc.
                return cont.resume(kernel::Value(std::make_shared<kernel::String>(content)));
            } catch (const std::exception& e) {
                throw std::runtime_error("FileSystem.read error: " + std::string(e.what()));
            }
        });
    
    // write operation - demonstrates parameter inspection and validation
    handler->set_handler("write",
        [](const std::vector<kernel::Value>& args, kernel::Continuation& cont) -> kernel::Value {
            // REQUIREMENT 41.15: Inspect effect parameters
            if (args.size() != 2) {
                throw std::runtime_error("FileSystem.write requires 2 arguments");
            }
            
            auto path_result = args[0].try_as<kernel::String>();
            auto content_result = args[1].try_as<kernel::String>();
            
            if (!path_result.has_value()) {
                throw std::runtime_error("FileSystem.write: path must be a string");
            }
            if (!content_result.has_value()) {
                throw std::runtime_error("FileSystem.write: content must be a string");
            }
            
            const std::string& path = path_result.value()->value();
            const std::string& content = content_result.value()->value();
            
            // Handler can inspect and validate parameters before proceeding
            // For example, check path permissions, content size limits, etc.
            if (path.empty()) {
                throw std::runtime_error("FileSystem.write: path cannot be empty");
            }
            
            try {
                std::ofstream file(path);
                if (!file.is_open()) {
                    throw std::runtime_error("Failed to open file for writing: " + path);
                }
                
                file << content;
                file.close();
                
                // REQUIREMENT 41.5: Resume execution from suspension point
                // Resume with empty (success) - could also return bytes written, etc.
                return cont.resume(kernel::Value(kernel::Empty::instance()));
            } catch (const std::exception& e) {
                throw std::runtime_error("FileSystem.write error: " + std::string(e.what()));
            }
        });
    
    // exists operation - demonstrates simple parameter inspection and boolean return
    handler->set_handler("exists",
        [](const std::vector<kernel::Value>& args, kernel::Continuation& cont) -> kernel::Value {
            // REQUIREMENT 41.15: Inspect effect parameters
            if (args.size() != 1) {
                throw std::runtime_error("FileSystem.exists requires 1 argument");
            }
            
            auto path_result = args[0].try_as<kernel::String>();
            if (!path_result.has_value()) {
                throw std::runtime_error("FileSystem.exists: path must be a string");
            }
            
            const std::string& path = path_result.value()->value();
            
            std::ifstream file(path);
            bool exists = file.good();
            
            // REQUIREMENT 41.16: Resume with specific return value
            return cont.resume(kernel::Value(kernel::Boolean::from(exists)));
        });
    
    // delete operation - demonstrates error handling in handlers
    handler->set_handler("delete",
        [](const std::vector<kernel::Value>& args, kernel::Continuation& cont) -> kernel::Value {
            // REQUIREMENT 41.15: Inspect effect parameters
            if (args.size() != 1) {
                throw std::runtime_error("FileSystem.delete requires 1 argument");
            }
            
            auto path_result = args[0].try_as<kernel::String>();
            if (!path_result.has_value()) {
                throw std::runtime_error("FileSystem.delete: path must be a string");
            }
            
            const std::string& path = path_result.value()->value();
            
            try {
                if (std::remove(path.c_str()) != 0) {
                    throw std::runtime_error("Failed to delete file: " + path);
                }
                
                // REQUIREMENT 41.5: Resume execution from suspension point
                return cont.resume(kernel::Value(kernel::Empty::instance()));
            } catch (const std::exception& e) {
                throw std::runtime_error("FileSystem.delete error: " + std::string(e.what()));
            }
        });
    
    return handler;
}

// Create a mock FileSystem handler for testing/sandboxing
// This demonstrates AI safety patterns where handlers can intercept and modify operations
std::shared_ptr<effects::EffectHandler> create_mock_filesystem_handler() {
    auto effect_def = effects::create_filesystem_effect();
    auto handler = std::make_shared<effects::EffectHandler>(effect_def);
    
    // Mock storage for the sandbox
    static std::unordered_map<std::string, std::string> mock_files;
    
    // read operation (mock) - demonstrates parameter inspection and sandboxing
    handler->set_handler("read",
        [](const std::vector<kernel::Value>& args, kernel::Continuation& cont) -> kernel::Value {
            // REQUIREMENT 41.15: Inspect effect parameters for validation/sandboxing
            if (args.size() != 1) {
                throw std::runtime_error("FileSystem.read requires 1 argument");
            }
            
            auto path_result = args[0].try_as<kernel::String>();
            if (!path_result.has_value()) {
                throw std::runtime_error("FileSystem.read: path must be a string");
            }
            
            const std::string& path = path_result.value()->value();
            
            // AI SAFETY: Handler can inspect and restrict file access
            // For example, prevent access to sensitive files
            if (path.find("/etc/") == 0 || path.find("C:\\Windows\\") == 0) {
                std::cout << "[SANDBOX] Blocked access to sensitive path: " << path << std::endl;
                throw std::runtime_error("Access denied: " + path);
            }
            
            auto it = mock_files.find(path);
            if (it == mock_files.end()) {
                throw std::runtime_error("Mock file not found: " + path);
            }
            
            // REQUIREMENT 41.16: Modify return values - could filter content, add metadata, etc.
            std::string content = it->second;
            
            // Example: Add sandbox metadata to the content
            content = "[SANDBOX] " + content;
            
            std::cout << "[SANDBOX] Read from " << path << ": " << content.size() << " bytes" << std::endl;
            
            // Resume with the (potentially modified) mock file content
            return cont.resume(kernel::Value(std::make_shared<kernel::String>(content)));
        });
    
    // write operation (mock) - demonstrates parameter inspection and logging
    handler->set_handler("write",
        [](const std::vector<kernel::Value>& args, kernel::Continuation& cont) -> kernel::Value {
            // REQUIREMENT 41.15: Inspect effect parameters
            if (args.size() != 2) {
                throw std::runtime_error("FileSystem.write requires 2 arguments");
            }
            
            auto path_result = args[0].try_as<kernel::String>();
            auto content_result = args[1].try_as<kernel::String>();
            
            if (!path_result.has_value()) {
                throw std::runtime_error("FileSystem.write: path must be a string");
            }
            if (!content_result.has_value()) {
                throw std::runtime_error("FileSystem.write: content must be a string");
            }
            
            const std::string& path = path_result.value()->value();
            const std::string& content = content_result.value()->value();
            
            // AI SAFETY: Handler can inspect and log all write operations
            std::cout << "[SANDBOX] Write to " << path << ": " << content.size() << " bytes" << std::endl;
            std::cout << "[SANDBOX] Content preview: " << content.substr(0, std::min(content.size(), size_t(50))) << "..." << std::endl;
            
            // Handler can validate content before allowing the write
            if (content.find("malicious") != std::string::npos) {
                std::cout << "[SANDBOX] Blocked write containing suspicious content" << std::endl;
                throw std::runtime_error("Content validation failed");
            }
            
            // Store in mock filesystem
            mock_files[path] = content;
            
            // REQUIREMENT 41.5: Resume execution from suspension point
            return cont.resume(kernel::Value(kernel::Empty::instance()));
        });
    
    // exists operation (mock) - demonstrates simple parameter inspection
    handler->set_handler("exists",
        [](const std::vector<kernel::Value>& args, kernel::Continuation& cont) -> kernel::Value {
            // REQUIREMENT 41.15: Inspect effect parameters
            if (args.size() != 1) {
                throw std::runtime_error("FileSystem.exists requires 1 argument");
            }
            
            auto path_result = args[0].try_as<kernel::String>();
            if (!path_result.has_value()) {
                throw std::runtime_error("FileSystem.exists: path must be a string");
            }
            
            const std::string& path = path_result.value()->value();
            bool exists = mock_files.find(path) != mock_files.end();
            
            std::cout << "[SANDBOX] Check exists " << path << ": " << (exists ? "true" : "false") << std::endl;
            
            // REQUIREMENT 41.16: Resume with specific return value
            return cont.resume(kernel::Value(kernel::Boolean::from(exists)));
        });
    
    return handler;
}

// Create a real Console handler
std::shared_ptr<effects::EffectHandler> create_real_console_handler() {
    auto effect_def = effects::create_console_effect();
    auto handler = std::make_shared<effects::EffectHandler>(effect_def);
    
    // print operation
    handler->set_handler("print",
        [](const std::vector<kernel::Value>& args, kernel::Continuation& cont) -> kernel::Value {
            if (args.size() != 1) {
                throw std::runtime_error("Console.print requires 1 argument");
            }
            
            auto message_result = args[0].try_as<kernel::String>();
            if (!message_result.has_value()) {
                throw std::runtime_error("Console.print: message must be a string");
            }
            
            const std::string& message = message_result.value()->value();
            std::cout << message << std::endl;
            
            // Resume with empty (success)
            return cont.resume(kernel::Value(kernel::Empty::instance()));
        });
    
    // readLine operation
    handler->set_handler("readLine",
        [](const std::vector<kernel::Value>& args, kernel::Continuation& cont) -> kernel::Value {
            if (args.size() != 0) {
                throw std::runtime_error("Console.readLine requires 0 arguments");
            }
            
            std::string line;
            std::getline(std::cin, line);
            
            // Resume with the input line
            return cont.resume(kernel::Value(std::make_shared<kernel::String>(line)));
        });
    
    return handler;
}

// Create a real Time handler
std::shared_ptr<effects::EffectHandler> create_real_time_handler() {
    auto effect_def = effects::create_time_effect();
    auto handler = std::make_shared<effects::EffectHandler>(effect_def);
    
    // now operation
    handler->set_handler("now",
        [](const std::vector<kernel::Value>& args, kernel::Continuation& cont) -> kernel::Value {
            if (args.size() != 0) {
                throw std::runtime_error("Time.now requires 0 arguments");
            }
            
            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
            
            // Resume with current timestamp
            return cont.resume(kernel::Value(std::make_shared<kernel::Integer>(now)));
        });
    
    // sleep operation
    handler->set_handler("sleep",
        [](const std::vector<kernel::Value>& args, kernel::Continuation& cont) -> kernel::Value {
            if (args.size() != 1) {
                throw std::runtime_error("Time.sleep requires 1 argument");
            }
            
            auto ms_result = args[0].try_as<kernel::Integer>();
            if (!ms_result.has_value()) {
                throw std::runtime_error("Time.sleep: duration must be an integer");
            }
            
            int64_t milliseconds = ms_result.value()->value();
            std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
            
            // Resume with empty (success)
            return cont.resume(kernel::Value(kernel::Empty::instance()));
        });
    
    return handler;
}

// Create a real Random handler
std::shared_ptr<effects::EffectHandler> create_real_random_handler() {
    auto effect_def = effects::create_random_effect();
    auto handler = std::make_shared<effects::EffectHandler>(effect_def);
    
    // Thread-local random number generator
    static thread_local std::random_device rd;
    static thread_local std::mt19937 gen(rd());
    
    // nextInt operation
    handler->set_handler("nextInt",
        [](const std::vector<kernel::Value>& args, kernel::Continuation& cont) -> kernel::Value {
            if (args.size() != 1) {
                throw std::runtime_error("Random.nextInt requires 1 argument");
            }
            
            auto max_result = args[0].try_as<kernel::Integer>();
            if (!max_result.has_value()) {
                throw std::runtime_error("Random.nextInt: max must be an integer");
            }
            
            int64_t max_value = max_result.value()->value();
            std::uniform_int_distribution<int64_t> dist(0, max_value - 1);
            int64_t result = dist(gen);
            
            // Resume with random integer
            return cont.resume(kernel::Value(std::make_shared<kernel::Integer>(result)));
        });
    
    // nextFloat operation
    handler->set_handler("nextFloat",
        [](const std::vector<kernel::Value>& args, kernel::Continuation& cont) -> kernel::Value {
            if (args.size() != 0) {
                throw std::runtime_error("Random.nextFloat requires 0 arguments");
            }
            
            std::uniform_real_distribution<double> dist(0.0, 1.0);
            double result = dist(gen);
            
            // Resume with random float (stored as Integer for now - would need Float type)
            // For now, multiply by 1000000 and store as integer
            int64_t int_result = static_cast<int64_t>(result * 1000000);
            return cont.resume(kernel::Value(std::make_shared<kernel::Integer>(int_result)));
        });
    
    return handler;
}

// ============================================================================
// CONVENIENCE FUNCTIONS FOR HANDLER CREATION
// ============================================================================

// Install real handlers for all built-in effects
void install_real_handlers() {
    // Note: This would typically be called during runtime initialization
    // For now, we provide factory functions that users can call manually
}

// Install mock handlers for testing/sandboxing
void install_mock_handlers() {
    // Note: This would typically be used in test environments
    // For now, we provide factory functions that users can call manually
}

// ============================================================================
// PERFORM FUNCTION - CORE EFFECT PERFORMANCE MECHANISM
// ============================================================================

// Template-based perform function for type-safe effect performance
// This is the main entry point for performing effects in user code
//
// Usage:
//   auto result = perform<string>("FileSystem", "read", {path_value});
//   
// This function:
// 1. Searches the handler stack for a handler for the specified effect
// 2. If found, uses suspend() to capture the continuation and invoke the handler
// 3. If not found, throws an unhandled effect error
// 4. Returns the result with proper type casting
//
// Requirements implemented:
// - 41.3: perform as library function using primitive_suspend
// - 41.13: Search handler stack for nearest matching handler  
// - 41.14: Execute handler code and capture continuation
template<typename T>
T perform_typed(const std::string& effect_name,
               const std::string& operation_name,
               const std::vector<kernel::Value>& args) {
    // Find the nearest handler for this effect
    auto handler = HandlerStack::instance().find_handler(effect_name);
    if (!handler) {
        throw std::runtime_error(std::format("Unhandled effect: {}.{}", effect_name, operation_name));
    }
    
    // Get the delimiter ID for this handler
    std::string delimiter_id = HandlerStack::instance().find_delimiter_id(effect_name);
    if (delimiter_id.empty()) {
        throw std::runtime_error(std::format("No delimiter found for effect: {}", effect_name));
    }
    
    // Use suspend() to capture the continuation and invoke the handler
    // This is where primitive_suspend is used internally via the suspend() library function
    kernel::Value result = suspend(delimiter_id, 
        [handler, operation_name, args](std::shared_ptr<kernel::Continuation> cont) -> kernel::Value {
            // Invoke the handler with the operation, arguments, and continuation
            // The handler will decide what to do with the continuation:
            // - Resume it immediately (pass-through)
            // - Resume it later (async)
            // - Discard it (exception handling)
            // - Transform the value before resuming
            return handler->handle(operation_name, args, *cont);
        });
    
    // Type-safe conversion of the result
    // In a real implementation, this would use proper type checking and conversion
    // For now, we assume the handler returns the correct type
    if constexpr (std::is_same_v<T, kernel::Value>) {
        return result;
    } else if constexpr (std::is_same_v<T, std::string>) {
        auto str_result = result.try_as<kernel::String>();
        if (str_result.has_value()) {
            return str_result.value()->value();
        }
        throw std::runtime_error("Effect result is not a string");
    } else if constexpr (std::is_same_v<T, int64_t>) {
        auto int_result = result.try_as<kernel::Integer>();
        if (int_result.has_value()) {
            return int_result.value()->value();
        }
        throw std::runtime_error("Effect result is not an integer");
    } else if constexpr (std::is_same_v<T, bool>) {
        auto bool_result = result.try_as<kernel::Boolean>();
        if (bool_result.has_value()) {
            return bool_result.value()->value();
        }
        throw std::runtime_error("Effect result is not a boolean");
    } else {
        // For other types, return the raw Value and let the caller handle conversion
        static_assert(std::is_same_v<T, kernel::Value>, "Unsupported return type for perform<T>");
        return result;
    }
}

// Explicit instantiations for common types
template std::string perform_typed<std::string>(const std::string&, const std::string&, const std::vector<kernel::Value>&);
template int64_t perform_typed<int64_t>(const std::string&, const std::string&, const std::vector<kernel::Value>&);
template bool perform_typed<bool>(const std::string&, const std::string&, const std::vector<kernel::Value>&);
template kernel::Value perform_typed<kernel::Value>(const std::string&, const std::string&, const std::vector<kernel::Value>&);

} // namespace meld::stdlib