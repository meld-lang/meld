/// @file mutability_enforcement_pass.cpp
/// @brief Semantic Analyzer — Mutability Enforcement Pass
///
/// Enforces mutability contracts on generic type parameters:
/// - val-qualified type param + mutating method → E4010
/// - var-qualified type param + mutating method without @effect(state) → E4011
/// - val→var qualifier upgrade without checked cast → E4012
///
/// Requirements: 165.3, 165.4, 165.5, 165.6

#include "meld/compiler/mutability_enforcement_pass.hpp"
#include "meld/compat/visit.hpp"
#include <algorithm>

namespace meld::compiler {

using MQ = parser::ast::type_annotation::MutabilityQualifier;

// ===========================================================================
// Known mutating methods
// ===========================================================================

const std::unordered_set<std::string>& MutabilityEnforcementPass::mutating_methods() {
    static const std::unordered_set<std::string> methods = {
        "add", "remove", "clear", "sort-in-place",
        "push", "pop", "pop-front", "pop-back",
        "set", "insert", "delete", "update",
        "append", "prepend"
    };
    return methods;
}

bool MutabilityEnforcementPass::is_mutating_method(const std::string& method_name) {
    return mutating_methods().count(method_name) > 0;
}

// ===========================================================================
// Qualifier compatibility (Req 165.5, 165.6)
// ===========================================================================

bool MutabilityEnforcementPass::is_qualifier_compatible(
    MQ source, MQ target
) {
    // VAL → VAL: compatible (same qualifier)
    // VAR → VAR: compatible (same qualifier)
    // VAR → VAL: compatible (downgrade — restricting permissions)
    // VAL → VAR: NOT compatible (upgrade — would need checked cast)
    if (source == target) return true;
    if (source == MQ::VAR && target == MQ::VAL) return true;
    // source == VAL && target == VAR → upgrade, not allowed
    return false;
}

bool MutabilityEnforcementPass::check_qualifier_assignment(
    MQ source, MQ target,
    const std::string& source_file,
    size_t line, size_t column,
    MutabilityEnforcementResult& result
) {
    if (is_qualifier_compatible(source, target)) {
        return true;
    }
    emit_qualifier_upgrade_violation(source_file, line, column, result);
    return false;
}

// ===========================================================================
// Constructor
// ===========================================================================

MutabilityEnforcementPass::MutabilityEnforcementPass() = default;

// ===========================================================================
// Effect detection
// ===========================================================================

bool MutabilityEnforcementPass::has_effect_state(
    const parser::ast::function_definition& func
) {
    if (!func.has_effects) return false;
    for (const auto& eff : func.effects_clause) {
        if (eff.name == "state") return true;
    }
    return false;
}

// ===========================================================================
// Binding extraction
// ===========================================================================

MutabilityEnforcementPass::BindingInfo MutabilityEnforcementPass::make_binding(
    const std::string& name,
    const parser::ast::type_annotation& type
) {
    BindingInfo info;
    info.name = name;
    info.has_generic_type = type.has_type_arguments && !type.type_arguments.empty();
    if (info.has_generic_type) {
        info.first_arg_qualifier = type.type_arguments[0].get().mutability_qualifier;
    }
    return info;
}

// ===========================================================================
// Diagnostics
// ===========================================================================

void MutabilityEnforcementPass::emit_val_violation(
    const std::string& method_name,
    const std::string& source_file,
    size_t line, size_t column,
    MutabilityEnforcementResult& result
) {
    result.diagnostics.push_back(MutabilityEnforcementDiagnostic{
        .level = MutabilityEnforcementDiagnostic::Level::Error,
        .code = "E4010",
        .message = "Cannot call mutating method '" + method_name +
                   "' on val-qualified type parameter"
                   " — the type parameter is immutable",
        .source_file = source_file,
        .line = line,
        .column = column
    });
    result.val_violations++;
    result.success = false;
}

void MutabilityEnforcementPass::emit_effect_violation(
    const std::string& method_name,
    const std::string& source_file,
    size_t line, size_t column,
    MutabilityEnforcementResult& result
) {
    result.diagnostics.push_back(MutabilityEnforcementDiagnostic{
        .level = MutabilityEnforcementDiagnostic::Level::Error,
        .code = "E4011",
        .message = "Mutating method '" + method_name +
                   "' on var-qualified type parameter requires"
                   " @effect(state) on the enclosing function",
        .source_file = source_file,
        .line = line,
        .column = column
    });
    result.effect_violations++;
    result.success = false;
}

// E4012: val→var qualifier upgrade attempt (Req 165.5, 165.6)
void MutabilityEnforcementPass::emit_qualifier_upgrade_violation(
    const std::string& source_file,
    size_t line, size_t column,
    MutabilityEnforcementResult& result
) {
    result.diagnostics.push_back(MutabilityEnforcementDiagnostic{
        .level = MutabilityEnforcementDiagnostic::Level::Error,
        .code = "E4012",
        .message = "Cannot pass val-qualified type where var-qualified is expected"
                   " — use an explicit checked cast to upgrade mutability permissions",
        .source_file = source_file,
        .line = line,
        .column = column
    });
    result.qualifier_upgrade_violations++;
    result.success = false;
}

// ===========================================================================
// Main entry point
// ===========================================================================

MutabilityEnforcementResult MutabilityEnforcementPass::run(
    const std::vector<parser::ast::expression>& expressions,
    const std::string& source_file
) {
    MutabilityEnforcementResult result;

    for (const auto& expr : expressions) {
        // We need a mutable bindings vector for top-level scanning
        std::vector<BindingInfo> bindings;
        scan_expression(expr, bindings, /*enclosing_has_effect_state=*/false,
                        source_file, result);
    }

    return result;
}

// ===========================================================================
// Function scanning
// ===========================================================================

void MutabilityEnforcementPass::scan_function(
    const parser::ast::function_definition& func,
    const std::string& source_file,
    MutabilityEnforcementResult& result
) {
    bool func_has_state = has_effect_state(func);

    // Collect bindings from function parameters
    std::vector<BindingInfo> bindings;
    for (const auto& param : func.parameters) {
        bindings.push_back(make_binding(param.name.name, param.type));
    }

    // Req 165.7: Auto-infer @effect(state) for functions with var-qualified type params
    if (!func_has_state) {
        bool has_var_param = false;
        for (const auto& binding : bindings) {
            if (binding.has_generic_type && binding.first_arg_qualifier == MQ::VAR) {
                has_var_param = true;
                break;
            }
        }
        if (has_var_param) {
            result.effect_state_inferred++;
            result.functions_with_inferred_state.push_back(func.name.name);
        }
    }

    // Scan function body
    const auto& body = func.body.get();
    scan_block(body.statements, bindings, func_has_state, source_file, result);
}

// ===========================================================================
// Block scanning
// ===========================================================================

void MutabilityEnforcementPass::scan_block(
    const std::vector<boost::spirit::x3::forward_ast<parser::ast::expression>>& statements,
    const std::vector<BindingInfo>& bindings,
    bool enclosing_has_effect_state,
    const std::string& source_file,
    MutabilityEnforcementResult& result
) {
    // Copy bindings so local declarations can be added
    std::vector<BindingInfo> local_bindings = bindings;
    for (const auto& stmt : statements) {
        scan_expression(stmt.get(), local_bindings, enclosing_has_effect_state,
                        source_file, result);
    }
}

// ===========================================================================
// Expression scanning
// ===========================================================================

void MutabilityEnforcementPass::scan_expression(
    const parser::ast::expression& expr,
    std::vector<BindingInfo>& bindings,
    bool enclosing_has_effect_state,
    const std::string& source_file,
    MutabilityEnforcementResult& result
) {
    meld::compat::visit([&](const auto& node) {
        using T = std::decay_t<decltype(node)>;

        // val declarations — register binding
        if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::val_declaration>>) {
            const auto& decl = node.get();
            if (decl.has_type_annotation) {
                bindings.push_back(make_binding(decl.name.name, decl.type_ann.get()));
            }
        }

        // var declarations — register binding
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::var_declaration>>) {
            const auto& decl = node.get();
            if (decl.has_type_annotation) {
                bindings.push_back(make_binding(decl.name.name, decl.type_ann.get()));
            }
        }

        // Function definitions — scan with own scope
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::function_definition>>) {
            scan_function(node.get(), source_file, result);
        }

