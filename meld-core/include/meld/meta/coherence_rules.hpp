#pragma once

#include "advanced_traits.hpp"
#include <string>
#include <memory>
#include <vector>
#include <map>
#include <mutex>
#include <set>
#include <expected>

namespace meld::meta {

// Forward declarations
class OrphanRuleChecker;
class ConflictDetector;
class BlanketImplementationChecker;

// Crate/module information for orphan rule checking
struct CrateInfo {
    std::string name;
    std::string version;
    std::vector<std::string> dependencies;
    
    CrateInfo() = default;
    CrateInfo(std::string n, std::string v = "1.0.0", std::vector<std::string> deps = {})
        : name(std::move(n)), version(std::move(v)), dependencies(std::move(deps)) {}
};

// Ownership information for types and traits
struct OwnershipInfo {
    std::string crate_name;
    std::string module_path;
    bool is_local;  // True if defined in current crate
    
    OwnershipInfo() : is_local(false) {}
    OwnershipInfo(std::string crate, std::string module = "", bool local = false)
        : crate_name(std::move(crate)), module_path(std::move(module)), is_local(local) {}
};

// Enhanced orphan rule checker
class OrphanRuleChecker {
public:
    // Register ownership information for types and traits
    void register_type_ownership(std::shared_ptr<MetaType> type, const OwnershipInfo& ownership);
    void register_trait_ownership(std::shared_ptr<AdvancedTraitMetaType> trait, const OwnershipInfo& ownership);
    
    // Check orphan rule for a trait implementation
    std::expected<void, std::string> check_orphan_rule(const TraitImplementation& impl) const;
    
    // Check orphan rule for blanket implementations
    std::expected<void, std::string> check_blanket_orphan_rule(const BlanketImplementation& blanket_impl) const;
    
    // Get ownership info
    std::expected<OwnershipInfo, std::string> get_type_ownership(const std::shared_ptr<MetaType>& type) const;
    std::expected<OwnershipInfo, std::string> get_trait_ownership(const std::shared_ptr<AdvancedTraitMetaType>& trait) const;
    
private:
    std::map<std::shared_ptr<MetaType>, OwnershipInfo> type_ownership_;
    std::map<std::shared_ptr<AdvancedTraitMetaType>, OwnershipInfo> trait_ownership_;
    mutable std::mutex mutex_;
};

// Conflict detection for trait implementations
class ConflictDetector {
public:
    // Check for direct conflicts (same type implementing same trait)
    std::expected<void, std::string> check_direct_conflict(
        const std::shared_ptr<MetaType>& type,
        const std::shared_ptr<AdvancedTraitMetaType>& trait,
        const std::vector<std::shared_ptr<TraitImplementation>>& existing_impls) const;
    
    // Check for blanket implementation conflicts
    std::expected<void, std::string> check_blanket_conflicts(
        const BlanketImplementation& new_blanket,
        const std::vector<std::shared_ptr<BlanketImplementation>>& existing_blankets) const;
    
    // Check for conflicts between explicit and blanket implementations
    std::expected<void, std::string> check_explicit_vs_blanket_conflict(
        const TraitImplementation& explicit_impl,
        const std::vector<std::shared_ptr<BlanketImplementation>>& blanket_impls) const;
    
    // Check for overlapping blanket implementations
    std::expected<void, std::string> check_overlapping_blankets(
        const BlanketImplementation& blanket1,
        const BlanketImplementation& blanket2) const;
    
private:
    // Helper to check if two type predicates might overlap
    bool predicates_might_overlap(
        const std::function<bool(const std::shared_ptr<MetaType>&)>& pred1,
        const std::function<bool(const std::shared_ptr<MetaType>&)>& pred2) const;
};

// Blanket implementation validation
class BlanketImplementationChecker {
public:
    // Validate that a blanket implementation is well-formed
    std::expected<void, std::string> validate_blanket_implementation(const BlanketImplementation& blanket_impl) const;
    
    // Check that blanket implementation doesn't violate coherence
    std::expected<void, std::string> check_blanket_coherence(
        const BlanketImplementation& blanket_impl,
        const std::vector<std::shared_ptr<TraitImplementation>>& existing_impls,
        const std::vector<std::shared_ptr<BlanketImplementation>>& existing_blankets) const;
    
