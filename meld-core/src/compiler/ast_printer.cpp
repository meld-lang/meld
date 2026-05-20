#include "meld/compiler/ast_printer.hpp"
#include "meld/compat/visit.hpp"

namespace meld::compiler {

std::string ASTPrinter::print(const parser::ast::expression& expr) const {
    return print(expr, 0);
}

std::string ASTPrinter::print(const parser::ast::expression& expr, int indent_level) const {
    // Use meld::compat::visit with a generic lambda — MSVC forbids template
    // members in local classes (C2892) but generic lambdas are fine.
    return meld::compat::visit<std::string>([&](auto const& node) -> std::string {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, parser::ast::identifier>) {
            return printIdentifier(node, indent_level);
        }
        else if constexpr (std::is_same_v<T, parser::ast::integer_literal>) {
            return printIntegerLiteral(node, indent_level);
        }
        else if constexpr (std::is_same_v<T, parser::ast::float_literal>) {
            return printFloatLiteral(node, indent_level);
        }
        else if constexpr (std::is_same_v<T, parser::ast::string_literal>) {
            return printStringLiteral(node, indent_level);
        }
        else if constexpr (std::is_same_v<T, parser::ast::boolean_literal>) {
            return printBooleanLiteral(node, indent_level);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::function_call>>) {
            return printFunctionCall(node.get(), indent_level);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::binary_operation>>) {
            return printBinaryOperation(node.get(), indent_level);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::unary_operation>>) {
            return printUnaryOperation(node.get(), indent_level);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::lambda_expression>>) {
            return printLambdaExpression(node.get(), indent_level);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::list_expression>>) {
            return printListExpression(node.get(), indent_level);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::val_declaration>>) {
            return printValDeclaration(node.get(), indent_level);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::var_declaration>>) {
            return printVarDeclaration(node.get(), indent_level);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::tuple_literal>>) {
            return printTupleLiteral(node.get(), indent_level);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::pipeline_expression>>) {
            return printPipelineExpression(node.get(), indent_level);
        }
        else {
            return "/* unsupported node type */";
        }
    }, expr);
}

std::string ASTPrinter::printIdentifier(const parser::ast::identifier& node, int indent) const {
    return node.name;
}

std::string ASTPrinter::printIntegerLiteral(const parser::ast::integer_literal& node, int indent) const {
    std::string result = std::to_string(node.value);
    if (!node.suffix.empty()) {
        result += node.suffix;
    }
    return result;
}

std::string ASTPrinter::printFloatLiteral(const parser::ast::float_literal& node, int indent) const {
    std::string result = std::to_string(node.value);
    if (!node.suffix.empty()) {
        result += node.suffix;
    }
    return result;
}

std::string ASTPrinter::printStringLiteral(const parser::ast::string_literal& node, int indent) const {
    if (node.has_interpolation) {
        return "`" + node.value + "`";
    } else {
        return "\"" + escapeString(node.value) + "\"";
    }
}

std::string ASTPrinter::printBooleanLiteral(const parser::ast::boolean_literal& node, int indent) const {
    return node.value ? "true" : "false";
}

std::string ASTPrinter::printFunctionCall(const parser::ast::function_call& node, int indent) const {
    std::string result = node.function_name.name + "(";
    
    // Print positional arguments
    for (size_t i = 0; i < node.arguments.size(); ++i) {
        if (i > 0) result += ", ";
        result += print(node.arguments[i], indent);
    }
    
    // Print named arguments
    for (size_t i = 0; i < node.named_arguments.size(); ++i) {
        if (i > 0 || !node.arguments.empty()) result += ", ";
        result += node.named_arguments[i].name.name + " = " + print(node.named_arguments[i].value, indent);
    }
    
    result += ")";
    return result;
}

std::string ASTPrinter::printBinaryOperation(const parser::ast::binary_operation& node, int indent) const {
    std::string left = print(node.left, indent);
    std::string right = print(node.right, indent);
    return "(" + left + " " + node.op + " " + right + ")";
}

std::string ASTPrinter::printUnaryOperation(const parser::ast::unary_operation& node, int indent) const {
    std::string operand = print(node.operand, indent);
    return node.op + operand;
}

