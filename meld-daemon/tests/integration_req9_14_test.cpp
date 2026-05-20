#include "meld/daemon/binary_freshness.hpp"
#include "meld/daemon/daemon.hpp"
#include "meld/daemon/lima_vm_manager.hpp"
#include "meld/daemon/tier0_sandbox.hpp"

#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

namespace meld::daemon {
namespace {

namespace fs = std::filesystem;

// ============================================================================
// 30.1: Tier 0 sandbox via MCP execute_script
// ============================================================================

class Tier0IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Populate model with test data
        FileSemantics sem;
        sem.path = "src/lib.meld";
        auto node = std::make_shared<ASTNode>();
        node->kind = "function_definition";
        node->name = "compute";
        node->type_info = "(Int) -> Int";
        node->location = {"src/lib.meld", 1, 0};
        sem.ast = node;
        sem.exports = {"compute"};
        model_.update_file("src/lib.meld", std::move(sem));
    }

    SemanticModel model_;
};

TEST_F(Tier0IntegrationTest, ExecuteScriptReturnsOutput) {
    Tier0Sandbox sandbox(model_);
    auto result = sandbox.execute("fn main() -> Int { 42 }");

    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.output.empty());
}

TEST_F(Tier0IntegrationTest, ExecuteScriptWithRpcBindings) {
    Tier0Sandbox sandbox(model_);
    auto result = sandbox.execute("list_symbols default");

    EXPECT_TRUE(result.success);
    EXPECT_NE(result.output.find("symbols:"), std::string::npos);
}

TEST_F(Tier0IntegrationTest, SandboxDestroyedAfterExecution) {
    // Execute and verify model is unchanged
    auto count_before = model_.file_count();

    {
        Tier0Sandbox sandbox(model_);
        sandbox.execute("fn test() -> Int { 1 }");
    }  // Sandbox destroyed here

    EXPECT_EQ(model_.file_count(), count_before);
}

// ============================================================================
// 30.2: Lima VM lifecycle with daemon startup/shutdown
// ============================================================================

TEST(LimaVmIntegrationTest, DaemonStartupStartsVm) {
    if (!LimaVmManager::is_required()) {
        // On Linux, verify no-op behavior
        LimaVmManager mgr;
        EXPECT_TRUE(mgr.ensure_running());
        auto status = mgr.check_status();
        EXPECT_TRUE(status.running);
        return;
    }

    // On macOS, test with mock executor
    auto lock_path = fs::temp_directory_path() / "meld_integ_vmm.lock";
    fs::remove(lock_path);

    bool vm_running = false;
    LimaVmManager mgr;
    mgr.set_lock_path(lock_path.string());
    mgr.set_command_executor(
        [&](const std::string& cmd, std::string& output) -> int {
            if (cmd.find("limactl start") != std::string::npos) {
                vm_running = true;
                output = "Started";
                return 0;
            }
            if (cmd.find("limactl stop") != std::string::npos) {
                vm_running = false;
                output = "Stopped";
                return 0;
            }
            if (cmd.find("limactl ls") != std::string::npos) {
                output = vm_running ? "Running" : "Stopped";
                return 0;
            }
            return 0;
        });

    // Simulate daemon startup
    EXPECT_TRUE(mgr.ensure_running());
    EXPECT_TRUE(vm_running);

    // Simulate daemon shutdown (last daemon)
    mgr.set_other_daemons_running(false);
    mgr.shutdown();
    EXPECT_FALSE(vm_running);

    fs::remove(lock_path);
}

// ============================================================================
// 30.3: Binary freshness + rebuild + execution pipeline
// ============================================================================

class FreshnessIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        tmp_dir_ = fs::temp_directory_path() / "meld_freshness_integ";
        fs::create_directories(tmp_dir_);

        src_path_ = tmp_dir_ / "main.meld";
        bin_path_ = tmp_dir_ / "main";

        // Create binary first (older)
        write_file(bin_path_, "BINARY_V1");
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Create source (newer)
        write_file(src_path_, "fn main() -> Int { 42 }");

        // Index source in model
        FileSemantics sem;
        sem.path = src_path_;
        auto node = std::make_shared<ASTNode>();
        node->kind = "function_definition";
        node->name = "main";
        sem.ast = node;
        model_.update_file(src_path_, std::move(sem));
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(tmp_dir_, ec);
    }

    void write_file(const fs::path& p, const std::string& content) {
        std::ofstream f(p);
        f << content;
    }

    fs::path tmp_dir_;
    fs::path src_path_;
    fs::path bin_path_;
    SemanticModel model_;
};

TEST_F(FreshnessIntegrationTest, StaleDetectionAndRebuild) {
    BinaryFreshnessChecker checker(model_);

    // Source is newer than binary → stale
    auto result = checker.check(bin_path_);
    EXPECT_FALSE(result.stale_sources.empty());
    EXPECT_TRUE(result.rebuild_triggered);
}

TEST_F(FreshnessIntegrationTest, FileChangeInvalidatesCache) {
    BinaryFreshnessChecker checker(model_);
    checker.set_debounce_window(std::chrono::milliseconds(5000));

    // First check caches the result
    checker.check(bin_path_);

    // Simulate FileWatcher detecting a change
    checker.invalidate(src_path_);

    // Modify source
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    write_file(src_path_, "fn main() -> Int { 99 }");

    // Re-check should not use stale cache
    auto result = checker.check(bin_path_);
    EXPECT_FALSE(result.stale_sources.empty());
}

TEST_F(FreshnessIntegrationTest, CurrentBinaryNoRebuild) {
    // Make binary newer than source
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    write_file(bin_path_, "BINARY_V2");

    BinaryFreshnessChecker checker(model_);
    auto result = checker.check(bin_path_);

    EXPECT_TRUE(result.is_current);
    EXPECT_TRUE(result.stale_sources.empty());
    EXPECT_FALSE(result.rebuild_triggered);
}

}  // namespace
}  // namespace meld::daemon
