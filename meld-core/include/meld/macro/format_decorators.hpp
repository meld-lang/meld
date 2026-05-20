#pragma once

#include "decorator.hpp"

namespace meld::macro {

// @debug decorator
// Generates debug() and pretty() methods that produce structural representations.
// Uses ${:debug field} for recursive formatting of nested fields.
//
// Example:
//   @debug
//   struct User {
//     val name: string
//     val age: int
//   }
// Generates:
//   extend User {
//     fnc debug() -> string {
//       rtn `User { name: ${:debug self.name}, age: ${:debug self.age} }`
//     }
//     fnc pretty() -> string {
//       rtn pretty-print(self, indent: 0)
//     }
//   }
std::shared_ptr<Decorator> create_debug_decorator();

// @stringify decorator
// Generates a to-string() method that produces a user-facing string representation.
//
// Example:
//   @stringify
//   struct User {
//     val name: string
//     val age: int
//   }
// Generates:
//   extend User {
//     fnc to-string() -> string {
//       rtn `${self.name} (${self.age})`
//     }
//   }
std::shared_ptr<Decorator> create_stringify_decorator();

// Generate a debug() method for a type
kernel::Value generate_debug(
    const std::string& class_name,
    const std::vector<parser::ast::field_declaration>& fields
);

// Generate a pretty() method for a type
kernel::Value generate_pretty(
    const std::string& class_name,
    const std::vector<parser::ast::field_declaration>& fields
);

// Generate a to-string() method (Meld-style kebab-case)
kernel::Value generate_meld_to_string(
    const std::string& class_name,
    const std::vector<parser::ast::field_declaration>& fields
);

// Register format-related decorators (@debug, @stringify) with the registry
void register_format_decorators();

} // namespace meld::macro
