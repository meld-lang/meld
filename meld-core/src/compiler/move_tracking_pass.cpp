/// @file move_tracking_pass.cpp
/// @brief Semantic Analyzer — Move Tracking Pass implementation
///
/// Tracks per-binding invalidation state. When std.mem.move(x) is
/// encountered, marks x as MOVED. Subsequent reads/writes to a moved
/// binding emit E4002. Conditional branches produce CONDITIONALLY_MOVED
/// when a binding is moved in some but not all branches.
///
/// Requirements: 4.2, 4.5, 7.2

#include "meld/compiler/move_tracking_pass.hpp"
#include "meld/compat/visit.hpp"
#include <algorithm>
#include <cassert>

namespace meld::compiler {

// ===========================================================================
// BindingScope
// ===========================================================================

BindingScope::BindingScope(BindingScope* parent)
    : parent_(parent) {}

void BindingScope::declare(const std::string& name) {
    states_[name] = BindingState::LIVE;
}

BindingState BindingScope::get_state(const std::string& name) const {
    auto it = states_.find(name);
    if (it != states_.end()) {
        return it->second;
    }
    if (parent_) {
        return parent_->get_state(name);
    }
    // Unknown binding — treat as LIVE (not our concern; type checker handles undeclared)
    return BindingState::LIVE;
}

bool BindingScope::has_binding(const std::string& name) const {
    if (states_.count(name)) return true;
    if (parent_) return parent_->has_binding(name);
    return false;
}

void BindingScope::mark_moved(const std::string& name) {
    states_[name] = BindingState::MOVED;
}

void BindingScope::mark_conditionally_moved(const std::string& name) {
    states_[name] = BindingState::CONDITIONALLY_MOVED;
}

std::unordered_map<std::string, BindingState> BindingScope::local_states() const {
    return states_;
}

// ===========================================================================
// MoveTrackingPass
// ===========================================================================

MoveTrackingPass::MoveTrackingPass() = default;

MoveTrackingResult MoveTrackingPass::run(
    const std::vector<parser::ast::expression>& expressions,
    const IntrinsicResolutionRegistry& registry,
    const std::string& source_file
) {
    MoveTrackingResult result;

    for (const auto& expr : expressions) {
        // Only analyze function definitions — move tracking is per-function
        if (auto* func_fwd = boost::get<
                boost::spirit::x3::forward_ast<parser::ast::function_definition>>(&expr)) {
            analyze_function(func_fwd->get(), registry, source_file, result);
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

bool MoveTrackingPass::is_move_call(
    const parser::ast::function_call& call,
    const IntrinsicResolutionRegistry& registry
) {
    const std::string& name = call.function_name.name;

    // Check direct name matches for move intrinsic
    // The registry maps memory_move → entity names like "std.mem.move", "move", "mem_move"
    auto entries = registry.lookup(std_mem::IntrinsicTag::memory_move);
    for (const auto* entry : entries) {
        if (entry->entity_name == name) return true;
    }

    // Also match qualified calls: std.mem.move, mem.move, move
    if (name == "move" || name == "mem_move" || name == "std.mem.move") {
        return registry.has_memory_move();
    }

    return false;
}

std::string MoveTrackingPass::extract_move_target(
    const parser::ast::function_call& call
) {
    // move() takes exactly one argument which should be an identifier
    if (call.arguments.size() != 1) return "";

    const auto& arg = call.arguments[0].get();
    if (auto* id = boost::get<parser::ast::identifier>(&arg)) {
        return id->name;
    }
    return "";
}

// ---------------------------------------------------------------------------
// Function analysis
// ---------------------------------------------------------------------------

void MoveTrackingPass::analyze_function(
    const parser::ast::function_definition& func,
    const IntrinsicResolutionRegistry& registry,
    const std::string& source_file,
    MoveTrackingResult& result
) {
    BindingScope scope;

    // Register function parameters as LIVE bindings
    for (const auto& param : func.parameters) {
        scope.declare(param.name.name);
        result.bindings_tracked++;
    }

    // Analyze the function body
    const auto& body = func.body.get();
    analyze_block(body.statements, scope, registry, source_file, result);
}

// ---------------------------------------------------------------------------
// Block analysis
// ---------------------------------------------------------------------------

void MoveTrackingPass::analyze_block(
    const std::vector<boost::spirit::x3::forward_ast<parser::ast::expression>>& statements,
    BindingScope& scope,
    const IntrinsicResolutionRegistry& registry,
    const std::string& source_file,
    MoveTrackingResult& result
) {
    for (const auto& stmt : statements) {
        analyze_expression(stmt.get(), scope, registry, source_file, result);
    }
}

// ---------------------------------------------------------------------------
// Expression analysis
// ---------------------------------------------------------------------------

void MoveTrackingPass::analyze_expression(
    const parser::ast::expression& expr,
    BindingScope& scope,
    const IntrinsicResolutionRegistry& registry,
    const std::string& source_file,
    MoveTrackingResult& result
) {
    meld::compat::visit([&](const auto& node) {
        using T = std::decay_t<decltype(node)>;

        // --- val/var declarations: register new binding, analyze initializer ---
        if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::val_declaration>>) {
            const auto& decl = node.get();
            // Check the initializer for use-after-move before declaring
            analyze_expression(decl.value.get(), scope, registry, source_file, result);
            scope.declare(decl.name.name);
            result.bindings_tracked++;
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::var_declaration>>) {
            const auto& decl = node.get();
            analyze_expression(decl.value.get(), scope, registry, source_file, result);
            scope.declare(decl.name.name);
            result.bindings_tracked++;
        }

        // --- function call: check for move() or use-after-move in args ---
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::function_call>>) {
            const auto& call = node.get();

            if (is_move_call(call, registry)) {
                // This is a move() call — mark the target as MOVED
                std::string target = extract_move_target(call);
                if (!target.empty() && scope.has_binding(target)) {
                    // Check if already moved
                    auto state = scope.get_state(target);
                    if (state == BindingState::MOVED) {
                        emit_use_after_move(target, false, source_file, 0, 0, result);
                    } else if (state == BindingState::CONDITIONALLY_MOVED) {
                        emit_use_after_move(target, true, source_file, 0, 0, result);
                    } else {
                        scope.mark_moved(target);
                        result.moves_detected++;
                    }
                }
            } else {
                // Not a move call — check arguments for use-after-move
                for (const auto& arg : call.arguments) {
                    analyze_expression(arg.get(), scope, registry, source_file, result);
                }
                // Check the function name itself (could be a moved binding used as callable)
                check_identifier_access(call.function_name, scope, source_file, result);
            }
        }

        // --- identifier: check for use-after-move ---
        else if constexpr (std::is_same_v<T, parser::ast::identifier>) {
            check_identifier_access(node, scope, source_file, result);
        }

        // --- binary operation: check both sides ---
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::binary_operation>>) {
            const auto& binop = node.get();

            // Assignment to a moved binding: check RHS first, then LHS
            if (binop.op == "=") {
                analyze_expression(binop.right.get(), scope, registry, source_file, result);
                // For assignment, the LHS is being written to — also an error if moved
                analyze_expression(binop.left.get(), scope, registry, source_file, result);
            } else {
                analyze_expression(binop.left.get(), scope, registry, source_file, result);
                analyze_expression(binop.right.get(), scope, registry, source_file, result);
            }
        }

        // --- block expression: new nested scope ---
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::block_expression>>) {
            const auto& block = node.get();
            BindingScope child_scope(&scope);
            analyze_block(block.statements, child_scope, registry, source_file, result);

            // Propagate move states from child to parent for bindings
            // that were declared in the parent scope
            for (const auto& [name, state] : child_scope.local_states()) {
                if (scope.has_binding(name) && state != BindingState::LIVE) {
                    if (state == BindingState::MOVED) {
                        scope.mark_moved(name);
                    } else if (state == BindingState::CONDITIONALLY_MOVED) {
                        scope.mark_conditionally_moved(name);
                    }
                }
            }
        }

        // --- match expression: conditional branches ---
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::match_expression>>) {
            const auto& match = node.get();

            // Analyze the matched value first
            analyze_expression(match.matched_value.get(), scope, registry, source_file, result);

            // Analyze each branch in its own scope
            std::vector<std::unique_ptr<BindingScope>> branch_scopes;
            std::vector<BindingScope*> branch_ptrs;

            for (const auto& case_branch : match.cases) {
                auto branch = std::make_unique<BindingScope>(&scope);

                // If the pattern binds a name, declare it
                if (!case_branch.case_pattern.binding_name.name.empty()) {
                    branch->declare(case_branch.case_pattern.binding_name.name);
                }

                analyze_expression(case_branch.result_expression.get(),
                                   *branch, registry, source_file, result);

                branch_ptrs.push_back(branch.get());
                branch_scopes.push_back(std::move(branch));
            }

            // Merge branch states
            if (!branch_ptrs.empty()) {
                merge_branch_scopes(branch_ptrs, scope);
            }
        }

        // --- return statement: check the returned expression ---
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::return_statement>>) {
            const auto& ret = node.get();
            if (ret.has_expression) {
                analyze_expression(ret.expr.get(), scope, registry, source_file, result);
            }
        }

