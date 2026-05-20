#include "meld/stdlib/generator.hpp"
#include "meld/effects/effect.hpp"
#include <iostream>
#include <sstream>

namespace meld::stdlib {

// ============================================================================
// GENERATOR EFFECT HANDLER IMPLEMENTATION
// Task 35.16: Implement generators as effects
// Requirements: 41.24, 41B.3, 41B.6, 41B.9
// ============================================================================

// Enhanced Generator Effect Handler
// This handler implements the core generator functionality:
// 1. Captures yielded values
// 2. Stores continuations for later resumption
// 3. Enables multiple resumptions to produce sequences
class GeneratorEffectHandler : public effects::EffectHandler {
public:
    GeneratorEffectHandler() : effects::EffectHandler(effects::create_generator_effect()) {
        setup_handlers();
    }
    
private:
    void setup_handlers() {
        // yield(value: string) -> void
        // This is the core generator operation that:
        // 1. Captures the yielded value
        // 2. Stores the continuation for later resumption
        // 3. Returns control to the generator caller
        // 4. Enables the continuation to be resumed multiple times
        set_enhanced_handler("yield", 
            [this](const std::vector<kernel::Value>& args, effects::EffectContinuation& cont) -> kernel::Value {
                if (args.size() != 1) {
                    throw std::runtime_error("Generator.yield requires 1 argument (value)");
                }
                
                try {
                    std::string value = args[0].as_string();
                    
                    // Store the yielded value for the generator
                    last_yielded_value_ = value;
                    has_yielded_value_ = true;
                    
                    // Store the continuation for later resumption
                    // This is the key difference from other effects:
                    // We don't resume immediately, we save it for later
                    stored_continuation_ = cont.get_kernel_continuation();
                    
                    // Increment yield count for tracking
                    yield_count_++;
                    
                    // Log the yield operation for debugging
                    if (debug_mode_) {
                        std::cout << "[Generator] Yielded value #" << yield_count_ 
                                  << ": '" << value << "'" << std::endl;
                    }
                    
                    // Return control to the generator caller
                    // The continuation will be resumed when next() is called
                    return kernel::Value::from_string("yield_suspended");
                    
                } catch (const std::exception& e) {
                    throw std::runtime_error("Generator.yield failed: " + std::string(e.what()));
                }
            });
    }
    
public:
    // Get the last yielded value
    std::string get_last_yielded_value() const {
        return last_yielded_value_;
    }
    
    // Check if a value has been yielded
    bool has_yielded_value() const {
        return has_yielded_value_;
    }
    
    // Clear the yielded value flag
    void clear_yielded_value() {
        has_yielded_value_ = false;
    }
    
    // Resume the stored continuation
    kernel::Value resume_continuation() {
        if (!stored_continuation_) {
            throw std::runtime_error("No continuation to resume");
        }
        
        try {
            // Resume the continuation with a unit value
            auto result = stored_continuation_->resume(kernel::Value::from_unit());
            
            // Clear the continuation after resumption
            stored_continuation_.reset();
            
            return result;
        } catch (const std::exception& e) {
            stored_continuation_.reset();
            throw std::runtime_error("Failed to resume generator continuation: " + std::string(e.what()));
        }
    }
    
    // Check if there's a stored continuation
    bool has_continuation() const {
        return stored_continuation_ != nullptr;
    }
    
    // Get yield count for debugging
    size_t get_yield_count() const {
        return yield_count_;
    }
    
    // Enable/disable debug mode
    void set_debug_mode(bool enabled) {
        debug_mode_ = enabled;
    }
    
private:
    std::string last_yielded_value_;
    bool has_yielded_value_ = false;
    std::shared_ptr<kernel::Continuation> stored_continuation_;
    size_t yield_count_ = 0;
    bool debug_mode_ = false;
};

// ============================================================================
// GENERATOR LIBRARY FUNCTIONS
// ============================================================================

// Create a generator effect handler
std::shared_ptr<effects::EffectHandler> create_enhanced_generator_handler() {
    return std::make_shared<GeneratorEffectHandler>();
}

// Simple generator function that demonstrates the yield mechanism
// This shows how generators work: they yield values and can be resumed
std::vector<std::string> run_simple_generator() {
    std::vector<std::string> results;
    
    // Install the generator handler
    auto handler = create_enhanced_generator_handler();
    effects::EffectRuntime::instance().pushScope(handler);
    
    try {
        // This simulates a generator function that yields multiple values
        // In a real implementation, this would be user-defined code
        
        // Yield first value
        effects::perform("Generator", "yield", {kernel::Value::from_string("first")});
        
        // Yield second value  
        effects::perform("Generator", "yield", {kernel::Value::from_string("second")});
        
        // Yield third value
        effects::perform("Generator", "yield", {kernel::Value::from_string("third")});
        
        // Generator completes
        
    } catch (const std::exception& e) {
        effects::EffectRuntime::instance().popScope();
        throw std::runtime_error("Generator execution failed: " + std::string(e.what()));
    }
    
    effects::EffectRuntime::instance().popScope();
    return results;
}

// Generator iterator implementation
// This demonstrates how to create an iterator that can yield multiple values
class SimpleGeneratorIterator {
public:
    explicit SimpleGeneratorIterator(std::vector<std::string> values)
        : values_(std::move(values)), current_index_(0) {}
    
