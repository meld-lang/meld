#include "meld/cli/crash_trace.hpp"

#include <sstream>

namespace meld::cli {

CrashAnalysis analyze_crash(int signal, uint64_t crash_pc,
                            const manifest::MdebugSidecar& sidecar) {
    CrashAnalysis result;
    result.signal = signal;
    result.crash_pc = crash_pc;

    // Search the AST-to-PC index for the entry containing crash_pc.
    for (const auto& entry : sidecar.ast_to_pc_index) {
        if (crash_pc >= entry.pc_start && crash_pc < entry.pc_end) {
            CrashTraceEntry trace;
            trace.pc = crash_pc;
            trace.ast_selector = entry.ast_selector;
            // Extract file:line from ast_selector if it contains location info.
            // Format: "module/file.meld::function::node" — we parse the prefix.
            auto sep = entry.ast_selector.find("::");
            if (sep != std::string::npos) {
                trace.file = entry.ast_selector.substr(0, sep);
                trace.node_description = entry.ast_selector.substr(sep + 2);
            } else {
                trace.node_description = entry.ast_selector;
            }
            result.ast_trace = trace;
            break;
        }
    }

    result.human_readable = format_crash_human(result);
    result.json = format_crash_json(result);
    return result;
}

std::string format_crash_human(const CrashAnalysis& analysis) {
    std::ostringstream oss;
    oss << "CRASH: signal " << analysis.signal
        << " at PC 0x" << std::hex << analysis.crash_pc << std::dec << "\n";

    if (analysis.ast_trace) {
        const auto& t = *analysis.ast_trace;
        if (!t.file.empty()) {
            oss << "  Source: " << t.file;
            if (t.line > 0) oss << ":" << t.line;
            oss << "\n";
        }
        oss << "  AST node: " << t.ast_selector << "\n";
        if (!t.node_description.empty()) {
            oss << "  Context: " << t.node_description << "\n";
        }
    } else {
        oss << "  (no AST mapping available for this PC)\n";
    }

    return oss.str();
}

std::string format_crash_json(const CrashAnalysis& analysis) {
    std::ostringstream oss;
    oss << "{\"signal\":" << analysis.signal
        << ",\"crash_pc\":" << analysis.crash_pc;

    if (analysis.ast_trace) {
        const auto& t = *analysis.ast_trace;
        oss << ",\"ast_trace\":{"
            << "\"pc\":" << t.pc
            << ",\"ast_selector\":\"" << t.ast_selector << "\""
            << ",\"file\":\"" << t.file << "\""
            << ",\"line\":" << t.line
            << ",\"node_description\":\"" << t.node_description << "\""
            << "}";
    } else {
        oss << ",\"ast_trace\":null";
    }

    oss << "}";
    return oss.str();
}

} // namespace meld::cli
