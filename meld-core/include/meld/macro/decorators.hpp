#pragma once

#include "property_decorators.hpp"
#include "method_decorators.hpp"
#include "constructor_decorators.hpp"
#include "composite_decorators.hpp"
#include "builder_decorator.hpp"
#include "format_decorators.hpp"
#include "meld/macro/effect_annotation.hpp"
#include "imposes_annotation.hpp"

namespace meld::macro {

// Register all compile-time decorators with the registry
// This function should be called during compiler initialization
// to make all decorators available for use
void register_all_decorators();

// Check if all required decorators are registered
// Returns true if all decorators from Requirement 25 are available
bool all_decorators_registered();

// Get a list of all registered decorator names
std::vector<std::string> get_registered_decorator_names();

} // namespace meld::macro

