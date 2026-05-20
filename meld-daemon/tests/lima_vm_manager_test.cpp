#include "meld/daemon/lima_vm_manager.hpp"

#include <gtest/gtest.h>
#include <filesystem>
#include <string>

namespace meld::daemon {
namespace {

namespace fs = std::filesystem;

/// Test fixture with a mock command executor to avoid real limactl calls.
class LimaVmManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        lock_path_ = fs::temp_directory_path() / "meld_test_vmm.lock";
        fs::remove(lock_path_);

        mgr_ = std::make_unique<LimaVmManager>(std::chrono::minutes(5));
        mgr_->set_lock_path(lock_path_.string());

        // Default mock: VM is stopped, commands succeed
        vm_running_ = false;
        mgr_->set_command_executor(
            [this](const std::string& cmd, std::string& output) -> int {
                return mock_exec(cmd, output);
            });
    }

    void TearDown() override {
        mgr_.reset();
        std::error_code ec;
        fs::remove(lock_path_, ec);
    }

    int mock_exec(const std::string& cmd, std::string& output) {
        last_command_ = cmd;
        if (cmd.find("limactl ls") != std::string::npos) {
            output = vm_running_ ? "Running" : "Stopped";
            return 0;
        }
        if (cmd.find("limactl start") != std::string::npos) {
            if (start_should_fail_) {
                output = "QEMU error: insufficient resources";
                return 1;
            }
            vm_running_ = true;
            output = "Started";
            return 0;
        }
        if (cmd.find("limactl stop") != std::string::npos) {
            vm_running_ = false;
            output = "Stopped";
            return 0;
        }
        return 0;
    }

    fs::path lock_path_;
    std::unique_ptr<LimaVmManager> mgr_;
    bool vm_running_{false};
    bool start_should_fail_{false};
    std::string last_command_;
};

TEST_F(LimaVmManagerTest, IsRequiredPlatformCheck) {
#ifdef __APPLE__
    EXPECT_TRUE(LimaVmManager::is_required());
#else
    EXPECT_FALSE(LimaVmManager::is_required());
#endif
}

TEST_F(LimaVmManagerTest, EnsureRunningStartsVm) {
    // Skip on Linux where Lima is not required
    if (!LimaVmManager::is_required()) {
        EXPECT_TRUE(mgr_->ensure_running());
        return;
    }

    EXPECT_TRUE(mgr_->ensure_running());
    EXPECT_TRUE(vm_running_);

    auto log = mgr_->audit_log();
    bool found_start = false;
    for (const auto& e : log) {
        if (e.event == "start") found_start = true;
    }
    EXPECT_TRUE(found_start);
}

TEST_F(LimaVmManagerTest, EnsureRunningSkipsIfAlreadyRunning) {
    if (!LimaVmManager::is_required()) {
        GTEST_SKIP() << "Lima not required on this platform";
    }

    vm_running_ = true;
    EXPECT_TRUE(mgr_->ensure_running());

    // Should not have issued a start command
    EXPECT_EQ(last_command_.find("limactl start"), std::string::npos);
}

TEST_F(LimaVmManagerTest, FileLockAcquireAndRelease) {
    if (!LimaVmManager::is_required()) {
        GTEST_SKIP() << "Lima not required on this platform";
    }

    mgr_->ensure_running();

    auto log = mgr_->audit_log();
    bool found_acquire = false;
    for (const auto& e : log) {
        if (e.event == "lock_acquire") found_acquire = true;
    }
    EXPECT_TRUE(found_acquire);

    mgr_->shutdown();

    log = mgr_->audit_log();
    bool found_release = false;
    for (const auto& e : log) {
        if (e.event == "lock_release") found_release = true;
    }
    EXPECT_TRUE(found_release);
}

TEST_F(LimaVmManagerTest, LastDaemonShutdownStopsVm) {
    if (!LimaVmManager::is_required()) {
        GTEST_SKIP() << "Lima not required on this platform";
    }

    mgr_->set_other_daemons_running(false);
    mgr_->ensure_running();
    EXPECT_TRUE(vm_running_);

    mgr_->shutdown();
    EXPECT_FALSE(vm_running_);
}

TEST_F(LimaVmManagerTest, NonLastDaemonShutdownKeepsVm) {
    if (!LimaVmManager::is_required()) {
        GTEST_SKIP() << "Lima not required on this platform";
    }

    mgr_->set_other_daemons_running(true);
    mgr_->ensure_running();
    EXPECT_TRUE(vm_running_);

    mgr_->shutdown();
    // VM should still be running since other daemons are active
    EXPECT_TRUE(vm_running_);
}

TEST_F(LimaVmManagerTest, IdleTimeoutStopsVmWhenLastDaemon) {
    if (!LimaVmManager::is_required()) {
        GTEST_SKIP() << "Lima not required on this platform";
    }

    mgr_->set_other_daemons_running(false);
    mgr_->ensure_running();
    EXPECT_TRUE(vm_running_);

    mgr_->check_idle_shutdown();
    EXPECT_FALSE(vm_running_);
}

TEST_F(LimaVmManagerTest, StartFailureEmitsDiagnostic) {
    if (!LimaVmManager::is_required()) {
        GTEST_SKIP() << "Lima not required on this platform";
    }

    start_should_fail_ = true;

    std::vector<Diagnostic> received_diags;
    mgr_->on_error([&](const Diagnostic& d) {
        received_diags.push_back(d);
    });

    mgr_->ensure_running();

    EXPECT_FALSE(vm_running_);
    ASSERT_FALSE(received_diags.empty());
    EXPECT_EQ(received_diags[0].rule_id, "LIMA-001");
    EXPECT_NE(received_diags[0].message.find("Failed to start"), std::string::npos);
}

TEST_F(LimaVmManagerTest, AuditLogRecordsEvents) {
    if (!LimaVmManager::is_required()) {
        GTEST_SKIP() << "Lima not required on this platform";
    }

    mgr_->ensure_running();
    mgr_->shutdown();

    auto log = mgr_->audit_log();
    EXPECT_GE(log.size(), 2u);  // At least lock_acquire + lock_release

    // Verify timestamps are ordered
    for (size_t i = 1; i < log.size(); ++i) {
        EXPECT_GE(log[i].timestamp, log[i - 1].timestamp);
    }
}

TEST_F(LimaVmManagerTest, CheckStatusReturnsRunningState) {
    if (!LimaVmManager::is_required()) {
        auto status = mgr_->check_status();
        EXPECT_TRUE(status.running);  // Linux: always available
        return;
    }

    vm_running_ = true;
    auto status = mgr_->check_status();
    EXPECT_TRUE(status.running);

    vm_running_ = false;
    status = mgr_->check_status();
    EXPECT_FALSE(status.running);
}

}  // namespace
}  // namespace meld::daemon
