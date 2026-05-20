#include "meld/lsp/ownership_lsp.hpp"
#include <sstream>
#include <algorithm>

namespace meld {
namespace lsp {

// Format hover information
std::string HoverInfo::format() const {
    std::ostringstream oss;
    
    oss << "**Type:** `" << type_info << "`\n\n";
    
    if (ownership.has_value()) {
        oss << "**Ownership:** ";
        switch (ownership->status) {
            case OwnershipStatus::Owned:
                oss << "Owned";
                break;
            case OwnershipStatus::BorrowedImmutable:
                oss << "Borrowed (immutable)";
                break;
            case OwnershipStatus::BorrowedMutable:
                oss << "Borrowed (mutable)";
                break;
            case OwnershipStatus::Moved:
                oss << "Moved (no longer accessible)";
                break;
            case OwnershipStatus::Uninitialized:
                oss << "Uninitialized";
                break;
        }
        
        if (!ownership->owner.empty()) {
            oss << " by `" << ownership->owner << "`";
        }
        
        if (ownership->lifetime.has_value()) {
            oss << " (lifetime: `" << *ownership->lifetime << "`)";
        }
        
        if (!ownership->borrowers.empty()) {
            oss << "\n**Active borrows:** ";
            for (size_t i = 0; i < ownership->borrowers.size(); ++i) {
                if (i > 0) oss << ", ";
                oss << "`" << ownership->borrowers[i] << "`";
            }
        }
        
        oss << "\n\n";
    }
    
    if (!traits.empty()) {
        oss << "**Traits:** ";
        for (size_t i = 0; i < traits.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << "`" << traits[i] << "`";
        }
        oss << "\n\n";
    }
    
    if (!documentation.empty()) {
        oss << documentation;
    }
    
    return oss.str();
}

// Provide hover information
std::optional<HoverInfo> OwnershipLSPProvider::provide_hover(
    const std::string& file_path,
    const Position& position
) {
    // Analyze ownership at position
    auto ownership = analyze_ownership_at_position(file_path, position);
    
    HoverInfo info;
    info.type_info = "Vec<int>";  // Example - would be determined by analysis
    info.ownership = ownership;
    info.traits = {"Clone", "Debug", "Send", "Sync"};
    info.documentation = "A dynamically-sized array type with ownership semantics.";
    
    return info;
}

// Provide code completion
std::vector<CompletionItem> OwnershipLSPProvider::provide_completion(
    const std::string& file_path,
    const Position& position,
    const std::string& trigger_character
) {
    std::vector<CompletionItem> items;
    
    // Add ownership-related completions
    if (trigger_character == "&") {
        CompletionItem borrow("&", "Immutable borrow");
        borrow.insert_text = "&";
        borrow.kind = "keyword";
        items.push_back(borrow);
        
        CompletionItem borrow_mut("&mut", "Mutable borrow");
        borrow_mut.insert_text = "&mut ";
        borrow_mut.kind = "keyword";
        items.push_back(borrow_mut);
    }
    
    // Add Rust-inspired feature completions
    auto rust_completions = RustFeatureCompletions::get_ownership_keywords();
    items.insert(items.end(), rust_completions.begin(), rust_completions.end());
    
    // Add context-specific completions
    auto context_completions = get_ownership_completions(file_path);
    items.insert(items.end(), context_completions.begin(), context_completions.end());
    
    return items;
}

// Provide real-time diagnostics
std::vector<Diagnostic> OwnershipLSPProvider::provide_diagnostics(
    const std::string& file_path,
    const std::string& content
) {
    return run_borrow_checker(file_path, content);
}

// Provide code actions
std::vector<CodeAction> OwnershipLSPProvider::provide_code_actions(
    const std::string& file_path,
    const Range& range,
    const std::vector<Diagnostic>& diagnostics
) {
    std::vector<CodeAction> actions;
    
    for (const auto& diag : diagnostics) {
        auto fixes = generate_ownership_fixes(diag);
        actions.insert(actions.end(), fixes.begin(), fixes.end());
    }
    
    return actions;
}

// Provide signature help
std::optional<OwnershipLSPProvider::SignatureHelp> OwnershipLSPProvider::provide_signature_help(
    const std::string& file_path,
    const Position& position
) {
    SignatureHelp help;
    help.label = "func process<'a>(data: &'a Vec<int>) -> &'a int";
    help.documentation = "Processes a vector and returns a reference to an element";
    help.parameters = {"data: &'a Vec<int>"};
    help.lifetimes = {"'a"};
    help.active_parameter = 0;
    
    return help;
}

// Provide inlay hints
std::vector<OwnershipLSPProvider::InlayHint> OwnershipLSPProvider::provide_inlay_hints(
    const std::string& file_path,
    const Range& range
) {
    std::vector<InlayHint> hints;
    
    // Example: show ownership status
    hints.push_back(InlayHint(
        Position(10, 15),
        ": Owned<Vec<int>>",
        "type"
    ));
    
    hints.push_back(InlayHint(
        Position(12, 20),
        "/* borrowed */",
        "ownership"
    ));
    
    hints.push_back(InlayHint(
        Position(15, 10),
        "<'a>",
        "lifetime"
    ));
    
    return hints;
}

// Provide semantic tokens
std::vector<OwnershipLSPProvider::SemanticToken> OwnershipLSPProvider::provide_semantic_tokens(
    const std::string& file_path
) {
    std::vector<SemanticToken> tokens;
    
    // Example tokens with ownership modifiers
    SemanticToken owned_var;
    owned_var.position = Position(10, 5);
    owned_var.length = 4;
    owned_var.type = TokenType::Variable;
    owned_var.modifiers = {TokenModifier::Owned};
    tokens.push_back(owned_var);
    
    SemanticToken borrowed_var;
    borrowed_var.position = Position(12, 10);
    borrowed_var.length = 3;
    borrowed_var.type = TokenType::Variable;
    borrowed_var.modifiers = {TokenModifier::Borrowed};
    tokens.push_back(borrowed_var);
    
    SemanticToken mut_borrowed_var;
    mut_borrowed_var.position = Position(15, 8);
    mut_borrowed_var.length = 5;
    mut_borrowed_var.type = TokenType::Variable;
    mut_borrowed_var.modifiers = {TokenModifier::Borrowed, TokenModifier::Mutable};
    tokens.push_back(mut_borrowed_var);
    
    return tokens;
}

// Helper: Analyze ownership at position
OwnershipInfo OwnershipLSPProvider::analyze_ownership_at_position(
    const std::string& file_path,
    const Position& position
) {
    // Simplified example - would integrate with actual borrow checker
    OwnershipInfo info(OwnershipStatus::Owned, "current_scope");
    info.lifetime = "'a";
    return info;
}

// Helper: Get ownership completions
std::vector<CompletionItem> OwnershipLSPProvider::get_ownership_completions(
    const std::string& context
) {
    std::vector<CompletionItem> items;
    
    CompletionItem clone("clone()", "Create a copy of the value");
    clone.insert_text = "clone()";
    clone.kind = "method";
    items.push_back(clone);
    
    CompletionItem borrow("borrow()", "Create an immutable borrow");
    borrow.insert_text = "borrow()";
    borrow.kind = "method";
    items.push_back(borrow);
    
    CompletionItem borrow_mut("borrow_mut()", "Create a mutable borrow");
    borrow_mut.insert_text = "borrow_mut()";
    borrow_mut.kind = "method";
    items.push_back(borrow_mut);
    
    return items;
}

// Helper: Run borrow checker
std::vector<Diagnostic> OwnershipLSPProvider::run_borrow_checker(
    const std::string& file_path,
    const std::string& content
) {
    std::vector<Diagnostic> diagnostics;
    
    // Example diagnostic - would be generated by actual borrow checker
    Diagnostic use_after_move(
        Range(Position(15, 10), Position(15, 14)),
        DiagnosticSeverity::Error,
        "use of moved value: `data`"
    );
    use_after_move.code = "E0382";
    use_after_move.related_information.push_back("value moved at line 14");
    diagnostics.push_back(use_after_move);
    
    return diagnostics;
}

// Helper: Generate ownership fixes
std::vector<CodeAction> OwnershipLSPProvider::generate_ownership_fixes(
    const Diagnostic& diagnostic
) {
    std::vector<CodeAction> actions;
    
    if (diagnostic.message.find("moved value") != std::string::npos) {
        CodeAction clone_fix("Clone the value before moving");
        clone_fix.edits.push_back({diagnostic.range, ".clone()"});
        actions.push_back(clone_fix);
        
        CodeAction borrow_fix("Borrow instead of moving");
        borrow_fix.edits.push_back({diagnostic.range, "&"});
        actions.push_back(borrow_fix);
    }
    
    if (diagnostic.message.find("mutable borrow") != std::string::npos) {
        CodeAction scope_fix("Use separate scope for borrow");
        scope_fix.kind = "refactor";
        actions.push_back(scope_fix);
    }
    
    return actions;
}

// Realtime borrow checker
RealtimeBorrowChecker::AnalysisResult RealtimeBorrowChecker::analyze_incremental(
    const std::string& file_path,
    const std::string& content,
    const std::optional<Range>& changed_range
) {
    AnalysisResult result;
    
    // Check cache
    if (analysis_cache_.find(file_path) != analysis_cache_.end() && !changed_range.has_value()) {
        return analysis_cache_[file_path];
    }
    
    // Perform incremental analysis
    // In a real implementation, this would only re-analyze changed regions
    
    // Example ownership info
    result.ownership_map["data"] = OwnershipInfo(OwnershipStatus::Owned, "main");
    result.ownership_map["ref_data"] = OwnershipInfo(OwnershipStatus::BorrowedImmutable, "data");
    
    // Example diagnostic
    Diagnostic diag(
        Range(Position(10, 5), Position(10, 9)),
        DiagnosticSeverity::Warning,
        "variable does not need to be mutable"
    );
    result.diagnostics.push_back(diag);
    
    // Cache result
    analysis_cache_[file_path] = result;
    
    return result;
}

std::optional<OwnershipInfo> RealtimeBorrowChecker::check_symbol_ownership(
    const std::string& file_path,
    const std::string& symbol_name,
    const Position& position
) {
    auto result = analyze_incremental(file_path, "");
    
    auto it = result.ownership_map.find(symbol_name);
    if (it != result.ownership_map.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

RealtimeBorrowChecker::BorrowValidation RealtimeBorrowChecker::validate_borrow(
    const std::string& file_path,
    const Position& position
) {
    BorrowValidation validation;
    validation.is_valid = true;
    validation.reason = "Borrow is valid";
    
    // Example validation logic
    // In real implementation, would check actual borrow rules
    
    return validation;
}

// Rust feature completions
std::vector<CompletionItem> RustFeatureCompletions::get_ownership_keywords() {
    std::vector<CompletionItem> items;
    
    CompletionItem owned("Owned<T>", "Owned value type");
    owned.insert_text = "Owned<$1>";
    owned.kind = "type";
    items.push_back(owned);
    
    CompletionItem borrowed("Borrowed<T>", "Borrowed reference type");
    borrowed.insert_text = "Borrowed<$1>";
    borrowed.kind = "type";
    items.push_back(borrowed);
    
    CompletionItem move_kw("move", "Transfer ownership");
    move_kw.insert_text = "move";
    move_kw.kind = "keyword";
    items.push_back(move_kw);
    
    return items;
}

std::vector<CompletionItem> RustFeatureCompletions::get_trait_completions(
    const std::string& type_name
) {
    std::vector<CompletionItem> items;
    
    std::vector<std::string> common_traits = {
        "Clone", "Copy", "Debug", "Display", "PartialEq", "Eq",
        "PartialOrd", "Ord", "Hash", "Send", "Sync"
    };
    
    for (const auto& trait : common_traits) {
        CompletionItem item(trait, "Implement " + trait + " trait");
        item.insert_text = trait;
        item.kind = "interface";
        items.push_back(item);
    }
    
    return items;
}

std::vector<CompletionItem> RustFeatureCompletions::get_pattern_completions(
    const std::string& enum_type
) {
    std::vector<CompletionItem> items;
    
    CompletionItem some_pattern("some(value)", "Match some variant");
    some_pattern.insert_text = "some($1) => $2";
    some_pattern.kind = "snippet";
    items.push_back(some_pattern);
    
    CompletionItem none_pattern("none", "Match none variant");
    none_pattern.insert_text = "none => $1";
    none_pattern.kind = "snippet";
    items.push_back(none_pattern);
    
    CompletionItem ok_pattern("ok(value)", "Match ok variant");
    ok_pattern.insert_text = "ok($1) => $2";
    ok_pattern.kind = "snippet";
    items.push_back(ok_pattern);
    
    CompletionItem err_pattern("err(error)", "Match err variant");
    err_pattern.insert_text = "err($1) => $2";
    err_pattern.kind = "snippet";
    items.push_back(err_pattern);
    
    return items;
}

std::vector<CompletionItem> RustFeatureCompletions::get_lifetime_completions() {
    std::vector<CompletionItem> items;
    
    CompletionItem lifetime_a("'a", "Lifetime parameter 'a");
    lifetime_a.insert_text = "'a";
    lifetime_a.kind = "typeParameter";
    items.push_back(lifetime_a);
    
    CompletionItem lifetime_static("'static", "Static lifetime");
    lifetime_static.insert_text = "'static";
    lifetime_static.kind = "typeParameter";
    items.push_back(lifetime_static);
    
    return items;
}

std::vector<CompletionItem> RustFeatureCompletions::get_derive_completions() {
    std::vector<CompletionItem> items;
    
    CompletionItem derive("@derive(...)", "Derive traits automatically");
    derive.insert_text = "@derive($1)";
    derive.kind = "snippet";
    derive.documentation = "Automatically implement common traits";
    items.push_back(derive);
    
    return items;
}

} // namespace lsp
} // namespace meld
