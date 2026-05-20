#include "meld/daemon/semantic_token_provider.hpp"

#include <algorithm>
#include <cctype>

namespace meld::daemon {

namespace {

const std::unordered_set<std::string> KEYWORDS = {
    "fnc", "let", "val", "var", "if", "else", "match", "return", "rtn",
    "import", "imp", "struct", "enum", "trait", "impl",
    "async", "await", "for", "while", "true", "false",
    "effect", "handle", "perform", "resume",
    "type", "newtype", "extend", "operator",
    "namespace", "from", "where", "select", "join", "group",
    "query", "flow", "on", "goto", "test", "assert",
    "infix", "prefix", "postfix"
};

}  // anonymous namespace

// ---------------------------------------------------------------------------
// Free functions
// ---------------------------------------------------------------------------

bool is_meld_keyword(const std::string& text) {
    return KEYWORDS.count(text) > 0;
}

SemanticTokenType classify_construct(const std::string& text) {
    if (KEYWORDS.count(text)) return SemanticTokenType::Keyword;
    if (text.size() > 1 && text[0] == '@') return SemanticTokenType::Decorator;
    if (!text.empty() && std::isupper(static_cast<unsigned char>(text[0])))
        return SemanticTokenType::Type;
    return SemanticTokenType::Variable;
}

// ---------------------------------------------------------------------------
// MeldLspTokenizer
// ---------------------------------------------------------------------------

char MeldLspTokenizer::peek() const {
    if (pos_ >= source_.size()) return '\0';
    return source_[pos_];
}

char MeldLspTokenizer::peek_ahead(size_t offset) const {
    if (pos_ + offset >= source_.size()) return '\0';
    return source_[pos_ + offset];
}

char MeldLspTokenizer::advance() {
    if (pos_ >= source_.size()) return '\0';
    char c = source_[pos_++];
    if (c == '\n') { line_++; col_ = 0; }
    else { col_++; }
    return c;
}

bool MeldLspTokenizer::at_end() const {
    return pos_ >= source_.size();
}

void MeldLspTokenizer::skip_whitespace() {
    while (!at_end()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') advance();
        else break;
    }
}

MeldToken MeldLspTokenizer::read_line_comment() {
    int sl = line_, sc = col_;
    std::string value;
    value += advance(); value += advance();
    while (!at_end() && peek() != '\n') value += advance();
    return {MeldTokenType::Comment, value, sl, sc};
}

MeldToken MeldLspTokenizer::read_block_comment() {
    int sl = line_, sc = col_;
    std::string value;
    value += advance(); value += advance();
    while (!at_end()) {
        if (peek() == '*' && peek_ahead(1) == '/') {
            value += advance(); value += advance();
            return {MeldTokenType::Comment, value, sl, sc};
        }
        value += advance();
    }
    errors_.push_back("Unterminated block comment at line " + std::to_string(sl + 1));
    diagnostics_.push_back({sl, sc, line_, col_, "Unterminated block comment", "meld"});
    return {MeldTokenType::Comment, value, sl, sc};
}

MeldToken MeldLspTokenizer::read_string() {
    int sl = line_, sc = col_;
    std::string value;
    if (peek_ahead(1) == '"' && peek_ahead(2) == '"') return read_multiline_string();
    value += advance();
    while (!at_end() && peek() != '"' && peek() != '\n') {
        if (peek() == '\\' && !at_end()) {
            value += advance();
            if (!at_end()) value += advance();
        } else {
            value += advance();
        }
    }
    if (!at_end() && peek() == '"') value += advance();
    else {
        errors_.push_back("Unterminated string literal at line " + std::to_string(sl + 1));
        diagnostics_.push_back({sl, sc, line_, col_, "Unterminated string literal", "meld"});
    }
    return {MeldTokenType::String, value, sl, sc};
}

MeldToken MeldLspTokenizer::read_multiline_string() {
    int sl = line_, sc = col_;
    std::string value;
    value += advance(); value += advance(); value += advance();
    while (!at_end()) {
        if (peek() == '"' && peek_ahead(1) == '"' && peek_ahead(2) == '"') {
            value += advance(); value += advance(); value += advance();
            return {MeldTokenType::String, value, sl, sc};
        }
        value += advance();
    }
    errors_.push_back("Unterminated multiline string at line " + std::to_string(sl + 1));
    diagnostics_.push_back({sl, sc, line_, col_, "Unterminated multiline string", "meld"});
    return {MeldTokenType::String, value, sl, sc};
}

MeldToken MeldLspTokenizer::read_number() {
    int sl = line_, sc = col_;
    std::string value;
    bool has_dot = false;
    while (!at_end()) {
        char c = peek();
        if (std::isdigit(static_cast<unsigned char>(c))) {
            value += advance();
        } else if (c == '.' && !has_dot &&
                   std::isdigit(static_cast<unsigned char>(peek_ahead(1)))) {
            has_dot = true;
            value += advance();
        } else if (c == '_') {
            advance();
        } else {
            break;
        }
    }
    if (!at_end()) {
        char c = peek();
        if (c == 'L' || c == 'F' || c == 'D' || c == 'l' || c == 'f' || c == 'd')
            value += advance();
    }
    return {MeldTokenType::Number, value, sl, sc};
}

MeldToken MeldLspTokenizer::read_identifier_or_keyword() {
    int sl = line_, sc = col_;
    std::string value;
    while (!at_end()) {
        char c = peek();
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') value += advance();
        else break;
    }
    if (KEYWORDS.count(value)) return {MeldTokenType::Keyword, value, sl, sc};
    if (!value.empty() && std::isupper(static_cast<unsigned char>(value[0])))
        return {MeldTokenType::TypeName, value, sl, sc};
    return {MeldTokenType::Identifier, value, sl, sc};
}

MeldToken MeldLspTokenizer::read_operator() {
    int sl = line_, sc = col_;
    std::string value;
    char c = peek();
    if ((c == '=' || c == '!' || c == '<' || c == '>') && peek_ahead(1) == '=') {
        value += advance(); value += advance();
    } else if (c == '|' && peek_ahead(1) == '>') {
        value += advance(); value += advance();
    } else if (c == '-' && peek_ahead(1) == '>') {
        value += advance(); value += advance();
    } else if (c == '=' && peek_ahead(1) == '>') {
        value += advance(); value += advance();
    } else if (c == '&' && peek_ahead(1) == '&') {
        value += advance(); value += advance();
    } else if (c == '|' && peek_ahead(1) == '|') {
        value += advance(); value += advance();
    } else if (c == '?' && peek_ahead(1) == '?') {
        value += advance(); value += advance();
    } else if (c == '?' && peek_ahead(1) == '.') {
        value += advance(); value += advance();
    } else if (c == '.' && peek_ahead(1) == '.' && peek_ahead(2) == '.') {
        value += advance(); value += advance(); value += advance();
    } else {
        value += advance();
    }
    return {MeldTokenType::Operator, value, sl, sc};
}

MeldToken MeldLspTokenizer::read_decorator() {
    int sl = line_, sc = col_;
    std::string value;
    value += advance();
    while (!at_end()) {
        char c = peek();
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') value += advance();
        else break;
    }
    return {MeldTokenType::Decorator, value, sl, sc};
}

std::vector<MeldToken> MeldLspTokenizer::tokenize(const std::string& source) {
    source_ = source;
    pos_ = 0; line_ = 0; col_ = 0;
    errors_.clear();
    diagnostics_.clear();
    std::vector<MeldToken> tokens;

    while (!at_end()) {
        skip_whitespace();
        if (at_end()) break;
        char c = peek();

        if (c == '/' && peek_ahead(1) == '/') { tokens.push_back(read_line_comment()); continue; }
        if (c == '/' && peek_ahead(1) == '*') { tokens.push_back(read_block_comment()); continue; }
        if (c == '"') { tokens.push_back(read_string()); continue; }
        if (std::isdigit(static_cast<unsigned char>(c))) { tokens.push_back(read_number()); continue; }
        if (c == '@') { tokens.push_back(read_decorator()); continue; }
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            tokens.push_back(read_identifier_or_keyword()); continue;
        }
        if (c == '(' || c == ')' || c == '{' || c == '}' ||
            c == '[' || c == ']' || c == ',' || c == ';' || c == ':') {
            int sl = line_, sc = col_;
            std::string v(1, advance());
            tokens.push_back({MeldTokenType::Punctuation, v, sl, sc});
            continue;
        }
        if (c == '.') {
            if (peek_ahead(1) == '.' && peek_ahead(2) == '.') tokens.push_back(read_operator());
            else { int sl = line_, sc = col_; std::string v(1, advance());
                   tokens.push_back({MeldTokenType::Punctuation, v, sl, sc}); }
            continue;
        }
        if (c == '+' || c == '-' || c == '*' || c == '/' ||
            c == '=' || c == '<' || c == '>' || c == '!' ||
            c == '&' || c == '|' || c == '^' || c == '~' ||
            c == '%' || c == '?') {
            tokens.push_back(read_operator()); continue;
        }
        {
            int sl = line_, sc = col_;
            std::string v(1, advance());
            errors_.push_back("Unexpected character '" + v + "' at line " +
                              std::to_string(sl + 1) + " col " + std::to_string(sc + 1));
            diagnostics_.push_back({sl, sc, sl, sc + 1,
                                    "Unexpected character '" + v + "'", "meld"});
            tokens.push_back({MeldTokenType::Invalid, v, sl, sc});
        }
    }
    return tokens;
}

