#pragma once

#include "ast.hpp"
#include "ast_node.hpp"

namespace meld::parser::ast {

/**
 * AST Parent Pointer Setter
 *
 * Traverses the AST and registers parent-child relationships in
 * ASTParentMap. This should be called after parsing to establish
 * the parent pointers needed for bottom-up traversal in macros.
 *
 * Usage:
 *   auto expressions = parser.parse(source);
 *   set_parent_pointers(expressions);
 *
 * After calling this, any node's parent can be looked up via:
 *   auto& map = ASTParentMap::instance();
 *   auto* parent = map.parent_as<class_definition>(&field_node);
 *
 * Requirements: 2.7, 2.8
 */

// Forward declaration
void set_parent_pointers(expression& expr, const void* parent = nullptr);

namespace detail {

// Helper to register a child's parent in the map
inline void wire_child(ASTNode* child, const void* parent) {
    child->set_parent(parent);
}

// Set parent pointers on a vector of expressions
inline void wire_expressions(std::vector<x3::forward_ast<expression>>& exprs, const void* parent) {
    for (auto& e : exprs) {
        set_parent_pointers(e.get(), parent);
    }
}

} // namespace detail

/**
 * Visitor that recursively sets parent pointers on all AST nodes.
 */
struct parent_setter_visitor {
    const void* parent;

    explicit parent_setter_visitor(const void* p) : parent(p) {}

    // Helper for forward_ast wrapped expressions
    template<typename T>
    void operator()(x3::forward_ast<T>& node) const {
        (*this)(node.get());
    }

    // --- Leaf nodes (no children to recurse into) ---
    void operator()(identifier& node) const {
        detail::wire_child(&node, parent);
    }

    void operator()(integer_literal& node) const {
        detail::wire_child(&node, parent);
    }

    void operator()(float_literal& node) const {
        detail::wire_child(&node, parent);
    }

    void operator()(string_literal& node) const {
        detail::wire_child(&node, parent);
    }

    void operator()(regex_literal& node) const {
        detail::wire_child(&node, parent);
    }

    void operator()(boolean_literal& node) const {
        detail::wire_child(&node, parent);
    }

    // --- Composite nodes ---

    void operator()(list_expression& node) const {
        detail::wire_child(&node, parent);
        detail::wire_expressions(node.elements, &node);
    }

    void operator()(spread_expression& node) const {
        detail::wire_child(&node, parent);
        set_parent_pointers(node.collection.get(), &node);
    }

    void operator()(function_call& node) const {
        detail::wire_child(&node, parent);
        detail::wire_child(&node.function_name, &node);
        detail::wire_expressions(node.arguments, &node);
        for (auto& na : node.named_arguments) {
            detail::wire_child(&na, &node);
            detail::wire_child(&na.name, &na);
            set_parent_pointers(na.value.get(), &na);
        }
    }

    void operator()(val_declaration& node) const {
        detail::wire_child(&node, parent);
        detail::wire_child(&node.name, &node);
        set_parent_pointers(node.value.get(), &node);
    }

    void operator()(var_declaration& node) const {
        detail::wire_child(&node, parent);
        detail::wire_child(&node.name, &node);
        set_parent_pointers(node.value.get(), &node);
    }

    void operator()(binary_operation& node) const {
        detail::wire_child(&node, parent);
        set_parent_pointers(node.left.get(), &node);
        set_parent_pointers(node.right.get(), &node);
    }

    void operator()(unary_operation& node) const {
        detail::wire_child(&node, parent);
        set_parent_pointers(node.operand.get(), &node);
    }

    void operator()(struct_definition& node) const {
        detail::wire_child(&node, parent);
        detail::wire_child(&node.name, &node);
        for (auto& f : node.fields) {
            detail::wire_child(&f, &node);
            detail::wire_child(&f.name, &f);
        }
        // Wire macro-injected methods
        for (auto& m : node.methods) {
            detail::wire_child(&m, &node);
            detail::wire_child(&m.name, &m);
            for (auto& p : m.parameters) {
                detail::wire_child(&p, &m);
                detail::wire_child(&p.name, &p);
            }
        }
    }

    void operator()(class_definition& node) const {
        detail::wire_child(&node, parent);
        detail::wire_child(&node.name, &node);
        for (auto& f : node.fields) {
            detail::wire_child(&f, &node);
            detail::wire_child(&f.name, &f);
        }
        // Wire macro-injected methods
        for (auto& m : node.methods) {
            detail::wire_child(&m, &node);
            detail::wire_child(&m.name, &m);
            for (auto& p : m.parameters) {
                detail::wire_child(&p, &m);
                detail::wire_child(&p.name, &p);
            }
        }
    }

