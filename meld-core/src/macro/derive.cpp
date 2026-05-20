#include "meld/macro/derive.hpp"
#include "meld/kernel/operations.hpp"
#include <format>
#include <sstream>
#include <algorithm>

namespace meld::macro {

// ============================================================================
// DeriveTrait Implementation
// ============================================================================

std::expected<kernel::Value, std::string> 
DeriveTrait::derive(const parser::ast::class_definition& class_def, MacroExpander& expander) const {
    return transformer_(class_def, expander);
}

// ============================================================================
// DeriveTraitRegistry Implementation
// ============================================================================

void DeriveTraitRegistry::register_trait(std::shared_ptr<DeriveTrait> trait) {
    std::lock_guard<std::mutex> lock(mutex_);
    traits_[trait->name()] = std::move(trait);
}

std::expected<std::shared_ptr<DeriveTrait>, std::string> 
DeriveTraitRegistry::get_trait(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = traits_.find(name);
    if (it != traits_.end()) {
        return it->second;
    }
    return std::unexpected(std::format("Derivable trait '{}' not found", name));
}

bool DeriveTraitRegistry::has_trait(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return traits_.contains(name);
}

void DeriveTraitRegistry::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    traits_.clear();
}

// ============================================================================
// DeriveMacro Implementation
// ============================================================================

std::vector<std::string> DeriveMacro::parse_derive_annotation(
    const parser::ast::class_definition& class_def) {
    
    std::vector<std::string> trait_names;
    
    // Parse @derive(Trait1, Trait2, ...) from class annotations
    // For now, this is a placeholder - in a full implementation,
    // this would parse actual annotations from the AST
    
    // TODO: Parse actual @derive annotations when parser supports them
    // For now, we'll look for a special metadata field
    
    return trait_names;
}

std::expected<kernel::Value, std::string> 
DeriveMacro::apply(const parser::ast::class_definition& class_def, MacroExpander& expander) {
    
    // Parse trait names from @derive annotation
    auto trait_names = parse_derive_annotation(class_def);
    
    if (trait_names.empty()) {
        // No traits to derive
        return kernel::Value(std::make_shared<kernel::Symbol>(class_def.name.name));
    }
    
    // Derive all specified traits
    return derive_traits(class_def, trait_names, expander);
}

std::expected<kernel::Value, std::string> 
DeriveMacro::derive_trait(
    const parser::ast::class_definition& class_def,
    const std::string& trait_name,
    MacroExpander& expander) {
    
    // Look up the derivable trait
    auto trait_result = DeriveTraitRegistry::instance().get_trait(trait_name);
    if (!trait_result) {
        return std::unexpected(trait_result.error());
    }
    
    // Derive the trait
    return (*trait_result)->derive(class_def, expander);
}

std::expected<kernel::Value, std::string> 
DeriveMacro::derive_traits(
    const parser::ast::class_definition& class_def,
    const std::vector<std::string>& trait_names,
    MacroExpander& expander) {
    
    std::vector<kernel::Value> trait_impls;
    
    // Derive each trait
    for (const auto& trait_name : trait_names) {
        auto result = derive_trait(class_def, trait_name, expander);
        if (!result) {
            return result;
        }
        trait_impls.push_back(*result);
    }
    
    // Combine all trait implementations
    // In a full implementation, this would merge the generated code
    // For now, we create a list of implementations
    kernel::Value combined = kernel::Value(kernel::nil());
    for (auto it = trait_impls.rbegin(); it != trait_impls.rend(); ++it) {
        combined = kernel::cons(*it, combined);
    }
    
    return combined;
}

// ============================================================================
// Common Trait Derivations
// ============================================================================

