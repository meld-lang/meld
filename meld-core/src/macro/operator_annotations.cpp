#include "meld/macro/operator_annotations.hpp"
#include <regex>
#include <format>

namespace meld::macro {

std::expected<InfixConfig, std::string>
OperatorAnnotationParser::parse_infix_annotation(const std::string& annotation_text) {
    InfixConfig config;
    
    // Extract precedence parameter
    auto precedence_str = extract_parameter(annotation_text, "precedence");
    if (precedence_str.has_value()) {
        auto precedence_result = parse_int_parameter(*precedence_str);
        if (!precedence_result.has_value()) {
            return std::unexpected(precedence_result.error());
        }
        config.precedence = *precedence_result;
    }
    
    // Extract associativity parameter
    auto assoc_str = extract_parameter(annotation_text, "assoc");
    if (assoc_str.has_value()) {
        auto assoc_result = parse_associativity(*assoc_str);
        if (!assoc_result.has_value()) {
            return std::unexpected(assoc_result.error());
        }
        config.associativity = *assoc_result;
    }
    
    return config;
}

std::expected<PrefixConfig, std::string>
OperatorAnnotationParser::parse_prefix_annotation(const std::string& annotation_text) {
    PrefixConfig config;
    
    // Extract precedence parameter
    auto precedence_str = extract_parameter(annotation_text, "precedence");
    if (precedence_str.has_value()) {
        auto precedence_result = parse_int_parameter(*precedence_str);
        if (!precedence_result.has_value()) {
            return std::unexpected(precedence_result.error());
        }
        config.precedence = *precedence_result;
    }
    
    return config;
}

std::expected<PostfixConfig, std::string>
OperatorAnnotationParser::parse_postfix_annotation(const std::string& annotation_text) {
    PostfixConfig config;
    
    // Extract precedence parameter
    auto precedence_str = extract_parameter(annotation_text, "precedence");
    if (precedence_str.has_value()) {
        auto precedence_result = parse_int_parameter(*precedence_str);
        if (!precedence_result.has_value()) {
            return std::unexpected(precedence_result.error());
        }
        config.precedence = *precedence_result;
    }
    
    return config;
}

std::expected<OperatorAnnotationType, std::string>
OperatorAnnotationParser::get_annotation_type(const std::string& annotation_name) {
    if (annotation_name == "infix") {
        return OperatorAnnotationType::INFIX;
    } else if (annotation_name == "prefix") {
        return OperatorAnnotationType::PREFIX;
    } else if (annotation_name == "postfix") {
        return OperatorAnnotationType::POSTFIX;
    } else {
        return std::unexpected(std::format("Unknown operator annotation type: {}", annotation_name));
    }
}

std::expected<kernel::Associativity, std::string>
OperatorAnnotationParser::parse_associativity(const std::string& assoc_str) {
    if (assoc_str == "left") {
        return kernel::Associativity::LEFT;
    } else if (assoc_str == "right") {
        return kernel::Associativity::RIGHT;
    } else if (assoc_str == "none") {
        return kernel::Associativity::NONE;
    } else {
        return std::unexpected(std::format("Invalid associativity: {}. Expected 'left', 'right', or 'none'", assoc_str));
    }
}

std::optional<std::string>
OperatorAnnotationParser::extract_parameter(const std::string& annotation_text, const std::string& param_name) {
    // Parse annotation format: @infix(precedence=5, assoc=left)
    std::regex param_regex(param_name + R"(\s*=\s*([^,\)]+))");
    std::smatch match;
    
    if (std::regex_search(annotation_text, match, param_regex)) {
        std::string value = match[1].str();
        // Trim whitespace
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        return value;
    }
    
    return std::nullopt;
}

std::expected<int, std::string>
OperatorAnnotationParser::parse_int_parameter(const std::string& value_str) {
    try {
        int value = std::stoi(value_str);
        return value;
    } catch (const std::exception& e) {
        return std::unexpected(std::format("Invalid integer value: {}", value_str));
    }
}

// OperatorAnnotationRegistry implementation

void OperatorAnnotationRegistry::register_infix_operator(
    const std::string& symbol,
    const InfixConfig& config
) {
    infix_operators_[symbol] = config;
    
    // Update parser with custom precedence
    update_parser_precedence(symbol, config.precedence);
}

void OperatorAnnotationRegistry::register_prefix_operator(
    const std::string& symbol,
    const PrefixConfig& config
) {
    prefix_operators_[symbol] = config;
    
    // Update parser with custom precedence
    update_parser_precedence(symbol, config.precedence);
}

void OperatorAnnotationRegistry::register_postfix_operator(
    const std::string& symbol,
    const PostfixConfig& config
) {
    postfix_operators_[symbol] = config;
    
    // Update parser with custom precedence
    update_parser_precedence(symbol, config.precedence);
}

std::expected<kernel::OperatorInfo, std::string>
OperatorAnnotationRegistry::get_operator_info(const std::string& symbol) const {
    // Check infix operators first
    if (auto it = infix_operators_.find(symbol); it != infix_operators_.end()) {
        const auto& config = it->second;
        return kernel::OperatorInfo(
            symbol,
            kernel::OperatorType::INFIX,
            config.precedence,
            config.associativity
        );
    }
    
    // Check prefix operators
    if (auto it = prefix_operators_.find(symbol); it != prefix_operators_.end()) {
        const auto& config = it->second;
        return kernel::OperatorInfo(
            symbol,
            kernel::OperatorType::PREFIX,
            config.precedence,
            kernel::Associativity::NONE  // Prefix operators don't have associativity
        );
    }
    
    // Check postfix operators
    if (auto it = postfix_operators_.find(symbol); it != postfix_operators_.end()) {
        const auto& config = it->second;
        return kernel::OperatorInfo(
            symbol,
            kernel::OperatorType::POSTFIX,
            config.precedence,
            kernel::Associativity::NONE  // Postfix operators don't have associativity
        );
    }
    
    return std::unexpected(std::format("Operator not found: {}", symbol));
}

bool OperatorAnnotationRegistry::has_operator(const std::string& symbol) const {
    return infix_operators_.contains(symbol) ||
           prefix_operators_.contains(symbol) ||
           postfix_operators_.contains(symbol);
}

void OperatorAnnotationRegistry::update_parser_precedence(const std::string& symbol, int precedence) {
    // TODO: This would need to integrate with the parser's precedence table
    // For now, we'll store the information and let the parser query it when needed
    // In a full implementation, this would update the parser's operator precedence table
    
    // Register with the kernel operator registry as well
    auto& kernel_registry = kernel::OperatorRegistry::instance();
    
    // Determine operator type based on which map it's in
    kernel::OperatorType op_type = kernel::OperatorType::INFIX;
    kernel::Associativity assoc = kernel::Associativity::LEFT;
    
    if (infix_operators_.contains(symbol)) {
        op_type = kernel::OperatorType::INFIX;
        assoc = infix_operators_.at(symbol).associativity;
    } else if (prefix_operators_.contains(symbol)) {
        op_type = kernel::OperatorType::PREFIX;
        assoc = kernel::Associativity::NONE;
    } else if (postfix_operators_.contains(symbol)) {
        op_type = kernel::OperatorType::POSTFIX;
        assoc = kernel::Associativity::NONE;
    }
    
    kernel::OperatorInfo info(symbol, op_type, precedence, assoc);
    kernel_registry.register_custom_operator(info);
}

} // namespace meld::macro