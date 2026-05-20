#include "meld/manifest/sandbox_diagnostics.hpp"

#include <sstream>

namespace meld::manifest {

void SandboxDiagnostics::record_denial(SandboxDiagnostic diag) {
    ++stats_.total_denials;
    ++stats_.total_executions;
    denials_.push_back(std::move(diag));
}

void SandboxDiagnostics::record_success(const std::string& /*module*/, double policy_gen_ms) {
    ++stats_.total_executions;
    stats_.total_policy_gen_ms += policy_gen_ms;
}

std::string SandboxDiagnostics::format_denial(const SandboxDiagnostic& diag) {
    std::ostringstream oss;
    oss << "[" << diag.rule_id << "] Sandbox denied "
        << effect_to_string(diag.denied_effect)
        << " for module '" << diag.source_module << "'"
        << " (resource: " << diag.blocked_resource << ")"
        << "\n  Suggested fix: " << diag.suggested_fix;
    return oss.str();
}

}  // namespace meld::manifest
