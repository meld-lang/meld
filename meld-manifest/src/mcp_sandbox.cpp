#include "meld/manifest/mcp_sandbox.hpp"

#include <algorithm>
#include <cstdlib>

namespace meld::manifest {

McpSandbox::McpSandbox(SandboxProvider& provider)
    : provider_(provider) {}

SandboxResult McpSandbox::execute_tool(
    const std::string& tool_name,
    const Manifest& module_manifest,
    const std::vector<std::string>& command) {

    auto config = config_from_manifest(module_manifest);
    auto result = provider_.launch(config, command);

    // Audit log (Req 15.5)
    EffectBitmask applied;
    for (const auto& sym : module_manifest.symbols) {
        applied = applied | sym.effects;
    }

    std::string outcome = result.sandbox_failed ? "sandbox-violation" :
                          (result.exit_code == 0 ? "success" : "failure");
    audit_log_.push_back({tool_name, module_manifest.project_name, applied, outcome});

    return result;
}

SandboxResult McpSandbox::execute_tool_multi(
    const std::string& tool_name,
    const std::vector<Manifest>& manifests,
    const std::vector<std::string>& command) {

    if (manifests.empty()) {
        return {1, "", "No manifests provided", true};
    }

    // Intersection of effect sets (Req 15.3)
    EffectBitmask intersection;
    intersection.set_all();
    for (const auto& m : manifests) {
        EffectBitmask module_effects;
        for (const auto& sym : m.symbols) {
            module_effects = module_effects | sym.effects;
        }
        intersection = intersection & module_effects;
    }

    SandboxConfig config;
    config.allowed_effects = intersection.to_vector();
    auto result = provider_.launch(config, command);

    std::string outcome = result.sandbox_failed ? "sandbox-violation" :
                          (result.exit_code == 0 ? "success" : "failure");
    audit_log_.push_back({tool_name, "multi-module", intersection, outcome});

    return result;
}

bool McpSandbox::is_unsafe_override_set() {
    const char* val = std::getenv("MELD_UNSAFE_NO_SANDBOX");
    return val != nullptr && std::string(val) == "1";
}

SandboxConfig McpSandbox::config_from_manifest(const Manifest& m) const {
    SandboxConfig config;
    EffectBitmask combined;
    for (const auto& sym : m.symbols) {
        combined = combined | sym.effects;
        // Merge resource bounds
        for (const auto& d : sym.bounds.allowed_domains)
            config.allowed_domains.push_back(d);
        for (const auto& p : sym.bounds.read_paths)
            config.read_only_paths.push_back(p);
        for (const auto& p : sym.bounds.write_paths)
            config.read_write_paths.push_back(p);
    }
    config.allowed_effects = combined.to_vector();
    return config;
}

}  // namespace meld::manifest
