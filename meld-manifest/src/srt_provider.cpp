#include "meld/manifest/srt_provider.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>

namespace meld::manifest {

SandboxResult MeldSRTProvider::launch(
    const SandboxConfig& config,
    const std::vector<std::string>& command) {

    if (!sandbox_enabled_) {
        // --sandbox=off: run without sandboxing
        PassthroughProvider passthrough;
        return passthrough.launch(config, command);
    }

    if (!is_srt_available() && srt_path_.empty()) {
        // SRT not found — fallback with warning (Req 13.3)
        PassthroughProvider passthrough;
        auto result = passthrough.launch(config, command);
        result.stderr_output = "[WARNING] SRT not found. Running without sandbox. "
                               "Install SRT via: npm install -g @anthropic/srt\n"
                               + result.stderr_output;
        return result;
    }

    // Generate policy
    auto policy = generate_policy(config);

    // In production: write policy to temp file, invoke SRT CLI
    // For now, delegate to passthrough
    PassthroughProvider passthrough;
    return passthrough.launch(config, command);
}

nlohmann::json MeldSRTProvider::generate_policy(const SandboxConfig& config) {
    nlohmann::json policy;

    // Check which effects are allowed
    auto has_effect = [&](Effect e) {
        return std::find(config.allowed_effects.begin(),
                         config.allowed_effects.end(), e) != config.allowed_effects.end();
    };

    // Network policy (Req 11.1)
    if (has_effect(Effect::Network)) {
        policy["network"] = {
            {"mode", "allow"},
            {"allowedDomains", config.allowed_domains}
        };
    } else {
        policy["network"] = {{"mode", "deny"}};
    }

    // Filesystem policy (Req 11.2, 11.3)
    nlohmann::json fs_policy;
    std::vector<std::string> read_only = config.read_only_paths;
    // Always grant read-only access to Meld stdlib (Req 11.6)
    read_only.push_back("/usr/local/lib/meld");
    read_only.push_back("/usr/lib/meld");
    fs_policy["readOnly"] = read_only;

    if (has_effect(Effect::FileSystemWrite)) {
        fs_policy["readWrite"] = config.read_write_paths;
    } else {
        fs_policy["readWrite"] = nlohmann::json::array();
    }
    policy["filesystem"] = fs_policy;

    // Process execution policy (Req 11.4)
    if (has_effect(Effect::ProcessExec)) {
        policy["process"] = {{"mode", "allow"}};
    } else {
        policy["process"] = {{"mode", "deny"}};
    }

    return policy;
}

bool MeldSRTProvider::is_srt_available() {
    // Check if srt is on PATH
    return std::system("which srt > /dev/null 2>&1") == 0;
}

void MeldSRTProvider::set_srt_path(const std::filesystem::path& path) {
    srt_path_ = path;
}

}  // namespace meld::manifest
