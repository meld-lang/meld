#pragma once

/// @file container_constraint_pass.hpp
/// @brief Semantic Analyzer — Container Constraint Check Pass
///
/// For every generic type instantiation `Container[T]`, checks if
/// `Container` has the `@intrinsic(managed_container)` annotation.
/// If so, verifies that `T` implements the trait annotated with
/// `@intrinsic(memory_strategy)`. If `T` is a raw class type that
/// does not implement Storable, emits E4003 with a fix-it suggesting
/// `Hold[T]` or `View[T]`.
///
/// Requirements: 5.3, 5.4, 5.5, 7.3

#include "meld/parser/ast.hpp"
#include "meld/compiler/intrinsic_resolution_pass.hpp"
#include <string>
#include <vector>
#include <unordered_set>

namespace meld::compiler {

// ---------------------------------------------------------------------------
// Container constraint diagnostic
// ---------------------------------------------------------------------------

/// Diagnostic produced by the container constraint check pass.
struct ContainerConstraintDiagnostic {
    enum class Level { Info, Warning, Error };

    Level level;
    std::string code;       ///< e.g., "E4003"
    std::string message;
    std::string source_file;
    size_t line = 0;
    size_t column = 0;
};

// ---------------------------------------------------------------------------
// ContainerConstraintResult — output of the pass
// ---------------------------------------------------------------------------

struct ContainerConstraintResult {
    bool success = true;
    std::vector<ContainerConstraintDiagnostic> diagnostics;
    size_t container_types_checked = 0;
    size_t raw_type_errors = 0;
};

// ---------------------------------------------------------------------------
// ContainerConstraintPass — the Semantic Analyzer pass
// ---------------------------------------------------------------------------

/// The Container Constraint Check Pass scans all type annotations for
/// generic type instantiations where the container is annotated with
/// `@intrinsic(managed_container)`. For each such instantiation, it
/// verifies that the element type implements the `@intrinsic(memory_strategy)`
/// trait (i.e., is `Hold[T]` or `View[T]`). Raw class types trigger E4003.
///
/// This pass runs after the View Access Enforcement Pass and receives
/// the IntrinsicResolutionRegistry.
class ContainerConstraintPass {
public:
    ContainerConstraintPass();

    /// Run the pass over a set of parsed expressions.
    /// @param expressions  The module-level AST expressions.
    /// @param registry     The intrinsic registry (from IntrinsicResolutionPass).
    /// @param source_file  Source file path for diagnostics.
    ContainerConstraintResult run(
        const std::vector<parser::ast::expression>& expressions,
        const IntrinsicResolutionRegistry& registry,
        const std::string& source_file = ""
    );

    /// Check if a type name refers to a managed container.
    static bool is_managed_container(
        const std::string& type_name,
        const IntrinsicResolutionRegistry& registry
    );

    /// Check if a type name refers to a known Storable implementor
    /// (Hold, View, or their qualified variants).
    static bool is_storable_type(const std::string& type_name);
    static bool is_exempt_from_container_check(const std::string& type_name);

private:
    /// Known Storable-implementing type names.
    static const std::unordered_set<std::string>& storable_type_names();

    /// Scan a single type annotation for managed container violations.
    void check_type_annotation(
        const parser::ast::type_annotation& type,
        const IntrinsicResolutionRegistry& registry,
        const std::string& source_file,
        ContainerConstraintResult& result
    );

    /// Scan a function definition for type annotations.
    void scan_function(
        const parser::ast::function_definition& func,
        const IntrinsicResolutionRegistry& registry,
        const std::string& source_file,
        ContainerConstraintResult& result
    );

    /// Scan a class definition for field type annotations.
    void scan_class(
        const parser::ast::class_definition& class_def,
        const IntrinsicResolutionRegistry& registry,
        const std::string& source_file,
        ContainerConstraintResult& result
    );

    /// Scan a struct definition for field type annotations.
    void scan_struct(
        const parser::ast::struct_definition& struct_def,
        const IntrinsicResolutionRegistry& registry,
        const std::string& source_file,
        ContainerConstraintResult& result
    );

    /// Scan an expression tree for type annotations (val/var declarations, etc.).
    void scan_expression(
        const parser::ast::expression& expr,
        const IntrinsicResolutionRegistry& registry,
        const std::string& source_file,
        ContainerConstraintResult& result
    );

    /// Emit an E4003 diagnostic.
    void emit_raw_type_in_container(
        const std::string& raw_type_name,
        const std::string& container_name,
        const std::string& source_file,
        size_t line,
        size_t column,
        ContainerConstraintResult& result
    );
};

} // namespace meld::compiler