std::expected<kernel::Value, std::string> 
derive_debug(const parser::ast::class_definition& class_def, MacroExpander& expander) {
    
    // Generate: func debug(): String { return "<ClassName> { field1: <value1>, ... }" }
    
    std::ostringstream body_oss;
    body_oss << "return \"" << class_def.name.name << " { ";
    
    // Add field representations
    for (size_t i = 0; i < class_def.fields.size(); ++i) {
        if (i > 0) body_oss << ", ";
        const auto& field = class_def.fields[i];
        body_oss << field.name.name << ": ${this." << field.name.name << ".debug()}";
    }
    
    body_oss << " }\"";
    
    // Create method AST
    auto body_sym = std::make_shared<kernel::Symbol>(body_oss.str());
    auto method = generate_method("debug", {}, "String", kernel::Value(body_sym));
    
    // Create trait implementation
    return generate_trait_impl("Debug", class_def.name.name, {method});
}

std::expected<kernel::Value, std::string> 
derive_clone(const parser::ast::class_definition& class_def, MacroExpander& expander) {
    
    // Generate: func clone(): <ClassName> { 
    //   return <ClassName> { field1: this.field1.clone(), ... } 
    // }
    
    // Check if all fields implement Clone
    if (!all_fields_implement_trait(class_def, "Clone")) {
        return std::unexpected(
            std::format("Cannot derive Clone for '{}': not all fields implement Clone",
                       class_def.name.name)
        );
    }
    
    std::ostringstream body_oss;
    body_oss << "return " << class_def.name.name << " { ";
    
    // Clone each field
    for (size_t i = 0; i < class_def.fields.size(); ++i) {
        if (i > 0) body_oss << ", ";
        const auto& field = class_def.fields[i];
        body_oss << field.name.name << ": this." << field.name.name << ".clone()";
    }
    
    body_oss << " }";
    
    // Create method AST
    auto body_sym = std::make_shared<kernel::Symbol>(body_oss.str());
    auto method = generate_method("clone", {}, class_def.name.name, kernel::Value(body_sym));
    
    // Create trait implementation
    return generate_trait_impl("Clone", class_def.name.name, {method});
}

std::expected<kernel::Value, std::string> 
derive_partial_eq(const parser::ast::class_definition& class_def, MacroExpander& expander) {
    
    // Generate: func equals(other: <ClassName>): Bool { 
    //   return this.field1 == other.field1 && ... 
    // }
    
    // Check if all fields implement PartialEq
    if (!all_fields_implement_trait(class_def, "PartialEq")) {
        return std::unexpected(
            std::format("Cannot derive PartialEq for '{}': not all fields implement PartialEq",
                       class_def.name.name)
        );
    }
    
    std::ostringstream body_oss;
    
    if (class_def.fields.empty()) {
        // No fields - all instances are equal
        body_oss << "return true";
    } else {
        body_oss << "return ";
        
        // Compare each field
        for (size_t i = 0; i < class_def.fields.size(); ++i) {
            if (i > 0) body_oss << " && ";
            const auto& field = class_def.fields[i];
            body_oss << "this." << field.name.name << " == other." << field.name.name;
        }
    }
    
    // Create method AST
    auto body_sym = std::make_shared<kernel::Symbol>(body_oss.str());
    auto method = generate_method(
        "equals", 
        {{"other", class_def.name.name}}, 
        "Bool", 
        kernel::Value(body_sym)
    );
    
    // Create trait implementation
    return generate_trait_impl("PartialEq", class_def.name.name, {method});
}

std::expected<kernel::Value, std::string> 
derive_eq(const parser::ast::class_definition& class_def, MacroExpander& expander) {
    
    // Eq requires PartialEq
    // This is a marker trait with no additional methods
    
    // Check if PartialEq is implemented
    // In a full implementation, we would check the type system
    
    // Create marker trait implementation
    return generate_trait_impl("Eq", class_def.name.name, {});
}

