/// @file mutability_checker_pass.cpp
/// @brief Semantic Analyzer - Mutability Checker Pass implementation
/// Requirements: 57.2, 57.3

#include "meld/compiler/mutability_checker_pass.hpp"
#include "meld/compat/visit.hpp"
#include <algorithm>

namespace meld::compiler {

MutabilityCheckerPass::MutabilityCheckerPass() = default;

bool MutabilityCheckerPass::is_this_field_assignment(
    const parser::ast::binary_operation& binop
) {
    if (binop.op != "=") return false;
    return meld::compat::visit<bool>([](const auto& lhs_node) -> bool {
        using T = std::decay_t<decltype(lhs_node)>;
        if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::binary_operation>>) {
            const auto& dot_op = lhs_node.get();
            if (dot_op.op != ".") return false;
            return meld::compat::visit<bool>([](const auto& receiver) -> bool {
                using R = std::decay_t<decltype(receiver)>;
                if constexpr (std::is_same_v<R, parser::ast::identifier>) {
                    return receiver.name == "this";
                }
                return false;
            }, dot_op.left.get());
        }
        return false;
    }, binop.left.get());
}

bool MutabilityCheckerPass::is_this_var_fnc_call(
    const parser::ast::function_call& call,
    const std::vector<parser::ast::function_definition>& sibling_methods
) {
    const std::string& callee_name = call.function_name.name;
    for (const auto& sibling : sibling_methods) {
        if (sibling.name.name == callee_name && sibling.is_mutating) {
            return true;
        }
    }
    return false;
}

bool MutabilityCheckerPass::is_this_mutation(
    const parser::ast::expression& expr,
    const std::vector<parser::ast::function_definition>& sibling_methods
) {
    return meld::compat::visit<bool>([&](const auto& node) -> bool {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::binary_operation>>) {
            return is_this_field_assignment(node.get());
        }
        if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::function_call>>) {
            return is_this_var_fnc_call(node.get(), sibling_methods);
        }
        return false;
    }, expr);
}

bool MutabilityCheckerPass::scan_for_this_mutation(
    const parser::ast::expression& expr,
    const std::vector<parser::ast::function_definition>& sibling_methods
) {
    if (is_this_mutation(expr, sibling_methods)) {
        return true;
    }
    return meld::compat::visit<bool>([&](const auto& node) -> bool {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::binary_operation>>) {
            const auto& binop = node.get();
            if (scan_for_this_mutation(binop.left.get(), sibling_methods)) return true;
            if (scan_for_this_mutation(binop.right.get(), sibling_methods)) return true;
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::function_call>>) {
            const auto& call = node.get();
            for (const auto& arg : call.arguments) {
                if (scan_for_this_mutation(arg.get(), sibling_methods)) return true;
            }
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::block_expression>>) {
            return scan_block_for_this_mutation(node.get().statements, sibling_methods);
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::val_declaration>>) {
            return scan_for_this_mutation(node.get().value.get(), sibling_methods);
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::var_declaration>>) {
            return scan_for_this_mutation(node.get().value.get(), sibling_methods);
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::return_statement>>) {
            const auto& ret = node.get();
            if (ret.has_expression) {
                return scan_for_this_mutation(ret.expr.get(), sibling_methods);
            }
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::unary_operation>>) {
            return scan_for_this_mutation(node.get().operand.get(), sibling_methods);
        }
        return false;
    }, expr);
}

bool MutabilityCheckerPass::scan_block_for_this_mutation(
    const std::vector<boost::spirit::x3::forward_ast<parser::ast::expression>>& statements,
    const std::vector<parser::ast::function_definition>& sibling_methods
) {
    for (const auto& stmt : statements) {
        if (scan_for_this_mutation(stmt.get(), sibling_methods)) {
            return true;
        }
    }
    return false;
}

void MutabilityCheckerPass::analyze_method(
    const parser::ast::function_definition& method,
    const std::vector<parser::ast::function_definition>& sibling_methods,
    const std::string& type_name,
    const std::string& source_file,
    MutabilityCheckerResult& result
) {
    result.methods_checked++;
    const auto& body = method.body.get();
    bool mutates_this = scan_block_for_this_mutation(body.statements, sibling_methods);
    if (mutates_this) {
        result.mutations_detected++;
    }
    if (mutates_this && !method.is_mutating) {
        emit_missing_var_fnc(method.name.name, type_name, source_file,
            0, 0, result);
    } else if (!mutates_this && method.is_mutating) {
        emit_unnecessary_var_fnc(method.name.name, type_name, source_file,
            0, 0, result);
    }
}

void MutabilityCheckerPass::analyze_class(
    const parser::ast::class_definition& class_def,
    const std::string& source_file,
    MutabilityCheckerResult& result
) {
    for (const auto& method : class_def.methods) {
        analyze_method(method, class_def.methods,
                       class_def.name.name, source_file, result);
    }
}

void MutabilityCheckerPass::analyze_struct(
    const parser::ast::struct_definition& struct_def,
    const std::string& source_file,
    MutabilityCheckerResult& result
) {
    for (const auto& method : struct_def.methods) {
        analyze_method(method, struct_def.methods,
                       struct_def.name.name, source_file, result);
    }
}

MutabilityCheckerResult MutabilityCheckerPass::run(
    const std::vector<parser::ast::expression>& expressions,
    const std::string& source_file
) {
    MutabilityCheckerResult result;
    for (const auto& expr : expressions) {
        meld::compat::visit([&](const auto& node) {
            using T = std::decay_t<decltype(node)>;
            if constexpr (std::is_same_v<T,
                    boost::spirit::x3::forward_ast<parser::ast::class_definition>>) {
                analyze_class(node.get(), source_file, result);
            }
            else if constexpr (std::is_same_v<T,
                    boost::spirit::x3::forward_ast<parser::ast::struct_definition>>) {
                analyze_struct(node.get(), source_file, result);
            }
        }, expr);
    }
    return result;
}

void MutabilityCheckerPass::emit_missing_var_fnc(
    const std::string& method_name,
    const std::string& type_name,
    const std::string& source_file,
    size_t line, size_t column,
    MutabilityCheckerResult& result
) {
    std::string message =
        "method '" + method_name + "' in '" + type_name +
        "' mutates 'this' but is not declared with 'var fnc' "
        "-- add 'var' before 'fnc' to mark it as mutating";
    result.diagnostics.push_back(MutabilityDiagnostic{
        .level = MutabilityDiagnostic::Level::Error,
        .code = "E5001",
        .message = message,
        .source_file = source_file,
        .line = line,
        .column = column
    });
    result.errors_emitted++;
    result.success = false;
}

void MutabilityCheckerPass::emit_unnecessary_var_fnc(
    const std::string& method_name,
    const std::string& type_name,
    const std::string& source_file,
    size_t line, size_t column,
    MutabilityCheckerResult& result
) {
    std::string message =
        "method '" + method_name + "' in '" + type_name +
        "' is declared 'var fnc' but does not mutate 'this' "
        "-- remove 'var' if mutation is not intended";
    result.diagnostics.push_back(MutabilityDiagnostic{
        .level = MutabilityDiagnostic::Level::Warning,
        .code = "W5001",
        .message = message,
        .source_file = source_file,
        .line = line,
        .column = column
    });
    result.warnings_emitted++;
}

} // namespace meld::compiler