        // --- unary operation: check operand ---
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::unary_operation>>) {
            const auto& unop = node.get();
            analyze_expression(unop.operand.get(), scope, registry, source_file, result);
        }

        // --- function definition (nested): analyze in its own scope ---
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::function_definition>>) {
            analyze_function(node.get(), registry, source_file, result);
        }

        // --- list expression: check elements ---
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::list_expression>>) {
            const auto& list = node.get();
            for (const auto& elem : list.elements) {
                analyze_expression(elem.get(), scope, registry, source_file, result);
            }
        }

        // --- Other node types: no-op for move tracking ---
        else {
            // Literals, type definitions, imports, etc. — nothing to track
        }

    }, expr);
}

// ---------------------------------------------------------------------------
// Identifier access checking
// ---------------------------------------------------------------------------

void MoveTrackingPass::check_identifier_access(
    const parser::ast::identifier& id,
    const BindingScope& scope,
    const std::string& source_file,
    MoveTrackingResult& result
) {
    if (!scope.has_binding(id.name)) return;

    auto state = scope.get_state(id.name);
    if (state == BindingState::MOVED) {
        emit_use_after_move(id.name, false, source_file, 0, 0, result);
    } else if (state == BindingState::CONDITIONALLY_MOVED) {
        emit_use_after_move(id.name, true, source_file, 0, 0, result);
    }
}

