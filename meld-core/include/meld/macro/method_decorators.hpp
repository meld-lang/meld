#pragma once

#include "decorator.hpp"

namespace meld::macro {

// @ToString decorator
// Generates a toString() method that returns a string representation of the object
// Example:
//   @ToString
//   class Person {
//     var name: String
//     var age: Int
//   }
// Generates:
//   func toString(): String {
//     return "Person(name=${this.name}, age=${this.age})"
//   }
std::shared_ptr<Decorator> create_to_string_decorator();

// @EqualsAndHashCode decorator
// Generates equals() and hashCode() methods based on all fields
// Example:
//   @EqualsAndHashCode
//   class Person {
//     var name: String
//     var age: Int
//   }
// Generates:
//   func equals(other: Any): Bool {
//     if (!(other is Person)) return false
//     val otherPerson = other as Person
//     return this.name == otherPerson.name && this.age == otherPerson.age
//   }
//   func hashCode(): Int {
//     var result = 17
//     result = 31 * result + this.name.hashCode()
//     result = 31 * result + this.age.hashCode()
//     return result
//   }
std::shared_ptr<Decorator> create_equals_and_hash_code_decorator();

// Register method generation decorators with the registry
void register_method_decorators();

} // namespace meld::macro

