#pragma once

#include "meld/kernel/operators.hpp"
#include <string>
#include <optional>
#include <expected>
#include <map>

namespace meld::macro {

// Operator annotation types
enum class OperatorAnnotationType {
    INFIX,
    PREFIX,
    POSTFIX
};

// Infix operator configuration
struct InfixConfig {
    int precedence = 5;                           // Default precedence
    kernel::Associativity associativity = kernel::Associativity::LEFT;  // Default associativity
    
    InfixConfig() = default;
    InfixConfig(int prec, kernel::Associativity assoc) 
        : precedence(prec), associativity(assoc) {}
};

// Prefix operator configuration
struct PrefixConfig {
    int precedence = 80;  // Default high precedence for prefix operators
    
    PrefixConfig() = default;
    PrefixConfig(int prec) : precedence(prec) {}
};

// Postfix operator configuration
struct PostfixConfig {
    int precedence = 90;  // Default highest precedence for postfix operators
    
    PostfixConfig() = default;
    PostfixConfig(int prec) : precedence(prec) {}
};

// Operator annotation parser
class OperatorAnnotationParser {
public:
    // Parse @infix annotation
    // Format: @infix(precedence=5, assoc=left)
    static std::expected<InfixConfig, std::string>
    parse_infix_annotation(const std::string& annotation_text);
    
    // Parse @prefix annotation
    // Format: @prefix(precedence=80)
    static std::expected<PrefixConfig, std::string>
    parse_prefix_annotation(const std::string& annotation_text);
    
    // Parse @postfix annotation
    // Format: @postfix(precedence=90)
    static std::expected<PostfixConfig, std::string>
    parse_postfix_annotation(const std::string& annotation_text);
    
    // Determine annotation type from string
    static std::expected<OperatorAnnotationType, std::string>
    get_annotation_type(const std::string& annotation_name);
    
private:
    // Helper to parse associativity string (public for testing)
public:
    static std::expected<kernel::Associativity, std::string>
    parse_associativity(const std::string& assoc_str);
    
private:
    // Helper to extract parameter value from annotation
    static std::optional<std::string>
    extract_parameter(const std::string& annotation_text, const std::string& param_name);
    
    // Helper to parse integer parameter
    static std::expected<int, std::string>
    parse_int_parameter(const std::string& value_str);
};

// Operator annotation registry
class OperatorAnnotationRegistry {
public:
    static OperatorAnnotationRegistry& instance() {
        static OperatorAnnotationRegistry registry;
        return registry;
    }
    
    // Register an infix operator with configuration
    void register_infix_operator(
        const std::string& symbol,
        const InfixConfig& config
    );
    
    // Register a prefix operator with configuration
    void register_prefix_operator(
        const std::string& symbol,
        const PrefixConfig& config
    );
    
    // Register a postfix operator with configuration
    void register_postfix_operator(
        const std::string& symbol,
        const PostfixConfig& config
    );
    
    // Get operator information
    std::expected<kernel::OperatorInfo, std::string>
    get_operator_info(const std::string& symbol) const;
    
    // Check if operator is registered
    bool has_operator(const std::string& symbol) const;
    
    // Update parser with custom operator precedence
    void update_parser_precedence(const std::string& symbol, int precedence);
    
private:
    OperatorAnnotationRegistry() = default;
    
    std::map<std::string, InfixConfig> infix_operators_;
    std::map<std::string, PrefixConfig> prefix_operators_;
    std::map<std::string, PostfixConfig> postfix_operators_;
};

} // namespace meld::macro