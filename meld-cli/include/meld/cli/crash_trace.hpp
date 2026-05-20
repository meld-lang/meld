#pragma once

#include "meld/manifest/mdebug_sidecar.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace meld::cli {

/// A crash trace entry mapping a PC to an AST node.
struct CrashTraceEntry {
    uint64_t pc{0};
    std::string ast_selector;
    std::string file;
    uint32_t line{0};
    std::string node_description;
};

/// Result of crash analysis.
struct CrashAnalysis {
    int signal{0};
    uint64_t crash_pc{0};
    std::optional<CrashTraceEntry> ast_trace;
    std::string human_readable;
    std::string json;
};

/// Analyze a crash using the .mdebug sidecar's AST-to-PC index (Req 2.16).
/// Maps the crash PC to an ast_selector for both human and AI consumption.
CrashAnalysis analyze_crash(int signal, uint64_t crash_pc,
                            const manifest::MdebugSidecar& sidecar);

/// Format a CrashAnalysis for human consumption.
std::string format_crash_human(const CrashAnalysis& analysis);

/// Format a CrashAnalysis as structured JSON for AI agent consumption.
std::string format_crash_json(const CrashAnalysis& analysis);

} // namespace meld::cli
