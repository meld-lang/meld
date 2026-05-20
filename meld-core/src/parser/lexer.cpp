#include "meld/parser/parser.hpp"
#include <cctype>
#include <sstream>

namespace meld::parser {

// Token type definitions
const std::unordered_set<std::string> Lexer::keywords_ = {
    "val", "var", "fnc", "class", "struct", "trait", "interface",
    "enum", "return", "rtn", "effect", "handle",
    "true", "false", "nil", "imp", "export",
    "namespace", "operator",
    "typealias", "newtype", "mixin", "by", "out",
    "where", "is", "typeof", "dynamic", "lazy", "override",
    "infix", "prefix", "postfix", "precedence", "associativity", "left", "right", "none",
    "effects", "query", "select",
    "join", "on", "group", "into", "test", "assert", "forall", "flow", "state", "goto",
    "when", "entry", "exit", "initial", "match", "case", "old"
};

// Banned keywords that produce compile-time errors directing users to the correct Meld syntax
const std::unordered_set<std::string> Lexer::banned_keywords_ = {
    "import", "from", "as"
};

// Deprecated keywords that should trigger warnings
const std::unordered_set<std::string> Lexer::deprecated_keywords_ = {
    "effect", "imposes", "perform", "with", "resume"
};

Lexer::Lexer(const std::string& source, bool strict_no_keywords) 
    : source_(source), position_(0), line_(1), column_(1), strict_no_keywords_(strict_no_keywords) {}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    
    while (!is_at_end()) {
        skip_whitespace_and_comments();
        if (is_at_end()) break;
        
        Token token = next_token();
        if (token.type != TokenType::INVALID) {
            tokens.push_back(token);
        }
    }
    
    tokens.push_back(Token{TokenType::END_OF_FILE, "", line_, column_});
    return tokens;
}

Token Lexer::next_token() {
    size_t start_line = line_;
    size_t start_column = column_;
    
    char c = peek();
    
    // Multi-line string template (""")
    if (c == '"' && peek_ahead(1) == '"' && peek_ahead(2) == '"') {
        return tokenize_multiline_string();
    }
    
    // String literal ("...")
    if (c == '"') {
        return tokenize_string();
    }
    
    // Multi-line template string (```)
    if (c == '`' && peek_ahead(1) == '`' && peek_ahead(2) == '`') {
        return tokenize_multiline_template_string();
    }
    
    // Template string (`...`)
    if (c == '`') {
        return tokenize_template_string();
    }
    
    // Regular expression literal
    if (c == '/' && !is_division_context()) {
        return tokenize_regex();
    }
    
    // Number literal
    if (std::isdigit(c)) {
        return tokenize_number();
    }
    
    // Identifier or keyword
    if (std::isalpha(c) || c == '_') {
        return tokenize_identifier();
    }
    
    // Operators and punctuation
    return tokenize_operator(start_line, start_column);
}

Token Lexer::tokenize_string() {
    size_t start_line = line_;
    size_t start_column = column_;
    std::string value;
    bool saw_interpolation_syntax = false;
    
    advance(); // consume opening "
    
    while (!is_at_end() && peek() != '"') {
        if (peek() == '\\') {
            advance();
            if (!is_at_end()) {
                char escaped = peek();
                switch (escaped) {
                    case 'n': value += '\n'; break;
                    case 't': value += '\t'; break;
                    case 'r': value += '\r'; break;
                    case '\\': value += '\\'; break;
                    case '"': value += '"'; break;
                    case '$': value += '$'; break;
                    default: value += escaped; break;
                }
                advance();
            }
        } else if (peek() == '$' && peek_ahead(1) == '{') {
            // Static string — treat $ as literal, but warn
            saw_interpolation_syntax = true;
            value += peek();
            advance();
        } else {
            value += peek();
            advance();
        }
    }
    
    if (is_at_end()) {
        errors_.push_back("Unterminated string at line " + std::to_string(start_line));
        return Token{TokenType::INVALID, "", start_line, start_column};
    }
    
    advance(); // consume closing "
    
    if (saw_interpolation_syntax) {
        warnings_.push_back("'${}' found in static string literal at line " +
            std::to_string(start_line) + ":" + std::to_string(start_column) +
            " — did you mean to use a template string (`...`)?");
    }
    
    return Token{TokenType::STRING, value, start_line, start_column};
}

