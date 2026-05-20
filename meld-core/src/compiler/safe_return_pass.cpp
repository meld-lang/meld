/// @file safe_return_pass.cpp
/// @brief Semantic Analyzer — Safe-Return Operator Pass implementation
/// Requirements: 14C.12, 14C.13, 14C.14, 24C.20

#include "meld/compiler/safe_return_pass.hpp"
#include "meld/compat/visit.hpp"
#include <algorithm>

namespace meld::compiler {

SafeReturnPass::SafeReturnPass() = default;

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

const std::unordered_set<std::string>& SafeReturnPass::forbidden_operator_symbols() {
    static const std::unordered_set<std::string> symbols = {
        "?"
    };
    return symbols;
}

bool SafeReturnPass::is_optional_type(const parser::ast::type_annotation& type) {
    if (type.is_nullable) return true;
    if (type.type_name.name == "optional" && type.has_type_arguments) return true;
    return false;
}

bool SafeReturnPass::is_result_type(const parser::ast::type_annotation& type) {
    return type.type_name.name == "Result" && type.has_type_arguments;
}

// ---------------------------------------------------------------------------
// Expression scanning
// ---------------------------------------------------------------------------

void SafeReturnPass::scan_expression(
    const parser::ast::expression& expr,
    const std::string& source_file,
    SafeReturnResult& result,
    bool in_function,
    const parser::ast::type_annotation* func_return_type
) {
    meld::compat::visit([&](const auto& node) {
        using T = std::decay_t<decltype(node)>;

        if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::unary_operation>>) {
            check_safe_return(node.get(), source_file, result,
                              in_function, func_return_type);
            // Also recurse into the operand
            scan_expression(node.get().operand.get(), source_file, result,
                            in_function, func_return_type);
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::binary_operation>>) {
            scan_expression(node.get().left.get(), source_file, result,
                            in_function, func_return_type);
            scan_expression(node.get().right.get(), source_file, result,
                            in_function, func_return_type);
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
            scan_block(node.get().statements, source_file, result,
                       in_function, func_return_type);
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::val_declaration>>) {
            scan_expression(node.get().value.get(), source_file, result,
                            in_function, func_return_type);
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::var_declaration>>) {
            scan_expression(node.get().value.get(), source_file, result,
                            in_function, func_return_type);
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::function_definition>>) {
            // Enter a new function scope — track its return type
            const auto& func = node.get();
            const parser::ast::type_annotation* ret_type =
                func.has_return_type ? &func.return_type : nullptr;
            scan_block(func.body.get().statements, source_file, result,
                       true, ret_type);
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::function_call>>) {
            for (const auto& arg : node.get().arguments) {
                scan_expression(arg.get(), source_file, result,
                                in_function, func_return_type);
            }
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::safe_navigation_expression>>) {
            scan_expression(node.get().nullable_expr.get(), source_file, result,
                            in_function, func_return_type);
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::return_statement>>) {
            if (node.get().has_expression) {
                scan_expression(node.get().expr.get(), source_file, result,
                                in_function, func_return_type);
            }
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::class_definition>>) {
            const auto& cls = node.get();
            for (const auto& method : cls.methods) {
                const parser::ast::type_annotation* ret_type =
                    method.has_return_type ? &method.return_type : nullptr;
                scan_block(method.body.get().statements, source_file, result,
                           true, ret_type);
            }
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::struct_definition>>) {
            const auto& s = node.get();
            for (const auto& method : s.methods) {
                const parser::ast::type_annotation* ret_type =
                    method.has_return_type ? &method.return_type : nullptr;
                scan_block(method.body.get().statements, source_file, result,
                           true, ret_type);
            }
        }
    }, expr);
}

void SafeReturnPass::scan_block(
    const std::vector<boost::spirit::x3::forward_ast<parser::ast::expression>>& statements,
    const std::string& source_file,
    SafeReturnResult& result,
    bool in_function,
    const parser::ast::type_annotation* func_return_type
) {
    for (const auto& stmt : statements) {
        scan_expression(stmt.get(), source_file, result,
                        in_function, func_return_type);
    }
}

