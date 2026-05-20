#pragma once

#include "meld/manifest/core_types.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace meld::manifest {

/// Configuration for a sandboxed execution
struct SandboxConfig {
    std::vector<Effect> allowed_effects;
    std::vector<std::string> read_only_paths;
    std::vector<std::string> read_write_paths;
    std::vector<std::string> allowed_domains;
    std::filesystem::path working_directory;
    bool vfs_mode{false};  // Req 22.6: memory-only VFS for AI agent dry runs
};

/// Result of a sandboxed execution
struct SandboxResult {
    int exit_code{0};
    std::string stdout_output;
    std::string stderr_output;
    bool sandbox_failed{false};  // true = sandbox itself failed, not the process
};

/// Abstract sandbox provider interface (Req 10).
/// Stateless — each launch() call is independent.
class SandboxProvider {
public:
    virtual ~SandboxProvider() = default;

    /// Launch a command in a sandbox with the given configuration
    virtual SandboxResult launch(const SandboxConfig& config,
                                 const std::vector<std::string>& command) = 0;

    /// Get the provider name (e.g., "SRT", "Passthrough")
    virtual std::string name() const = 0;
};

/// Factory: create the best available provider for the host OS
std::unique_ptr<SandboxProvider> create_default_provider();

/// Passthrough provider (no sandboxing) — fallback
class PassthroughProvider : public SandboxProvider {
public:
    SandboxResult launch(const SandboxConfig& config,
                         const std::vector<std::string>& command) override;
    std::string name() const override { return "Passthrough"; }
};

}  // namespace meld::manifest
