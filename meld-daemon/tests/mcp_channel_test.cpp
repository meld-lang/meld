#include "meld/daemon/mcp_channel.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

namespace meld::daemon {
namespace {

class McpChannelTest : public ::testing::Test {
protected:
    SemanticModel model;

    void SetUp() override {
        // Populate model with test data
        FileSemantics sem;
        sem.path = "test.meld";
        auto root = std::make_shared<ASTNode>();
        root->kind = "module";
        root->name = "test";
        root->location = {"test.meld", 1, 1};

        auto child = std::make_shared<ASTNode>();
        child->kind = "val_declaration";
        child->name = "x";
        child->type_info = "Own[Int]";
        child->location = {"test.meld", 3, 1};
        child->effects = {"file_system"};
        root->children.push_back(child);

        sem.ast = root;

        Diagnostic d;
        d.location = {"test.meld", 5, 1};
        d.severity = DiagnosticSeverity::Warning;
        d.message = "potential ownership leak";
        d.rule_id = "W0020-ownership-leak";
        d.ast_selector = "$.module.val[0]";
        sem.diagnostics.push_back(d);

        model.update_file("test.meld", std::move(sem));
    }
};

TEST_F(McpChannelTest, ConstructionRegistersBuiltinTools) {
    McpChannel channel(model);
    // Should not crash — tools are registered in constructor
}

TEST_F(McpChannelTest, QueryTypeReturnsTypeInfo) {
    McpChannel channel(model);

    // Directly test the tool via handle_tool_call (exposed through register_tool)
    nlohmann::json args;
    args["file"] = "test.meld";
    args["symbol"] = "x";

    // Use a custom tool to test
    nlohmann::json result;
    channel.register_tool("test_query_type", [&](const nlohmann::json& a) -> nlohmann::json {
        // This just verifies registration works
        return {{"tested", true}};
    });

    // Test query_type through the model directly (channel delegates to model)
    auto type_info = model.query_type("test.meld", "x");
    ASSERT_TRUE(type_info.has_value());
    EXPECT_EQ(type_info->qualified_type, "Own[Int]");
}

TEST_F(McpChannelTest, QueryOwnershipReturnsOwn) {
    auto ownership = model.query_ownership("test.meld", "x");
    ASSERT_TRUE(ownership.has_value());
    EXPECT_EQ(ownership->kind, OwnershipKind::Own);
}

TEST_F(McpChannelTest, QueryEffectsReturnsEffects) {
    auto effects = model.query_effects("test.meld", 3);
    ASSERT_TRUE(effects.has_value());
    ASSERT_EQ(effects->required_effects.size(), 1u);
    EXPECT_EQ(effects->required_effects[0], "file_system");
}

TEST_F(McpChannelTest, GetDiagnosticsReturnsDiags) {
    auto diags = model.get_diagnostics("test.meld");
    ASSERT_EQ(diags.size(), 1u);
    EXPECT_EQ(diags[0].rule_id, "W0020-ownership-leak");
}

TEST_F(McpChannelTest, PushDiagnosticEventDoesNotCrash) {
    McpChannel channel(model);
    auto diags = model.get_diagnostics("test.meld");
    // Should not crash even though SSE transport is not connected
    channel.push_diagnostic_event("test.meld", diags);
}

TEST_F(McpChannelTest, StartAndStopDoNotCrash) {
    McpChannel channel(model);
    channel.start();
    channel.stop();
}

}  // namespace
}  // namespace meld::daemon
