#pragma once

/// @file safe_return_pass.hpp
/// @brief Semantic Analyzer — Safe-Return Operator Pass
///
/// Validates usage of the postfix `?` (safe-return) operator from the
/// Control Flow Quintet. Enforces:
///   - Operator targets `optional[T]` only; rejects `Result[T, E]` (E4703)
///   - Enclosing function return type must be compatible with `optional[T]` (E4704)
///   - Operator is non-overloadable; rejects `opr ?` definitions (E4702)
///   - Function-level return semantics: if value is nil, immediately
///     return nil from the enclosing function (NOT expression-level)
///
/// Requirements: 14C.12, 14C.13, 14C.14, 24C.20

#include "meld/parser/ast.hpp"
#include <string>
#include <vector>
#include <unordered_set>

namespace meld::compiler {

// ---------------------------------------------------------------------------
// Safe-return diagnostic
// ---------------------------------------------------------------------------

/// Diagnostic produced by the safe-return pass.
struct SafeReturnDiagnostic {
    enum class Level { Info, Warning, Error };

    Level level;
    std::string code;       ///< e.g., "E4702", "E4703", "E4704"
    std::string message;
    std::string source_file;
    size_t line = 0;
    size_t column = 0;
};

// ---------------------------------------------------------------------------
// SafeReturnResult — output of the pass
// ---------------------------------------------------------------------------

struct SafeReturnResult {
    bool success = true;
    std::vector<SafeReturnDiagnostic> diagnostics;
    size_t safe_returns_checked = 0;
    size_t operator_defs_checked = 0;
    size_t errors_emitted = 0;
};

// ---------------------------------------------------------------------------
// SafeReturnPass — the Semantic Analyzer pass
// ---------------------------------------------------------------------------

/// The Safe-Return Pass validates all uses of the postfix `?` operator
/// in the AST. It enforces:
///
///   1. The operand of `?` must be typed `optional[T]`. If the operand
///      is `Result[T, E]`, the pass emits E4703 directing the developer
///      to use `?!` instead.
///
///   2. The enclosing function's return type must be compatible with
///      `optional[T]`. If not, the pass emits E4704.
///
///   3. No `opr` (operator function / custom operator) definition may
///      redefine `?`. If found, the pass emits E4702.
///
///   4. Function-level return semantics: when the operand is nil, the
///      enclosing function immediately returns nil (NOT expression-level
///      short-circuit like `?.`).
///
/// The pass operates on parsed AST expressions. The postfix `?` is
/// represented as a `unary_operation` with op "?".
class SafeReturnPass {
public:
    SafeReturnPass();

    /// Run the pass over a set of parsed expressions.
    /// @param expressions  The module-level AST expressions.
    /// @param source_file  Source file path for diagnostics.
    SafeReturnResult run(
        const std::vector<parser::ast::expression>& expressions,
        const std::string& source_file = ""
    );

    /// Check if a type annotation represents an optional type.
    static bool is_optional_type(const parser::ast::type_annotation& type);

    /// Check if a type annotation represents a Result type.
    static bool is_result_type(const parser::ast::type_annotation& type);

    /// The set of non-overloadable safe-return operator symbols.
    static const std::unordered_set<std::string>& forbidden_operator_symbols();

    /// Emit E4703: safe-return operator `?` used on non-optional type.
    void emit_non_optional_error(
        const std::string& type_name,
        const std::string& source_file,
        size_t line, size_t column,
        SafeReturnResult& result
    );

    /// Emit E4704: enclosing function return type not compatible with optional[T].
    void emit_incompatible_return_type_error(
        const std::string& return_type_name,
        const std::string& source_file,
        size_t line, size_t column,
        SafeReturnResult& result
    );

    /// Emit E4702: attempt to overload a non-overloadable operator.
    void emit_forbidden_overload_error(
        const std::string& operator_symbol,
        const std::string& source_file,
        size_t line, size_t column,
        SafeReturnResult& result
    );

private:
    /// Scan an expression for safe-return usage and operator definitions.
    /// @param in_function  Whether we are inside a function body.
    /// @param func_return_type  The return type of the enclosing function (if any).
    void scan_expression(
        const parser::ast::expression& expr,
        const std::string& source_file,
        SafeReturnResult& result,
        bool in_function = false,
        const parser::ast::type_annotation* func_return_type = nullptr
    );

    /// Check a unary_operation for postfix `?` usage.
    void check_safe_return(
        const parser::ast::unary_operation& unary,
        const std::string& source_file,
        SafeReturnResult& result,
        bool in_function,
        const parser::ast::type_annotation* func_return_type
    );

    /// Check an operator_function definition for forbidden symbols.
    void check_operator_definition(
        const parser::ast::operator_function& op_func,
        const std::string& source_file,
        SafeReturnResult& result
    );

    /// Check a custom_operator_definition for forbidden symbols.
    void check_custom_operator_definition(
        const parser::ast::custom_operator_definition& op_def,
        const std::string& source_file,
        SafeReturnResult& result
    );

    /// Recursively scan a block for safe-return usage.
    void scan_block(
        const std::vector<boost::spirit::x3::forward_ast<parser::ast::expression>>& statements,
        const std::string& source_file,
        SafeReturnResult& result,
        bool in_function = false,
        const parser::ast::type_annotation* func_return_type = nullptr
    );
};

} // namespace meld::compiler
