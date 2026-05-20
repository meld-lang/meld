#include "meld_parser_adapter.hpp"

#include <algorithm>
#include <cctype>

namespace meld::lsp::integration {

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

} // anonymous namespace

// ---------------------------------------------------------------------------
// Free functions
// ---------------------------------------------------------------------------

bool is_meld_keyword(const std::string& text) {
    return KEYWORDS.count(text) > 0;
}

SemanticTokenType classify_construct(const std::string& text) {
    if (KEYWORDS.count(text)) {
        return SemanticTokenType::Keyword;
    }
    if (text.size() > 1 && text[0] == '@') {
        return SemanticTokenType::Decorator;
    }
    if (!text.empty() &&
        std::isupper(static_cast<unsigned char>(text[0]))) {
        return SemanticTokenType::Type;
    }
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
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
        } else {
            break;
        }
    }
}

MeldToken MeldLspTokenizer::read_line_comment() {
    int start_line = line_;
    int start_col = col_;
    std::string value;
    // consume //
    value += advance();
    value += advance();
    while (!at_end() && peek() != '\n') {
        value += advance();
    }
    return {MeldTokenType::Comment, value, start_line, start_col};
}

MeldToken MeldLspTokenizer::read_block_comment() {
    int start_line = line_;
    int start_col = col_;
    std::string value;
    value += advance(); // /
    value += advance(); // *
    while (!at_end()) {
        if (peek() == '*' && peek_ahead(1) == '/') {
            value += advance();
            value += advance();
            return {MeldTokenType::Comment, value, start_line, start_col};
        }
        value += advance();
    }
    errors_.push_back("Unterminated block comment at line " +
                       std::to_string(start_line + 1));
    return {MeldTokenType::Comment, value, start_line, start_col};
}

MeldToken MeldLspTokenizer::read_string() {
    int start_line = line_;
    int start_col = col_;
    std::string value;

    // Check for triple-quoted multiline string
    if (peek_ahead(1) == '"' && peek_ahead(2) == '"') {
        return read_multiline_string();
    }

    value += advance(); // opening "
    while (!at_end() && peek() != '"' && peek() != '\n') {
        if (peek() == '\\' && !at_end()) {
            value += advance(); // backslash
            if (!at_end()) value += advance(); // escaped char
        } else {
            value += advance();
        }
    }
    if (!at_end() && peek() == '"') {
        value += advance(); // closing "
    } else {
        errors_.push_back("Unterminated string literal at line " +
                           std::to_string(start_line + 1));
    }
    return {MeldTokenType::String, value, start_line, start_col};
}

MeldToken MeldLspTokenizer::read_multiline_string() {
    int start_line = line_;
    int start_col = col_;
    std::string value;
    value += advance(); // "
    value += advance(); // "
    value += advance(); // "
    while (!at_end()) {
        if (peek() == '"' && peek_ahead(1) == '"' && peek_ahead(2) == '"') {
            value += advance();
            value += advance();
            value += advance();
            return {MeldTokenType::String, value, start_line, start_col};
        }
        value += advance();
    }
    errors_.push_back("Unterminated multiline string at line " +
                       std::to_string(start_line + 1));
    return {MeldTokenType::String, value, start_line, start_col};
}

MeldToken MeldLspTokenizer::read_number() {
    int start_line = line_;
    int start_col = col_;
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
            advance(); // skip numeric separator
        } else {
            break;
        }
    }
    // Optional suffix (L, F, D)
    if (!at_end()) {
        char c = peek();
        if (c == 'L' || c == 'F' || c == 'D' ||
            c == 'l' || c == 'f' || c == 'd') {
            value += advance();
        }
    }
    return {MeldTokenType::Number, value, start_line, start_col};
}

