#include "meld/meta/coherence_rules.hpp"
#include <algorithm>
#include <ranges>
#include <format>
#include <sstream>

namespace meld::meta {

// OrphanRuleChecker implementation
void OrphanRuleChecker::register_type_ownership(std::shared_ptr<MetaType> type, const OwnershipInfo& ownership) {
    std::lock_guard<std::mutex> lock(mutex_);
    type_ownership_[type] = ownership;
}

void OrphanRuleChecker::register_trait_ownership(std::shared_ptr<AdvancedTraitMetaType> trait, const OwnershipInfo& ownership) {
    std::lock_guard<std::mutex> lock(mutex_);
    trait_ownership_[trait] = ownership;
}

std::expected<void, std::string> OrphanRuleChecker::check_orphan_rule(const TraitImplementation& impl) const {
    auto type_ownership = get_type_ownership(impl.implementing_type());
    auto trait_ownership = get_trait_ownership(impl.trait_type());
    
    if (!type_ownership || !trait_ownership) {
        return std::unexpected("Cannot determine ownership for orphan rule check");
    }
    
    const auto& type_owner = type_ownership.value();
    const auto& trait_owner = trait_ownership.value();
    
    // Orphan rule: You can implement a trait for a type if:
    // 1. You own the trait, OR
    // 2. You own the type, OR  
    // 3. The trait is a fundamental trait (like Copy, Clone)
    
    if (type_owner.is_local || trait_owner.is_local) {
        return {}; // OK - we own either the type or the trait
    }
    
    // Check for fundamental traits that can be implemented anywhere
    const std::vector<std::string> fundamental_traits = {"Copy", "Clone", "Debug", "Default"};
    if (std::ranges::find(fundamental_traits, impl.trait_type()->name()) != fundamental_traits.end()) {
        return {}; // OK - fundamental trait
    }
    
    return std::unexpected(std::format(
        "Orphan rule violation: Cannot implement trait '{}' from crate '{}' for type '{}' from crate '{}' in current crate",
        impl.trait_type()->name(), trait_owner.crate_name,
        impl.implementing_type()->name(), type_owner.crate_name));
}

std::expected<void, std::string> OrphanRuleChecker::check_blanket_orphan_rule(const BlanketImplementation& blanket_impl) const {
    auto trait_ownership = get_trait_ownership(blanket_impl.trait_type());
    
    if (!trait_ownership) {
        return std::unexpected("Cannot determine trait ownership for blanket implementation orphan rule check");
    }
    
    const auto& trait_owner = trait_ownership.value();
    
    // Blanket implementations can only be defined in the same crate as the trait
    if (!trait_owner.is_local) {
        return std::unexpected(std::format(
            "Orphan rule violation: Cannot define blanket implementation for trait '{}' from external crate '{}'",
            blanket_impl.trait_type()->name(), trait_owner.crate_name));
    }
    
    return {};
}

std::expected<OwnershipInfo, std::string> OrphanRuleChecker::get_type_ownership(const std::shared_ptr<MetaType>& type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = type_ownership_.find(type);
    if (it != type_ownership_.end()) {
        return it->second;
    }
    return std::unexpected(std::format("No ownership information found for type '{}'", type->name()));
}

std::expected<OwnershipInfo, std::string> OrphanRuleChecker::get_trait_ownership(const std::shared_ptr<AdvancedTraitMetaType>& trait) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = trait_ownership_.find(trait);
    if (it != trait_ownership_.end()) {
        return it->second;
    }
    return std::unexpected(std::format("No ownership information found for trait '{}'", trait->name()));
}

// ConflictDetector implementation
std::expected<void, std::string> ConflictDetector::check_direct_conflict(
    const std::shared_ptr<MetaType>& type,
    const std::shared_ptr<AdvancedTraitMetaType>& trait,
    const std::vector<std::shared_ptr<TraitImplementation>>& existing_impls) const {
    
    for (const auto& existing_impl : existing_impls) {
        if (existing_impl->implementing_type() == type && existing_impl->trait_type() == trait) {
            return std::unexpected(std::format(
                "Conflicting implementation: trait '{}' is already implemented for type '{}'",
                trait->name(), type->name()));
        }
    }
    
    return {};
}

