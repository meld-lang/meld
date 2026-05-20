#pragma once

/// @file elvis_type_checker_pass.hpp
/// @brief Semantic Analyzer — Elvis Operator Type Checker Pass
///
/// Validates usage of the elvis operator (`?:`) from the Control Flow
/// Quintet. Enforces:
///   - Operator targets `optional[T]` only; rejects `Result[T, E]` (E4708)
///   - Operator is non-overloadable; rejects `opr ?:` definitions (E4702)
///   - Provides default value when left operand is nil
///
/// Requirements: 14D.16, 14D.17, 24C.21

#include "meld/parser/ast.hpp"
#include <string>
#include <vector>
#include <unordered_set>

namespace meld::compiler {

// ---------------------------------------------------------------------------
// Elvis type-checker diagnostic
// ---------------------------------------------------------------------------

/// Diagnostic produced by the elvis type-checker pass.
struct ElvisTypeCheckerDiagnostic {
    enum class Level { Info, Warning, Error };

    Level level;
    std::string code;       ///< e.g., "E4708" or "E4702"
    std::string message;
    std::string source_file;
    size_t line = 0;
    size_t column = 0;
};

// ---------------------------------------------------------------------------
// ElvisTypeCheckerResult — output of the pass
// ---------------------------------------------------------------------------

struct ElvisTypeCheckerResult {
    bool success = true;
    std::vector<ElvisTypeCheckerDiagnostic> diagnostics;
    size_t elvis_ops_checked = 0;
    size_t operator_defs_checked = 0;
    size_t errors_emitted = 0;
};

// ---------------------------------------------------------------------------
// ElvisTypeCheckerPass — the Semantic Analyzer pass
// ---------------------------------------------------------------------------

/// The Elvis Type Checker Pass validates all uses of the `?:` operator
/// in the AST. It enforces:
///
///   1. The left operand of `?:` must be typed `optional[T]` or nullable.
///      If the operand is `Result[T, E]`, the pass emits E4708 directing
///      the developer to use `?!` for error propagation instead.
///
///   2. No `opr` (operator function / custom operator) definition may
///      redefine `?:`. If found, the pass emits E4702.
///
/// The pass operates on parsed AST expressions. The elvis operator `?:`
/// is represented as a `binary_operation` with op "?:".
class ElvisTypeCheckerPass {
public:
    ElvisTypeCheckerPass();

    /// Run the pass over a set of parsed expressions.
    /// @param expressions  The module-level AST expressions.
    /// @param source_file  Source file path for diagnostics.
    ElvisTypeCheckerResult run(
        const std::vector<parser::ast::expression>& expressions,
        const std::string& source_file = ""
    );

    /// Check if a type annotation represents an optional type.
    static bool is_optional_type(const parser::ast::type_annotation& type);

    /// Check if a type annotation represents a Result type.
    static bool is_result_type(const parser::ast::type_annotation& type);

    /// The set of non-overloadable elvis operator symbols.
    static const std::unordered_set<std::string>& forbidden_operator_symbols();

    /// Emit E4708: elvis operator `?:` used on Result[T, E] type.
    void emit_result_type_error(
        const std::string& type_name,
        const std::string& source_file,
        size_t line, size_t column,
        ElvisTypeCheckerResult& result
    );

    /// Emit E4702: attempt to overload a non-overloadable operator.
    void emit_forbidden_overload_error(
        const std::string& operator_symbol,
        const std::string& source_file,
        size_t line, size_t column,
        ElvisTypeCheckerResult& result
    );

private:
    /// Scan an expression for elvis usage and operator definitions.
    void scan_expression(
        const parser::ast::expression& expr,
        const std::string& source_file,
        ElvisTypeCheckerResult& result
    );

    /// Check a binary_operation for elvis operator `?:` usage.
    void check_elvis_operation(
        const parser::ast::binary_operation& binop,
        const std::string& source_file,
        ElvisTypeCheckerResult& result
    );

    /// Check an operator_function definition for forbidden symbols.
    void check_operator_definition(
        const parser::ast::operator_function& op_func,
        const std::string& source_file,
        ElvisTypeCheckerResult& result
    );

    /// Check a custom_operator_definition for forbidden symbols.
    void check_custom_operator_definition(
        const parser::ast::custom_operator_definition& op_def,
        const std::string& source_file,
        ElvisTypeCheckerResult& result
    );

    /// Recursively scan a block for elvis usage.
    void scan_block(
        const std::vector<boost::spirit::x3::forward_ast<parser::ast::expression>>& statements,
        const std::string& source_file,
        ElvisTypeCheckerResult& result
    );
};

} // namespace meld::compiler