    // Check for fundamental type conflicts (e.g., implementing conflicting traits)
    std::expected<void, std::string> check_fundamental_conflicts(const BlanketImplementation& blanket_impl) const;
    
private:
    // Check if a blanket implementation is too broad
    bool is_too_broad(const BlanketImplementation& blanket_impl) const;
};

// Enhanced coherence checker with comprehensive rule enforcement
class EnhancedCoherenceChecker {
public:
    EnhancedCoherenceChecker();
    
    // Register crate information
    void register_crate(const CrateInfo& crate_info);
    void set_current_crate(const std::string& crate_name);
    
    // Register ownership information
    void register_type_ownership(std::shared_ptr<MetaType> type, const OwnershipInfo& ownership);
    void register_trait_ownership(std::shared_ptr<AdvancedTraitMetaType> trait, const OwnershipInfo& ownership);
    
    // Comprehensive coherence checking
    std::expected<void, std::string> check_implementation_coherence(const TraitImplementation& impl);
    std::expected<void, std::string> check_blanket_implementation_coherence(const BlanketImplementation& blanket_impl);
    
    // Register implementations with full validation
    std::expected<void, std::string> register_implementation(std::shared_ptr<TraitImplementation> impl);
    std::expected<void, std::string> register_blanket_implementation(std::shared_ptr<BlanketImplementation> blanket_impl);
    
    // Query implementations
    std::expected<std::shared_ptr<TraitImplementation>, std::string>
    find_implementation(const std::shared_ptr<MetaType>& type,
                        const std::shared_ptr<AdvancedTraitMetaType>& trait) const;
    
    // Get all implementations
    std::vector<std::shared_ptr<TraitImplementation>> get_all_implementations() const;
    std::vector<std::shared_ptr<BlanketImplementation>> get_all_blanket_implementations() const;
    
    // Coherence validation for the entire system
    std::expected<void, std::string> validate_global_coherence() const;
    
private:
    OrphanRuleChecker orphan_checker_;
    ConflictDetector conflict_detector_;
    BlanketImplementationChecker blanket_checker_;
    
    // Implementation storage
    std::map<std::pair<std::shared_ptr<MetaType>, std::shared_ptr<AdvancedTraitMetaType>>, 
             std::shared_ptr<TraitImplementation>> implementations_;
    std::vector<std::shared_ptr<BlanketImplementation>> blanket_implementations_;
    
    // Crate management
    std::map<std::string, CrateInfo> crates_;
    std::string current_crate_;
    
    mutable std::mutex mutex_;
};

// Coherence error types
enum class CoherenceErrorType {
    OrphanRuleViolation,
    ConflictingImplementation,
    OverlappingBlanketImplementation,
    InvalidBlanketImplementation,
    CircularDependency,
    MissingDependency
};

struct CoherenceError {
    CoherenceErrorType type;
    std::string message;
    std::string crate_name;
    std::string location;
    
    CoherenceError(CoherenceErrorType t, std::string msg, std::string crate = "", std::string loc = "")
        : type(t), message(std::move(msg)), crate_name(std::move(crate)), location(std::move(loc)) {}
};

// Coherence validation result
struct CoherenceValidationResult {
    bool is_valid;
    std::vector<CoherenceError> errors;
    std::vector<std::string> warnings;
    
    CoherenceValidationResult(bool valid = true) : is_valid(valid) {}
    
    void add_error(const CoherenceError& error) {
        errors.push_back(error);
        is_valid = false;
    }
    
    void add_warning(const std::string& warning) {
        warnings.push_back(warning);
    }
};

// Global coherence validator
class GlobalCoherenceValidator {
public:
    // Validate coherence across all registered implementations
    CoherenceValidationResult validate_all_implementations() const;
    
    // Validate coherence for a specific crate
    CoherenceValidationResult validate_crate(const std::string& crate_name) const;
    
    // Check for potential future conflicts
    std::vector<std::string> predict_potential_conflicts(
        const TraitImplementation& proposed_impl) const;
    
    // Generate coherence report
    std::string generate_coherence_report() const;
    
private:
    // Helper methods for validation
    void check_orphan_rules(CoherenceValidationResult& result) const;
    void check_implementation_conflicts(CoherenceValidationResult& result) const;
    void check_blanket_implementation_coherence(CoherenceValidationResult& result) const;
    void check_circular_dependencies(CoherenceValidationResult& result) const;
};

} // namespace meld::meta