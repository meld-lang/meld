#pragma once

#include "meld/parser/ast.hpp"
#include <string>
#include <sstream>

namespace meld::compiler {

/**
 * Converts AST nodes back to source code
 */
class ASTPrinter {
public:
    ASTPrinter() = default;
    
    // Print an expression to source code
    std::string print(const parser::ast::expression& expr) const;
    
    // Print with indentation
    std::string print(const parser::ast::expression& expr, int indent_level) const;
    
    // Print a type annotation with [] for generic type arguments
    std::string printTypeAnnotation(const parser::ast::type_annotation& type, int indent = 0) const;
    
    // Print generic type parameter declarations with [] syntax (e.g., [T, E: Error])
    std::string printGenericTypeParameters(
        const std::vector<parser::ast::generic_type_parameter>& params, int indent = 0) const;
    
private:
    // Helper methods for different node types
    std::string printIdentifier(const parser::ast::identifier& node, int indent) const;
    std::string printIntegerLiteral(const parser::ast::integer_literal& node, int indent) const;
    std::string printFloatLiteral(const parser::ast::float_literal& node, int indent) const;
    std::string printStringLiteral(const parser::ast::string_literal& node, int indent) const;
    std::string printBooleanLiteral(const parser::ast::boolean_literal& node, int indent) const;
    std::string printFunctionCall(const parser::ast::function_call& node, int indent) const;
    std::string printBinaryOperation(const parser::ast::binary_operation& node, int indent) const;
    std::string printUnaryOperation(const parser::ast::unary_operation& node, int indent) const;
    std::string printLambdaExpression(const parser::ast::lambda_expression& node, int indent) const;
    std::string printBlockExpression(const parser::ast::block_expression& node, int indent) const;
    std::string printListExpression(const parser::ast::list_expression& node, int indent) const;
    std::string printValDeclaration(const parser::ast::val_declaration& node, int indent) const;
    std::string printVarDeclaration(const parser::ast::var_declaration& node, int indent) const;
    std::string printTupleLiteral(const parser::ast::tuple_literal& node, int indent) const;
    std::string printPipelineExpression(const parser::ast::pipeline_expression& node, int indent) const;
    
    // Utility methods
    std::string getIndent(int level) const;
    std::string escapeString(const std::string& str) const;
};

} // namespace meld::compiler