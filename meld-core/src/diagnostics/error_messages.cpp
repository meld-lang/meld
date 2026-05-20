#include "meld/diagnostics/error_messages.hpp"
#include <sstream>
#include <algorithm>

namespace meld {
namespace diagnostics {

// Format diagnostic for display
std::string Diagnostic::format() const {
    std::ostringstream oss;
    
    // Severity prefix
    switch (severity_) {
        case Severity::Error:   oss << "error"; break;
        case Severity::Warning: oss << "warning"; break;
        case Severity::Info:    oss << "info"; break;
        case Severity::Hint:    oss << "hint"; break;
    }
    
    // Location and message
    oss << ": " << message_ << "\n";
    oss << "  --> " << primary_location_.file_path << ":"
        << primary_location_.line << ":" << primary_location_.column << "\n";
    
    // Notes
    for (const auto& [note, loc] : notes_) {
        oss << "note: " << note << "\n";
        oss << "  --> " << loc.file_path << ":" << loc.line << ":" << loc.column << "\n";
    }
    
    // Suggestions
    if (!suggestions_.empty()) {
        oss << "\nSuggested fixes:\n";
        for (size_t i = 0; i < suggestions_.size(); ++i) {
            const auto& sug = suggestions_[i];
            oss << "  " << (i + 1) << ". " << sug.description << "\n";
            if (!sug.replacement_text.empty()) {
                oss << "     Replace with: " << sug.replacement_text << "\n";
            }
            if (!sug.explanation.empty()) {
                oss << "     " << sug.explanation << "\n";
            }
        }
    }
    
    // Related information
    for (const auto& info : related_info_) {
        oss << "info: " << info << "\n";
    }
    
    return oss.str();
}

// Ownership error builders
Diagnostic OwnershipErrorBuilder::use_after_move(
    const SourceLocation& use_loc,
    const SourceLocation& move_loc,
    const std::string& variable_name
) {
    Diagnostic diag(
        Severity::Error,
        "use of moved value: `" + variable_name + "`",
        use_loc
    );
    
    diag.add_note(
        "value moved here",
        move_loc
    );
    
    diag.add_related_info(
        "move occurs because `" + variable_name + "` has type that does not implement the `Copy` trait"
    );
    
    // Add suggestions
    CodeSuggestion clone_suggestion(
        "Clone the value before moving",
        move_loc,
        variable_name + ".clone()",
        "This creates a copy of the value, allowing the original to be used later"
    );
    diag.add_suggestion(clone_suggestion);
    
    CodeSuggestion borrow_suggestion(
        "Borrow the value instead of moving",
        move_loc,
        "&" + variable_name,
        "This passes a reference instead of transferring ownership"
    );
    diag.add_suggestion(borrow_suggestion);
    
    return diag;
}

Diagnostic OwnershipErrorBuilder::multiple_mutable_borrows(
    const SourceLocation& second_borrow_loc,
    const SourceLocation& first_borrow_loc,
    const std::string& variable_name
) {
    Diagnostic diag(
        Severity::Error,
        "cannot borrow `" + variable_name + "` as mutable more than once at a time",
        second_borrow_loc
    );
    
    diag.add_note(
        "first mutable borrow occurs here",
        first_borrow_loc
    );
    
    diag.add_related_info(
        "mutable borrows are exclusive to prevent data races"
    );
    
    CodeSuggestion scope_suggestion(
        "Use separate scopes for mutable borrows",
        second_borrow_loc,
        "{ let borrow = &mut " + variable_name + "; /* use borrow */ }",
        "This ensures the first borrow ends before the second begins"
    );
    diag.add_suggestion(scope_suggestion);
    
    return diag;
}

Diagnostic OwnershipErrorBuilder::borrow_while_mutably_borrowed(
    const SourceLocation& immutable_borrow_loc,
    const SourceLocation& mutable_borrow_loc,
    const std::string& variable_name
) {
    Diagnostic diag(
        Severity::Error,
        "cannot borrow `" + variable_name + "` as immutable because it is also borrowed as mutable",
        immutable_borrow_loc
    );
    
    diag.add_note(
        "mutable borrow occurs here",
        mutable_borrow_loc
    );
    
    diag.add_related_info(
        "immutable and mutable borrows cannot coexist to prevent data races"
    );
    
    return diag;
}

Diagnostic OwnershipErrorBuilder::moved_value_not_copyable(
    const SourceLocation& move_loc,
    const std::string& variable_name,
    const std::string& type_name
) {
    Diagnostic diag(
        Severity::Error,
        "cannot move out of `" + variable_name + "` because it is not copyable",
        move_loc
    );
    
    diag.add_related_info(
        "type `" + type_name + "` does not implement the `Copy` trait"
    );
    
    CodeSuggestion derive_copy(
        "Implement Copy trait for the type",
        move_loc,
        "@derive(Copy, Clone)\nstruct " + type_name + " { ... }",
        "This allows the type to be copied implicitly"
    );
    diag.add_suggestion(derive_copy);
    
    return diag;
}

Diagnostic OwnershipErrorBuilder::lifetime_mismatch(
    const SourceLocation& error_loc,
    const std::string& expected_lifetime,
    const std::string& actual_lifetime
) {
    Diagnostic diag(
        Severity::Error,
        "lifetime mismatch: expected `" + expected_lifetime + "`, found `" + actual_lifetime + "`",
        error_loc
    );
    
    diag.add_related_info(
        "the lifetime `" + actual_lifetime + "` does not outlive `" + expected_lifetime + "`"
    );
    
    return diag;
}

// Type error builders
Diagnostic TypeErrorBuilder::type_mismatch(
    const SourceLocation& loc,
    const std::string& expected_type,
    const std::string& actual_type
) {
    Diagnostic diag(
        Severity::Error,
        "mismatched types: expected `" + expected_type + "`, found `" + actual_type + "`",
        loc
    );
    
    // Check for common conversions
    if (expected_type == "string" && actual_type == "int") {
        CodeSuggestion conversion(
            "Convert to string",
            loc,
            "value.to_string()",
            "Use the to_string() method to convert integers to strings"
        );
        diag.add_suggestion(conversion);
    }
    
    return diag;
}

Diagnostic TypeErrorBuilder::missing_trait_implementation(
    const SourceLocation& loc,
    const std::string& type_name,
    const std::string& trait_name
) {
    Diagnostic diag(
        Severity::Error,
        "the trait `" + trait_name + "` is not implemented for `" + type_name + "`",
        loc
    );
    
    CodeSuggestion derive_trait(
        "Derive the trait automatically",
        loc,
        "@derive(" + trait_name + ")\nstruct " + type_name + " { ... }",
        "Many common traits can be automatically derived"
    );
    diag.add_suggestion(derive_trait);
    
    CodeSuggestion manual_impl(
        "Implement the trait manually",
        loc,
        "impl " + trait_name + " for " + type_name + " {\n    // implementation\n}",
        "Provide a custom implementation of the trait"
    );
    diag.add_suggestion(manual_impl);
    
    return diag;
}

Diagnostic TypeErrorBuilder::non_exhaustive_pattern(
    const SourceLocation& match_loc,
    const std::vector<std::string>& missing_patterns
) {
    std::ostringstream missing;
    for (size_t i = 0; i < missing_patterns.size(); ++i) {
        if (i > 0) missing << ", ";
        missing << "`" << missing_patterns[i] << "`";
    }
    
    Diagnostic diag(
        Severity::Error,
        "non-exhaustive patterns: " + missing.str() + " not covered",
        match_loc
    );
    
    diag.add_related_info(
        "ensure that all possible cases are being handled"
    );
    
    // Suggest adding missing patterns
    std::ostringstream suggestion_text;
    for (const auto& pattern : missing_patterns) {
        suggestion_text << "    case " << pattern << " => // handle this case\n";
    }
    
    CodeSuggestion add_patterns(
        "Add missing patterns",
        match_loc,
        suggestion_text.str(),
        "Add cases for all missing patterns"
    );
    diag.add_suggestion(add_patterns);
    
    return diag;
}

Diagnostic TypeErrorBuilder::trait_coherence_violation(
    const SourceLocation& new_impl_loc,
    const SourceLocation& existing_impl_loc,
    const std::string& trait_name,
    const std::string& type_name
) {
    Diagnostic diag(
        Severity::Error,
        "conflicting implementations of trait `" + trait_name + "` for type `" + type_name + "`",
        new_impl_loc
    );
    
    diag.add_note(
        "first implementation here",
        existing_impl_loc
    );
    
    diag.add_related_info(
        "only one implementation of a trait is allowed per type (coherence rule)"
    );
    
    return diag;
}

// Concurrency error builders
Diagnostic ConcurrencyErrorBuilder::type_not_send(
    const SourceLocation& loc,
    const std::string& type_name,
    const std::string& reason
) {
    Diagnostic diag(
        Severity::Error,
        "`" + type_name + "` cannot be sent between threads safely",
        loc
    );
    
    diag.add_related_info(
        "the trait `Send` is not implemented for `" + type_name + "`"
    );
    
    diag.add_related_info(reason);
    
    CodeSuggestion wrap_in_arc(
        "Wrap in Arc for shared ownership",
        loc,
        "Arc::new(" + type_name + ")",
        "Arc provides thread-safe reference counting"
    );
    diag.add_suggestion(wrap_in_arc);
    
    return diag;
}

Diagnostic ConcurrencyErrorBuilder::type_not_sync(
    const SourceLocation& loc,
    const std::string& type_name,
    const std::string& reason
) {
    Diagnostic diag(
        Severity::Error,
        "`" + type_name + "` cannot be shared between threads safely",
        loc
    );
    
    diag.add_related_info(
        "the trait `Sync` is not implemented for `" + type_name + "`"
    );
    
    diag.add_related_info(reason);
    
    return diag;
}

Diagnostic ConcurrencyErrorBuilder::data_race_detected(
    const SourceLocation& loc,
    const std::string& variable_name,
    const std::vector<SourceLocation>& access_locations
) {
    Diagnostic diag(
        Severity::Error,
        "potential data race detected on `" + variable_name + "`",
        loc
    );
    
    for (const auto& access_loc : access_locations) {
        diag.add_note("concurrent access here", access_loc);
    }
    
    diag.add_related_info(
        "use synchronization primitives like Mutex or RwLock to prevent data races"
    );
    
    return diag;
}

// Error template formatting
std::string ErrorTemplate::format(const std::map<std::string, std::string>& replacements) const {
    std::string result = template_;
    
    for (const auto& [key, value] : replacements) {
        std::string placeholder = "{" + key + "}";
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.length(), value);
            pos += value.length();
        }
    }
    
