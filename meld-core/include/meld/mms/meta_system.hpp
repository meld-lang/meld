#pragma once
#include "meld/parser/ast.hpp"
#include <string>
#include <unordered_map>
#include <vector>

namespace meld::mms {

// A type kind definition (e.g., class, struct, enum, trait, effect)
struct TypeKind {
    std::string name;
    std::unordered_map<std::string, std::string> metadata;
    // Derived properties
    bool is_reference() const { return metadata.count("semantics") && metadata.at("semantics") == "reference"; }
    bool is_value() const { return metadata.count("semantics") && metadata.at("semantics") == "value"; }
    bool is_arc() const { return metadata.count("memory") && metadata.at("memory") == "arc"; }
};

// Result of expanding a type instance (e.g., class Person { ... })
struct ExpansionResult {
    std::string type_name;
    std::string kind_name;
    std::vector<std::string> fields;
    std::vector<std::string> methods;
    TypeKind kind;
};

// The Meld Meta System — resolves type kinds and expands type instances
class MetaSystem {
public:
    MetaSystem();

    // Register a type kind from a `type <kind> { meta_set(...) }` definition
    void register_kind(const std::string& name, const TypeKind& kind);

    // Check if a kind is registered
    bool has_kind(const std::string& name) const;

    // Get a registered kind
    const TypeKind& get_kind(const std::string& name) const;

    // Expand a type instance: `class Foo { fields... methods... }`
    // Returns metadata about the expansion for codegen
    ExpansionResult expand(const std::string& kind_name,
                           const std::string& type_name,
                           const parser::ast::expression& body);

    // Load built-in type kinds (class, struct, enum, trait, effect)
    void load_builtins();

private:
    std::unordered_map<std::string, TypeKind> kinds_;
};

} // namespace meld::mms
