#pragma once

#include "decorator.hpp"

namespace meld::macro {

// @Data decorator
// Composite decorator for mutable classes that combines:
// - @Getter (generates getters for all fields)
// - @Setter (generates setters for all mutable fields)
// - @ToString (generates toString method)
// - @EqualsAndHashCode (generates equals and hashCode methods)
// - @AllArgsConstructor (generates constructor with all fields)
//
// Example:
//   @Data
//   class Person {
//     var name: String
//     var age: Int
//   }
// Generates all of the above methods
std::shared_ptr<Decorator> create_data_decorator();

// @Value decorator
// Composite decorator for immutable classes/structs that combines:
// - @Getter (generates getters for all fields)
// - @ToString (generates toString method)
// - @EqualsAndHashCode (generates equals and hashCode methods)
// - @AllArgsConstructor (generates constructor with all fields)
// Note: No @Setter since @Value is for immutable types
//
// Example:
//   @Value
//   class Point {
//     val x: Int
//     val y: Int
//   }
// Generates all of the above methods (except setters)
std::shared_ptr<Decorator> create_value_decorator();

// Register composite decorators with the registry
void register_composite_decorators();

} // namespace meld::macro

