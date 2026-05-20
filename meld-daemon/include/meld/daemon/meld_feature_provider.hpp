#pragma once

#include "meld/daemon/semantic_model.hpp"

#include <optional>
#include <string>
#include <vector>

namespace meld::daemon {

/// Result of homoiconic analysis (Req 21.1).
struct HomoiconicAnalysis {
    std::string node_kind;        // AST node kind
    std::string runtime_type;     // Corresponding runtime type
    bool ast_runtime_match{false}; // Whether AST and runtime representations match
    std::string description;
};

/// Result of macro system analysis (Req 21.2).
struct MacroAnalysis {
    std::string macro_name;
    std::string macro_kind;       // "syntax", "derive", "attribute", "meta"
    std::vector<std::string> parameters;
    bool valid{false};
    std::string expansion_hint;   // Description of what the macro expands to
};

/// Result of refinement type evaluation (Req 21.3).
struct RefinementResult {
    std::string type_name;
    std::string base_type;
    std::string predicate;        // The logical predicate expression
    bool predicate_valid{false};  // Whether the predicate is well-formed
    bool constraint_satisfied{false};
    std::string violation_message;
};

/// Result of multiple dispatch resolution (Req 21.4).
struct DispatchResolution {
    std::string function_name;
    std::vector<std::string> argument_types;
    std::string resolved_overload;  // Signature of the resolved overload
    bool resolved{false};
    bool ambiguous{false};
    std::vector<std::string> candidates;  // All matching candidates
};

/// Result of tree initialization validation (Req 21.5).
struct TreeInitValidation {
    std::string type_name;
    bool valid_syntax{false};
    bool valid_nesting{false};
    std::vector<std::string> property_names;
    std::vector<std::string> errors;
};

/// LSP Meld-specific language feature provider (Req 21).
/// Provides analysis for homoiconic code, macros, refinement types,
/// multiple dispatch, and tree initialization syntax.
/// Reads from the shared SemanticModel.
class MeldFeatureProvider {
public:
    explicit MeldFeatureProvider(const SemanticModel& model);
    ~MeldFeatureProvider() = default;

    // --- Homoiconic analysis (Req 21.1) ---

    /// Analyze homoiconic code: understand AST ↔ runtime value relationship.
    HomoiconicAnalysis analyze_homoiconic(
        const std::filesystem::path& file,
        const std::string& node_name) const;

    // --- Macro system support (Req 21.2) ---

    /// Analyze a macro definition and provide language service support.
    MacroAnalysis analyze_macro(
        const std::filesystem::path& file,
        const std::string& macro_name) const;

    // --- Refinement type evaluation (Req 21.3) ---

    /// Evaluate a refinement type's logical predicates and constraints.
    RefinementResult evaluate_refinement(
        const std::filesystem::path& file,
        const std::string& type_name) const;

    // --- Multiple dispatch resolution (Req 21.4) ---

    /// Resolve multiple dispatch based on all argument types.
    DispatchResolution resolve_dispatch(
        const std::filesystem::path& file,
        const std::string& function_name,
        const std::vector<std::string>& argument_types) const;

    // --- Tree initialization validation (Req 21.5) ---

    /// Validate tree initialization syntax (constructor blocks, nested props).
    TreeInitValidation validate_tree_init(
        const std::filesystem::path& file,
        const std::string& type_name) const;

private:
    const SemanticModel& model_;

    /// Find an AST node by name in a file.
    std::shared_ptr<ASTNode> find_node(
        const std::filesystem::path& file,
        const std::string& name) const;

    /// Recursively search for a node by name.
    std::shared_ptr<ASTNode> find_node_recursive(
        const std::shared_ptr<ASTNode>& root,
        const std::string& name) const;

    /// Classify a node kind for homoiconic analysis.
    static std::string classify_runtime_type(const std::string& ast_kind);

    /// Check if a predicate expression is well-formed.
    static bool is_valid_predicate(const std::string& predicate);

    /// Extract property names from tree init children.
    static std::vector<std::string> extract_property_names(
        const std::shared_ptr<ASTNode>& node);
};

}  // namespace meld::daemon
