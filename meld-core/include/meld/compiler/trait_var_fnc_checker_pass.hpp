#pragma once

/// @file trait_var_fnc_checker_pass.hpp
/// @brief Semantic Analyzer — Trait `var fnc` Compatibility Checker Pass
///
/// Enforces that trait implementations match the `var fnc` declaration
/// of the trait's method signatures. Emits errors when:
///   - An implementation uses `var fnc` but the trait does not (E5003)
///   - An implementation omits `var fnc` but the trait requires it (E5004)
///
/// Requirements: 57.8

#include "meld/parser/ast.hpp"
#include <string>
#include <vector>

namespace meld::compiler {

// ---------------------------------------------------------------------------
// Trait var fnc diagnostic
// ---------------------------------------------------------------------------

/// Diagnostic produced by the trait var fnc checker pass.
struct TraitVarFncDiagnostic {
    enum class Level { Info, Warning, Error };

    Level level;
    std::string code;       ///< e.g., "E5003" or "E5004"
    std::string message;
    std::string source_file;
    size_t line = 0;
    size_t column = 0;
};

// ---------------------------------------------------------------------------
// Input structures
// ---------------------------------------------------------------------------

/// Signature of a single trait method (name + mutating flag).
struct TraitMethodSignature {
    std::string name;
    bool is_mutating = false;
};

/// A pairing of trait declaration and its implementation.
struct TraitImplPair {
    std::string trait_name;
    std::string impl_type_name;
    std::vector<TraitMethodSignature> trait_methods;
    std::vector<parser::ast::function_definition> impl_methods;
};

// ---------------------------------------------------------------------------
// TraitVarFncCheckerResult — output of the pass
// ---------------------------------------------------------------------------

struct TraitVarFncCheckerResult {
    bool success = true;
    std::vector<TraitVarFncDiagnostic> diagnostics;
    size_t methods_checked = 0;
    size_t errors_emitted = 0;
};

// ---------------------------------------------------------------------------
// TraitVarFncCheckerPass — the Semantic Analyzer pass
// ---------------------------------------------------------------------------

/// The Trait Var Fnc Checker Pass validates that trait implementations
/// match the `var fnc` declarations of the trait's method signatures.
///
/// For each trait method, the pass finds the corresponding implementation
/// method and checks that `is_mutating` matches between the two.
///
/// Errors:
///   E5003 — implementation uses `var fnc` but trait does not
///   E5004 — implementation omits `var fnc` but trait requires it
class TraitVarFncCheckerPass {
public:
    TraitVarFncCheckerPass();

    /// Run the pass over a list of trait-implementation pairs.
    /// @param pairs        Trait/impl pairs to check.
    /// @param source_file  Source file path for diagnostics.
    TraitVarFncCheckerResult run(
        const std::vector<TraitImplPair>& pairs,
        const std::string& source_file = ""
    );

private:
    /// Check a single trait-implementation pair.
    void check_pair(
        const TraitImplPair& pair,
        const std::string& source_file,
        TraitVarFncCheckerResult& result
    );

    /// Emit E5003: impl uses `var fnc` but trait does not.
    void emit_impl_adds_var_fnc(
        const std::string& method_name,
        const std::string& impl_type_name,
        const std::string& trait_name,
        const std::string& source_file,
        size_t line, size_t column,
        TraitVarFncCheckerResult& result
    );

    /// Emit E5004: impl omits `var fnc` but trait requires it.
    void emit_impl_missing_var_fnc(
        const std::string& method_name,
        const std::string& impl_type_name,
        const std::string& trait_name,
        const std::string& source_file,
        size_t line, size_t column,
        TraitVarFncCheckerResult& result
    );
};

} // namespace meld::compiler