    void operator()(enum_definition& node) const {
        detail::wire_child(&node, parent);
        detail::wire_child(&node.name, &node);
        for (auto& v : node.variants) {
            detail::wire_child(&v, &node);
            detail::wire_child(&v.name, &v);
            for (auto& f : v.associated_fields) {
                detail::wire_child(&f, &v);
            }
        }
    }

    void operator()(type_definition& node) const {
        detail::wire_child(&node, parent);
        detail::wire_child(&node.kind, &node);
        detail::wire_child(&node.name, &node);
        for (auto& f : node.fields) {
            detail::wire_child(&f, &node);
            detail::wire_child(&f.name, &f);
        }
        for (auto& v : node.variants) {
            detail::wire_child(&v, &node);
            detail::wire_child(&v.name, &v);
            for (auto& f : v.associated_fields) {
                detail::wire_child(&f, &v);
            }
        }
        for (auto& m : node.methods) {
            detail::wire_child(&m, &node);
            detail::wire_child(&m.name, &m);
            for (auto& p : m.parameters) {
                detail::wire_child(&p, &m);
                detail::wire_child(&p.name, &p);
            }
        }
    }

    void operator()(function_definition& node) const {
        detail::wire_child(&node, parent);
        detail::wire_child(&node.name, &node);
        for (auto& p : node.parameters) {
            detail::wire_child(&p, &node);
            detail::wire_child(&p.name, &p);
        }
        auto& body = node.body.get();
        detail::wire_child(&body, &node);
        detail::wire_expressions(body.statements, &body);
    }

    void operator()(lambda_expression& node) const {
        detail::wire_child(&node, parent);
        for (auto& p : node.parameters) {
            detail::wire_child(&p, &node);
        }
        set_parent_pointers(node.body.get(), &node);
    }

    void operator()(extension_block& node) const {
        detail::wire_child(&node, parent);
        for (auto& m : node.methods) {
            detail::wire_child(&m, &node);
            detail::wire_child(&m.name, &m);
        }
    }

    void operator()(pipeline_expression& node) const {
        detail::wire_child(&node, parent);
        set_parent_pointers(node.value.get(), &node);
        set_parent_pointers(node.function.get(), &node);
    }

    void operator()(tuple_literal& node) const {
        detail::wire_child(&node, parent);
        for (auto& e : node.elements) {
            detail::wire_child(&e, &node);
            set_parent_pointers(e.value.get(), &e);
        }
    }

    void operator()(tuple_indexing& node) const {
        detail::wire_child(&node, parent);
        set_parent_pointers(node.tuple.get(), &node);
    }

    void operator()(array_indexing& node) const {
        detail::wire_child(&node, parent);
        set_parent_pointers(node.array.get(), &node);
        set_parent_pointers(node.index.get(), &node);
    }

    void operator()(safe_navigation_expression& node) const {
        detail::wire_child(&node, parent);
        set_parent_pointers(node.nullable_expr.get(), &node);
    }

    void operator()(elvis_expression& node) const {
        detail::wire_child(&node, parent);
        set_parent_pointers(node.nullable_expr.get(), &node);
        set_parent_pointers(node.default_value.get(), &node);
    }

    void operator()(tuple_destructuring& node) const {
        detail::wire_child(&node, parent);
        for (auto& b : node.bindings) {
            detail::wire_child(&b, &node);
        }
        set_parent_pointers(node.tuple_expr.get(), &node);
    }

    void operator()(match_expression& node) const {
        detail::wire_child(&node, parent);
        set_parent_pointers(node.matched_value.get(), &node);
        for (auto& c : node.cases) {
            detail::wire_child(&c, &node);
            set_parent_pointers(c.result_expression.get(), &c);
        }
    }

    void operator()(namespace_declaration& node) const {
        detail::wire_child(&node, parent);
        detail::wire_expressions(node.body, &node);
    }

    void operator()(import_declaration& node) const {
        detail::wire_child(&node, parent);
    }

    void operator()(anonymous_object_literal& node) const {
        detail::wire_child(&node, parent);
        for (auto& f : node.fields) {
            detail::wire_child(&f, &node);
            detail::wire_child(&f.field_name, &f);
            set_parent_pointers(f.value.get(), &f);
        }
    }

    void operator()(anonymous_array_literal& node) const {
        detail::wire_child(&node, parent);
        detail::wire_expressions(node.elements, &node);
    }

    void operator()(anonymous_tuple_literal& node) const {
        detail::wire_child(&node, parent);
        for (auto& e : node.elements) {
            detail::wire_child(&e, &node);
            set_parent_pointers(e.value.get(), &e);
        }
    }

