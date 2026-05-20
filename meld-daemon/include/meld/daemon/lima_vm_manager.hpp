#pragma once

#include "meld/daemon/semantic_model.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace meld::daemon {

/// Status of the Lima VM (Req 14.9).
struct VmStatus {
    bool running{false};
    size_t allocated_memory_mb{0};
    uint32_t active_microvm_count{0};
    uint32_t active_container_count{0};
};

/// Audit log entry for Lima lifecycle events (Req 14.11).
struct VmLifecycleEvent {
    std::string event;      // "start", "stop", "lock_acquire", "lock_release"
    std::string detail;
    std::chrono::system_clock::time_point timestamp;
};

/// Manages the Lima VM lifecycle on macOS with cross-daemon coordination
/// via POSIX file lock (Req 14).
///
/// On macOS: coordinates a single global `meld-vm` Lima instance across
/// N concurrent `meldd` daemon processes using a file lock at /tmp/meld-vmm.lock.
///
/// On Linux: all operations are no-ops since KVM and containerd are native.
class LimaVmManager {
public:
    explicit LimaVmManager(
        std::chrono::minutes idle_timeout = std::chrono::minutes(5));
    ~LimaVmManager();

    // Non-copyable
    LimaVmManager(const LimaVmManager&) = delete;
    LimaVmManager& operator=(const LimaVmManager&) = delete;

    /// Start the meld-vm instance if not already running (Req 14.1).
    /// Acquires file lock, starts VM if stopped, waits for KVM probe.
    /// Returns true if VM is running after this call.
    bool ensure_running();

    /// Graceful shutdown: release file lock, stop VM if last daemon (Req 14.6).
    void shutdown();

    /// Query current VM status (Req 14.9).
    VmStatus check_status() const;

    /// Periodic idle check: stop VM if last daemon and idle (Req 14.7).
    void check_idle_shutdown();

    /// Returns true on macOS, false on Linux (Req 14.8).
    static bool is_required();

    /// Access audit log entries.
    std::vector<VmLifecycleEvent> audit_log() const;

    /// Register a diagnostic callback for VM start failures (Req 14.10).
    using DiagnosticCallback = std::function<void(const Diagnostic&)>;
    void on_error(DiagnosticCallback cb);

    /// For testing: override the command executor.
    using CommandExecutor = std::function<int(const std::string& cmd, std::string& output)>;
    void set_command_executor(CommandExecutor exec);

    /// For testing: override the lock file path.
    void set_lock_path(const std::string& path);

    /// For testing: set whether other daemons are running.
    void set_other_daemons_running(bool running);

private:
    std::chrono::minutes idle_timeout_;
    int lock_fd_{-1};
    std::string lock_path_{"/tmp/meld-vmm.lock"};
    std::atomic<bool> vm_started_{false};
    mutable std::mutex mutex_;
    std::vector<VmLifecycleEvent> audit_log_;
    std::vector<DiagnosticCallback> error_callbacks_;
    bool other_daemons_running_{false};

    // Command executor (overridable for testing)
    CommandExecutor exec_;

    bool acquire_lock();
    void release_lock();
    bool is_vm_running() const;
    bool is_last_daemon() const;
    void start_vm();
    void stop_vm();
    void log_event(const std::string& event, const std::string& detail = "");
    void emit_diagnostic(const std::string& message, const std::string& rule_id);

    int default_exec(const std::string& cmd, std::string& output);
};

}  // namespace meld::daemon
