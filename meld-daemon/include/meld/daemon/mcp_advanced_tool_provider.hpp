#pragma once

#include "meld/daemon/deterministic_context.hpp"
#include "meld/daemon/dependency_graph.hpp"
#include "meld/daemon/semantic_model.hpp"
#include "meld/daemon/tier0_sandbox.hpp"
#include "meld/daemon/vector_index.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace meld::daemon {

// ============================================================================
// Req 30: Structured Diagnostic Frame (SDF)
// ============================================================================

/// AST_Patch — a single AST-level fix suggestion.
struct ASTPatch {
    std::string selector;   ///< JSONPath-like AST selector for the target node.
    std::string action;     ///< "replace", "insert", "delete"
    std::string content;    ///< Replacement/insertion content.
};

/// Structured Diagnostic Frame — machine-parseable diagnostic (Req 30).
struct StructuredDiagnosticFrame {
    std::string rule_id;        ///< Unique stable diagnostic rule identifier (Req 30.1).
    std::string ast_selector;   ///< JSONPath-like path to relevant AST node (Req 30.2).
    std::string context_hash;   ///< Hash of surrounding code context (Req 30.3).
    std::string version_hash;   ///< Version hash of containing module (Req 30.4).
    std::string message;        ///< Human-readable description.
    std::string severity;       ///< "error", "warning", "info"
    std::optional<std::vector<ASTPatch>> fix;  ///< Optional fix patches (Req 30.5).
};

// ============================================================================
// Req 31: Proof-Carrying Failures — provenance trace
// ============================================================================

/// A single phase in a provenance trace (Origin, Escape, or Conflict).
struct ProvenancePhase {
    std::string phase;          ///< "Origin", "Escape", or "Conflict"
    std::string source_location;///< file:line:col
    std::string ast_selector;   ///< JSONPath-like AST selector
    std::string description;    ///< Human-readable explanation
};

/// Safety violation with full provenance trace (Req 31).
struct SafetyViolation {
    StructuredDiagnosticFrame sdf;                ///< Base SDF diagnostic.
    std::vector<ProvenancePhase> provenance_trace; ///< Origin → Escape → Conflict.
    std::string agent_context;                     ///< JSON agent_context with provenance (Req 31.6).
};

// ============================================================================
// Req 29: Named tool result types
// ============================================================================

/// Lifecycle query result (Req 29.2).
struct LifecycleInfo {
    std::string symbol;
    std::string ownership_state;  ///< "Hold[T]" or "View[T]"
    std::string scope;
    bool valid{true};
};

/// Effect trace result (Req 29.3).
struct EffectTrace {
    std::string expression;
    std::vector<std::string> required_permissions;
};

/// Version conflict resolution (Req 29.4).
struct VersionConflict {
    std::string dependency;
    std::vector<std::string> collisions;
    std::vector<std::string> resolution_strategies;
};

/// Dry-run patch result (Req 29.5).
struct DryRunResult {
    bool valid{false};
    std::vector<StructuredDiagnosticFrame> diagnostics;
};

/// Apply-patch result (Req 29.6).
struct ApplyPatchResult {
    bool success{false};
    size_t files_modified{0};
    std::vector<StructuredDiagnosticFrame> diagnostics;
};

/// Build diagnosis result (Req 29.7).
struct BuildDiagnosis {
    std::string target;
    std::vector<StructuredDiagnosticFrame> errors;
    std::vector<ASTPatch> fix_suggestions;
};

/// Closure verification result (Req 29.8).
struct ClosureVerification {
    std::vector<std::pair<std::string, std::string>> version_tree; ///< dep → version
    bool complete{false};
};

// ============================================================================
// Req 32: get_module_specs result types
// ============================================================================

/// A single spec pair from a @blueprint block (Req 32.2).
struct SpecEntry {
    std::string symbol_name;
    std::string action_source;
    std::string expected_result;
    std::vector<std::string> declared_effects;
    std::string verification_status;  ///< "verified" or "unverified"
};

/// Module specs result (Req 32).
struct ModuleSpecs {
    std::string module_coordinate;
    std::string summary;
    std::vector<std::string> rules;
    std::vector<SpecEntry> specs;
    std::vector<std::string> examples;  ///< Fallback examples (Req 32.7).
};

// ============================================================================
// Req 33: find_intent result types
// ============================================================================

/// A single intent match (Req 33.2).
struct IntentMatch {
    std::string symbol_name;
    std::string module_coordinate;
    std::string type_signature;
    std::string effect_profile;
    std::string srt_profile;
    double relevance_score{0.0};
    std::string summary;
};

// ============================================================================
// Req 34: refactor result types
// ============================================================================

/// AST_Transform object (Req 34.7).
struct ASTTransform {
    std::string intent_type;
    std::string target;
    std::string transform_json;  ///< Serialized AST_Transform.
    std::vector<StructuredDiagnosticFrame> validation_diagnostics;
};

/// Refactor result (Req 34).
struct RefactorResult {
    std::vector<ASTTransform> transforms;
    bool dry_run{true};
    bool ambiguous{false};
    std::vector<std::string> disambiguation_candidates;
};