std::expected<kernel::Value, std::string> 
derive_hash(const parser::ast::class_definition& class_def, MacroExpander& expander) {
    
    // Generate: func hash(): Int { 
    //   var h = 0
    //   h = 31 * h + field1.hash()
    //   ...
    //   return h
    // }
    
    // Check if all fields implement Hash
    if (!all_fields_implement_trait(class_def, "Hash")) {
        return std::unexpected(
            std::format("Cannot derive Hash for '{}': not all fields implement Hash",
                       class_def.name.name)
        );
    }
    
    std::ostringstream body_oss;
    body_oss << "var h = 0; ";
    
    // Hash each field
    for (const auto& field : class_def.fields) {
        body_oss << "h = 31 * h + this." << field.name.name << ".hash(); ";
    }
    
    body_oss << "return h";
    
    // Create method AST
    auto body_sym = std::make_shared<kernel::Symbol>(body_oss.str());
    auto method = generate_method("hash", {}, "Int", kernel::Value(body_sym));
    
    // Create trait implementation
    return generate_trait_impl("Hash", class_def.name.name, {method});
}

std::expected<kernel::Value, std::string> 
derive_default(const parser::ast::class_definition& class_def, MacroExpander& expander) {
    
    // Generate: func default(): <ClassName> { 
    //   return <ClassName> { field1: default(), ... } 
    // }
    
    // Check if all fields implement Default
    if (!all_fields_implement_trait(class_def, "Default")) {
        return std::unexpected(
            std::format("Cannot derive Default for '{}': not all fields implement Default",
                       class_def.name.name)
        );
    }
    
    std::ostringstream body_oss;
    body_oss << "return " << class_def.name.name << " { ";
    
    // Initialize each field with default value
    for (size_t i = 0; i < class_def.fields.size(); ++i) {
        if (i > 0) body_oss << ", ";
        const auto& field = class_def.fields[i];
        body_oss << field.name.name << ": default()";
    }
    
    body_oss << " }";
    
    // Create method AST
    auto body_sym = std::make_shared<kernel::Symbol>(body_oss.str());
    auto method = generate_method("default", {}, class_def.name.name, kernel::Value(body_sym));
    
    // Create trait implementation
    return generate_trait_impl("Default", class_def.name.name, {method});
}

std::expected<kernel::Value, std::string> 
derive_copy(const parser::ast::class_definition& class_def, MacroExpander& expander) {
    
    // Copy is a marker trait - compiler handles bitwise copying
    // All fields must implement Copy
    
    if (!all_fields_implement_trait(class_def, "Copy")) {
        return std::unexpected(
            std::format("Cannot derive Copy for '{}': not all fields implement Copy",
                       class_def.name.name)
        );
    }
    
    // Create marker trait implementation
    return generate_trait_impl("Copy", class_def.name.name, {});
}

std::expected<kernel::Value, std::string> 
derive_send(const parser::ast::class_definition& class_def, MacroExpander& expander) {
    
    // Send is a marker trait for thread safety
    // All fields must implement Send
    
    if (!all_fields_implement_trait(class_def, "Send")) {
        return std::unexpected(
            std::format("Cannot derive Send for '{}': not all fields implement Send",
                       class_def.name.name)
        );
    }
    
    // Create marker trait implementation
    return generate_trait_impl("Send", class_def.name.name, {});
}

std::expected<kernel::Value, std::string> 
derive_sync(const parser::ast::class_definition& class_def, MacroExpander& expander) {
    
    // Sync is a marker trait for thread safety
    // All fields must implement Sync
    
    if (!all_fields_implement_trait(class_def, "Sync")) {
        return std::unexpected(
            std::format("Cannot derive Sync for '{}': not all fields implement Sync",
                       class_def.name.name)
        );
    }
    
    // Create marker trait implementation
    return generate_trait_impl("Sync", class_def.name.name, {});
}

// ============================================================================
// Helper Functions
// ============================================================================

std::shared_ptr<DeriveTrait> make_derive_trait(
    std::string name,
    DeriveTraitTransformer transformer) {
    
    return std::make_shared<DeriveTrait>(
        std::move(name),
        std::move(transformer)
    );
}

