#include "meld/manifest/manifest.hpp"
#include "meld/manifest/sandbox_diagnostics.hpp"
#include "meld/manifest/sandbox_provider.hpp"
#include "meld/manifest/srt_provider.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <algorithm>

namespace meld::manifest {
namespace {

TEST(SandboxEnforcementTest, ModuleWithFileSystemAllowed) {
    // Module declares @uses(file_system) → sandbox allows filesystem
    SandboxConfig config;
    config.allowed_effects = {Effect::FileSystemRead, Effect::FileSystemWrite};
    config.read_only_paths = {"/data"};
    config.read_write_paths = {"/tmp"};

    auto policy = MeldSRTProvider::generate_policy(config);
    ASSERT_TRUE(policy.contains("filesystem"));

    // Filesystem should be allowed
    auto& fs = policy["filesystem"];
    EXPECT_FALSE(fs["readOnly"].empty());
    EXPECT_FALSE(fs["readWrite"].empty());
}

TEST(SandboxEnforcementTest, ModuleWithNoEffectsDeniesNetwork) {
    // Module with no effects → sandbox denies network
    SandboxConfig config;
    // No allowed_effects

    auto policy = MeldSRTProvider::generate_policy(config);

    if (policy.contains("network")) {
        EXPECT_EQ(policy["network"]["mode"], "deny");
    }
}

TEST(SandboxEnforcementTest, DenialProducesStructuredDiagnostic) {
    SandboxDiagnostics diags;

    SandboxDiagnostic denial;
    denial.rule_id = "SANDBOX-NET-001";
    denial.ast_selector = "module::my_app";
    denial.context_hash = "ctx_integration";
    denial.denied_effect = Effect::Network;
    denial.blocked_resource = "api.example.com:443";
    denial.source_module = "my_app";
    denial.suggested_fix = "Add @uses(network) to module declaration";

    diags.record_denial(denial);

    EXPECT_EQ(diags.denials().size(), 1u);
    auto formatted = SandboxDiagnostics::format_denial(denial);
    EXPECT_NE(formatted.find("SANDBOX-NET-001"), std::string::npos);
    EXPECT_NE(formatted.find("@uses(network)"), std::string::npos);
}

TEST(SandboxEnforcementTest, PolicyReflectsManifestEffects) {
    // Build a manifest with specific effects, derive sandbox config, verify policy
    Manifest m;
    m.format_version = 1;
    m.project_name = "enforcement-test";
    m.project_version = "1.0.0";
    m.code_hash = "abc";

    SymbolEffectEntry entry;
    entry.symbol_name = "main";
    entry.effects.set(Effect::Network);
    entry.effects.set(Effect::FileSystemRead);
    entry.bounds.allowed_domains = {"api.example.com"};
    entry.bounds.read_paths = {"/config"};
    m.symbols.push_back(entry);

    // Derive config from manifest symbols
    SandboxConfig config;
    for (const auto& sym : m.symbols) {
        for (auto eff : sym.effects.to_vector()) {
            if (std::find(config.allowed_effects.begin(),
                          config.allowed_effects.end(), eff)
                == config.allowed_effects.end()) {
                config.allowed_effects.push_back(eff);
            }
        }
        config.allowed_domains.insert(config.allowed_domains.end(),
                                       sym.bounds.allowed_domains.begin(),
                                       sym.bounds.allowed_domains.end());
        config.read_only_paths.insert(config.read_only_paths.end(),
                                       sym.bounds.read_paths.begin(),
                                       sym.bounds.read_paths.end());
    }

    auto policy = MeldSRTProvider::generate_policy(config);
    EXPECT_TRUE(policy.contains("network"));
    EXPECT_TRUE(policy.contains("filesystem"));
    EXPECT_EQ(policy["network"]["mode"], "allow");
}

}  // namespace
}  // namespace meld::manifest
