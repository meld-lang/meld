#include "meld/manifest/build_sandbox.hpp"

#include <gtest/gtest.h>
#include <algorithm>

namespace meld::manifest {
namespace {

TEST(BuildSandboxTest, ThirdPartyNoNetwork) {
    auto config = BuildSandbox::config_for_target(
        TargetType::ThirdParty, "/src", "/bazel-out");

    // Third-party: no network allowed
    auto it = std::find(config.allowed_effects.begin(),
                        config.allowed_effects.end(),
                        Effect::Network);
    EXPECT_EQ(it, config.allowed_effects.end());
}

TEST(BuildSandboxTest, ThirdPartyReadOnlySource) {
    auto config = BuildSandbox::config_for_target(
        TargetType::ThirdParty, "/src", "/bazel-out");

    // Source tree should be read-only
    EXPECT_FALSE(config.read_only_paths.empty());
    // Output dir should be read-write
    EXPECT_FALSE(config.read_write_paths.empty());
}

TEST(BuildSandboxTest, ThirdPartyNoProcessExec) {
    auto config = BuildSandbox::config_for_target(
        TargetType::ThirdParty, "/src", "/bazel-out");

    auto it = std::find(config.allowed_effects.begin(),
                        config.allowed_effects.end(),
                        Effect::ProcessExec);
    EXPECT_EQ(it, config.allowed_effects.end());
}

TEST(BuildSandboxTest, FirstPartyRelaxed) {
    auto config = BuildSandbox::config_for_target(
        TargetType::FirstParty, "/workspace", "/bazel-out");

    // First-party: filesystem reads within workspace allowed
    EXPECT_FALSE(config.read_only_paths.empty());
    // But still no network
    auto it = std::find(config.allowed_effects.begin(),
                        config.allowed_effects.end(),
                        Effect::Network);
    EXPECT_EQ(it, config.allowed_effects.end());
}

TEST(BuildSandboxTest, PerTargetOverride) {
    SandboxConfig overrides;
    overrides.allowed_effects = {Effect::Network};
    overrides.allowed_domains = {"registry.npmjs.org"};

    auto config = BuildSandbox::config_with_overrides(
        TargetType::ThirdParty, "/src", "/bazel-out", overrides);

    // Override should add network access
    auto it = std::find(config.allowed_effects.begin(),
                        config.allowed_effects.end(),
                        Effect::Network);
    EXPECT_NE(it, config.allowed_effects.end());
}

TEST(BuildSandboxTest, OutputDirIsReadWrite) {
    auto config = BuildSandbox::config_for_target(
        TargetType::ThirdParty, "/src", "/bazel-out");

    bool has_output = false;
    for (const auto& p : config.read_write_paths) {
        if (p.find("bazel-out") != std::string::npos) {
            has_output = true;
            break;
        }
    }
    EXPECT_TRUE(has_output);
}

}  // namespace
}  // namespace meld::manifest
