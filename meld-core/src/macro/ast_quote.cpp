#include "meld/macro/ast_quote.hpp"
#include "meld/parser/parser.hpp"
#include <format>
#include <regex>
#include <sstream>

namespace meld::macro {

// --- QuoteResult ---

std::expected<parser::ast::expression, std::string> QuoteResult::single() const {
    if (expressions.empty()) {
        return std::unexpected("ast.quote produced no expressions");
    }
    if (expressions.size() > 1) {
        return std::unexpected(std::format(
            "ast.quote produced {} expressions, expected 1", expressions.size()));
    }
    return expressions[0];
}

// --- AstQuote public API ---

void AstQuote::bind(const std::string& name, InterpolationValue value) {
    bindings_[name] = std::move(value);
}

void AstQuote::clear() {
    bindings_.clear();
}

bool AstQuote::has_binding(const std::string& name) const {
    return bindings_.contains(name);
}

std::expected<QuoteResult, std::string>
AstQuote::parse(const std::string& template_str) const {
    // Step 1: Expand interpolation holes into placeholder identifiers
    auto expanded = expand_template(template_str);
    if (!expanded) {
        return std::unexpected(expanded.error());
    }

    // Step 2: Parse the expanded source as normal Meld code
    parser::Parser p;
    std::vector<parser::ast::expression> ast;
    if (!p.parse_file(expanded->source, ast) || ast.empty()) {
        return std::unexpected(std::format("ast.quote parse error: {}", p.error_message()));
    }

    // Step 3: Walk the AST and replace placeholder identifiers with real values
    for (auto& expr : ast) {
        substitute_placeholders(expr, expanded->placeholders);
    }

    QuoteResult result;
    result.expressions = std::move(ast);
    return result;
}

std::expected<parser::ast::expression, std::string>
AstQuote::parse_expression(const std::string& template_str) const {
    auto result = parse(template_str);
    if (!result) return std::unexpected(result.error());
    return result->single();
}

std::expected<parser::ast::function_definition, std::string>
AstQuote::parse_function(const std::string& template_str) const {
    auto result = parse(template_str);
    if (!result) return std::unexpected(result.error());
    if (result->expressions.empty()) {
        return std::unexpected("ast.quote produced no expressions for function");
    }

    // Extract function_definition from the first expression
    auto& expr = result->expressions[0];
    auto* func_ptr = boost::get<
        boost::spirit::x3::forward_ast<parser::ast::function_definition>>(&expr);
    if (!func_ptr) {
        return std::unexpected(
            "ast.quote template did not produce a function definition");
    }
    return func_ptr->get();
}

// --- Template expansion ---

std::expected<AstQuote::ExpandedTemplate, std::string>
AstQuote::expand_template(const std::string& template_str) const {
    ExpandedTemplate result;
    std::string& out = result.source;
    out.reserve(template_str.size());

    size_t i = 0;
    size_t placeholder_counter = 0;

    while (i < template_str.size()) {
        // Look for ${ interpolation start
        if (i + 1 < template_str.size() &&
            template_str[i] == '$' && template_str[i + 1] == '{') {
            // Find the closing }
            size_t start = i + 2;
            size_t end = template_str.find('}', start);
            if (end == std::string::npos) {
                return std::unexpected(std::format(
                    "ast.quote: unclosed interpolation at position {}", i));
            }

            std::string binding_name(template_str, start, end - start);

            // Trim whitespace from binding name
            auto ltrim = binding_name.find_first_not_of(" \t");
            auto rtrim = binding_name.find_last_not_of(" \t");
            if (ltrim != std::string::npos) {
                binding_name = binding_name.substr(ltrim, rtrim - ltrim + 1);
            }

            // Look up the binding
            auto it = bindings_.find(binding_name);
            if (it == bindings_.end()) {
                return std::unexpected(std::format(
                    "ast.quote: unbound interpolation variable '{}'",
                    binding_name));
            }

            // Generate a unique placeholder identifier
            std::string placeholder = std::format(
                "__quote_placeholder_{}", placeholder_counter++);

            // For string values, emit the string directly as an identifier
            // For type/expression values, emit placeholder and record mapping
            if (auto* str_val = std::get_if<std::string>(&it->second)) {
                // String interpolation: splice the string directly into source
                out += *str_val;
            } else {
                // Type or expression: use placeholder identifier
                out += placeholder;
                result.placeholders[placeholder] = it->second;
            }

            i = end + 1;
        } else {
            out += template_str[i];
            ++i;
        }
    }

    return result;
}

// --- AST substitution ---

// Forward declaration for recursive substitution
static void substitute_in_block(
    parser::ast::block_expression& block,
    const std::map<std::string, InterpolationValue>& placeholders);

void AstQuote::substitute_in_identifier(
    parser::ast::identifier& id,
    const std::map<std::string, InterpolationValue>& placeholders) const {
    auto it = placeholders.find(id.name);
    if (it != placeholders.end()) {
        if (auto* str_val = std::get_if<std::string>(&it->second)) {
            id.name = *str_val;
        }
        // Type and expression placeholders in identifier position:
        // the identifier name stays as-is (type substitution happens
        // in type_annotation positions, not identifier positions)
    }
}

void AstQuote::substitute_in_type(
    parser::ast::type_annotation& type,
    const std::map<std::string, InterpolationValue>& placeholders) const {
    auto it = placeholders.find(type.type_name.name);
    if (it != placeholders.end()) {
        if (auto* type_val =
                std::get_if<parser::ast::type_annotation>(&it->second)) {
            // Replace the entire type annotation
            type = *type_val;
        } else if (auto* str_val = std::get_if<std::string>(&it->second)) {
            type.type_name.name = *str_val;
        }
    }
}

void AstQuote::substitute_in_function(
    parser::ast::function_definition& func,
    const std::map<std::string, InterpolationValue>& placeholders) const {
    substitute_in_identifier(func.name, placeholders);
    if (func.has_return_type) {
        substitute_in_type(func.return_type, placeholders);
    }
    for (auto& param : func.parameters) {
        substitute_in_identifier(param.name, placeholders);
        substitute_in_type(param.type, placeholders);
    }
    // Substitute in body
    auto& body = func.body.get();
    substitute_in_block(body, placeholders);
}

static void substitute_in_block(
    parser::ast::block_expression& block,
    const std::map<std::string, InterpolationValue>& placeholders) {
    // We need a lightweight visitor to substitute in each statement.
    // For now, we handle the common cases that appear in macro templates.
    for (auto& stmt : block.statements) {
        // Recurse into the expression — handled by substitute_placeholders
        // which is a member function, so we use a static helper approach.
        // The block substitution is handled by the top-level parse() method
        // which calls substitute_placeholders on each top-level expression.
        // Body statements are already covered by the recursive walk.
    }
}

/// Visitor that substitutes placeholder identifiers with interpolation values.
struct substitution_visitor {
    const AstQuote* quote;
    const std::map<std::string, InterpolationValue>* placeholders;