// ---------------------------------------------------------------------------
// SemanticTokenProvider
// ---------------------------------------------------------------------------

SemanticTokenProvider::SemanticTokenProvider() = default;
SemanticTokenProvider::~SemanticTokenProvider() = default;

SemanticTokenType SemanticTokenProvider::classify_token(const MeldToken& token) {
    switch (token.type) {
        case MeldTokenType::Keyword:    return SemanticTokenType::Keyword;
        case MeldTokenType::String:     return SemanticTokenType::String;
        case MeldTokenType::Number:     return SemanticTokenType::Number;
        case MeldTokenType::Operator:   return SemanticTokenType::Operator;
        case MeldTokenType::Comment:    return SemanticTokenType::Comment;
        case MeldTokenType::Decorator:  return SemanticTokenType::Decorator;
        case MeldTokenType::TypeName:   return SemanticTokenType::Type;
        case MeldTokenType::Identifier: return SemanticTokenType::Variable;
        default:                        return SemanticTokenType::Variable;
    }
}

std::vector<SemanticToken> SemanticTokenProvider::to_semantic_tokens(
    const std::vector<MeldToken>& tokens) {
    std::vector<SemanticToken> result;
    result.reserve(tokens.size());
    for (const auto& tok : tokens) {
        if (tok.type == MeldTokenType::Punctuation ||
            tok.type == MeldTokenType::EndOfFile ||
            tok.type == MeldTokenType::Invalid) continue;
        if (tok.value.empty()) continue;
        result.push_back({tok.line, tok.column,
                          static_cast<int>(tok.value.size()),
                          classify_token(tok), 0});
    }
    return result;
}

