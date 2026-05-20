#include <gtest/gtest.h>
#include "meld/cli/daemon_client.hpp"
#include "meld/cli/crash_trace.hpp"
#include "meld/manifest/mdebug_sidecar.hpp"

#include <filesystem>
#include <fstream>

using namespace meld::cli;
namespace fs = std::filesystem;

// ===========================================================================
// DaemonClient tests
// ===========================================================================

class DaemonClientTest : public ::testing::Test {
protected:
    fs::path tmp_workspace;

    void SetUp() override {
        tmp_workspace = fs::temp_directory_path() / "meld_daemon_client_test";
        fs::create_directories(tmp_workspace / ".meld");
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(tmp_workspace, ec);
    }
};

TEST_F(DaemonClientTest, NoDaemonRunning_IsNotRunning) {
    DaemonClient client(tmp_workspace);
    EXPECT_FALSE(client.is_daemon_running());
}

TEST_F(DaemonClientTest, StalePidFile_IsNotRunning) {
    // Write a PID that doesn't exist (very high number).
    auto pid_path = tmp_workspace / ".meld" / "daemon.pid";
    std::ofstream(pid_path) << "4294967295";

    DaemonClient client(tmp_workspace);
    EXPECT_FALSE(client.is_daemon_running());
}

TEST_F(DaemonClientTest, FreshnessCheck_NoDaemon_ReturnsNullopt) {
    DaemonClient client(tmp_workspace);
    auto result = client.check_binary_freshness("/some/binary");
    EXPECT_FALSE(result.has_value());
}

TEST_F(DaemonClientTest, ResolveSidecar_NoDaemon_ReturnsNullopt) {
    DaemonClient client(tmp_workspace);
    auto result = client.resolve_debug_sidecar("some-debug-id");
    EXPECT_FALSE(result.has_value());
}

TEST_F(DaemonClientTest, InvalidPidFile_IsNotRunning) {
    auto pid_path = tmp_workspace / ".meld" / "daemon.pid";
    std::ofstream(pid_path) << "not_a_number";

    DaemonClient client(tmp_workspace);
    EXPECT_FALSE(client.is_daemon_running());
}

TEST_F(DaemonClientTest, EmptyPidFile_IsNotRunning) {
    auto pid_path = tmp_workspace / ".meld" / "daemon.pid";
    std::ofstream(pid_path) << "";

    DaemonClient client(tmp_workspace);
    EXPECT_FALSE(client.is_daemon_running());
}

// ===========================================================================
// CrashTrace tests
// ===========================================================================

class CrashTraceTest : public ::testing::Test {
protected:
    manifest::MdebugSidecar make_sidecar_with_entries() {
        manifest::MdebugSidecar sidecar;
        sidecar.debug_id = "test-debug-id";
        sidecar.ast_to_pc_index = {
            {0x1000, 0x1050, "src/main.meld::main::val_decl_x"},
            {0x1050, 0x10A0, "src/main.meld::main::call_process"},
            {0x10A0, 0x1100, "src/math.meld::divide::binary_op"},
        };
        return sidecar;
    }
};

TEST_F(CrashTraceTest, CrashInKnownRange_MapsToAstNode) {
    auto sidecar = make_sidecar_with_entries();
    auto analysis = analyze_crash(11, 0x1020, sidecar);  // SIGSEGV at 0x1020

    EXPECT_EQ(analysis.signal, 11);
    EXPECT_EQ(analysis.crash_pc, 0x1020);
    ASSERT_TRUE(analysis.ast_trace.has_value());
    EXPECT_EQ(analysis.ast_trace->ast_selector, "src/main.meld::main::val_decl_x");
    EXPECT_EQ(analysis.ast_trace->file, "src/main.meld");
}

TEST_F(CrashTraceTest, CrashInSecondRange_MapsCorrectly) {
    auto sidecar = make_sidecar_with_entries();
    auto analysis = analyze_crash(6, 0x1060, sidecar);  // SIGABRT at 0x1060

    ASSERT_TRUE(analysis.ast_trace.has_value());
    EXPECT_EQ(analysis.ast_trace->ast_selector, "src/main.meld::main::call_process");
}

TEST_F(CrashTraceTest, CrashOutsideAllRanges_NoAstTrace) {
    auto sidecar = make_sidecar_with_entries();
    auto analysis = analyze_crash(11, 0x2000, sidecar);

    EXPECT_FALSE(analysis.ast_trace.has_value());
}

TEST_F(CrashTraceTest, EmptySidecar_NoAstTrace) {
    manifest::MdebugSidecar empty;
    auto analysis = analyze_crash(11, 0x1000, empty);

    EXPECT_FALSE(analysis.ast_trace.has_value());
}

TEST_F(CrashTraceTest, HumanFormat_ContainsSignalAndPC) {
    auto sidecar = make_sidecar_with_entries();
    auto analysis = analyze_crash(11, 0x1020, sidecar);

    EXPECT_NE(analysis.human_readable.find("signal 11"), std::string::npos);
    EXPECT_NE(analysis.human_readable.find("1020"), std::string::npos);
    EXPECT_NE(analysis.human_readable.find("src/main.meld"), std::string::npos);
}

TEST_F(CrashTraceTest, JsonFormat_ContainsRequiredFields) {
    auto sidecar = make_sidecar_with_entries();
    auto analysis = analyze_crash(11, 0x1020, sidecar);

    EXPECT_NE(analysis.json.find("\"signal\":11"), std::string::npos);
    EXPECT_NE(analysis.json.find("\"ast_selector\""), std::string::npos);
    EXPECT_NE(analysis.json.find("\"file\""), std::string::npos);
}

TEST_F(CrashTraceTest, JsonFormat_NullTraceWhenNoMatch) {
    manifest::MdebugSidecar empty;
    auto analysis = analyze_crash(11, 0x9999, empty);

    EXPECT_NE(analysis.json.find("\"ast_trace\":null"), std::string::npos);
}

TEST_F(CrashTraceTest, NodeDescription_ExtractedFromSelector) {
    auto sidecar = make_sidecar_with_entries();
    auto analysis = analyze_crash(11, 0x10B0, sidecar);  // In divide range

    ASSERT_TRUE(analysis.ast_trace.has_value());
    EXPECT_EQ(analysis.ast_trace->node_description, "divide::binary_op");
}