std::expected<void, std::string> ConflictDetector::check_blanket_conflicts(
    const BlanketImplementation& new_blanket,
    const std::vector<std::shared_ptr<BlanketImplementation>>& existing_blankets) const {
    
    for (const auto& existing_blanket : existing_blankets) {
        if (existing_blanket->trait_type() == new_blanket.trait_type()) {
            // Check for overlapping predicates
            auto overlap_result = check_overlapping_blankets(new_blanket, *existing_blanket);
            if (!overlap_result) {
                return overlap_result;
            }
        }
    }
    
    return {};
}

std::expected<void, std::string> ConflictDetector::check_explicit_vs_blanket_conflict(
    const TraitImplementation& explicit_impl,
    const std::vector<std::shared_ptr<BlanketImplementation>>& blanket_impls) const {
    
    for (const auto& blanket_impl : blanket_impls) {
        if (blanket_impl->trait_type() == explicit_impl.trait_type() &&
            blanket_impl->applies_to_type(explicit_impl.implementing_type())) {
            return std::unexpected(std::format(
                "Conflicting implementation: explicit implementation of trait '{}' for type '{}' conflicts with blanket implementation",
                explicit_impl.trait_type()->name(), explicit_impl.implementing_type()->name()));
        }
    }
    
    return {};
}

std::expected<void, std::string> ConflictDetector::check_overlapping_blankets(
    const BlanketImplementation& blanket1,
    const BlanketImplementation& blanket2) const {
    
    // This is a simplified check - in practice, we'd need more sophisticated overlap detection
    // For now, we assume that if they're for the same trait, they might overlap
    if (blanket1.trait_type() == blanket2.trait_type()) {
        return std::unexpected(std::format(
            "Potentially overlapping blanket implementations for trait '{}'",
            blanket1.trait_type()->name()));
    }
    
    return {};
}

bool ConflictDetector::predicates_might_overlap(
    const std::function<bool(const std::shared_ptr<MetaType>&)>& pred1,
    const std::function<bool(const std::shared_ptr<MetaType>&)>& pred2) const {
    
    // This is a placeholder - real implementation would need sophisticated analysis
    // For now, we conservatively assume they might overlap
    return true;
}

// BlanketImplementationChecker implementation
std::expected<void, std::string> BlanketImplementationChecker::validate_blanket_implementation(const BlanketImplementation& blanket_impl) const {
    // Check that the blanket implementation is not too broad
    if (is_too_broad(blanket_impl)) {
        return std::unexpected(std::format(
            "Blanket implementation for trait '{}' is too broad and might cause coherence issues",
            blanket_impl.trait_type()->name()));
    }
    
    return {};
}

std::expected<void, std::string> BlanketImplementationChecker::check_blanket_coherence(
    const BlanketImplementation& blanket_impl,
    const std::vector<std::shared_ptr<TraitImplementation>>& existing_impls,
    const std::vector<std::shared_ptr<BlanketImplementation>>& existing_blankets) const {
    
    ConflictDetector detector;
    
    // Check conflicts with existing blanket implementations
    auto blanket_conflict_result = detector.check_blanket_conflicts(blanket_impl, existing_blankets);
    if (!blanket_conflict_result) {
        return blanket_conflict_result;
    }
    
    // Check conflicts with explicit implementations
    for (const auto& explicit_impl : existing_impls) {
        if (blanket_impl.trait_type() == explicit_impl->trait_type() &&
            blanket_impl.applies_to_type(explicit_impl->implementing_type())) {
            return std::unexpected(std::format(
                "Blanket implementation conflicts with explicit implementation of trait '{}' for type '{}'",
                blanket_impl.trait_type()->name(), explicit_impl->implementing_type()->name()));
        }
    }
    
    return {};
}

std::expected<void, std::string> BlanketImplementationChecker::check_fundamental_conflicts(const BlanketImplementation& blanket_impl) const {
    // Check for conflicts with fundamental language features
    // This is a placeholder for more sophisticated checks
    return {};
}

bool BlanketImplementationChecker::is_too_broad(const BlanketImplementation& blanket_impl) const {
    // Heuristic: if the predicate always returns true, it's too broad
    // In practice, we'd analyze the predicate more carefully
    return false;
}

// EnhancedCoherenceChecker implementation
EnhancedCoherenceChecker::EnhancedCoherenceChecker() = default;

