#pragma once

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

namespace meld::manifest {

/// Effect categories for OS-level sandboxing.
/// Maps from Meld-level effect traits (file_system, network, etc.) to
/// finer-grained sandbox capabilities.
enum class Effect : uint8_t {
    Network         = 0,
    FileSystemRead  = 1,
    FileSystemWrite = 2,
    ProcessExec     = 3,
    SystemTime      = 4,
    State           = 5,
};

/// Number of defined effects
constexpr size_t kEffectCount = 6;

/// Compact bitfield encoding of a set of effects
class EffectBitmask {
public:
    EffectBitmask() = default;
    explicit EffectBitmask(uint8_t raw) : bits_(raw) {}

    void set(Effect e) { bits_ |= (1u << static_cast<uint8_t>(e)); }
    void clear(Effect e) { bits_ &= ~(1u << static_cast<uint8_t>(e)); }
    bool has(Effect e) const { return (bits_ >> static_cast<uint8_t>(e)) & 1u; }
    bool empty() const { return bits_ == 0; }
    uint8_t raw() const { return bits_; }

    /// Set all effects (Full IO)
    void set_all() { bits_ = (1u << kEffectCount) - 1; }

    /// Intersection (most restrictive)
    EffectBitmask operator&(const EffectBitmask& other) const {
        return EffectBitmask(bits_ & other.bits_);
    }

    /// Union
    EffectBitmask operator|(const EffectBitmask& other) const {
        return EffectBitmask(bits_ | other.bits_);
    }

    bool operator==(const EffectBitmask& other) const { return bits_ == other.bits_; }
    bool operator!=(const EffectBitmask& other) const { return bits_ != other.bits_; }

    /// Get all set effects as a vector
    std::vector<Effect> to_vector() const {
        std::vector<Effect> result;
        for (uint8_t i = 0; i < kEffectCount; ++i) {
            if ((bits_ >> i) & 1u) {
                result.push_back(static_cast<Effect>(i));
            }
        }
        return result;
    }

    /// Create from a vector of effects
    static EffectBitmask from_vector(const std::vector<Effect>& effects) {
        EffectBitmask mask;
        for (auto e : effects) mask.set(e);
        return mask;
    }

private:
    uint8_t bits_{0};
};

/// Per-effect resource constraints
struct ResourceBounds {
    std::vector<std::string> allowed_domains;  // For Network
    std::vector<std::string> read_paths;       // For FileSystemRead
    std::vector<std::string> write_paths;      // For FileSystemWrite
    std::vector<std::string> exec_paths;       // For ProcessExec

    bool operator==(const ResourceBounds& other) const {
        return allowed_domains == other.allowed_domains &&
               read_paths == other.read_paths &&
               write_paths == other.write_paths &&
               exec_paths == other.exec_paths;
    }
};

/// A single entry in the manifest's symbol-to-effect map
struct SymbolEffectEntry {
    std::string symbol_name;
    EffectBitmask effects;
    ResourceBounds bounds;
    bool is_ffi{false};  // True if this is an @extern("C") function

    bool operator==(const SymbolEffectEntry& other) const {
        return symbol_name == other.symbol_name &&
               effects == other.effects &&
               bounds == other.bounds &&
               is_ffi == other.is_ffi;
    }
};

/// Convert Effect enum to string
inline std::string effect_to_string(Effect e) {
    switch (e) {
        case Effect::Network:         return "Network";
        case Effect::FileSystemRead:  return "FileSystemRead";
        case Effect::FileSystemWrite: return "FileSystemWrite";
        case Effect::ProcessExec:     return "ProcessExec";
        case Effect::SystemTime:      return "SystemTime";
        case Effect::State:           return "State";
    }
    return "Unknown";
}

}  // namespace meld::manifest
