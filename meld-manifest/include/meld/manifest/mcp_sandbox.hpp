#pragma once

#include "meld/manifest/manifest.hpp"
#include "meld/manifest/sandbox_provider.hpp"

#include <string>
#include <vector>

namespace meld::manifest {

/// Audit entry for MCP tool executions
struct McpAuditEntry {
    std::string tool_name;
    std::string target_module;
    EffectBitmask applied_effects;
    std::string outcome;  // "success", "failure", "sandbox-violation"
};

/// MCP tool hardening wrapper (Req 15).
class McpSandbox {
public:
    explicit McpSandbox(SandboxProvider& provider);

    /// Execute an MCP tool in a sandbox derived from the module's manifest
    SandboxResult execute_tool(const std::string& tool_name,
                               const Manifest& module_manifest,
                               const std::vector<std::string>& command);

    /// Execute across multiple modules — uses intersection of effect sets
    SandboxResult execute_tool_multi(const std::string& tool_name,
                                     const std::vector<Manifest>& manifests,
                                     const std::vector<std::string>& command);

    /// Check if sandbox disable is allowed (MELD_UNSAFE_NO_SANDBOX=1)
    static bool is_unsafe_override_set();

    /// Get audit log
    const std::vector<McpAuditEntry>& audit_log() const { return audit_log_; }

private:
    SandboxConfig config_from_manifest(const Manifest& m) const;

    SandboxProvider& provider_;
    std::vector<McpAuditEntry> audit_log_;
};

}  // namespace meld::manifest
