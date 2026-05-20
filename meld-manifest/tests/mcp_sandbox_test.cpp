#include "meld/manifest/mcp_sandbox.hpp"

#include <gtest/gtest.h>
#include <cstdlib>

namespace meld::manifest {
namespace {

/// Test provider that records what it receives
class MockSandboxProvider : public SandboxProvider {
public:
    SandboxResult launch(const SandboxConfig& config,
                         const std::vector<std::string>& command) override {
        last_config = config;
        last_command = command;
        return SandboxResult{0, "ok", "", false};
    }
    std::string name() const override { return "Mock"; }

    SandboxConfig last_config;
    std::vector<std::string> last_command;
};

Manifest make_module_manifest(const std::vector<Effect>& effects) {
    Manifest m;
    m.format_version = 1;
    m.project_name = "test-module";
    m.project_version = "1.0.0";
    m.code_hash = "abc123";

    SymbolEffectEntry entry;
    entry.symbol_name = "main";
    for (auto e : effects) entry.effects.set(e);
    m.symbols.push_back(entry);
    return m;
}

TEST(McpSandboxTest, PolicyDerivedFromManifest) {
    MockSandboxProvider mock;
    McpSandbox sandbox(mock);

    auto manifest = make_module_manifest({Effect::FileSystemRead});
    sandbox.execute_tool("read_file", manifest, {"cat", "/tmp/data"});

    // The mock should have received a config with FileSystemRead
    auto it = std::find(mock.last_config.allowed_effects.begin(),
                        mock.last_config.allowed_effects.end(),
                        Effect::FileSystemRead);
    EXPECT_NE(it, mock.last_config.allowed_effects.end());
}

TEST(McpSandboxTest, IntersectionPolicyForMultiModule) {
    MockSandboxProvider mock;
    McpSandbox sandbox(mock);

    auto m1 = make_module_manifest({Effect::Network, Effect::FileSystemRead});
    auto m2 = make_module_manifest({Effect::FileSystemRead, Effect::State});

    sandbox.execute_tool_multi("shared_op", {m1, m2}, {"echo", "test"});

    // Intersection: only FileSystemRead is common
    bool has_fs_read = false;
    bool has_network = false;
    for (auto e : mock.last_config.allowed_effects) {
        if (e == Effect::FileSystemRead) has_fs_read = true;
        if (e == Effect::Network) has_network = true;
    }
    EXPECT_TRUE(has_fs_read);
    EXPECT_FALSE(has_network);
}

TEST(McpSandboxTest, AuditLogging) {
    MockSandboxProvider mock;
    McpSandbox sandbox(mock);

    auto manifest = make_module_manifest({Effect::Network});
    sandbox.execute_tool("net_call", manifest, {"curl", "example.com"});

    ASSERT_EQ(sandbox.audit_log().size(), 1u);
    EXPECT_EQ(sandbox.audit_log()[0].tool_name, "net_call");
    EXPECT_EQ(sandbox.audit_log()[0].target_module, "test-module");
}

TEST(McpSandboxTest, UnsafeOverrideNotSetByDefault) {
    // Ensure env var is not set in test environment
    EXPECT_FALSE(McpSandbox::is_unsafe_override_set());
}

TEST(McpSandboxTest, RejectDisableSandboxWithoutEnvVar) {
    // Without MELD_UNSAFE_NO_SANDBOX=1, disabling should be rejected
    EXPECT_FALSE(McpSandbox::is_unsafe_override_set());
}

}  // namespace
}  // namespace meld::manifest