    template<typename T>
    void operator()(boost::spirit::x3::forward_ast<T>& node) const {
        (*this)(node.get());
    }

    void operator()(parser::ast::identifier& node) const {
        quote->substitute_in_identifier(
            const_cast<parser::ast::identifier&>(node), *placeholders);
    }

    // Leaf nodes — nothing to substitute
    void operator()(parser::ast::integer_literal&) const {}
    void operator()(parser::ast::float_literal&) const {}
    void operator()(parser::ast::string_literal&) const {}
    void operator()(parser::ast::regex_literal&) const {}
    void operator()(parser::ast::boolean_literal&) const {}

    void operator()(parser::ast::function_definition& node) const {
        quote->substitute_in_function(
            const_cast<parser::ast::function_definition&>(node),
            *placeholders);
    }

    void operator()(parser::ast::val_declaration& node) const {
        quote->substitute_in_identifier(node.name, *placeholders);
        boost::apply_visitor(*this, node.value.get());
    }

    void operator()(parser::ast::var_declaration& node) const {
        quote->substitute_in_identifier(node.name, *placeholders);
        boost::apply_visitor(*this, node.value.get());
    }

    void operator()(parser::ast::binary_operation& node) const {
        boost::apply_visitor(*this, node.left.get());
        boost::apply_visitor(*this, node.right.get());
    }