std::string ASTPrinter::printLambdaExpression(const parser::ast::lambda_expression& node, int indent) const {
    std::string result;
    
    // Print parameters
    if (node.parameters.size() == 1) {
        result += node.parameters[0].name.name;
    } else {
        result += "(";
        for (size_t i = 0; i < node.parameters.size(); ++i) {
            if (i > 0) result += ", ";
            result += node.parameters[i].name.name;
            if (node.parameters[i].has_type) {
                result += ": " + node.parameters[i].type.type_name.name;
            }
        }
        result += ")";
    }
    
    result += " => ";
    
    // Print body
    if (node.is_block) {
        result += "{\n";
        result += print(node.body, indent + 1);
        result += "\n" + getIndent(indent) + "}";
    } else {
        result += print(node.body, indent);
    }
    
    return result;
}

std::string ASTPrinter::printBlockExpression(const parser::ast::block_expression& node, int indent) const {
    std::string result;
    
    for (size_t i = 0; i < node.statements.size(); ++i) {
        if (i > 0) result += "\n";
        result += getIndent(indent) + print(node.statements[i], indent);
    }
    
    return result;
}

std::string ASTPrinter::printListExpression(const parser::ast::list_expression& node, int indent) const {
    std::string result = "[";
    
    for (size_t i = 0; i < node.elements.size(); ++i) {
        if (i > 0) result += ", ";
        result += print(node.elements[i], indent);
    }
    
    result += "]";
    return result;
}

std::string ASTPrinter::printValDeclaration(const parser::ast::val_declaration& node, int indent) const {
    std::string result = "val " + node.name.name;
    if (node.has_type_annotation) {
        result += ": " + node.type_ann.get().type_name.name;
        if (node.type_ann.get().is_nullable) result += "?";
    }
    result += " = " + print(node.value, indent);
    return result;
}

std::string ASTPrinter::printVarDeclaration(const parser::ast::var_declaration& node, int indent) const {
    std::string result = "var " + node.name.name;
    if (node.has_type_annotation) {
        result += ": " + node.type_ann.get().type_name.name;
        if (node.type_ann.get().is_nullable) result += "?";
    }
    result += " = " + print(node.value, indent);
    return result;
}

std::string ASTPrinter::printTupleLiteral(const parser::ast::tuple_literal& node, int indent) const {
    std::string result = "[";
    
    for (size_t i = 0; i < node.elements.size(); ++i) {
        if (i > 0) result += ", ";
        
        if (node.elements[i].is_named) {
            result += node.elements[i].name + ": ";
        }
        result += print(node.elements[i].value, indent);
    }
    
    result += "]";
    return result;
}

std::string ASTPrinter::printPipelineExpression(const parser::ast::pipeline_expression& node, int indent) const {
    return print(node.value, indent) + " |> " + print(node.function, indent);
}

std::string ASTPrinter::getIndent(int level) const {
    return std::string(level * 2, ' ');
}

std::string ASTPrinter::escapeString(const std::string& str) const {
    std::string result;
    for (char c : str) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\t': result += "\\t"; break;
            case '\r': result += "\\r"; break;
            default: result += c; break;
        }
    }
    return result;
}

std::string ASTPrinter::printTypeAnnotation(const parser::ast::type_annotation& type, int indent) const {
    std::string result = type.type_name.name;

    if (type.has_type_arguments && !type.type_arguments.empty()) {
        result += "[";
        for (size_t i = 0; i < type.type_arguments.size(); ++i) {
            if (i > 0) result += ", ";
            result += printTypeAnnotation(type.type_arguments[i].get(), indent);
        }
        result += "]";
    }

    if (type.is_nullable) result += "?";

    if (type.is_union && !type.union_types.empty()) {
        for (const auto& ut : type.union_types) {
            result += " | " + printTypeAnnotation(ut.get(), indent);
        }
    }

    if (type.is_intersection && !type.intersection_types.empty()) {
        for (const auto& it : type.intersection_types) {
            result += " & " + printTypeAnnotation(it.get(), indent);
        }
    }

    return result;
}

std::string ASTPrinter::printGenericTypeParameters(
    const std::vector<parser::ast::generic_type_parameter>& params, int indent) const {
    if (params.empty()) return "";
    std::string result = "[";
    for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0) result += ", ";
        if (!params[i].variance.empty()) result += params[i].variance + " ";
        result += params[i].name.name;
        if (params[i].has_bound) {
            result += ": " + printTypeAnnotation(params[i].bound.get(), indent);
        }
    }
    result += "]";
    return result;
}

} // namespace meld::compiler