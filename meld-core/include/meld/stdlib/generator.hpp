#pragma once

#include "meld/effects/effect.hpp"
#include "meld/effects/builtin_effects.hpp"
#include "meld/kernel/primitives.hpp"
#include <memory>
#include <vector>
#include <optional>
#include <functional>

namespace meld::stdlib {

// ============================================================================
// GENERATOR LIBRARY IMPLEMENTATION
// Task 35.16: Implement generators as effects
// Requirements: 41.24, 41B.3, 41B.6, 41B.9
// ============================================================================

// Generator state enumeration
enum class GeneratorState {
    Created,    // Generator created but not started
    Running,    // Generator is currently executing
    Suspended,  // Generator is suspended at a yield point
    Completed   // Generator has finished execution
};

// Generator class - represents a generator that can yield multiple values
template<typename T>
class Generator {
public:
    // Create a generator from a function that yields values
    // The function should call yield() to produce values
    using GeneratorFunc = std::function<void()>;
    
    explicit Generator(GeneratorFunc func)
        : func_(std::move(func))
        , state_(GeneratorState::Created)
        , has_value_(false) {}
    
    // Check if the generator has more values
    bool has_next() const {
        return state_ != GeneratorState::Completed;
    }
    
    // Get the next value from the generator
    // Returns std::nullopt if the generator is completed
    std::optional<T> next() {
        if (state_ == GeneratorState::Completed) {
            return std::nullopt;
        }
        
        // If this is the first call, start the generator
        if (state_ == GeneratorState::Created) {
            state_ = GeneratorState::Running;
            
            // Install a generator handler that captures yielded values
            auto handler = create_generator_handler<T>(*this);
            effects::EffectRuntime::instance().pushScope(handler);
            
            try {
                // Execute the generator function
                func_();
                
                // If we reach here, the generator completed without yielding
                state_ = GeneratorState::Completed;
                effects::EffectRuntime::instance().popScope();
                
                // Return the last yielded value if any
                if (has_value_) {
                    has_value_ = false;
                    return current_value_;
                }
                return std::nullopt;
            } catch (const std::exception& e) {
                state_ = GeneratorState::Completed;
                effects::EffectRuntime::instance().popScope();
                throw;
            }
        }
        
        // If the generator is suspended, resume it
        if (state_ == GeneratorState::Suspended && continuation_) {
            state_ = GeneratorState::Running;
            
            try {
                // Resume the continuation to get the next value
                continuation_->resume(kernel::Value::from_unit());
                
                // Check if we got a value
                if (has_value_) {
                    has_value_ = false;
                    return current_value_;
                }
                
                // Generator completed
                state_ = GeneratorState::Completed;
                return std::nullopt;
            } catch (const std::exception& e) {
                state_ = GeneratorState::Completed;
                throw;
            }
        }
        
        return std::nullopt;
    }
    
    // Get the current state of the generator
    GeneratorState state() const {
        return state_;
    }
    
    // Collect all remaining values into a vector
    std::vector<T> collect() {
        std::vector<T> results;
        while (auto value = next()) {
            results.push_back(*value);
        }
        return results;
    }
    
    // Internal: Store a yielded value
    void store_value(const T& value) {
        current_value_ = value;
        has_value_ = true;
    }
    
    // Internal: Store the continuation for resumption
    void store_continuation(std::shared_ptr<kernel::Continuation> cont) {
        continuation_ = std::move(cont);
        state_ = GeneratorState::Suspended;
    }
    
private:
    GeneratorFunc func_;
    GeneratorState state_;
    T current_value_;
    bool has_value_;
    std::shared_ptr<kernel::Continuation> continuation_;
    
