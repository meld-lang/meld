#pragma once

#include "decorator.hpp"

namespace meld::macro {

// @NoArgsConstructor decorator
// Generates a constructor with no arguments that initializes all fields with default values
// Example:
//   @NoArgsConstructor
//   class Person {
//     var name: String
//     var age: Int
//   }
// Generates:
//   func Person(): Person {
//     return Person { name = "", age = 0 }
//   }
std::shared_ptr<Decorator> create_no_args_constructor_decorator();

// @RequiredArgsConstructor decorator
// Generates a constructor with parameters for all non-nullable fields without default values
// Example:
//   @RequiredArgsConstructor
//   class Person {
//     val name: String      // Required (non-nullable, no default)
//     var age: Int = 0      // Not required (has default)
//     var email: String?    // Not required (nullable)
//   }
// Generates:
//   func Person(name: String): Person {
//     return Person { name = name, age = 0, email = null }
//   }
std::shared_ptr<Decorator> create_required_args_constructor_decorator();

// @AllArgsConstructor decorator
// Generates a constructor with parameters for all fields
// Example:
//   @AllArgsConstructor
//   class Person {
//     var name: String
//     var age: Int
//     var email: String?
//   }
// Generates:
//   func Person(name: String, age: Int, email: String?): Person {
//     return Person { name = name, age = age, email = email }
//   }
std::shared_ptr<Decorator> create_all_args_constructor_decorator();

// Register constructor decorators with the registry
void register_constructor_decorators();

} // namespace meld::macro

