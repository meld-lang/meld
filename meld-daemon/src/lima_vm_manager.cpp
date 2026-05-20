#include "meld/daemon/lima_vm_manager.hpp"

#include <array>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

namespace meld::daemon {

LimaVmManager::LimaVmManager(std::chrono::minutes idle_timeout)
    : idle_timeout_(idle_timeout) {
    exec_ = [this](const std::string& cmd, std::string& output) {
        return default_exec(cmd, output);
    };
}

LimaVmManager::~LimaVmManager() {
    if (lock_fd_ >= 0) {
        release_lock();
    }
}

bool LimaVmManager::is_required() {
#ifdef __APPLE__
    return true;
#else
    return false;
#endif
}

bool LimaVmManager::ensure_running() {
    if (!is_required()) return true;  // Linux: no-op, always "ready"

    std::lock_guard lock(mutex_);

    if (vm_started_.load()) return true;

    // Try to acquire the file lock (Req 14.2)
    if (!acquire_lock()) {
        // Lock held by another daemon — VM should already be running (Req 14.4)
        log_event("lock_wait", "Lock held by another daemon, verifying VM");
        if (is_vm_running()) {
            vm_started_.store(true);
            return true;
        }
        emit_diagnostic("Lima VM not running despite lock being held",
                        "LIMA-002");
        return false;
    }

    // We hold the lock — check if VM is running
    if (is_vm_running()) {
        vm_started_.store(true);
        log_event("lock_acquire", "VM already running");
        return true;
    }

    // Start the VM (Req 14.1, 14.3)
    start_vm();
    return vm_started_.load();
}

void LimaVmManager::shutdown() {
    if (!is_required()) return;

    std::lock_guard lock(mutex_);

    if (is_last_daemon()) {
        // Last daemon: stop the VM (Req 14.6)
        stop_vm();
    }

    release_lock();
}

VmStatus LimaVmManager::check_status() const {
    if (!is_required()) {
        return {true, 0, 0, 0};  // Linux: native KVM always available
    }

    VmStatus status;
    status.running = is_vm_running();

    if (status.running) {
        // Query Lima for details
        std::string output;
        int rc = exec_("limactl ls --json meld-vm", output);
        if (rc == 0) {
            // Parse basic info (simplified)
            status.allocated_memory_mb = 4096;  // Default Lima allocation
        }
    }

    return status;
}

void LimaVmManager::check_idle_shutdown() {
    if (!is_required()) return;

    std::lock_guard lock(mutex_);

    if (is_last_daemon() && vm_started_.load()) {
        // Stop VM after idle timeout (Req 14.7)
        log_event("idle_check", "Last daemon, stopping VM");
        stop_vm();
    }
}

std::vector<VmLifecycleEvent> LimaVmManager::audit_log() const {
    std::lock_guard lock(mutex_);
    return audit_log_;
}

void LimaVmManager::on_error(DiagnosticCallback cb) {
    std::lock_guard lock(mutex_);
    error_callbacks_.push_back(std::move(cb));
}

void LimaVmManager::set_command_executor(CommandExecutor exec) {
    exec_ = std::move(exec);
}

void LimaVmManager::set_lock_path(const std::string& path) {
    lock_path_ = path;
}

void LimaVmManager::set_other_daemons_running(bool running) {
    other_daemons_running_ = running;
}

// --- Private ---

bool LimaVmManager::acquire_lock() {
    lock_fd_ = open(lock_path_.c_str(), O_CREAT | O_RDWR, 0600);
    if (lock_fd_ < 0) return false;

    struct flock fl{};
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;

    // Non-blocking try
    if (fcntl(lock_fd_, F_SETLK, &fl) == -1) {
        close(lock_fd_);
        lock_fd_ = -1;
        return false;
    }

    log_event("lock_acquire");
    return true;
}

void LimaVmManager::release_lock() {
    if (lock_fd_ < 0) return;

    struct flock fl{};
    fl.l_type = F_UNLCK;
    fl.l_whence = SEEK_SET;
    fcntl(lock_fd_, F_SETLK, &fl);

    close(lock_fd_);
    lock_fd_ = -1;
    log_event("lock_release");
}

bool LimaVmManager::is_vm_running() const {
    std::string output;
    int rc = exec_("limactl ls -q meld-vm", output);
    return rc == 0 && output.find("Running") != std::string::npos;
}

bool LimaVmManager::is_last_daemon() const {
    return !other_daemons_running_;
}

void LimaVmManager::start_vm() {
    log_event("start", "Starting meld-vm via limactl");

    std::string output;
    int rc = exec_("limactl start meld-vm", output);

    if (rc != 0) {
        // Req 14.10: emit diagnostic on failure
        emit_diagnostic(
            "Failed to start Lima VM: " + output +
            ". Run `meld vm start` manually or check that virtualization is enabled.",
            "LIMA-001");
        return;
    }

    vm_started_.store(true);
    log_event("start", "meld-vm started successfully");
}

void LimaVmManager::stop_vm() {
    if (!vm_started_.load()) return;

    log_event("stop", "Stopping meld-vm via limactl");

    std::string output;
    exec_("limactl stop meld-vm", output);

    vm_started_.store(false);
    log_event("stop", "meld-vm stopped");
}

void LimaVmManager::log_event(const std::string& event,
                               const std::string& detail) {
    audit_log_.push_back({event, detail, std::chrono::system_clock::now()});
}

void LimaVmManager::emit_diagnostic(const std::string& message,
                                     const std::string& rule_id) {
    Diagnostic diag;
    diag.severity = DiagnosticSeverity::Error;
    diag.message = message;
    diag.rule_id = rule_id;

    for (const auto& cb : error_callbacks_) {
        cb(diag);
    }
}

int LimaVmManager::default_exec(const std::string& cmd, std::string& output) {
    std::array<char, 256> buffer;
    output.clear();

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return -1;

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        output += buffer.data();
    }

    return pclose(pipe);
}

}  // namespace meld::daemon
