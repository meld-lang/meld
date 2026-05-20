#include <gtest/gtest.h>
#include "meld/cli/interpreter_module.hpp"
#include <filesystem>
#include <fstream>

using namespace meld::cli;

class MainEntryPointTest : public ::testing::Test {
protected:
    void SetUp() override {
        interpreter_ = std::make_unique<InterpreterModule>();
        temp_dir_ = std::filesystem::temp_directory_path() / "meld_entry_test";
        std::filesystem::create_directories(temp_dir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(temp_dir_);
    }

    ExecutionResult run(const std::string& source) {
        auto path = temp_dir_ / "test.meld";
        std::ofstream(path) << source;
        return interpreter_->run_file(path);
    }

    std::unique_ptr<InterpreterModule> interpreter_;
    std::filesystem::path temp_dir_;
};

// ── main() auto-invocation ──────────────────────────────────────────

TEST_F(MainEntryPointTest, MainIsAutoInvoked) {
    auto result = run(R"(
fnc main() {
    println("Hello, World!")
}
)");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.exit_code, 0);
}

TEST_F(MainEntryPointTest, EmptyMainExitsZero) {
    auto result = run("fnc main() {}");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.exit_code, 0);
}

TEST_F(MainEntryPointTest, VoidMainWithPrintlnExitsZero) {
    auto result = run(R"(
fnc main() {
    println("ok")
}
)");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.exit_code, 0);
}

// ── Exit code propagation ───────────────────────────────────────────

TEST_F(MainEntryPointTest, ReturnZeroPropagates) {
    auto result = run("fnc main() -> int { rtn 0 }");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.exit_code, 0);
}

TEST_F(MainEntryPointTest, ReturnOnePropagates) {
    auto result = run("fnc main() -> int { rtn 1 }");
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.exit_code, 1);
}

TEST_F(MainEntryPointTest, ReturnArbitraryCodePropagates) {
    auto result = run("fnc main() -> int { rtn 42 }");
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.exit_code, 42);
}

// ── Programs without main() ─────────────────────────────────────────

TEST_F(MainEntryPointTest, NoMainReturnsLastExpressionValue) {
    auto result = run("val x = 1");
    EXPECT_EQ(result.exit_code, 1);
}
