#include "meld/effects/effect_checker.hpp"
#include "meld/parser/ast.hpp"

#include <algorithm>
#include <set>
#include <variant>
#include <boost/variant/apply_visitor.hpp>

namespace meld::effects {

namespace x3 = boost::spirit::x3;

// ---------------------------------------------------------------------------
// AST visitor helpers — extract effect names from perform() calls and
// @uses (effects_clause) annotations.
// ---------------------------------------------------------------------------

namespace {

/// Recursively walk an expression to find perform_expression and
/// implicit_effect_call nodes, collecting the effect names they reference.
class EffectCollector {
public:
    EffectCollector(const std::string& file_path,
                    const std::string& package_name,
                    const std::vector<std::string>& allowed,
                    std::vector<std::string>& effects_out,
                    std::vector<EffectViolation>& violations_out)
        : file_path_(file_path)
        , package_name_(package_name)
        , allowed_(allowed)
        , effects_(effects_out)
        , violations_(violations_out) {}

    void walk(const std::vector<parser::ast::expression>& exprs) {
        for (const auto& expr : exprs) {
            visit_expression(expr);
        }
    }

    void walk_stmts(const std::vector<x3::forward_ast<parser::ast::expression>>& stmts) {
        for (const auto& stmt : stmts) {
            visit_expression(stmt.get());
        }
    }

private:
    const std::string& file_path_;
    const std::string& package_name_;
    const std::vector<std::string>& allowed_;
    std::vector<std::string>& effects_;
    std::vector<EffectViolation>& violations_;

    void record_effect(const std::string& effect_name, size_t line, size_t col) {
        // Deduplicate in the effects list.
        if (std::find(effects_.begin(), effects_.end(), effect_name) == effects_.end()) {
            effects_.push_back(effect_name);
        }

        // Check against allow list — flag violation if not permitted.
        if (!allowed_.empty()) {
            bool permitted = std::find(allowed_.begin(), allowed_.end(), effect_name)
                             != allowed_.end();
            if (!permitted) {
                violations_.push_back(EffectViolation{
                    package_name_,
                    effect_name,
                    EffectSourceLocation{file_path_, line, col}
                });
            }
        }
    }

    // --- variant visitor dispatch -------------------------------------------

    void visit_expression(const parser::ast::expression& expr) {
        boost::apply_visitor([this](const auto& node) { this->visit_node(node); },
                   expr);
    }

    // --- concrete node visitors ----------------------------------------------

    // perform EffectName.operation(args)
    void visit_node(const x3::forward_ast<parser::ast::perform_expression>& node) {
        const auto& perf = node.get();
        record_effect(perf.effect_name.name, /*line=*/0, /*col=*/0);
        for (const auto& arg : perf.arguments) {
            visit_expression(arg.get());
        }
    }

    // Implicit effect call: EffectName.operation(args)
    void visit_node(const x3::forward_ast<parser::ast::implicit_effect_call>& node) {
        const auto& call = node.get();
        record_effect(call.effect_name.name, /*line=*/0, /*col=*/0);
        for (const auto& arg : call.arguments) {
            visit_expression(arg.get());
        }
    }

    // function_definition — also harvest effects_clause (@uses annotations)
    void visit_node(const x3::forward_ast<parser::ast::function_definition>& node) {
        const auto& func = node.get();
        if (func.has_effects) {
            for (const auto& eff_id : func.effects_clause) {
                record_effect(eff_id.name, /*line=*/0, /*col=*/0);
            }
        }
        // Walk the function body.
        if (func.body.get().statements.size() > 0) {
            walk_stmts(func.body.get().statements);
        }
    }

    // block_expression — recurse into statements
    void visit_node(const x3::forward_ast<parser::ast::block_expression>& node) {
        walk_stmts(node.get().statements);
    }

    // binary / unary — recurse into operands
    void visit_node(const x3::forward_ast<parser::ast::binary_operation>& node) {
        visit_expression(node.get().left.get());
        visit_expression(node.get().right.get());
    }

    void visit_node(const x3::forward_ast<parser::ast::unary_operation>& node) {
        visit_expression(node.get().operand.get());
    }

    // function_call — recurse into arguments
    void visit_node(const x3::forward_ast<parser::ast::function_call>& node) {
        const auto& call = node.get();
        for (const auto& arg : call.arguments) {
            visit_expression(arg.get());
        }
    }

    // val / var declarations — recurse into initialiser
    void visit_node(const x3::forward_ast<parser::ast::val_declaration>& node) {
        visit_expression(node.get().value.get());
    }

