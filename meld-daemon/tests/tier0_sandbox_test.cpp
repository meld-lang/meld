#include "meld/daemon/tier0_sandbox.hpp"
#include "meld/daemon/semantic_model.hpp"

#include <gtest/gtest.h>
#include <future>
#include <thread>

namespace meld::daemon {
namespace {

class Tier0SandboxTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Populate model with a test symbol
        FileSemantics sem;
        sem.path = "src/main.meld";
        auto node = std::make_shared<ASTNode>();
        node->kind = "function_definition";
        node->name = "main";
        node->type_info = "() -> Int";
        node->location = {"src/main.meld", 1, 0};
        sem.ast = node;
        sem.exports = {"main"};
        model_.update_file("src/main.meld", std::move(sem));
    }

    SemanticModel model_;
};

TEST_F(Tier0SandboxTest, SuccessfulExecution) {
    Tier0Sandbox sandbox(model_);
    auto result = sandbox.execute("fn main() -> Int { 42 }");

    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.output.empty());
    EXPECT_FALSE(result.diagnostics.has_value());
}

TEST_F(Tier0SandboxTest, EmptySourceReturnsDiagnostic) {
    Tier0Sandbox sandbox(model_);
    auto result = sandbox.execute("");

    EXPECT_FALSE(result.success);
    ASSERT_TRUE(result.diagnostics.has_value());
    EXPECT_FALSE(result.diagnostics->empty());
    EXPECT_EQ(result.diagnostics->at(0).rule_id, "T0-001");
}

TEST_F(Tier0SandboxTest, CompilationErrorReturnsDiagnostic) {
    Tier0Sandbox sandbox(model_);
    auto result = sandbox.execute("COMPILE_ERROR invalid syntax");

    EXPECT_FALSE(result.success);
    ASSERT_TRUE(result.diagnostics.has_value());
    EXPECT_FALSE(result.diagnostics->empty());
    EXPECT_EQ(result.diagnostics->at(0).rule_id, "T0-010");
}

TEST_F(Tier0SandboxTest, RuntimeErrorReturnsDiagnostic) {
    Tier0Sandbox sandbox(model_);
    auto result = sandbox.execute("RUNTIME_ERROR crash here");

    EXPECT_FALSE(result.success);
    ASSERT_TRUE(result.diagnostics.has_value());
    EXPECT_EQ(result.diagnostics->at(0).rule_id, "T0-030");
}

TEST_F(Tier0SandboxTest, MemoryLimitExceeded) {
    Tier0Sandbox sandbox(model_);
    auto result = sandbox.execute("MEMORY_EXCEED allocate too much");

    EXPECT_FALSE(result.success);
    ASSERT_TRUE(result.diagnostics.has_value());
    EXPECT_EQ(result.diagnostics->at(0).rule_id, "T0-020");
}

TEST_F(Tier0SandboxTest, RpcBindingsQuerySymbol) {
    Tier0Sandbox sandbox(model_);
    auto result = sandbox.execute("query_symbol main");

    EXPECT_TRUE(result.success);
    // Output should mention the found symbol
    EXPECT_NE(result.output.find("Script executed"), std::string::npos);
}

TEST_F(Tier0SandboxTest, RpcBindingsListSymbols) {
    Tier0Sandbox sandbox(model_);
    auto result = sandbox.execute("list_symbols default");

    EXPECT_TRUE(result.success);
    EXPECT_NE(result.output.find("symbols:"), std::string::npos);
}

TEST_F(Tier0SandboxTest, ConcurrentExecutionsAreIndependent) {
    // Two concurrent sandbox executions should not interfere
    auto run = [this]() {
        Tier0Sandbox sandbox(model_);
        return sandbox.execute("fn test() -> Int { 1 }");
    };

    auto f1 = std::async(std::launch::async, run);
    auto f2 = std::async(std::launch::async, run);

    auto r1 = f1.get();
    auto r2 = f2.get();

    EXPECT_TRUE(r1.success);
    EXPECT_TRUE(r2.success);
}

TEST_F(Tier0SandboxTest, ConcurrentCrashDoesNotAffectOther) {
    auto run_ok = [this]() {
        Tier0Sandbox sandbox(model_);
        return sandbox.execute("fn ok() -> Int { 1 }");
    };

    auto run_crash = [this]() {
        Tier0Sandbox sandbox(model_);
        return sandbox.execute("RUNTIME_ERROR crash");
    };

    auto f1 = std::async(std::launch::async, run_ok);
    auto f2 = std::async(std::launch::async, run_crash);

    auto r1 = f1.get();
    auto r2 = f2.get();

    EXPECT_TRUE(r1.success);
    EXPECT_FALSE(r2.success);
}

TEST_F(Tier0SandboxTest, ConfigDefaults) {
    Tier0Sandbox sandbox(model_);
    EXPECT_EQ(sandbox.config().max_memory_bytes, 16u * 1024 * 1024);
    EXPECT_EQ(sandbox.config().max_execution_time.count(), 5000);
}

TEST_F(Tier0SandboxTest, CustomConfig) {
    Tier0Config cfg;
    cfg.max_memory_bytes = 8 * 1024 * 1024;
    cfg.max_execution_time = std::chrono::milliseconds(1000);

    Tier0Sandbox sandbox(model_, cfg);
    EXPECT_EQ(sandbox.config().max_memory_bytes, 8u * 1024 * 1024);
    EXPECT_EQ(sandbox.config().max_execution_time.count(), 1000);
}

}  // namespace
}  // namespace meld::daemon