Token Lexer::tokenize_multiline_string() {
    size_t start_line = line_;
    size_t start_column = column_;
    std::string value;
    bool saw_interpolation_syntax = false;
    
    advance(); // first "
    advance(); // second "
    advance(); // third "
    
    while (!is_at_end()) {
        if (peek() == '"' && peek_ahead(1) == '"' && peek_ahead(2) == '"') {
            advance(); // first "
            advance(); // second "
            advance(); // third "
            
            if (saw_interpolation_syntax) {
                warnings_.push_back("'${}' found in static multi-line string at line " +
                    std::to_string(start_line) + ":" + std::to_string(start_column) +
                    " — did you mean to use a template string (```...```)?");
            }
            
            return Token{TokenType::MULTILINE_STRING, value, start_line, start_column};
        }
        
        if (peek() == '\\' && peek_ahead(1) == '$') {
            advance(); // consume backslash
            value += '$';
            advance();
        } else if (peek() == '$' && peek_ahead(1) == '{') {
            saw_interpolation_syntax = true;
            value += peek();
            advance();
        } else {
            value += peek();
            advance();
        }
    }
    
    errors_.push_back("Unterminated multiline string at line " + std::to_string(start_line));
    return Token{TokenType::INVALID, "", start_line, start_column};
}

// ─── Template string: `...` (single-line, with interpolation) ───────

Token Lexer::tokenize_template_string() {
    size_t start_line = line_;
    size_t start_column = column_;
    std::string value;
    
    advance(); // consume opening `
    
    while (!is_at_end() && peek() != '`') {
        if (peek() == '\n') {
            errors_.push_back("Unterminated template string at line " + std::to_string(start_line) +
                " (use ```...``` for multi-line template strings)");
            return Token{TokenType::INVALID, "", start_line, start_column};
        }
        if (peek() == '\\') {
            advance();
            if (!is_at_end()) {
                char escaped = peek();
                switch (escaped) {
                    case 'n': value += '\n'; break;
                    case 't': value += '\t'; break;
                    case 'r': value += '\r'; break;
                    case '\\': value += '\\'; break;
                    case '`': value += '`'; break;
                    case '$': value += '$'; break;
                    default: value += escaped; break;
                }
                advance();
            }
        } else if (peek() == '$' && peek_ahead(1) == '{') {
            // Interpolation: scan ${expr} or ${:modifier expr}
            advance(); // $
            advance(); // {
            
            // Skip whitespace after ${
            while (!is_at_end() && (peek() == ' ' || peek() == '\t')) {
                advance();
            }
            
            if (!is_at_end() && peek() == ':' && peek_ahead(1) != ':' && 
                (std::isalpha(peek_ahead(1)) || peek_ahead(1) == '_')) {
                // Modifier: ${:symbol ...}
                advance(); // consume ':'
                std::string modifier;
                while (!is_at_end() && (std::isalnum(peek()) || peek() == '_' || peek() == '-')) {
                    if (peek() == '-') {
                        char next = peek_ahead(1);
                        if (!std::isalnum(next) && next != '_') break;
                    }
                    modifier += peek();
                    advance();
                }
                value += "${:" + modifier + " ";
                
                int brace_count = 1;
                while (!is_at_end() && brace_count > 0) {
                    if (peek() == '{') brace_count++;
                    else if (peek() == '}') brace_count--;
                    if (brace_count > 0) {
                        value += peek();
                    }
                    advance();
                }
                value += "}";
            } else {
                // Standard interpolation ${expr}
                value += "${";
                int brace_count = 1;
                while (!is_at_end() && brace_count > 0) {
                    if (peek() == '{') brace_count++;
                    else if (peek() == '}') brace_count--;
                    value += peek();
                    advance();
                }
            }
        } else {
            value += peek();
            advance();
        }
    }
    
    if (is_at_end()) {
        errors_.push_back("Unterminated template string at line " + std::to_string(start_line));
        return Token{TokenType::INVALID, "", start_line, start_column};
    }
    
    advance(); // consume closing `
    return Token{TokenType::TEMPLATE_STRING, value, start_line, start_column};
}

