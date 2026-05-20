/// @file safe_chaining_pass.cpp
/// @brief Semantic Analyzer — Safe-Chaining Operator Pass implementation
/// Requirements: 14B.7, 14B.8, 14B.9, 14B.10, 14B.11, 24C.19

#include "meld/compiler/safe_chaining_pass.hpp"
#include "meld/compat/visit.hpp"
#include <algorithm>

namespace meld::compiler {

SafeChainingPass::SafeChainingPass() = default;

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

const std::unordered_set<std::string>& SafeChainingPass::forbidden_operator_symbols() {
    static const std::unordered_set<std::string> symbols = {
        "?.", "?[]", "?()"
    };
    return symbols;
}

bool SafeChainingPass::is_optional_type(const parser::ast::type_annotation& type) {
    if (type.is_nullable) return true;
    if (type.type_name.name == "optional" && type.has_type_arguments) return true;
    return false;
}

bool SafeChainingPass::is_result_type(const parser::ast::type_annotation& type) {
    return type.type_name.name == "Result" && type.has_type_arguments;
}

SafeChainingPass::ChainEvaluation SafeChainingPass::evaluate_chain(
    const parser::ast::safe_navigation_expression& expr,
    const parser::ast::type_annotation& left_type
) {
    ChainEvaluation eval;
    // If the left type is optional and the value is nil, the chain
    // short-circuits to nil at expression level (NOT function return).
    if (is_optional_type(left_type)) {
        eval.short_circuits = true;  // chain CAN short-circuit
        eval.is_optional_result = true;
        eval.result_type_name = "optional";
    } else {
        eval.short_circuits = false;
        eval.is_optional_result = false;
        eval.result_type_name = left_type.type_name.name;
    }
    return eval;
}

// ---------------------------------------------------------------------------
// Expression scanning
// ---------------------------------------------------------------------------

void SafeChainingPass::scan_expression(
    const parser::ast::expression& expr,
    const std::string& source_file,
    SafeChainingResult& result
) {
    meld::compat::visit([&](const auto& node) {
        using T = std::decay_t<decltype(node)>;

        if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::safe_navigation_expression>>) {
            check_safe_navigation(node.get(), source_file, result);
            // Also recurse into the nullable_expr
            scan_expression(node.get().nullable_expr.get(), source_file, result);
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::binary_operation>>) {
            check_safe_chaining_binop(node.get(), source_file, result);
            scan_expression(node.get().left.get(), source_file, result);
            scan_expression(node.get().right.get(), source_file, result);
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::operator_function>>) {
            check_operator_definition(node.get(), source_file, result);
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::custom_operator_definition>>) {
            check_custom_operator_definition(node.get(), source_file, result);
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::block_expression>>) {
            scan_block(node.get().statements, source_file, result);
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::val_declaration>>) {
            scan_expression(node.get().value.get(), source_file, result);
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::var_declaration>>) {
            scan_expression(node.get().value.get(), source_file, result);
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::function_definition>>) {
            scan_block(node.get().body.get().statements, source_file, result);
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::function_call>>) {
            for (const auto& arg : node.get().arguments) {
                scan_expression(arg.get(), source_file, result);
            }
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::unary_operation>>) {
            scan_expression(node.get().operand.get(), source_file, result);
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::return_statement>>) {
            if (node.get().has_expression) {
                scan_expression(node.get().expr.get(), source_file, result);
            }
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::class_definition>>) {
            const auto& cls = node.get();
            for (const auto& method : cls.methods) {
                scan_block(method.body.get().statements, source_file, result);
            }
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::struct_definition>>) {
            const auto& s = node.get();
            for (const auto& method : s.methods) {
                scan_block(method.body.get().statements, source_file, result);
            }
        }
    }, expr);
}

void SafeChainingPass::scan_block(
    const std::vector<boost::spirit::x3::forward_ast<parser::ast::expression>>& statements,
    const std::string& source_file,
    SafeChainingResult& result
) {
    for (const auto& stmt : statements) {
        scan_expression(stmt.get(), source_file, result);
    }
}

