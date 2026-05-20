#pragma once

#include "deterministic_context.hpp"
#include <cstdint>
#include <functional>
#include <memory>
#include <queue>
#include <string>
#include <vector>

namespace meld::daemon {

/// Abstract base for algebraic effect handlers intercepted by the
/// deterministic runtime context.  Each handler replaces a
/// non-deterministic system call with a deterministic equivalent.
class EffectHandler {
public:
    virtual ~EffectHandler() = default;
    virtual std::string effect_name() const = 0;
    virtual bool is_active() const = 0;
};

/// Intercepts EffectTime operations.
/// Time.now() returns a configurable epoch advancing by a fixed increment.
/// Time.sleep() advances the virtual clock without actual delay.
class DeterministicTimeHandler : public EffectHandler {
public:
    explicit DeterministicTimeHandler(DeterministicContext& ctx);

    std::string effect_name() const override { return "EffectTime"; }
    bool is_active() const override { return true; }

    /// Returns current virtual time in milliseconds, then advances.
    uint64_t now_ms();

    /// Advances virtual clock by the given duration without real delay.
    void sleep_ms(uint64_t duration_ms);

    /// Current virtual time without advancing.
    uint64_t peek_ms() const;

private:
    DeterministicContext& ctx_;
};

/// Intercepts EffectRandom operations.
/// All randomness delegates to a seeded PRNG; Random.seed() is ignored.
class DeterministicRandomHandler : public EffectHandler {
public:
    explicit DeterministicRandomHandler(DeterministicContext& ctx);

    std::string effect_name() const override { return "EffectRandom"; }
    bool is_active() const override { return true; }

    /// Next random uint64.
    uint64_t next();

    /// Next random int in [0, bound).
    int next_int(int bound);

    /// Next random float in [0.0, 1.0).
    double next_double();

    /// Ignored — seed is locked to DeterministicConfig.seed.
    void seed(uint64_t /*ignored*/) {}

private:
    DeterministicContext& ctx_;
};

/// Replaces the concurrent task executor with a sequential executor
/// that processes tasks in spawn order.
class DeterministicScheduler : public EffectHandler {
public:
    explicit DeterministicScheduler(DeterministicContext& ctx);

    std::string effect_name() const override { return "EffectScheduler"; }
    bool is_active() const override { return true; }

    /// A task is a callable with no arguments.
    using Task = std::function<void()>;

    /// Enqueue a task (assigned spawn-order index).
    uint32_t spawn(Task task);

    /// Run all queued tasks in spawn order (sequential, deterministic).
    void drain();

    /// Number of pending tasks.
    size_t pending() const;

private:
    DeterministicContext& ctx_;
    std::vector<std::pair<uint32_t, Task>> queue_;
};

/// Bundles all three deterministic handlers for convenient installation.
struct DeterministicHandlerSet {
    std::unique_ptr<DeterministicTimeHandler> time;
    std::unique_ptr<DeterministicRandomHandler> random;
    std::unique_ptr<DeterministicScheduler> scheduler;

    /// Create all handlers from a context.
    static DeterministicHandlerSet create(DeterministicContext& ctx);

    /// Check if all handlers are active.
    bool all_active() const;
};

}  // namespace meld::daemon
