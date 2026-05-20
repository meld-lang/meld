#include "meld/manifest/sandbox_provider.hpp"

#include <gtest/gtest.h>

namespace meld::manifest {
namespace {

TEST(SandboxConfigTest, DefaultConstruction) {
    SandboxConfig config;
    EXPECT_TRUE(config.allowed_effects.empty());
    EXPECT_TRUE(config.read_only_paths.empty());
    EXPECT_TRUE(config.read_write_paths.empty());
    EXPECT_TRUE(config.allowed_domains.empty());
    EXPECT_TRUE(config.working_directory.empty());
}

TEST(SandboxConfigTest, PopulatedConfig) {
    SandboxConfig config;
    config.allowed_effects = {Effect::Network, Effect::FileSystemRead};
    config.read_only_paths = {"/usr/lib", "/opt/meld"};
    config.read_write_paths = {"/tmp"};
    config.allowed_domains = {"api.example.com"};
    config.working_directory = "/home/user/project";

    EXPECT_EQ(config.allowed_effects.size(), 2u);
    EXPECT_EQ(config.read_only_paths.size(), 2u);
    EXPECT_EQ(config.read_write_paths.size(), 1u);
    EXPECT_EQ(config.allowed_domains.size(), 1u);
}

TEST(SandboxResultTest, DefaultConstruction) {
    SandboxResult result;
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_TRUE(result.stdout_output.empty());
    EXPECT_TRUE(result.stderr_output.empty());
    EXPECT_FALSE(result.sandbox_failed);
}

TEST(SandboxResultTest, FieldAccess) {
    SandboxResult result;
    result.exit_code = 42;
    result.stdout_output = "hello";
    result.stderr_output = "warning";
    result.sandbox_failed = true;

    EXPECT_EQ(result.exit_code, 42);
    EXPECT_EQ(result.stdout_output, "hello");
    EXPECT_EQ(result.stderr_output, "warning");
    EXPECT_TRUE(result.sandbox_failed);
}

TEST(PassthroughProviderTest, Name) {
    PassthroughProvider provider;
    EXPECT_EQ(provider.name(), "Passthrough");
}

TEST(PassthroughProviderTest, LaunchEchoCommand) {
    PassthroughProvider provider;
    SandboxConfig config;
    auto result = provider.launch(config, {"echo", "hello"});
    // Passthrough just executes the command directly
    EXPECT_FALSE(result.sandbox_failed);
}

TEST(CreateDefaultProviderTest, ReturnsNonNull) {
    auto provider = create_default_provider();
    ASSERT_NE(provider, nullptr);
    // Should return SRT on macOS/Linux, or Passthrough as fallback
    auto name = provider->name();
    EXPECT_FALSE(name.empty());
}

}  // namespace
}  // namespace meld::manifest