// ─── Multi-line template string: ```...``` (with interpolation) ─────

Token Lexer::tokenize_multiline_template_string() {
    size_t start_line = line_;
    size_t start_column = column_;
    std::string value;
    
    advance(); // first `
    advance(); // second `
    advance(); // third `
    
    while (!is_at_end()) {
        if (peek() == '`' && peek_ahead(1) == '`' && peek_ahead(2) == '`') {
            advance(); // first `
            advance(); // second `
            advance(); // third `
            return Token{TokenType::MULTILINE_TEMPLATE_STRING, value, start_line, start_column};
        }
        
        if (peek() == '\\') {
            advance();
            if (!is_at_end()) {
                char escaped = peek();
                switch (escaped) {
                    case 'n': value += '\n'; break;
                    case 't': value += '\t'; break;
                    case '\\': value += '\\'; break;
                    case '`': value += '`'; break;
                    case '$': value += '$'; break;
                    default: value += escaped; break;
                }
                advance();
            }
        } else if (peek() == '$' && peek_ahead(1) == '{') {
            // Interpolation: scan ${expr} or ${:modifier expr}
            advance(); // $
            advance(); // {
            
            while (!is_at_end() && (peek() == ' ' || peek() == '\t')) {
                advance();
            }
            
            if (!is_at_end() && peek() == ':' && peek_ahead(1) != ':' && 
                (std::isalpha(peek_ahead(1)) || peek_ahead(1) == '_')) {
                advance(); // consume ':'
                std::string modifier;
                while (!is_at_end() && (std::isalnum(peek()) || peek() == '_' || peek() == '-')) {
                    if (peek() == '-') {
                        char next = peek_ahead(1);
                        if (!std::isalnum(next) && next != '_') break;
                    }
                    modifier += peek();
                    advance();
                }
                value += "${:" + modifier + " ";
                
                int brace_count = 1;
                while (!is_at_end() && brace_count > 0) {
                    if (peek() == '{') brace_count++;
                    else if (peek() == '}') brace_count--;
                    if (brace_count > 0) {
                        value += peek();
                    }
                    advance();
                }
                value += "}";
            } else {
                value += "${";
                int brace_count = 1;
                while (!is_at_end() && brace_count > 0) {
                    if (peek() == '{') brace_count++;
                    else if (peek() == '}') brace_count--;
                    value += peek();
                    advance();
                }
            }
        } else {
            value += peek();
            advance();
        }
    }
    
    errors_.push_back("Unterminated multiline template string at line " + std::to_string(start_line));
    return Token{TokenType::INVALID, "", start_line, start_column};
}

Token Lexer::tokenize_regex() {
    size_t start_line = line_;
    size_t start_column = column_;
    std::string pattern;
    
    advance(); // consume opening /
    
    while (!is_at_end() && peek() != '/') {
        if (peek() == '\\') {
            pattern += peek();
            advance();
            if (!is_at_end()) {
                pattern += peek();
                advance();
            }
        } else if (peek() == '\n') {
            errors_.push_back("Unterminated regex at line " + std::to_string(start_line));
            return Token{TokenType::INVALID, "", start_line, start_column};
        } else {
            pattern += peek();
            advance();
        }
    }
    
    if (is_at_end()) {
        errors_.push_back("Unterminated regex at line " + std::to_string(start_line));
        return Token{TokenType::INVALID, "", start_line, start_column};
    }
    
    advance(); // consume closing /
    
    // Parse flags
    std::string flags;
    while (!is_at_end() && std::isalpha(peek())) {
        flags += peek();
        advance();
    }
    
    return Token{TokenType::REGEX, pattern + "/" + flags, start_line, start_column};
}

