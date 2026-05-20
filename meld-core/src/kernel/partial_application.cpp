#include "meld/kernel/partial_application.hpp"
#include "meld/kernel/operations.hpp"
#include <algorithm>
#include <format>

namespace meld::kernel {

// Check if a value is a placeholder
bool is_placeholder(const Value& value) {
    return value.is<Placeholder>();
}

// Create a placeholder value
Value make_placeholder() {
    return Value(Placeholder::instance());
}

// Partial application implementation
std::expected<Value, std::string> partial_apply(
    const Value& fn,
    const std::vector<Value>& args
) {
    // Verify fn is a function
    if (!fn.is<Function>()) {
        return std::unexpected("partial_apply: first argument must be a function");
    }
    
    auto func = fn.as<Function>();
    const auto& params = func->params();
    
    // Check that we don't have more arguments than parameters
    if (args.size() > params.size()) {
        return std::unexpected(std::format(
            "partial_apply: too many arguments ({} provided, {} expected)",
            args.size(), params.size()
        ));
    }
    
    // Count placeholders to determine how many parameters the new function needs
    size_t placeholder_count = std::ranges::count_if(args, is_placeholder);
    
    // If no placeholders, this is just a regular function call
    if (placeholder_count == 0) {
        // If all arguments provided, call the function
        if (args.size() == params.size()) {
            return apply(fn, args);
        }
        // Otherwise, create a function that takes the remaining arguments
        placeholder_count = params.size() - args.size();
    }
    
    // Create new parameter list for the partially applied function
    std::vector<std::shared_ptr<Symbol>> new_params;
    for (size_t i = 0; i < placeholder_count; ++i) {
        new_params.push_back(gensym("p"));
    }
    
    // Create a closure that captures the original function and bound arguments
    Function::Environment closure_env;
    closure_env["__original_fn"] = fn;
    
    // Store bound arguments in closure
    for (size_t i = 0; i < args.size(); ++i) {
        closure_env[std::format("__bound_arg_{}", i)] = args[i];
    }
    
    // Store metadata about the partial application
    closure_env["__bound_count"] = Value(std::make_shared<Integer>(args.size()));
    closure_env["__placeholder_count"] = Value(std::make_shared<Integer>(placeholder_count));
    
    // Create native implementation that reconstructs full argument list
    auto impl = [original_fn = fn, bound_args = args, params_size = params.size()](const std::vector<Value>& new_args) -> Value {
        // Reconstruct the full argument list
        std::vector<Value> full_args;
        full_args.reserve(params_size);
        
        size_t new_arg_idx = 0;
        for (const auto& bound_arg : bound_args) {
            if (is_placeholder(bound_arg)) {
                if (new_arg_idx >= new_args.size()) {
                    return Value(std::make_shared<String>(
                        "Error: not enough arguments for partial application"
                    ));
                }
                full_args.push_back(new_args[new_arg_idx++]);
            } else {
                full_args.push_back(bound_arg);
            }
        }
        
        // Add remaining arguments if any (for cases where we have fewer bound args than params)
        while (new_arg_idx < new_args.size() && full_args.size() < params_size) {
            full_args.push_back(new_args[new_arg_idx++]);
        }
        
        // Apply the original function with the reconstructed arguments
        auto result = meld::kernel::apply(original_fn, full_args);
        if (result.has_value()) {
            return result.value();
        } else {
            return Value(std::make_shared<String>(std::format("Error: {}", result.error())));
        }
    };
    
    // Create the partially applied function
    auto partial_fn = std::make_shared<Function>(
        new_params,
        Value(),  // Body is handled by native impl
        impl,
        std::make_optional(std::string("partial_applied")),
        closure_env
    );
    
    return Value(partial_fn);
}

// Currying implementation
std::expected<Value, std::string> curry(const Value& fn) {
    // Verify fn is a function
    if (!fn.is<Function>()) {
        return std::unexpected("curry: argument must be a function");
    }
    
    auto func = fn.as<Function>();
    const auto& params = func->params();
    
    // If function has 0 or 1 parameters, it's already curried
    if (params.size() <= 1) {
        return fn;
    }
    
    // Create a curried function that accumulates arguments
    // curry(f(a, b, c)) => f'(a) => f''(b) => f'''(c)
    
    // Create the outermost curried function (takes first parameter)
    std::vector<std::shared_ptr<Symbol>> curry_params = { params[0] };
    
    Function::Environment curry_env;
    curry_env["__original_fn"] = fn;
    curry_env["__curry_level"] = Value(std::make_shared<Integer>(0));
    curry_env["__total_params"] = Value(std::make_shared<Integer>(params.size()));
    
    auto curry_impl = [original_fn = fn, total_params = params.size()](const std::vector<Value>& args) -> Value {
        if (args.size() != 1) {
            return Value(std::make_shared<String>("Error: curried function expects exactly 1 argument"));
        }
        
        // If this is the last parameter, call the original function
        if (total_params == 1) {
            auto result = meld::kernel::apply(original_fn, args);
            if (result.has_value()) {
                return result.value();
            } else {
                return Value(std::make_shared<String>(std::format("Error: {}", result.error())));
            }
        }
        
        // Otherwise, return a new curried function with one less parameter
        std::vector<Value> partial_args = { args[0] };
        for (size_t i = 1; i < total_params; ++i) {
            partial_args.push_back(make_placeholder());
        }
        
        auto partial_result = partial_apply(original_fn, partial_args);
        if (partial_result.has_value()) {
            // Recursively curry the partially applied function
            auto curry_result = curry(partial_result.value());
            if (curry_result.has_value()) {
                return curry_result.value();
            } else {
                return Value(std::make_shared<String>(std::format("Error: {}", curry_result.error())));
            }
        } else {
            return Value(std::make_shared<String>(std::format("Error: {}", partial_result.error())));
        }
    };
    
    auto curried_fn = std::make_shared<Function>(
        curry_params,
        Value(),
        curry_impl,
        std::make_optional(std::string("curried")),
        curry_env
    );
    
    return Value(curried_fn);
}

// Check if a function is curried
bool is_curried(const Value& fn) {
    if (!fn.is<Function>()) {
        return false;
    }
    
    auto func = fn.as<Function>();
    const auto& env = func->closure_env();
    
    if (!env.has_value()) {
        return false;
    }
    
    // Check if the closure environment has curry metadata
    return env->contains("__curry_level");
}

// Apply a curried function
std::expected<Value, std::string> curry_apply(
    const Value& curried_fn,
    const Value& arg
) {
    if (!curried_fn.is<Function>()) {
        return std::unexpected("curry_apply: first argument must be a function");
    }
    
    // If not curried, just apply normally
    if (!is_curried(curried_fn)) {
        return apply(curried_fn, {arg});
    }
    
    // For curried functions, just apply normally since the curry implementation
    // handles the chaining internally
    return apply(curried_fn, {arg});
}

} // namespace meld::kernel