    return result;
}

// Suggestion engine implementations
std::vector<CodeSuggestion> SuggestionEngine::suggest_ownership_fixes(
    const std::string& error_type,
    const SourceLocation& error_loc,
    const std::string& variable_name
) {
    std::vector<CodeSuggestion> suggestions;
    
    if (error_type == "use_after_move") {
        suggestions.push_back(CodeSuggestion(
            "Clone before moving",
            error_loc,
            variable_name + ".clone()",
            "Creates a copy, allowing continued use of the original"
        ));
        
        suggestions.push_back(CodeSuggestion(
            "Use a reference",
            error_loc,
            "&" + variable_name,
            "Borrows instead of moving"
        ));
    }
    
    return suggestions;
}

std::vector<CodeSuggestion> SuggestionEngine::suggest_trait_implementations(
    const std::string& type_name,
    const std::string& trait_name,
    const SourceLocation& loc
) {
    std::vector<CodeSuggestion> suggestions;
    
    // Common derivable traits
    std::vector<std::string> derivable = {"Debug", "Clone", "Copy", "PartialEq", "Eq", "Hash"};
    
    if (std::find(derivable.begin(), derivable.end(), trait_name) != derivable.end()) {
        suggestions.push_back(CodeSuggestion(
            "Derive " + trait_name + " automatically",
            loc,
            "@derive(" + trait_name + ")",
            "Add this attribute above the type definition"
        ));
    }
    
    return suggestions;
}