        // Binary operations — detect x.method(args) pattern
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::binary_operation>>) {
            const auto& binop = node.get();
            if (binop.op == ".") {
                // Check if LHS is an identifier (the binding)
                const std::string* receiver_name = nullptr;
                meld::compat::visit([&](const auto& lhs) {
                    using L = std::decay_t<decltype(lhs)>;
                    if constexpr (std::is_same_v<L, parser::ast::identifier>) {
                        receiver_name = &lhs.name;
                    }
                }, binop.left.get());

                if (receiver_name) {
                    // Check if RHS is a function_call (method call)
                    meld::compat::visit([&](const auto& rhs) {
                        using R = std::decay_t<decltype(rhs)>;
                        if constexpr (std::is_same_v<R,
                                boost::spirit::x3::forward_ast<parser::ast::function_call>>) {
                            const auto& call = rhs.get();
                            const std::string& method = call.function_name.name;

                            if (is_mutating_method(method)) {
                                // Look up the binding
                                for (const auto& b : bindings) {
                                    if (b.name == *receiver_name && b.has_generic_type) {
                                        if (b.first_arg_qualifier == MQ::VAL) {
                                            emit_val_violation(method, source_file,
                                                               0, 0, result);
                                        } else if (b.first_arg_qualifier == MQ::VAR) {
                                            if (!enclosing_has_effect_state) {
                                                emit_effect_violation(method, source_file,
                                                                      0, 0, result);
                                            }
                                        }
                                        break;
                                    }
                                }
                            }
                        }
                    }, binop.right.get());
                }
            }
        }

        // Class definitions — scan methods
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::class_definition>>) {
            const auto& cls = node.get();
            for (const auto& method : cls.methods) {
                scan_function(method, source_file, result);
            }
        }

        // Struct definitions — scan methods
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::struct_definition>>) {
            const auto& s = node.get();
            for (const auto& method : s.methods) {
                scan_function(method, source_file, result);
            }
        }

        // Other nodes: no-op
        else {
            // Literals, identifiers, etc.
        }

    }, expr);
}

} // namespace meld::compiler
