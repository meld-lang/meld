#pragma once

#include "meld/manifest/sandbox_provider.hpp"

#include <string>

namespace meld::manifest {

/// Target type for build-time isolation
enum class TargetType { ThirdParty, FirstParty };

/// Build-time sandbox configuration (Req 14).
class BuildSandbox {
public:
    /// Create a SandboxConfig for a build target
    static SandboxConfig config_for_target(TargetType type,
                                           const std::string& source_root,
                                           const std::string& output_dir);

    /// Create a SandboxConfig with per-target overrides
    static SandboxConfig config_with_overrides(TargetType type,
                                               const std::string& source_root,
                                               const std::string& output_dir,
                                               const SandboxConfig& overrides);
};

}  // namespace meld::manifest