// ============================================================================
// Req 36: Code Mode result types
// ============================================================================

/// API search result (Req 36.1).
struct ApiDefinition {
    std::string name;
    std::string kind;       ///< "struct", "interface", "function"
    std::string definition; ///< Meld interface syntax.
    std::vector<std::string> rpc_bindings;
};

/// Script execution result (Req 36.4).
struct ScriptExecutionResult {
    bool success{false};
    std::string output;
    std::vector<StructuredDiagnosticFrame> diagnostics;
    std::optional<DeterministicConfig> deterministic_config;
};

// ============================================================================
// McpAdvancedToolProvider — MCP tools for Req 29–36
// ============================================================================

/// Provides advanced MCP tool functionality:
/// - Named tools with SDF diagnostics (Req 29-30)
/// - Proof-carrying failures (Req 31)
/// - AI DX tools: get_module_specs, find_intent, refactor (Req 32-34)
/// - Deterministic execution mode (Req 35)
/// - Code Mode: search_api, execute_script (Req 36)
class McpAdvancedToolProvider {
public:
    McpAdvancedToolProvider(const SemanticModel& model,
                            const DependencyGraph& dep_graph,
                            const VectorIndex& vector_index);
    ~McpAdvancedToolProvider() = default;

    // --- Req 29: Named Tools ---

    /// Analyze memory safety violations for a file/module (Req 29.1).
    std::vector<SafetyViolation> analyze_safety(const std::string& target) const;

    /// Query ownership lifecycle for a symbol (Req 29.2).
    LifecycleInfo query_lifecycle(const std::string& symbol_ref) const;

    /// Trace required Effect Firewall permissions (Req 29.3).
    EffectTrace trace_effect(const std::string& expression) const;

    /// Resolve version conflicts for a dependency (Req 29.4).
    VersionConflict resolve_version_conflict(const std::string& dependency) const;

    /// Dry-run a patch on VFS without disk writes (Req 29.5).
    DryRunResult dry_run_patch(const std::vector<ASTPatch>& patches) const;

    /// Apply a patch with VFS verification then commit to disk (Req 29.6).
    ApplyPatchResult apply_patch(const std::vector<ASTPatch>& patches) const;

    /// Diagnose build errors for a target (Req 29.7).
    BuildDiagnosis diagnose_build(const std::string& target) const;

    /// Verify full Bazel-resolved version closure (Req 29.8).
    ClosureVerification verify_closure() const;

    // --- Req 32-34: AI DX Tools ---

    /// Get module specs from @blueprint blocks (Req 32).
    ModuleSpecs get_module_specs(const std::string& module_coord,
                                 const std::string& symbol = "") const;

    /// Intent-based symbol discovery (Req 33).
    std::vector<IntentMatch> find_intent(
        const std::string& query,
        const std::string& scope = "all",
        size_t max_results = 10) const;

    /// Intent-based refactoring (Req 34).
    RefactorResult refactor(const std::string& intent_type,
                            const std::string& target,
                            const std::string& params_json = "",
                            bool dry_run = true) const;

    // --- Req 35: Deterministic Execution ---

    /// Set deterministic mode for code-executing tools (Req 35).
    void set_deterministic(bool enabled,
                           const std::optional<DeterministicConfig>& config = std::nullopt);

    /// Check if deterministic mode is active.
    bool is_deterministic() const { return deterministic_enabled_; }

    /// Get the active deterministic config (if enabled).
    std::optional<DeterministicConfig> get_deterministic_config() const;

    // --- Req 36: Code Mode Tools ---

    /// Search the SemanticModel API surface (Req 36.1).
    std::vector<ApiDefinition> search_api(const std::string& query) const;

    /// Execute a Meld script in Tier0Sandbox (Req 36.3).
    ScriptExecutionResult execute_script(
        const std::string& source,
        uint32_t timeout_ms = 5000,
        uint32_t memory_limit_mb = 16,
        bool deterministic = false,
        const std::optional<DeterministicConfig>& det_config = std::nullopt) const;

    // --- SDF Helpers ---

    /// Create an SDF from a Diagnostic (Req 30).
    static StructuredDiagnosticFrame make_sdf(
        const std::string& rule_id,
        const std::string& ast_selector,
        const std::string& message,
        const std::string& severity,
        const std::string& context_hash = "",
        const std::string& version_hash = "");

    /// Compute a context hash for staleness detection (Req 30.3).
    static std::string compute_context_hash(const std::string& code_context);

    /// Check if a tool is static-analysis-only (Req 35.7).
    static bool is_static_analysis_tool(const std::string& tool_name);

private:
    const SemanticModel& model_;
    const DependencyGraph& dep_graph_;
    const VectorIndex& vector_index_;
    bool deterministic_enabled_{false};
    std::optional<DeterministicConfig> deterministic_config_;

    /// Find a named node in any indexed file.
    std::shared_ptr<ASTNode> find_node_any(const std::string& name) const;

    /// Build provenance trace for a safety violation (Req 31).
    std::vector<ProvenancePhase> build_provenance(
        const std::shared_ptr<ASTNode>& node,
        const std::filesystem::path& file) const;
};

}  // namespace meld::daemon
