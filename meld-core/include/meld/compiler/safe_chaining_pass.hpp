#pragma once

/// @file safe_chaining_pass.hpp
/// @brief Semantic Analyzer — Safe-Chaining Operator Pass
///
/// Validates usage of the safe-chaining operators (`?.`, `?[]`, `?()`)
/// from the Control Flow Quintet. Enforces:
///   - Operators target `optional[T]` only; rejects `Result[T, E]` (E4701)
///   - Operators are non-overloadable; rejects `opr` definitions (E4702)
///   - Short-circuit semantics: if left side is nil, the entire chain
///     evaluates to nil WITHOUT returning from the function
///
/// Requirements: 14B.7, 14B.8, 14B.9, 14B.10, 14B.11, 24C.19

#include "meld/parser/ast.hpp"
#include <string>
#include <vector>
#include <unordered_set>

namespace meld::compiler {

// ---------------------------------------------------------------------------
// Safe-chaining diagnostic
// ---------------------------------------------------------------------------

/// Diagnostic produced by the safe-chaining pass.
struct SafeChainingDiagnostic {
    enum class Level { Info, Warning, Error };

    Level level;
    std::string code;       ///< e.g., "E4701" or "E4702"
    std::string message;
    std::string source_file;
    size_t line = 0;
    size_t column = 0;
};

// ---------------------------------------------------------------------------
// SafeChainingResult — output of the pass
// ---------------------------------------------------------------------------

struct SafeChainingResult {
    bool success = true;
    std::vector<SafeChainingDiagnostic> diagnostics;
    size_t safe_chains_checked = 0;
    size_t operator_defs_checked = 0;
    size_t errors_emitted = 0;
};

// ---------------------------------------------------------------------------
// SafeChainingPass — the Semantic Analyzer pass
// ---------------------------------------------------------------------------

/// The Safe-Chaining Pass validates all uses of `?.`, `?[]`, and `?()`
/// operators in the AST. It enforces:
///
///   1. The left operand of each safe-chaining operator must be typed
///      `optional[T]`. If the operand is `Result[T, E]`, the pass emits
///      E4701 directing the developer to use `?!` instead.
///
///   2. No `opr` (operator function / custom operator) definition may
///      redefine `?.`, `?[]`, or `?()`. If found, the pass emits E4702.
///
///   3. Short-circuit nil propagation: when the left side of a chain is
///      nil, the entire chain evaluates to nil at expression level
///      (does NOT return from the enclosing function).
///
/// The pass operates on parsed AST expressions and uses type annotations
/// to determine whether an operand is optional or Result.
class SafeChainingPass {
public:
    SafeChainingPass();

    /// Run the pass over a set of parsed expressions.
    /// @param expressions  The module-level AST expressions.
    /// @param source_file  Source file path for diagnostics.
    SafeChainingResult run(
        const std::vector<parser::ast::expression>& expressions,
        const std::string& source_file = ""
    );

    /// Check if a type annotation represents an optional type.
    /// Returns true for `optional[T]` or types with is_nullable set.
    static bool is_optional_type(const parser::ast::type_annotation& type);

    /// Check if a type annotation represents a Result type.
    /// Returns true for `Result[T, E]`.
    static bool is_result_type(const parser::ast::type_annotation& type);

    /// The set of non-overloadable safe-chaining operator symbols.
    static const std::unordered_set<std::string>& forbidden_operator_symbols();

    /// Evaluate a safe-chaining expression for short-circuit semantics.
    /// Returns true if the chain short-circuits to nil (left side is nil).
    /// This is expression-level short-circuit, NOT function-level return.
    struct ChainEvaluation {
        bool short_circuits = false;   ///< true if chain yields nil
        bool is_optional_result = true; ///< result type is optional[T]
        std::string result_type_name;   ///< inferred result type name
    };

    static ChainEvaluation evaluate_chain(
        const parser::ast::safe_navigation_expression& expr,
        const parser::ast::type_annotation& left_type
    );

    /// Emit E4701: safe-chaining operator used on non-optional type.
    void emit_non_optional_error(
        const std::string& operator_symbol,
        const std::string& type_name,
        const std::string& source_file,
        size_t line, size_t column,
        SafeChainingResult& result
    );

    /// Emit E4702: attempt to overload a non-overloadable operator.
    void emit_forbidden_overload_error(
        const std::string& operator_symbol,
        const std::string& source_file,
        size_t line, size_t column,
        SafeChainingResult& result
    );

private:
    /// Scan an expression for safe-chaining usage and operator definitions.
    void scan_expression(
        const parser::ast::expression& expr,
        const std::string& source_file,
        SafeChainingResult& result
    );

    /// Check a safe_navigation_expression for type correctness.
    void check_safe_navigation(
        const parser::ast::safe_navigation_expression& expr,
        const std::string& source_file,
        SafeChainingResult& result
    );

    /// Check a binary_operation for safe-chaining operators (?[], ?()).
    void check_safe_chaining_binop(
        const parser::ast::binary_operation& binop,
        const std::string& source_file,
        SafeChainingResult& result
    );

    /// Check an operator_function definition for forbidden symbols.
    void check_operator_definition(
        const parser::ast::operator_function& op_func,
        const std::string& source_file,
        SafeChainingResult& result
    );

    /// Check a custom_operator_definition for forbidden symbols.
    void check_custom_operator_definition(
        const parser::ast::custom_operator_definition& op_def,
        const std::string& source_file,
        SafeChainingResult& result
    );

    /// Recursively scan a block for safe-chaining usage.
    void scan_block(
        const std::vector<boost::spirit::x3::forward_ast<parser::ast::expression>>& statements,
        const std::string& source_file,
        SafeChainingResult& result
    );
};

} // namespace meld::compiler
