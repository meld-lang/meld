#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <memory>

namespace meld {
namespace adoption {

// Ownership checking mode levels
enum class OwnershipMode {
    Disabled,      // No ownership checking (legacy mode)
    Warn,          // Check and warn but don't error
    Strict,        // Full ownership checking with errors
    Gradual        // Progressive checking with migration hints
};

// Per-module ownership configuration
struct ModuleOwnershipConfig {
    std::string module_name;
    OwnershipMode mode;
    std::vector<std::string> excluded_functions;
    std::vector<std::string> excluded_types;
    bool allow_mixed_paradigm;
    
    ModuleOwnershipConfig(const std::string& name, OwnershipMode m = OwnershipMode::Disabled)
        : module_name(name), mode(m), allow_mixed_paradigm(true) {}
};

// Global ownership configuration
class OwnershipConfiguration {
public:
    OwnershipConfiguration();
    
    // Set global default mode
    void set_default_mode(OwnershipMode mode);
    OwnershipMode get_default_mode() const;
    
    // Per-module configuration
    void set_module_mode(const std::string& module, OwnershipMode mode);
    OwnershipMode get_module_mode(const std::string& module) const;
    
    // Exclusion management
    void exclude_function(const std::string& module, const std::string& function);
    void exclude_type(const std::string& module, const std::string& type);
    bool is_function_excluded(const std::string& module, const std::string& function) const;
    bool is_type_excluded(const std::string& module, const std::string& type) const;
    
    // Mixed paradigm support
    void set_mixed_paradigm(const std::string& module, bool allow);
    bool allows_mixed_paradigm(const std::string& module) const;
    
    // Configuration loading
    bool load_from_file(const std::string& config_path);
    bool save_to_file(const std::string& config_path) const;
    
    // Get module config
    const ModuleOwnershipConfig* get_module_config(const std::string& module) const;
    
private:
    OwnershipMode default_mode_;
    std::unordered_map<std::string, ModuleOwnershipConfig> module_configs_;
};

// Compiler flag parser for ownership modes
class OwnershipFlagParser {
public:
    static std::optional<OwnershipMode> parse_mode(const std::string& flag);
    static std::string mode_to_string(OwnershipMode mode);
    
    // Parse command-line arguments
    static bool parse_flags(
        const std::vector<std::string>& args,
        OwnershipConfiguration& config
    );
    
    // Generate help text
    static std::string get_help_text();
};

// Migration assistance for gradual adoption
class MigrationAssistant {
public:
    struct MigrationSuggestion {
        std::string location;
        std::string issue;
        std::string suggestion;
        std::string example_fix;
        int confidence;  // 0-100
    };
    
    MigrationAssistant(const OwnershipConfiguration& config);
    
    // Analyze code for migration opportunities
    std::vector<MigrationSuggestion> analyze_for_migration(
        const std::string& module_name,
        const std::string& source_code
    );
    
    // Generate migration report
    std::string generate_migration_report(
        const std::vector<MigrationSuggestion>& suggestions
    ) const;
    
    // Suggest ownership annotations
    std::vector<std::string> suggest_ownership_annotations(
        const std::string& function_signature
    ) const;
    
    // Check if code is ready for strict mode
    bool is_ready_for_strict_mode(
        const std::string& module_name,
        const std::vector<MigrationSuggestion>& suggestions
    ) const;
    
private:
    const OwnershipConfiguration& config_;
    
    // Analysis helpers
    bool needs_ownership_annotation(const std::string& code) const;
    bool has_potential_use_after_move(const std::string& code) const;
    bool has_aliasing_issues(const std::string& code) const;
};

// Progressive ownership checker
class ProgressiveOwnershipChecker {
public:
    ProgressiveOwnershipChecker(const OwnershipConfiguration& config);
    
    // Check code according to configured mode
    struct CheckResult {
        bool passed;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
        std::vector<MigrationAssistant::MigrationSuggestion> suggestions;
    };
    
    CheckResult check_module(
        const std::string& module_name,
        const std::string& source_code
    );
    
    CheckResult check_function(
        const std::string& module_name,
        const std::string& function_name,
        const std::string& function_code
    );
    
    // Mode-specific checking
    CheckResult check_disabled_mode(const std::string& code);
    CheckResult check_warn_mode(const std::string& code);
    CheckResult check_strict_mode(const std::string& code);
    CheckResult check_gradual_mode(const std::string& code);
    
private:
    const OwnershipConfiguration& config_;
    MigrationAssistant migration_assistant_;
    
    // Helper methods
    bool should_check_function(
        const std::string& module,
        const std::string& function
    ) const;
    
    bool should_check_type(
        const std::string& module,
        const std::string& type
    ) const;
};

// Configuration file format helper
class OwnershipConfigFormat {
public:
    // Parse YAML-style configuration
    static std::optional<OwnershipConfiguration> parse_yaml(
        const std::string& yaml_content
    );
    
    // Generate YAML configuration
    static std::string to_yaml(const OwnershipConfiguration& config);
    
    // Example configuration
    static std::string generate_example_config();
};

} // namespace adoption
} // namespace meld
