#pragma once

#include "meld/manifest/core_types.hpp"

#include <string>
#include <vector>

namespace meld::manifest {

/// Diagnostic from bridge rule checking
struct BridgeDiagnostic {
    std::string symbol_name;
    std::string message;
    bool is_warning{false};  // true = defaulted to Full IO; false = error
};

/// Checks FFI functions for required @effect annotations (Req 3).
/// Untagged @extern("C") functions default to "Full IO" with a warning.
class BridgeRuleChecker {
public:
    /// Check an FFI function declaration.
    /// Returns a diagnostic if the function is untagged (defaulted to Full IO).
    std::optional<BridgeDiagnostic> check_ffi_function(
        const std::string& symbol_name,
        bool has_effect_annotation,
        EffectBitmask declared_effects) const;

    /// Process a list of FFI declarations and produce manifest entries + diagnostics.
    struct CheckResult {
        std::vector<SymbolEffectEntry> entries;
        std::vector<BridgeDiagnostic> diagnostics;
    };

    struct FfiDeclaration {
        std::string symbol_name;
        bool has_effect_annotation{false};
        EffectBitmask declared_effects;
        ResourceBounds bounds;
    };

    CheckResult check_all(const std::vector<FfiDeclaration>& declarations) const;
};

}  // namespace meld::manifest