    void visit_node(const x3::forward_ast<parser::ast::var_declaration>& node) {
        visit_expression(node.get().value.get());
    }

    // lambda_expression — recurse into body
    void visit_node(const x3::forward_ast<parser::ast::lambda_expression>& node) {
        visit_expression(node.get().body.get());
    }

    // handle_expression — recurse into body and handler bodies
    void visit_node(const x3::forward_ast<parser::ast::handle_expression>& node) {
        const auto& handle = node.get();
        // Visit body statements
        for (const auto& stmt : handle.body.get().statements) {
            visit_expression(stmt.get());
        }
        // Visit each handler's method bodies
        for (const auto& handler : handle.handlers) {
            for (const auto& method : handler.methods) {
                for (const auto& stmt : method.body.get().statements) {
                    visit_expression(stmt.get());
                }
            }
        }
    }

    // inline_trait_impl is not a top-level expression variant — it lives inside handle_expression.
    // The handle_expression visitor above already recurses into handler method bodies.

    // Catch-all for leaf nodes and nodes we don't need to recurse into.
    template <typename T>
    void visit_node(const T& /*unused*/) {
        // No effect-relevant children — nothing to do.
    }
};

} // anonymous namespace

// ---------------------------------------------------------------------------
// EffectChecker implementation
// ---------------------------------------------------------------------------

std::vector<std::string> EffectChecker::collect_direct_effects(
    const std::vector<parser::ast::expression>& ast,
    const std::string& file_path,
    std::vector<EffectViolation>& violations_out,
    const std::string& package_name,
    const std::vector<std::string>& allowed) const
{
    std::vector<std::string> effects;
    EffectCollector collector(file_path, package_name, allowed,
                              effects, violations_out);
    collector.walk(ast);
    return effects;
}

void EffectChecker::propagate_transitive_effects(
    EffectNode& node,
    const std::map<std::string, size_t>& index) const
{
    std::set<std::string> transitive;

    for (const auto* dep : node.dependencies) {
        // Include the dependency's own direct effects.
        for (const auto& eff : dep->direct_effects) {
            transitive.insert(eff);
        }
        // Include the dependency's transitive effects.
        for (const auto& eff : dep->transitive_effects) {
            transitive.insert(eff);
        }
    }

    node.transitive_effects.assign(transitive.begin(), transitive.end());
}

EffectTree EffectChecker::build_effect_tree(
    const std::vector<ModuleInfo>& modules) const
{
    EffectTree tree;
    if (modules.empty()) {
        return tree;
    }

    tree.root_package = modules.front().package_name;

    // Build a name → index map so we can wire up dependency pointers.
    std::map<std::string, size_t> name_to_index;

    // Phase 1: create nodes and collect direct effects.
    tree.nodes.reserve(modules.size());
    for (size_t i = 0; i < modules.size(); ++i) {
        const auto& mod = modules[i];
        EffectNode node;
        node.package_name = mod.package_name;
        node.allowed_effects = mod.allowed_effects;

        if (mod.ast && !mod.ast->empty()) {
            node.direct_effects = collect_direct_effects(
                *mod.ast, mod.file_path, node.violations,
                mod.package_name, mod.allowed_effects);
        }

        name_to_index[mod.package_name] = i;
        tree.nodes.push_back(std::move(node));
    }

    // Phase 2: wire dependency pointers.
    for (size_t i = 0; i < modules.size(); ++i) {
        for (const auto& dep_name : modules[i].dependency_names) {
            auto it = name_to_index.find(dep_name);
            if (it != name_to_index.end()) {
                tree.nodes[i].dependencies.push_back(&tree.nodes[it->second]);
            }
        }
    }

    // Phase 3: propagate transitive effects (simple BFS from leaves).
    // Process nodes with no dependencies first, then work upward.
    std::vector<bool> processed(modules.size(), false);
    size_t remaining = modules.size();

    while (remaining > 0) {
        bool progress = false;
        for (size_t i = 0; i < modules.size(); ++i) {
            if (processed[i]) continue;

            // Check that all dependencies are already processed.
            bool deps_ready = true;
            for (const auto* dep : tree.nodes[i].dependencies) {
                auto dep_it = name_to_index.find(dep->package_name);
                if (dep_it != name_to_index.end() && !processed[dep_it->second]) {
                    deps_ready = false;
                    break;
                }
            }

            if (deps_ready) {
                propagate_transitive_effects(tree.nodes[i], name_to_index);
                processed[i] = true;
                --remaining;
                progress = true;
            }
        }

        // Safety valve: break cycles.
        if (!progress) break;
    }

    return tree;
}

} // namespace meld::effects