Token Lexer::tokenize_number() {
    size_t start_line = line_;
    size_t start_column = column_;
    std::string value;
    
    while (!is_at_end() && std::isdigit(peek())) {
        value += peek();
        advance();
    }
    
    // Check for decimal point
    if (!is_at_end() && peek() == '.' && std::isdigit(peek_ahead(1))) {
        value += peek();
        advance();
        while (!is_at_end() && std::isdigit(peek())) {
            value += peek();
            advance();
        }
    }
    
    // Check for numeric suffix (L, F, D)
    if (!is_at_end() && (peek() == 'L' || peek() == 'F' || peek() == 'D')) {
        value += peek();
        advance();
    }
    
    return Token{TokenType::NUMBER, value, start_line, start_column};
}

Token Lexer::tokenize_identifier() {
    size_t start_line = line_;
    size_t start_column = column_;
    std::string value;
    
    // First character must be alpha or underscore (not hyphen)
    while (!is_at_end() && (std::isalnum(peek()) || peek() == '_' || peek() == '-')) {
        // If we encounter a hyphen, check if it's valid
        if (peek() == '-') {
            // Hyphen is only valid if:
            // 1. It's not the first character (value is not empty)
            // 2. The next character is alphanumeric or underscore (not another hyphen or end)
            if (value.empty()) {
                // Hyphen at start - not part of identifier
                break;
            }
            
            char next = peek_ahead(1);
            if (!std::isalnum(next) && next != '_') {
                // Hyphen at end or followed by non-identifier character
                break;
            }
        }
        
        value += peek();
        advance();
    }
    
    // Remove trailing hyphens if any (shouldn't happen with above logic, but safety check)
    while (!value.empty() && value.back() == '-') {
        value.pop_back();
        // Move position back
        position_--;
        column_--;
    }
    
    // Check if it's a keyword
    TokenType type = keywords_.count(value) ? TokenType::KEYWORD : TokenType::IDENTIFIER;
    
    // Check if it's a banned keyword and produce compile-time error
    if (banned_keywords_.count(value)) {
        std::string suggestion;
        if (value == "import") {
            suggestion = "Use 'imp' instead. Meld uses 'imp' as the only keyword for imports. Example: imp std.math";
        } else if (value == "from") {
            suggestion = "The 'from' keyword is not part of Meld's grammar. Use 'imp' for imports. Example: imp { sin, cos } = std.math";
        } else if (value == "as") {
            suggestion = "The 'as' keyword is not part of Meld's grammar. Use '=' for module aliasing. Example: imp m = std.math";
        }
        
        std::string error = "Error: '" + value + "' is not a valid Meld keyword at line " +
                          std::to_string(start_line) + ":" + std::to_string(start_column) + ". " + suggestion;
        errors_.push_back(error);
    }
    
    // Check if it's a deprecated keyword and add warning or error
    if (deprecated_keywords_.count(value)) {
        std::string suggestion;
        if (value == "effect") {
            suggestion = "Use '@effect' annotation instead. Example: @effect trait EffectName { ... }";
        } else if (value == "imposes") {
            suggestion = "Use '@imposes(...)' annotation instead. Example: @imposes(EffectType1, EffectType2)";
        } else if (value == "perform") {
            suggestion = "Use 'perform { ... }' function instead. Example: perform { EffectType.operation(args) }";
        } else if (value == "with") {
            suggestion = "Use handler configuration block in 'handle()' function instead";
        } else if (value == "resume") {
            suggestion = "Use 'resume()' or 'resume(value)' function instead";
        }
        
        if (strict_no_keywords_) {
            std::string error = "Error: Keyword-based effect syntax is not allowed in strict mode. '" + value + "' at line " +
                              std::to_string(start_line) + ":" + std::to_string(start_column) + ". " + suggestion;
            errors_.push_back(error);
        } else {
            std::string warning = "Deprecated keyword '" + value + "' at line " + 
                                std::to_string(start_line) + ":" + std::to_string(start_column) + ". " + suggestion;
            warnings_.push_back(warning);
        }
    }
    
    return Token{type, value, start_line, start_column};
}

