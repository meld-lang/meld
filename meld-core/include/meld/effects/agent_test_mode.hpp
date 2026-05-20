#pragma once

#include "effect.hpp"
#include "builtin_effects.hpp"
#include <string>
#include <vector>
#include <chrono>
#include <mutex>
#include <sstream>

namespace meld::effects {

// Structured effect trace entry (AI_DX Req 143)
struct EffectTraceEntry {
    std::string effect_name;
    std::string operation_name;
    std::vector<std::string> args;      // Stringified arguments
    std::string result;                  // Stringified result
    int64_t virtual_timestamp_ms;        // Virtual time at invocation
    uint64_t sequence_number;            // Global ordering
};

// Agent-Test Mode runtime (AI_DX Req 143)
// Installs deterministic handlers and logs all effect invocations.
class AgentTestMode {
public:
    // Enable Agent-Test mode on the EffectRuntime singleton.
    // Installs: virtual time (epoch 0), seedable random (seed 0).
    // User-installed handlers that are already on the stack take precedence.
    static void enable();

    // Disable Agent-Test mode and remove its handler scope.
    static void disable();

    // Check if Agent-Test mode is currently active.
    static bool is_active();

    // Get the structured effect trace accumulated since enable().
    static std::vector<EffectTraceEntry> get_trace();

    // Clear the trace without disabling Agent-Test mode.
    static void clear_trace();

    // Serialize the trace to JSON (compatible with Flight Recorder format).
    static std::string trace_to_json();

    // Record a trace entry (called internally by tracing wrappers).
    static void record(EffectTraceEntry entry);

private:
    static bool active_;
    static std::vector<EffectTraceEntry> trace_;
    static uint64_t sequence_counter_;
    static std::mutex mutex_;
};

} // namespace meld::effects
