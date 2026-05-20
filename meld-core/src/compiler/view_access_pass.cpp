/// @file link_access_pass.cpp
/// @brief Semantic Analyzer — View Access Enforcement Pass implementation
///
/// For every member access `expr.field` or `expr.method()`, resolves
/// the type of `expr`. If the type is `View[T]`, emits E4001 unless
/// the access uses `?.` (safe navigation), or occurs inside an upgrade
/// scope (`if val` or `match`). Tracks upgrade scopes as a stack where
/// promoted bindings have type T (not View[T]).
///
/// Requirements: 121.1, 121.7, 125.1

#include "meld/compiler/view_access_pass.hpp"
#include "meld/compat/visit.hpp"
#include <algorithm>
#include <cassert>

namespace meld::compiler {

// ===========================================================================
// LinkBindingTracker
// ===========================================================================

LinkBindingTracker::LinkBindingTracker(LinkBindingTracker* parent)
    : parent_(parent) {}

void LinkBindingTracker::mark_as_link(const std::string& name) {
    link_bindings_.insert(name);
}

void LinkBindingTracker::mark_as_promoted(const std::string& name) {
    promoted_bindings_.insert(name);
}

bool LinkBindingTracker::is_link_binding(const std::string& name) const {
    if (promoted_bindings_.count(name)) return false;
    if (link_bindings_.count(name)) return true;
    if (parent_) return parent_->is_link_binding(name);
    return false;
}

bool LinkBindingTracker::is_promoted(const std::string& name) const {
    if (promoted_bindings_.count(name)) return true;
    if (parent_) return parent_->is_promoted(name);
    return false;
}

// ===========================================================================
// ViewAccessPass
// ===========================================================================

ViewAccessPass::ViewAccessPass() = default;

ViewAccessResult ViewAccessPass::run(
    const std::vector<parser::ast::expression>& expressions,
    const IntrinsicResolutionRegistry& registry,
    const std::string& source_file
) {
    ViewAccessResult result;

    for (const auto& expr : expressions) {
        // Only analyze function definitions — link access is per-function
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

bool ViewAccessPass::is_link_call(
    const parser::ast::function_call& call,
    const IntrinsicResolutionRegistry& registry
) {
    const std::string& name = call.function_name.name;
    // Match link(), std.mem.link(), mem.link()
    return (name == "link" || name == "std.mem.link" || name == "mem.link");
}

bool ViewAccessPass::is_link_type_annotation(const parser::ast::type_annotation& type) {
    const std::string& name = type.type_name.name;
    return (name == "Link" || name == "std.mem.Link" || name == "mem.Link");
}

bool ViewAccessPass::is_link_initializer(
    const parser::ast::expression& init_expr,
    const IntrinsicResolutionRegistry& registry
) {
    if (auto* call_fwd = boost::get<
            boost::spirit::x3::forward_ast<parser::ast::function_call>>(&init_expr)) {
        return is_link_call(call_fwd->get(), registry);
    }
    return false;
}

// ---------------------------------------------------------------------------
// Function analysis
// ---------------------------------------------------------------------------

void ViewAccessPass::analyze_function(
    const parser::ast::function_definition& func,
    const IntrinsicResolutionRegistry& registry,
    const std::string& source_file,
    ViewAccessResult& result
) {
    LinkBindingTracker tracker;
    std::vector<UpgradeScope> upgrade_stack;

    // Check function parameters for View[T] type annotations
    for (const auto& param : func.parameters) {
        if (is_link_type_annotation(param.type)) {
            tracker.mark_as_link(param.name.name);
            result.link_bindings_found++;
        }
    }

    // Analyze the function body
    const auto& body = func.body.get();
    analyze_block(body.statements, tracker, upgrade_stack, registry, source_file, result);
}

// ---------------------------------------------------------------------------
// Block analysis
// ---------------------------------------------------------------------------

void ViewAccessPass::analyze_block(
    const std::vector<boost::spirit::x3::forward_ast<parser::ast::expression>>& statements,
    LinkBindingTracker& tracker,
    std::vector<UpgradeScope>& upgrade_stack,
    const IntrinsicResolutionRegistry& registry,
    const std::string& source_file,
    ViewAccessResult& result
) {
    for (const auto& stmt : statements) {
        analyze_expression(stmt.get(), tracker, upgrade_stack, registry, source_file, result);
    }
}

// ---------------------------------------------------------------------------
// Expression analysis
// ---------------------------------------------------------------------------

void ViewAccessPass::analyze_expression(
    const parser::ast::expression& expr,
    LinkBindingTracker& tracker,
    std::vector<UpgradeScope>& upgrade_stack,
    const IntrinsicResolutionRegistry& registry,
    const std::string& source_file,
    ViewAccessResult& result
) {
    meld::compat::visit([&](const auto& node) {
        using T = std::decay_t<decltype(node)>;

        // --- val declaration: check if initializer is link() ---
        if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::val_declaration>>) {
            const auto& decl = node.get();
            // Analyze the initializer first
            analyze_expression(decl.value.get(), tracker, upgrade_stack,
                               registry, source_file, result);
            // If the initializer is a link() call, mark the binding as View[T]
            if (is_link_initializer(decl.value.get(), registry)) {
                tracker.mark_as_link(decl.name.name);
                result.link_bindings_found++;
            }
        }
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::var_declaration>>) {
            const auto& decl = node.get();
            analyze_expression(decl.value.get(), tracker, upgrade_stack,
                               registry, source_file, result);
            if (is_link_initializer(decl.value.get(), registry)) {
                tracker.mark_as_link(decl.name.name);
                result.link_bindings_found++;
            }
        }

        // --- binary operation: check for member access (dot or safe navigation) ---
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::binary_operation>>) {
            const auto& binop = node.get();
            if (binop.op == "." || binop.op == "?.") {
                check_member_access(binop, tracker, upgrade_stack, source_file, result);
            } else {
                analyze_expression(binop.left.get(), tracker, upgrade_stack,
                                   registry, source_file, result);
                analyze_expression(binop.right.get(), tracker, upgrade_stack,
                                   registry, source_file, result);
            }
        }

        // --- function call: check for link() and member access patterns ---
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::function_call>>) {
            const auto& call = node.get();
            // Check if this is a method call on a View[T] binding
            // Method calls like `link_ref.method()` appear as function_call
            // with a qualified name containing a dot
            const std::string& fname = call.function_name.name;
            auto dot_pos = fname.find('.');
            if (dot_pos != std::string::npos) {
                std::string receiver = fname.substr(0, dot_pos);
                if (tracker.is_link_binding(receiver) &&
                    !is_in_upgrade_scope(receiver, upgrade_stack)) {
                    emit_link_direct_access(receiver, source_file, 0, 0, result);
                }
            }
            // Analyze arguments
            for (const auto& arg : call.arguments) {
                analyze_expression(arg.get(), tracker, upgrade_stack,
                                   registry, source_file, result);
            }
        }

        // --- match expression: handle upgrade scopes via on<some>/on<none> ---
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::match_expression>>) {
            const auto& match = node.get();

            // Analyze the matched value
            analyze_expression(match.matched_value.get(), tracker, upgrade_stack,
                               registry, source_file, result);

            // Check if the matched value is a View[T] binding
            std::string matched_link_name;
            if (auto* id = boost::get<parser::ast::identifier>(&match.matched_value.get())) {
                if (tracker.is_link_binding(id->name)) {
                    matched_link_name = id->name;
                }
            }

            for (const auto& case_branch : match.cases) {
                LinkBindingTracker child_tracker(&tracker);

                // If matching on a View[T] and the pattern binds a name,
                // that binding is promoted (type T, not View[T])
                if (!matched_link_name.empty() &&
                    !case_branch.case_pattern.binding_name.name.empty()) {
                    const std::string& bound = case_branch.case_pattern.binding_name.name;

                    // on<some> branch: the bound variable is promoted
                    UpgradeScope scope;
                    scope.link_binding = matched_link_name;
                    scope.promoted_binding = bound;
                    scope.scope_depth = static_cast<uint32_t>(upgrade_stack.size());
                    upgrade_stack.push_back(scope);
                    child_tracker.mark_as_promoted(bound);
                    result.upgrade_scopes_entered++;

                    analyze_expression(case_branch.result_expression.get(),
                                       child_tracker, upgrade_stack,
                                       registry, source_file, result);

                    upgrade_stack.pop_back();
                } else {
                    // on<none> or non-Link match branch
                    analyze_expression(case_branch.result_expression.get(),
                                       child_tracker, upgrade_stack,
                                       registry, source_file, result);
                }
            }
        }

