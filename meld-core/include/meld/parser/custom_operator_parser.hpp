#pragma once

#include "meld/kernel/operators.hpp"
#include "ast.hpp"
#include <string>
#include <map>
#include <expected>

namespace meld::parser {

// Custom operator precedence table
class CustomOperatorPrecedenceTable {
public:
    static CustomOperatorPrecedenceTable& instance() {
        static CustomOperatorPrecedenceTable table;
        return table;
    }
    
    // Register a custom infix operator with precedence and associativity
    void register_infix_operator(
        const std::string& symbol,
        int precedence,
        kernel::Associativity associativity
    );
    
    // Register a custom prefix operator with precedence
    void register_prefix_operator(
        const std::string& symbol,
        int precedence
    );
    
    // Register a custom postfix operator with precedence
    void register_postfix_operator(
        const std::string& symbol,
        int precedence
    );
    
    // Get precedence for an operator
    std::expected<int, std::string> get_precedence(const std::string& symbol) const;
    
    // Get associativity for an infix operator
    std::expected<kernel::Associativity, std::string> get_associativity(const std::string& symbol) const;
    
    // Check if operator is registered
    bool has_operator(const std::string& symbol) const;
    
    // Get operator type
    std::expected<kernel::OperatorType, std::string> get_operator_type(const std::string& symbol) const;
    
    // Update parser with custom operators from annotation registry
    void sync_with_annotation_registry();
    
private:
    CustomOperatorPrecedenceTable() = default;
    
    struct OperatorEntry {
        kernel::OperatorType type;
        int precedence;
        kernel::Associativity associativity;
        
        OperatorEntry() : type(kernel::OperatorType::INFIX), precedence(0), associativity(kernel::Associativity::NONE) {}
        OperatorEntry(kernel::OperatorType t, int prec, kernel::Associativity assoc = kernel::Associativity::NONE)
            : type(t), precedence(prec), associativity(assoc) {}
    };
    
    std::map<std::string, OperatorEntry> operators_;
};

// Parser extension for handling custom operators
class CustomOperatorParser {
public:
    // Parse infix expression with custom operator precedence
    static std::expected<ast::expression, std::string>
    parse_infix_expression(
        const std::string& left_operand,
        const std::string& operator_symbol,
        const std::string& right_operand,
        int min_precedence = 0
    );
    
    // Parse prefix expression with custom operator
    static std::expected<ast::expression, std::string>
    parse_prefix_expression(
        const std::string& operator_symbol,
        const std::string& operand
    );
    
    // Parse postfix expression with custom operator
    static std::expected<ast::expression, std::string>
    parse_postfix_expression(
        const std::string& operand,
        const std::string& operator_symbol
    );
    
    // Check if token is a custom operator
    static bool is_custom_operator(const std::string& token);
    
    // Get operator precedence (including custom operators)
    static int get_operator_precedence(const std::string& operator_symbol);
    
    // Get operator associativity (including custom operators)
    static kernel::Associativity get_operator_associativity(const std::string& operator_symbol);
    
private:
    // Built-in operator precedence table
    static const std::map<std::string, int> builtin_precedence_;
    
    // Built-in operator associativity table
    static const std::map<std::string, kernel::Associativity> builtin_associativity_;
};

// Integration with the main parser
class ParserIntegration {
public:
    // Initialize custom operator support in the parser
    static void initialize_custom_operators();
    
    // Update parser precedence tables with custom operators
    static void update_parser_precedence_tables();
    
    // Process @infix, @prefix, @postfix annotations during parsing
    static std::expected<ast::expression, std::string>
    process_operator_annotation(const ast::expression& expr);
    
private:
    static bool initialized_;
};

} // namespace meld::parser