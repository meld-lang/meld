#pragma once

#include "meld/daemon/semantic_model.hpp"
#include "meld/daemon/deterministic_context.hpp"
#include "meld/daemon/deterministic_handlers.hpp"
#include "meld/daemon/vector_index.hpp"

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace meld::daemon {

/// Configuration parsed from CLI arguments
struct DaemonConfig {
    std::filesystem::path workspace;
    int timeout_seconds{0};       // 0 = no idle timeout
    bool sandbox_verbose{false};
    bool verbose{false};
};

/// Forward declarations for subsystem interfaces
class LspChannel;
class McpChannel;
class FileWatcher;
class IncrementalAnalyzer;
class BazelWorker;
class DependencyGraph;
class SandboxLifecycleManager;
class IntegrityVerifier;

/// The unified Meld daemon — owns the SemanticModel and all subsystems.
class MeldDaemon {
public:
    explicit MeldDaemon(DaemonConfig config);
    ~MeldDaemon();

    // Non-copyable
    MeldDaemon(const MeldDaemon&) = delete;
    MeldDaemon& operator=(const MeldDaemon&) = delete;

    /// Start the daemon: initialize subsystems, enter event loop
    void run();

    /// Request graceful shutdown
    void shutdown();

    /// Check if the daemon is running
    bool is_running() const { return running_.load(); }

    /// Access the shared SemanticModel (for channel adapters)
    SemanticModel& semantic_model() { return model_; }
    const SemanticModel& semantic_model() const { return model_; }

    /// Access configuration
    const DaemonConfig& config() const { return config_; }

    /// Activate deterministic context for agent-mode execution.
    /// Called when MCP tool has `deterministic: true` or CLI has `--agent-test`.
    void activate_deterministic(const DeterministicConfig& cfg = {});

    /// Deactivate deterministic context, restoring real handlers.
    void deactivate_deterministic();

    /// Check if deterministic context is active.
    bool is_deterministic() const;

    /// Get the active deterministic config (for echoing in MCP responses).
    std::optional<DeterministicConfig> deterministic_config() const;

    /// Register a callback for diagnostic updates (channels subscribe here)
    using DiagnosticCallback = std::function<void(const std::vector<Diagnostic>&)>;
    void on_diagnostics_updated(DiagnosticCallback cb);

    /// Notify all subscribers of updated diagnostics
    void publish_diagnostics(const std::vector<Diagnostic>& diags);

private:
    void initialize_subsystems();
    void cleanup_stale_resources();
    void run_event_loop();

    DaemonConfig config_;
    SemanticModel model_;
    std::atomic<bool> running_{false};
    std::atomic<bool> initialized_{false};

    std::mutex subscribers_mutex_;
    std::vector<DiagnosticCallback> diagnostic_subscribers_;

    // Indexing thread (joined on shutdown)
    std::thread indexing_thread_;

    // Subsystem pointers (initialized lazily)
    std::unique_ptr<LspChannel> lsp_channel_;
    std::unique_ptr<McpChannel> mcp_channel_;
    std::unique_ptr<FileWatcher> file_watcher_;
    std::unique_ptr<IncrementalAnalyzer> incremental_analyzer_;
    std::unique_ptr<BazelWorker> bazel_worker_;
    std::unique_ptr<DependencyGraph> dependency_graph_;
    std::unique_ptr<SandboxLifecycleManager> sandbox_manager_;
    std::unique_ptr<IntegrityVerifier> integrity_verifier_;

    // Vector index for intent-based search (AI_DX Req 9)
    std::unique_ptr<VectorIndex> vector_index_;

    // Deterministic context (activated on demand)
    std::unique_ptr<DeterministicContext> deterministic_ctx_;
    std::optional<DeterministicHandlerSet> deterministic_handlers_;
};

}  // namespace meld::daemon
