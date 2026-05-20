#pragma once

#include "meld/daemon/semantic_model.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace meld::daemon {

/// Configuration for the Tier 0 in-process JIT sandbox (Req 13).
struct Tier0Config {
    size_t max_memory_bytes{16 * 1024 * 1024};                    // 16MB default
    std::chrono::milliseconds max_execution_time{5000};            // 5s default
};

/// Result of a Tier 0 script execution.
struct ScriptResult {
    bool success{false};
    std::string output;
    std::optional<std::vector<Diagnostic>> diagnostics;  // Compile or runtime errors
};

/// Read-only RPC bindings injected into the sandbox (Req 13.7, 13.8).
/// Scripts access the SemanticModel through these typed functions.
struct SandboxRpcBindings {
    std::function<std::shared_ptr<ASTNode>(const std::string&)> get_ast_node;
    std::function<std::optional<TypeInfo>(const std::string&)> query_symbol;
    std::function<std::vector<std::string>(const std::string&)> list_symbols;
    std::function<std::optional<EffectInfo>(const std::string&, uint32_t)> trace_effects;
    std::function<std::vector<Diagnostic>(const std::filesystem::path&)> get_diagnostics;
};

/// In-process JIT sandbox for ephemeral agent scripts (Req 13).
///
/// Accepts a Meld source string, JIT-compiles it in-memory via LLVM ORC JIT,
/// and executes it within an isolated context. The script has NO filesystem,
/// network, or raw I/O access — only read-only RPC bindings to the SemanticModel.
///
/// Each sandbox instance is independent with its own memory and time limits,
/// destroyed immediately after execution.
class Tier0Sandbox {
public:
    /// Construct a sandbox with read-only access to the SemanticModel.
    explicit Tier0Sandbox(const SemanticModel& model,
                          Tier0Config config = {});
    ~Tier0Sandbox();

    // Non-copyable
    Tier0Sandbox(const Tier0Sandbox&) = delete;
    Tier0Sandbox& operator=(const Tier0Sandbox&) = delete;

    /// Compile and execute a Meld source string in an isolated context.
    /// Returns the script's output or structured diagnostics on failure.
    /// Thread-safe: multiple execute() calls create independent sandbox instances.
    ScriptResult execute(const std::string& source);

    /// Access the configuration.
    const Tier0Config& config() const { return config_; }

private:
    const SemanticModel& model_;
    Tier0Config config_;

    /// Build read-only RPC bindings from the SemanticModel.
    SandboxRpcBindings build_rpc_bindings() const;

    /// JIT-compile the source and return compiled module handle, or diagnostics.
    ScriptResult compile(const std::string& source);

    /// Execute a compiled module with resource limits enforced.
    ScriptResult run_with_limits(const std::string& source,
                                 const SandboxRpcBindings& bindings);

    /// Check if memory limit is exceeded.
    bool check_memory_limit(size_t current_bytes) const;
};

}  // namespace meld::daemon