void EnhancedCoherenceChecker::register_crate(const CrateInfo& crate_info) {
    std::lock_guard<std::mutex> lock(mutex_);
    crates_[crate_info.name] = crate_info;
}

void EnhancedCoherenceChecker::set_current_crate(const std::string& crate_name) {
    current_crate_ = crate_name;
}

void EnhancedCoherenceChecker::register_type_ownership(std::shared_ptr<MetaType> type, const OwnershipInfo& ownership) {
    orphan_checker_.register_type_ownership(type, ownership);
}

void EnhancedCoherenceChecker::register_trait_ownership(std::shared_ptr<AdvancedTraitMetaType> trait, const OwnershipInfo& ownership) {
    orphan_checker_.register_trait_ownership(trait, ownership);
}

std::expected<void, std::string> EnhancedCoherenceChecker::check_implementation_coherence(const TraitImplementation& impl) {
    // Check orphan rule
    auto orphan_result = orphan_checker_.check_orphan_rule(impl);
    if (!orphan_result) {
        return orphan_result;
    }
    
    // Check for direct conflicts
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<TraitImplementation>> existing_impls;
    for (const auto& [key, existing_impl] : implementations_) {
        existing_impls.push_back(existing_impl);
    }
    
    auto conflict_result = conflict_detector_.check_direct_conflict(
        impl.implementing_type(), impl.trait_type(), existing_impls);
    if (!conflict_result) {
        return conflict_result;
    }
    
    // Check conflicts with blanket implementations
    auto blanket_conflict_result = conflict_detector_.check_explicit_vs_blanket_conflict(impl, blanket_implementations_);
    if (!blanket_conflict_result) {
        return blanket_conflict_result;
    }
    
    return {};
}

std::expected<void, std::string> EnhancedCoherenceChecker::check_blanket_implementation_coherence(const BlanketImplementation& blanket_impl) {
    // Check orphan rule for blanket implementations
    auto orphan_result = orphan_checker_.check_blanket_orphan_rule(blanket_impl);
    if (!orphan_result) {
        return orphan_result;
    }
    
    // Validate the blanket implementation
    auto validation_result = blanket_checker_.validate_blanket_implementation(blanket_impl);
    if (!validation_result) {
        return validation_result;
    }
    
    // Check coherence with existing implementations
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<TraitImplementation>> existing_impls;
    for (const auto& [key, existing_impl] : implementations_) {
        existing_impls.push_back(existing_impl);
    }
    
    auto coherence_result = blanket_checker_.check_blanket_coherence(blanket_impl, existing_impls, blanket_implementations_);
    if (!coherence_result) {
        return coherence_result;
    }
    
    return {};
}

std::expected<void, std::string> EnhancedCoherenceChecker::register_implementation(std::shared_ptr<TraitImplementation> impl) {
    // Comprehensive coherence checking
    auto coherence_result = check_implementation_coherence(*impl);
    if (!coherence_result) {
        return coherence_result;
    }
    
    // Validate the implementation itself
    auto validation_result = impl->validate();
    if (!validation_result) {
        return validation_result;
    }
    
    // Register the implementation
    std::lock_guard<std::mutex> lock(mutex_);
    auto key = std::make_pair(impl->implementing_type(), impl->trait_type());
    implementations_[key] = std::move(impl);
    
    return {};
}

std::expected<void, std::string> EnhancedCoherenceChecker::register_blanket_implementation(std::shared_ptr<BlanketImplementation> blanket_impl) {
    // Comprehensive coherence checking
    auto coherence_result = check_blanket_implementation_coherence(*blanket_impl);
    if (!coherence_result) {
        return coherence_result;
    }
    
    // Register the blanket implementation
    std::lock_guard<std::mutex> lock(mutex_);
    blanket_implementations_.push_back(std::move(blanket_impl));
    
    return {};
}

std::expected<std::shared_ptr<TraitImplementation>, std::string>
EnhancedCoherenceChecker::find_implementation(const std::shared_ptr<MetaType>& type,
                                              const std::shared_ptr<AdvancedTraitMetaType>& trait) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // First check explicit implementations
    auto key = std::make_pair(type, trait);
    auto it = implementations_.find(key);
    if (it != implementations_.end()) {
        return it->second;
    }
    
    // Check blanket implementations
    for (const auto& blanket_impl : blanket_implementations_) {
        if (blanket_impl->trait_type() == trait && blanket_impl->applies_to_type(type)) {
            return blanket_impl->instantiate_for_type(type);
        }
    }
    
    return std::unexpected(std::format("No implementation found for trait '{}' on type '{}'",
        trait->name(), type->name()));
}

