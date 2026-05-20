#include "meld/kernel/trait_dispatch.hpp"
#include <algorithm>
#include <numeric>
#include <sstream>

namespace meld::kernel {

// --- DispatchCacheKey ---

bool DispatchCacheKey::operator==(const DispatchCacheKey& other) const {
    return function_name == other.function_name &&
           arg_type_names == other.arg_type_names &&
           ownership_qualifiers == other.ownership_qualifiers;
}

} // namespace meld::kernel

namespace std {

size_t hash<meld::kernel::DispatchCacheKey>::operator()(
    const meld::kernel::DispatchCacheKey& key) const {
    size_t h = hash<string>()(key.function_name);
    for (const auto& tn : key.arg_type_names) {
        h ^= hash<string>()(tn) + 0x9e3779b9 + (h << 6) + (h >> 2);
    }
    for (auto q : key.ownership_qualifiers) {
        h ^= hash<int>()(static_cast<int>(q)) + 0x9e3779b9 + (h << 6) + (h >> 2);
    }
    return h;
}

} // namespace std

namespace meld::kernel {

// --- TraitDispatchIntegration ---

TraitDispatchIntegration::TraitDispatchIntegration()
    : registry_(DispatchRegistry::instance()) {}

TraitDispatchIntegration::TraitDispatchIntegration(DispatchRegistry& registry)
    : registry_(registry) {}

// --- Trait-based dispatch ---

std::expected<void, std::string>
TraitDispatchIntegration::register_trait_implementation(
    const std::shared_ptr<meta::TraitImplementation>& impl) {

    if (!impl) {
        return std::unexpected(std::string("Cannot register null trait implementation"));
    }

    std::unique_lock lock(mutex_);

    const auto& trait = impl->trait_type();
    const auto& implementing_type = impl->implementing_type();

    // Validate the implementation
    auto validation = impl->validate();
    if (!validation.has_value()) {
        return std::unexpected(validation.error());
    }

    // For each method in the trait, create a FunctionSignature and register it
    for (const auto& method : trait->get_all_methods()) {
        // Check if the implementation provides this method
        auto method_impl = impl->get_method_implementation(method.name);
        Value impl_value;
        if (method_impl.has_value()) {
            impl_value = *method_impl;
        } else {
            // Use the default implementation from the trait if available
            impl_value = method.implementation;
        }

        // Build parameter types: prepend the implementing type as the receiver
        std::vector<std::shared_ptr<meta::MetaType>> param_types;
        param_types.push_back(implementing_type);
        for (const auto& pt : method.param_types) {
            param_types.push_back(pt);
        }

        // Construct a qualified name: TraitName::method_name
        std::string qualified_name = trait->name() + "::" + method.name;

        auto sig = std::make_shared<FunctionSignature>(
            qualified_name,
            std::move(param_types),
            method.return_type,
            impl_value
        );

        registry_.register_function(sig);

        // Also register under the bare method name for convenience
        auto bare_sig = std::make_shared<FunctionSignature>(
            method.name,
            sig->param_types,  // copy
            method.return_type,
            impl_value
        );
        registry_.register_function(bare_sig);
    }

    registered_impls_.push_back(impl);
    // Invalidate cache since new signatures were added
    cache_.clear();

    return {};
}

std::expected<std::shared_ptr<FunctionSignature>, std::string>
TraitDispatchIntegration::resolve_trait_dispatch(
    const std::string& method_name,
    const std::shared_ptr<meta::MetaType>& receiver_type,
    const std::vector<std::shared_ptr<meta::MetaType>>& arg_types) const {

    std::shared_lock lock(mutex_);

    // Build full argument list with receiver as first argument
    std::vector<std::shared_ptr<meta::MetaType>> full_args;
    full_args.push_back(receiver_type);
    full_args.insert(full_args.end(), arg_types.begin(), arg_types.end());

    // First, try to find a trait-qualified method
    for (const auto& impl : registered_impls_) {
        const auto& trait = impl->trait_type();
        std::string qualified_name = trait->name() + "::" + method_name;

        auto result = registry_.resolve(qualified_name, full_args);
        if (result.has_value()) {
            return result;
        }
    }

    // Fall back to bare method name in the dispatch registry
    return registry_.resolve(method_name, full_args);
}

// --- Ownership-aware method resolution ---

std::expected<std::shared_ptr<FunctionSignature>, std::string>
TraitDispatchIntegration::resolve_with_ownership(
    const std::string& method_name,
    const std::vector<std::shared_ptr<meta::MetaType>>& arg_types,
    const std::vector<OwnershipQualifier>& qualifiers) const {

    std::shared_lock lock(mutex_);

    // Check cache first
    auto cache_key = make_cache_key(method_name, arg_types, qualifiers);
    auto cache_it = cache_.find(cache_key);
    if (cache_it != cache_.end()) {
        cache_hits_++;
        cache_it->second.hit_count++;
        return cache_it->second.signature;
    }
    cache_misses_++;

    // Look for ownership-qualified signatures
    auto it = qualified_signatures_.find(method_name);
    if (it != qualified_signatures_.end()) {
        // Find the best matching qualified signature
        std::shared_ptr<FunctionSignature> best_match;
        int best_score = -1;

        for (const auto& entry : it->second) {
            // Check that the base types match
            if (!entry.signature->matches(arg_types)) {
                continue;
            }

            // Score the ownership qualifier match
            int score = ownership_match_score(entry.qualifiers, qualifiers);
            if (score < 0) {
                continue; // Incompatible qualifiers
            }

            // Combine with type specificity
            int type_score = entry.signature->total_specificity(arg_types);
            if (type_score < 0) {
                continue;
            }

            int total_score = type_score + score * 100; // Weight ownership heavily
            if (total_score > best_score) {
                best_score = total_score;
                best_match = entry.signature;
            }
        }

        if (best_match) {
            cache_result(cache_key, best_match);
            return best_match;
        }
    }

    // Fall back to standard dispatch (ignoring ownership qualifiers)
    auto result = registry_.resolve(method_name, arg_types);
    if (result.has_value()) {
        cache_result(cache_key, *result);
    }
    return result;
}

void TraitDispatchIntegration::register_ownership_qualified(
    std::shared_ptr<FunctionSignature> signature,
    const std::vector<OwnershipQualifier>& qualifiers) {

    std::unique_lock lock(mutex_);

    QualifiedEntry entry;
    entry.signature = std::move(signature);
    entry.qualifiers = qualifiers;

    qualified_signatures_[entry.signature->name].push_back(std::move(entry));

    // Invalidate cache
    cache_.clear();
}

// --- Dispatch cache / optimization ---

std::optional<std::shared_ptr<FunctionSignature>>
TraitDispatchIntegration::lookup_cache(
    const std::string& name,
    const std::vector<std::shared_ptr<meta::MetaType>>& arg_types,
    const std::vector<OwnershipQualifier>& qualifiers) const {

    std::shared_lock lock(mutex_);

    auto key = make_cache_key(name, arg_types, qualifiers);
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        cache_hits_++;
        it->second.hit_count++;
        return it->second.signature;
    }
    cache_misses_++;
    return std::nullopt;
}

