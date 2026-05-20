#include "meld/manifest/policy_lifecycle.hpp"
#include "meld/manifest/srt_provider.hpp"

#include <chrono>
#include <fstream>
#include <random>
#include <sstream>

namespace meld::manifest {

PolicyLifecycleManager::~PolicyLifecycleManager() {
    std::lock_guard lock(mutex_);
    for (const auto& [_, path] : active_policies_) {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
}

std::filesystem::path PolicyLifecycleManager::generate_policy(const SandboxConfig& config) {
    namespace fs = std::filesystem;
    auto tmp = fs::temp_directory_path() / "meld-manifest";
    std::error_code ec;
    fs::create_directories(tmp, ec);

    auto uuid = generate_uuid();
    auto path = tmp / ("srt-settings-" + uuid + ".json");

    // Generate policy content
    auto policy = MeldSRTProvider::generate_policy(config);

    // Write with restrictive permissions (Req 12.6)
    {
        std::ofstream ofs(path);
        ofs << policy.dump(2);
    }
    // Set owner-read-only
    fs::permissions(path, fs::perms::owner_read, fs::perm_options::replace, ec);

    std::lock_guard lock(mutex_);
    active_policies_[uuid] = path;
    return path;
}

void PolicyLifecycleManager::cleanup_policy(const std::filesystem::path& policy_path) {
    std::error_code ec;
    std::filesystem::remove(policy_path, ec);

    std::lock_guard lock(mutex_);
    for (auto it = active_policies_.begin(); it != active_policies_.end(); ++it) {
        if (it->second == policy_path) {
            active_policies_.erase(it);
            break;
        }
    }
}

void PolicyLifecycleManager::cleanup_stale(const std::filesystem::path& tmp_dir) {
    namespace fs = std::filesystem;
    auto manifest_tmp = tmp_dir / "meld-manifest";
    if (!fs::exists(manifest_tmp)) return;

    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(manifest_tmp, ec)) {
        if (entry.path().extension() == ".json" &&
            entry.path().filename().string().find("srt-settings") != std::string::npos) {
            fs::remove(entry.path(), ec);
        }
    }
}

size_t PolicyLifecycleManager::active_count() const {
    std::lock_guard lock(mutex_);
    return active_policies_.size();
}

std::string PolicyLifecycleManager::generate_uuid() {
    static std::mt19937 rng(static_cast<unsigned>(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    std::uniform_int_distribution<uint32_t> dist;
    std::ostringstream oss;
    oss << std::hex << std::setfill('0')
        << std::setw(8) << dist(rng) << "-"
        << std::setw(4) << (dist(rng) & 0xFFFF) << "-"
        << std::setw(4) << (dist(rng) & 0xFFFF) << "-"
        << std::setw(4) << (dist(rng) & 0xFFFF) << "-"
        << std::setw(8) << dist(rng) << std::setw(4) << (dist(rng) & 0xFFFF);
    return oss.str();
}

}  // namespace meld::manifest