Token Lexer::tokenize_operator(size_t start_line, size_t start_column) {
    char c = peek();
    std::string op;
    op += c;
    advance();
    
    // Handle @ for decorators
    if (c == '@') {
        // @ followed by identifier is a decorator
        if (!is_at_end() && (std::isalpha(peek()) || peek() == '_')) {
            std::string decorator_name;
            while (!is_at_end() && (std::isalnum(peek()) || peek() == '_')) {
                decorator_name += peek();
                advance();
            }
            return Token{TokenType::OPERATOR, "@" + decorator_name, start_line, start_column};
        }
        return Token{TokenType::OPERATOR, "@", start_line, start_column};
    }
    
    // Two-character operators
    if (!is_at_end()) {
        char next = peek();
        std::string two_char = op + next;
        
        // Check for two-character operators
        if (two_char == "==" || two_char == "!=" || two_char == "<=" || 
            two_char == ">=" || two_char == "&&" || two_char == "||" ||
            two_char == "+=" || two_char == "-=" || two_char == "*=" ||
            two_char == "/=" || two_char == "?." || two_char == "??" ||
            two_char == "?:" || two_char == "|>" ||
            two_char == "::" || two_char == ".." || two_char == "->") {
            op = two_char;
            advance();
        }
        
        // Three-character operators
        if (!is_at_end() && op == "..") {
            if (peek() == '.') {
                op += peek();
                advance();
            }
        }
    }
    
    return Token{TokenType::OPERATOR, op, start_line, start_column};
}

void Lexer::skip_whitespace_and_comments() {
    while (!is_at_end()) {
        char c = peek();
        
        if (c == ' ' || c == '\t' || c == '\r') {
            advance();
        } else if (c == '\n') {
            advance();
        } else if (c == '/' && peek_ahead(1) == '/') {
            // Single-line comment (//)
            while (!is_at_end() && peek() != '\n') {
                advance();
            }
        } else if (c == '/' && peek_ahead(1) == '*') {
            // Multi-line comment
            advance(); // /
            advance(); // *
            while (!is_at_end()) {
                if (peek() == '*' && peek_ahead(1) == '/') {
                    advance(); // *
                    advance(); // /
                    break;
                }
                advance();
            }
        } else {
            break;
        }
    }
}

char Lexer::peek() const {
    if (is_at_end()) return '\0';
    return source_[position_];
}

char Lexer::peek_ahead(size_t offset) const {
    if (position_ + offset >= source_.length()) return '\0';
    return source_[position_ + offset];
}

char Lexer::advance() {
    if (is_at_end()) return '\0';
    
    char c = source_[position_++];
    
    if (c == '\n') {
        line_++;
        column_ = 1;
    } else {
        column_++;
    }
    
    return c;
}

bool Lexer::is_at_end() const {
    return position_ >= source_.length();
}

bool Lexer::is_division_context() const {
    // Simple heuristic: if the last non-whitespace token was a value-like token,
    // then / is likely division, not regex
    // This is a simplified version - a full implementation would track token history
    if (position_ == 0) return false;
    
    size_t pos = position_ - 1;
    while (pos > 0 && std::isspace(source_[pos])) {
        pos--;
    }
    
    if (pos == 0) return false;
    
    char prev = source_[pos];
    return std::isalnum(prev) || prev == ')' || prev == ']' || prev == '}';
}

const std::vector<std::string>& Lexer::errors() const {
    return errors_;
}

const std::vector<std::string>& Lexer::warnings() const {
    return warnings_;
}

} // namespace meld::parser
