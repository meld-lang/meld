#pragma once

/// @file cycle_detection_pass.hpp
/// @brief Semantic Analyzer — Cycle Detection Pass
///
/// For every pair of class declarations A and B, checks if A has a field
/// of type Hold[B] and B has a field of type Hold[A]. If so, emits warning
/// W4001: "potential reference cycle: {A} and {B} have mutual Hold[T]
/// references". This is a simple two-hop analysis; deeper cycle detection
/// (A → B → C → A) is deferred to a future enhancement.
///
/// Requirements: 7.4, 10.4

#include "meld/parser/ast.hpp"
#include "meld/compiler/intrinsic_resolution_pass.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace meld::compiler {

// ---------------------------------------------------------------------------
// Cycle detection diagnostic
// ---------------------------------------------------------------------------

/// Diagnostic produced by the cycle detection pass.
struct CycleDetectionDiagnostic {
    enum class Level { Info, Warning, Error };

    Level level;
    std::string code;       ///< e.g., "W4001"
    std::string message;
    std::string source_file;
    size_t line = 0;
    size_t column = 0;
};

// ---------------------------------------------------------------------------
// CycleDetectionResult — output of the pass
// ---------------------------------------------------------------------------

struct CycleDetectionResult {
    bool success = true;  ///< Always true — warnings don't block compilation.
    std::vector<CycleDetectionDiagnostic> diagnostics;
    size_t classes_scanned = 0;
    size_t mutual_cycles_detected = 0;
};

// ---------------------------------------------------------------------------
// CycleDetectionPass — the Semantic Analyzer pass
// ---------------------------------------------------------------------------

/// The Cycle Detection Pass scans all class definitions for mutual
/// Hold[T] references. For every pair (A, B) where A has a Hold[B]
/// field and B has a Hold[A] field, it emits W4001.
///
/// This pass runs after the Container Constraint Check Pass. It only
/// needs the AST (class definitions with fields) — it uses the same
/// is_storable_type pattern from ContainerConstraintPass to identify
/// Hold types specifically.
class CycleDetectionPass {
public:
    CycleDetectionPass();

    /// Run the pass over a set of parsed expressions.
    /// @param expressions  The module-level AST expressions.
    /// @param source_file  Source file path for diagnostics.
    CycleDetectionResult run(
        const std::vector<parser::ast::expression>& expressions,
        const std::string& source_file = ""
    );

    /// Extract the set of Hold[T] target class names from a class's fields.
    /// For a field typed Hold[X] or std.mem.Hold[X], returns "X".
    static std::unordered_set<std::string> extract_own_targets(
        const std::vector<parser::ast::field_declaration>& fields
    );

    /// Check if a type annotation represents a Hold[T] wrapper.
    /// Returns the inner type name if it is Hold[T], or empty string otherwise.
    static std::string extract_own_inner_type(
        const parser::ast::type_annotation& type
    );

private:
    /// Known Hold type names (unqualified and qualified variants).
    static const std::unordered_set<std::string>& own_type_names();

    /// Collect all class definitions from the AST into a map: name → fields.
    void collect_class_definitions(
        const std::vector<parser::ast::expression>& expressions,
        std::unordered_map<std::string, const parser::ast::class_definition*>& classes
    );

    /// Emit a W4001 diagnostic for a mutual Hold[T] cycle.
    void emit_mutual_cycle_warning(
        const std::string& class_a,
        const std::string& class_b,
        const std::string& source_file,
        CycleDetectionResult& result
    );
};

} // namespace meld::compiler
