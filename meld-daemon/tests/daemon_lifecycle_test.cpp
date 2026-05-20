#include "meld/daemon/daemon.hpp"

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <thread>

namespace meld::daemon {
namespace {

class DaemonLifecycleTest : public ::testing::Test {
protected:
    std::filesystem::path tmp_workspace;

    void SetUp() override {
        tmp_workspace = std::filesystem::temp_directory_path() / "meld-daemon-lifecycle-test";
        std::filesystem::create_directories(tmp_workspace);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(tmp_workspace, ec);
    }
};

TEST_F(DaemonLifecycleTest, ConstructWithConfig) {
    DaemonConfig config;
    config.workspace = tmp_workspace;
    config.timeout_seconds = 1;
    config.verbose = false;

    MeldDaemon daemon(std::move(config));
    EXPECT_FALSE(daemon.is_running());
    EXPECT_EQ(daemon.config().workspace, tmp_workspace);
}

TEST_F(DaemonLifecycleTest, RunAndShutdown) {
    DaemonConfig config;
    config.workspace = tmp_workspace;
    config.timeout_seconds = 1;  // Auto-exit after 1 second idle

    MeldDaemon daemon(std::move(config));

    // Run in a background thread (will auto-exit via timeout)
    std::thread t([&] { daemon.run(); });

    // Wait a bit then verify it's running
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(daemon.is_running());

    // Wait for idle timeout to trigger shutdown
    t.join();
    EXPECT_FALSE(daemon.is_running());
}

TEST_F(DaemonLifecycleTest, ExplicitShutdown) {
    DaemonConfig config;
    config.workspace = tmp_workspace;
    config.timeout_seconds = 0;  // No idle timeout

    MeldDaemon daemon(std::move(config));

    std::thread t([&] { daemon.run(); });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(daemon.is_running());

    daemon.shutdown();
    t.join();
    EXPECT_FALSE(daemon.is_running());
}

TEST_F(DaemonLifecycleTest, SemanticModelAccessible) {
    DaemonConfig config;
    config.workspace = tmp_workspace;

    MeldDaemon daemon(std::move(config));

    // Model should be accessible before run()
    EXPECT_EQ(daemon.semantic_model().file_count(), 0u);

    FileSemantics sem;
    sem.path = "test.meld";
    daemon.semantic_model().update_file("test.meld", std::move(sem));
    EXPECT_EQ(daemon.semantic_model().file_count(), 1u);
}

TEST_F(DaemonLifecycleTest, DiagnosticSubscription) {
    DaemonConfig config;
    config.workspace = tmp_workspace;

    MeldDaemon daemon(std::move(config));

    std::vector<Diagnostic> received;
    daemon.on_diagnostics_updated([&](const std::vector<Diagnostic>& diags) {
        received.insert(received.end(), diags.begin(), diags.end());
    });

    Diagnostic d;
    d.location = {"test.meld", 1, 1};
    d.severity = DiagnosticSeverity::Error;
    d.message = "test error";
    d.rule_id = "E0001";

    daemon.publish_diagnostics({d});
    ASSERT_EQ(received.size(), 1u);
    EXPECT_EQ(received[0].message, "test error");
}

TEST_F(DaemonLifecycleTest, ShutdownIsIdempotent) {
    DaemonConfig config;
    config.workspace = tmp_workspace;
    config.timeout_seconds = 1;

    MeldDaemon daemon(std::move(config));
    std::thread t([&] { daemon.run(); });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    daemon.shutdown();
    daemon.shutdown();  // Should not crash
    t.join();
}

TEST_F(DaemonLifecycleTest, CleanupStaleResourcesOnStartup) {
    // Create a stale policy file
    namespace fs = std::filesystem;
    auto daemon_tmp = fs::temp_directory_path() / "meld-daemon";
    fs::create_directories(daemon_tmp);
    {
        std::ofstream ofs(daemon_tmp / "srt-settings-stale.json");
        ofs << "{}";
    }

    DaemonConfig config;
    config.workspace = tmp_workspace;
    config.timeout_seconds = 1;

    MeldDaemon daemon(std::move(config));
    std::thread t([&] { daemon.run(); });
    t.join();

    // Stale file should have been cleaned up
    EXPECT_FALSE(fs::exists(daemon_tmp / "srt-settings-stale.json"));
}

}  // namespace
}  // namespace meld::daemon
