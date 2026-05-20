#include "meld/manifest/bridge_rule.hpp"

namespace meld::manifest {

std::optional<BridgeDiagnostic> BridgeRuleChecker::check_ffi_function(
    const std::string& symbol_name,
    bool has_effect_annotation,
    EffectBitmask /*declared_effects*/) const {

    if (!has_effect_annotation) {
        return BridgeDiagnostic{
            symbol_name,
            "FFI function '" + symbol_name + "' has no @effect annotation; "
            "defaulting to Full IO. Add an explicit @effect annotation to "
            "restrict its permissions.",
            true  // warning
        };
    }
    return std::nullopt;
}

BridgeRuleChecker::CheckResult BridgeRuleChecker::check_all(
    const std::vector<FfiDeclaration>& declarations) const {

    CheckResult result;
    for (const auto& decl : declarations) {
        SymbolEffectEntry entry;
        entry.symbol_name = decl.symbol_name;
        entry.is_ffi = true;
        entry.bounds = decl.bounds;

        if (decl.has_effect_annotation) {
            entry.effects = decl.declared_effects;
        } else {
            // Default to Full IO (Req 3.2)
            entry.effects.set_all();
        }

        auto diag = check_ffi_function(decl.symbol_name,
                                        decl.has_effect_annotation,
                                        decl.declared_effects);
        if (diag) {
            result.diagnostics.push_back(std::move(*diag));
        }

        result.entries.push_back(std::move(entry));
    }
    return result;
}

}  // namespace meld::manifest
