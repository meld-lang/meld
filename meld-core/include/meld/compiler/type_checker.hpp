#pragma once

#include "meld/parser/ast.hpp"
#include "meld/meta/metatype.hpp"
#include "meld/compiler/contract_checker.hpp"
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <expected>
#include <optional>

namespace meld::compiler {

// Type error information
struct TypeError {
    std::string message;
    std::string file;
    size_t line;
    size_t column;
    std::string context;  // Additional context for the error
    
    TypeError(std::string msg, std::string f = "", size_t l = 0, size_t c = 0, std::string ctx = "")
        : message(std::move(msg))
        , file(std::move(f))
        , line(l)
        , column(c)
        , context(std::move(ctx)) {}
    
    std::string format() const;
};

// Type environment for tracking variable types in scope
class TypeEnvironment : public std::enable_shared_from_this<TypeEnvironment> {
public:
    TypeEnvironment() = default;
    explicit TypeEnvironment(std::shared_ptr<TypeEnvironment> parent)
        : parent_(std::move(parent)) {}
    
    // Variable type tracking
    void bind(const std::string& name, std::shared_ptr<meta::MetaType> type);
    std::optional<std::shared_ptr<meta::MetaType>> lookup(const std::string& name) const;
    
    // Create child scope
    std::shared_ptr<TypeEnvironment> create_child() const;
    
    // Check if variable exists in current scope (not parent)
    bool has_local(const std::string& name) const;
    
private:
    std::map<std::string, std::shared_ptr<meta::MetaType>> bindings_;
    std::shared_ptr<TypeEnvironment> parent_;
};

// Type checker - performs type inference and checking
class TypeChecker {
public:
    TypeChecker();
    
    // Check an expression and return its type
    std::expected<std::shared_ptr<meta::MetaType>, TypeError>
    check_expression(const parser::ast::expression& expr, 
                    std::shared_ptr<TypeEnvironment> env);
    
    // Check a program (list of expressions)
    std::expected<std::vector<std::shared_ptr<meta::MetaType>>, std::vector<TypeError>>
    check_program(const std::vector<parser::ast::expression>& expressions);
    
    // Type inference
    std::expected<std::shared_ptr<meta::MetaType>, TypeError>
    infer_type(const parser::ast::expression& expr,
              std::shared_ptr<TypeEnvironment> env);
    
    // Type compatibility checking
    bool is_compatible(const meta::MetaType& expected, const meta::MetaType& actual) const;
    bool is_assignable(const meta::MetaType& target, const meta::MetaType& source) const;
    
    // Generic type instantiation
    std::expected<std::shared_ptr<meta::MetaType>, TypeError>
    instantiate_generic(const meta::GenericMetaType& generic,
                       std::shared_ptr<meta::MetaType> concrete_type);
    
    // Generic type instantiation with multiple type arguments
    std::expected<std::shared_ptr<meta::MetaType>, TypeError>
    instantiate_generic_type(
        std::shared_ptr<meta::MetaType> generic_type,
        const std::vector<std::shared_ptr<meta::MetaType>>& type_arguments);
    
    // Type substitution - replace type parameters with concrete types
    std::shared_ptr<meta::MetaType> substitute_type_parameters(
        std::shared_ptr<meta::MetaType> type,
        const std::map<std::string, std::shared_ptr<meta::MetaType>>& substitutions);
    
    // Check type parameter bounds
    std::expected<void, TypeError> check_type_parameter_bounds(
        const std::vector<parser::ast::generic_type_parameter>& type_params,
        const std::vector<std::shared_ptr<meta::MetaType>>& type_arguments);
    
    // Unification for type inference
    std::expected<std::shared_ptr<meta::MetaType>, TypeError>
    unify(std::shared_ptr<meta::MetaType> t1, std::shared_ptr<meta::MetaType> t2);
    
    // Get collected errors
    const std::vector<TypeError>& errors() const { return errors_; }
    bool has_errors() const { return !errors_.empty(); }
    
private:
    // Type checking for specific AST nodes
    std::expected<std::shared_ptr<meta::MetaType>, TypeError>
    check_identifier(const parser::ast::identifier& id,
                    std::shared_ptr<TypeEnvironment> env);
    
    std::expected<std::shared_ptr<meta::MetaType>, TypeError>
    check_literal(const parser::ast::expression& expr);
    
    std::expected<std::shared_ptr<meta::MetaType>, TypeError>
    check_binary_operation(const parser::ast::binary_operation& op,
                          std::shared_ptr<TypeEnvironment> env);
    
    std::expected<std::shared_ptr<meta::MetaType>, TypeError>
    check_unary_operation(const parser::ast::unary_operation& op,
                         std::shared_ptr<TypeEnvironment> env);
    
    std::expected<std::shared_ptr<meta::MetaType>, TypeError>
    check_function_call(const parser::ast::function_call& call,
                       std::shared_ptr<TypeEnvironment> env);
    