std::vector<int> SemanticTokenProvider::encode_semantic_tokens(
    const std::vector<SemanticToken>& tokens) {
    std::vector<int> encoded;
    encoded.reserve(tokens.size() * 5);
    int prev_line = 0, prev_char = 0;
    for (const auto& t : tokens) {
        int delta_line = t.line - prev_line;
        int delta_char = (delta_line == 0) ? (t.start_char - prev_char) : t.start_char;
        encoded.push_back(delta_line);
        encoded.push_back(delta_char);
        encoded.push_back(t.length);
        encoded.push_back(static_cast<int>(t.type));
        encoded.push_back(t.modifiers);
        prev_line = t.line;
        prev_char = t.start_char;
    }
    return encoded;
}

std::vector<std::string> SemanticTokenProvider::token_type_names() {
    return {"keyword", "function", "variable", "type", "parameter",
            "property", "string", "number", "comment", "operator",
            "decorator", "event", "macro", "namespace"};
}

ParseResult SemanticTokenProvider::parse(const std::string& source) {
    ParseResult result;
    if (source.empty()) { result.success = true; return result; }

    MeldLspTokenizer tokenizer;
    auto meld_tokens = tokenizer.tokenize(source);
    for (const auto& err : tokenizer.errors()) result.errors.push_back(err);
    for (const auto& diag : tokenizer.diagnostics()) result.diagnostics.push_back(diag);
    result.tokens = to_semantic_tokens(meld_tokens);

    // Structural validation: balanced delimiters
    int brace = 0, paren = 0, bracket = 0;
    int last_open_brace_line = 0, last_open_brace_col = 0;
    int last_open_paren_line = 0, last_open_paren_col = 0;
    int last_open_bracket_line = 0, last_open_bracket_col = 0;
    for (const auto& tok : meld_tokens) {
        if (tok.value == "{") { brace++; last_open_brace_line = tok.line; last_open_brace_col = tok.column; }
        else if (tok.value == "}") brace--;
        else if (tok.value == "(") { paren++; last_open_paren_line = tok.line; last_open_paren_col = tok.column; }
        else if (tok.value == ")") paren--;
        else if (tok.value == "[") { bracket++; last_open_bracket_line = tok.line; last_open_bracket_col = tok.column; }
        else if (tok.value == "]") bracket--;
    }
    if (brace != 0) {
        result.errors.push_back("Unbalanced braces");
        result.diagnostics.push_back({last_open_brace_line, last_open_brace_col,
                                      last_open_brace_line, last_open_brace_col + 1,
                                      "Unbalanced braces", "meld"});
    }
    if (paren != 0) {
        result.errors.push_back("Unbalanced parentheses");
        result.diagnostics.push_back({last_open_paren_line, last_open_paren_col,
                                      last_open_paren_line, last_open_paren_col + 1,
                                      "Unbalanced parentheses", "meld"});
    }
    if (bracket != 0) {
        result.errors.push_back("Unbalanced brackets");
        result.diagnostics.push_back({last_open_bracket_line, last_open_bracket_col,
                                      last_open_bracket_line, last_open_bracket_col + 1,
                                      "Unbalanced brackets", "meld"});
    }

    result.success = result.errors.empty();
    result.has_partial_results = !result.tokens.empty() && !result.success;
    return result;
}

ParseResult SemanticTokenProvider::parse_document(
    const std::string& uri, const std::string& content, int version) {
    auto result = parse(content);
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        cache_[uri] = {uri, content, result,
                       std::chrono::steady_clock::now(), version};
    }
    return result;
}

std::optional<ParseResult> SemanticTokenProvider::get_cached(
    const std::string& uri) const {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    auto it = cache_.find(uri);
    if (it != cache_.end()) return it->second.result;
    return std::nullopt;
}

std::optional<std::string> SemanticTokenProvider::get_content(
    const std::string& uri) const {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    auto it = cache_.find(uri);
    if (it != cache_.end()) return it->second.content;
    return std::nullopt;
}

void SemanticTokenProvider::invalidate(const std::string& uri) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    cache_.erase(uri);
}

void SemanticTokenProvider::clear_cache() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    cache_.clear();
}

}  // namespace meld::daemon