size_t TraitDispatchIntegration::cache_size() const {
    std::shared_lock lock(mutex_);
    return cache_.size();
}

size_t TraitDispatchIntegration::cache_hits() const {
    std::shared_lock lock(mutex_);
    return cache_hits_;
}

size_t TraitDispatchIntegration::cache_misses() const {
    std::shared_lock lock(mutex_);
    return cache_misses_;
}

void TraitDispatchIntegration::clear_cache() {
    std::unique_lock lock(mutex_);
    cache_.clear();
    cache_hits_ = 0;
    cache_misses_ = 0;
}

// --- Monomorphization hints ---

void TraitDispatchIntegration::record_monomorphization_hint(
    const std::string& function_name,
    const std::vector<std::shared_ptr<meta::MetaType>>& concrete_types) {

    std::unique_lock lock(mutex_);

    auto& hints = mono_hints_[function_name];

    // Avoid duplicate hints
    for (const auto& existing : hints) {
        if (existing.size() != concrete_types.size()) continue;
        bool same = true;
        for (size_t i = 0; i < existing.size(); ++i) {
            if (existing[i]->name() != concrete_types[i]->name()) {
                same = false;
                break;
            }
        }
        if (same) return; // Already recorded
    }

    hints.push_back(concrete_types);
}

std::vector<std::vector<std::shared_ptr<meta::MetaType>>>
TraitDispatchIntegration::get_monomorphization_hints(const std::string& function_name) const {
    std::shared_lock lock(mutex_);
    auto it = mono_hints_.find(function_name);
    if (it == mono_hints_.end()) {
        return {};
    }
    return it->second;
}

// --- Integration with DispatchRegistry ---

DispatchRegistry& TraitDispatchIntegration::registry() {
    return registry_;
}

const DispatchRegistry& TraitDispatchIntegration::registry() const {
    return registry_;
}

// --- Internal helpers ---

DispatchCacheKey TraitDispatchIntegration::make_cache_key(
    const std::string& name,
    const std::vector<std::shared_ptr<meta::MetaType>>& arg_types,
    const std::vector<OwnershipQualifier>& qualifiers) const {

    DispatchCacheKey key;
    key.function_name = name;
    key.arg_type_names.reserve(arg_types.size());
    for (const auto& t : arg_types) {
        key.arg_type_names.push_back(t->name());
    }
    key.ownership_qualifiers = qualifiers;
    return key;
}

void TraitDispatchIntegration::cache_result(
    const DispatchCacheKey& key,
    std::shared_ptr<FunctionSignature> sig) const {

    DispatchCacheEntry entry;
    entry.signature = std::move(sig);
    entry.timestamp = std::chrono::steady_clock::now();
    entry.hit_count = 0;
    cache_[key] = std::move(entry);
}

int TraitDispatchIntegration::ownership_match_score(
    const std::vector<OwnershipQualifier>& required,
    const std::vector<OwnershipQualifier>& provided) const {

    if (required.size() != provided.size()) {
        return -1;
    }

    int score = 0;
    for (size_t i = 0; i < required.size(); ++i) {
        if (required[i] == provided[i]) {
            score += 10; // Exact match
        } else if (required[i] == OwnershipQualifier::Borrowed &&
                   provided[i] == OwnershipQualifier::Owned) {
            // Owned can be implicitly borrowed (immutable)
            score += 5;
        } else if (required[i] == OwnershipQualifier::BorrowedMut &&
                   provided[i] == OwnershipQualifier::Owned) {
            // Owned can be mutably borrowed
            score += 3;
        } else {
            // Incompatible: e.g., BorrowedMut required but Borrowed provided
            return -1;
        }
    }
    return score;
}

} // namespace meld::kernel
