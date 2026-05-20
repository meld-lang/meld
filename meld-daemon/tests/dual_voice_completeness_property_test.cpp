/// Property 3: Dual-Voice Completeness
/// For any diagnostic, both `message` and `agent_context` fields SHALL be
/// non-empty, and `agent_context` SHALL contain valid `rule_id` and `ast_selector`.

#include "meld/daemon/dual_voice.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>

namespace meld::daemon {
namespace {

rc::Gen<DiagnosticSeverity> genSeverity() {
    return rc::gen::element(DiagnosticSeverity::Error,
                            DiagnosticSeverity::Warning,
                            DiagnosticSeverity::Info,
                            DiagnosticSeverity::Hint);
}

rc::Gen<std::string> genNonEmptyString() {
    return rc::gen::nonEmpty(rc::gen::container<std::string>(
        rc::gen::inRange('a', static_cast<char>('z' + 1))));
}

rc::Gen<Diagnostic> genDiagnostic() {
    return rc::gen::apply([](std::string file, uint32_t line, uint32_t col,
                             DiagnosticSeverity sev, std::string msg,
                             std::string rule, std::string selector) {
        Diagnostic d;
        d.location = {file + ".meld", line, col};
        d.severity = sev;
        d.message = msg;
        d.rule_id = rule;
        d.ast_selector = selector;
        return d;
    },
    genNonEmptyString(),
    rc::gen::inRange(1u, 10000u),
    rc::gen::inRange(1u, 200u),
    genSeverity(),
    genNonEmptyString(),
    genNonEmptyString(),
    genNonEmptyString());
}

RC_GTEST_PROP(DualVoiceCompleteness, MessageAndContextNonEmpty, ()) {
    auto diag = *genDiagnostic();
    auto dv = DualVoiceFormatter::format_diagnostic(diag);

    RC_ASSERT(!dv.message.empty());
    RC_ASSERT(!dv.agent_context.rule_id.empty());
    RC_ASSERT(!dv.agent_context.ast_selector.empty());
    RC_ASSERT(!dv.agent_context.context_hash.empty());
}

RC_GTEST_PROP(DualVoiceCompleteness, RuleIdPreserved, ()) {
    auto diag = *genDiagnostic();
    auto dv = DualVoiceFormatter::format_diagnostic(diag);

    RC_ASSERT(dv.agent_context.rule_id == diag.rule_id);
}

RC_GTEST_PROP(DualVoiceCompleteness, AstSelectorPreserved, ()) {
    auto diag = *genDiagnostic();
    auto dv = DualVoiceFormatter::format_diagnostic(diag);

    RC_ASSERT(dv.agent_context.ast_selector == diag.ast_selector);
}

RC_GTEST_PROP(DualVoiceCompleteness, LspJsonContainsBothFields, ()) {
    auto diag = *genDiagnostic();
    auto dv = DualVoiceFormatter::format_diagnostic(diag);
    auto j = DualVoiceFormatter::to_lsp_json(dv);

    RC_ASSERT(j.contains("message"));
    RC_ASSERT(j.contains("data"));
    RC_ASSERT(j["data"].contains("rule_id"));
    RC_ASSERT(j["data"].contains("ast_selector"));
    RC_ASSERT(j["data"].contains("context_hash"));
}

RC_GTEST_PROP(DualVoiceCompleteness, McpJsonContainsBothFields, ()) {
    auto diag = *genDiagnostic();
    auto dv = DualVoiceFormatter::format_diagnostic(diag);
    auto j = DualVoiceFormatter::to_mcp_json(dv);

    RC_ASSERT(j.contains("message"));
    RC_ASSERT(j.contains("agent_context"));
    RC_ASSERT(j["agent_context"].contains("rule_id"));
    RC_ASSERT(j["agent_context"].contains("ast_selector"));
    RC_ASSERT(j["agent_context"].contains("context_hash"));
}

RC_GTEST_PROP(DualVoiceCompleteness, FixIncludedWhenProvided, ()) {
    auto diag = *genDiagnostic();
    auto selector = *genNonEmptyString();
    auto operation = *rc::gen::element<std::string>("insert", "replace", "delete");
    auto replacement = *genNonEmptyString();

    AstPatch fix{selector, operation, replacement};
    auto dv = DualVoiceFormatter::format_diagnostic_with_fix(diag, fix);

    RC_ASSERT(dv.agent_context.fix.has_value());
    RC_ASSERT(dv.agent_context.fix->ast_selector == selector);
    RC_ASSERT(dv.agent_context.fix->operation == operation);

    auto j = DualVoiceFormatter::to_mcp_json(dv);
    RC_ASSERT(j["agent_context"].contains("fix"));
    RC_ASSERT(j["agent_context"]["fix"]["operation"] == operation);
}

}  // namespace
}  // namespace meld::daemon