// ---------------------------------------------------------------------------
// Branch merging
// ---------------------------------------------------------------------------

void MoveTrackingPass::merge_branch_scopes(
    const std::vector<BindingScope*>& branches,
    BindingScope& target
) {
    if (branches.empty()) return;

    // Collect all binding names that were modified in any branch
    std::unordered_set<std::string> modified_names;
    for (const auto* branch : branches) {
        for (const auto& [name, state] : branch->local_states()) {
            if (state != BindingState::LIVE) {
                modified_names.insert(name);
            }
        }
    }

    // For each modified binding, check if it was moved in ALL branches or just some
    for (const auto& name : modified_names) {
        // Only merge bindings that exist in the target (parent) scope
        if (!target.has_binding(name)) continue;

        bool moved_in_all = true;
        bool moved_in_any = false;

        for (const auto* branch : branches) {
            auto state = branch->get_state(name);
            if (state == BindingState::MOVED || state == BindingState::CONDITIONALLY_MOVED) {
                moved_in_any = true;
            } else {
                moved_in_all = false;
            }
        }

        if (moved_in_all) {
            target.mark_moved(name);
        } else if (moved_in_any) {
            target.mark_conditionally_moved(name);
        }
    }
}

// ---------------------------------------------------------------------------
// Diagnostic emission
// ---------------------------------------------------------------------------

void MoveTrackingPass::emit_use_after_move(
    const std::string& binding_name,
    bool is_conditional,
    const std::string& source_file,
    size_t line,
    size_t column,
    MoveTrackingResult& result
) {
    std::string message;
    if (is_conditional) {
        message = "use of possibly moved value '" + binding_name + "'";
    } else {
        message = "use of moved value '" + binding_name + "'";
    }

    result.diagnostics.push_back(MoveTrackingDiagnostic{
        .level = MoveTrackingDiagnostic::Level::Error,
        .code = "E4002",
        .message = message,
        .source_file = source_file,
        .line = line,
        .column = column
    });

    result.errors_emitted++;
    result.success = false;
}

} // namespace meld::compiler
