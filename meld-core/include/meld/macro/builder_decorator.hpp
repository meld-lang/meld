#pragma once

#include "decorator.hpp"

namespace meld::macro {

// @Builder decorator
// Generates a builder class with fluent API for constructing instances
// Example:
//   @Builder
//   class Person {
//     var name: String
//     var age: Int
//     var email: String?
//   }
// Generates:
//   class PersonBuilder {
//     private var name: String = ""
//     private var age: Int = 0
//     private var email: String? = null
//     
//     func name(value: String): PersonBuilder {
//       this.name = value
//       return this
//     }
//     
//     func age(value: Int): PersonBuilder {
//       this.age = value
//       return this
//     }
//     
//     func email(value: String?): PersonBuilder {
//       this.email = value
//       return this
//     }
//     
//     func build(): Person {
//       return Person {
//         name = this.name,
//         age = this.age,
//         email = this.email
//       }
//     }
//   }
//   
//   // Usage:
//   val person = PersonBuilder()
//     .name("Alice")
//     .age(30)
//     .email("alice@example.com")
//     .build()
std::shared_ptr<Decorator> create_builder_decorator();

// Register builder decorator with the registry
void register_builder_decorator();

} // namespace meld::macro