        // --- block expression: new nested scope ---
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::block_expression>>) {
            const auto& block = node.get();
            LinkBindingTracker child_tracker(&tracker);
            analyze_block(block.statements, child_tracker, upgrade_stack,
                          registry, source_file, result);
        }

        // --- return statement: check the returned expression ---
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::return_statement>>) {
            const auto& ret = node.get();
            if (ret.has_expression) {
                analyze_expression(ret.expr.get(), tracker, upgrade_stack,
                                   registry, source_file, result);
            }
        }

        // --- unary operation: check operand ---
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::unary_operation>>) {
            const auto& unop = node.get();
            analyze_expression(unop.operand.get(), tracker, upgrade_stack,
                               registry, source_file, result);
        }

        // --- nested function definition ---
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::function_definition>>) {
            analyze_function(node.get(), registry, source_file, result);
        }

        // --- list expression: check elements ---
        else if constexpr (std::is_same_v<T,
                boost::spirit::x3::forward_ast<parser::ast::list_expression>>) {
            const auto& list = node.get();
            for (const auto& elem : list.elements) {
                analyze_expression(elem.get(), tracker, upgrade_stack,
                                   registry, source_file, result);
            }
        }

        // --- Other node types: no-op for link access checking ---
        else {
            // Literals, identifiers (without member access), type defs, imports, etc.
        }

    }, expr);
}

