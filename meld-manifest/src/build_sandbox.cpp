#include "meld/manifest/build_sandbox.hpp"

namespace meld::manifest {

SandboxConfig BuildSandbox::config_for_target(
    TargetType type,
    const std::string& source_root,
    const std::string& output_dir) {

    SandboxConfig config;

    if (type == TargetType::ThirdParty) {
        // Req 14.1: Zero network, read-only source, write only to bazel-out
        config.allowed_effects = {Effect::FileSystemRead, Effect::FileSystemWrite};
        config.read_only_paths = {source_root};
        config.read_write_paths = {output_dir};
        // No network, no ProcessExec (Req 14.3)
    } else {
        // Req 14.4: Relaxed — filesystem reads within workspace, deny network
        config.allowed_effects = {Effect::FileSystemRead, Effect::FileSystemWrite, Effect::ProcessExec};
        config.read_only_paths = {source_root};
        config.read_write_paths = {output_dir};
    }

    return config;
}

SandboxConfig BuildSandbox::config_with_overrides(
    TargetType type,
    const std::string& source_root,
    const std::string& output_dir,
    const SandboxConfig& overrides) {

    auto config = config_for_target(type, source_root, output_dir);

    // Apply overrides (Req 14.5)
    if (!overrides.allowed_effects.empty()) {
        config.allowed_effects = overrides.allowed_effects;
    }
    if (!overrides.read_only_paths.empty()) {
        config.read_only_paths.insert(config.read_only_paths.end(),
                                       overrides.read_only_paths.begin(),
                                       overrides.read_only_paths.end());
    }
    if (!overrides.read_write_paths.empty()) {
        config.read_write_paths.insert(config.read_write_paths.end(),
                                        overrides.read_write_paths.begin(),
                                        overrides.read_write_paths.end());
    }
    if (!overrides.allowed_domains.empty()) {
        config.allowed_domains = overrides.allowed_domains;
    }

    return config;
}

}  // namespace meld::manifest
