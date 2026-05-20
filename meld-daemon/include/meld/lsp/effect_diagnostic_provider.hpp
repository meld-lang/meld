#pragma once

#include "ownership_lsp.hpp"
#include "meld/compiler/effect_checker.hpp"
#include "meld/parser/ast.hpp"
#include <string>
#include <vector>
#include <memory>

namespace meld {
namespace lsp {

// TASK 9.1: LSP diagnostic provider for implicit effect call errors and deprecation warnings.
// Uses the existing EffectChecker infrastructure to surface diagnostics to the editor.
// Requirements: 7.1, 7.2, 7.3, 4.3
class EffectDiagnosticProvider {
public:
    EffectDiagnosticProvider();
    explicit EffectDiagnosticProvider(std::shared_ptr<compiler::EffectChecker> checker);

    // Provide effect-related diagnostics for a file.
    // Runs the effect checker over the supplied function definitions and converts
    // errors/warnings into LSP Diagnostic objects.
    std::vector<Diagnostic> provide_diagnostics(
        const std::string& file_path,
        const std::vector<parser::ast::function_definition>& functions);

    // Provide diagnostics for a single function definition.
    std::vector<Diagnostic> provide_function_diagnostics(
        const parser::ast::function_definition& func_def);

    // Validate an implicit effect call and return any diagnostics.
    // Reports: unknown effect, unknown operation, missing @uses, type mismatch,
    //          top-level usage (outside function context).
    std::vector<Diagnostic> validate_implicit_effect_call(
        const parser::ast::implicit_effect_call& call,
        const parser::ast::function_definition* enclosing_function = nullptr);

    // Collect deprecation warnings for perform { ... } blocks in an expression tree.
    // Requirement 4.3
    std::vector<Diagnostic> collect_perform_deprecation_diagnostics(
        const parser::ast::expression& expr);

    // Access the underlying effect checker (e.g. for registration of custom effects).
    compiler::EffectChecker& effect_checker() { return *effect_checker_; }
    const compiler::EffectChecker& effect_checker() const { return *effect_checker_; }

private:
    std::shared_ptr<compiler::EffectChecker> effect_checker_;

    // Convert an EffectCheckResult into LSP diagnostics at the given range.
    std::vector<Diagnostic> convert_check_result(
        const compiler::EffectCheckResult& result,
        const Range& range) const;

    // Build a Range for an AST node (uses position_tagged info when available,
    // otherwise falls back to a zero-width range at line 0).
    Range range_for_node(const parser::ast::implicit_effect_call& node) const;
    Range range_for_node(const parser::ast::perform_expression& node) const;
    Range range_for_function(const parser::ast::function_definition& func_def) const;
};

} // namespace lsp
} // namespace meld
