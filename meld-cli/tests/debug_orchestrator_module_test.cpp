#include <gtest/gtest.h>
#include "meld/cli/debug_orchestrator_module.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace meld::cli;

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Fixture — provides helpers for building CommandArgs
// ---------------------------------------------------------------------------

class DebugOrchestratorModuleTest : public ::testing::Test {
protected:
    DebugOrchestratorModule module;

    /// Build a CommandArgs with the given options and flags.
    static CommandArgs make_args(
        const std::map<std::string, std::string>& options = {},
        const std::map<std::string, std::string>& flags = {})
    {
        CommandArgs args;
        args.command = "debug";
        args.options = options;
        args.flags = flags;
        return args;
    }
};

// ---------------------------------------------------------------------------
// validate_args tests
// ---------------------------------------------------------------------------

TEST_F(DebugOrchestratorModuleTest, ValidateArgs_NoModeSpecified) {
    auto args = make_args();
    std::string err;
    EXPECT_FALSE(module.validate_args(args, err));
    EXPECT_NE(err.find("--run"), std::string::npos);
}

TEST_F(DebugOrchestratorModuleTest, ValidateArgs_BothModesSpecified) {
    auto args = make_args({{"run", "/bin/app"}, {"attach", "1234"}});
    std::string err;
    EXPECT_FALSE(module.validate_args(args, err));
    EXPECT_NE(err.find("mutually exclusive"), std::string::npos);
}

TEST_F(DebugOrchestratorModuleTest, ValidateArgs_InvalidPid) {
    auto args = make_args({{"attach", "not_a_number"}});
    std::string err;
    EXPECT_FALSE(module.validate_args(args, err));
    EXPECT_NE(err.find("invalid PID"), std::string::npos);
}

TEST_F(DebugOrchestratorModuleTest, ValidateArgs_ValidRunMode) {
    auto args = make_args({{"run", "/bin/app"}});
    std::string err;
    EXPECT_TRUE(module.validate_args(args, err));
    EXPECT_TRUE(err.empty());
}

TEST_F(DebugOrchestratorModuleTest, ValidateArgs_ValidAttachMode) {
    auto args = make_args({{"attach", "42"}});
    std::string err;
    EXPECT_TRUE(module.validate_args(args, err));
    EXPECT_TRUE(err.empty());
}

// ---------------------------------------------------------------------------
// parse_options tests (via validate + execute path inspection)
// ---------------------------------------------------------------------------

// We test parse_options indirectly through the public interface since it is
// private.  The validate_args + execute path exercises it fully.

TEST_F(DebugOrchestratorModuleTest, Execute_NoArgs_ReturnsInvalidArguments) {
    auto args = make_args();
    EXPECT_EQ(module.execute(args), CommandResult::InvalidArguments);
}

TEST_F(DebugOrchestratorModuleTest, Execute_RunNonexistentBinary_ReturnsError) {
    auto args = make_args({{"run", "/nonexistent/path/to/binary_xyz_42"}});
    EXPECT_EQ(module.execute(args), CommandResult::Error);
}

TEST_F(DebugOrchestratorModuleTest, Execute_AttachNonexistentPid_ReturnsError) {
    // PID 4294967295 (max uint32) is extremely unlikely to exist.
    auto args = make_args({{"attach", "4294967295"}});
    EXPECT_EQ(module.execute(args), CommandResult::Error);
}

// ---------------------------------------------------------------------------
// get_help / get_usage
// ---------------------------------------------------------------------------

TEST_F(DebugOrchestratorModuleTest, GetHelp_ReturnsNonEmpty) {
    auto help = module.get_help();
    EXPECT_FALSE(help.empty());
    EXPECT_NE(help.find("--run"), std::string::npos);
    EXPECT_NE(help.find("--attach"), std::string::npos);
    EXPECT_NE(help.find("--sandbox"), std::string::npos);
    EXPECT_NE(help.find("--dap"), std::string::npos);
}

TEST_F(DebugOrchestratorModuleTest, GetUsage_ReturnsNonEmpty) {
    auto usage = module.get_usage();
    EXPECT_FALSE(usage.empty());
    EXPECT_NE(usage.find("meld debug"), std::string::npos);
}

// ---------------------------------------------------------------------------
// get_name / get_description
// ---------------------------------------------------------------------------

TEST_F(DebugOrchestratorModuleTest, GetName_ReturnsDebug) {
    EXPECT_EQ(module.get_name(), "debug");
}

TEST_F(DebugOrchestratorModuleTest, GetDescription_ReturnsNonEmpty) {
    EXPECT_FALSE(module.get_description().empty());
}

// ---------------------------------------------------------------------------
// get_completions
// ---------------------------------------------------------------------------

TEST_F(DebugOrchestratorModuleTest, GetCompletions_EmptyPartial_ReturnsAllFlags) {
    auto completions = module.get_completions("--");
    EXPECT_GE(completions.size(), 7u);  // --run, --attach, --debugger, --break, --sandbox, --dap, --port
}

