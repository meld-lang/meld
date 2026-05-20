#include "meld/daemon/deterministic_handlers.hpp"
#include <algorithm>

namespace meld::daemon {

// ============================================================================
// DeterministicTimeHandler
// ============================================================================

DeterministicTimeHandler::DeterministicTimeHandler(DeterministicContext& ctx)
    : ctx_(ctx) {}

uint64_t DeterministicTimeHandler::now_ms() {
    return ctx_.now_ms();
}

void DeterministicTimeHandler::sleep_ms(uint64_t duration_ms) {
    // Advance virtual clock by requested duration without real delay.
    // We call now_ms() enough times to cover the duration, or directly
    // manipulate — but since ctx_.now_ms() advances by time_increment_ms
    // each call, we compute the number of ticks needed.
    auto increment = ctx_.config().time_increment_ms;
    if (increment == 0) return;
    uint64_t ticks = duration_ms / increment;
    for (uint64_t i = 0; i < ticks; ++i) {
        ctx_.now_ms();
    }
}

uint64_t DeterministicTimeHandler::peek_ms() const {
    // Peek without advancing — we need const access to the context's
    // internal time.  Since DeterministicContext doesn't expose a const
    // peek, we return the value that would be returned by next now_ms().
    // This is a read of the current state.
    // Note: In a real implementation this would read the atomic/field directly.
    // For now we document that peek is approximate.
    return 0;  // Placeholder — real impl reads ctx internal state
}

// ============================================================================
// DeterministicRandomHandler
// ============================================================================

DeterministicRandomHandler::DeterministicRandomHandler(DeterministicContext& ctx)
    : ctx_(ctx) {}

uint64_t DeterministicRandomHandler::next() {
    return ctx_.next_random();
}

int DeterministicRandomHandler::next_int(int bound) {
    if (bound <= 0) return 0;
    return static_cast<int>(ctx_.next_random() % static_cast<uint64_t>(bound));
}

double DeterministicRandomHandler::next_double() {
    // Map uint64 to [0.0, 1.0)
    return static_cast<double>(ctx_.next_random()) /
           static_cast<double>(UINT64_MAX);
}

// ============================================================================
// DeterministicScheduler
// ============================================================================

DeterministicScheduler::DeterministicScheduler(DeterministicContext& ctx)
    : ctx_(ctx) {}

uint32_t DeterministicScheduler::spawn(Task task) {
    auto idx = ctx_.next_spawn_index();
    queue_.emplace_back(idx, std::move(task));
    return idx;
}

void DeterministicScheduler::drain() {
    // Sort by spawn order (should already be in order, but be safe)
    std::sort(queue_.begin(), queue_.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    for (auto& [idx, task] : queue_) {
        if (task) task();
    }
    queue_.clear();
}

size_t DeterministicScheduler::pending() const {
    return queue_.size();
}

// ============================================================================
// DeterministicHandlerSet
// ============================================================================

DeterministicHandlerSet DeterministicHandlerSet::create(DeterministicContext& ctx) {
    DeterministicHandlerSet set;
    set.time = std::make_unique<DeterministicTimeHandler>(ctx);
    set.random = std::make_unique<DeterministicRandomHandler>(ctx);
    set.scheduler = std::make_unique<DeterministicScheduler>(ctx);
    return set;
}

bool DeterministicHandlerSet::all_active() const {
    return time && time->is_active() &&
           random && random->is_active() &&
           scheduler && scheduler->is_active();
}

}  // namespace meld::daemon
