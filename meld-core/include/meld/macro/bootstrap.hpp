#pragma once

#include "macro.hpp"
#include "blueprint.hpp"
#include "meld/meta/metatype.hpp"
#include "meld/effects/effect.hpp"

namespace meld::macro {

// Bootstrap core language constructs as macros
// These macros generate MetaType instances for class, struct, and trait definitions

// Register all bootstrap macros
void register_bootstrap_macros();

// Register all AI-native macros (including @blueprint)
void register_ai_macros();

// Individual bootstrap macro creators

// class macro - generates ClassMetaType instances
// Syntax: (class ClassName (field1 Type1) (field2 Type2) ... (method1 ...) ...)
std::shared_ptr<Macro> create_class_macro();

// struct macro - generates StructMetaType instances
// Syntax: (struct StructName (field1 Type1) (field2 Type2) ...)
std::shared_ptr<Macro> create_struct_macro();

// trait macro - generates TraitMetaType instances
// Syntax: (trait TraitName (method1 ...) (method2 ...) ...)
std::shared_ptr<Macro> create_trait_macro();

// effect macro - generates EffectDefinition instances
// Syntax: (effect EffectName (operation1 ...) (operation2 ...) ...)
std::shared_ptr<Macro> create_effect_macro();

// Helper functions for macro implementations

// Extract field definitions from AST
std::expected<std::vector<meta::Field>, std::string>
extract_fields(const std::vector<kernel::Value>& field_defs);

// Extract method definitions from AST
std::expected<std::vector<meta::Method>, std::string>
extract_methods(const std::vector<kernel::Value>& method_defs);

// Extract effect operation definitions from AST
std::expected<std::vector<effects::EffectOperation>, std::string>
extract_effect_operations(const std::vector<kernel::Value>& operation_defs);

// Generate constructor function for a class/struct
kernel::Value generate_constructor(
    const std::string& name,
    const std::vector<meta::Field>& fields
);

// Generate method functions
std::vector<kernel::Value> generate_methods(
    const std::string& class_name,
    const std::vector<meta::Method>& methods
);

// Generate effect dispatch logic
kernel::Value generate_effect_dispatch(
    const std::string& effect_name,
    const std::vector<effects::EffectOperation>& operations
);

} // namespace meld::macro
