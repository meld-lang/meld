#include "meld/daemon/sandbox_lifecycle.hpp"

#include <csignal>
#include <filesystem>
#include <fstream>

namespace meld::daemon {

SandboxLifecycleManager::~SandboxLifecycleManager() {
    terminate_all();
}

std::optional<int> SandboxLifecycleManager::launch(SandboxConfig config) {
    // 1. Generate ephemeral srt-settings.json
    auto policy_json = generate_policy_json(config);

    // Ensure temp directory exists
    namespace fs = std::filesystem;
    auto tmp_dir = fs::temp_directory_path() / "meld-daemon";
    std::error_code ec;
    fs::create_directories(tmp_dir, ec);

    // Write policy file
    config.policy_file = tmp_dir / ("srt-settings-" +
        config.binary_path.stem().string() + ".json");
    {
        std::ofstream ofs(config.policy_file);
        if (!ofs.is_open()) return std::nullopt;
        ofs << policy_json;
    }

    // 2. Delegate to SandboxProvider to launch the process
    // In production: fork+exec with sandbox constraints
    // For now, simulate with a placeholder PID
    int pid = static_cast<int>(std::hash<std::string>{}(
        config.binary_path.string()) % 100000 + 1000);

    SandboxedProcess proc;
    proc.pid = pid;
    proc.config = std::move(config);
    proc.started = std::chrono::steady_clock::now();

    {
        std::lock_guard lock(mutex_);
        active_processes_[pid] = std::move(proc);
    }

    return pid;
}

void SandboxLifecycleManager::on_process_exit(int pid) {
    std::lock_guard lock(mutex_);
    auto it = active_processes_.find(pid);
    if (it == active_processes_.end()) return;

    // Delete ephemeral policy file (Req 5.3)
    std::error_code ec;
    std::filesystem::remove(it->second.config.policy_file, ec);

    active_processes_.erase(it);
}

void SandboxLifecycleManager::terminate_all() {
    std::lock_guard lock(mutex_);
    for (auto& [pid, proc] : active_processes_) {
        // In production: kill(pid, SIGTERM)
        std::error_code ec;
        std::filesystem::remove(proc.config.policy_file, ec);
    }
    active_processes_.clear();
}

void SandboxLifecycleManager::enforce_timeouts() {
    auto now = std::chrono::steady_clock::now();
    std::vector<int> timed_out;

    {
        std::lock_guard lock(mutex_);
        for (const auto& [pid, proc] : active_processes_) {
            if (proc.config.timeout.count() == 0) continue;  // Unlimited
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - proc.started);
            if (elapsed >= proc.config.timeout) {
                timed_out.push_back(pid);
            }
        }
    }

    for (int pid : timed_out) {
        // In production: kill(pid, SIGKILL)
        on_process_exit(pid);
    }
}

void SandboxLifecycleManager::cleanup_stale_policies(const std::filesystem::path& tmp_dir) {
    namespace fs = std::filesystem;
    auto daemon_tmp = tmp_dir / "meld-daemon";
    if (!fs::exists(daemon_tmp)) return;

    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(daemon_tmp, ec)) {
        if (entry.path().extension() == ".json" &&
            entry.path().filename().string().find("srt-settings") != std::string::npos) {
            fs::remove(entry.path(), ec);
        }
    }
}

size_t SandboxLifecycleManager::active_count() const {
    std::lock_guard lock(mutex_);
    return active_processes_.size();
}

std::optional<SandboxedProcess> SandboxLifecycleManager::get_process(int pid) const {
    std::lock_guard lock(mutex_);
    auto it = active_processes_.find(pid);
    if (it == active_processes_.end()) return std::nullopt;
    return it->second;
}

std::string SandboxLifecycleManager::generate_policy_json(const SandboxConfig& config) const {
    // Generate srt-settings.json from the SandboxConfig
    std::string json = "{\n";
    json += "  \"binary\": \"" + config.binary_path.string() + "\",\n";
    json += "  \"manifest\": \"" + config.manifest_path.string() + "\",\n";
    json += "  \"allowed_effects\": [";
    for (size_t i = 0; i < config.allowed_effects.size(); ++i) {
        if (i > 0) json += ", ";
        json += "\"" + config.allowed_effects[i] + "\"";
    }
    json += "],\n";
    json += "  \"timeout_seconds\": " + std::to_string(config.timeout.count()) + "\n";
    json += "}\n";
    return json;
}

}  // namespace meld::daemon