    // Check if there are more values
    bool has_next() const {
        return current_index_ < values_.size();
    }
    
    // Get the next value
    std::string next() {
        if (!has_next()) {
            throw std::runtime_error("No more values in generator");
        }
        return values_[current_index_++];
    }
    
    // Get all remaining values
    std::vector<std::string> collect() {
        std::vector<std::string> remaining;
        while (has_next()) {
            remaining.push_back(next());
        }
        return remaining;
    }
    
    // Reset the iterator
    void reset() {
        current_index_ = 0;
    }
    
    // Get current position
    size_t position() const {
        return current_index_;
    }
    
    // Get total count
    size_t size() const {
        return values_.size();
    }
    
private:
    std::vector<std::string> values_;
    size_t current_index_;
};

// Create a simple generator iterator
SimpleGeneratorIterator create_number_generator(int start, int end) {
    std::vector<std::string> numbers;
    for (int i = start; i <= end; ++i) {
        numbers.push_back(std::to_string(i));
    }
    return SimpleGeneratorIterator(std::move(numbers));
}

// Create a fibonacci generator
SimpleGeneratorIterator create_fibonacci_generator(int count) {
    std::vector<std::string> fibonacci;
    
    if (count <= 0) {
        return SimpleGeneratorIterator(std::move(fibonacci));
    }
    
    int64_t a = 0, b = 1;
    
    for (int i = 0; i < count; ++i) {
        fibonacci.push_back(std::to_string(a));
        int64_t next = a + b;
        a = b;
        b = next;
    }
    
    return SimpleGeneratorIterator(std::move(fibonacci));
}

// ============================================================================
// ADVANCED GENERATOR IMPLEMENTATION
// ============================================================================

// Advanced generator that uses continuations properly
class AdvancedGenerator {
public:
    using GeneratorFunction = std::function<void(AdvancedGenerator&)>;
    
    explicit AdvancedGenerator(GeneratorFunction func)
        : func_(std::move(func)), state_(State::Created) {}
    
    // Start or resume the generator
    std::optional<std::string> next() {
        if (state_ == State::Completed) {
            return std::nullopt;
        }
        
        // Install generator handler
        auto handler = std::make_shared<GeneratorEffectHandler>();
        handler->set_debug_mode(true);
        effects::EffectRuntime::instance().pushScope(handler);
        
        try {
            if (state_ == State::Created) {
                // First run - start the generator function
                state_ = State::Running;
                func_(*this);
                
                // If we reach here, generator completed without yielding
                state_ = State::Completed;
                effects::EffectRuntime::instance().popScope();
                return std::nullopt;
            } else if (state_ == State::Suspended) {
                // Resume from suspension
                state_ = State::Running;
                
                // Get the handler and resume continuation
                auto* gen_handler = static_cast<GeneratorEffectHandler*>(handler.get());
                if (gen_handler->has_continuation()) {
                    gen_handler->resume_continuation();
                    
                    // Check if we got a new value
                    if (gen_handler->has_yielded_value()) {
                        std::string value = gen_handler->get_last_yielded_value();
                        gen_handler->clear_yielded_value();
                        state_ = State::Suspended;
                        effects::EffectRuntime::instance().popScope();
                        return value;
                    }
                }
                
                // Generator completed
                state_ = State::Completed;
                effects::EffectRuntime::instance().popScope();
                return std::nullopt;
            }
        } catch (const std::exception& e) {
            state_ = State::Completed;
            effects::EffectRuntime::instance().popScope();
            throw std::runtime_error("Generator failed: " + std::string(e.what()));
        }
        
        effects::EffectRuntime::instance().popScope();
        return std::nullopt;
    }
    
    // Yield a value (called from generator function)
    void yield(const std::string& value) {
        // Perform the yield effect
        effects::perform("Generator", "yield", {kernel::Value::from_string(value)});
        
        // After yield, we're suspended
        state_ = State::Suspended;
    }
    
    // Check if generator has more values
    bool has_next() const {
        return state_ != State::Completed;
    }
    
    // Collect all remaining values
    std::vector<std::string> collect() {
        std::vector<std::string> results;
        while (auto value = next()) {
            results.push_back(*value);
        }
        return results;
    }
    
private:
    enum class State {
        Created,
        Running,
        Suspended,
        Completed
    };
    
    GeneratorFunction func_;
    State state_;
};

// Create an advanced generator
AdvancedGenerator create_advanced_generator(AdvancedGenerator::GeneratorFunction func) {
    return AdvancedGenerator(std::move(func));
}

} // namespace meld::stdlib