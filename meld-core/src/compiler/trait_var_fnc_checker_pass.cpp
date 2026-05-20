/// @file trait_var_fnc_checker_pass.cpp
/// @brief Semantic Analyzer — Trait `var fnc` Compatibility Checker Pass
/// Requirements: 57.8

#include "meld/compiler/trait_var_fnc_checker_pass.hpp"

namespace meld::compiler {

TraitVarFncCheckerPass::TraitVarFncCheckerPass() = default;

TraitVarFncCheckerResult TraitVarFncCheckerPass::run(
    const std::vector<TraitImplPair>& pairs,
    const std::string& source_file
) {
    TraitVarFncCheckerResult result;
    for (const auto& pair : pairs) {
        check_pair(pair, source_file, result);
    }
    return result;
}

void TraitVarFncCheckerPass::check_pair(
    const TraitImplPair& pair,
    const std::string& source_file,
    TraitVarFncCheckerResult& result
) {
    for (const auto& trait_method : pair.trait_methods) {
        // Find the corresponding implementation method
        const parser::ast::function_definition* impl_method = nullptr;
        for (const auto& m : pair.impl_methods) {
            if (m.name.name == trait_method.name) {
                impl_method = &m;
                break;
            }
        }

        // If the method is not found in the impl, skip — other passes
        // handle missing method errors.
        if (!impl_method) {
            continue;
        }

        result.methods_checked++;

        if (impl_method->is_mutating && !trait_method.is_mutating) {
            // E5003: impl adds var fnc that trait doesn't declare
            emit_impl_adds_var_fnc(
                trait_method.name, pair.impl_type_name, pair.trait_name,
                source_file, 0, 0, result);
        } else if (!impl_method->is_mutating && trait_method.is_mutating) {
            // E5004: impl omits var fnc that trait requires
            emit_impl_missing_var_fnc(
                trait_method.name, pair.impl_type_name, pair.trait_name,
                source_file, 0, 0, result);
        }
    }
}

void TraitVarFncCheckerPass::emit_impl_adds_var_fnc(
    const std::string& method_name,
    const std::string& impl_type_name,
    const std::string& trait_name,
    const std::string& source_file,
    size_t line, size_t column,
    TraitVarFncCheckerResult& result
) {
    std::string message =
        "method '" + method_name + "' in '" + impl_type_name +
        "' is declared 'var fnc' but trait '" + trait_name +
        "' declares it without 'var' "
        "— implementation must match trait declaration";
    result.diagnostics.push_back(TraitVarFncDiagnostic{
        .level = TraitVarFncDiagnostic::Level::Error,
        .code = "E5003",
        .message = message,
        .source_file = source_file,
        .line = line,
        .column = column
    });
    result.errors_emitted++;
    result.success = false;
}

void TraitVarFncCheckerPass::emit_impl_missing_var_fnc(
    const std::string& method_name,
    const std::string& impl_type_name,
    const std::string& trait_name,
    const std::string& source_file,
    size_t line, size_t column,
    TraitVarFncCheckerResult& result
) {
    std::string message =
        "method '" + method_name + "' in '" + impl_type_name +
        "' is not declared 'var fnc' but trait '" + trait_name +
        "' requires it "
        "— add 'var' before 'fnc' to match trait declaration";
    result.diagnostics.push_back(TraitVarFncDiagnostic{
        .level = TraitVarFncDiagnostic::Level::Error,
        .code = "E5004",
        .message = message,
        .source_file = source_file,
        .line = line,
        .column = column
    });
    result.errors_emitted++;
    result.success = false;
}

} // namespace meld::compiler
