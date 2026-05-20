#pragma once

#include "meld/manifest/sandbox_provider.hpp"

#include <nlohmann/json.hpp>
#include <filesystem>
#include <string>
#include <vector>

namespace meld::manifest {

/// SRT-backed sandbox provider (Req 11, 13).
/// Maps Meld effects to SRT policy fields and invokes the SRT CLI.
class MeldSRTProvider : public SandboxProvider {
public:
    MeldSRTProvider() = default;

    SandboxResult launch(const SandboxConfig& config,
                         const std::vector<std::string>& command) override;
    std::string name() const override { return "SRT"; }

    /// Generate srt-settings.json content from a SandboxConfig
    static nlohmann::json generate_policy(const SandboxConfig& config);

    /// Check if SRT CLI is available on the system
    static bool is_srt_available();

    /// Set the SRT CLI path (default: search $PATH)
    void set_srt_path(const std::filesystem::path& path);

    /// Enable/disable sandboxing (--sandbox=off)
    void set_sandbox_enabled(bool enabled) { sandbox_enabled_ = enabled; }

private:
    std::filesystem::path srt_path_;
    bool sandbox_enabled_{true};
};

}  // namespace meld::manifest
