#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <map>

namespace meld {
namespace lsp {

// LSP position and range structures
struct Position {
    size_t line;
    size_t character;
    
    Position() : line(0), character(0) {}
    Position(size_t l, size_t c) : line(l), character(c) {}
};

struct Range {
    Position start;
    Position end;
    
    Range() = default;
    Range(const Position& s, const Position& e) : start(s), end(e) {}
};

// Ownership information for LSP
enum class OwnershipStatus {
    Owned,
    BorrowedImmutable,
    BorrowedMutable,
    Moved,
    Uninitialized
};

struct OwnershipInfo {
    OwnershipStatus status;
    std::string owner;
    std::optional<std::string> lifetime;
    std::vector<std::string> borrowers;
    
    OwnershipInfo() : status(OwnershipStatus::Owned) {}
    OwnershipInfo(OwnershipStatus s, const std::string& o = "")
        : status(s), owner(o) {}
};

// Hover information with ownership details
struct HoverInfo {
    std::string type_info;
    std::string documentation;
    std::optional<OwnershipInfo> ownership;
    std::vector<std::string> traits;
    
    std::string format() const;
};

// Code completion item with ownership awareness
struct CompletionItem {
    std::string label;
    std::string detail;
    std::string documentation;
    std::string insert_text;
    std::string kind;  // function, variable, type, etc.
    std::optional<OwnershipInfo> ownership_hint;
    
    CompletionItem(const std::string& lbl, const std::string& det = "")
        : label(lbl), detail(det), kind("text") {}
};

// Diagnostic severity for LSP
enum class DiagnosticSeverity {
    Error = 1,
    Warning = 2,
    Information = 3,
    Hint = 4
};

// LSP diagnostic with ownership context
struct Diagnostic {
    Range range;
    DiagnosticSeverity severity;
    std::string message;
    std::string source;
    std::optional<std::string> code;
    std::vector<std::string> related_information;
    
    Diagnostic(const Range& r, DiagnosticSeverity sev, const std::string& msg)
        : range(r), severity(sev), message(msg), source("meld-borrow-checker") {}
};

// Code action for quick fixes
struct CodeAction {
    std::string title;
    std::string kind;  // quickfix, refactor, etc.
    std::vector<std::pair<Range, std::string>> edits;
    
    CodeAction(const std::string& t, const std::string& k = "quickfix")
        : title(t), kind(k) {}
};

// Ownership-aware LSP provider
class OwnershipLSPProvider {
public:
    // Hover information with ownership details
    std::optional<HoverInfo> provide_hover(
        const std::string& file_path,
        const Position& position
    );
    
    // Code completion with ownership hints
    std::vector<CompletionItem> provide_completion(
        const std::string& file_path,
        const Position& position,
        const std::string& trigger_character = ""
    );
    
    // Real-time borrow checker diagnostics
    std::vector<Diagnostic> provide_diagnostics(
        const std::string& file_path,
        const std::string& content
    );
    
    // Code actions for ownership fixes
    std::vector<CodeAction> provide_code_actions(
        const std::string& file_path,
        const Range& range,
        const std::vector<Diagnostic>& diagnostics
    );
    
    // Signature help with lifetime information
    struct SignatureHelp {
        std::string label;
        std::string documentation;
        std::vector<std::string> parameters;
        std::vector<std::string> lifetimes;
        size_t active_parameter;
    };
    
    std::optional<SignatureHelp> provide_signature_help(
        const std::string& file_path,
        const Position& position
    );
    
    // Inlay hints for ownership and lifetimes
    struct InlayHint {
        Position position;
        std::string label;
        std::string kind;  // type, parameter, ownership
        
        InlayHint(const Position& pos, const std::string& lbl, const std::string& k)
            : position(pos), label(lbl), kind(k) {}
    };
    
    std::vector<InlayHint> provide_inlay_hints(
        const std::string& file_path,
        const Range& range
    );
    
    // Semantic tokens for ownership visualization
    enum class TokenType {
        Variable,
        Function,
        Type,
        Lifetime,
        OwnershipKeyword
    };
    
    enum class TokenModifier {
        Owned,
        Borrowed,
        Mutable,
        Moved
    };
    
    struct SemanticToken {
        Position position;
        size_t length;
        TokenType type;
        std::vector<TokenModifier> modifiers;
    };
    
    std::vector<SemanticToken> provide_semantic_tokens(
        const std::string& file_path
    );
    
private:
    // Helper methods
    OwnershipInfo analyze_ownership_at_position(
        const std::string& file_path,
        const Position& position
    );
    
    std::vector<CompletionItem> get_ownership_completions(
        const std::string& context
    );
    
    std::vector<Diagnostic> run_borrow_checker(
        const std::string& file_path,
        const std::string& content
    );
    
    std::vector<CodeAction> generate_ownership_fixes(
        const Diagnostic& diagnostic
    );
};

// Real-time borrow checker integration
class RealtimeBorrowChecker {
public:
    // Incremental analysis for LSP
    struct AnalysisResult {
        std::vector<Diagnostic> diagnostics;
        std::map<std::string, OwnershipInfo> ownership_map;
        std::vector<std::string> warnings;
    };
    
    AnalysisResult analyze_incremental(
        const std::string& file_path,
        const std::string& content,
        const std::optional<Range>& changed_range = std::nullopt
    );
    
    // Quick ownership check for specific symbol
    std::optional<OwnershipInfo> check_symbol_ownership(
        const std::string& file_path,
        const std::string& symbol_name,
        const Position& position
    );
    
    // Validate borrow at position
    struct BorrowValidation {
        bool is_valid;
        std::string reason;
        std::vector<std::string> suggestions;
    };
    
    BorrowValidation validate_borrow(
        const std::string& file_path,
        const Position& position
    );
    
private:
    // Cache for incremental analysis
    std::map<std::string, AnalysisResult> analysis_cache_;
};

// Rust-inspired feature completions
class RustFeatureCompletions {
public:
    // Ownership-related completions
    static std::vector<CompletionItem> get_ownership_keywords();
    
    // Trait completions
    static std::vector<CompletionItem> get_trait_completions(
        const std::string& type_name
    );
    
    // Pattern matching completions
    static std::vector<CompletionItem> get_pattern_completions(
        const std::string& enum_type
    );
    
    // Lifetime completions
    static std::vector<CompletionItem> get_lifetime_completions();
    
    // Derive macro completions
    static std::vector<CompletionItem> get_derive_completions();
};

} // namespace lsp
} // namespace meld
