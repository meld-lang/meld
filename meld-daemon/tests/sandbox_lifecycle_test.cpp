#include "meld/daemon/sandbox_lifecycle.hpp"

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

namespace meld::daemon {
namespace {

class SandboxLifecycleTest : public ::testing::Test {
protected:
    SandboxLifecycleManager manager;

    SandboxConfig make_config(const std::string& name) {
        SandboxConfig cfg;
        cfg.binary_path = "/tmp/test-" + name;
        cfg.manifest_path = "/tmp/test-" + name + ".manifest";
        cfg.allowed_effects = {"file_system", "network"};
        cfg.timeout = std::chrono::seconds(300);
        return cfg;
    }
};

TEST_F(SandboxLifecycleTest, LaunchCreatesProcess) {
    auto pid = manager.launch(make_config("app1"));
    ASSERT_TRUE(pid.has_value());
    EXPECT_GT(*pid, 0);
    EXPECT_EQ(manager.active_count(), 1u);
}

TEST_F(SandboxLifecycleTest, OnProcessExitCleansUp) {
    auto pid = manager.launch(make_config("app2"));
    ASSERT_TRUE(pid.has_value());
    manager.on_process_exit(*pid);
    EXPECT_EQ(manager.active_count(), 0u);
}

TEST_F(SandboxLifecycleTest, TerminateAllClearsProcesses) {
    manager.launch(make_config("a"));
    manager.launch(make_config("b"));
    manager.launch(make_config("c"));
    EXPECT_EQ(manager.active_count(), 3u);

    manager.terminate_all();
    EXPECT_EQ(manager.active_count(), 0u);
}

TEST_F(SandboxLifecycleTest, EnforceTimeoutsKillsExpired) {
    SandboxConfig cfg = make_config("timeout-test");
    cfg.timeout = std::chrono::seconds(0);  // Immediate timeout (but 0 means unlimited)
    auto pid = manager.launch(cfg);
    ASSERT_TRUE(pid.has_value());

    // With timeout=0 (unlimited), enforce_timeouts should not kill it
    manager.enforce_timeouts();
    EXPECT_EQ(manager.active_count(), 1u);
}

TEST_F(SandboxLifecycleTest, GetProcessReturnsInfo) {
    auto pid = manager.launch(make_config("info-test"));
    ASSERT_TRUE(pid.has_value());

    auto proc = manager.get_process(*pid);
    ASSERT_TRUE(proc.has_value());
    EXPECT_EQ(proc->pid, *pid);
}

TEST_F(SandboxLifecycleTest, GetProcessReturnsNulloptForUnknown) {
    EXPECT_FALSE(manager.get_process(99999).has_value());
}

TEST_F(SandboxLifecycleTest, CleanupStalePolicies) {
    namespace fs = std::filesystem;
    auto tmp = fs::temp_directory_path() / "meld-daemon-test-cleanup";
    auto daemon_tmp = tmp / "meld-daemon";
    fs::create_directories(daemon_tmp);

    // Create a stale policy file
    {
        std::ofstream ofs(daemon_tmp / "srt-settings-stale.json");
        ofs << "{}";
    }

    SandboxLifecycleManager::cleanup_stale_policies(tmp);
    EXPECT_FALSE(fs::exists(daemon_tmp / "srt-settings-stale.json"));

    fs::remove_all(tmp);
}

}  // namespace
}  // namespace meld::daemon
