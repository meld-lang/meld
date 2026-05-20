#include "meld/compiler/uses_enforcement_pass.hpp"
#include <boost/spirit/home/x3/support/ast/variant.hpp>
#include <set>
#include <iostream>

namespace meld::compiler {

namespace x3 = boost::spirit::x3;
using namespace parser::ast;

// Visitor that finds implicit_effect_call nodes in an expression tree
class EffectCallFinder : public boost::static_visitor<void> {
public:
    EffectCallFinder(const std::string& func_name,
                     const std::set<std::string>& allowed,
                     const std::string& source_file,
                     UsesEnforcementResult& result)
        : func_name_(func_name), allowed_(allowed),
          source_file_(source_file), result_(result) {}

    void scan(const expression& expr) {
        boost::apply_visitor(*this, expr);
    }

    // The effect call we're looking for
    void operator()(const x3::forward_ast<implicit_effect_call>& call) const {
        const auto& c = call.get();
        const std::string& effect = c.effect_name.name;
        const std::string& op = c.operation_name.name;
        std::string full_path = effect + "." + op;

        // Wildcard: @uses(*) allows everything
        if (allowed_.count("*") > 0) return;
        // kernel.* is always allowed (internal bridge)
        if (effect == "kernel") return;

        // Hierarchical prefix matching:
        // @uses(Net) allows Net.TCP.connect, Net.HTTP.get, etc.
        // @uses(Net.TCP) allows Net.TCP.connect but not Net.HTTP.get
        // @uses(Net.TCP.connect) allows only that specific operation
        for (const auto& a : allowed_) {
            if (a == full_path) return;          // exact: Net.TCP.connect
            if (a == effect) return;             // parent: Net matches Net.*
            if (full_path.rfind(a + ".", 0) == 0) return;  // prefix: Net.TCP matches Net.TCP.connect
            // Wildcard suffix: Net.* matches Net.anything
            if (a.size() > 2 && a.back() == '*' && a[a.size()-2] == '.' &&
                full_path.rfind(a.substr(0, a.size()-1), 0) == 0) return;
        }

        result_.ok = false;
        result_.violations.push_back({
            func_name_, effect, op, source_file_,
            "Effect '" + full_path + "' is not declared in @uses for function '" + func_name_ + "'"
        });
    }

    // Recurse into val/var declarations
    void operator()(const x3::forward_ast<val_declaration>& decl) const {
        scan_expr(decl.get().value.get());
    }
    void operator()(const x3::forward_ast<var_declaration>& decl) const {
        scan_expr(decl.get().value.get());
    }

    // Recurse into function calls (check args)
    void operator()(const x3::forward_ast<function_call>& call) const {
        for (const auto& arg : call.get().arguments) {
            scan_expr(arg.get());
        }
    }

    // Recurse into return statements
    void operator()(const x3::forward_ast<return_statement>& ret) const {
        if (ret.get().has_expression) scan_expr(ret.get().expr.get());
    }

    // Catch-all for nodes we don't need to recurse into
    template<typename T>
    void operator()(const T&) const {}

private:
    void scan_expr(const expression& expr) const {
        boost::apply_visitor(*this, expr);
    }

    const std::string& func_name_;
    const std::set<std::string>& allowed_;
    const std::string& source_file_;
    UsesEnforcementResult& result_;
};

// ═══════════════════════════════════════════════════════════════════════════

UsesEnforcementResult UsesEnforcementPass::run(
    const std::vector<expression>& expressions,
    const std::string& source_file
) {
    UsesEnforcementResult result;

    for (const auto& expr : expressions) {
        auto* func = boost::get<x3::forward_ast<function_definition>>(&expr);
        if (func) {
            check_function(func->get(), source_file, result);
        }
    }

    return result;
}

void UsesEnforcementPass::check_function(
    const function_definition& func,
    const std::string& source_file,
    UsesEnforcementResult& result
) {
    const std::string& name = func.name.name;

    // main() is implicitly @uses(*) — all effects allowed
    if (name == "main") return;

    // Test functions are implicitly @uses(*)
    if (name.find("test") == 0) return;

    // Build the allowed effects set
    std::set<std::string> allowed;
    if (func.has_effects) {
        for (const auto& eff : func.effects_clause) {
            allowed.insert(eff.name);
        }
    }
    // No @uses annotation = no effects allowed (pure by default)

    // Scan the function body for effect calls
    EffectCallFinder finder(name, allowed, source_file, result);
    for (const auto& stmt : func.body.get().statements) {
        finder.scan(stmt.get());
    }
}

} // namespace meld::compiler
