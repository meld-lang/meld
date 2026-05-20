#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <map>

namespace meld {
namespace diagnostics {

// Error severity levels
enum class Severity {
    Error,
    Warning,
    Info,
    Hint
};

// Source location information
struct SourceLocation {
    std::string file_path;
    size_t line;
    size_t column;
    size_t length;
    
    SourceLocation(const std::string& path, size_t l, size_t c, size_t len = 1)
        : file_path(path), line(l), column(c), length(len) {}
};

// Code suggestion for fixing errors
struct CodeSuggestion {
    std::string description;
    SourceLocation location;
    std::string replacement_text;
    std::string explanation;
    
    CodeSuggestion(const std::string& desc, const SourceLocation& loc,
                   const std::string& replacement, const std::string& explain = "")
        : description(desc), location(loc), replacement_text(replacement), explanation(explain) {}
};

// Diagnostic message with rich information
class Diagnostic {
public:
    Diagnostic(Severity sev, const std::string& msg, const SourceLocation& loc)
        : severity_(sev), message_(msg), primary_location_(loc) {}
    
    // Add additional context locations
    void add_note(const std::string& note, const SourceLocation& loc) {
        notes_.push_back({note, loc});
    }
    
    // Add code suggestions
    void add_suggestion(const CodeSuggestion& suggestion) {
        suggestions_.push_back(suggestion);
    }
    
    // Add related information
    void add_related_info(const std::string& info) {
        related_info_.push_back(info);
    }
    
    // Getters
    Severity severity() const { return severity_; }
    const std::string& message() const { return message_; }
    const SourceLocation& location() const { return primary_location_; }
    const std::vector<std::pair<std::string, SourceLocation>>& notes() const { return notes_; }
    const std::vector<CodeSuggestion>& suggestions() const { return suggestions_; }
    const std::vector<std::string>& related_info() const { return related_info_; }
    
    // Format diagnostic for display
    std::string format() const;
    
private:
    Severity severity_;
    std::string message_;
    SourceLocation primary_location_;
    std::vector<std::pair<std::string, SourceLocation>> notes_;
    std::vector<CodeSuggestion> suggestions_;
    std::vector<std::string> related_info_;
};

// Ownership-specific error messages
class OwnershipErrorBuilder {
public:
    static Diagnostic use_after_move(
        const SourceLocation& use_loc,
        const SourceLocation& move_loc,
        const std::string& variable_name
    );
    
    static Diagnostic multiple_mutable_borrows(
        const SourceLocation& second_borrow_loc,
        const SourceLocation& first_borrow_loc,
        const std::string& variable_name
    );
    
    static Diagnostic borrow_while_mutably_borrowed(
        const SourceLocation& immutable_borrow_loc,
        const SourceLocation& mutable_borrow_loc,
        const std::string& variable_name
    );
    
    static Diagnostic moved_value_not_copyable(
        const SourceLocation& move_loc,
        const std::string& variable_name,
        const std::string& type_name
    );
    
    static Diagnostic lifetime_mismatch(
        const SourceLocation& error_loc,
        const std::string& expected_lifetime,
        const std::string& actual_lifetime
    );
};

// Type system error messages
class TypeErrorBuilder {
public:
    static Diagnostic type_mismatch(
        const SourceLocation& loc,
        const std::string& expected_type,
        const std::string& actual_type
    );
    
    static Diagnostic missing_trait_implementation(
        const SourceLocation& loc,
        const std::string& type_name,
        const std::string& trait_name
    );
    
    static Diagnostic non_exhaustive_pattern(
        const SourceLocation& match_loc,
        const std::vector<std::string>& missing_patterns
    );
    
    static Diagnostic trait_coherence_violation(
        const SourceLocation& new_impl_loc,
        const SourceLocation& existing_impl_loc,
        const std::string& trait_name,
        const std::string& type_name
    );
};

// Concurrency error messages
class ConcurrencyErrorBuilder {
public:
    static Diagnostic type_not_send(
        const SourceLocation& loc,
        const std::string& type_name,
        const std::string& reason
    );
    
    static Diagnostic type_not_sync(
        const SourceLocation& loc,
        const std::string& type_name,
        const std::string& reason
    );
    
    static Diagnostic data_race_detected(
        const SourceLocation& loc,
        const std::string& variable_name,
        const std::vector<SourceLocation>& access_locations
    );
};

// Error message template system
class ErrorTemplate {
public:
    ErrorTemplate(const std::string& template_text) : template_(template_text) {}
    
    // Replace placeholders in template
    std::string format(const std::map<std::string, std::string>& replacements) const;
    
private:
    std::string template_;
};

// Suggestion engine for common fixes
class SuggestionEngine {
public:
    // Suggest fixes for ownership errors
    static std::vector<CodeSuggestion> suggest_ownership_fixes(
        const std::string& error_type,
        const SourceLocation& error_loc,
        const std::string& variable_name
    );
    
    // Suggest trait implementations
    static std::vector<CodeSuggestion> suggest_trait_implementations(
        const std::string& type_name,
        const std::string& trait_name,
        const SourceLocation& loc
    );
    
    // Suggest pattern completions
    static std::vector<CodeSuggestion> suggest_pattern_completions(
        const std::vector<std::string>& missing_patterns,
        const SourceLocation& match_loc
    );
    
    // Suggest lifetime annotations
    static std::vector<CodeSuggestion> suggest_lifetime_annotations(
        const SourceLocation& loc,
        const std::string& function_signature
    );
};

// Diagnostic collector
class DiagnosticCollector {
public:
    void add_diagnostic(const Diagnostic& diag) {
        diagnostics_.push_back(diag);
    }
    
    bool has_errors() const {
        for (const auto& diag : diagnostics_) {
            if (diag.severity() == Severity::Error) {
                return true;
            }
        }
        return false;
    }
    
    const std::vector<Diagnostic>& diagnostics() const { return diagnostics_; }
    
    // Format all diagnostics
    std::string format_all() const;
    
    void clear() { diagnostics_.clear(); }
    
private:
    std::vector<Diagnostic> diagnostics_;
};

} // namespace diagnostics
} // namespace meld
