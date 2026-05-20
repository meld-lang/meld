#pragma once

#include "ast.hpp"
#include "parser_context.hpp"
#include <boost/spirit/home/x3.hpp>
#include <string>
#include <vector>
#include <unordered_set>

namespace meld::parser {

namespace x3 = boost::spirit::x3;

// Token types
enum class TokenType {
    // Literals
    NUMBER,
    STRING,
    MULTILINE_STRING,
    TEMPLATE_STRING,
    MULTILINE_TEMPLATE_STRING,
    REGEX,
    
    // Identifiers and keywords
    IDENTIFIER,
    KEYWORD,
    
    // Operators and punctuation
    OPERATOR,
    
    // Special
    END_OF_FILE,
    INVALID
};

// Token structure with position information
struct Token {
    TokenType type;
    std::string value;
    size_t line;
    size_t column;
    
    std::string to_string() const {
        return "Token(" + value + " at " + std::to_string(line) + ":" + std::to_string(column) + ")";
    }
};

// Lexer class
class Lexer {
public:
    explicit Lexer(const std::string& source, bool strict_no_keywords = false);
    
    std::vector<Token> tokenize();
    const std::vector<std::string>& errors() const;
    const std::vector<std::string>& warnings() const;
    
private:
    Token next_token();
    Token tokenize_string();
    Token tokenize_multiline_string();
    Token tokenize_template_string();
    Token tokenize_multiline_template_string();
    Token tokenize_regex();
    Token tokenize_number();
    Token tokenize_identifier();
    Token tokenize_operator(size_t start_line, size_t start_column);
    
    void skip_whitespace_and_comments();
    char peek() const;
    char peek_ahead(size_t offset) const;
    char advance();
    bool is_at_end() const;
    bool is_division_context() const;
    
    std::string source_;
    size_t position_;
    size_t line_;
    size_t column_;
    std::vector<std::string> errors_;
    std::vector<std::string> warnings_;
    bool strict_no_keywords_;
    
    static const std::unordered_set<std::string> keywords_;
    static const std::unordered_set<std::string> deprecated_keywords_;
    static const std::unordered_set<std::string> banned_keywords_;
};

// Parser interface
class Parser {
public:
    bool parse_expression(const std::string& input, ast::expression& result);
    bool parse_file(const std::string& input, std::vector<ast::expression>& result);
    
    const std::string& error_message() const { return error_message_; }
    const std::vector<std::string>& warnings() const { return warnings_; }
    const std::vector<std::string>& errors() const { return errors_; }
    
    // Set the parser context for effect name resolution
    void set_context(const ParserContext& ctx) { context_ = ctx; }
    const ParserContext& context() const { return context_; }
    
private:
    std::string error_message_;
    std::vector<std::string> warnings_;
    std::vector<std::string> errors_;
    ParserContext context_;
};

} // namespace meld::parser
