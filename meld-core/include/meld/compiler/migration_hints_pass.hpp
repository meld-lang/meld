#pragma once

/// @file migration_hints_pass.hpp
/// @brief Semantic Analyzer — Migration Hints Pass
///
/// Emits info-level diagnostics when the AST contains references to
/// old (pre-rename) type or function names:
///   I4010: Own[T]  → Hold[T]
///   I4011: Link[T] → View[T]
///   I4012: link()  → view()
///   I4013: Dict    → Map
///   I4014: Deque   → Queue
///   I4015: vec     → List
///
/// This pass never blocks compilation — it only produces informational
/// hints to guide developers through the rename migration.
///
/// Requirements: 119.7, 120.8, 166.3, 166.4

#include "meld/parser/ast.hpp"
#include "meld/compiler/intrinsic_resolution_pass.hpp"
#include <string>
#include <vector>
#include <unordered_set>

namespace meld::compiler {

// ---------------------------------------------------------------------------
// Migration hint diagnostic
// ---------------------------------------------------------------------------

/// Diagnostic produced by the migration hints pass.
struct MigrationHintDiagnostic {
    enum class Level { Info, Warning, Error };

    Level level;
    std::string code;       ///< e.g., "I4010", "I4011", "I4012"
    std::string message;
    std::string source_file;
    size_t line = 0;
    size_t column = 0;
};

// ---------------------------------------------------------------------------
// MigrationHintsResult — output of the pass
// ---------------------------------------------------------------------------

struct MigrationHintsResult {
    bool success = true;  ///< Always true — hints never block compilation.
    std::vector<MigrationHintDiagnostic> diagnostics;
    size_t own_hints = 0;   ///< Count of Own → Hold hints emitted.
    size_t link_hints = 0;  ///< Count of Link → View hints emitted.
    size_t link_call_hints = 0;  ///< Count of link() → view() hints emitted.
    size_t dict_hints = 0;  ///< Count of Dict → Map hints emitted.
    size_t deque_hints = 0; ///< Count of Deque → Queue hints emitted.
    size_t vec_hints = 0;   ///< Count of vec → List hints emitted.
};

// ---------------------------------------------------------------------------
// MigrationHintsPass — the Semantic Analyzer pass
// ---------------------------------------------------------------------------

/// The Migration Hints Pass scans all type annotations and function
/// calls in the AST for old names (Own, Link, link) and emits
/// informational diagnostics pointing to the new names (Hold, View, view).
///
/// This pass can run at any point in the pipeline. It only emits
/// info-level diagnostics (I4010, I4011, I4012) — never errors.
class MigrationHintsPass {
public:
    MigrationHintsPass();

    /// Run the pass over a set of parsed expressions.
    /// @param expressions  The module-level AST expressions.
    /// @param source_file  Source file path for diagnostics.
    MigrationHintsResult run(
        const std::vector<parser::ast::expression>& expressions,
        const std::string& source_file = ""
    );

    /// Old Own type names that should trigger I4010.
    static const std::unordered_set<std::string>& old_own_type_names();

    /// Old Link type names that should trigger I4011.
    static const std::unordered_set<std::string>& old_link_type_names();

    /// Old link() function names that should trigger I4012.
    static const std::unordered_set<std::string>& old_link_call_names();

    /// Old Dict type names that should trigger I4013.
    static const std::unordered_set<std::string>& old_dict_type_names();

    /// Old Deque type names that should trigger I4014.
    static const std::unordered_set<std::string>& old_deque_type_names();

    /// Old vec type names that should trigger I4015.
    static const std::unordered_set<std::string>& old_vec_type_names();

private:
    /// Check a single type annotation for old names.
    void check_type_annotation(
        const parser::ast::type_annotation& type,
        const std::string& source_file,
        MigrationHintsResult& result
    );

    /// Scan a function definition for old names.
    void scan_function(
        const parser::ast::function_definition& func,
        const std::string& source_file,
        MigrationHintsResult& result
    );

    /// Scan a class definition for old names in field types.
    void scan_class(
        const parser::ast::class_definition& cls,
        const std::string& source_file,
        MigrationHintsResult& result
    );

    /// Scan a struct definition for old names in field types.
    void scan_struct(
        const parser::ast::struct_definition& s,
        const std::string& source_file,
        MigrationHintsResult& result
    );

    /// Scan a block of statements.
    void scan_block(
        const std::vector<boost::spirit::x3::forward_ast<parser::ast::expression>>& statements,
        const std::string& source_file,
        MigrationHintsResult& result
    );

    /// Scan a single expression.
    void scan_expression(
        const parser::ast::expression& expr,
        const std::string& source_file,
        MigrationHintsResult& result
    );
};

} // namespace meld::compiler
