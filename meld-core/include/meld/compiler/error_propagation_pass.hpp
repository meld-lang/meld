#pragma once

/// @file error_propagation_pass.hpp
/// @brief Semantic Analyzer — Error-Propagation Operator Pass
///
/// Validates usage of the postfix `?!` (error-propagation) operator from the
/// Control Flow Quintet. Enforces:
///   - Operator targets `Result[T, E]` only; rejects `optional[T]` (E4705)
///   - Enclosing function return type must be compatible with `Result[_, E]` (E4706)
///   - Operator is non-overloadable; rejects `opr ?!` definitions (E4702)
///   - Function-level return semantics: if `Ok(v)`, unwrap to `v`;
///     if `Err(e)`, immediately return `Err(e)` from the enclosing function
///
/// Requirements: 28.14, 28.15, 28.16, 24C.22

#include "meld/parser/ast.hpp"
#include <string>
#include <vector>
#include <unordered_set>

namespace meld::compiler {

// ---------------------------------------------------------------------------
// Error-propagation diagnostic
// ---------------------------------------------------------------------------

/// Diagnostic produced by the error-propagation pass.
struct ErrorPropagationDiagnostic {
    enum class Level { Info, Warning, Error };

    Level level;
    std::string code;       ///< e.g., "E4702", "E4705", "E4706"
    std::string message;
    std::string source_file;
    size_t line = 0;
    size_t column = 0;
};

// ---------------------------------------------------------------------------
// ErrorPropagationResult — output of the pass
// ---------------------------------------------------------------------------

struct ErrorPropagationResult {
    bool success = true;
    std::vector<ErrorPropagationDiagnostic> diagnostics;
    size_t error_propagations_checked = 0;
    size_t operator_defs_checked = 0;
    size_t errors_emitted = 0;
};

// ---------------------------------------------------------------------------
// ErrorPropagationPass — the Semantic Analyzer pass
// ---------------------------------------------------------------------------

/// The Error-Propagation Pass validates all uses of the postfix `?!` operator
/// in the AST. It enforces:
///
///   1. The operand of `?!` must be typed `Result[T, E]`. If the operand
///      is `optional[T]`, the pass emits E4705 directing the developer
///      to use `?` instead.
///
///   2. The enclosing function's return type must be compatible with
///      `Result[_, E]`. If not, the pass emits E4706.
///
///   3. No `opr` (operator function / custom operator) definition may
///      redefine `?!`. If found, the pass emits E4702.
///
///   4. Function-level return semantics: when the result is `Err(e)`, the
///      enclosing function immediately returns `Err(e)` (NOT expression-level
///      short-circuit like `?.`).
///
/// The pass operates on parsed AST expressions. The postfix `?!` is
/// represented as a `unary_operation` with op "?!".
class ErrorPropagationPass {
public:
    ErrorPropagationPass();

    /// Run the pass over a set of parsed expressions.
    /// @param expressions  The module-level AST expressions.
    /// @param source_file  Source file path for diagnostics.
    ErrorPropagationResult run(
        const std::vector<parser::ast::expression>& expressions,
        const std::string& source_file = ""
    );

    /// Check if a type annotation represents an optional type.
    static bool is_optional_type(const parser::ast::type_annotation& type);

    /// Check if a type annotation represents a Result type.
    static bool is_result_type(const parser::ast::type_annotation& type);

    /// The set of non-overloadable error-propagation operator symbols.
    static const std::unordered_set<std::string>& forbidden_operator_symbols();

    /// Emit E4705: error-propagation operator `?!` used on non-Result type.
    void emit_non_result_error(
        const std::string& type_name,
        const std::string& source_file,
        size_t line, size_t column,
        ErrorPropagationResult& result
    );

    /// Emit E4706: enclosing function return type not compatible with Result[_, E].
    void emit_incompatible_return_type_error(
        const std::string& return_type_name,
        const std::string& source_file,
        size_t line, size_t column,
        ErrorPropagationResult& result
    );

    /// Emit E4702: attempt to overload a non-overloadable operator.
    void emit_forbidden_overload_error(
        const std::string& operator_symbol,
        const std::string& source_file,
        size_t line, size_t column,
        ErrorPropagationResult& result
    );

private:
    /// Scan an expression for error-propagation usage and operator definitions.
    /// @param in_function  Whether we are inside a function body.
    /// @param func_return_type  The return type of the enclosing function (if any).
    void scan_expression(
        const parser::ast::expression& expr,
        const std::string& source_file,
        ErrorPropagationResult& result,
        bool in_function = false,
        const parser::ast::type_annotation* func_return_type = nullptr
    );

    /// Check a unary_operation for postfix `?!` usage.
    void check_error_propagation(
        const parser::ast::unary_operation& unary,
        const std::string& source_file,
        ErrorPropagationResult& result,
        bool in_function,
        const parser::ast::type_annotation* func_return_type
    );

    /// Check an operator_function definition for forbidden symbols.
    void check_operator_definition(
        const parser::ast::operator_function& op_func,
        const std::string& source_file,
        ErrorPropagationResult& result
    );

    /// Check a custom_operator_definition for forbidden symbols.
    void check_custom_operator_definition(
        const parser::ast::custom_operator_definition& op_def,
        const std::string& source_file,
        ErrorPropagationResult& result
    );

    /// Recursively scan a block for error-propagation usage.
    void scan_block(
        const std::vector<boost::spirit::x3::forward_ast<parser::ast::expression>>& statements,
        const std::string& source_file,
        ErrorPropagationResult& result,
        bool in_function = false,
        const parser::ast::type_annotation* func_return_type = nullptr
    );
};

} // namespace meld::compiler