MeldToken MeldLspTokenizer::read_identifier_or_keyword() {
    int start_line = line_;
    int start_col = col_;
    std::string value;
    while (!at_end()) {
        char c = peek();
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
            value += advance();
        } else {
            break;
        }
    }
    if (KEYWORDS.count(value)) {
        return {MeldTokenType::Keyword, value, start_line, start_col};
    }
    if (!value.empty() &&
        std::isupper(static_cast<unsigned char>(value[0]))) {
        return {MeldTokenType::TypeName, value, start_line, start_col};
    }
    return {MeldTokenType::Identifier, value, start_line, start_col};
}

MeldToken MeldLspTokenizer::read_operator() {
    int start_line = line_;
    int start_col = col_;
    std::string value;
    char c = peek();

    // Multi-character operators
    if ((c == '=' || c == '!' || c == '<' || c == '>') &&
        peek_ahead(1) == '=') {
        value += advance();
        value += advance();
    } else if (c == '|' && peek_ahead(1) == '>') {
        value += advance();
        value += advance();
    } else if (c == '-' && peek_ahead(1) == '>') {
        value += advance();
        value += advance();
    } else if (c == '=' && peek_ahead(1) == '>') {
        value += advance();
        value += advance();
    } else if (c == '&' && peek_ahead(1) == '&') {
        value += advance();
        value += advance();
    } else if (c == '|' && peek_ahead(1) == '|') {
        value += advance();
        value += advance();
    } else if (c == '?' && peek_ahead(1) == '?') {
        value += advance();
        value += advance();
    } else if (c == '?' && peek_ahead(1) == '.') {
        value += advance();
        value += advance();
    } else if (c == '.' && peek_ahead(1) == '.' && peek_ahead(2) == '.') {
        value += advance();
        value += advance();
        value += advance();
    } else {
        value += advance();
    }
    return {MeldTokenType::Operator, value, start_line, start_col};
}

MeldToken MeldLspTokenizer::read_decorator() {
    int start_line = line_;
    int start_col = col_;
    std::string value;
    value += advance(); // @
    while (!at_end()) {
        char c = peek();
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
            value += advance();
        } else {
            break;
        }
    }
    return {MeldTokenType::Decorator, value, start_line, start_col};
}

std::vector<MeldToken> MeldLspTokenizer::tokenize(const std::string& source) {
    source_ = source;
    pos_ = 0;
    line_ = 0;
    col_ = 0;
    errors_.clear();

    std::vector<MeldToken> tokens;

    while (!at_end()) {
        skip_whitespace();
        if (at_end()) break;

        char c = peek();

        // Line comment
        if (c == '/' && peek_ahead(1) == '/') {
            tokens.push_back(read_line_comment());
            continue;
        }

        // Block comment
        if (c == '/' && peek_ahead(1) == '*') {
            tokens.push_back(read_block_comment());
            continue;
        }

        // String literal
        if (c == '"') {
            tokens.push_back(read_string());
            continue;
        }

        // Number literal
        if (std::isdigit(static_cast<unsigned char>(c))) {
            tokens.push_back(read_number());
            continue;
        }

        // Decorator (@effect, @uses, etc.)
        if (c == '@') {
            tokens.push_back(read_decorator());
            continue;
        }

        // Identifier or keyword
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            tokens.push_back(read_identifier_or_keyword());
            continue;
        }

        // Punctuation (braces, parens, brackets, commas, semicolons, dots)
        if (c == '(' || c == ')' || c == '{' || c == '}' ||
            c == '[' || c == ']' || c == ',' || c == ';' || c == ':') {
            int sl = line_, sc = col_;
            std::string v(1, advance());
            tokens.push_back({MeldTokenType::Punctuation, v, sl, sc});
            continue;
        }

        // Dot — could be member access or start of range operator
        if (c == '.') {
            if (peek_ahead(1) == '.' && peek_ahead(2) == '.') {
                tokens.push_back(read_operator());
            } else {
                int sl = line_, sc = col_;
                std::string v(1, advance());
                tokens.push_back({MeldTokenType::Punctuation, v, sl, sc});
            }
            continue;
        }

        // Operators
        if (c == '+' || c == '-' || c == '*' || c == '/' ||
            c == '=' || c == '<' || c == '>' || c == '!' ||
            c == '&' || c == '|' || c == '^' || c == '~' ||
            c == '%' || c == '?') {
            tokens.push_back(read_operator());
            continue;
        }

        // Unknown character — skip and record error
        {
            int sl = line_, sc = col_;
            std::string v(1, advance());
            errors_.push_back("Unexpected character '" + v + "' at line " +
                               std::to_string(sl + 1) + " col " +
                               std::to_string(sc + 1));
            tokens.push_back({MeldTokenType::Invalid, v, sl, sc});
        }
    }

    return tokens;
}

