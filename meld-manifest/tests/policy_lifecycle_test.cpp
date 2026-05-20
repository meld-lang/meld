#include "meld/manifest/policy_lifecycle.hpp"

#include <gtest/gtest.h>
#include <filesystem>

namespace meld::manifest {
namespace {

TEST(PolicyLifecycleTest, GenerateProducesUniquePaths) {
    PolicyLifecycleManager mgr;
    SandboxConfig config;
    config.allowed_effects = {Effect::Network};

    auto path1 = mgr.generate_policy(config);
    auto path2 = mgr.generate_policy(config);

    EXPECT_NE(path1, path2);
    EXPECT_EQ(mgr.active_count(), 2u);
}

TEST(PolicyLifecycleTest, CleanupRemovesFile) {
    PolicyLifecycleManager mgr;
    SandboxConfig config;

    auto path = mgr.generate_policy(config);
    EXPECT_TRUE(std::filesystem::exists(path));
    EXPECT_EQ(mgr.active_count(), 1u);

    mgr.cleanup_policy(path);
    EXPECT_FALSE(std::filesystem::exists(path));
    EXPECT_EQ(mgr.active_count(), 0u);
}

TEST(PolicyLifecycleTest, FilePermissions) {
    PolicyLifecycleManager mgr;
    SandboxConfig config;

    auto path = mgr.generate_policy(config);
    ASSERT_TRUE(std::filesystem::exists(path));

    auto perms = std::filesystem::status(path).permissions();
    // Owner-read-only: should have owner_read, should NOT have group/other write
    EXPECT_NE(perms & std::filesystem::perms::owner_read,
              std::filesystem::perms::none);
    EXPECT_EQ(perms & std::filesystem::perms::group_write,
              std::filesystem::perms::none);
    EXPECT_EQ(perms & std::filesystem::perms::others_write,
              std::filesystem::perms::none);

    mgr.cleanup_policy(path);
}

TEST(PolicyLifecycleTest, DestructorCleansUp) {
    std::filesystem::path saved_path;
    {
        PolicyLifecycleManager mgr;
        SandboxConfig config;
        saved_path = mgr.generate_policy(config);
        EXPECT_TRUE(std::filesystem::exists(saved_path));
    }
    // After destructor, file should be cleaned up
    EXPECT_FALSE(std::filesystem::exists(saved_path));
}

TEST(PolicyLifecycleTest, CleanupStaleNonExistentDir) {
    // Should not crash on non-existent directory
    PolicyLifecycleManager::cleanup_stale("/nonexistent/tmp/dir");
}

TEST(PolicyLifecycleTest, CleanupNonExistentPolicy) {
    PolicyLifecycleManager mgr;
    // Should not crash when cleaning up a path that doesn't exist
    mgr.cleanup_policy("/nonexistent/policy.json");
    EXPECT_EQ(mgr.active_count(), 0u);
}

}  // namespace
}  // namespace meld::manifest