    std::expected<std::shared_ptr<meta::MetaType>, TypeError>
    check_val_declaration(const parser::ast::val_declaration& decl,
                         std::shared_ptr<TypeEnvironment> env);
    
    std::expected<std::shared_ptr<meta::MetaType>, TypeError>
    check_var_declaration(const parser::ast::var_declaration& decl,
                         std::shared_ptr<TypeEnvironment> env);
    
    std::expected<std::shared_ptr<meta::MetaType>, TypeError>
    check_struct_definition(const parser::ast::struct_definition& def,
                           std::shared_ptr<TypeEnvironment> env);
    
    std::expected<std::shared_ptr<meta::MetaType>, TypeError>
    check_class_definition(const parser::ast::class_definition& def,
                          std::shared_ptr<TypeEnvironment> env);
    
    std::expected<std::shared_ptr<meta::MetaType>, TypeError>
    check_enum_definition(const parser::ast::enum_definition& def,
                         std::shared_ptr<TypeEnvironment> env);
    
    std::expected<std::shared_ptr<meta::MetaType>, TypeError>
    check_function_definition(const parser::ast::function_definition& def,
                             std::shared_ptr<TypeEnvironment> env);
    
    std::expected<std::shared_ptr<meta::MetaType>, TypeError>
    check_lambda_expression(const parser::ast::lambda_expression& lambda,
                           std::shared_ptr<TypeEnvironment> env);
    
    std::expected<std::shared_ptr<meta::MetaType>, TypeError>
    check_tuple_literal(const parser::ast::tuple_literal& tuple,
                       std::shared_ptr<TypeEnvironment> env);
    
    std::expected<std::shared_ptr<meta::MetaType>, TypeError>
    check_anonymous_array_literal(const parser::ast::anonymous_array_literal& array,
                                  std::shared_ptr<TypeEnvironment> env);
    
    std::expected<std::shared_ptr<meta::MetaType>, TypeError>
    check_array_indexing(const parser::ast::array_indexing& indexing,
                        std::shared_ptr<TypeEnvironment> env);
    
    std::expected<std::shared_ptr<meta::MetaType>, TypeError>
    check_tuple_indexing(const parser::ast::tuple_indexing& indexing,
                        std::shared_ptr<TypeEnvironment> env);
    
    std::expected<std::shared_ptr<meta::MetaType>, TypeError>
    check_anonymous_tuple_literal(const parser::ast::anonymous_tuple_literal& tuple,
                                 std::shared_ptr<TypeEnvironment> env);
    
    std::expected<std::shared_ptr<meta::MetaType>, TypeError>
    check_extension_block(const parser::ast::extension_block& ext,
                         std::shared_ptr<TypeEnvironment> env);
    
    std::expected<std::shared_ptr<meta::MetaType>, TypeError>
    check_pipeline_expression(const parser::ast::pipeline_expression& pipe,
                             std::shared_ptr<TypeEnvironment> env);
    
    std::expected<std::shared_ptr<meta::MetaType>, TypeError>
    check_match_expression(const parser::ast::match_expression& match,
                          std::shared_ptr<TypeEnvironment> env);
    
    std::expected<std::shared_ptr<meta::MetaType>, TypeError>
    check_initialization_block(const parser::ast::initialization_block& init,
                              std::shared_ptr<TypeEnvironment> env);
    
    std::expected<std::shared_ptr<meta::MetaType>, TypeError>
    check_typealias_declaration(const parser::ast::typealias_declaration& decl,
                               std::shared_ptr<TypeEnvironment> env);
    
    std::expected<std::shared_ptr<meta::MetaType>, TypeError>
    check_newtype_declaration(const parser::ast::newtype_declaration& decl,
                             std::shared_ptr<TypeEnvironment> env);
    
    std::expected<std::shared_ptr<meta::MetaType>, TypeError>
    check_namespace_declaration(const parser::ast::namespace_declaration& decl,
                               std::shared_ptr<TypeEnvironment> env);
    
    std::expected<std::shared_ptr<meta::MetaType>, TypeError>
    check_import_declaration(const parser::ast::import_declaration& decl,
                            std::shared_ptr<TypeEnvironment> env);
    
    std::expected<std::shared_ptr<meta::MetaType>, TypeError>
    check_safe_navigation(const parser::ast::safe_navigation_expression& expr,
                         std::shared_ptr<TypeEnvironment> env);
    
    std::expected<std::shared_ptr<meta::MetaType>, TypeError>
    check_elvis_expression(const parser::ast::elvis_expression& expr,
                          std::shared_ptr<TypeEnvironment> env);
    
    // Helper methods
    std::shared_ptr<meta::MetaType> resolve_type_annotation(
        const parser::ast::type_annotation& annotation);
    
    TypeError make_error(const std::string& message,
                        const std::string& context = "") const;
    
    void add_error(const TypeError& error);
    
    // Type registry access
    meta::TypeRegistry& registry_;
    
    // Error collection
    std::vector<TypeError> errors_;
    
    // Current file being checked (for error reporting)
    std::string current_file_;
};

} // namespace meld::compiler
