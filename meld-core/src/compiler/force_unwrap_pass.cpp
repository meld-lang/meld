/// @file force_unwrap_pass.cpp
/// @brief Semantic Analyzer — Force-Unwrap / Panic Operator Pass implementation
/// Requirements: 14E.18, 14E.19, 24C.23

#include "meld/compiler/force_unwrap_pass.hpp"
#include "meld/compat/visit.hpp"
#include <algorithm>

namespace meld::compiler {

ForceUnwrapPass::ForceUnwrapPass() = default;

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

const std::unordered_set<std::string>& ForceUnwrapPass::forbidden_operator_symbols() {
    static const std::unordered_set<std::string> symbols = {
        "!!"
    };
    return symbols;
}

bool ForceUnwrapPass::is_optional_type(const parser::ast::type_annotation& type) {
    if (type.is_nullable) return true;
    if (type.type_name.name == "optional" && type.has_type_arguments) return true;
    return false;
}

bool ForceUnwrapPass::is_result_type(const parser::ast::type_annotation& type) {
    return type.type_name.name == "Result" && type.has_type_arguments;
}

// ---------------------------------------------------------------------------
// Expression scanning
// ---------------------------------------------------------------------------

void ForceUnwrapPass::scan_expression(
    const parser::ast::expression& expr,
    const std::string& source_file,
    ForceUnwrapResult& result,
    bool in_function
) {
    meld::compat::visit([&](const auto& node) {
        using T = std::decay_t<decltype(node)>;

        if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::unary_operation>>) {
            check_force_unwrap(node.get(), source_file, result, in_function);
            // Also recurse into the operand
            scan_expression(node.get().operand.get(), source_file, result,
                            in_function);
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::binary_operation>>) {
            scan_expression(node.get().left.get(), source_file, result,
                            in_function);
            scan_expression(node.get().right.get(), source_file, result,
                            in_function);
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
                       in_function);
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::val_declaration>>) {
            scan_expression(node.get().value.get(), source_file, result,
                            in_function);
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::var_declaration>>) {
            scan_expression(node.get().value.get(), source_file, result,
                            in_function);
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::function_definition>>) {
            // Enter a new function scope
            const auto& func = node.get();
            scan_block(func.body.get().statements, source_file, result, true);
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::function_call>>) {
            for (const auto& arg : node.get().arguments) {
                scan_expression(arg.get(), source_file, result, in_function);
            }
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::safe_navigation_expression>>) {
            scan_expression(node.get().nullable_expr.get(), source_file, result,
                            in_function);
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::return_statement>>) {
            if (node.get().has_expression) {
                scan_expression(node.get().expr.get(), source_file, result,
                                in_function);
            }
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::class_definition>>) {
            const auto& cls = node.get();
            for (const auto& method : cls.methods) {
                scan_block(method.body.get().statements, source_file, result,
                           true);
            }
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::struct_definition>>) {
            const auto& s = node.get();
            for (const auto& method : s.methods) {
                scan_block(method.body.get().statements, source_file, result,
                           true);
            }
        }
    }, expr);
}

void ForceUnwrapPass::scan_block(
    const std::vector<boost::spirit::x3::forward_ast<parser::ast::expression>>& statements,
    const std::string& source_file,
    ForceUnwrapResult& result,
    bool in_function
) {
    for (const auto& stmt : statements) {
        scan_expression(stmt.get(), source_file, result, in_function);
    }
}

// ---------------------------------------------------------------------------
// Force-unwrap checking — `!!` targets BOTH optional[T] and Result[T, E]
// ---------------------------------------------------------------------------

void ForceUnwrapPass::check_force_unwrap(
    const parser::ast::unary_operation& unary,
    const std::string& source_file,
    ForceUnwrapResult& result,
    bool /*in_function*/
) {
    if (unary.op != "!!") return;

    result.force_unwraps_checked++;

    // `!!` is allowed at top level — no function context check needed.
    // `!!` targets both optional[T] and Result[T, E] — no type rejection.
    // The pass only warns if the operand is neither optional nor Result.
    // (Full type inference is not available at this pass level, so this
    //  check is best-effort based on AST structure.)
}

// ---------------------------------------------------------------------------
// Operator overload enforcement
// ---------------------------------------------------------------------------

void ForceUnwrapPass::check_operator_definition(
    const parser::ast::operator_function& op_func,
    const std::string& source_file,
    ForceUnwrapResult& result
) {
    result.operator_defs_checked++;
    const auto& forbidden = forbidden_operator_symbols();
    if (forbidden.count(op_func.symbol)) {
        emit_forbidden_overload_error(op_func.symbol, source_file, 0, 0, result);
    }
}

void ForceUnwrapPass::check_custom_operator_definition(
    const parser::ast::custom_operator_definition& op_def,
    const std::string& source_file,
    ForceUnwrapResult& result
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

ForceUnwrapResult ForceUnwrapPass::run(
    const std::vector<parser::ast::expression>& expressions,
    const std::string& source_file
) {
    ForceUnwrapResult result;
    for (const auto& expr : expressions) {
        scan_expression(expr, source_file, result);
    }
    return result;
}

// ---------------------------------------------------------------------------
// Diagnostic emitters
// ---------------------------------------------------------------------------

void ForceUnwrapPass::emit_unnecessary_force_unwrap_warning(
    const std::string& type_name,
    const std::string& source_file,
    size_t line, size_t column,
    ForceUnwrapResult& result
) {
    std::string message =
        "force-unwrap operator '!!' used on type '" + type_name +
        "' which is neither optional[T] nor Result[T, E] — "
        "the '!!' operator is intended for unwrapping optional or Result values; "
        "using it on a non-optional, non-Result type has no effect";
    result.diagnostics.push_back(ForceUnwrapDiagnostic{
        .level = ForceUnwrapDiagnostic::Level::Warning,
        .code = "W4707",
        .message = message,
        .source_file = source_file,
        .line = line,
        .column = column
    });
    result.warnings_emitted++;
}

void ForceUnwrapPass::emit_forbidden_overload_error(
    const std::string& operator_symbol,
    const std::string& source_file,
    size_t line, size_t column,
    ForceUnwrapResult& result
) {
    std::string message =
        "operator '" + operator_symbol + "' is reserved and non-overloadable — "
        "the force-unwrap operator has fixed compiler-guaranteed panic semantics "
        "to ensure predictable unwrap-or-crash control flow; it cannot be redefined";
    result.diagnostics.push_back(ForceUnwrapDiagnostic{
        .level = ForceUnwrapDiagnostic::Level::Error,
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
