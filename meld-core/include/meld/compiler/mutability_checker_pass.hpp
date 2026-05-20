#pragma once

/// @file mutability_checker_pass.hpp
/// @brief Semantic Analyzer — Mutability Checker Pass
///
/// Enforces `var fnc` declarations for methods that mutate `this`.
/// Scans method bodies inside class/struct definitions to detect
/// mutations of the receiver (assignments to `this.field`, calls to
/// `var fnc` methods on `this`). Emits errors when:
///   - A method mutates `this` without being declared `var fnc` (E5001)
///   - A method is declared `var fnc` but never mutates `this` (W5001)
///
/// Requirements: 57.2, 57.3

#include "meld/parser/ast.hpp"
#include <string>
#include <vector>

namespace meld::compiler {

// ---------------------------------------------------------------------------
// Mutability checker diagnostic
// ---------------------------------------------------------------------------

/// Diagnostic produced by the mutability checker pass.
struct MutabilityDiagnostic {
    enum class Level { Info, Warning, Error };

    Level level;
    std::string code;       ///< e.g., "E5001" or "W5001"
    std::string message;
    std::string source_file;
    size_t line = 0;
    size_t column = 0;
};

// ---------------------------------------------------------------------------
// MutabilityCheckerResult — output of the pass
// ---------------------------------------------------------------------------

struct MutabilityCheckerResult {
    bool success = true;
    std::vector<MutabilityDiagnostic> diagnostics;
    size_t methods_checked = 0;
    size_t mutations_detected = 0;
    size_t errors_emitted = 0;
    size_t warnings_emitted = 0;
};

// ---------------------------------------------------------------------------
// MutabilityCheckerPass — the Semantic Analyzer pass
// ---------------------------------------------------------------------------

/// The Mutability Checker Pass scans class/struct method bodies for
/// mutations of `this` (the receiver). It enforces that:
///   1. Methods mutating `this` must be declared with `var fnc`
///   2. Methods declared `var fnc` must actually mutate `this`
///
/// Mutation of `this` is detected by:
///   - Assignment to `this.field` (binary_operation "=" where LHS is
///     a dot-access on `this`)
///   - Calling a `var fnc` method on `this` (function_call where the
///     receiver is `this` and the callee is known to be `var fnc`)
///
/// This pass runs after parsing and operates on the methods vector
/// of class_definition and struct_definition nodes.
class MutabilityCheckerPass {
public:
    MutabilityCheckerPass();

    /// Run the pass over a set of parsed expressions.
    /// @param expressions  The module-level AST expressions.
    /// @param source_file  Source file path for diagnostics.
    MutabilityCheckerResult run(
        const std::vector<parser::ast::expression>& expressions,
        const std::string& source_file = ""
    );

    /// Check if an expression is a mutation of `this`.
    /// Returns true if the expression assigns to `this.field` or calls
    /// a `var fnc` method on `this`.
    static bool is_this_mutation(
        const parser::ast::expression& expr,
        const std::vector<parser::ast::function_definition>& sibling_methods
    );

    /// Check if a binary operation is an assignment to `this.field`.
    static bool is_this_field_assignment(
        const parser::ast::binary_operation& binop
    );

    /// Check if a function call is a `var fnc` method call on `this`.
    static bool is_this_var_fnc_call(
        const parser::ast::function_call& call,
        const std::vector<parser::ast::function_definition>& sibling_methods
    );

private:
    /// Analyze all methods in a class definition.
    void analyze_class(
        const parser::ast::class_definition& class_def,
        const std::string& source_file,
        MutabilityCheckerResult& result
    );

    /// Analyze all methods in a struct definition.
    void analyze_struct(
        const parser::ast::struct_definition& struct_def,
        const std::string& source_file,
        MutabilityCheckerResult& result
    );

    /// Analyze a single method for `this` mutations.
    /// @param method          The method to analyze.
    /// @param sibling_methods All methods in the enclosing type (for var fnc lookup).
    /// @param type_name       Name of the enclosing class/struct (for diagnostics).
    /// @param source_file     Source file path for diagnostics.
    /// @param result          Accumulates diagnostics.
    void analyze_method(
        const parser::ast::function_definition& method,
        const std::vector<parser::ast::function_definition>& sibling_methods,
        const std::string& type_name,
        const std::string& source_file,
        MutabilityCheckerResult& result
    );

    /// Recursively scan an expression for `this` mutations.
    /// Returns true if any mutation of `this` was found.
    bool scan_for_this_mutation(
        const parser::ast::expression& expr,
        const std::vector<parser::ast::function_definition>& sibling_methods
    );

    /// Scan a block for `this` mutations.
    bool scan_block_for_this_mutation(
        const std::vector<boost::spirit::x3::forward_ast<parser::ast::expression>>& statements,
        const std::vector<parser::ast::function_definition>& sibling_methods
    );

    /// Emit E5001: method mutates `this` without `var fnc`.
    void emit_missing_var_fnc(
        const std::string& method_name,
        const std::string& type_name,
        const std::string& source_file,
        size_t line,
        size_t column,
        MutabilityCheckerResult& result
    );

    /// Emit W5001: method declared `var fnc` but doesn't mutate `this`.
    void emit_unnecessary_var_fnc(
        const std::string& method_name,
        const std::string& type_name,
        const std::string& source_file,
        size_t line,
        size_t column,
        MutabilityCheckerResult& result
    );
};

} // namespace meld::compiler