std::vector<CodeSuggestion> SuggestionEngine::suggest_pattern_completions(
    const std::vector<std::string>& missing_patterns,
    const SourceLocation& match_loc
) {
    std::vector<CodeSuggestion> suggestions;
    
    std::ostringstream cases;
    for (const auto& pattern : missing_patterns) {
        cases << "    case " << pattern << " => // TODO: handle this case\n";
    }
    
    suggestions.push_back(CodeSuggestion(
        "Add missing pattern cases",
        match_loc,
        cases.str(),
        "Ensure all possible patterns are handled"
    ));
    
    return suggestions;
}

std::vector<CodeSuggestion> SuggestionEngine::suggest_lifetime_annotations(
    const SourceLocation& loc,
    const std::string& function_signature
) {
    std::vector<CodeSuggestion> suggestions;
    
    suggestions.push_back(CodeSuggestion(
        "Add explicit lifetime parameters",
        loc,
        function_signature + "<'a>",
        "Specify lifetime relationships explicitly"
    ));
    
    return suggestions;
}

// Diagnostic collector
std::string DiagnosticCollector::format_all() const {
    std::ostringstream oss;
    
    for (const auto& diag : diagnostics_) {
        oss << diag.format() << "\n";
    }
    
    // Summary
    size_t error_count = 0;
    size_t warning_count = 0;
    
    for (const auto& diag : diagnostics_) {
        if (diag.severity() == Severity::Error) error_count++;
        if (diag.severity() == Severity::Warning) warning_count++;
    }
    
    if (error_count > 0 || warning_count > 0) {
        oss << "\nSummary: ";
        if (error_count > 0) {
            oss << error_count << " error" << (error_count != 1 ? "s" : "");
        }
        if (warning_count > 0) {
            if (error_count > 0) oss << ", ";
            oss << warning_count << " warning" << (warning_count != 1 ? "s" : "");
        }
        oss << "\n";
    }
    
    return oss.str();
}

} // namespace diagnostics
} // namespace meld
