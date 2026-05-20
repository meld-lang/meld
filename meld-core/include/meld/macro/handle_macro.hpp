#pragma once

#include "meld/kernel/primitives.hpp"
#include "macro.hpp"
#include <string>
#include <vector>
#include <expected>

namespace meld::macro {

// Structure representing a single handler definition within a handle block
struct HandlerDefinition {
    std::string operation_name;      // Name of the effect operation (e.g., "read", "write")
    std::vector<std::string> parameters;  // Parameter names for the handler
    kernel::Value body;              // AST of the handler body
};

// Structure representing a complete effect handler (one per inline trait impl)
struct EffectHandlerGroup {
    std::string effect_name;                    // Effect trait name (e.g., "FileSystem")
    std::vector<HandlerDefinition> operations;  // Handler definitions for each operation
};

// Main handle macro expansion function (unified syntax)
//
// Expands handle macro calls from:
//   handle({ body }, EffectName { fnc op1() { ... } }, EffectName2 { fnc op2() { ... } })
//
// To:
//   {
//       // For each effect handler:
//       auto handler1 = create_effect_handler("EffectName");
//       handler1->set_handler("op1", [](args, cont) { ... });
//       EffectScope scope1("EffectName", handler1);
//
//       auto handler2 = create_effect_handler("EffectName2");
//       handler2->set_handler("op2", [](args, cont) { ... });
//       EffectScope scope2("EffectName2", handler2);
//
//       // Execute body
//       val result = body()
//       result
//   }
//
// Requirements: 144.1, 144.2, 144.3, 144.5
std::expected<kernel::Value, std::string> 
expand_handle_macro(const kernel::Value& ast_node, MacroExpander& expander);

// Parse handler definitions from a single handler's AST
std::expected<std::vector<HandlerDefinition>, std::string>
parse_handler_definitions(const kernel::Value& handlers_ast);

// Parse a single handler definition
std::expected<HandlerDefinition, std::string>
parse_single_handler(const kernel::Value& handler_ast);

// Generate the expanded AST for the handle macro with multiple effect handlers
kernel::Value generate_handle_expansion(
    const kernel::Value& body,
    const std::vector<EffectHandlerGroup>& handler_groups,
    MacroExpander& expander
);

// Generate a set_handler call for a single handler definition
kernel::Value generate_set_handler_call(
    std::shared_ptr<kernel::Symbol> handler_var,
    const HandlerDefinition& handler_def,
    MacroExpander& expander
);

// Register the handle macro with the macro system
void register_handle_macro();

} // namespace meld::macro