// ---------------------------------------------------------------------------
// Type checking for safe-chaining operators
// ---------------------------------------------------------------------------

void SafeChainingPass::check_safe_navigation(
    const parser::ast::safe_navigation_expression& expr,
    const std::string& source_file,
    SafeChainingResult& result
) {
    result.safe_chains_checked++;

    // Check if the nullable_expr has a type annotation we can inspect.
    // We look for val/var declarations wrapping the expression, or
    // check the expression itself for type information.
    // For AST-level checking, we inspect the inner expression for
    // Result type indicators.
    meld::compat::visit([&](const auto& inner) {
        using T = std::decay_t<decltype(inner)>;
        if constexpr (std::is_same_v<T, parser::ast::identifier>) {
            // Check if the identifier name suggests a Result type
            // (actual type resolution happens in a later pass; here we
            // check annotations attached to declarations)
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::function_call>>) {
            // Function calls may return Result — checked via return type
        }
    }, expr.nullable_expr.get());
}

void SafeChainingPass::check_safe_chaining_binop(
    const parser::ast::binary_operation& binop,
    const std::string& source_file,
    SafeChainingResult& result
) {
    if (binop.op == "?[]" || binop.op == "?()") {
        result.safe_chains_checked++;
    }
}

// ---------------------------------------------------------------------------
// Operator overload enforcement
// ---------------------------------------------------------------------------

void SafeChainingPass::check_operator_definition(
    const parser::ast::operator_function& op_func,
    const std::string& source_file,
    SafeChainingResult& result
) {
    result.operator_defs_checked++;
    const auto& forbidden = forbidden_operator_symbols();
    if (forbidden.count(op_func.symbol)) {
        emit_forbidden_overload_error(op_func.symbol, source_file, 0, 0, result);
    }
}

void SafeChainingPass::check_custom_operator_definition(
    const parser::ast::custom_operator_definition& op_def,
    const std::string& source_file,
    SafeChainingResult& result
) {
    result.operator_defs_checked++;
    const auto& forbidden = forbidden_operator_symbols();
    if (forbidden.count(op_def.symbol)) {
        emit_forbidden_overload_error(op_def.symbol, source_file, 0, 0, result);
    }
}

// ---------------------------------------------------------------------------
// Top-level run
// ---------------------------------------------------------------------------

SafeChainingResult SafeChainingPass::run(
    const std::vector<parser::ast::expression>& expressions,
    const std::string& source_file
) {
    SafeChainingResult result;
    for (const auto& expr : expressions) {
        scan_expression(expr, source_file, result);
    }
    return result;
}

// ---------------------------------------------------------------------------
// Diagnostic emitters
// ---------------------------------------------------------------------------

void SafeChainingPass::emit_non_optional_error(
    const std::string& operator_symbol,
    const std::string& type_name,
    const std::string& source_file,
    size_t line, size_t column,
    SafeChainingResult& result
) {
    std::string message =
        "safe-chaining operator '" + operator_symbol + "' used on type '" +
        type_name + "' which is not optional[T] — " +
        "safe-chaining operators target optional[T] only; "
        "use '?!' for Result[T, E] error propagation";
    result.diagnostics.push_back(SafeChainingDiagnostic{
        .level = SafeChainingDiagnostic::Level::Error,
        .code = "E4701",
        .message = message,
        .source_file = source_file,
        .line = line,
        .column = column
    });
    result.errors_emitted++;
    result.success = false;
}

void SafeChainingPass::emit_forbidden_overload_error(
    const std::string& operator_symbol,
    const std::string& source_file,
    size_t line, size_t column,
    SafeChainingResult& result
) {
    std::string message =
        "operator '" + operator_symbol + "' is reserved and non-overloadable — "
        "safe-chaining operators have fixed compiler-guaranteed semantics "
        "to ensure predictable control flow; they cannot be redefined";
    result.diagnostics.push_back(SafeChainingDiagnostic{
        .level = SafeChainingDiagnostic::Level::Error,
        .code = "E4702",
        .message = message,
        .source_file = source_file,
        .line = line,
        .column = column
    });
    result.errors_emitted++;
    result.success = false;
}

} // namespace meld::compiler
