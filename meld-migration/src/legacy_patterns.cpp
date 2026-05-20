#include "meld/adoption/legacy_patterns.hpp"
#include <regex>
#include <sstream>
#include <algorithm>

namespace meld {
namespace adoption {

// LegacyCompatibilityLayer implementation

LegacyCompatibilityLayer::LegacyCompatibilityLayer() {
    initialize_pattern_mappings();
}

void LegacyCompatibilityLayer::initialize_pattern_mappings() {
    pattern_descriptions_[LegacyPattern::ManualMemoryManagement] = 
        "Manual memory management with malloc/free";
    pattern_descriptions_[LegacyPattern::NullableReferences] = 
        "Using null instead of Option<T>";
    pattern_descriptions_[LegacyPattern::ExceptionBased] = 
        "Exception-based error handling";
    pattern_descriptions_[LegacyPattern::ImplicitCopying] = 
        "Implicit copying without ownership tracking";
    pattern_descriptions_[LegacyPattern::GlobalMutableState] = 
        "Global mutable state";
    pattern_descriptions_[LegacyPattern::UncheckedCasts] = 
        "Unchecked type casts";
    pattern_descriptions_[LegacyPattern::RawPointers] = 
        "Raw pointer manipulation";
    
    modern_alternatives_[LegacyPattern::ManualMemoryManagement] = 
        "Use Owned<T> for automatic memory management";
    modern_alternatives_[LegacyPattern::NullableReferences] = 
        "Use Option<T> for optional values";
    modern_alternatives_[LegacyPattern::ExceptionBased] = 
        "Use Result<T, E> for error handling";
    modern_alternatives_[LegacyPattern::ImplicitCopying] = 
        "Use explicit move() or borrow() operations";
    modern_alternatives_[LegacyPattern::GlobalMutableState] = 
        "Use dependency injection or state management patterns";
    modern_alternatives_[LegacyPattern::UncheckedCasts] = 
        "Use safe type conversions with pattern matching";
    modern_alternatives_[LegacyPattern::RawPointers] = 
        "Use Owned<T> or Borrowed<T> wrappers";
}

std::vector<LegacyPatternDetection> LegacyCompatibilityLayer::detect_legacy_patterns(
    const std::string& source_code
) const {
    std::vector<LegacyPatternDetection> detections;
    
    // Detect nullable references
    std::regex null_pattern(R"(\w+\s*=\s*null|\w+\s*:\s*\w+\?)");
    if (std::regex_search(source_code, null_pattern)) {
        detections.push_back({
            LegacyPattern::NullableReferences,
            "variable declaration",
            "Using nullable references instead of Option<T>",
            modern_alternatives_.at(LegacyPattern::NullableReferences),
            7
        });
    }
    
    // Detect exception-based error handling
    std::regex exception_pattern(R"(throw\s+|catch\s*\()");
    if (std::regex_search(source_code, exception_pattern)) {
        detections.push_back({
            LegacyPattern::ExceptionBased,
            "error handling",
            "Using exceptions instead of Result<T, E>",
            modern_alternatives_.at(LegacyPattern::ExceptionBased),
            6
        });
    }
    
    // Detect implicit copying
    std::regex copy_pattern(R"(val\s+\w+\s*=\s*\w+\s*$)");
    if (std::regex_search(source_code, copy_pattern)) {
        detections.push_back({
            LegacyPattern::ImplicitCopying,
            "variable assignment",
            "Implicit copying without ownership tracking",
            modern_alternatives_.at(LegacyPattern::ImplicitCopying),
            5
        });
    }
    
    // Detect global mutable state
    std::regex global_pattern(R"(global\s+var\s+|static\s+var\s+)");
    if (std::regex_search(source_code, global_pattern)) {
        detections.push_back({
            LegacyPattern::GlobalMutableState,
            "global variable",
            "Using global mutable state",
            modern_alternatives_.at(LegacyPattern::GlobalMutableState),
            8
        });
    }
    
    return detections;
}

std::optional<std::string> LegacyCompatibilityLayer::convert_to_modern(
    LegacyPattern pattern,
    const std::string& code_snippet
) const {
    switch (pattern) {
        case LegacyPattern::NullableReferences: {
            // Convert "val x: String? = null" to "val x: Option<String> = Option.none()"
            std::regex nullable_regex(R"((\w+)\s*:\s*(\w+)\?\s*=\s*null)");
            return std::regex_replace(code_snippet, nullable_regex, 
                "$1: Option<$2> = Option.none()");
        }
        
        case LegacyPattern::ExceptionBased: {
            // Convert "throw Error(msg)" to "return Result.err(Error(msg))"
            std::regex throw_regex(R"(throw\s+(\w+\([^)]*\)))");
            return std::regex_replace(code_snippet, throw_regex, 
                "return Result.err($1)");
        }
        
        case LegacyPattern::ImplicitCopying: {
            // Convert "val y = x" to "val y = borrow(x)"
            std::regex copy_regex(R"(val\s+(\w+)\s*=\s*(\w+))");
            return std::regex_replace(code_snippet, copy_regex, 
                "val $1 = borrow($2)");
        }
        
        default:
            return std::nullopt;
    }
}

std::string LegacyCompatibilityLayer::generate_compatibility_wrapper(
    const std::string& legacy_function,
    const std::string& modern_signature
) const {
    std::ostringstream wrapper;
    
    wrapper << "// Compatibility wrapper for legacy function\n";
    wrapper << modern_signature << " {\n";
    wrapper << "    // Convert modern types to legacy format\n";
    wrapper << "    val legacy_result = " << legacy_function << "\n";
    wrapper << "    // Convert legacy result to modern format\n";
    wrapper << "    return Result.ok(legacy_result)\n";
    wrapper << "}\n";
    
    return wrapper.str();
}

bool LegacyCompatibilityLayer::uses_legacy_patterns(const std::string& code) const {
    return !detect_legacy_patterns(code).empty();
}

std::string LegacyCompatibilityLayer::get_migration_path(LegacyPattern pattern) const {
    std::ostringstream path;
    
    path << "Migration path for " << pattern_descriptions_.at(pattern) << ":\n";
    path << "1. Identify all occurrences of the pattern\n";
    path << "2. " << modern_alternatives_.at(pattern) << "\n";
    path << "3. Update tests to verify new behavior\n";
    path << "4. Remove legacy code once migration is complete\n";
    
    return path.str();
}

// MigrationWarningSystem implementation

MigrationWarningSystem::MigrationWarningSystem() {}

std::vector<MigrationWarningSystem::Warning> MigrationWarningSystem::generate_warnings(
    const std::string& module_name,
    const std::string& source_code
) const {
    std::vector<Warning> warnings;
    LegacyCompatibilityLayer compat_layer;
    
    auto detections = compat_layer.detect_legacy_patterns(source_code);
    
    for (const auto& detection : detections) {
        if (!should_suppress_warning(module_name, 
            Warning{detection.location, "", "", "", 0, detection.pattern})) {
            warnings.push_back(create_warning_for_pattern(detection));
        }
    }
    
    return warnings;
}

std::string MigrationWarningSystem::format_warning(const Warning& warning) const {
    std::ostringstream formatted;
    
    formatted << "[Priority " << warning.priority << "] ";
    formatted << warning.location << ": " << warning.message << "\n";
    formatted << "  Suggestion: " << warning.suggestion << "\n";
    
    if (!warning.code_example.empty()) {
        formatted << "  Example:\n";
        formatted << "    " << warning.code_example << "\n";
    }
    
    return formatted.str();
}

std::string MigrationWarningSystem::generate_migration_guide(
    const std::vector<Warning>& warnings
) const {
    std::ostringstream guide;
    
    guide << "=== Migration Guide ===\n\n";
    guide << "Total warnings: " << warnings.size() << "\n\n";
    
    // Group by priority
    std::vector<Warning> sorted_warnings = warnings;
    std::sort(sorted_warnings.begin(), sorted_warnings.end(),
        [](const Warning& a, const Warning& b) { return a.priority > b.priority; });
    
    int current_priority = -1;
    for (const auto& warning : sorted_warnings) {
        if (warning.priority != current_priority) {
            current_priority = warning.priority;
            guide << "\n--- Priority " << current_priority << " ---\n\n";
        }
        guide << format_warning(warning);
    }
    
    return guide.str();
}

bool MigrationWarningSystem::should_suppress_warning(
    const std::string& module,
    const Warning& warning
) const {
    auto it = suppressed_warnings_.find(module);
    if (it == suppressed_warnings_.end()) return false;
    
    const auto& patterns = it->second;
    return std::find(patterns.begin(), patterns.end(), warning.related_pattern) 
        != patterns.end();
}

void MigrationWarningSystem::suppress_warning(
    const std::string& module,
    LegacyPattern pattern
) {
    suppressed_warnings_[module].push_back(pattern);
}

MigrationWarningSystem::Warning MigrationWarningSystem::create_warning_for_pattern(
    const LegacyPatternDetection& detection
) const {
    Warning warning;
    warning.location = detection.location;
    warning.message = detection.description;
    warning.suggestion = detection.modern_alternative;
    warning.priority = (detection.severity + 1) / 2;  // Convert 1-10 to 1-5
    warning.related_pattern = detection.pattern;
    
    // Add code example based on pattern
    switch (detection.pattern) {
        case LegacyPattern::NullableReferences:
            warning.code_example = "val name: Option<String> = Option.some(\"value\")";
            break;
        case LegacyPattern::ExceptionBased:
            warning.code_example = "return Result.err(Error(\"message\"))";
            break;
        case LegacyPattern::ImplicitCopying:
            warning.code_example = "val borrowed = borrow(original)";
            break;
        default:
            warning.code_example = "";
    }
    
    return warning;
}

// MixedParadigmSupport implementation

MixedParadigmSupport::MixedParadigmSupport() {
    initialize_default_conversions();
}

void MixedParadigmSupport::initialize_default_conversions() {
    // String conversions
    conversion_rules_.push_back({
        "Owned<string>", "string", "owned.value()", true
    });
    conversion_rules_.push_back({
        "string", "Owned<string>", "Owned.new(gc_string)", true
    });
    
    // Integer conversions
    conversion_rules_.push_back({
        "Owned<int>", "int", "owned.value()", true
    });
    conversion_rules_.push_back({
        "int", "Owned<int>", "Owned.new(gc_int)", true
    });
    
    // List conversions
    conversion_rules_.push_back({
        "Owned<list<T>>", "list<T>", "owned.value()", false
    });
    conversion_rules_.push_back({
        "list<T>", "Owned<list<T>>", "Owned.new(gc_list.clone())", false
    });
}

std::string MixedParadigmSupport::convert_owned_to_gc(
    const std::string& owned_expr,
    const std::string& type
) const {
    // Find conversion rule
    for (const auto& rule : conversion_rules_) {
        if (rule.from_type.find("Owned") != std::string::npos &&
            rule.to_type == type) {
            return rule.conversion_function;
        }
    }
    
    // Default conversion
    return owned_expr + ".value()";
}

std::string MixedParadigmSupport::convert_gc_to_owned(
    const std::string& gc_expr,
    const std::string& type
) const {
    // Find conversion rule
    for (const auto& rule : conversion_rules_) {
        if (rule.from_type == type &&
            rule.to_type.find("Owned") != std::string::npos) {
            return "Owned.new(" + gc_expr + ")";
        }
    }
    
    // Default conversion
    return "Owned.new(" + gc_expr + ")";
}

std::string MixedParadigmSupport::generate_bridge_function(
    const std::string& from_paradigm,
    const std::string& to_paradigm,
    const std::string& function_signature
) const {
    std::ostringstream bridge;
    
    bridge << "// Bridge function: " << from_paradigm << " -> " << to_paradigm << "\n";
    bridge << function_signature << " {\n";
    bridge << "    // Convert parameters\n";
    bridge << "    // Call target function\n";
    bridge << "    // Convert result\n";
    bridge << "}\n";
    
    return bridge.str();
}

bool MixedParadigmSupport::are_types_compatible(
    const std::string& owned_type,
    const std::string& gc_type
) const {
    // Check if conversion rule exists
    for (const auto& rule : conversion_rules_) {
        if ((rule.from_type == owned_type && rule.to_type == gc_type) ||
            (rule.from_type == gc_type && rule.to_type == owned_type)) {
            return true;
        }
    }
    return false;
}

std::string MixedParadigmSupport::generate_interop_code(
    const std::string& owned_code,
    const std::string& gc_code
) const {
    std::ostringstream interop;
    
    interop << "// Mixed paradigm interop\n";
    interop << "module MixedParadigm {\n";
    interop << "    // Owned code section\n";
    interop << "    " << owned_code << "\n\n";
    interop << "    // GC code section\n";
    interop << "    " << gc_code << "\n\n";
    interop << "    // Conversion helpers\n";
    interop << "    func to_owned<T>(gc_value: T) -> Owned<T> {\n";
    interop << "        Owned.new(gc_value)\n";
    interop << "    }\n";
    interop << "    func to_gc<T>(owned_value: Owned<T>) -> T {\n";
    interop << "        owned_value.value()\n";
    interop << "    }\n";
    interop << "}\n";
    
    return interop.str();
}

std::vector<MixedParadigmSupport::ConversionRule> 
MixedParadigmSupport::get_conversion_rules() const {
    return conversion_rules_;
}

void MixedParadigmSupport::add_conversion_rule(const ConversionRule& rule) {
    conversion_rules_.push_back(rule);
}

// LegacyAPIWrapper implementation

LegacyAPIWrapper::LegacyAPIWrapper() {}

std::string LegacyAPIWrapper::wrap_legacy_function(
    const std::string& legacy_signature,
    const std::string& legacy_body,
    bool add_error_handling
) const {
    std::ostringstream wrapper;
    
    wrapper << "// Modern wrapper for legacy function\n";
    wrapper << legacy_signature << " {\n";
    
    if (add_error_handling) {
        wrapper << "    try {\n";
        wrapper << "        " << legacy_body << "\n";
        wrapper << "        return Result.ok(result)\n";
        wrapper << "    } catch (error) {\n";
        wrapper << "        return Result.err(error)\n";
        wrapper << "    }\n";
    } else {
        wrapper << "    " << legacy_body << "\n";
    }
    
    wrapper << "}\n";
    
    return wrapper.str();
}

std::string LegacyAPIWrapper::generate_class_adapter(
    const std::string& legacy_class,
    const std::string& modern_interface
) const {
    std::ostringstream adapter;
    
    adapter << "// Adapter for legacy class\n";
    adapter << "class " << legacy_class << "Adapter implements " << modern_interface << " {\n";
    adapter << "    private legacy: " << legacy_class << "\n\n";
    adapter << "    func new(legacy_instance: " << legacy_class << ") {\n";
    adapter << "        self.legacy = legacy_instance\n";
    adapter << "    }\n\n";
    adapter << "    // Implement modern interface methods\n";
    adapter << "    // by delegating to legacy instance\n";
    adapter << "}\n";
    
    return adapter.str();
}

std::string LegacyAPIWrapper::create_module_facade(
    const std::string& legacy_module,
    const std::vector<std::string>& functions_to_wrap
) const {
    std::ostringstream facade;
    
    facade << "// Modern facade for legacy module\n";
    facade << "module " << legacy_module << "Facade {\n";
    facade << "    import " << legacy_module << " as Legacy\n\n";
    
    for (const auto& func : functions_to_wrap) {
        facade << "    func " << func << "_modern(...) -> Result<T, Error> {\n";
        facade << "        // Call legacy function and wrap result\n";
        facade << "        val result = Legacy." << func << "(...)\n";
        facade << "        return Result.ok(result)\n";
        facade << "    }\n\n";
    }
    
    facade << "}\n";
    
    return facade.str();
}

std::string LegacyAPIWrapper::add_result_wrapper(const std::string& function) const {
    return "Result<T, Error> wrapping " + function;
}

std::string LegacyAPIWrapper::add_ownership_annotations(const std::string& function) const {
    return "Owned<T> annotations for " + function;
}

std::string LegacyAPIWrapper::add_null_safety(const std::string& function) const {
    return "Option<T> for nullable returns in " + function;
}

// GradualMigrationPlanner implementation

GradualMigrationPlanner::GradualMigrationPlanner() {}

std::vector<GradualMigrationPlanner::MigrationStep> 
GradualMigrationPlanner::generate_migration_plan(
    const std::string& module_name,
    const std::vector<LegacyPatternDetection>& patterns
) const {
    std::vector<MigrationStep> steps;
    int step_num = 1;
    
    // Step 1: Enable warning mode
    steps.push_back({
        step_num++,
        "Enable ownership warning mode",
        "Set module to warn mode to identify issues",
        {module_name + ".meld"},
        "Compilation succeeds with warnings about legacy patterns",
        2
    });
    
    // Step 2: Fix high-severity issues
    for (const auto& pattern : patterns) {
        if (pattern.severity >= 7) {
            steps.push_back({
                step_num++,
                "Fix " + pattern.description,
                pattern.modern_alternative,
                {module_name + ".meld"},
                "High-severity issue resolved",
                4
            });
        }
    }
    
    // Step 3: Enable gradual mode
    steps.push_back({
        step_num++,
        "Enable gradual ownership mode",
        "Set module to gradual mode for progressive checking",
        {module_name + ".meld"},
        "Module uses gradual ownership checking",
        1
    });
    
    // Step 4: Fix medium-severity issues
    for (const auto& pattern : patterns) {
        if (pattern.severity >= 4 && pattern.severity < 7) {
            steps.push_back({
                step_num++,
                "Fix " + pattern.description,
                pattern.modern_alternative,
                {module_name + ".meld"},
                "Medium-severity issue resolved",
                3
            });
        }
    }
    
    // Step 5: Enable strict mode
    steps.push_back({
        step_num++,
        "Enable strict ownership mode",
        "Set module to strict mode for full checking",
        {module_name + ".meld"},
        "Module fully migrated to ownership system",
        2
    });
    
    return steps;
}

std::vector<GradualMigrationPlanner::MigrationStep> 
GradualMigrationPlanner::prioritize_steps(
    const std::vector<MigrationStep>& steps
) const {
    std::vector<MigrationStep> prioritized = steps;
    
    std::sort(prioritized.begin(), prioritized.end(),
        [this](const MigrationStep& a, const MigrationStep& b) {
            return calculate_step_priority(a) > calculate_step_priority(b);
        });
    
    return prioritized;
}

std::string GradualMigrationPlanner::generate_step_guide(
    const std::vector<MigrationStep>& steps
) const {
    std::ostringstream guide;
    
    guide << "=== Gradual Migration Guide ===\n\n";
    guide << "Total steps: " << steps.size() << "\n";
    guide << "Estimated effort: " << estimate_total_effort(steps) << " hours\n\n";
    
    for (const auto& step : steps) {
        guide << "Step " << step.step_number << ": " << step.description << "\n";
        guide << "  Action: " << step.action << "\n";
        guide << "  Files: ";
        for (size_t i = 0; i < step.files_to_modify.size(); ++i) {
            if (i > 0) guide << ", ";
            guide << step.files_to_modify[i];
        }
        guide << "\n";
        guide << "  Expected: " << step.expected_outcome << "\n";
        guide << "  Effort: " << step.estimated_effort << " hours\n\n";
    }
    
    return guide.str();
}

int GradualMigrationPlanner::estimate_total_effort(
    const std::vector<MigrationStep>& steps
) const {
    int total = 0;
    for (const auto& step : steps) {
        total += step.estimated_effort;
    }
    return total;
}

int GradualMigrationPlanner::calculate_step_priority(const MigrationStep& step) const {
    // Higher priority for steps that enable modes or fix critical issues
    if (step.description.find("Enable") != std::string::npos) {
        return 10;
    }
    if (step.description.find("high-severity") != std::string::npos) {
        return 8;
    }
    return 5;
}

// BackwardCompatibilityChecker implementation

BackwardCompatibilityChecker::BackwardCompatibilityChecker() {}

std::vector<BackwardCompatibilityChecker::CompatibilityIssue> 
BackwardCompatibilityChecker::check_compatibility(
    const std::string& old_code,
    const std::string& new_code
) const {
    std::vector<CompatibilityIssue> issues;
    
    // Check for signature changes
    std::regex func_pattern(R"(func\s+(\w+)\s*\([^)]*\))");
    std::smatch old_matches, new_matches;
    
    auto old_it = old_code.cbegin();
    auto new_it = new_code.cbegin();
    
    std::vector<std::string> old_funcs, new_funcs;
    
    while (std::regex_search(old_it, old_code.cend(), old_matches, func_pattern)) {
        old_funcs.push_back(old_matches[0]);
        old_it = old_matches.suffix().first;
    }
    
    while (std::regex_search(new_it, new_code.cend(), new_matches, func_pattern)) {
        new_funcs.push_back(new_matches[0]);
        new_it = new_matches.suffix().first;
    }
    
    // Check for removed functions
    for (const auto& old_func : old_funcs) {
        if (std::find(new_funcs.begin(), new_funcs.end(), old_func) == new_funcs.end()) {
            issues.push_back({
                "function signature",
                "removed_function",
                "Function removed or signature changed: " + old_func,
                "Breaking change - existing code may fail",
                true
            });
        }
    }
    
    return issues;
}

std::vector<std::string> BackwardCompatibilityChecker::suggest_compatible_changes(
    const std::vector<CompatibilityIssue>& issues
) const {
    std::vector<std::string> suggestions;
    
    for (const auto& issue : issues) {
        if (issue.is_breaking) {
            if (issue.issue_type == "removed_function") {
                suggestions.push_back(
                    "Keep old function as deprecated wrapper calling new implementation"
                );
            } else if (issue.issue_type == "signature_change") {
                suggestions.push_back(
                    "Provide overload with old signature for backward compatibility"
                );
            }
        }
    }
    
    return suggestions;
}

std::string BackwardCompatibilityChecker::generate_compatibility_report(
    const std::vector<CompatibilityIssue>& issues
) const {
    std::ostringstream report;
    
    report << "=== Backward Compatibility Report ===\n\n";
    report << "Total issues: " << issues.size() << "\n";
    
    int breaking = 0;
    for (const auto& issue : issues) {
        if (issue.is_breaking) breaking++;
    }
    
    report << "Breaking changes: " << breaking << "\n\n";
    
    for (const auto& issue : issues) {
        report << (issue.is_breaking ? "[BREAKING] " : "[WARNING] ");
        report << issue.description << "\n";
        report << "  Impact: " << issue.impact << "\n\n";
    }
    
    return report.str();
}

bool BackwardCompatibilityChecker::is_signature_compatible(
    const std::string& old_sig,
    const std::string& new_sig
) const {
    return old_sig == new_sig;
}

bool BackwardCompatibilityChecker::is_type_compatible(
    const std::string& old_type,
    const std::string& new_type
) const {
    // Simple check - in production would need full type system
    return old_type == new_type;
}

} // namespace adoption
} // namespace meld
