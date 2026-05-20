#include "meld/manifest/srt_provider.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

namespace meld::manifest {
namespace {

TEST(MeldSRTProviderTest, Name) {
    MeldSRTProvider provider;
    EXPECT_EQ(provider.name(), "SRT");
}

TEST(MeldSRTProviderTest, NetworkEffectMapping) {
    SandboxConfig config;
    config.allowed_effects = {Effect::Network};
    config.allowed_domains = {"api.example.com", "cdn.example.com"};

    auto policy = MeldSRTProvider::generate_policy(config);
    ASSERT_TRUE(policy.contains("network"));
    EXPECT_EQ(policy["network"]["mode"], "allow");
    EXPECT_EQ(policy["network"]["allowedDomains"].size(), 2u);
}

TEST(MeldSRTProviderTest, FileSystemWriteMapping) {
    SandboxConfig config;
    config.allowed_effects = {Effect::FileSystemWrite};
    config.read_write_paths = {"/tmp/output", "/var/data"};

    auto policy = MeldSRTProvider::generate_policy(config);
    ASSERT_TRUE(policy.contains("filesystem"));
    EXPECT_FALSE(policy["filesystem"]["readWrite"].empty());
}

TEST(MeldSRTProviderTest, FileSystemReadMapping) {
    SandboxConfig config;
    config.allowed_effects = {Effect::FileSystemRead};
    config.read_only_paths = {"/usr/lib", "/opt/meld/stdlib"};

    auto policy = MeldSRTProvider::generate_policy(config);
    ASSERT_TRUE(policy.contains("filesystem"));
    EXPECT_FALSE(policy["filesystem"]["readOnly"].empty());
}

TEST(MeldSRTProviderTest, ProcessExecMapping) {
    SandboxConfig config;
    config.allowed_effects = {Effect::ProcessExec};

    auto policy = MeldSRTProvider::generate_policy(config);
    ASSERT_TRUE(policy.contains("process"));
}

TEST(MeldSRTProviderTest, DenyByDefault) {
    // No effects allowed — everything should be denied
    SandboxConfig config;

    auto policy = MeldSRTProvider::generate_policy(config);
    // Network should be denied
    if (policy.contains("network")) {
        EXPECT_EQ(policy["network"]["mode"], "deny");
    }
}

TEST(MeldSRTProviderTest, StdlibReadOnlyAlwaysGranted) {
    SandboxConfig config;
    // Even with no effects, stdlib paths should be readable
    auto policy = MeldSRTProvider::generate_policy(config);
    ASSERT_TRUE(policy.contains("filesystem"));
    // Should contain Meld stdlib/runtime paths in readOnly
    auto read_only = policy["filesystem"]["readOnly"];
    EXPECT_FALSE(read_only.empty());
}

TEST(MeldSRTProviderTest, MultipleEffects) {
    SandboxConfig config;
    config.allowed_effects = {Effect::Network, Effect::FileSystemRead, Effect::FileSystemWrite};
    config.allowed_domains = {"example.com"};
    config.read_only_paths = {"/data"};
    config.read_write_paths = {"/tmp"};

    auto policy = MeldSRTProvider::generate_policy(config);
    EXPECT_TRUE(policy.contains("network"));
    EXPECT_TRUE(policy.contains("filesystem"));
}

TEST(MeldSRTProviderTest, SrtAvailabilityCheck) {
    // Just verify it doesn't crash
    bool available = MeldSRTProvider::is_srt_available();
    (void)available;
}

TEST(MeldSRTProviderTest, SetSandboxEnabled) {
    MeldSRTProvider provider;
    provider.set_sandbox_enabled(false);
    // When disabled, launch should still work (passthrough mode)
    SandboxConfig config;
    auto result = provider.launch(config, {"echo", "test"});
    EXPECT_FALSE(result.sandbox_failed);
}

}  // namespace
}  // namespace meld::manifest