// ---------------------------------------------------------------------------
// MeldParserAdapter
// ---------------------------------------------------------------------------

MeldParserAdapter::MeldParserAdapter() = default;
MeldParserAdapter::~MeldParserAdapter() = default;

SemanticTokenType MeldParserAdapter::classify_token(const MeldToken& token) {
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

std::vector<SemanticToken> MeldParserAdapter::to_semantic_tokens(
    const std::vector<MeldToken>& tokens) {

    std::vector<SemanticToken> result;
    result.reserve(tokens.size());

    for (const auto& tok : tokens) {
        // Skip punctuation and invalid tokens — they don't get semantic highlighting
        if (tok.type == MeldTokenType::Punctuation ||
            tok.type == MeldTokenType::EndOfFile ||
            tok.type == MeldTokenType::Invalid) {
            continue;
        }
        if (tok.value.empty()) continue;

        SemanticToken st;
        st.line = tok.line;
        st.start_char = tok.column;
        st.length = static_cast<int>(tok.value.size());
        st.type = classify_token(tok);
        st.modifiers = 0;

        result.push_back(st);
    }

    return result;
}

ParseResult MeldParserAdapter::parse(const std::string& source) {
    ParseResult result;

    if (source.empty()) {
        result.success = true;
        return result;
    }

    MeldLspTokenizer tokenizer;
    auto meld_tokens = tokenizer.tokenize(source);

    // Collect tokenizer errors
    for (const auto& err : tokenizer.errors()) {
        result.errors.push_back(err);
    }

    // Convert to semantic tokens
    result.tokens = to_semantic_tokens(meld_tokens);

    // Basic structural validation: check balanced braces
    int brace_depth = 0;
    int paren_depth = 0;
    int bracket_depth = 0;
    for (const auto& tok : meld_tokens) {
        if (tok.value == "{") brace_depth++;
        else if (tok.value == "}") brace_depth--;
        else if (tok.value == "(") paren_depth++;
        else if (tok.value == ")") paren_depth--;
        else if (tok.value == "[") bracket_depth++;
        else if (tok.value == "]") bracket_depth--;
    }

    if (brace_depth != 0) {
        result.errors.push_back("Unbalanced braces");
    }
    if (paren_depth != 0) {
        result.errors.push_back("Unbalanced parentheses");
    }
    if (bracket_depth != 0) {
        result.errors.push_back("Unbalanced brackets");
    }

    result.success = result.errors.empty();
    result.has_partial_results = !result.tokens.empty() && !result.success;

    return result;
}

ParseResult MeldParserAdapter::parse_document(
    const std::string& uri, const std::string& content, int version) {

    auto result = parse(content);

    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        CachedParse entry;
        entry.uri = uri;
        entry.content = content;
        entry.result = result;
        entry.timestamp = std::chrono::steady_clock::now();
        entry.version = version;
        cache_[uri] = std::move(entry);
    }

    return result;
}

std::optional<ParseResult> MeldParserAdapter::get_cached(
    const std::string& uri) const {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    auto it = cache_.find(uri);
    if (it != cache_.end()) {
        return it->second.result;
    }
    return std::nullopt;
}

void MeldParserAdapter::invalidate(const std::string& uri) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    cache_.erase(uri);
}

void MeldParserAdapter::clear_cache() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    cache_.clear();
}

} // namespace meld::lsp::integration
