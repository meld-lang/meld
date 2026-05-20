#pragma once

#include "macro.hpp"
#include "operator_annotations.hpp"
#include "meld/kernel/operators.hpp"
#include "meld/parser/ast.hpp"
#include <string>
#include <memory>
#include <expected>

namespace meld::macro {

// Operator macro implementation
// Transforms 'opr' declarations into function definitions with operator registration
class OperatorMacro {
public:
    // Apply opr macro transformation
    static std::expected<kernel::Value, std::string>
    apply(const kernel::Value& ast_node, MacroExpander& expander);
    
    // Apply @infix annotation to operator function
    static std::expected<kernel::Value, std::string>
    apply_infix_annotation(const kernel::Value& ast_node, const InfixConfig& config);
    
    // Apply @prefix annotation to operator function
    static std::expected<kernel::Value, std::string>
    apply_prefix_annotation(const kernel::Value& ast_node, const PrefixConfig& config);
    
    // Apply @postfix annotation to operator function
    static std::expected<kernel::Value, std::string>
    apply_postfix_annotation(const kernel::Value& ast_node, const PostfixConfig& config);
    
    // Parse operator symbol from function name
    static std::expected<std::string, std::string>
    parse_operator_symbol(const std::string& function_name);
    
    // Extract function name from AST node
    static std::expected<std::string, std::string>
    extract_function_name(const kernel::Value& ast_node);
    
    // Generate mangled operator function name
    static std::string mangle_operator_name(const std::string& symbol);
    
    // Extract type signature from function parameters
    static std::vector<std::string> extract_type_signature(
        const std::vector<parser::ast::function_parameter>& params
    );
    
    // Register operator with the registry
    static void register_operator_overload(
        const std::string& symbol,
        const std::vector<std::string>& type_signature,
        const std::string& mangled_name
    );
    
    // Process annotations on operator function
    static std::expected<kernel::Value, std::string>
    process_operator_annotations(const kernel::Value& ast_node);

    // Helper to validate operator symbol
    static bool is_valid_operator_symbol(const std::string& symbol);
    
    // Helper to determine operator type from symbol and arity
    static kernel::OperatorType determine_operator_type(
        const std::string& symbol, 
        size_t arity
    );

private:
    
    // Extract annotations from function definition
    static std::vector<std::string> extract_annotations(const kernel::Value& ast_node);
    
    // Parse annotation text to determine type and configuration
    static std::expected<std::pair<OperatorAnnotationType, std::string>, std::string>
    parse_annotation(const std::string& annotation_text);
};

// Register the opr macro with the macro registry
void register_operator_macro();

// Register annotation processors
void register_operator_annotation_processors();

} // namespace meld::macro