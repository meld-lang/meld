#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <functional>

namespace meld {
namespace adoption {

// Legacy pattern types that need compatibility support
enum class LegacyPattern {
    ManualMemoryManagement,    // malloc/free style
    NullableReferences,        // Using null instead of Option
    ExceptionBased,            // throw/catch instead of Result
    ImplicitCopying,           // Automatic copying without ownership
    GlobalMutableState,        // Global variables
    UncheckedCasts,           // Type casts without validation
    RawPointers               // Direct pointer manipulation
};

// Compatibility mode for legacy patterns
enum class CompatibilityMode {
    Strict,      // No legacy patterns allowed
    Warn,        // Warn about legacy patterns
    Allow,       // Allow with automatic conversion
    Transparent  // Full backward compatibility
};

// Legacy pattern detection result
struct LegacyPatternDetection {
    LegacyPattern pattern;
    std::string location;
    std::string description;
    std::string modern_alternative;
    int severity;  // 1-10, higher is more critical
};

// Compatibility layer for bridging old and new code
class LegacyCompatibilityLayer {
public:
    LegacyCompatibilityLayer();
    
    // Detect legacy patterns in code
    std::vector<LegacyPatternDetection> detect_legacy_patterns(
        const std::string& source_code
    ) const;
    
    // Convert legacy pattern to modern equivalent
    std::optional<std::string> convert_to_modern(
        LegacyPattern pattern,
        const std::string& code_snippet
    ) const;
    
    // Generate compatibility wrapper
    std::string generate_compatibility_wrapper(
        const std::string& legacy_function,
        const std::string& modern_signature
    ) const;
    
    // Check if code uses legacy patterns
    bool uses_legacy_patterns(const std::string& code) const;
    
    // Get migration path for pattern
    std::string get_migration_path(LegacyPattern pattern) const;
    
private:
    std::unordered_map<LegacyPattern, std::string> pattern_descriptions_;
    std::unordered_map<LegacyPattern, std::string> modern_alternatives_;
    
    void initialize_pattern_mappings();
};

// Migration warning system
class MigrationWarningSystem {
public:
    struct Warning {
        std::string location;
        std::string message;
        std::string suggestion;
        std::string code_example;
        int priority;  // 1-5, higher is more urgent
        LegacyPattern related_pattern;
    };
    
    MigrationWarningSystem();
    
    // Generate warnings for legacy code
    std::vector<Warning> generate_warnings(
        const std::string& module_name,
        const std::string& source_code
    ) const;
    
    // Format warning for display
    std::string format_warning(const Warning& warning) const;
    
    // Generate migration guide
    std::string generate_migration_guide(
        const std::vector<Warning>& warnings
    ) const;
    
    // Check if warning should be suppressed
    bool should_suppress_warning(
        const std::string& module,
        const Warning& warning
    ) const;
    
    // Add warning suppression
    void suppress_warning(
        const std::string& module,
        LegacyPattern pattern
    );
    
private:
    std::unordered_map<std::string, std::vector<LegacyPattern>> suppressed_warnings_;
    
    Warning create_warning_for_pattern(
        const LegacyPatternDetection& detection
    ) const;
};

// Mixed paradigm support
class MixedParadigmSupport {
public:
    MixedParadigmSupport();
    
    // Convert between owned and GC types
    std::string convert_owned_to_gc(
        const std::string& owned_expr,
        const std::string& type
    ) const;
    
    std::string convert_gc_to_owned(
        const std::string& gc_expr,
        const std::string& type
    ) const;
    
    // Generate bridge code
    std::string generate_bridge_function(
        const std::string& from_paradigm,
        const std::string& to_paradigm,
        const std::string& function_signature
    ) const;
    
    // Check if types are compatible across paradigms
    bool are_types_compatible(
        const std::string& owned_type,
        const std::string& gc_type
    ) const;
    
    // Generate interop code
    std::string generate_interop_code(
        const std::string& owned_code,
        const std::string& gc_code
    ) const;
    
    // Automatic conversion rules
    struct ConversionRule {
        std::string from_type;
        std::string to_type;
        std::string conversion_function;
        bool is_safe;
    };
    
    std::vector<ConversionRule> get_conversion_rules() const;
    
    // Add custom conversion rule
    void add_conversion_rule(const ConversionRule& rule);
    
private:
    std::vector<ConversionRule> conversion_rules_;
    
    void initialize_default_conversions();
};

// Legacy API wrapper generator
class LegacyAPIWrapper {
public:
    LegacyAPIWrapper();
    
    // Wrap legacy function with modern interface
    std::string wrap_legacy_function(
        const std::string& legacy_signature,
        const std::string& legacy_body,
        bool add_error_handling = true
    ) const;
    
    // Generate adapter for legacy class
    std::string generate_class_adapter(
        const std::string& legacy_class,
        const std::string& modern_interface
    ) const;
    
    // Create facade for legacy module
    std::string create_module_facade(
        const std::string& legacy_module,
        const std::vector<std::string>& functions_to_wrap
    ) const;
    
private:
    std::string add_result_wrapper(const std::string& function) const;
    std::string add_ownership_annotations(const std::string& function) const;
    std::string add_null_safety(const std::string& function) const;
};

// Gradual migration planner
class GradualMigrationPlanner {
public:
    struct MigrationStep {
        int step_number;
        std::string description;
        std::string action;
        std::vector<std::string> files_to_modify;
        std::string expected_outcome;
        int estimated_effort;  // hours
    };
    
    GradualMigrationPlanner();
    
    // Generate migration plan
    std::vector<MigrationStep> generate_migration_plan(
        const std::string& module_name,
        const std::vector<LegacyPatternDetection>& patterns
    ) const;
    
    // Prioritize migration steps
    std::vector<MigrationStep> prioritize_steps(
        const std::vector<MigrationStep>& steps
    ) const;
    
    // Generate step-by-step guide
    std::string generate_step_guide(
        const std::vector<MigrationStep>& steps
    ) const;
    
    // Estimate migration effort
    int estimate_total_effort(
        const std::vector<MigrationStep>& steps
    ) const;
    
private:
    int calculate_step_priority(const MigrationStep& step) const;
};

// Backward compatibility checker
class BackwardCompatibilityChecker {
public:
    BackwardCompatibilityChecker();
    
    // Check if changes break backward compatibility
    struct CompatibilityIssue {
        std::string location;
        std::string issue_type;
        std::string description;
        std::string impact;
        bool is_breaking;
    };
    
    std::vector<CompatibilityIssue> check_compatibility(
        const std::string& old_code,
        const std::string& new_code
    ) const;
    
    // Suggest compatibility preserving changes
    std::vector<std::string> suggest_compatible_changes(
        const std::vector<CompatibilityIssue>& issues
    ) const;
    
    // Generate compatibility report
    std::string generate_compatibility_report(
        const std::vector<CompatibilityIssue>& issues
    ) const;
    
private:
    bool is_signature_compatible(
        const std::string& old_sig,
        const std::string& new_sig
    ) const;
    
    bool is_type_compatible(
        const std::string& old_type,
        const std::string& new_type
    ) const;
};

} // namespace adoption
} // namespace meld
