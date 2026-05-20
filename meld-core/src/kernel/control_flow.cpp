#include "meld/kernel/control_flow.hpp"
#include "meld/kernel/operations.hpp"
#include <stdexcept>

namespace meld::kernel {

// Helper to extract boolean value from Value
bool extract_bool(const Value& val) {
    if (val.is<Boolean>()) {
        return val.as<Boolean>()->value();
    }
    throw std::runtime_error("Expected Boolean value");
}

// Helper to extract integer value from Value
int64_t extract_int(const Value& val) {
    if (val.is<Integer>()) {
        return val.as<Integer>()->value();
    }
    throw std::runtime_error("Expected Integer value");
}

// Helper to extract function from Value
std::shared_ptr<Function> extract_function(const Value& val) {
    if (val.is<Function>()) {
        return val.as<Function>();
    }
    throw std::runtime_error("Expected Function value");
}

void register_control_flow_extensions() {
    ExtensionRegistry& registry = ExtensionRegistry::instance();
    
    // Boolean.ifTrue: method
    // Returns an IfTrueBuilder for chaining with ifFalse:
    registry.register_extension(
        "Boolean",
        "ifTrue",
        [](const std::vector<Value>& args) -> Value {
            if (args.size() != 2) {
                throw std::runtime_error("ifTrue: requires 2 arguments (self, block)");
            }
            
            bool condition = extract_bool(args[0]);
            auto true_block = extract_function(args[1]);
            
            // Create IfTrueBuilder with captured condition and block
            auto builder = std::make_shared<IfTrueBuilder>(
                condition,
                [true_block]() -> Value {
                    // Execute the true block
                    if (true_block->impl()) {
                        return (*true_block->impl())(std::vector<Value>{});
                    }
                    // For AST-based functions, we'd need an evaluator
                    return Value{}; // Unit
                }
            );
            
            // Return the builder wrapped in a Value
            // Note: We need to extend Value to support IfTrueBuilder
            // For now, we'll return a function that represents the builder
            auto builder_fn = std::make_shared<Function>(
                std::vector<std::shared_ptr<Symbol>>{},
                Value(),
                [builder](const std::vector<Value>& false_args) -> Value {
                    if (false_args.size() != 1) {
                        throw std::runtime_error("ifFalse: requires 1 argument (block)");
                    }
                    auto false_block = extract_function(false_args[0]);
                    return builder->ifFalse([false_block]() -> Value {
                        if (false_block->impl()) {
                            return (*false_block->impl())(std::vector<Value>{});
                        }
                        return Value{};
                    });
                },
                "ifFalse"
            );
            
            return Value(builder_fn);
        },
        {"Function"},
        "IfTrueBuilder"
    );
    
    // Boolean.ifTrue:ifFalse: method (direct version)
    registry.register_extension(
        "Boolean",
        "ifTrueIfFalse",
        [](const std::vector<Value>& args) -> Value {
            if (args.size() != 3) {
                throw std::runtime_error("ifTrueIfFalse: requires 3 arguments (self, trueBlock, falseBlock)");
            }
            
            bool condition = extract_bool(args[0]);
            auto true_block = extract_function(args[1]);
            auto false_block = extract_function(args[2]);
            
            if (condition) {
                if (true_block->impl()) {
                    return (*true_block->impl())(std::vector<Value>{});
                }
            } else {
                if (false_block->impl()) {
                    return (*false_block->impl())(std::vector<Value>{});
                }
            }
            
            // Return Unit
            return Value{};
        },
        {"Function", "Function"},
        "Value"
    );
    
    // Int.times: method for counted loops
    registry.register_extension(
        "Int",
        "times",
        [](const std::vector<Value>& args) -> Value {
            if (args.size() != 2) {
                throw std::runtime_error("times: requires 2 arguments (self, block)");
            }
            
            int64_t count = extract_int(args[0]);
            auto block = extract_function(args[1]);
            
            for (int64_t i = 0; i < count; ++i) {
                if (block->impl()) {
                    (*block->impl())(std::vector<Value>{});
                }
            }
            
            // Return Unit
            return Value{};
        },
        {"Function"},
        "Unit"
    );
    
    // Int.to: method returning Range
    registry.register_extension(
        "Int",
        "to",
        [](const std::vector<Value>& args) -> Value {
            if (args.size() != 2) {
                throw std::runtime_error("to: requires 2 arguments (self, end)");
            }
            
            int64_t start = extract_int(args[0]);
            int64_t end = extract_int(args[1]);
            
            // Create a Range and wrap it in a Function that has a do: method
            // For simplicity, we'll return a function that represents the range
            auto range = std::make_shared<Range>(start, end);
            
            // Create a function that represents the Range with a do: method
            auto range_fn = std::make_shared<Function>(
                std::vector<std::shared_ptr<Symbol>>{},
                Value(),
                [range](const std::vector<Value>& do_args) -> Value {
                    if (do_args.size() != 1) {
                        throw std::runtime_error("do: requires 1 argument (block)");
                    }
                    auto block = extract_function(do_args[0]);
                    
                    range->do_iterate([block](int64_t i) {
                        if (block->impl()) {
                            auto int_val = Value(std::make_shared<Integer>(i));
                            (*block->impl())(std::vector<Value>{int_val});
                        }
                    });
                    
                    return Value{}; // Unit
                },
                "do"
            );
            
            return Value(range_fn);
        },
        {"Int"},
        "Range"
    );
    
    // Block.whileTrue: method for conditional loops
    // Note: In practice, blocks would be created by the parser/evaluator
    // This is a placeholder for the extension mechanism
    registry.register_extension(
        "Block",
        "whileTrue",
        [](const std::vector<Value>& args) -> Value {
            if (args.size() != 2) {
                throw std::runtime_error("whileTrue: requires 2 arguments (self, body)");
            }
            
            auto condition_block = extract_function(args[0]);
            auto body_block = extract_function(args[1]);
            
            while (true) {
                // Evaluate condition
                Value cond_result;
                if (condition_block->impl()) {
                    cond_result = (*condition_block->impl())(std::vector<Value>{});
                } else {
                    break;
                }
                
                // Check if condition is true
                if (!cond_result.is<Boolean>() || !cond_result.as<Boolean>()->value()) {
                    break;
                }
                
                // Execute body
                if (body_block->impl()) {
                    (*body_block->impl())(std::vector<Value>{});
                }
            }
            
            // Return Unit
            return Value{};
        },
        {"Function"},
        "Unit"
    );
    
    // Collection.forEach: method for collection iteration
    // Note: This is a generic extension that works with any collection type
    // In practice, specific collection types would register their own forEach implementations
    registry.register_extension(
        "Collection",
        "forEach",
        [](const std::vector<Value>& args) -> Value {
            if (args.size() != 2) {
                throw std::runtime_error("forEach: requires 2 arguments (self, block)");
            }
            
            // For this implementation, we expect the collection to be represented
            // as a Cons list or similar structure
            // This is a placeholder that demonstrates the extension mechanism
            
            auto block = extract_function(args[1]);
            
            // In a real implementation, we would iterate over the collection
            // For now, this is a stub that shows the extension is registered
            
            // Return Unit
            return Value{};
        },
        {"Function"},
        "Unit"
    );
}

} // namespace meld::kernel
