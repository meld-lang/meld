#pragma once

/// @file force_unwrap_pass.hpp
/// @brief Semantic Analyzer — Force-Unwrap / Panic Operator Pass
///
/// Validates usage of the postfix `!!` (force-unwrap) operator from the
/// Control Flow Quintet. Enforces:
///   - Operator targets BOTH `optional[T]` and `Result[T, E]`
///   - Warns (W4707) if used on a type that is neither optional nor Result
///   - Operator is non-overloadable; rejects `opr !!` definitions (E4702)
///   - Panic semantics: if `nil` or `Err`, crash the current Fiber/Actor
///   - Allowed at top level (no function context required, unlike `?` and `?!`)
///
/// Requirements: 14E.18, 14E.19, 24C.23

#include "meld/parser/ast.hpp"
#include <string>
#include <vector>
#include <unordered_set>

namespace meld::compiler {

// ---------------------------------------------------------------------------
// Force-unwrap diagnostic
// ---------------------------------------------------------------------------

/// Diagnostic produced by the force-unwrap pass.
struct ForceUnwrapDiagnostic {
    enum class Level { Info, Warning, Error };

    Level level;
    std::string code;       ///< e.g., "E4702", "W4707"
    std::string message;
    std::string source_file;
    size_t line = 0;
    size_t column = 0;
};

// ---------------------------------------------------------------------------
// ForceUnwrapResult — output of the pass
// ---------------------------------------------------------------------------

struct ForceUnwrapResult {
    bool success = true;
    std::vector<ForceUnwrapDiagnostic> diagnostics;
    size_t force_unwraps_checked = 0;
    size_t operator_defs_checked = 0;
    size_t errors_emitted = 0;
    size_t warnings_emitted = 0;
};

// ---------------------------------------------------------------------------
// ForceUnwrapPass — the Semantic Analyzer pass
// ---------------------------------------------------------------------------

/// The Force-Unwrap Pass validates all uses of the postfix `!!` operator
/// in the AST. It enforces:
///
///   1. The operand of `!!` should be typed `optional[T]` or `Result[T, E]`.
///      If neither, the pass emits W4707 (warning, not error).
///
///   2. No function context is required — `!!` is allowed at top level.
///
///   3. No `opr` (operator function / custom operator) definition may
///      redefine `!!`. If found, the pass emits E4702.
///
///   4. Panic semantics: when the value is `nil` (optional) or `Err`
///      (Result), the current Fiber/Actor crashes.
///
/// The pass operates on parsed AST expressions. The postfix `!!` is
/// represented as a `unary_operation` with op "!!".
class ForceUnwrapPass {
public:
    ForceUnwrapPass();

    /// Run the pass over a set of parsed expressions.
    /// @param expressions  The module-level AST expressions.
    /// @param source_file  Source file path for diagnostics.
    ForceUnwrapResult run(
        const std::vector<parser::ast::expression>& expressions,
        const std::string& source_file = ""
    );

    /// Check if a type annotation represents an optional type.
    static bool is_optional_type(const parser::ast::type_annotation& type);

    /// Check if a type annotation represents a Result type.
    static bool is_result_type(const parser::ast::type_annotation& type);

    /// The set of non-overloadable force-unwrap operator symbols.
    static const std::unordered_set<std::string>& forbidden_operator_symbols();

    /// Emit W4707: force-unwrap `!!` used on type that is neither optional nor Result.
    void emit_unnecessary_force_unwrap_warning(
        const std::string& type_name,
        const std::string& source_file,
        size_t line, size_t column,
        ForceUnwrapResult& result
    );

    /// Emit E4702: attempt to overload a non-overloadable operator.
    void emit_forbidden_overload_error(
        const std::string& operator_symbol,
        const std::string& source_file,
        size_t line, size_t column,
        ForceUnwrapResult& result
    );

private:
    /// Scan an expression for force-unwrap usage and operator definitions.
    /// @param in_function  Whether we are inside a function body.
    void scan_expression(
        const parser::ast::expression& expr,
        const std::string& source_file,
        ForceUnwrapResult& result,
        bool in_function = false
    );

    /// Check a unary_operation for postfix `!!` usage.
    void check_force_unwrap(
        const parser::ast::unary_operation& unary,
        const std::string& source_file,
        ForceUnwrapResult& result,
        bool in_function
    );

    /// Check an operator_function definition for forbidden symbols.
    void check_operator_definition(
        const parser::ast::operator_function& op_func,
        const std::string& source_file,
        ForceUnwrapResult& result
    );

    /// Check a custom_operator_definition for forbidden symbols.
    void check_custom_operator_definition(
        const parser::ast::custom_operator_definition& op_def,
        const std::string& source_file,
        ForceUnwrapResult& result
    );

    /// Recursively scan a block for force-unwrap usage.
    void scan_block(
        const std::vector<boost::spirit::x3::forward_ast<parser::ast::expression>>& statements,
        const std::string& source_file,
        ForceUnwrapResult& result,
        bool in_function = false
    );
};

} // namespace meld::compiler
