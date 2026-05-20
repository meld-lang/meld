#include "meld/daemon/deterministic_context.hpp"

namespace meld::daemon {

DeterministicContext::DeterministicContext(const DeterministicConfig& config)
    : config_(config)
    , current_time_ms_(
          (config.epoch != 0 ? config.epoch : DeterministicConfig::kDefaultEpoch) * 1000)
    , rng_state_(config.seed) {}

uint64_t DeterministicContext::now_ms() {
    uint64_t t = current_time_ms_;
    current_time_ms_ += config_.time_increment_ms;
    return t;
}

uint64_t DeterministicContext::next_random() {
    // xorshift64* — simple, fast, deterministic PRNG.
    rng_state_ ^= rng_state_ >> 12;
    rng_state_ ^= rng_state_ << 25;
    rng_state_ ^= rng_state_ >> 27;
    return rng_state_ * 0x2545F4914F6CDD1DULL;
}

uint32_t DeterministicContext::next_spawn_index() {
    return spawn_counter_++;
}

void DeterministicContext::reset() {
    current_time_ms_ =
        (config_.epoch != 0 ? config_.epoch : DeterministicConfig::kDefaultEpoch) * 1000;
    rng_state_ = config_.seed;
    spawn_counter_ = 0;
}

}  // namespace meld::daemon
