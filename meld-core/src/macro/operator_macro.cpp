#include "meld/macro/operator_macro.hpp"
#include "meld/kernel/symbol_table.hpp"
#include "meld/parser/ast.hpp"
#include <format>
#include <regex>
#include <set>

namespace meld::macro {

std::expected<kernel::Value, std::string>
OperatorMacro::apply(const kernel::Value& ast_node, MacroExpander& expander) {
    // Parse the opr declaration
    // Expected format: (opr symbol (params...) body)
    
    auto cons = ast_node.try_as<kernel::Cons>();
    if (!cons) {
        return std::unexpected("opr macro expects a cons cell");
    }
    
    auto head = (*cons)->head();
    auto tail = (*cons)->tail();
    
    // Verify this is an opr call
    auto opr_symbol = head.try_as<kernel::Symbol>();
    if (!opr_symbol || (*opr_symbol)->name() != "opr") {
        return std::unexpected("Expected opr symbol");
    }
    
    // Extract operator symbol
    auto tail_cons = tail.try_as<kernel::Cons>();
    if (!tail_cons) {
        return std::unexpected("opr macro expects operator symbol");
    }
    
    auto symbol_value = (*tail_cons)->head();
    auto symbol_obj = symbol_value.try_as<kernel::Symbol>();
    if (!symbol_obj) {
        return std::unexpected("opr macro expects operator symbol");
    }
    
    std::string operator_symbol = (*symbol_obj)->name();
    
    // Validate operator symbol
    if (!is_valid_operator_symbol(operator_symbol)) {
        return std::unexpected(std::format("Invalid operator symbol: {}", operator_symbol));
    }
    
    // Extract parameters
    auto params_tail = (*tail_cons)->tail();
    auto params_cons = params_tail.try_as<kernel::Cons>();
    if (!params_cons) {
        return std::unexpected("opr macro expects parameter list");
    }
    
    auto params_list = (*params_cons)->head();
    
    // Extract function body
    auto body_tail = (*params_cons)->tail();
    auto body_cons = body_tail.try_as<kernel::Cons>();
    if (!body_cons) {
        return std::unexpected("opr macro expects function body");
    }
    
    auto function_body = (*body_cons)->head();
    
    // Generate mangled function name
    std::string mangled_name = mangle_operator_name(operator_symbol);
    
    // Create function definition AST
    // (fnc mangled_name params body)
    auto fnc_symbol = kernel::SymbolTable::instance().intern("fnc");
    auto name_symbol = kernel::SymbolTable::instance().intern(mangled_name);
    
    // Build the function definition
    auto name_value = kernel::Value(name_symbol);
    auto fnc_params = kernel::Value(std::make_shared<kernel::Cons>(params_list, kernel::nil()));
    auto fnc_body = kernel::Value(std::make_shared<kernel::Cons>(function_body, kernel::nil()));
    auto fnc_tail = kernel::Value(std::make_shared<kernel::Cons>(fnc_params, fnc_body));
    auto fnc_def = kernel::Value(std::make_shared<kernel::Cons>(name_value, fnc_tail));
    auto result = kernel::Value(std::make_shared<kernel::Cons>(kernel::Value(fnc_symbol), fnc_def));
    
    // TODO: Register operator overload with registry
    // This would need type information which requires type checking
    // For now, we'll generate the function and let the runtime handle registration
    
    return result;
}

std::expected<kernel::Value, std::string>
OperatorMacro::apply_infix_annotation(const kernel::Value& ast_node, const InfixConfig& config) {
    // Extract operator symbol from the function definition
    auto annotations = extract_annotations(ast_node);
    
    // Find the operator symbol from the function name or annotations
    std::string operator_symbol;
    
    // Try to extract from function name if it's a mangled operator name
    // For now, we'll assume the operator symbol is provided in the annotation
    // In a full implementation, this would be extracted from the function definition
    
    // Register the infix operator with the specified configuration
    auto& registry = OperatorAnnotationRegistry::instance();
    registry.register_infix_operator(operator_symbol, config);
    
    // Return the original AST node (annotation processing doesn't modify the AST)
    return ast_node;
}

std::expected<kernel::Value, std::string>
OperatorMacro::apply_prefix_annotation(const kernel::Value& ast_node, const PrefixConfig& config) {
    // For now, we'll use a simplified approach where the operator symbol
    // is extracted from the context or provided explicitly
    // In a full implementation, this would parse the AST to find the operator symbol
    
    // TODO: Implement proper AST traversal to extract operator symbol
    // For now, we'll register common prefix operators with the given configuration
    
    auto& registry = OperatorAnnotationRegistry::instance();
    
    // Register common prefix operators that might be annotated
    std::vector<std::string> common_prefix_ops = {"-", "!", "~", "|||", "^", "++", "--"};
    
    for (const auto& op : common_prefix_ops) {
        registry.register_prefix_operator(op, config);
    }
    
    return ast_node;
}

std::expected<kernel::Value, std::string>
OperatorMacro::apply_postfix_annotation(const kernel::Value& ast_node, const PostfixConfig& config) {
    // For now, we'll use a simplified approach similar to prefix operators
    // In a full implementation, this would parse the AST to find the operator symbol
    
    auto& registry = OperatorAnnotationRegistry::instance();
    
    // Register common postfix operators that might be annotated
    std::vector<std::string> common_postfix_ops = {"++", "--", "!"};
    
    for (const auto& op : common_postfix_ops) {
        registry.register_postfix_operator(op, config);
    }
    
    return ast_node;
}

std::expected<kernel::Value, std::string>
OperatorMacro::process_operator_annotations(const kernel::Value& ast_node) {
    // Extract annotations from the AST node
    auto annotations = extract_annotations(ast_node);
    
    for (const auto& annotation_text : annotations) {
        auto annotation_result = parse_annotation(annotation_text);
        if (!annotation_result.has_value()) {
            continue; // Skip invalid annotations
        }
        
        auto [annotation_type, config_text] = *annotation_result;
        
        switch (annotation_type) {
            case OperatorAnnotationType::INFIX: {
                auto config_result = OperatorAnnotationParser::parse_infix_annotation(config_text);
                if (config_result.has_value()) {
                    auto result = apply_infix_annotation(ast_node, *config_result);
                    if (!result.has_value()) {
                        return result;
                    }
                }
                break;
            }
            case OperatorAnnotationType::PREFIX: {
                auto config_result = OperatorAnnotationParser::parse_prefix_annotation(config_text);
                if (config_result.has_value()) {
                    auto result = apply_prefix_annotation(ast_node, *config_result);
                    if (!result.has_value()) {
                        return result;
                    }
                }
                break;
            }
            case OperatorAnnotationType::POSTFIX: {
                auto config_result = OperatorAnnotationParser::parse_postfix_annotation(config_text);
                if (config_result.has_value()) {
                    auto result = apply_postfix_annotation(ast_node, *config_result);
                    if (!result.has_value()) {
                        return result;
                    }
                }
                break;
            }
        }
    }
    
    return ast_node;
}

std::expected<std::string, std::string>
OperatorMacro::parse_operator_symbol(const std::string& function_name) {
    // Extract operator symbol from mangled name
    // Format: __op_<symbol>__
    std::regex pattern(R"(__op_(.+)__)");
    std::smatch match;
    
    if (std::regex_match(function_name, match, pattern)) {
        return match[1].str();
    }
    
    return std::unexpected(std::format("Invalid operator function name: {}", function_name));
}

std::expected<std::string, std::string>
OperatorMacro::extract_function_name(const kernel::Value& ast_node) {
    // Extract function name from AST node
    // This is a simplified implementation - in a full implementation,
    // this would traverse the AST to find the function name
    
    // For now, we'll assume the AST node represents a function definition
    // and try to extract the name from it
    auto cons = ast_node.try_as<kernel::Cons>();
    if (!cons) {
        return std::unexpected("AST node is not a cons cell");
    }
    
    // Try to find the function name in the AST structure
    // This is a placeholder implementation
    // In a real implementation, this would properly traverse the AST
    
    return std::unexpected("Function name extraction not yet fully implemented");
}

std::string OperatorMacro::mangle_operator_name(const std::string& symbol) {
    // Convert operator symbol to valid function name
    // + -> __op_add__
    // - -> __op_sub__
    // * -> __op_mul__
    // ... -> __op_ellipsis__
    // etc.
    
    std::string mangled = "__op_";
    
    // Handle multi-character operators first
    if (symbol == "...") {
        mangled += "ellipsis";
    } else if (symbol == "==") {
        mangled += "eq_eq";
    } else if (symbol == "!=") {
        mangled += "not_eq";
    } else if (symbol == "<=") {
        mangled += "lt_eq";
    } else if (symbol == ">=") {
        mangled += "gt_eq";
    } else if (symbol == "&&") {
        mangled += "and_and";
    } else if (symbol == "||") {
        mangled += "or_or";
    } else if (symbol == "<<") {
        mangled += "lshift";
    } else if (symbol == ">>") {
        mangled += "rshift";
    } else if (symbol == ">>>") {
        mangled += "urshift";
    } else if (symbol == "++") {
        mangled += "inc";
    } else if (symbol == "--") {
        mangled += "dec";
    } else if (symbol == "+=") {
        mangled += "add_assign";
    } else if (symbol == "-=") {
        mangled += "sub_assign";
    } else if (symbol == "*=") {
        mangled += "mul_assign";
    } else if (symbol == "/=") {
        mangled += "div_assign";
    } else if (symbol == "%=") {
        mangled += "mod_assign";
    } else if (symbol == "?:") {
        mangled += "elvis";
    } else if (symbol == "?.") {
        mangled += "safe_nav";
    } else {
        // Handle single-character operators
        for (char c : symbol) {
            switch (c) {
                case '+': mangled += "add"; break;
                case '-': mangled += "sub"; break;
                case '*': mangled += "mul"; break;
                case '/': mangled += "div"; break;
                case '%': mangled += "mod"; break;
                case '=': mangled += "eq"; break;
                case '<': mangled += "lt"; break;
                case '>': mangled += "gt"; break;
                case '!': mangled += "not"; break;
                case '&': mangled += "and"; break;
                case '|': mangled += "or"; break;
                case '^': mangled += "xor"; break;
                case '~': mangled += "neg"; break;
                default:
                    if (std::isalnum(c)) {
                        mangled += c;
                    } else {
                        mangled += std::format("_{:02x}_", static_cast<unsigned char>(c));
                    }
                    break;
            }
        }
    }
    
    mangled += "__";
    return mangled;
}

std::vector<std::string> OperatorMacro::extract_type_signature(
    const std::vector<parser::ast::function_parameter>& params
) {
    std::vector<std::string> signature;
    
    for (const auto& param : params) {
        // For now, use generic type names
        // In a full implementation, this would extract actual type information
        signature.push_back("Any");
    }
    
    return signature;
}

void OperatorMacro::register_operator_overload(
    const std::string& symbol,
    const std::vector<std::string>& type_signature,
    const std::string& mangled_name
) {
    // Register with operator registry
    // This would be called at runtime when the operator function is defined
    auto& registry = kernel::OperatorRegistry::instance();
    
    // Create a function that calls the mangled operator function
    auto operator_func = [mangled_name](const std::vector<kernel::Value>& args) -> std::expected<kernel::Value, std::string> {
        // TODO: Call the actual mangled function
        // For now, return an error indicating this needs runtime support
        return std::unexpected(std::format("Operator {} not yet implemented at runtime", mangled_name));
    };
    
    // Register for the first type in signature (simple dispatch for now)
    if (!type_signature.empty()) {
        registry.register_operator_overload(symbol, type_signature[0], operator_func);
    }
}

bool OperatorMacro::is_valid_operator_symbol(const std::string& symbol) {
    if (symbol.empty()) {
        return false;
    }
    
    // Allow common operator symbols
    static const std::set<std::string> valid_symbols = {
        "+", "-", "*", "/", "%",
        "==", "!=", "<", "<=", ">", ">=",
        "&&", "||", "!",
        "&", "|", "^", "~",
        "<<", ">>", ">>>",
        "++", "--",
        "[]", "()",
        "=", "+=", "-=", "*=", "/=", "%=",
        "?:", "?.",
        "..."  // Ellipsis operator for rest parameters and spread syntax
    };
    
    return valid_symbols.contains(symbol);
}

kernel::OperatorType OperatorMacro::determine_operator_type(
    const std::string& symbol, 
    size_t arity
) {
    if (arity == 1) {
        // Unary operators can be prefix or postfix
        if (symbol == "++" || symbol == "--") {
            return kernel::OperatorType::POSTFIX; // Default for increment/decrement
        }
        return kernel::OperatorType::PREFIX;
    } else if (arity == 2) {
        return kernel::OperatorType::INFIX;
    }
    
    // Default to infix for unknown cases
    return kernel::OperatorType::INFIX;
}

std::vector<std::string> OperatorMacro::extract_annotations(const kernel::Value& ast_node) {
    std::vector<std::string> annotations;
    
    // TODO: Extract annotations from AST node metadata
    // This would need to be implemented based on how annotations are stored in the AST
    // For now, return empty vector
    
    return annotations;
}

std::expected<std::pair<OperatorAnnotationType, std::string>, std::string>
OperatorMacro::parse_annotation(const std::string& annotation_text) {
    // Parse annotation format: @infix(precedence=5, assoc=left)
    std::regex annotation_regex(R"(@(\w+)(\([^)]*\))?)");
    std::smatch match;
    
    if (std::regex_match(annotation_text, match, annotation_regex)) {
        std::string annotation_name = match[1].str();
        std::string config_text = match[2].str();
        
        auto annotation_type_result = OperatorAnnotationParser::get_annotation_type(annotation_name);
        if (!annotation_type_result.has_value()) {
            return std::unexpected(annotation_type_result.error());
        }
        
        return std::make_pair(*annotation_type_result, config_text);
    }
    
    return std::unexpected(std::format("Invalid annotation format: {}", annotation_text));
}

void register_operator_macro() {
    auto& registry = MacroRegistry::instance();
    
    auto opr_macro = make_macro(
        "opr",
        {"symbol", "params", "body"},
        [](const kernel::Value& ast_node, MacroExpander& expander) {
            return OperatorMacro::apply(ast_node, expander);
        }
    );
    
    registry.register_macro(opr_macro);
}

void register_operator_annotation_processors() {
    auto& registry = MacroRegistry::instance();
    
    // Register @infix annotation processor
    auto infix_processor = make_macro(
        "@infix",
        {"config"},
        [](const kernel::Value& ast_node, MacroExpander& expander) {
            return OperatorMacro::process_operator_annotations(ast_node);
        }
    );
    
    registry.register_macro(infix_processor);
    
    // Register @prefix annotation processor
    auto prefix_processor = make_macro(
        "@prefix",
        {"config"},
        [](const kernel::Value& ast_node, MacroExpander& expander) {
            return OperatorMacro::process_operator_annotations(ast_node);
        }
    );
    
    registry.register_macro(prefix_processor);
    
    // Register @postfix annotation processor
    auto postfix_processor = make_macro(
        "@postfix",
        {"config"},
        [](const kernel::Value& ast_node, MacroExpander& expander) {
            return OperatorMacro::process_operator_annotations(ast_node);
        }
    );
    
    registry.register_macro(postfix_processor);
}

} // namespace meld::macro