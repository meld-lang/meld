#pragma once

#include "meld/daemon/semantic_model.hpp"

#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace meld::daemon {

/// An AST-level fix suggestion
struct AstPatch {
    std::string ast_selector;   // Target AST node
    std::string operation;      // "insert", "replace", "delete"
    std::string replacement;    // Replacement content (empty for delete)
};

/// Machine-readable context for agents
struct AgentContext {
    std::string rule_id;        // Stable diagnostic identifier
    std::string ast_selector;   // JSONPath-like path to AST node
    std::string context_hash;   // Hash of surrounding code context
    std::string version_hash;   // Module version hash
    std::optional<AstPatch> fix;  // Optional fix suggestion
};

/// Dual-voice response: human-readable + machine-readable
struct DualVoiceResponse {
    std::string message;         // Human-readable prose
    AgentContext agent_context;  // Machine-readable metadata
};

/// Formats diagnostics into dual-voice responses
class DualVoiceFormatter {
public:
    /// Convert a Diagnostic to a DualVoiceResponse
    static DualVoiceResponse format_diagnostic(const Diagnostic& diag);

    /// Convert a Diagnostic with a fix suggestion
    static DualVoiceResponse format_diagnostic_with_fix(const Diagnostic& diag,
                                                         const AstPatch& fix);

    /// Serialize for LSP channel (message as primary, agent_context in Diagnostic.data)
    static nlohmann::json to_lsp_json(const DualVoiceResponse& response);

    /// Serialize for MCP channel (both as top-level fields)
    static nlohmann::json to_mcp_json(const DualVoiceResponse& response);

private:
    static nlohmann::json agent_context_to_json(const AgentContext& ctx);
};

}  // namespace meld::daemon
