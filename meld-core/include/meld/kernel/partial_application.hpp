#pragma once

#include "primitives.hpp"
#include "operations.hpp"
#include <vector>
#include <optional>
#include <memory>
#include <expected>

namespace meld::kernel {

// Check if a value is a placeholder
bool is_placeholder(const Value& value);

// Create a placeholder value
Value make_placeholder();

// Partial application: bind some arguments, leave others as placeholders
// Returns a new function with fewer parameters
std::expected<Value, std::string> partial_apply(
    const Value& fn,
    const std::vector<Value>& args
);

// Currying: transform a multi-parameter function into a chain of single-parameter functions
// Returns a curried version of the function
std::expected<Value, std::string> curry(const Value& fn);

// Helper to check if a function is already curried
bool is_curried(const Value& fn);

// Apply a curried function with a single argument
std::expected<Value, std::string> curry_apply(
    const Value& curried_fn,
    const Value& arg
);

} // namespace meld::kernel
