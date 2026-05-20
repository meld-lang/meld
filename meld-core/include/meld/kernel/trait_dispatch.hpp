#pragma once

#include "dispatch.hpp"
#include "meld/meta/advanced_traits.hpp"
#include <unordered_map>
#include <shared_mutex>
#include <chrono>

namespace meld::kernel {

// Ownership qualifier for dispatch resolution
enum class OwnershipQualifier {
    Owned,      // Owned<T> - exclusive ownership
    Borrowed,   // Borrowed<T> - immutable reference
    BorrowedMut // Borrowed<T> with mutable access
};

// Cache entry for resolved dispatch results
struct DispatchCacheEntry {
    std::shared_ptr<FunctionSignature> signature;
    std::chrono::steady_clock::time_point timestamp;
    size_t hit_count = 0;
};

// Key for the dispatch cache (function name + type signature hash)
struct DispatchCacheKey {
    std::string function_name;
    std::vector<std::string> arg_type_names;
    std::vector<OwnershipQualifier> ownership_qualifiers;

    bool operator==(const DispatchCacheKey& other) const;
};

} // namespace meld::kernel

// Hash specialization for DispatchCacheKey
namespace std {
    template<>
    struct hash<meld::kernel::DispatchCacheKey> {
        size_t operator()(const meld::kernel::DispatchCacheKey& key) const;
    };
} // namespace std

namespace meld::kernel {

// Bridges the trait system with the multiple dispatch system
class TraitDispatchIntegration {
public:
    TraitDispatchIntegration();
    explicit TraitDispatchIntegration(DispatchRegistry& registry);

    // --- Trait-based dispatch ---

    // Register a trait method implementation as a dispatchable function.
    // This creates FunctionSignatures from TraitImplementation method entries
    // and registers them with the DispatchRegistry.
    std::expected<void, std::string>
    register_trait_implementation(const std::shared_ptr<meta::TraitImplementation>& impl);

    // Resolve a method call through trait-based dispatch.
    // First checks trait implementations, then falls back to the standard registry.
    std::expected<std::shared_ptr<FunctionSignature>, std::string>
    resolve_trait_dispatch(const std::string& method_name,
                           const std::shared_ptr<meta::MetaType>& receiver_type,
                           const std::vector<std::shared_ptr<meta::MetaType>>& arg_types) const;

    // --- Ownership-aware method resolution ---

    // Resolve dispatch considering ownership qualifiers.
    // Owned<T> vs Borrowed<T> can select different implementations.
    std::expected<std::shared_ptr<FunctionSignature>, std::string>
    resolve_with_ownership(const std::string& method_name,
                           const std::vector<std::shared_ptr<meta::MetaType>>& arg_types,
                           const std::vector<OwnershipQualifier>& qualifiers) const;

    // Register an ownership-qualified function signature.
    void register_ownership_qualified(std::shared_ptr<FunctionSignature> signature,
                                      const std::vector<OwnershipQualifier>& qualifiers);

    // --- Dispatch cache / optimization ---

    // Look up a cached dispatch result. Returns nullopt on cache miss.
    std::optional<std::shared_ptr<FunctionSignature>>
    lookup_cache(const std::string& name,
                 const std::vector<std::shared_ptr<meta::MetaType>>& arg_types,
                 const std::vector<OwnershipQualifier>& qualifiers) const;

    // Get cache statistics
    size_t cache_size() const;
    size_t cache_hits() const;
    size_t cache_misses() const;

    // Clear the dispatch cache
    void clear_cache();

    // --- Monomorphization hints ---

    // Record that a generic function was called with specific types,
    // hinting the compiler to monomorphize.
    void record_monomorphization_hint(const std::string& function_name,
                                       const std::vector<std::shared_ptr<meta::MetaType>>& concrete_types);

    // Get recorded monomorphization hints for a function
    std::vector<std::vector<std::shared_ptr<meta::MetaType>>>
    get_monomorphization_hints(const std::string& function_name) const;

    // --- Integration with DispatchRegistry ---

    // Get the underlying dispatch registry
    DispatchRegistry& registry();
    const DispatchRegistry& registry() const;

private:
    DispatchRegistry& registry_;

    // Ownership-qualified signatures: maps (name, qualifiers) -> signatures
    struct QualifiedEntry {
        std::shared_ptr<FunctionSignature> signature;
        std::vector<OwnershipQualifier> qualifiers;
    };
    std::unordered_map<std::string, std::vector<QualifiedEntry>> qualified_signatures_;

    // Dispatch cache
    mutable std::unordered_map<DispatchCacheKey, DispatchCacheEntry> cache_;
    mutable size_t cache_hits_ = 0;
    mutable size_t cache_misses_ = 0;

    // Monomorphization hints
    std::unordered_map<std::string, std::vector<std::vector<std::shared_ptr<meta::MetaType>>>> mono_hints_;

    // Trait implementation tracking
    std::vector<std::shared_ptr<meta::TraitImplementation>> registered_impls_;

    mutable std::shared_mutex mutex_;

    // Internal helpers
    DispatchCacheKey make_cache_key(
        const std::string& name,
        const std::vector<std::shared_ptr<meta::MetaType>>& arg_types,
        const std::vector<OwnershipQualifier>& qualifiers) const;

    void cache_result(const DispatchCacheKey& key,
                      std::shared_ptr<FunctionSignature> sig) const;

    // Score how well ownership qualifiers match
    int ownership_match_score(const std::vector<OwnershipQualifier>& required,
                              const std::vector<OwnershipQualifier>& provided) const;
};

} // namespace meld::kernel
