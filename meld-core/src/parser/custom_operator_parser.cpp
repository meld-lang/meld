#include "meld/parser/custom_operator_parser.hpp"
#include <format>

namespace meld::parser {

// CustomOperatorPrecedenceTable implementation

void CustomOperatorPrecedenceTable::register_infix_operator(
    const std::string& symbol,
    int precedence,
    kernel::Associativity associativity
) {
    operators_[symbol] = OperatorEntry(kernel::OperatorType::INFIX, precedence, associativity);
}

void CustomOperatorPrecedenceTable::register_prefix_operator(
    const std::string& symbol,
    int precedence
) {
    operators_[symbol] = OperatorEntry(kernel::OperatorType::PREFIX, precedence);
}

void CustomOperatorPrecedenceTable::register_postfix_operator(
    const std::string& symbol,
    int precedence
) {
    operators_[symbol] = OperatorEntry(kernel::OperatorType::POSTFIX, precedence);
}

std::expected<int, std::string> CustomOperatorPrecedenceTable::get_precedence(const std::string& symbol) const {
    if (auto it = operators_.find(symbol); it != operators_.end()) {
        return it->second.precedence;
    }
    return std::unexpected(std::format("Operator not found: {}", symbol));
}

std::expected<kernel::Associativity, std::string> CustomOperatorPrecedenceTable::get_associativity(const std::string& symbol) const {
    if (auto it = operators_.find(symbol); it != operators_.end()) {
        if (it->second.type == kernel::OperatorType::INFIX) {
            return it->second.associativity;
        }
        return std::unexpected(std::format("Operator {} is not infix", symbol));
    }
    return std::unexpected(std::format("Operator not found: {}", symbol));
}

bool CustomOperatorPrecedenceTable::has_operator(const std::string& symbol) const {
    return operators_.contains(symbol);
}

std::expected<kernel::OperatorType, std::string> CustomOperatorPrecedenceTable::get_operator_type(const std::string& symbol) const {
    if (auto it = operators_.find(symbol); it != operators_.end()) {
        return it->second.type;
    }
    return std::unexpected(std::format("Operator not found: {}", symbol));
}

void CustomOperatorPrecedenceTable::sync_with_annotation_registry() {
    // TODO: Integrate with macro::OperatorAnnotationRegistry once circular
    // dependency between :parser and :macro is resolved.
    // For now, this is a placeholder implementation.
}

// CustomOperatorParser implementation

const std::map<std::string, int> CustomOperatorParser::builtin_precedence_ = {
    // Arithmetic operators
    {"+", 10}, {"-", 10},
    {"*", 20}, {"/", 20}, {"%", 20},
    
    // Comparison operators
    {"==", 5}, {"!=", 5},
    {"<", 5}, {"<=", 5}, {">", 5}, {">=", 5},
    
    // Logical operators
    {"&&", 3}, {"||", 2},
    {"!", 30},
    
    // Bitwise operators
    {"&", 8}, {"|", 6}, {"^", 7},
    {"<<", 15}, {">>", 15}, {">>>", 15},
    
    // Assignment operators
    {"=", 1},
    {"+=", 1}, {"-=", 1}, {"*=", 1}, {"/=", 1}, {"%=", 1},
    
    // Other operators
    {"?:", 1}, {"?.", 25},
    {"++", 30}, {"--", 30},
    {"...", 35}  // Ellipsis operator (highest precedence for rest/spread)
};

const std::map<std::string, kernel::Associativity> CustomOperatorParser::builtin_associativity_ = {
    // Arithmetic operators (left associative)
    {"+", kernel::Associativity::LEFT}, {"-", kernel::Associativity::LEFT},
    {"*", kernel::Associativity::LEFT}, {"/", kernel::Associativity::LEFT}, {"%", kernel::Associativity::LEFT},
    
    // Comparison operators (left associative)
    {"==", kernel::Associativity::LEFT}, {"!=", kernel::Associativity::LEFT},
    {"<", kernel::Associativity::LEFT}, {"<=", kernel::Associativity::LEFT},
    {">", kernel::Associativity::LEFT}, {">=", kernel::Associativity::LEFT},
    
    // Logical operators
    {"&&", kernel::Associativity::LEFT}, {"||", kernel::Associativity::LEFT},
    
    // Bitwise operators
    {"&", kernel::Associativity::LEFT}, {"|", kernel::Associativity::LEFT}, {"^", kernel::Associativity::LEFT},
    {"<<", kernel::Associativity::LEFT}, {">>", kernel::Associativity::LEFT}, {">>>", kernel::Associativity::LEFT},
    
    // Assignment operators (right associative)
    {"=", kernel::Associativity::RIGHT},
    {"+=", kernel::Associativity::RIGHT}, {"-=", kernel::Associativity::RIGHT},
    {"*=", kernel::Associativity::RIGHT}, {"/=", kernel::Associativity::RIGHT}, {"%=", kernel::Associativity::RIGHT},
    
    // Other operators
    {"?:", kernel::Associativity::RIGHT},
    {"...", kernel::Associativity::RIGHT}  // Ellipsis operator (right associative for prefix usage)
};

std::expected<ast::expression, std::string>
CustomOperatorParser::parse_infix_expression(
    const std::string& left_operand,
    const std::string& operator_symbol,
    const std::string& right_operand,
    int min_precedence
) {
    int precedence = get_operator_precedence(operator_symbol);
    
    if (precedence < min_precedence) {
        return std::unexpected(std::format("Operator {} has insufficient precedence", operator_symbol));
    }
    
    // Create binary operation AST node
    ast::binary_operation binary_op;
    // TODO: Parse left and right operands into proper AST nodes
    // For now, this is a placeholder
    
    return ast::expression(binary_op);
}

std::expected<ast::expression, std::string>
CustomOperatorParser::parse_prefix_expression(
    const std::string& operator_symbol,
    const std::string& operand
) {
    if (!is_custom_operator(operator_symbol)) {
        return std::unexpected(std::format("Unknown prefix operator: {}", operator_symbol));
    }
    
    // Create unary expression AST node for prefix operator
    ast::unary_operation unary_expr;
    unary_expr.op = operator_symbol;
    // TODO: Parse operand into proper AST node
    // For now, create a simple identifier
    ast::identifier operand_id;
    operand_id.name = operand;
    unary_expr.operand = ast::expression(operand_id);
    
    return ast::expression(unary_expr);
}

std::expected<ast::expression, std::string>
CustomOperatorParser::parse_postfix_expression(
    const std::string& operand,
    const std::string& operator_symbol
) {
    if (!is_custom_operator(operator_symbol)) {
        return std::unexpected(std::format("Unknown postfix operator: {}", operator_symbol));
    }
    
    // Create unary expression AST node for postfix operator
    ast::unary_operation unary_expr;
    unary_expr.op = operator_symbol;
    // TODO: Parse operand into proper AST node
    // For now, create a simple identifier
    ast::identifier operand_id;
    operand_id.name = operand;
    unary_expr.operand = ast::expression(operand_id);
    
    return ast::expression(unary_expr);
}

bool CustomOperatorParser::is_custom_operator(const std::string& token) {
    auto& custom_table = CustomOperatorPrecedenceTable::instance();
    return custom_table.has_operator(token) || builtin_precedence_.contains(token);
}

int CustomOperatorParser::get_operator_precedence(const std::string& operator_symbol) {
    // Check custom operators first
    auto& custom_table = CustomOperatorPrecedenceTable::instance();
    if (auto precedence = custom_table.get_precedence(operator_symbol); precedence.has_value()) {
        return *precedence;
    }
    
    // Fall back to built-in operators
    if (auto it = builtin_precedence_.find(operator_symbol); it != builtin_precedence_.end()) {
        return it->second;
    }
    
    // Default precedence for unknown operators
    return 5;
}

kernel::Associativity CustomOperatorParser::get_operator_associativity(const std::string& operator_symbol) {
    // Check custom operators first
    auto& custom_table = CustomOperatorPrecedenceTable::instance();
    if (auto associativity = custom_table.get_associativity(operator_symbol); associativity.has_value()) {
        return *associativity;
    }
    
    // Fall back to built-in operators
    if (auto it = builtin_associativity_.find(operator_symbol); it != builtin_associativity_.end()) {
        return it->second;
    }
    
    // Default associativity for unknown operators
    return kernel::Associativity::LEFT;
}

// ParserIntegration implementation

bool ParserIntegration::initialized_ = false;

void ParserIntegration::initialize_custom_operators() {
    if (initialized_) {
        return;
    }
    
    // Initialize the custom operator precedence table
    auto& precedence_table = CustomOperatorPrecedenceTable::instance();
    
    // Sync with annotation registry
    precedence_table.sync_with_annotation_registry();
    
    // Update parser precedence tables
    update_parser_precedence_tables();
    
    initialized_ = true;
}

void ParserIntegration::update_parser_precedence_tables() {
    // This would update the main parser's precedence tables with custom operators
    // For now, this is a placeholder implementation
    
    auto& precedence_table = CustomOperatorPrecedenceTable::instance();
    // TODO: Integrate with the main parser's precedence handling
}

std::expected<ast::expression, std::string>
ParserIntegration::process_operator_annotation(const ast::expression& expr) {
    // Process operator annotations and update precedence accordingly
    // This would be called during AST processing to handle @infix, @prefix, @postfix annotations
    
    // For now, return the expression unchanged
    return expr;
}

} // namespace meld::parser