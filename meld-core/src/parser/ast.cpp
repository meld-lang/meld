#include "meld/parser/ast.hpp"
#include "meld/parser/ast_node.hpp"
#include "meld/parser/ast_parent_setter.hpp"
#include <algorithm>
#include <iostream>

namespace meld::parser::ast {

// ─── class_definition member methods (Requirements: 25B.13) ────

void class_definition::add_method(function_definition method) {
    methods.push_back(std::move(method));
    auto& added = methods.back();
    detail::wire_child(&added, this);
    detail::wire_child(&added.name, &added);
    for (auto& p : added.parameters) {
        detail::wire_child(&p, &added);
        detail::wire_child(&p.name, &p);
    }
}

function_definition* class_definition::find_method(const std::string& name) {
    auto it = std::find_if(methods.begin(), methods.end(),
        [&name](const function_definition& m) { return m.name.name == name; });
    return (it != methods.end()) ? &(*it) : nullptr;
}

const function_definition* class_definition::find_method(const std::string& name) const {
    auto it = std::find_if(methods.begin(), methods.end(),
        [&name](const function_definition& m) { return m.name.name == name; });
    return (it != methods.end()) ? &(*it) : nullptr;
}

bool class_definition::has_method(const std::string& name) const {
    return find_method(name) != nullptr;
}

// ─── struct_definition member methods (Requirements: 25B.13) ───

void struct_definition::add_method(function_definition method) {
    methods.push_back(std::move(method));
    auto& added = methods.back();
    detail::wire_child(&added, this);
    detail::wire_child(&added.name, &added);
    for (auto& p : added.parameters) {
        detail::wire_child(&p, &added);
        detail::wire_child(&p.name, &p);
    }
}

function_definition* struct_definition::find_method(const std::string& name) {
    auto it = std::find_if(methods.begin(), methods.end(),
        [&name](const function_definition& m) { return m.name.name == name; });
    return (it != methods.end()) ? &(*it) : nullptr;
}

const function_definition* struct_definition::find_method(const std::string& name) const {
    auto it = std::find_if(methods.begin(), methods.end(),
        [&name](const function_definition& m) { return m.name.name == name; });
    return (it != methods.end()) ? &(*it) : nullptr;
}

bool struct_definition::has_method(const std::string& name) const {
    return find_method(name) != nullptr;
}

// ─── type_definition member methods ───

void type_definition::add_method(function_definition method) {
    methods.push_back(std::move(method));
    auto& added = methods.back();
    detail::wire_child(&added, this);
    detail::wire_child(&added.name, &added);
    for (auto& p : added.parameters) {
        detail::wire_child(&p, &added);
        detail::wire_child(&p.name, &p);
    }
}

function_definition* type_definition::find_method(const std::string& name) {
    auto it = std::find_if(methods.begin(), methods.end(),
        [&name](const function_definition& m) { return m.name.name == name; });
    return (it != methods.end()) ? &(*it) : nullptr;
}

const function_definition* type_definition::find_method(const std::string& name) const {
    auto it = std::find_if(methods.begin(), methods.end(),
        [&name](const function_definition& m) { return m.name.name == name; });
    return (it != methods.end()) ? &(*it) : nullptr;
}

bool type_definition::has_method(const std::string& name) const {
    return find_method(name) != nullptr;
}

// AST visitor for printing
// The expression variant stores raw types for simple literals (identifier, integer_literal, etc.)
// and x3::forward_ast<T> for complex types. We handle both via overloads and a catch-all.
struct ast_printer {
    using result_type = void;

    // Catch-all for any forward_ast<T> — unwrap and print type name
    template<typename T>
    void operator()(const x3::forward_ast<T>& node) const {
        (*this)(node.get());
    }

    // Catch-all for unhandled concrete types
    template<typename T>
    void operator()(const T&) const {
        std::cout << "<node>";
    }

    void operator()(const identifier& id) const {
        std::cout << "Identifier: " << id.name;
    }
    
