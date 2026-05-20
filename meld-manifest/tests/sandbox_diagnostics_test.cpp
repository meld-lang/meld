#include "meld/manifest/sandbox_diagnostics.hpp"

#include <gtest/gtest.h>

namespace meld::manifest {
namespace {

SandboxDiagnostic make_test_denial() {
    return SandboxDiagnostic{
        .rule_id = "SANDBOX-001",
        .ast_selector = "fn::send_data",
        .context_hash = "ctx_abc123",
        .denied_effect = Effect::Network,
        .blocked_resource = "api.example.com:443",
        .source_module = "my_module",
        .suggested_fix = "Add @uses(network) to module declaration",
    };
}

TEST(SandboxDiagnosticsTest, RecordDenial) {
    SandboxDiagnostics diags;
    diags.record_denial(make_test_denial());

    EXPECT_EQ(diags.denials().size(), 1u);
    EXPECT_EQ(diags.denials()[0].rule_id, "SANDBOX-001");
    EXPECT_EQ(diags.denials()[0].denied_effect, Effect::Network);
}

TEST(SandboxDiagnosticsTest, RecordSuccess) {
    SandboxDiagnostics diags;
    diags.record_success("my_module", 1.5);
    diags.record_success("other_module", 2.3);

    auto stats = diags.stats();
    EXPECT_EQ(stats.total_executions, 2u);
    EXPECT_EQ(stats.total_denials, 0u);
    EXPECT_DOUBLE_EQ(stats.total_policy_gen_ms, 3.8);
}

TEST(SandboxDiagnosticsTest, StatsIncludeDenials) {
    SandboxDiagnostics diags;
    diags.record_denial(make_test_denial());
    diags.record_success("ok_module", 1.0);

    auto stats = diags.stats();
    EXPECT_EQ(stats.total_executions, 1u);
    EXPECT_EQ(stats.total_denials, 1u);
}

TEST(SandboxDiagnosticsTest, FormatDenialContainsSDF) {
    auto diag = make_test_denial();
    auto formatted = SandboxDiagnostics::format_denial(diag);

    // Should contain SDF fields
    EXPECT_NE(formatted.find("SANDBOX-001"), std::string::npos);
    EXPECT_NE(formatted.find("fn::send_data"), std::string::npos);
    EXPECT_NE(formatted.find("ctx_abc123"), std::string::npos);
    EXPECT_NE(formatted.find("Network"), std::string::npos);
    EXPECT_NE(formatted.find("my_module"), std::string::npos);
}

TEST(SandboxDiagnosticsTest, FormatDenialContainsSuggestedFix) {
    auto diag = make_test_denial();
    auto formatted = SandboxDiagnostics::format_denial(diag);
    EXPECT_NE(formatted.find("@uses(network)"), std::string::npos);
}

TEST(SandboxDiagnosticsTest, VerboseMode) {
    SandboxDiagnostics diags;
    EXPECT_FALSE(diags.verbose());

    diags.set_verbose(true);
    EXPECT_TRUE(diags.verbose());
}

TEST(SandboxDiagnosticsTest, EmptyStats) {
    SandboxDiagnostics diags;
    auto stats = diags.stats();
    EXPECT_EQ(stats.total_executions, 0u);
    EXPECT_EQ(stats.total_denials, 0u);
    EXPECT_DOUBLE_EQ(stats.total_policy_gen_ms, 0.0);
}

}  // namespace
}  // namespace meld::manifest