void register_standard_derive_traits() {
    auto& registry = DeriveTraitRegistry::instance();
    
    // Register common derivable traits
    registry.register_trait(make_derive_trait("Debug", derive_debug));
    registry.register_trait(make_derive_trait("Clone", derive_clone));
    registry.register_trait(make_derive_trait("PartialEq", derive_partial_eq));
    registry.register_trait(make_derive_trait("Eq", derive_eq));
    registry.register_trait(make_derive_trait("Hash", derive_hash));
    registry.register_trait(make_derive_trait("Default", derive_default));
    registry.register_trait(make_derive_trait("Copy", derive_copy));
    registry.register_trait(make_derive_trait("Send", derive_send));
    registry.register_trait(make_derive_trait("Sync", derive_sync));
}

kernel::Value generate_trait_impl(
    const std::string& trait_name,
    const std::string& class_name,
    const std::vector<kernel::Value>& methods) {
    
    // Generate: impl <TraitName> for <ClassName> { <methods> }
    
    auto trait_sym = std::make_shared<kernel::Symbol>(trait_name);
    auto class_sym = std::make_shared<kernel::Symbol>(class_name);
    auto impl_sym = std::make_shared<kernel::Symbol>("impl");
    auto for_sym = std::make_shared<kernel::Symbol>("for");
    
    // Build methods list
    kernel::Value methods_list = kernel::Value(kernel::nil());
    for (auto it = methods.rbegin(); it != methods.rend(); ++it) {
        methods_list = kernel::cons(*it, methods_list);
    }
    
    // Create: (impl TraitName for ClassName (methods...))
    auto impl_ast = kernel::cons(
        kernel::Value(impl_sym),
        kernel::cons(
            kernel::Value(trait_sym),
            kernel::cons(
                kernel::Value(for_sym),
                kernel::cons(
                    kernel::Value(class_sym),
                    kernel::cons(
                        methods_list,
                        kernel::Value(kernel::nil())
                    )
                )
            )
        )
    );
    
    return impl_ast;
}

kernel::Value generate_method(
    const std::string& method_name,
    const std::vector<std::pair<std::string, std::string>>& params,
    const std::string& return_type,
    const kernel::Value& body) {
    
    // Generate: func <method_name>(<params>): <return_type> { <body> }
    
    auto func_sym = std::make_shared<kernel::Symbol>("func");
    auto name_sym = std::make_shared<kernel::Symbol>(method_name);
    auto return_sym = std::make_shared<kernel::Symbol>(return_type);
    
    // Build parameter list
    kernel::Value params_list = kernel::Value(kernel::nil());
    for (auto it = params.rbegin(); it != params.rend(); ++it) {
        auto param_name = std::make_shared<kernel::Symbol>(it->first);
        auto param_type = std::make_shared<kernel::Symbol>(it->second);
        
        auto param_pair = kernel::cons(
            kernel::Value(param_name),
            kernel::cons(
                kernel::Value(param_type),
                kernel::Value(kernel::nil())
            )
        );
        
        params_list = kernel::cons(param_pair, params_list);
    }
    
    // Create: (func method_name (params...) return_type body)
    auto method_ast = kernel::cons(
        kernel::Value(func_sym),
        kernel::cons(
            kernel::Value(name_sym),
            kernel::cons(
                params_list,
                kernel::cons(
                    kernel::Value(return_sym),
                    kernel::cons(
                        body,
                        kernel::Value(kernel::nil())
                    )
                )
            )
        )
    );
    
    return method_ast;
}

bool all_fields_implement_trait(
    const parser::ast::class_definition& class_def,
    const std::string& trait_name) {
    
    // In a full implementation, this would check the type system
    // to verify that all field types implement the specified trait
    
    // For now, we assume all fields implement all traits
    // This is a placeholder for proper type system integration
    
    return true;
}

} // namespace meld::macro
