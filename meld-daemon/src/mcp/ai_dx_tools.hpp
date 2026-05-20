#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace meld::mcp {

// ============================================================================
// Deterministic execution configuration (Req 14)
// ============================================================================

/// Per-session deterministic execution config for MCP tool invocations.
struct DeterministicExecConfig {
    bool deterministic{false};          ///< Enable deterministic mode.
    uint64_t seed{42};                  ///< PRNG seed.
    uint64_t epoch{0};                  ///< Fixed epoch (0 = default 2025-01-01).
    uint64_t time_increment_ms{1};      ///< Ms to advance per time query.
};

// ============================================================================
// AI_DX MCP Tool Handlers (Reqs 11, 12, 13)
// ============================================================================

// --- get_module_specs (Req 11) ---

struct SpecEntry {
    std::string symbol_name;
    std::string action_source;
    std::string expected_result;
    std::vector<std::string> declared_effects;
    std::string verification_status;  // "verified" | "unverified"
};

struct ModuleSpecsResult {
    std::string module_name;
    std::string summary;           // @blueprint summary
    std::vector<std::string> rules; // @blueprint rules
    std::vector<SpecEntry> specs;
    std::vector<std::string> examples_fallback;  // When no spec blocks
    std::string error;             // SDF-formatted error if any
};

/// Retrieve @blueprint spec blocks for a module's exported symbols.
/// When `symbol` is non-empty, filter to that symbol only.
ModuleSpecsResult get_module_specs(const std::string& module_name,
                                   const std::string& version = "",
                                   const std::string& symbol = "");

// --- find_intent (Req 12) ---

struct IntentMatch {
    std::string symbol_name;
    std::string module_coordinate;
    std::string type_signature;
    std::string effect_profile;
    std::string srt_security_profile;
    float relevance_score{0.0f};
    std::string summary;
};

struct FindIntentResult {
    std::vector<IntentMatch> matches;
    std::string error;
};

/// Semantic search for symbols matching a natural language query.
FindIntentResult find_intent(const std::string& query,
                             const std::string& scope = "all",
                             size_t max_results = 10);

// --- refactor (Req 13) ---

struct AstTransformEntry {
    std::string op;        // "insert" | "replace" | "delete" | "rename" | "move"
    std::string selector;
    std::string payload;   // New code or new name
    std::string file;
};

struct DisambiguationCandidate {
    std::string symbol_name;
    std::string file;
    uint32_t line{0};
    std::string type_signature;
};

struct RefactorResult {
    std::vector<AstTransformEntry> transforms;
    std::vector<std::string> validation_diagnostics;
    std::vector<DisambiguationCandidate> disambiguation;  // Non-empty if ambiguous
    bool applied{false};  // true only when dry_run=false and apply succeeded
    std::string error;
};

/// Resolve a structured refactoring intent into AST_Transform objects.
RefactorResult refactor(const std::string& intent_type,
                        const std::string& target,
                        const std::string& params_json = "{}",
                        bool dry_run = true);

// ============================================================================
// execute_code — code execution with optional deterministic mode (Req 14)
// ============================================================================

struct ExecuteCodeResult {
    int exit_code{0};
    std::string stdout_output;
    std::string stderr_output;
    std::string deterministic_summary;  ///< Non-empty when deterministic mode was active.
    std::string error;
};

/// Execute compiled Meld code, optionally in deterministic mode.
/// When config.deterministic is true, time/entropy/scheduling are pinned.
ExecuteCodeResult execute_code(const std::string& binary_path,
                               const std::vector<std::string>& args = {},
                               const DeterministicExecConfig& config = {});

}  // namespace meld::mcp
