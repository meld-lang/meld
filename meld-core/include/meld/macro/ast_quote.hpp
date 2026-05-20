#pragma once

#include "meld/parser/ast.hpp"
#include "meld/parser/ast_node.hpp"
#include <string>
#include <vector>
#include <map>
#include <variant>
#include <expected>
#include <functional>

namespace meld::macro {

/// Interpolation value — what can be spliced into an ast.quote template.
/// Strings are used for identifiers/names, type_annotations for types,
/// and expressions for arbitrary AST subtrees.
using InterpolationValue = std::variant<
    std::string,                        // For identifier names (e.g., ${node.name})
    parser::ast::type_annotation,       // For types (e.g., ${node.type})
    parser::ast::expression             // For arbitrary expressions
>;

/// Source location for error reporting in ast.quote templates.
struct QuoteSourceLocation {
    size_t line = 0;
    size_t column = 0;
    std::string template_text;
};

/// Result of an ast.quote expansion — either a single expression or
/// multiple statements (for block templates).
struct QuoteResult {
    std::vector<parser::ast::expression> expressions;

    /// Convenience: get the single expression (most common case).
    /// Returns unexpected if there are zero or multiple expressions.
    std::expected<parser::ast::expression, std::string> single() const;
};

/// AstQuote — the C++ implementation of Meld's `ast.quote { ... }` facility.
///
/// Usage (from C++ macro code):
///   AstQuote quote;
///   quote.bind("node_name", std::string("age"));
///   quote.bind("node_type", type_annotation{...});
///   auto result = quote.parse("pub fnc ${node_name}() -> ${node_type} { rtn this.${node_name} }");
///
/// The template string is valid Meld code with ${...} interpolation holes.
/// Each hole references a bound name. The template is lexed and parsed with
/// interpolation markers replaced by the bound values.
class AstQuote {
public:
    AstQuote() = default;

    /// Bind a name to an interpolation value.
    void bind(const std::string& name, InterpolationValue value);

    /// Parse a Meld code template with interpolation, producing AST nodes.
    /// Interpolation holes use ${name} syntax where 'name' was previously bound.
    /// Returns the parsed AST expressions with interpolated values spliced in.
    std::expected<QuoteResult, std::string> parse(const std::string& template_str) const;

    /// Convenience: parse and return a single expression.
    std::expected<parser::ast::expression, std::string>
    parse_expression(const std::string& template_str) const;

    /// Convenience: parse and return a single function_definition.
    /// Useful for generating getter/setter methods.
    std::expected<parser::ast::function_definition, std::string>
    parse_function(const std::string& template_str) const;

    /// Clear all bindings.
    void clear();

    /// Check if a binding exists.
    bool has_binding(const std::string& name) const;

    /// Substitute in an identifier (replace if it matches a placeholder).
    void substitute_in_identifier(
        parser::ast::identifier& id,
        const std::map<std::string, InterpolationValue>& placeholders) const;

    /// Substitute in a type_annotation.
    void substitute_in_type(
        parser::ast::type_annotation& type,
        const std::map<std::string, InterpolationValue>& placeholders) const;

    /// Substitute in a function_definition's name, parameters, return type, body.
    void substitute_in_function(
        parser::ast::function_definition& func,
        const std::map<std::string, InterpolationValue>& placeholders) const;

private:
    /// Expand interpolation holes in the template string.
    /// Returns the expanded source with placeholder identifiers, plus a map
    /// from placeholder names to their interpolation values.
    struct ExpandedTemplate {
        std::string source;
        std::map<std::string, InterpolationValue> placeholders;
    };

    std::expected<ExpandedTemplate, std::string>
    expand_template(const std::string& template_str) const;

    /// After parsing, walk the AST and replace placeholder identifiers
    /// with the actual interpolation values.
    void substitute_placeholders(
        parser::ast::expression& expr,
        const std::map<std::string, InterpolationValue>& placeholders) const;

    std::map<std::string, InterpolationValue> bindings_;
};

} // namespace meld::macro
