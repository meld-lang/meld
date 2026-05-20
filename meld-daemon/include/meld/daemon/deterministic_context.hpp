#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace meld::daemon {

/// Per-session configuration for deterministic replay.
/// All fields are optional; unset fields use their defaults.
struct DeterministicConfig {
    uint64_t seed{42};                  ///< PRNG seed for entropy sources.
    uint64_t epoch{0};                  ///< Fixed epoch (seconds since Unix epoch). 0 = use 2025-01-01T00:00:00Z.
    uint64_t time_increment_ms{1};      ///< Milliseconds to advance per time query.

    static constexpr uint64_t kDefaultEpoch = 1735689600;  // 2025-01-01T00:00:00Z

    bool operator==(const DeterministicConfig& o) const {
        return seed == o.seed && epoch == o.epoch &&
               time_increment_ms == o.time_increment_ms;
    }
};

/// Deterministic runtime context for agent-mode debugging.
///
/// When `deterministic: true` is set on an MCP execution request, this
/// context replaces non-deterministic system calls:
///   - Time: monotonically advancing from a fixed epoch
///   - Entropy: seeded PRNG replacing /dev/urandom / getrandom()
///   - Scheduling: sequential execution by spawn order (no preemption)
///
/// Network non-determinism is out of scope — handled via mocks.
class DeterministicContext {
public:
    explicit DeterministicContext(const DeterministicConfig& config = {});
    ~DeterministicContext() = default;

    /// Get the current deterministic timestamp (advances by time_increment_ms each call).
    uint64_t now_ms();

    /// Get the next deterministic random value.
    uint64_t next_random();

    /// Get the spawn-order index for the next task (sequential scheduling).
    uint32_t next_spawn_index();

    /// Reset all counters to initial state (same config).
    void reset();

    /// Access the active configuration.
    const DeterministicConfig& config() const { return config_; }

    /// Whether this context is active (always true once constructed).
    bool active() const { return true; }

private:
    DeterministicConfig config_;
    uint64_t current_time_ms_;
    uint64_t rng_state_;
    uint32_t spawn_counter_{0};
};

}  // namespace meld::daemon