    void operator()(const integer_literal& lit) const {
        std::cout << "Integer: " << lit.value;
        if (!lit.suffix.empty()) std::cout << lit.suffix;
    }
    
    void operator()(const float_literal& lit) const {
        std::cout << "Float: " << lit.value;
        if (!lit.suffix.empty()) std::cout << lit.suffix;
    }
    
    void operator()(const string_literal& lit) const {
        if (lit.is_multiline) {
            std::cout << "MultilineString: \"\"\"" << lit.value << "\"\"\"";
        } else {
            std::cout << "String: \"" << lit.value << "\"";
        }
        if (lit.has_interpolation) std::cout << " (with interpolation)";
        if (lit.has_modifier_interpolation) std::cout << " (with modifier interpolation)";
        if (lit.is_template) std::cout << " (template)";
    }
    
    void operator()(const regex_literal& lit) const {
        std::cout << "Regex: /" << lit.pattern << "/" << lit.flags;
    }
    
    void operator()(const boolean_literal& lit) const {
        std::cout << "Boolean: " << (lit.value ? "true" : "false");
    }
    
    void operator()(const list_expression& list) const {
        std::cout << "List[" << list.elements.size() << "]";
    }
    
    void operator()(const anonymous_array_literal& array) const {
        std::cout << "Array[" << array.elements.size() << "]";
    }
    
    void operator()(const anonymous_tuple_literal& tuple) const {
        std::cout << "Tuple[" << tuple.elements.size() << "]";
    }
    
    void operator()(const function_call& call) const {
        std::cout << "Call: " << call.function_name.name;
    }
    
    void operator()(const val_declaration& decl) const {
        std::cout << "val " << decl.name.name;
    }
    
    void operator()(const var_declaration& decl) const {
        std::cout << "var " << decl.name.name;
    }
    
    void operator()(const binary_operation& op) const {
        std::cout << "BinOp(" << op.op << ")";
    }
    
    void operator()(const unary_operation& op) const {
        std::cout << "UnaryOp(" << op.op << ")";
    }
    
    void operator()(const struct_definition& def) const {
        std::cout << "struct " << def.name.name;
    }
    
    void operator()(const class_definition& def) const {
        std::cout << "class " << def.name.name;
    }
    
    void operator()(const enum_definition& def) const {
        std::cout << "enum " << def.name.name;
    }
    
    void operator()(const function_definition& def) const {
        std::cout << "fn " << def.name.name;
    }
    
    void operator()(const pipeline_expression&) const {
        std::cout << "Pipeline";
    }
    
    void operator()(const match_expression&) const {
        std::cout << "Match";
    }
    
    void operator()(const return_statement&) const {
        std::cout << "rtn";
    }
    
    void operator()(const named_return_assignment& assignment) const {
        std::cout << assignment.return_name.name << " = ...";
    }
    
    void operator()(const perform_expression& expr) const {
        std::cout << "Perform: " << expr.effect_name.name << "." << expr.operation_name.name;
        if (!expr.arguments.empty()) {
            std::cout << "(";
            for (size_t i = 0; i < expr.arguments.size(); ++i) {
                if (i > 0) std::cout << ", ";
                boost::apply_visitor(*this, expr.arguments[i].get());
            }
            std::cout << ")";
        }
    }

    void operator()(const implicit_effect_call& call) const {
        std::cout << "ImplicitEffect: " << call.effect_name.name << "." << call.operation_name.name;
        if (!call.arguments.empty()) {
            std::cout << "(";
            for (size_t i = 0; i < call.arguments.size(); ++i) {
                if (i > 0) std::cout << ", ";
                boost::apply_visitor(*this, call.arguments[i].get());
            }
            std::cout << ")";
        }
    }
};

void print_ast(const expression& expr) {
    boost::apply_visitor(ast_printer{}, expr);
}

} // namespace meld::parser::ast
