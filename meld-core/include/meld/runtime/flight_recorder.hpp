#pragma once
#include <string>
#include <vector>
namespace meld::runtime {
class FlightRecorder {
public:
    FlightRecorder() = default;
    void record(const std::string&) {}
    void snapshot(const std::string&) {}
    std::vector<std::string> entries() const { return {}; }
};
} // namespace meld::runtime