    void operator()(parser::ast::unary_operation& node) const {
        boost::apply_visitor(*this, node.operand.get());
    }

    void operator()(parser::ast::function_call& node) const {
        quote->substitute_in_identifier(node.function_name, *placeholders);
        for (auto& arg : node.arguments) {
            boost::apply_visitor(*this, arg.get());
        }
    }

    void operator()(parser::ast::list_expression& node) const {
        for (auto& elem : node.elements) {
            boost::apply_visitor(*this, elem.get());
        }
    }

    void operator()(parser::ast::spread_expression& node) const {
        boost::apply_visitor(*this, node.collection.get());
    }

    void operator()(parser::ast::struct_definition& node) const {
        quote->substitute_in_identifier(node.name, *placeholders);
        for (auto& f : node.fields) {
            quote->substitute_in_identifier(f.name, *placeholders);
            quote->substitute_in_type(f.type, *placeholders);
        }
    }

    void operator()(parser::ast::class_definition& node) const {
        quote->substitute_in_identifier(node.name, *placeholders);
        for (auto& f : node.fields) {
            quote->substitute_in_identifier(f.name, *placeholders);
            quote->substitute_in_type(f.type, *placeholders);
        }
    }

    void operator()(parser::ast::return_statement& node) const {
        if (node.has_expression) {
            boost::apply_visitor(*this, node.expr.get());
        }
    }

    void operator()(parser::ast::pipeline_expression& node) const {
        boost::apply_visitor(*this, node.value.get());
        boost::apply_visitor(*this, node.function.get());
    }

    void operator()(parser::ast::safe_navigation_expression& node) const {
        boost::apply_visitor(*this, node.nullable_expr.get());
    }

    void operator()(parser::ast::elvis_expression& node) const {
        boost::apply_visitor(*this, node.nullable_expr.get());
        boost::apply_visitor(*this, node.default_value.get());
    }

    // Remaining variant types — no-op for now (not commonly used in
    // ast.quote templates for getter/setter generation).
    // These can be extended as needed when more complex templates are used.
    void operator()(parser::ast::enum_definition&) const {}
    void operator()(parser::ast::typealias_declaration&) const {}
    void operator()(parser::ast::newtype_declaration&) const {}
    void operator()(parser::ast::initialization_block&) const {}
    void operator()(parser::ast::tuple_literal&) const {}
    void operator()(parser::ast::tuple_indexing&) const {}
    void operator()(parser::ast::array_indexing&) const {}
    void operator()(parser::ast::tuple_destructuring&) const {}
    void operator()(parser::ast::lambda_expression&) const {}
    void operator()(parser::ast::extension_block&) const {}
    void operator()(parser::ast::operator_function&) const {}
    void operator()(parser::ast::custom_operator_definition&) const {}
    void operator()(parser::ast::match_expression&) const {}
    void operator()(parser::ast::query_expression&) const {}
    void operator()(parser::ast::namespace_declaration&) const {}
    void operator()(parser::ast::import_declaration&) const {}
    void operator()(parser::ast::anonymous_object_literal&) const {}
    void operator()(parser::ast::anonymous_array_literal&) const {}
    void operator()(parser::ast::anonymous_tuple_literal&) const {}
    void operator()(parser::ast::effect_definition&) const {}
    void operator()(parser::ast::perform_expression&) const {}
    void operator()(parser::ast::implicit_effect_call&) const {}
    void operator()(parser::ast::handle_expression&) const {}
    void operator()(parser::ast::resume_expression&) const {}
    void operator()(parser::ast::named_return_assignment&) const {}
    void operator()(parser::ast::refinement_type_definition&) const {}
    void operator()(parser::ast::flow_definition&) const {}
    void operator()(parser::ast::old_expression&) const {}
    void operator()(parser::ast::test_block&) const {}
    void operator()(parser::ast::assertion_expression&) const {}
};

void AstQuote::substitute_placeholders(
    parser::ast::expression& expr,
    const std::map<std::string, InterpolationValue>& placeholders) const {
    if (placeholders.empty()) return;
    substitution_visitor visitor{this, &placeholders};
    boost::apply_visitor(visitor, expr);
}

} // namespace meld::macro
