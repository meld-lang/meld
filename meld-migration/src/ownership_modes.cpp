#include "meld/adoption/ownership_modes.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>

namespace meld {
namespace adoption {

// OwnershipConfiguration implementation

OwnershipConfiguration::OwnershipConfiguration()
    : default_mode_(OwnershipMode::Disabled) {}

void OwnershipConfiguration::set_default_mode(OwnershipMode mode) {
    default_mode_ = mode;
}

OwnershipMode OwnershipConfiguration::get_default_mode() const {
    return default_mode_;
}

void OwnershipConfiguration::set_module_mode(const std::string& module, OwnershipMode mode) {
    auto it = module_configs_.find(module);
    if (it != module_configs_.end()) {
        it->second.mode = mode;
    } else {
        module_configs_.emplace(module, ModuleOwnershipConfig(module, mode));
    }
}

OwnershipMode OwnershipConfiguration::get_module_mode(const std::string& module) const {
    auto it = module_configs_.find(module);
    return (it != module_configs_.end()) ? it->second.mode : default_mode_;
}

void OwnershipConfiguration::exclude_function(
    const std::string& module,
    const std::string& function
) {
    auto it = module_configs_.find(module);
    if (it != module_configs_.end()) {
        it->second.excluded_functions.push_back(function);
    } else {
        ModuleOwnershipConfig config(module, default_mode_);
        config.excluded_functions.push_back(function);
        module_configs_.emplace(module, std::move(config));
    }
}

void OwnershipConfiguration::exclude_type(
    const std::string& module,
    const std::string& type
) {
    auto it = module_configs_.find(module);
    if (it != module_configs_.end()) {
        it->second.excluded_types.push_back(type);
    } else {
        ModuleOwnershipConfig config(module, default_mode_);
        config.excluded_types.push_back(type);
        module_configs_.emplace(module, std::move(config));
    }
}

bool OwnershipConfiguration::is_function_excluded(
    const std::string& module,
    const std::string& function
) const {
    auto it = module_configs_.find(module);
    if (it == module_configs_.end()) return false;
    
    const auto& excluded = it->second.excluded_functions;
    return std::find(excluded.begin(), excluded.end(), function) != excluded.end();
}

bool OwnershipConfiguration::is_type_excluded(
    const std::string& module,
    const std::string& type
) const {
    auto it = module_configs_.find(module);
    if (it == module_configs_.end()) return false;
    
    const auto& excluded = it->second.excluded_types;
    return std::find(excluded.begin(), excluded.end(), type) != excluded.end();
}

void OwnershipConfiguration::set_mixed_paradigm(const std::string& module, bool allow) {
    auto it = module_configs_.find(module);
    if (it != module_configs_.end()) {
        it->second.allow_mixed_paradigm = allow;
    } else {
        ModuleOwnershipConfig config(module, default_mode_);
        config.allow_mixed_paradigm = allow;
        module_configs_.emplace(module, std::move(config));
    }
}

bool OwnershipConfiguration::allows_mixed_paradigm(const std::string& module) const {
    auto it = module_configs_.find(module);
    return (it != module_configs_.end()) ? it->second.allow_mixed_paradigm : true;
}

const ModuleOwnershipConfig* OwnershipConfiguration::get_module_config(
    const std::string& module
) const {
    auto it = module_configs_.find(module);
    return (it != module_configs_.end()) ? &it->second : nullptr;
}

bool OwnershipConfiguration::load_from_file(const std::string& config_path) {
    std::ifstream file(config_path);
    if (!file.is_open()) return false;
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    
    auto config = OwnershipConfigFormat::parse_yaml(buffer.str());
    if (!config) return false;
    
    *this = *config;
    return true;
}

bool OwnershipConfiguration::save_to_file(const std::string& config_path) const {
    std::ofstream file(config_path);
    if (!file.is_open()) return false;
    
    file << OwnershipConfigFormat::to_yaml(*this);
    return true;
}

// OwnershipFlagParser implementation

std::optional<OwnershipMode> OwnershipFlagParser::parse_mode(const std::string& flag) {
    if (flag == "disabled" || flag == "off") return OwnershipMode::Disabled;
    if (flag == "warn" || flag == "warning") return OwnershipMode::Warn;
    if (flag == "strict" || flag == "error") return OwnershipMode::Strict;
    if (flag == "gradual" || flag == "progressive") return OwnershipMode::Gradual;
    return std::nullopt;
}

std::string OwnershipFlagParser::mode_to_string(OwnershipMode mode) {
    switch (mode) {
        case OwnershipMode::Disabled: return "disabled";
        case OwnershipMode::Warn: return "warn";
        case OwnershipMode::Strict: return "strict";
        case OwnershipMode::Gradual: return "gradual";
        default: return "unknown";
    }
}

bool OwnershipFlagParser::parse_flags(
    const std::vector<std::string>& args,
    OwnershipConfiguration& config
) {
    for (size_t i = 0; i < args.size(); ++i) {
        const auto& arg = args[i];
        
        if (arg == "--ownership-mode" && i + 1 < args.size()) {
            auto mode = parse_mode(args[++i]);
            if (mode) {
                config.set_default_mode(*mode);
            } else {
                return false;
            }
        }
        else if (arg == "--ownership-module" && i + 2 < args.size()) {
            std::string module = args[++i];
            auto mode = parse_mode(args[++i]);
            if (mode) {
                config.set_module_mode(module, *mode);
            } else {
                return false;
            }
        }
        else if (arg == "--ownership-config" && i + 1 < args.size()) {
            if (!config.load_from_file(args[++i])) {
                return false;
            }
        }
    }
    
    return true;
}

std::string OwnershipFlagParser::get_help_text() {
    return R"(
Ownership Checking Options:
  --ownership-mode <mode>           Set global ownership checking mode
                                    Modes: disabled, warn, strict, gradual
  
  --ownership-module <name> <mode>  Set per-module ownership mode
  
  --ownership-config <path>         Load ownership configuration from file

Examples:
  meld compile --ownership-mode warn myfile.meld
  meld compile --ownership-module core strict --ownership-module legacy disabled
  meld compile --ownership-config .meld-ownership.yaml
)";
}

// MigrationAssistant implementation

MigrationAssistant::MigrationAssistant(const OwnershipConfiguration& config)
    : config_(config) {}

std::vector<MigrationAssistant::MigrationSuggestion> 
MigrationAssistant::analyze_for_migration(
    const std::string& module_name,
    const std::string& source_code
) {
    std::vector<MigrationSuggestion> suggestions;
    
    // Check for potential use-after-move
    if (has_potential_use_after_move(source_code)) {
        suggestions.push_back({
            "function body",
            "Potential use-after-move detected",
            "Consider adding ownership annotations or using borrow() instead of move()",
            "val borrowed_value = borrow(value) // instead of moving",
            75
        });
    }
    
    // Check for aliasing issues
    if (has_aliasing_issues(source_code)) {
        suggestions.push_back({
            "variable usage",
            "Potential aliasing violation detected",
            "Ensure only one mutable reference exists at a time",
            "Use borrow_mut() for exclusive mutable access",
            80
        });
    }
    
    // Check for missing ownership annotations
    if (needs_ownership_annotation(source_code)) {
        suggestions.push_back({
            "function signature",
            "Function could benefit from ownership annotations",
            "Add Owned<T> or Borrowed<T> to clarify ownership semantics",
            "func process(data: Owned<string>) -> Result<(), Error>",
            60
        });
    }
    
    return suggestions;
}

std::string MigrationAssistant::generate_migration_report(
    const std::vector<MigrationSuggestion>& suggestions
) const {
    std::ostringstream report;
    report << "=== Ownership Migration Report ===\n\n";
    report << "Total suggestions: " << suggestions.size() << "\n\n";
    
    for (size_t i = 0; i < suggestions.size(); ++i) {
        const auto& s = suggestions[i];
        report << (i + 1) << ". " << s.issue << " (confidence: " << s.confidence << "%)\n";
        report << "   Location: " << s.location << "\n";
        report << "   Suggestion: " << s.suggestion << "\n";
        report << "   Example: " << s.example_fix << "\n\n";
    }
    
    return report.str();
}

std::vector<std::string> MigrationAssistant::suggest_ownership_annotations(
    const std::string& function_signature
) const {
    std::vector<std::string> suggestions;
    
    // Simple heuristics for ownership suggestions
    if (function_signature.find("string") != std::string::npos) {
        suggestions.push_back("Consider using Owned<string> for owned strings");
        suggestions.push_back("Consider using Borrowed<string> for borrowed strings");
    }
    
    if (function_signature.find("->") != std::string::npos) {
        suggestions.push_back("Return type could use Owned<T> to transfer ownership");
    }
    
    return suggestions;
}

bool MigrationAssistant::is_ready_for_strict_mode(
    const std::string& module_name,
    const std::vector<MigrationSuggestion>& suggestions
) const {
    // Module is ready if all high-confidence issues are resolved
    for (const auto& s : suggestions) {
        if (s.confidence >= 70) {
            return false;
        }
    }
    return true;
}

bool MigrationAssistant::needs_ownership_annotation(const std::string& code) const {
    // Simple heuristic: check for function parameters without ownership types
    std::regex func_pattern(R"(func\s+\w+\s*\([^)]*\w+:\s*(?!Owned|Borrowed)\w+)");
    return std::regex_search(code, func_pattern);
}

bool MigrationAssistant::has_potential_use_after_move(const std::string& code) const {
    // Simple heuristic: check for move() followed by usage
    std::regex move_pattern(R"(move\(\w+\))");
    return std::regex_search(code, move_pattern);
}

bool MigrationAssistant::has_aliasing_issues(const std::string& code) const {
    // Simple heuristic: check for multiple mutable references
    std::regex mut_ref_pattern(R"(borrow_mut\(\w+\))");
    std::smatch matches;
    std::string::const_iterator search_start(code.cbegin());
    int count = 0;
    
    while (std::regex_search(search_start, code.cend(), matches, mut_ref_pattern)) {
        count++;
        search_start = matches.suffix().first;
    }
    
    return count > 1;
}

// ProgressiveOwnershipChecker implementation

ProgressiveOwnershipChecker::ProgressiveOwnershipChecker(
    const OwnershipConfiguration& config
)
    : config_(config), migration_assistant_(config) {}

ProgressiveOwnershipChecker::CheckResult 
ProgressiveOwnershipChecker::check_module(
    const std::string& module_name,
    const std::string& source_code
) {
    OwnershipMode mode = config_.get_module_mode(module_name);
    
    switch (mode) {
        case OwnershipMode::Disabled:
            return check_disabled_mode(source_code);
        case OwnershipMode::Warn:
            return check_warn_mode(source_code);
        case OwnershipMode::Strict:
            return check_strict_mode(source_code);
        case OwnershipMode::Gradual:
            return check_gradual_mode(source_code);
        default:
            return {true, {}, {}, {}};
    }
}

ProgressiveOwnershipChecker::CheckResult 
ProgressiveOwnershipChecker::check_function(
    const std::string& module_name,
    const std::string& function_name,
    const std::string& function_code
) {
    if (!should_check_function(module_name, function_name)) {
        return {true, {}, {}, {}};
    }
    
    return check_module(module_name, function_code);
}

ProgressiveOwnershipChecker::CheckResult 
ProgressiveOwnershipChecker::check_disabled_mode(const std::string& code) {
    // No checking in disabled mode
    return {true, {}, {}, {}};
}

ProgressiveOwnershipChecker::CheckResult 
ProgressiveOwnershipChecker::check_warn_mode(const std::string& code) {
    CheckResult result;
    result.passed = true;
    
    // Perform analysis but only generate warnings
    auto suggestions = migration_assistant_.analyze_for_migration("", code);
    
    for (const auto& s : suggestions) {
        result.warnings.push_back(s.issue + ": " + s.suggestion);
    }
    
    return result;
}

ProgressiveOwnershipChecker::CheckResult 
ProgressiveOwnershipChecker::check_strict_mode(const std::string& code) {
    CheckResult result;
    
    // Perform full ownership checking
    auto suggestions = migration_assistant_.analyze_for_migration("", code);
    
    for (const auto& s : suggestions) {
        if (s.confidence >= 70) {
            result.errors.push_back(s.issue + ": " + s.suggestion);
            result.passed = false;
        } else {
            result.warnings.push_back(s.issue + ": " + s.suggestion);
        }
    }
    
    return result;
}

ProgressiveOwnershipChecker::CheckResult 
ProgressiveOwnershipChecker::check_gradual_mode(const std::string& code) {
    CheckResult result;
    result.passed = true;
    
    // Perform analysis with migration suggestions
    auto suggestions = migration_assistant_.analyze_for_migration("", code);
    result.suggestions = suggestions;
    
    // Only error on high-confidence issues
    for (const auto& s : suggestions) {
        if (s.confidence >= 85) {
            result.errors.push_back(s.issue + ": " + s.suggestion);
            result.passed = false;
        } else if (s.confidence >= 60) {
            result.warnings.push_back(s.issue + ": " + s.suggestion);
        }
    }
    
    return result;
}

bool ProgressiveOwnershipChecker::should_check_function(
    const std::string& module,
    const std::string& function
) const {
    return !config_.is_function_excluded(module, function);
}

bool ProgressiveOwnershipChecker::should_check_type(
    const std::string& module,
    const std::string& type
) const {
    return !config_.is_type_excluded(module, type);
}

// OwnershipConfigFormat implementation

std::optional<OwnershipConfiguration> OwnershipConfigFormat::parse_yaml(
    const std::string& yaml_content
) {
    // Simplified YAML parsing (in production, use a proper YAML library)
    OwnershipConfiguration config;
    
    std::istringstream stream(yaml_content);
    std::string line;
    std::string current_module;
    
    while (std::getline(stream, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') continue;
        
        // Parse default mode
        if (line.find("default_mode:") != std::string::npos) {
            auto pos = line.find(':');
            if (pos != std::string::npos) {
                std::string mode_str = line.substr(pos + 1);
                mode_str.erase(0, mode_str.find_first_not_of(" \t"));
                auto mode = OwnershipFlagParser::parse_mode(mode_str);
                if (mode) config.set_default_mode(*mode);
            }
        }
        
        // Parse module configurations
        if (line.find("module:") != std::string::npos) {
            auto pos = line.find(':');
            if (pos != std::string::npos) {
                current_module = line.substr(pos + 1);
                current_module.erase(0, current_module.find_first_not_of(" \t"));
            }
        }
        
        if (!current_module.empty() && line.find("mode:") != std::string::npos) {
            auto pos = line.find(':');
            if (pos != std::string::npos) {
                std::string mode_str = line.substr(pos + 1);
                mode_str.erase(0, mode_str.find_first_not_of(" \t"));
                auto mode = OwnershipFlagParser::parse_mode(mode_str);
                if (mode) config.set_module_mode(current_module, *mode);
            }
        }
    }
    
    return config;
}

std::string OwnershipConfigFormat::to_yaml(const OwnershipConfiguration& config) {
    std::ostringstream yaml;
    
    yaml << "# Meld Ownership Configuration\n";
    yaml << "default_mode: " << OwnershipFlagParser::mode_to_string(config.get_default_mode()) << "\n\n";
    yaml << "modules:\n";
    
    // Note: In production, iterate through all module configs
    yaml << "  # Add module-specific configurations here\n";
    
    return yaml.str();
}

std::string OwnershipConfigFormat::generate_example_config() {
    return R"(# Meld Ownership Configuration Example

# Global default mode for all modules
default_mode: disabled

# Per-module configuration
modules:
  - module: core
    mode: strict
    allow_mixed_paradigm: false
    
  - module: utils
    mode: gradual
    allow_mixed_paradigm: true
    excluded_functions:
      - legacy_function
      - old_api
    
  - module: legacy
    mode: disabled
    allow_mixed_paradigm: true

# Migration settings
migration:
  generate_reports: true
  suggest_annotations: true
  confidence_threshold: 70
)";
}

} // namespace adoption
} // namespace meld