TEST_F(DebugOrchestratorModuleTest, GetCompletions_RunPrefix_ReturnsRunOnly) {
    auto completions = module.get_completions("--r");
    EXPECT_EQ(completions.size(), 1u);
    EXPECT_EQ(completions[0], "--run");
}

TEST_F(DebugOrchestratorModuleTest, GetCompletions_DPrefix_ReturnsDebuggerAndDap) {
    auto completions = module.get_completions("--d");
    EXPECT_EQ(completions.size(), 2u);
    // Should contain --debugger and --dap
    bool has_debugger = false, has_dap = false;
    for (const auto& c : completions) {
        if (c == "--debugger") has_debugger = true;
        if (c == "--dap") has_dap = true;
    }
    EXPECT_TRUE(has_debugger);
    EXPECT_TRUE(has_dap);
}

TEST_F(DebugOrchestratorModuleTest, GetCompletions_NoMatch_ReturnsEmpty) {
    auto completions = module.get_completions("--xyz");
    EXPECT_TRUE(completions.empty());
}

// ---------------------------------------------------------------------------
// Sandbox flag parsing
// ---------------------------------------------------------------------------

TEST_F(DebugOrchestratorModuleTest, ValidateArgs_SandboxFlagAccepted) {
    // Sandbox is a flag, not validated by validate_args — just ensure it
    // doesn't interfere with validation.
    auto args = make_args({{"run", "/bin/app"}}, {{"sandbox", ""}});
    std::string err;
    EXPECT_TRUE(module.validate_args(args, err));
}

// ---------------------------------------------------------------------------
// DAP flag parsing
// ---------------------------------------------------------------------------

TEST_F(DebugOrchestratorModuleTest, ValidateArgs_DapFlagAccepted) {
    auto args = make_args({{"run", "/bin/app"}}, {{"dap", ""}});
    std::string err;
    EXPECT_TRUE(module.validate_args(args, err));
}

// ---------------------------------------------------------------------------
// Debugger override parsing
// ---------------------------------------------------------------------------

TEST_F(DebugOrchestratorModuleTest, ValidateArgs_DebuggerOverrideAccepted) {
    auto args = make_args({{"run", "/bin/app"}, {"debugger", "lldb"}});
    std::string err;
    EXPECT_TRUE(module.validate_args(args, err));
}

TEST_F(DebugOrchestratorModuleTest, ValidateArgs_GdbOverrideAccepted) {
    auto args = make_args({{"run", "/bin/app"}, {"debugger", "gdb"}});
    std::string err;
    EXPECT_TRUE(module.validate_args(args, err));
}

// ---------------------------------------------------------------------------
// Breakpoint option parsing
// ---------------------------------------------------------------------------

TEST_F(DebugOrchestratorModuleTest, ValidateArgs_BreakpointAccepted) {
    auto args = make_args({{"run", "/bin/app"}, {"break", "main.meld:42"}});
    std::string err;
    EXPECT_TRUE(module.validate_args(args, err));
}

// ---------------------------------------------------------------------------
// Port option parsing
// ---------------------------------------------------------------------------

TEST_F(DebugOrchestratorModuleTest, ValidateArgs_PortAccepted) {
    auto args = make_args({{"run", "/bin/app"}, {"port", "5000"}});
    std::string err;
    EXPECT_TRUE(module.validate_args(args, err));
}

// ---------------------------------------------------------------------------
// .mdebug sidecar resolution — run_session attempts resolution
// ---------------------------------------------------------------------------

TEST_F(DebugOrchestratorModuleTest, Execute_RunExistingBinaryWithoutMdebug_NoSidecarCrash) {
    // Create a temporary "binary" file to pass the fs::exists check.
    auto tmp = fs::temp_directory_path() / "meld_test_binary_dom";
    {
        std::ofstream ofs(tmp);
        ofs << "#!/bin/sh\nexit 0\n";
    }
    fs::permissions(tmp, fs::perms::owner_exec | fs::perms::owner_read | fs::perms::owner_write);

    // Execute will try to detect a debugger and may fail if neither lldb nor
    // gdb is on PATH, but it should NOT crash during .mdebug resolution.
    auto args = make_args({{"run", tmp.string()}, {"debugger", "lldb"}});
    // We don't assert Success because lldb may not be installed in CI,
    // but we verify it doesn't crash or return InvalidArguments.
    auto result = module.execute(args);
    EXPECT_NE(result, CommandResult::InvalidArguments);

    fs::remove(tmp);
}

// ---------------------------------------------------------------------------
// Edge cases
// ---------------------------------------------------------------------------

TEST_F(DebugOrchestratorModuleTest, ValidateArgs_AttachZeroPid) {
    // PID 0 is technically parseable but unusual.
    auto args = make_args({{"attach", "0"}});
    std::string err;
    EXPECT_TRUE(module.validate_args(args, err));
}

TEST_F(DebugOrchestratorModuleTest, ValidateArgs_AttachEmptyString) {
    auto args = make_args({{"attach", ""}});
    std::string err;
    EXPECT_FALSE(module.validate_args(args, err));
    EXPECT_NE(err.find("invalid PID"), std::string::npos);
}
