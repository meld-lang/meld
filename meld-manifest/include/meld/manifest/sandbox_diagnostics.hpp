#pragma once

#include "meld/manifest/core_types.hpp"

#include <string>
#include <vector>

namespace meld::manifest {

/// Structured diagnostic for sandbox denials (Req 16)
struct SandboxDiagnostic {
    std::string rule_id;
    std::string ast_selector;
    std::string context_hash;
    Effect denied_effect;
    std::string blocked_resource;
    std::string source_module;
    std::string suggested_fix;
};

/// Sandbox execution statistics
struct SandboxStats {
    size_t total_executions{0};
    size_t total_denials{0};
    double total_policy_gen_ms{0.0};
};

/// Sandbox diagnostics and observability (Req 16).
class SandboxDiagnostics {
public:
    /// Record a sandbox denial
    void record_denial(SandboxDiagnostic diag);

    /// Record a successful execution
    void record_success(const std::string& module, double policy_gen_ms);

    /// Get all denial diagnostics
    const std::vector<SandboxDiagnostic>& denials() const { return denials_; }

    /// Get statistics summary
    SandboxStats stats() const { return stats_; }

    /// Format a denial as a human-readable string
    static std::string format_denial(const SandboxDiagnostic& diag);

    /// Set verbose mode
    void set_verbose(bool v) { verbose_ = v; }
    bool verbose() const { return verbose_; }

private:
    std::vector<SandboxDiagnostic> denials_;
    SandboxStats stats_;
    bool verbose_{false};
};

}  // namespace meld::manifest
