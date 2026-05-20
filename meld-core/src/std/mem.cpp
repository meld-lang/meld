/// @file mem.cpp
/// @brief std.mem module implementation — runtime support for Hold[T], View[T],
///        Storable trait, and intrinsic annotation registration.
///
/// Most of the logic lives in the header (template types), but this file
/// provides the non-template runtime support: intrinsic registry helpers
/// and any future non-inline implementations.

#include "meld/std/mem.hpp"
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace meld::std_mem {

// ---------------------------------------------------------------------------
// Intrinsic Annotation Registry
// ---------------------------------------------------------------------------

/// Runtime registry that maps @intrinsic annotation names to their
/// associated type information. The Semantic Analyzer's Intrinsic Resolution
/// Pass populates this during module loading.
class IntrinsicRegistry {
public:
    enum class IntrinsicKind {
        MemoryStrategy,    // @intrinsic(memory_strategy) → Storable trait
        ManagedContainer,  // @intrinsic(managed_container) → vec, dict, set
        MemoryMove         // @intrinsic(memory_move) → std.mem.move()
    };

    struct Entry {
        IntrinsicKind kind;
        std::string type_name;
    };

    static IntrinsicRegistry& instance() {
        static IntrinsicRegistry registry;
        return registry;
    }

    /// Register an intrinsic annotation mapping.
    void register_intrinsic(const std::string& annotation, IntrinsicKind kind,
                            const std::string& type_name) {
        entries_[annotation] = Entry{kind, type_name};
    }

    /// Look up an intrinsic by annotation name.
    const Entry* lookup(const std::string& annotation) const {
        auto it = entries_.find(annotation);
        if (it != entries_.end()) {
            return &it->second;
        }
        return nullptr;
    }

    /// Check if an annotation is registered.
    bool has(const std::string& annotation) const {
        return entries_.count(annotation) > 0;
    }

    /// Clear all registrations (for testing).
    void clear() { entries_.clear(); }

private:
    IntrinsicRegistry() = default;
    std::unordered_map<std::string, Entry> entries_;
};

// ---------------------------------------------------------------------------
// Module initialization
// ---------------------------------------------------------------------------

/// Register the std.mem module's intrinsic annotations with the global
/// registry. Called during module loading by the Semantic Analyzer.
void register_std_mem_intrinsics() {
    auto& registry = IntrinsicRegistry::instance();

    // @intrinsic(memory_strategy) → Storable trait
    registry.register_intrinsic(
        IntrinsicTag::memory_strategy,
        IntrinsicRegistry::IntrinsicKind::MemoryStrategy,
        "Storable");

    // @intrinsic(memory_move) → std.mem.move()
    registry.register_intrinsic(
        IntrinsicTag::memory_move,
        IntrinsicRegistry::IntrinsicKind::MemoryMove,
        "std.mem.move");
}

// ---------------------------------------------------------------------------
// Storable trait runtime helpers
// ---------------------------------------------------------------------------

/// Check if a Storable instance represents an owning reference.
bool is_owning_reference(const Storable& storable) {
    return storable.is_owning();
}

/// Check if a Storable instance's referenced object is alive.
bool is_alive(const Storable& storable) {
    return storable.access().has_value();
}

} // namespace meld::std_mem
