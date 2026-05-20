#include "meld/daemon/dual_voice.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

namespace meld::daemon {
namespace {

class DualVoiceTest : public ::testing::Test {
protected:
    Diagnostic make_diag() {
        Diagnostic d;
        d.location = {"test.meld", 10, 5};
        d.severity = DiagnosticSeverity::Error;
        d.message = "type mismatch: expected Int, got String";
        d.rule_id = "E0042-type-mismatch";
        d.ast_selector = "$.module.function[0].body.expr[2]";
        d.context_hash = "abc123";
        return d;
    }
};

TEST_F(DualVoiceTest, FormatDiagnosticProducesBothFields) {
    auto dv = DualVoiceFormatter::format_diagnostic(make_diag());
    EXPECT_FALSE(dv.message.empty());
    EXPECT_FALSE(dv.agent_context.rule_id.empty());
    EXPECT_FALSE(dv.agent_context.ast_selector.empty());
    EXPECT_FALSE(dv.agent_context.context_hash.empty());
}

TEST_F(DualVoiceTest, FormatDiagnosticMessageContainsLocation) {
    auto dv = DualVoiceFormatter::format_diagnostic(make_diag());
    EXPECT_NE(dv.message.find("test.meld"), std::string::npos);
    EXPECT_NE(dv.message.find("10"), std::string::npos);
}

TEST_F(DualVoiceTest, FormatDiagnosticPreservesRuleId) {
    auto dv = DualVoiceFormatter::format_diagnostic(make_diag());
    EXPECT_EQ(dv.agent_context.rule_id, "E0042-type-mismatch");
}

TEST_F(DualVoiceTest, FormatWithFixIncludesPatch) {
    AstPatch fix;
    fix.ast_selector = "$.module.function[0].body.expr[2]";
    fix.operation = "replace";
    fix.replacement = "to_string(x)";

    auto dv = DualVoiceFormatter::format_diagnostic_with_fix(make_diag(), fix);
    ASSERT_TRUE(dv.agent_context.fix.has_value());
    EXPECT_EQ(dv.agent_context.fix->operation, "replace");
    EXPECT_EQ(dv.agent_context.fix->replacement, "to_string(x)");
}

TEST_F(DualVoiceTest, ToLspJsonHasMessageAndData) {
    auto dv = DualVoiceFormatter::format_diagnostic(make_diag());
    auto j = DualVoiceFormatter::to_lsp_json(dv);

    EXPECT_TRUE(j.contains("message"));
    EXPECT_TRUE(j.contains("data"));
    EXPECT_TRUE(j["data"].contains("rule_id"));
    EXPECT_TRUE(j["data"].contains("ast_selector"));
}

TEST_F(DualVoiceTest, ToMcpJsonHasMessageAndAgentContext) {
    auto dv = DualVoiceFormatter::format_diagnostic(make_diag());
    auto j = DualVoiceFormatter::to_mcp_json(dv);

    EXPECT_TRUE(j.contains("message"));
    EXPECT_TRUE(j.contains("agent_context"));
    EXPECT_EQ(j["agent_context"]["rule_id"], "E0042-type-mismatch");
}

TEST_F(DualVoiceTest, LspJsonIncludesFixWhenPresent) {
    AstPatch fix{"$.expr", "replace", "fixed_code"};
    auto dv = DualVoiceFormatter::format_diagnostic_with_fix(make_diag(), fix);
    auto j = DualVoiceFormatter::to_lsp_json(dv);

    EXPECT_TRUE(j["data"].contains("fix"));
    EXPECT_EQ(j["data"]["fix"]["operation"], "replace");
}

TEST_F(DualVoiceTest, ContextHashComputedWhenEmpty) {
    Diagnostic d = make_diag();
    d.context_hash = "";  // Force auto-computation
    auto dv = DualVoiceFormatter::format_diagnostic(d);
    EXPECT_FALSE(dv.agent_context.context_hash.empty());
}

}  // namespace
}  // namespace meld::daemon
