#include "meld/lsp/effect_diagnostic_provider.hpp"
#include <sstream>

namespace meld {
namespace lsp {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

EffectDiagnosticProvider::EffectDiagnosticProvider()
    : effect_checker_(std::make_shared<compiler::EffectChecker>()) {}

EffectDiagnosticProvider::EffectDiagnosticProvider(std::shared_ptr<compiler::EffectChecker> checker)
    : effect_checker_(std::move(checker)) {}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

std::vector<Diagnostic> EffectDiagnosticProvider::provide_diagnostics(
    const std::string& /*file_path*/,
    const std::vector<parser::ast::function_definition>& functions) {

    std::vector<Diagnostic> all_diagnostics;

    for (const auto& func : functions) {
        auto func_diags = provide_function_diagnostics(func);
        all_diagnostics.insert(all_diagnostics.end(), func_diags.begin(), func_diags.end());
    }

    return all_diagnostics;
}

std::vector<Diagnostic> EffectDiagnosticProvider::provide_function_diagnostics(
    const parser::ast::function_definition& func_def) {

    std::vector<Diagnostic> diagnostics;

    // 1. Run the full effect check on the function (validates @uses, infers effects, etc.)
    auto check_result = effect_checker_->check_function(func_def);
    Range func_range = range_for_function(func_def);

    // Convert errors from the check result into LSP Error diagnostics.
    for (const auto& error : check_result.errors) {
        Diagnostic diag(func_range, DiagnosticSeverity::Error, error);
        diag.source = "meld-effect-checker";
        diag.code = "effect-error";
        diagnostics.push_back(diag);
    }

    // Convert warnings from the check result into LSP Warning diagnostics.
    for (const auto& warning : check_result.warnings) {
        DiagnosticSeverity severity = DiagnosticSeverity::Warning;
        std::string code = "effect-warning";

        // Tag deprecation warnings specifically.
        if (warning.find("Deprecated") != std::string::npos) {
            code = "effect-deprecated";
        }
        // Tag unused @uses warnings.
        if (warning.find("Unused effect declaration") != std::string::npos) {
            code = "effect-unused-uses";
        }

        Diagnostic diag(func_range, severity, warning);
        diag.source = "meld-effect-checker";
        diag.code = code;
        diagnostics.push_back(diag);
    }

    return diagnostics;
}

std::vector<Diagnostic> EffectDiagnosticProvider::validate_implicit_effect_call(
    const parser::ast::implicit_effect_call& call,
    const parser::ast::function_definition* enclosing_function) {

    std::vector<Diagnostic> diagnostics;
    Range call_range = range_for_node(call);

    // Requirement 7.3: Reject implicit effect calls at module top level.
    if (enclosing_function == nullptr) {
        Diagnostic diag(
            call_range,
            DiagnosticSeverity::Error,
            "Implicit effect call '" + call.effect_name.name + "." +
                call.operation_name.name +
                "(...)' cannot be used at module top level. "
                "Effect calls require a function context with a @uses annotation.");
        diag.source = "meld-effect-checker";
        diag.code = "effect-top-level";
        diagnostics.push_back(diag);
        return diagnostics;
    }

    // Requirement 7.1: Validate effect name and operation via the effect checker.
    std::vector<parser::ast::expression> args;
    args.reserve(call.arguments.size());
    for (const auto& arg : call.arguments) {
        args.push_back(arg.get());
    }

    auto validation = effect_checker_->validate_effect_operation(
        call.effect_name.name, call.operation_name.name, args);

    auto validation_diags = convert_check_result(validation, call_range);
    diagnostics.insert(diagnostics.end(), validation_diags.begin(), validation_diags.end());

    // Requirement 7.2 / 2.2 / 2.3: Check that the enclosing function declares the effect.
    if (enclosing_function != nullptr && validation.is_valid) {
        bool has_uses = enclosing_function->has_effects &&
                        !enclosing_function->effects_clause.empty();

        if (!has_uses) {
            // Requirement 2.3: No @uses annotation at all.
            Diagnostic diag(
                call_range,
                DiagnosticSeverity::Error,
                "Function '" + enclosing_function->name.name +
                    "' uses effect '" + call.effect_name.name +
                    "' but has no @uses annotation. "
                    "Add @uses(" + call.effect_name.name + ") to the function signature.");
            diag.source = "meld-effect-checker";
            diag.code = "effect-missing-uses";
            diagnostics.push_back(diag);
        } else {
            // Requirement 2.2: @uses exists but doesn't include this effect.
            bool effect_declared = false;
            for (const auto& eff : enclosing_function->effects_clause) {
                if (eff.name == call.effect_name.name) {
                    effect_declared = true;
                    break;
                }
            }
            if (!effect_declared) {
                std::ostringstream oss;
                oss << "Effect '" << call.effect_name.name
                    << "' is not listed in @uses annotation of function '"
                    << enclosing_function->name.name << "'. Add '"
                    << call.effect_name.name << "' to the @uses annotation.";
                Diagnostic diag(call_range, DiagnosticSeverity::Error, oss.str());
                diag.source = "meld-effect-checker";
                diag.code = "effect-undeclared";
                diagnostics.push_back(diag);
            }
        }
    }

    return diagnostics;
}

std::vector<Diagnostic> EffectDiagnosticProvider::collect_perform_deprecation_diagnostics(
    const parser::ast::expression& expr) {

    std::vector<Diagnostic> diagnostics;

    // Delegate to the effect checker's deprecation collection, then convert.
    compiler::EffectCheckResult result;
    effect_checker_->collect_perform_deprecation_warnings(expr, result);

    // Each warning from the checker becomes a Warning diagnostic.
    for (const auto& warning : result.warnings) {
        // Use a zero-range at line 0 as a fallback; in practice the caller
        // would refine the range based on AST position info.
        Range range(Position(0, 0), Position(0, 0));

        Diagnostic diag(range, DiagnosticSeverity::Warning, warning);
        diag.source = "meld-effect-checker";
        diag.code = "effect-deprecated";
        diagnostics.push_back(diag);
    }

    return diagnostics;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

std::vector<Diagnostic> EffectDiagnosticProvider::convert_check_result(
    const compiler::EffectCheckResult& result,
    const Range& range) const {

    std::vector<Diagnostic> diagnostics;

    for (const auto& error : result.errors) {
        Diagnostic diag(range, DiagnosticSeverity::Error, error);
        diag.source = "meld-effect-checker";
        diag.code = "effect-error";
        diagnostics.push_back(diag);
    }

    for (const auto& warning : result.warnings) {
        Diagnostic diag(range, DiagnosticSeverity::Warning, warning);
        diag.source = "meld-effect-checker";
        diag.code = "effect-warning";
        diagnostics.push_back(diag);
    }

    return diagnostics;
}

Range EffectDiagnosticProvider::range_for_node(
    const parser::ast::implicit_effect_call& /*node*/) const {
    // AST nodes are position_tagged; when full source positions are available
    // we would extract them here.  For now return a placeholder range.
    return Range(Position(0, 0), Position(0, 0));
}

Range EffectDiagnosticProvider::range_for_node(
    const parser::ast::perform_expression& /*node*/) const {
    return Range(Position(0, 0), Position(0, 0));
}

Range EffectDiagnosticProvider::range_for_function(
    const parser::ast::function_definition& /*func_def*/) const {
    return Range(Position(0, 0), Position(0, 0));
}

} // namespace lsp
} // namespace meld
