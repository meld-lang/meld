#include "meld/daemon/dual_voice.hpp"

#include <functional>
#include <sstream>

namespace meld::daemon {

namespace {

std::string severity_to_string(DiagnosticSeverity sev) {
    switch (sev) {
        case DiagnosticSeverity::Error:   return "error";
        case DiagnosticSeverity::Warning: return "warning";
        case DiagnosticSeverity::Info:    return "info";
        case DiagnosticSeverity::Hint:    return "hint";
    }
    return "unknown";
}

int severity_to_lsp(DiagnosticSeverity sev) {
    switch (sev) {
        case DiagnosticSeverity::Error:   return 1;
        case DiagnosticSeverity::Warning: return 2;
        case DiagnosticSeverity::Info:    return 3;
        case DiagnosticSeverity::Hint:    return 4;
    }
    return 1;
}

std::string compute_context_hash(const Diagnostic& diag) {
    // Simple hash of file + line + message for deduplication
    std::hash<std::string> hasher;
    std::ostringstream oss;
    oss << diag.location.file.string() << ":" << diag.location.line << ":" << diag.message;
    auto h = hasher(oss.str());
    std::ostringstream hex;
    hex << std::hex << h;
    return hex.str();
}

}  // namespace

DualVoiceResponse DualVoiceFormatter::format_diagnostic(const Diagnostic& diag) {
    std::ostringstream msg;
    msg << diag.location.file.string() << ":" << diag.location.line
        << ":" << diag.location.column << ": "
        << severity_to_string(diag.severity) << ": " << diag.message;

    AgentContext ctx;
    ctx.rule_id = diag.rule_id;
    ctx.ast_selector = diag.ast_selector;
    ctx.context_hash = diag.context_hash.empty() ? compute_context_hash(diag) : diag.context_hash;

    return DualVoiceResponse{msg.str(), std::move(ctx)};
}

DualVoiceResponse DualVoiceFormatter::format_diagnostic_with_fix(const Diagnostic& diag,
                                                                   const AstPatch& fix) {
    auto response = format_diagnostic(diag);
    response.agent_context.fix = fix;
    return response;
}

nlohmann::json DualVoiceFormatter::agent_context_to_json(const AgentContext& ctx) {
    nlohmann::json j;
    j["rule_id"] = ctx.rule_id;
    j["ast_selector"] = ctx.ast_selector;
    j["context_hash"] = ctx.context_hash;
    if (!ctx.version_hash.empty()) {
        j["version_hash"] = ctx.version_hash;
    }
    if (ctx.fix.has_value()) {
        j["fix"] = {
            {"ast_selector", ctx.fix->ast_selector},
            {"operation", ctx.fix->operation},
            {"replacement", ctx.fix->replacement}
        };
    }
    return j;
}

nlohmann::json DualVoiceFormatter::to_lsp_json(const DualVoiceResponse& response) {
    nlohmann::json j;
    j["message"] = response.message;
    j["severity"] = severity_to_lsp(
        response.message.find("error:") != std::string::npos ? DiagnosticSeverity::Error :
        response.message.find("warning:") != std::string::npos ? DiagnosticSeverity::Warning :
        DiagnosticSeverity::Info);
    j["data"] = agent_context_to_json(response.agent_context);
    return j;
}

nlohmann::json DualVoiceFormatter::to_mcp_json(const DualVoiceResponse& response) {
    nlohmann::json j;
    j["message"] = response.message;
    j["agent_context"] = agent_context_to_json(response.agent_context);
    return j;
}

}  // namespace meld::daemon
