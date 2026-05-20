/// @file param_mutability_checker_pass.cpp
/// @brief Semantic Analyzer - Parameter Mutability Checker Pass implementation
/// Requirements: 57.5, 57.6, 57.7

#include "meld/compiler/param_mutability_checker_pass.hpp"
#include "meld/compat/visit.hpp"
#include <algorithm>

namespace meld::compiler {

ParamMutabilityCheckerPass::ParamMutabilityCheckerPass() = default;

std::string ParamMutabilityCheckerPass::get_direct_assign_target(
    const parser::ast::binary_operation& binop
) {
    if (binop.op != "=") return "";
    // LHS must be a plain identifier (not a dot-access)
    return meld::compat::visit<std::string>([](const auto& lhs_node) -> std::string {
        using T = std::decay_t<decltype(lhs_node)>;
        if constexpr (std::is_same_v<T, parser::ast::identifier>) {
            return lhs_node.name;
        }
        return "";
    }, binop.left.get());
}

std::string ParamMutabilityCheckerPass::get_field_assign_target(
    const parser::ast::binary_operation& binop
) {
    if (binop.op != "=") return "";
    // LHS must be a dot-access: identifier.field
    return meld::compat::visit<std::string>([](const auto& lhs_node) -> std::string {
        using T = std::decay_t<decltype(lhs_node)>;
        if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::binary_operation>>) {
            const auto& dot_op = lhs_node.get();
            if (dot_op.op != ".") return "";
            // The receiver of the dot must be a plain identifier
            return meld::compat::visit<std::string>([](const auto& receiver) -> std::string {
                using R = std::decay_t<decltype(receiver)>;
                if constexpr (std::is_same_v<R, parser::ast::identifier>) {
                    // Skip "this" — that's handled by MutabilityCheckerPass
                    if (receiver.name == "this") return "";
                    return receiver.name;
                }
                return "";
            }, dot_op.left.get());
        }
        return "";
    }, binop.left.get());
}

void ParamMutabilityCheckerPass::collect_mutated_params(
    const parser::ast::expression& expr,
    const std::set<std::string>& param_names,
    std::set<std::string>& mutated
) {
    meld::compat::visit([&](const auto& node) {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::binary_operation>>) {
            const auto& binop = node.get();
            // Check for direct assignment: param = value
            std::string direct = get_direct_assign_target(binop);
            if (!direct.empty() && param_names.count(direct)) {
                mutated.insert(direct);
            }
            // Check for field assignment: param.field = value
            std::string field = get_field_assign_target(binop);
            if (!field.empty() && param_names.count(field)) {
                mutated.insert(field);
            }
            // Recurse into both sides
            collect_mutated_params(binop.left.get(), param_names, mutated);
            collect_mutated_params(binop.right.get(), param_names, mutated);
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::function_call>>) {
            const auto& call = node.get();
            for (const auto& arg : call.arguments) {
                collect_mutated_params(arg.get(), param_names, mutated);
            }
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::block_expression>>) {
            collect_mutated_params_in_block(node.get().statements, param_names, mutated);
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::val_declaration>>) {
            collect_mutated_params(node.get().value.get(), param_names, mutated);
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::var_declaration>>) {
            collect_mutated_params(node.get().value.get(), param_names, mutated);
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::return_statement>>) {
            const auto& ret = node.get();
            if (ret.has_expression) {
                collect_mutated_params(ret.expr.get(), param_names, mutated);
            }
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::unary_operation>>) {
            collect_mutated_params(node.get().operand.get(), param_names, mutated);
        }
    }, expr);
}

void ParamMutabilityCheckerPass::collect_mutated_params_in_block(
    const std::vector<boost::spirit::x3::forward_ast<parser::ast::expression>>& statements,
    const std::set<std::string>& param_names,
    std::set<std::string>& mutated
) {
    for (const auto& stmt : statements) {
        collect_mutated_params(stmt.get(), param_names, mutated);
    }
}

void ParamMutabilityCheckerPass::analyze_function(
    const parser::ast::function_definition& func,
    const std::string& context_name,
    const std::string& source_file,
    ParamMutabilityResult& result
) {
    result.functions_checked++;

    // Build set of parameter names
    std::set<std::string> param_names;
    for (const auto& param : func.parameters) {
        param_names.insert(param.name.name);
    }

    if (param_names.empty()) return;

    // Scan body for mutations
    std::set<std::string> mutated;
    const auto& body = func.body.get();
    collect_mutated_params_in_block(body.statements, param_names, mutated);

    result.param_mutations_detected += mutated.size();

    // Check each parameter
    for (const auto& param : func.parameters) {
        bool is_mutated = mutated.count(param.name.name) > 0;
        bool is_var = param.is_mutable;

        if (is_mutated && !is_var) {
            emit_param_mutated_without_var(
                param.name.name, func.name.name,
                source_file, 0, 0, result);
        } else if (!is_mutated && is_var) {
            emit_unnecessary_var_param(
                param.name.name, func.name.name,
                source_file, 0, 0, result);
        }
    }
}

void ParamMutabilityCheckerPass::analyze_class(
    const parser::ast::class_definition& class_def,
    const std::string& source_file,
    ParamMutabilityResult& result
) {
    for (const auto& method : class_def.methods) {
        analyze_function(method, class_def.name.name, source_file, result);
    }
}

void ParamMutabilityCheckerPass::analyze_struct(
    const parser::ast::struct_definition& struct_def,
    const std::string& source_file,
    ParamMutabilityResult& result
) {
    for (const auto& method : struct_def.methods) {
        analyze_function(method, struct_def.name.name, source_file, result);
    }
}

ParamMutabilityResult ParamMutabilityCheckerPass::run(
    const std::vector<parser::ast::expression>& expressions,
    const std::string& source_file
) {
    ParamMutabilityResult result;
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
            else if constexpr (std::is_same_v<T,
                    boost::spirit::x3::forward_ast<parser::ast::function_definition>>) {
                // Top-level function
                analyze_function(node.get(), "", source_file, result);
            }
        }, expr);
    }
    return result;
}

void ParamMutabilityCheckerPass::emit_param_mutated_without_var(
    const std::string& param_name,
    const std::string& func_name,
    const std::string& source_file,
    size_t line, size_t column,
    ParamMutabilityResult& result
) {
    std::string message =
        "parameter '" + param_name + "' in function '" + func_name +
        "' is mutated but not declared with 'var' "
        "-- use 'fnc " + func_name + "(" + param_name + ": var ...)' to allow mutation";
    result.diagnostics.push_back(ParamMutabilityDiagnostic{
        .level = ParamMutabilityDiagnostic::Level::Error,
        .code = "E5002",
        .message = message,
        .source_file = source_file,
        .line = line,
        .column = column
    });
    result.errors_emitted++;
    result.success = false;
}

void ParamMutabilityCheckerPass::emit_unnecessary_var_param(
    const std::string& param_name,
    const std::string& func_name,
    const std::string& source_file,
    size_t line, size_t column,
    ParamMutabilityResult& result
) {
    std::string message =
        "parameter '" + param_name + "' in function '" + func_name +
        "' is declared 'var' but never mutated "
        "-- remove 'var' if mutation is not intended";
    result.diagnostics.push_back(ParamMutabilityDiagnostic{
        .level = ParamMutabilityDiagnostic::Level::Warning,
        .code = "W5002",
        .message = message,
        .source_file = source_file,
        .line = line,
        .column = column
    });
    result.warnings_emitted++;
}

} // namespace meld::compiler
