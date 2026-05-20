#pragma once

/// @file intrinsic_resolution_pass.hpp
/// @brief Semantic Analyzer — Intrinsic Resolution Pass
///
/// Scans all trait and class declarations for @intrinsic annotations during
/// module loading. Builds a registry mapping intrinsic names to their
/// annotated types. This pass runs once during module loading and the
/// registry is consulted by all subsequent passes (Move Tracking, View
/// Access, Container Constraint, Cycle Detection).
///
/// Requirements: 5.2, 5.3

#include "meld/parser/ast.hpp"
#include "meld/std/mem.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <expected>

namespace meld::compiler {

// ---------------------------------------------------------------------------
// Intrinsic annotation entry — represents a resolved @intrinsic mapping
// ---------------------------------------------------------------------------

/// The kind of entity annotated with @intrinsic.
enum class IntrinsicEntityKind {
    Trait,       // e.g., Storable trait
    Class,       // e.g., Hold[T], View[T]
    Function,    // e.g., std.mem.move()
    Container    // e.g., vec, dict, set
};

/// A single resolved intrinsic annotation entry.
struct IntrinsicEntry {
    /// The intrinsic tag name (e.g., "memory_strategy", "managed_container").
    std::string intrinsic_name;

    /// The fully-qualified name of the annotated entity.
    std::string entity_name;

    /// What kind of entity carries the annotation.
    IntrinsicEntityKind entity_kind;

    /// Source location for diagnostics.
    std::string source_file;
    size_t line = 0;
    size_t column = 0;
};

// ---------------------------------------------------------------------------
// IntrinsicResolutionRegistry — the output of the Intrinsic Resolution Pass
// ---------------------------------------------------------------------------

/// Registry mapping intrinsic annotation names to their resolved entities.
/// Built once during module loading by the IntrinsicResolutionPass and
/// consulted by Move Tracking, View Access, Container Constraint, and
/// Cycle Detection passes.
class IntrinsicResolutionRegistry {
public:
    IntrinsicResolutionRegistry() = default;

    /// Register an intrinsic mapping.
    void register_entry(const IntrinsicEntry& entry);

    /// Look up all entries for a given intrinsic name.
    std::vector<const IntrinsicEntry*> lookup(const std::string& intrinsic_name) const;

    /// Look up the first (primary) entry for a given intrinsic name.
    const IntrinsicEntry* lookup_primary(const std::string& intrinsic_name) const;

    /// Check if an intrinsic name has been registered.
    bool has(const std::string& intrinsic_name) const;

    /// Get the entity name for a given intrinsic (convenience for single-entry intrinsics).
    std::optional<std::string> entity_for(const std::string& intrinsic_name) const;

    /// Get all registered entries.
    const std::vector<IntrinsicEntry>& all_entries() const { return entries_; }

    /// Get the number of registered entries.
    size_t size() const { return entries_.size(); }

    /// Clear all registrations (for testing).
    void clear();

    /// Check if the memory_strategy intrinsic is registered.
    bool has_memory_strategy() const { return has(std_mem::IntrinsicTag::memory_strategy); }

    /// Check if the managed_container intrinsic is registered.
    bool has_managed_container() const { return has(std_mem::IntrinsicTag::managed_container); }

    /// Check if the memory_move intrinsic is registered.
    bool has_memory_move() const { return has(std_mem::IntrinsicTag::memory_move); }

private:
    /// All registered entries.
    std::vector<IntrinsicEntry> entries_;

    /// Index: intrinsic_name → indices into entries_.
    std::unordered_map<std::string, std::vector<size_t>> index_;
};

// ---------------------------------------------------------------------------
// IntrinsicResolutionPass — the Semantic Analyzer pass
// ---------------------------------------------------------------------------

/// Diagnostic produced by the intrinsic resolution pass.
struct IntrinsicDiagnostic {
    enum class Level { Info, Warning, Error };
    Level level;
    std::string message;
    std::string source_file;
    size_t line = 0;
    size_t column = 0;
};

/// Result of running the intrinsic resolution pass.
struct IntrinsicResolutionResult {
    bool success = true;
    IntrinsicResolutionRegistry registry;
    std::vector<IntrinsicDiagnostic> diagnostics;
};

/// The Intrinsic Resolution Pass scans all trait and class declarations
/// for @intrinsic annotations. It builds a registry mapping intrinsic
/// names to their annotated types.
///
/// This pass runs once during module loading. The registry is consulted
/// by all subsequent passes:
///   - Move Tracking Pass (checks memory_move)
///   - View Access Enforcement Pass (checks memory_strategy / is_owning)
///   - Container Constraint Check Pass (checks managed_container)
///   - Cycle Detection Pass (checks memory_strategy for Hold[T] detection)
class IntrinsicResolutionPass {
public:
    IntrinsicResolutionPass();

    /// Run the pass over a set of parsed expressions (module-level AST).
    /// Scans for @intrinsic annotations on trait and class declarations
    /// and populates the registry.
    IntrinsicResolutionResult run(
        const std::vector<parser::ast::expression>& expressions,
        const std::string& source_file = ""
    );

    /// Register the std.mem module's built-in intrinsics.
    /// Called to seed the registry with the standard library mappings:
    ///   memory_strategy  → Storable
    ///   managed_container → vec, dict, set
    ///   memory_move      → std.mem.move
    void register_std_mem_intrinsics(IntrinsicResolutionRegistry& registry);

    /// Get the last run's registry (convenience accessor).
    const IntrinsicResolutionRegistry& registry() const { return last_registry_; }

private:
    IntrinsicResolutionRegistry last_registry_;

    /// Scan a class definition for @intrinsic annotations.
    void scan_class_definition(
        const parser::ast::class_definition& class_def,
        const std::string& source_file,
        IntrinsicResolutionRegistry& registry,
        std::vector<IntrinsicDiagnostic>& diagnostics
    );

    /// Scan a function definition for @intrinsic annotations.
    void scan_function_definition(
        const parser::ast::function_definition& func_def,
        const std::string& source_file,
        IntrinsicResolutionRegistry& registry,
        std::vector<IntrinsicDiagnostic>& diagnostics
    );

    /// Scan a struct definition for @intrinsic annotations.
    void scan_struct_definition(
        const parser::ast::struct_definition& def,
        const std::string& source_file,
        IntrinsicResolutionRegistry& registry,
        std::vector<IntrinsicDiagnostic>& diagnostics
    );

    /// Extract @intrinsic(name) annotation value from an identifier name.
    /// Returns the intrinsic tag if the name follows the convention, or
    /// nullopt if no intrinsic annotation is detected.
    ///
    /// The Meld parser does not yet have a dedicated annotation AST node,
    /// so intrinsic annotations are detected by convention:
    ///   - A class/trait named with a known Storable-implementing type
    ///     that is registered in std.mem
    ///   - Explicit intrinsic metadata attached to the AST node (future)
    static std::optional<std::string> extract_intrinsic_annotation(
        const std::string& entity_name,
        const std::vector<std::string>& annotations = {}
    );

    /// Check if a name matches a known intrinsic tag.
    static bool is_known_intrinsic_tag(const std::string& tag);
};

} // namespace meld::compiler