std::vector<std::shared_ptr<TraitImplementation>> EnhancedCoherenceChecker::get_all_implementations() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<TraitImplementation>> result;
    for (const auto& [key, impl] : implementations_) {
        result.push_back(impl);
    }
    return result;
}

std::vector<std::shared_ptr<BlanketImplementation>> EnhancedCoherenceChecker::get_all_blanket_implementations() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return blanket_implementations_;
}

std::expected<void, std::string> EnhancedCoherenceChecker::validate_global_coherence() const {
    // This would perform a comprehensive validation of all implementations
    // For now, we'll do a basic check
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check for any obvious conflicts
    for (const auto& [key1, impl1] : implementations_) {
        for (const auto& [key2, impl2] : implementations_) {
            if (key1 != key2 && 
                impl1->implementing_type() == impl2->implementing_type() &&
                impl1->trait_type() == impl2->trait_type()) {
                return std::unexpected(std::format(
                    "Global coherence violation: duplicate implementations found for trait '{}' on type '{}'",
                    impl1->trait_type()->name(), impl1->implementing_type()->name()));
            }
        }
    }
    
    return {};
}

// GlobalCoherenceValidator implementation
CoherenceValidationResult GlobalCoherenceValidator::validate_all_implementations() const {
    CoherenceValidationResult result;
    
    check_orphan_rules(result);
    check_implementation_conflicts(result);
    check_blanket_implementation_coherence(result);
    check_circular_dependencies(result);
    
    return result;
}

CoherenceValidationResult GlobalCoherenceValidator::validate_crate(const std::string& crate_name) const {
    CoherenceValidationResult result;
    
    // This would validate coherence for a specific crate
    // For now, we'll return a successful result
    result.add_warning(std::format("Crate-specific validation for '{}' not fully implemented", crate_name));
    
    return result;
}

std::vector<std::string> GlobalCoherenceValidator::predict_potential_conflicts(
    const TraitImplementation& proposed_impl) const {
    
    std::vector<std::string> potential_conflicts;
    
    // This would analyze the proposed implementation and predict potential future conflicts
    // For now, we'll return an empty list
    
    return potential_conflicts;
}

std::string GlobalCoherenceValidator::generate_coherence_report() const {
    std::ostringstream report;
    
    report << "=== Coherence Report ===\n";
    report << "Global coherence validation: ";
    
    auto validation_result = validate_all_implementations();
    if (validation_result.is_valid) {
        report << "PASSED\n";
    } else {
        report << "FAILED\n";
        report << "\nErrors:\n";
        for (const auto& error : validation_result.errors) {
            report << "  - " << error.message << "\n";
        }
    }
    
    if (!validation_result.warnings.empty()) {
        report << "\nWarnings:\n";
        for (const auto& warning : validation_result.warnings) {
            report << "  - " << warning << "\n";
        }
    }
    
    return report.str();
}

void GlobalCoherenceValidator::check_orphan_rules(CoherenceValidationResult& result) const {
    // This would check orphan rules across all implementations
    // For now, we'll add a placeholder warning
    result.add_warning("Orphan rule checking not fully implemented");
}

void GlobalCoherenceValidator::check_implementation_conflicts(CoherenceValidationResult& result) const {
    // This would check for implementation conflicts
    // For now, we'll add a placeholder warning
    result.add_warning("Implementation conflict checking not fully implemented");
}

void GlobalCoherenceValidator::check_blanket_implementation_coherence(CoherenceValidationResult& result) const {
    // This would check blanket implementation coherence
    // For now, we'll add a placeholder warning
    result.add_warning("Blanket implementation coherence checking not fully implemented");
}

void GlobalCoherenceValidator::check_circular_dependencies(CoherenceValidationResult& result) const {
    // This would check for circular dependencies in trait hierarchies
    // For now, we'll add a placeholder warning
    result.add_warning("Circular dependency checking not fully implemented");
}

} // namespace meld::meta