    // Create a generator-specific handler
    template<typename U>
    static std::shared_ptr<effects::EffectHandler> create_generator_handler(Generator<U>& gen) {
        auto effect = effects::create_generator_effect();
        auto handler = std::make_shared<effects::EffectHandler>(effect);
        
        // yield(value: T) -> void
        // This handler captures the yielded value and stores the continuation
        // for later resumption, enabling multiple yields
        handler->set_enhanced_handler("yield", 
            [&gen](const std::vector<kernel::Value>& args, effects::EffectContinuation& cont) -> kernel::Value {
                if (args.size() != 1) {
                    throw std::runtime_error("Generator.yield requires 1 argument (value)");
                }
                
                try {
                    // Extract the yielded value
                    // For now, we'll assume the value is a string
                    // In a real implementation, we'd have proper type conversion
                    std::string value_str = args[0].as_string();
                    
                    // Store the value in the generator
                    // Note: This is a simplified implementation
                    // In a real implementation, we'd need proper type handling
                    if constexpr (std::is_same_v<U, std::string>) {
                        gen.store_value(value_str);
                    } else if constexpr (std::is_same_v<U, int64_t>) {
                        gen.store_value(args[0].as_int());
                    } else {
                        // For other types, we'd need proper conversion
                        throw std::runtime_error("Unsupported generator value type");
                    }
                    
                    // Store the continuation for later resumption
                    // This is the key to generators: we don't resume immediately
                    // Instead, we save the continuation and return control to the caller
                    gen.store_continuation(cont.get_kernel_continuation());
                    
                    // Return a special value indicating suspension
                    // The generator will resume this continuation when next() is called
                    return kernel::Value::from_unit();
                } catch (const std::exception& e) {
                    throw std::runtime_error("Generator.yield failed: " + std::string(e.what()));
                }
            });
        
        return handler;
    }
};

// ============================================================================
// LIBRARY FUNCTIONS FOR GENERATOR OPERATIONS
// ============================================================================

// yield function - performs Generator.yield effect
// This is a library function (not a keyword) that uses the effect system
// Requirement 41.24: Implement yield as library function performing Generator.yield
template<typename T>
void yield(const T& value) {
    // Convert value to kernel::Value
    kernel::Value kernel_value;
    if constexpr (std::is_same_v<T, std::string>) {
        kernel_value = kernel::Value::from_string(value);
    } else if constexpr (std::is_same_v<T, int64_t> || std::is_same_v<T, int>) {
        kernel_value = kernel::Value::from_int(static_cast<int64_t>(value));
    } else if constexpr (std::is_same_v<T, bool>) {
        kernel_value = kernel::Value::from_bool(value);
    } else {
        // For other types, convert to string
        kernel_value = kernel::Value::from_string(std::to_string(value));
    }
    
    // Perform the Generator.yield effect
    // This will suspend execution and capture the continuation
    effects::perform("Generator", "yield", {kernel_value});
}

// Convenience function to create a generator
template<typename T>
Generator<T> make_generator(typename Generator<T>::GeneratorFunc func) {
    return Generator<T>(std::move(func));
}

// ============================================================================
// GENERATOR ITERATOR IMPLEMENTATION
// ============================================================================

// Iterator wrapper for Generator to enable range-based for loops
template<typename T>
class GeneratorIterator {
public:
    using iterator_category = std::input_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = T*;
    using reference = T&;
    
    explicit GeneratorIterator(Generator<T>* gen, bool is_end = false)
        : gen_(gen), is_end_(is_end) {
        if (!is_end_ && gen_) {
            advance();
        }
    }
    
    T operator*() const {
        if (!current_value_) {
            throw std::runtime_error("Dereferencing invalid generator iterator");
        }
        return *current_value_;
    }
    
    GeneratorIterator& operator++() {
        advance();
        return *this;
    }
    
    GeneratorIterator operator++(int) {
        GeneratorIterator tmp = *this;
        advance();
        return tmp;
    }
    
    bool operator==(const GeneratorIterator& other) const {
        if (is_end_ && other.is_end_) {
            return true;
        }
        if (is_end_ != other.is_end_) {
            return false;
        }
        return gen_ == other.gen_ && current_value_.has_value() == other.current_value_.has_value();
    }
    
    bool operator!=(const GeneratorIterator& other) const {
        return !(*this == other);
    }
    
private:
    void advance() {
        if (gen_) {
            current_value_ = gen_->next();
            if (!current_value_) {
                is_end_ = true;
            }
        } else {
            is_end_ = true;
        }
    }
    
    Generator<T>* gen_;
    bool is_end_;
    std::optional<T> current_value_;
};

// Enable range-based for loops for Generator
template<typename T>
GeneratorIterator<T> begin(Generator<T>& gen) {
    return GeneratorIterator<T>(&gen, false);
}

template<typename T>
GeneratorIterator<T> end(Generator<T>& gen) {
    return GeneratorIterator<T>(&gen, true);
}

} // namespace meld::stdlib
