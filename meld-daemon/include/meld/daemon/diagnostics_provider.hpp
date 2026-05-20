#pragma once

#include "meld/daemon/semantic_model.hpp"

#include <optional>
#include <string>
#include <vector>

namespace meld::daemon {

/// Severity of a type diagnostic
enum class TypeDiagnosticSeverity { Error, Warning };

/// A type diagnostic with location, description, and optional fix suggestion
struct TypeDiagnostic {
    std::filesystem::path file;
    uint32_t line{0};
    uint32_t column{0};
    TypeDiagnosticSeverity severity{TypeDiagnosticSeverity::Error};
    std::string code;         // Stable error code, e.g. "E1001"
    std::string message;      // Human-readable description
    std::string suggestion;   // Optional fix suggestion
};

/// Result of a grammar conformance check
struct GrammarCheckResult {
    bool conforms{true};
    std::vector<TypeDiagnostic> diagnostics;
};

/// Result of type checking a file
struct TypeCheckResult {
    std::vector<TypeDiagnostic> errors;
    std::vector<TypeDiagnostic> warnings;
};

/// A suggestion for an undefined symbol (similar name + edit distance)
struct SymbolSuggestion {
    std::string name;
    int edit_distance{0};
};

/// Result of undefined symbol detection
struct UndefinedSymbolResult {
    std::string undefined_name;
    std::filesystem::path file;
    uint32_t line{0};
    uint32_t column{0};
    std::vector<SymbolSuggestion> suggestions;
};

/// A refinement constraint violation
struct RefinementViolation {
    std::string type_name;
    std::string predicate;       // The constraint predicate, e.g. "x > 0"
    std::string violating_value; // The value that violates the constraint
    std::filesystem::path file;
    uint32_t line{0};
    uint32_t column{0};
};

/// A dispatch ambiguity between multiple function overloads
struct DispatchAmbiguity {
    std::string function_name;
    std::vector<std::string> candidate_signatures;
    std::vector<std::string> argument_types;
    std::filesystem::path file;
    uint32_t line{0};
    uint32_t column{0};
};

/// Provides LSP diagnostics and type checking by reading from the SemanticModel.
/// Integrates grammar validation, type error reporting, undefined symbol detection,
/// refinement constraint validation, and dispatch ambiguity detection.
class DiagnosticsProvider {
public:
    explicit DiagnosticsProvider(const SemanticModel& model);
    ~DiagnosticsProvider() = default;

    // --- Grammar conformance (Req 17.1) ---

    /// Validate that source code conforms to the Meld grammar specification.
    GrammarCheckResult check_grammar(const std::filesystem::path& file,
                                     const std::string& source) const;

    // --- Type error reporting (Req 17.2) ---

    /// Run type checking on a file and return type errors with descriptions and fixes.
    TypeCheckResult check_types(const std::filesystem::path& file) const;

    /// Format a type mismatch error with a suggested fix.
    static TypeDiagnostic make_type_error(const std::filesystem::path& file,
                                          uint32_t line, uint32_t column,
                                          const std::string& expected_type,
                                          const std::string& actual_type,
                                          const std::string& context);

    // --- Undefined symbol detection (Req 17.3) ---

    /// Detect undefined symbols in a file and suggest similar names.
    std::vector<UndefinedSymbolResult> detect_undefined_symbols(
        const std::filesystem::path& file) const;

    /// Compute Levenshtein edit distance between two strings.
    static int edit_distance(const std::string& a, const std::string& b);

    /// Find symbols similar to the given name from available symbols.
    static std::vector<SymbolSuggestion> find_similar(
        const std::string& name,
        const std::vector<std::string>& available,
        int max_distance = 3);

    // --- Refinement constraint validation (Req 17.4) ---

    /// Validate refinement type constraints in a file.
    std::vector<RefinementViolation> check_refinement_constraints(
        const std::filesystem::path& file) const;

    /// Check if a value satisfies a refinement predicate.
    static bool satisfies_predicate(const std::string& predicate,
                                    const std::string& value);

    // --- Dispatch ambiguity detection (Req 17.5) ---

    /// Detect ambiguous multiple dispatch calls in a file.
    std::vector<DispatchAmbiguity> detect_dispatch_ambiguities(
        const std::filesystem::path& file) const;

    /// Check if two function signatures are ambiguous for given argument types.
    static bool signatures_ambiguous(const std::string& sig_a,
                                     const std::string& sig_b,
                                     const std::vector<std::string>& arg_types);

    // --- Aggregate diagnostics ---

    /// Run all diagnostic checks on a file and return combined results.
    std::vector<TypeDiagnostic> diagnose_file(const std::filesystem::path& file,
                                               const std::string& source) const;

private:
    const SemanticModel& model_;

    /// Collect all defined symbol names from a file's AST.
    std::vector<std::string> collect_defined_symbols(
        const std::shared_ptr<ASTNode>& node) const;

    /// Collect all referenced symbol names from a file's AST.
    std::vector<std::string> collect_referenced_symbols(
        const std::shared_ptr<ASTNode>& node) const;

    /// Collect all function signatures for a given name (for dispatch analysis).
    std::vector<std::string> collect_overloads(
        const std::string& function_name,
        const std::filesystem::path& file) const;

    static void collect_defined_symbols_impl(
        const std::shared_ptr<ASTNode>& node,
        std::vector<std::string>& out);

    static void collect_referenced_symbols_impl(
        const std::shared_ptr<ASTNode>& node,
        std::vector<std::string>& out);
};

}  // namespace meld::daemon
