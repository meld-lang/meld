#pragma once

#include "decorator.hpp"

namespace meld::macro {

// @Getter decorator
// Generates getter methods for all fields in a class
// Example:
//   @Getter
//   class Person {
//     var name: String
//     var age: Int
//   }
// Generates:
//   func getName(): String { return this.name }
//   func getAge(): Int { return this.age }
std::shared_ptr<Decorator> create_getter_decorator();

// @Setter decorator
// Generates setter methods for all mutable fields in a class
// Example:
//   @Setter
//   class Person {
//     var name: String
//     val birthYear: Int  // No setter generated (immutable)
//   }
// Generates:
//   func setName(value: String) { this.name = value }
std::shared_ptr<Decorator> create_setter_decorator();

// @Property decorator (composite: field rename + visibility + @Getter + @Setter)
// Field-level decorator that:
//   1. Renames the field from `name` to `_name` (underscore prefix)
//   2. Changes backing field visibility to package-private
//   3. Generates public getter: `fnc name() -> T { rtn this._name }`
//   4. Generates public setter (mutable only): `fnc set_name(v: T) { this._name = v }`
// Requirements: 17.1, 17.2, 17.3, 17.4, 17.5, 25B.11
std::shared_ptr<Decorator> create_property_decorator();

// Register property decorators with the registry
void register_property_decorators();

} // namespace meld::macro

