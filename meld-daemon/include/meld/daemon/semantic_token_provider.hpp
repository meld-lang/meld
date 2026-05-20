#pragma once

#include "meld/daemon/semantic_model.hpp"

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace meld::daemon {

/// Semantic token types for LSP syntax highlighting
enum class SemanticTokenType {
    Keyword,
    Function,
    Variable,
    Type,
    Parameter,
    Property,
    String,
    Number,
    Comment,
    Operator,
    Decorator,
    Effect,
    Macro,
    Namespace
};

/// A single semantic token for syntax highlighting
struct SemanticToken {
    int line;
    int start_char;
    int length;
    SemanticTokenType type;
    int modifiers = 0;
};

/// Internal token types produced by the Meld tokenizer
enum class MeldTokenType {
    Keyword,
    Identifier,
    TypeName,
    Number,
    String,
    Operator,
    Comment,
    Decorator,
    Punctuation,
    EndOfFile,
    Invalid
};

/// A token from the Meld tokenizer
struct MeldToken {
    MeldTokenType type;
    std::string value;
    int line;
    int column;
};

/// Structured diagnostic with precise location information (Req 15.3)
struct ParseDiagnostic {
    int line = 0;
    int column = 0;
    int end_line = 0;
    int end_column = 0;
    std::string message;
    std::string source = "meld";
};

/// Result of parsing a Meld source file
struct ParseResult {
    bool success = false;
    std::vector<SemanticToken> tokens;
    std::vector<std::string> errors;
    std::vector<ParseDiagnostic> diagnostics;  // Structured diagnostics with locations
    bool has_partial_results = false;
};

/// Cached parse entry
struct CachedParse {
    std::string uri;
    std::string content;
    ParseResult result;
    std::chrono::steady_clock::time_point timestamp;
    int version = 0;
};

/// Returns true if the given text is a Meld keyword.
bool is_meld_keyword(const std::string& text);

/// Classifies a Meld language construct text into a SemanticTokenType.
SemanticTokenType classify_construct(const std::string& text);

/// Lightweight Meld tokenizer for LSP purposes.
/// Prioritizes error recovery over strict correctness.
class MeldLspTokenizer {
public:
    std::vector<MeldToken> tokenize(const std::string& source);
    const std::vector<std::string>& errors() const { return errors_; }
    const std::vector<ParseDiagnostic>& diagnostics() const { return diagnostics_; }

private:
    std::string source_;
    size_t pos_ = 0;
    int line_ = 0;
    int col_ = 0;
    std::vector<std::string> errors_;
    std::vector<ParseDiagnostic> diagnostics_;

    char peek() const;
    char peek_ahead(size_t offset) const;
    char advance();
    bool at_end() const;
    void skip_whitespace();

    MeldToken read_string();
    MeldToken read_multiline_string();
    MeldToken read_number();
    MeldToken read_identifier_or_keyword();
    MeldToken read_operator();
    MeldToken read_decorator();
    MeldToken read_line_comment();
    MeldToken read_block_comment();
};

/// Semantic token provider that integrates with the SemanticModel.
/// Provides tokenization, AST-to-LSP conversion, caching, and error recovery.
class SemanticTokenProvider {
public:
    SemanticTokenProvider();
    ~SemanticTokenProvider();

    /// Parse source text and produce semantic tokens.
    ParseResult parse(const std::string& source);

    /// Parse and cache the result for a document URI.
    ParseResult parse_document(const std::string& uri,
                               const std::string& content,
                               int version = 0);

    /// Retrieve cached parse result for a URI.
    std::optional<ParseResult> get_cached(const std::string& uri) const;

    /// Retrieve cached document content for a URI.
    std::optional<std::string> get_content(const std::string& uri) const;

    /// Invalidate cached result for a URI.
    void invalidate(const std::string& uri);

    /// Clear all cached results.
    void clear_cache();

    /// Convert MeldTokens to LSP SemanticTokens.
    static std::vector<SemanticToken> to_semantic_tokens(
        const std::vector<MeldToken>& tokens);

    /// Classify a single MeldToken into an LSP SemanticTokenType.
    static SemanticTokenType classify_token(const MeldToken& token);

    /// Delta-encode semantic tokens for LSP protocol.
    static std::vector<int> encode_semantic_tokens(
        const std::vector<SemanticToken>& tokens);

    /// Get the semantic token legend type names.
    static std::vector<std::string> token_type_names();

private:
    mutable std::mutex cache_mutex_;
    std::unordered_map<std::string, CachedParse> cache_;
};

}  // namespace meld::daemon