// ---------------------------------------------------------------------------
// Type checking for safe-return operator
// ---------------------------------------------------------------------------

void SafeReturnPass::check_safe_return(
    const parser::ast::unary_operation& unary,
    const std::string& source_file,
    SafeReturnResult& result,
    bool in_function,
    const parser::ast::type_annotation* func_return_type
) {
    if (unary.op != "?") return;

    result.safe_returns_checked++;

    // If not inside a function body, emit E4704 (no enclosing function)
    if (!in_function) {
        emit_incompatible_return_type_error(
            "<top-level>", source_file, 0, 0, result);
        return;
    }

    // If enclosing function has a return type, verify it's optional-compatible
    if (func_return_type && !is_optional_type(*func_return_type)) {
        emit_incompatible_return_type_error(
            func_return_type->type_name.name, source_file, 0, 0, result);
    }
}

// ---------------------------------------------------------------------------
// Operator overload enforcement
// ---------------------------------------------------------------------------

void SafeReturnPass::check_operator_definition(
    const parser::ast::operator_function& op_func,
    const std::string& source_file,
    SafeReturnResult& result
) {
    result.operator_defs_checked++;
    const auto& forbidden = forbidden_operator_symbols();
    if (forbidden.count(op_func.symbol)) {
        emit_forbidden_overload_error(op_func.symbol, source_file, 0, 0, result);
    }
}

void SafeReturnPass::check_custom_operator_definition(
    const parser::ast::custom_operator_definition& op_def,
    const std::string& source_file,
    SafeReturnResult& result
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

SafeReturnResult SafeReturnPass::run(
    const std::vector<parser::ast::expression>& expressions,
    const std::string& source_file
) {
    SafeReturnResult result;
    for (const auto& expr : expressions) {
        scan_expression(expr, source_file, result);
    }
    return result;
}

// ---------------------------------------------------------------------------
// Diagnostic emitters
// ---------------------------------------------------------------------------

void SafeReturnPass::emit_non_optional_error(
    const std::string& type_name,
    const std::string& source_file,
    size_t line, size_t column,
    SafeReturnResult& result
) {
    std::string message =
        "safe-return operator '?' used on type '" + type_name +
        "' which is not optional[T] — "
        "the postfix '?' operator targets optional[T] only; "
        "use '?!' for Result[T, E] error propagation";
    result.diagnostics.push_back(SafeReturnDiagnostic{
        .level = SafeReturnDiagnostic::Level::Error,
        .code = "E4703",
        .message = message,
        .source_file = source_file,
        .line = line,
        .column = column
    });
    result.errors_emitted++;
    result.success = false;
}

void SafeReturnPass::emit_incompatible_return_type_error(
    const std::string& return_type_name,
    const std::string& source_file,
    size_t line, size_t column,
    SafeReturnResult& result
) {
    std::string message =
        "safe-return operator '?' used in function with return type '" +
        return_type_name + "' which is not compatible with optional[T] — "
        "the enclosing function must return optional[T] to use the '?' operator, "
        "because '?' returns nil from the function when the operand is nil";
    result.diagnostics.push_back(SafeReturnDiagnostic{
        .level = SafeReturnDiagnostic::Level::Error,
        .code = "E4704",
        .message = message,
        .source_file = source_file,
        .line = line,
        .column = column
    });
    result.errors_emitted++;
    result.success = false;
}

void SafeReturnPass::emit_forbidden_overload_error(
    const std::string& operator_symbol,
    const std::string& source_file,
    size_t line, size_t column,
    SafeReturnResult& result
) {
    std::string message =
        "operator '" + operator_symbol + "' is reserved and non-overloadable — "
        "the safe-return operator has fixed compiler-guaranteed semantics "
        "to ensure predictable nil-propagation control flow; it cannot be redefined";
    result.diagnostics.push_back(SafeReturnDiagnostic{
        .level = SafeReturnDiagnostic::Level::Error,
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
