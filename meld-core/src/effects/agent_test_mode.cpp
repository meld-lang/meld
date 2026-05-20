#include "meld/effects/agent_test_mode.hpp"
#include <sstream>

namespace meld::effects {

// Static member definitions
bool AgentTestMode::active_ = false;
std::vector<EffectTraceEntry> AgentTestMode::trace_;
uint64_t AgentTestMode::sequence_counter_ = 0;
std::mutex AgentTestMode::mutex_;

void AgentTestMode::enable() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (active_) return;  // Already active

    active_ = true;
    sequence_counter_ = 0;
    trace_.clear();

    // Install deterministic handlers: virtual time (epoch 0), seedable random (seed 0)
    // These go at the bottom of the stack — user-installed handlers on top take precedence
    auto& runtime = EffectRuntime::instance();
    std::vector<std::shared_ptr<EffectHandler>> agent_handlers = {
        create_virtual_time_handler(0),    // epoch 0
        create_seedable_random_handler(0)  // seed 0
    };
    runtime.pushScope(agent_handlers);
}

void AgentTestMode::disable() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!active_) return;

    auto& runtime = EffectRuntime::instance();
    runtime.popScope();
    active_ = false;
}

bool AgentTestMode::is_active() {
    std::lock_guard<std::mutex> lock(mutex_);
    return active_;
}

std::vector<EffectTraceEntry> AgentTestMode::get_trace() {
    std::lock_guard<std::mutex> lock(mutex_);
    return trace_;
}

void AgentTestMode::clear_trace() {
    std::lock_guard<std::mutex> lock(mutex_);
    trace_.clear();
    sequence_counter_ = 0;
}

void AgentTestMode::record(EffectTraceEntry entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!active_) return;
    entry.sequence_number = sequence_counter_++;
    trace_.push_back(std::move(entry));
}

std::string AgentTestMode::trace_to_json() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream json;

    json << "{\n";
    json << "  \"agent_test_mode\": true,\n";
    json << "  \"trace_entries\": " << trace_.size() << ",\n";
    json << "  \"trace\": [\n";

    for (size_t i = 0; i < trace_.size(); ++i) {
        const auto& e = trace_[i];
        if (i > 0) json << ",\n";

        json << "    {\n";
        json << "      \"seq\": " << e.sequence_number << ",\n";
        json << "      \"effect\": \"" << e.effect_name << "\",\n";
        json << "      \"operation\": \"" << e.operation_name << "\",\n";
        json << "      \"args\": [";
        for (size_t j = 0; j < e.args.size(); ++j) {
            if (j > 0) json << ", ";
            json << "\"" << e.args[j] << "\"";
        }
        json << "],\n";
        json << "      \"result\": \"" << e.result << "\",\n";
        json << "      \"virtual_time_ms\": " << e.virtual_timestamp_ms << "\n";
        json << "    }";
    }

    json << "\n  ]\n";
    json << "}";

    return json.str();
}

} // namespace meld::effects