// ---------------------------------------------------------------------------
// Member access checking
// ---------------------------------------------------------------------------

void ViewAccessPass::check_member_access(
    const parser::ast::binary_operation& binop,
    const LinkBindingTracker& tracker,
    const std::vector<UpgradeScope>& upgrade_stack,
    const std::string& source_file,
    ViewAccessResult& result
) {
    // The LHS of a dot or ?. operation should be an identifier (the receiver)
    if (auto* id = boost::get<parser::ast::identifier>(&binop.left.get())) {
        const std::string& receiver_name = id->name;

        // Check if the receiver is a View[T] binding
        if (tracker.is_link_binding(receiver_name)) {
            // Safe navigation (?.) is always allowed on View[T]
            if (is_safe_navigation_access(binop)) {
                return;
            }
            // Check if we're inside an upgrade scope for this binding
            if (!is_in_upgrade_scope(receiver_name, upgrade_stack)) {
                emit_link_direct_access(receiver_name, source_file, 0, 0, result);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Safe navigation checking
// ---------------------------------------------------------------------------

bool ViewAccessPass::is_safe_navigation_access(
    const parser::ast::binary_operation& binop
) {
    return binop.op == "?.";
}

// ---------------------------------------------------------------------------
// Upgrade scope checking
// ---------------------------------------------------------------------------

bool ViewAccessPass::is_in_upgrade_scope(
    const std::string& binding_name,
    const std::vector<UpgradeScope>& upgrade_stack
) {
    for (const auto& scope : upgrade_stack) {
        // The original link binding is being accessed inside its upgrade scope
        // This is allowed because the upgrade scope has promoted it
        if (scope.link_binding == binding_name) {
            return true;
        }
        // The promoted binding is always allowed (it has type T, not View[T])
        if (scope.promoted_binding == binding_name) {
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Diagnostic emission
// ---------------------------------------------------------------------------

void ViewAccessPass::emit_link_direct_access(
    const std::string& binding_name,
    const std::string& source_file,
    size_t line,
    size_t column,
    ViewAccessResult& result
) {
    std::string message =
        "View[T] must be accessed via '?.' (safe navigation), "
        "'if val' (upgrade), or 'match' — direct access on View[T] is not "
        "allowed because the referenced object may have been deallocated";

    result.diagnostics.push_back(ViewAccessDiagnostic{
        .level = ViewAccessDiagnostic::Level::Error,
        .code = "E4001",
        .message = message,
        .source_file = source_file,
        .line = line,
        .column = column
    });

    result.direct_access_errors++;
    result.success = false;
}

} // namespace meld::compiler
