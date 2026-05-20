#pragma once

/// @file advisory_diagnostics_pass.hpp
/// @brief Semantic Analyzer — Advisory Diagnostics Pass
///
/// Emits non-fatal advisory diagnostics:
///   W4002: "unnecessary retain/release — consider std.mem.move()"
///          when a Hold[T] assignment could use move to elide ARC ops.
///   I4001: "consider View[T] instead of Hold[T]"
///          when a back-reference pattern is detected (e.g., a field
///          named "parent" pointing back to the containing type).
///
/// This pass runs after the Cycle Detection Pass. It only produces
/// warnings and info messages — it never blocks compilation.
///
/// Requirements: 7.5, 7.6

#include "meld/parser/ast.hpp"
#include "meld/compiler/intrinsic_resolution_pass.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace meld::compiler {

// ---------------------------------------------------------------------------
// Advisory diagnostic
// ---------------------------------------------------------------------------

/// Diagnostic produced by the advisory diagnostics pass.
struct AdvisoryDiagnostic {
    enum class Level { Info, Warning, Error };

    Level level;
    std::string code;       ///< e.g., "W4002", "I4001"
    std::string message;
    std::string source_file;
    size_t line = 0;
    size_t column = 0;
};

// ---------------------------------------------------------------------------
// AdvisoryDiagnosticsResult — output of the pass
// ---------------------------------------------------------------------------

struct AdvisoryDiagnosticsResult {
    bool success = true;  ///< Always true — advisories never block compilation.
    std::vector<AdvisoryDiagnostic> diagnostics;
    size_t move_suggestions = 0;
    size_t back_reference_suggestions = 0;
};

// ---------------------------------------------------------------------------
// AdvisoryDiagnosticsPass — the Semantic Analyzer pass
// ---------------------------------------------------------------------------

/// The Advisory Diagnostics Pass scans function bodies for Hold[T]
/// assignments that could benefit from std.mem.move(), and class
/// definitions for back-reference fields that should use View[T].
///
/// This pass runs after the Cycle Detection Pass. It only emits
/// warnings (W4002) and info (I4001) — never errors.
class AdvisoryDiagnosticsPass {
public:
    AdvisoryDiagnosticsPass();

    /// Run the pass over a set of parsed expressions.
    /// @param expressions  The module-level AST expressions.
    /// @param registry     The intrinsic registry (from IntrinsicResolutionPass).
    /// @param source_file  Source file path for diagnostics.
    AdvisoryDiagnosticsResult run(
        const std::vector<parser::ast::expression>& expressions,
        const IntrinsicResolutionRegistry& registry,
        const std::string& source_file = ""
    );

    /// Known Hold type names (unqualified and qualified variants).
    static const std::unordered_set<std::string>& own_type_names();

    /// Known back-reference field name patterns (e.g., "parent", "owner").
    static const std::unordered_set<std::string>& back_reference_field_names();

    /// Extract the inner type name from a Hold[T] type annotation.
    /// Returns empty string if not a Hold[T] type.
    static std::string extract_own_inner_type(
        const parser::ast::type_annotation& type
    );

private:
    /// Scan function bodies for Hold[T] assignments that could use move.
    void scan_function_for_move_suggestions(
        const parser::ast::function_definition& func,
        const IntrinsicResolutionRegistry& registry,
        const std::string& source_file,
        AdvisoryDiagnosticsResult& result
    );

    /// Scan a block of statements for move suggestions.
    void scan_block_for_move_suggestions(
        const std::vector<boost::spirit::x3::forward_ast<parser::ast::expression>>& statements,
        const IntrinsicResolutionRegistry& registry,
        const std::string& source_file,
        AdvisoryDiagnosticsResult& result
    );

    /// Scan an expression for Hold[T] assignments that could use move.
    void scan_expression_for_move_suggestions(
        const parser::ast::expression& expr,
        const IntrinsicResolutionRegistry& registry,
        const std::string& source_file,
        AdvisoryDiagnosticsResult& result
    );

    /// Scan class definitions for back-reference patterns.
    void scan_class_for_back_references(
        const parser::ast::class_definition& cls,
        const std::unordered_map<std::string, const parser::ast::class_definition*>& all_classes,
        const std::string& source_file,
        AdvisoryDiagnosticsResult& result
    );

    /// Collect all class definitions from the AST.
    void collect_class_definitions(
        const std::vector<parser::ast::expression>& expressions,
        std::unordered_map<std::string, const parser::ast::class_definition*>& classes
    );

    /// Emit a W4002 diagnostic.
    void emit_move_suggestion(
        const std::string& binding_name,
        const std::string& source_file,
        size_t line,
        size_t column,
        AdvisoryDiagnosticsResult& result
    );

    /// Emit an I4001 diagnostic.
    void emit_back_reference_suggestion(
        const std::string& field_name,
        const std::string& class_name,
        const std::string& target_class,
        const std::string& source_file,
        size_t line,
        size_t column,
        AdvisoryDiagnosticsResult& result
    );
};

} // namespace meld::compiler