    void operator()(effect_definition& node) const {
        detail::wire_child(&node, parent);
        detail::wire_child(&node.name, &node);
        for (auto& op : node.operations) {
            detail::wire_child(&op, &node);
            detail::wire_child(&op.name, &op);
        }
    }

    void operator()(perform_expression& node) const {
        detail::wire_child(&node, parent);
        detail::wire_child(&node.effect_name, &node);
        detail::wire_child(&node.operation_name, &node);
        detail::wire_expressions(node.arguments, &node);
    }

    void operator()(implicit_effect_call& node) const {
        detail::wire_child(&node, parent);
        detail::wire_child(&node.effect_name, &node);
        detail::wire_child(&node.operation_name, &node);
        detail::wire_expressions(node.arguments, &node);
    }

    void operator()(handle_expression& node) const {
        detail::wire_child(&node, parent);
        auto& body = node.body.get();
        detail::wire_child(&body, &node);
        detail::wire_expressions(body.statements, &body);
        for (auto& h : node.handlers) {
            detail::wire_child(&h, &node);
            detail::wire_child(&h.trait_name, &h);
            for (auto& m : h.methods) {
                detail::wire_child(&m, &h);
                detail::wire_child(&m.name, &m);
            }
        }
    }

    void operator()(resume_expression& node) const {
        detail::wire_child(&node, parent);
        if (node.has_value) {
            set_parent_pointers(node.value.get(), &node);
        }
    }

    void operator()(return_statement& node) const {
        detail::wire_child(&node, parent);
        if (node.has_expression) {
            set_parent_pointers(node.expr.get(), &node);
        }
        for (auto& nr : node.named_returns) {
            detail::wire_child(&nr, &node);
        }
        detail::wire_expressions(node.tuple_values, &node);
    }

    void operator()(named_return_assignment& node) const {
        detail::wire_child(&node, parent);
        detail::wire_child(&node.return_name, &node);
        set_parent_pointers(node.value.get(), &node);
    }

    void operator()(refinement_type_definition& node) const {
        detail::wire_child(&node, parent);
        detail::wire_child(&node.name, &node);
        set_parent_pointers(node.predicate.get(), &node);
    }

    void operator()(flow_definition& node) const {
        detail::wire_child(&node, parent);
        detail::wire_child(&node.name, &node);
        for (auto& s : node.states) {
            detail::wire_child(&s, &node);
            detail::wire_child(&s.name, &s);
        }
    }

    void operator()(old_expression& node) const {
        detail::wire_child(&node, parent);
        set_parent_pointers(node.expression.get(), &node);
    }

    void operator()(test_block& node) const {
        detail::wire_child(&node, parent);
    }

    void operator()(assertion_expression& node) const {
        detail::wire_child(&node, parent);
        set_parent_pointers(node.condition.get(), &node);
    }

    // Remaining types that need basic wiring
    void operator()(typealias_declaration& node) const {
        detail::wire_child(&node, parent);
        detail::wire_child(&node.alias_name, &node);
    }

    void operator()(newtype_declaration& node) const {
        detail::wire_child(&node, parent);
        detail::wire_child(&node.wrapper_name, &node);
    }

    void operator()(initialization_block& node) const {
        detail::wire_child(&node, parent);
        detail::wire_child(&node.type_name, &node);
        for (auto& p : node.parameters) {
            detail::wire_child(&p, &node);
            detail::wire_child(&p.name, &p);
            set_parent_pointers(p.value.get(), &p);
        }
    }

    void operator()(operator_function& node) const {
        detail::wire_child(&node, parent);
        for (auto& p : node.parameters) {
            detail::wire_child(&p, &node);
        }
    }

    void operator()(custom_operator_definition& node) const {
        detail::wire_child(&node, parent);
        for (auto& p : node.parameters) {
            detail::wire_child(&p, &node);
        }
    }

    void operator()(query_expression& node) const {
        detail::wire_child(&node, parent);
    }
};

/**
 * Set parent pointers on an expression and all its descendants.
 * @param expr The expression to process
 * @param parent The parent node (nullptr for root expressions)
 */
inline void set_parent_pointers(expression& expr, const void* parent) {
    parent_setter_visitor visitor{parent};
    boost::apply_visitor(visitor, expr);
}

/**
 * Set parent pointers on a vector of top-level expressions.
 * Clears the parent map first, then rebuilds all relationships.
 * @param expressions The expressions to process
 */
inline void set_parent_pointers(std::vector<expression>& expressions) {
    ASTParentMap::instance().clear();
    for (auto& expr : expressions) {
        set_parent_pointers(expr, nullptr);
    }
}

} // namespace meld::parser::ast
