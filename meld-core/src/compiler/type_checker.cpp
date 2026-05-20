#include "meld/compiler/type_checker.hpp"
#include "meld/types/anonymous_types.hpp"
#include "meld/kernel/namespace_registry.hpp"
#include "meld/kernel/extension_registry.hpp"
#include "meld/compat/visit.hpp"
#include <sstream>
#include <algorithm>

namespace meld::compiler {

// TypeError implementation
std::string TypeError::format() const {
    std::ostringstream oss;
    if (!file.empty()) {
        oss << file << ":";
    }
    if (line > 0) {
        oss << line << ":" << column << ": ";
    }
    oss << "error: " << message;
    if (!context.empty()) {
        oss << "\n  " << context;
    }
    return oss.str();
}

// TypeEnvironment implementation
void TypeEnvironment::bind(const std::string& name, std::shared_ptr<meta::MetaType> type) {
    bindings_[name] = std::move(type);
}

std::optional<std::shared_ptr<meta::MetaType>> 
TypeEnvironment::lookup(const std::string& name) const {
    auto it = bindings_.find(name);
    if (it != bindings_.end()) {
        return it->second;
    }
    if (parent_) {
        return parent_->lookup(name);
    }
    return std::nullopt;
}

std::shared_ptr<TypeEnvironment> TypeEnvironment::create_child() const {
    return std::make_shared<TypeEnvironment>(
        std::const_pointer_cast<TypeEnvironment>(shared_from_this())
    );
}

bool TypeEnvironment::has_local(const std::string& name) const {
    return bindings_.find(name) != bindings_.end();
}

// TypeChecker implementation
TypeChecker::TypeChecker()
    : registry_(meta::TypeRegistry::instance()) {
}

std::expected<std::shared_ptr<meta::MetaType>, TypeError>
TypeChecker::check_expression(const parser::ast::expression& expr,
                              std::shared_ptr<TypeEnvironment> env) {
    // Use meld::compat::visit with a generic lambda — MSVC forbids template
    // members in local classes (C2892) but generic lambdas are fine.
    using R = std::expected<std::shared_ptr<meta::MetaType>, TypeError>;
    return meld::compat::visit<R>([&](auto const& node) -> R {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, parser::ast::identifier>) {
            return check_identifier(node, env);
        }
        else if constexpr (std::is_same_v<T, parser::ast::integer_literal> ||
                          std::is_same_v<T, parser::ast::float_literal> ||
                          std::is_same_v<T, parser::ast::string_literal> ||
                          std::is_same_v<T, parser::ast::boolean_literal>) {
            return check_literal(expr);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::binary_operation>>) {
            return check_binary_operation(node.get(), env);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::unary_operation>>) {
            return check_unary_operation(node.get(), env);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::function_call>>) {
            return check_function_call(node.get(), env);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::val_declaration>>) {
            return check_val_declaration(node.get(), env);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::var_declaration>>) {
            return check_var_declaration(node.get(), env);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::struct_definition>>) {
            return check_struct_definition(node.get(), env);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::class_definition>>) {
            return check_class_definition(node.get(), env);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::enum_definition>>) {
            return check_enum_definition(node.get(), env);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::function_definition>>) {
            return check_function_definition(node.get(), env);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::lambda_expression>>) {
            return check_lambda_expression(node.get(), env);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::tuple_literal>>) {
            return check_tuple_literal(node.get(), env);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::namespace_declaration>>) {
            return check_namespace_declaration(node.get(), env);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::import_declaration>>) {
            return check_import_declaration(node.get(), env);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::anonymous_array_literal>>) {
            return check_anonymous_array_literal(node.get(), env);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::array_indexing>>) {
            return check_array_indexing(node.get(), env);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::anonymous_tuple_literal>>) {
            return check_anonymous_tuple_literal(node.get(), env);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::tuple_indexing>>) {
            return check_tuple_indexing(node.get(), env);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::extension_block>>) {
            return check_extension_block(node.get(), env);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::pipeline_expression>>) {
            return check_pipeline_expression(node.get(), env);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::match_expression>>) {
            return check_match_expression(node.get(), env);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::initialization_block>>) {
            return check_initialization_block(node.get(), env);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::typealias_declaration>>) {
            return check_typealias_declaration(node.get(), env);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::newtype_declaration>>) {
            return check_newtype_declaration(node.get(), env);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::safe_navigation_expression>>) {
            return check_safe_navigation(node.get(), env);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::elvis_expression>>) {
            return check_elvis_expression(node.get(), env);
        }
        else {
            return std::unexpected(make_error("Unsupported expression type in type checker"));
        }
    }, expr);
}

std::expected<std::vector<std::shared_ptr<meta::MetaType>>, std::vector<TypeError>>
TypeChecker::check_program(const std::vector<parser::ast::expression>& expressions) {
    auto env = std::make_shared<TypeEnvironment>();
    std::vector<std::shared_ptr<meta::MetaType>> types;
    std::vector<TypeError> all_errors;
    
    for (const auto& expr : expressions) {
        auto result = check_expression(expr, env);
        if (result) {
            types.push_back(*result);
        } else {
            all_errors.push_back(result.error());
        }
    }
    
    if (!all_errors.empty()) {
        return std::unexpected(all_errors);
    }
    
    return types;
}

std::expected<std::shared_ptr<meta::MetaType>, TypeError>
TypeChecker::infer_type(const parser::ast::expression& expr,
                       std::shared_ptr<TypeEnvironment> env) {
    // Type inference is the same as type checking for now
    // In a more sophisticated system, we'd have constraint generation and solving
    return check_expression(expr, env);
}

bool TypeChecker::is_compatible(const meta::MetaType& expected, 
                                const meta::MetaType& actual) const {
    // Check if actual can be used where expected is required
    return expected.is_assignable_from(actual);
}

bool TypeChecker::is_assignable(const meta::MetaType& target,
                               const meta::MetaType& source) const {
    return target.is_assignable_from(source);
}

std::expected<std::shared_ptr<meta::MetaType>, TypeError>
TypeChecker::instantiate_generic(const meta::GenericMetaType& generic,
                                std::shared_ptr<meta::MetaType> concrete_type) {
    // Check bounds
    if (generic.bound()) {
        if (!concrete_type->is_subtype_of(*generic.bound())) {
            return std::unexpected(make_error(
                "Type '" + concrete_type->name() + "' does not satisfy bound '" + 
                generic.bound()->name() + "' for generic parameter '" + generic.name() + "'"
            ));
        }
    }
    
    // Check variance
    // For now, we just return the concrete type
    // In a full implementation, we'd need to track variance constraints
    return concrete_type;
}

std::expected<std::shared_ptr<meta::MetaType>, TypeError>
TypeChecker::unify(std::shared_ptr<meta::MetaType> t1, 
                  std::shared_ptr<meta::MetaType> t2) {
    // Simple unification - check if types are compatible
    if (t1->is_assignable_from(*t2)) {
        return t1;
    }
    if (t2->is_assignable_from(*t1)) {
        return t2;
    }
    
    // Try to find common supertype
    // For now, just fail
    return std::unexpected(make_error(
        "Cannot unify types '" + t1->name() + "' and '" + t2->name() + "'"
    ));
}

// Private helper methods

std::expected<std::shared_ptr<meta::MetaType>, TypeError>
TypeChecker::check_identifier(const parser::ast::identifier& id,
                              std::shared_ptr<TypeEnvironment> env) {
    // nil literal returns the null type (Req 14A-NIL)
    if (id.name == "nil") {
        return registry_.get_null_type();
    }
    
    auto type = env->lookup(id.name);
    if (!type) {
        return std::unexpected(make_error(
            "Undefined variable '" + id.name + "'",
            "Variable must be declared before use"
        ));
    }
    return *type;
}

std::expected<std::shared_ptr<meta::MetaType>, TypeError>
TypeChecker::check_literal(const parser::ast::expression& expr) {
    using R = std::expected<std::shared_ptr<meta::MetaType>, TypeError>;
    return meld::compat::visit<R>([&](auto const& node) -> R {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, parser::ast::integer_literal>) {
            return registry_.get_int_type();
        }
        else if constexpr (std::is_same_v<T, parser::ast::float_literal>) {
            // Could distinguish float vs double based on suffix
            return registry_.get_type("float").value_or(registry_.get_int_type());
        }
        else if constexpr (std::is_same_v<T, parser::ast::string_literal>) {
            return registry_.get_string_type();
        }
        else if constexpr (std::is_same_v<T, parser::ast::boolean_literal>) {
            return registry_.get_bool_type();
        }
        else {
            return std::unexpected(make_error("Unknown literal type"));
        }
    }, expr);
}

std::expected<std::shared_ptr<meta::MetaType>, TypeError>
TypeChecker::check_binary_operation(const parser::ast::binary_operation& op,
                                    std::shared_ptr<TypeEnvironment> env) {
    auto left_type = check_expression(op.left, env);
    if (!left_type) return std::unexpected(left_type.error());
    
    auto right_type = check_expression(op.right, env);
    if (!right_type) return std::unexpected(right_type.error());
    
    // Check operator compatibility
    // For arithmetic operators, both operands should be numeric
    if (op.op == "+" || op.op == "-" || op.op == "*" || op.op == "/") {
        auto int_type = registry_.get_int_type();
        if (!is_compatible(*int_type, **left_type) || !is_compatible(*int_type, **right_type)) {
            return std::unexpected(make_error(
                "Operator '" + op.op + "' requires numeric operands",
                "Left: " + (*left_type)->name() + ", Right: " + (*right_type)->name()
            ));
        }
        return *left_type;  // Result type is same as operand type
    }
    
    // Comparison operators return Bool
    if (op.op == "==" || op.op == "!=" || op.op == "<" || op.op == ">" || 
        op.op == "<=" || op.op == ">=") {
        return registry_.get_bool_type();
    }
    
    // Logical operators require Bool operands and return Bool
    if (op.op == "&&" || op.op == "||") {
        auto bool_type = registry_.get_bool_type();
        if (!is_compatible(*bool_type, **left_type) || !is_compatible(*bool_type, **right_type)) {
            return std::unexpected(make_error(
                "Logical operator '" + op.op + "' requires boolean operands"
            ));
        }
        return bool_type;
    }
    
    return std::unexpected(make_error("Unknown binary operator '" + op.op + "'"));
}

std::expected<std::shared_ptr<meta::MetaType>, TypeError>
TypeChecker::check_unary_operation(const parser::ast::unary_operation& op,
                                   std::shared_ptr<TypeEnvironment> env) {
    auto operand_type = check_expression(op.operand, env);
    if (!operand_type) return std::unexpected(operand_type.error());
    
    if (op.op == "-") {
        auto int_type = registry_.get_int_type();
        if (!is_compatible(*int_type, **operand_type)) {
            return std::unexpected(make_error(
                "Unary minus requires numeric operand"
            ));
        }
        return *operand_type;
    }
    
    if (op.op == "!") {
        auto bool_type = registry_.get_bool_type();
        if (!is_compatible(*bool_type, **operand_type)) {
            return std::unexpected(make_error(
                "Logical not requires boolean operand"
            ));
        }
        return bool_type;
    }
    
    // !! (force unwrap): optional[T] → T, panics on nil (Req 14E.18)
    if (op.op == "!!") {
        if (registry_.is_nullable_type(**operand_type)) {
            auto inner = registry_.get_inner_type(**operand_type);
            if (inner) return *inner;
        }
        return *operand_type;  // Non-optional: !! is a no-op
    }
    
    // ? (safe return): optional[T] → T, returns nil from function if nil (Req 14C.12)
    if (op.op == "?") {
        if (registry_.is_nullable_type(**operand_type)) {
            auto inner = registry_.get_inner_type(**operand_type);
            if (inner) return *inner;
        }
        return *operand_type;
    }
    
    // ?! (result unwrap): Result[T, E] → T, returns Err from function if Err
    if (op.op == "?!") {
        // For now, pass through — full Result type checking is a future task
        return *operand_type;
    }
    
    return std::unexpected(make_error("Unknown unary operator '" + op.op + "'"));
}

// ── Safe navigation: expr?.field ──
// Narrows optional[T] → accesses field on T → wraps result in optional
// Req 14A-NIL.25, 14B.7
std::expected<std::shared_ptr<meta::MetaType>, TypeError>
TypeChecker::check_safe_navigation(const parser::ast::safe_navigation_expression& expr,
                                   std::shared_ptr<TypeEnvironment> env) {
    auto receiver_type = check_expression(expr.nullable_expr, env);
    if (!receiver_type) return std::unexpected(receiver_type.error());

    // If receiver is optional[T], narrow to T for field access, then wrap result in optional
    if (registry_.is_nullable_type(**receiver_type)) {
        auto inner = registry_.get_inner_type(**receiver_type);
        if (inner) {
            // The field access on the inner type succeeds → result is optional[field_type]
            // For now, return optional[unit] since we don't resolve field types yet
            return registry_.create_optional_type(registry_.get_unit_type());
        }
    }

    // Non-optional receiver: ?. is unnecessary but not an error
    return registry_.get_unit_type();
}

// ── Elvis: expr ?: default ──
// If left is optional[T] and non-nil, narrows to T. Otherwise evaluates right.
// Result type is T (the unwrapped type).
// Req 14A-NIL.25, 14D.15
std::expected<std::shared_ptr<meta::MetaType>, TypeError>
TypeChecker::check_elvis_expression(const parser::ast::elvis_expression& expr,
                                    std::shared_ptr<TypeEnvironment> env) {
    auto left_type = check_expression(expr.nullable_expr, env);
    if (!left_type) return std::unexpected(left_type.error());

    auto right_type = check_expression(expr.default_value, env);
    if (!right_type) return std::unexpected(right_type.error());

    // If left is optional[T], the result is T (narrowed)
    if (registry_.is_nullable_type(**left_type)) {
        auto inner = registry_.get_inner_type(**left_type);
        if (inner) return *inner;
    }

    // Non-optional left: ?: is unnecessary, return left type
    return *left_type;
}

std::expected<std::shared_ptr<meta::MetaType>, TypeError>
TypeChecker::check_function_call(const parser::ast::function_call& call,
                                std::shared_ptr<TypeEnvironment> env) {
    // Look up function type
    auto func_type = env->lookup(call.function_name.name);
    if (!func_type) {
        return std::unexpected(make_error(
            "Undefined function '" + call.function_name.name + "'"
        ));
    }
    
    // Check arguments for nil passed to non-optional parameters (Req 14A-NIL.21)
    auto null_type = registry_.get_null_type();
    if (auto* func_meta = (*func_type)->as<meta::FunctionMetaType>()) {
        const auto& param_types = func_meta->param_types();
        for (size_t i = 0; i < call.arguments.size() && i < param_types.size(); ++i) {
            auto arg_type = check_expression(call.arguments[i], env);
            if (!arg_type) return std::unexpected(arg_type.error());
            
            if ((*arg_type).get() == null_type.get() && !registry_.is_nullable_type(*param_types[i])) {
                return std::unexpected(make_error(
                    "cannot pass nil to parameter of type " + param_types[i]->name(),
                    "Parameter at position " + std::to_string(i + 1) + " of function '" + call.function_name.name + "' is non-nullable"
                ));
            }
            
            // Reject optional[T] passed where T is expected (Req 14A-NIL.22)
            if (registry_.is_nullable_type(**arg_type) && !registry_.is_nullable_type(*param_types[i])) {
                auto inner = registry_.get_inner_type(**arg_type);
                std::string inner_name = inner ? (*inner)->name() : "T";
                return std::unexpected(make_error(
                    "cannot pass optional[" + inner_name + "] to parameter of type " + param_types[i]->name(),
                    "Use ?:, !!, or narrow the type before passing to function '" + call.function_name.name + "'"
                ));
            }
        }
    }
    
    // For now, assume functions return Unit
    // In a full implementation, we'd check parameter types and return the function's return type
    return registry_.get_unit_type();
}

std::expected<std::shared_ptr<meta::MetaType>, TypeError>
TypeChecker::check_val_declaration(const parser::ast::val_declaration& decl,
                                  std::shared_ptr<TypeEnvironment> env) {
    // Infer type from value
    auto value_type = check_expression(decl.value, env);
    if (!value_type) return std::unexpected(value_type.error());
    
    // If there's a type annotation, resolve it and check nil assignment (Req 14A-NIL.20)
    if (decl.has_type_annotation) {
        auto annotated_type = resolve_type_annotation(decl.type_ann.get());
        if (!annotated_type) {
            return std::unexpected(make_error(
                "Unknown type '" + decl.type_ann.get().type_name.name + "' in type annotation for '" + decl.name.name + "'"
            ));
        }
        
        // Check if value is nil and annotated type is non-optional
        auto null_type = registry_.get_null_type();
        if ((*value_type).get() == null_type.get() && !registry_.is_nullable_type(*annotated_type)) {
            return std::unexpected(make_error(
                "cannot assign nil to non-nullable type " + annotated_type->name(),
                "Consider using optional[" + annotated_type->name() + "] or " + annotated_type->name() + "? if nil is intended"
            ));
        }
        
        // Bind with the annotated type
        env->bind(decl.name.name, annotated_type);
    } else {
        // Bind variable in environment with inferred type
        env->bind(decl.name.name, *value_type);
    }
    
    return registry_.get_unit_type();
}

std::expected<std::shared_ptr<meta::MetaType>, TypeError>
TypeChecker::check_var_declaration(const parser::ast::var_declaration& decl,
                                  std::shared_ptr<TypeEnvironment> env) {
    // Infer type from value
    auto value_type = check_expression(decl.value, env);
    if (!value_type) return std::unexpected(value_type.error());
    
    // If there's a type annotation, resolve it and check nil assignment (Req 14A-NIL.20)
    if (decl.has_type_annotation) {
        auto annotated_type = resolve_type_annotation(decl.type_ann.get());
        if (!annotated_type) {
            return std::unexpected(make_error(
                "Unknown type '" + decl.type_ann.get().type_name.name + "' in type annotation for '" + decl.name.name + "'"
            ));
        }
        
        // Check if value is nil and annotated type is non-optional
        auto null_type = registry_.get_null_type();
        if ((*value_type).get() == null_type.get() && !registry_.is_nullable_type(*annotated_type)) {
            return std::unexpected(make_error(
                "cannot assign nil to non-nullable type " + annotated_type->name(),
                "Consider using optional[" + annotated_type->name() + "] or " + annotated_type->name() + "? if nil is intended"
            ));
        }
        
        // Bind with the annotated type
        env->bind(decl.name.name, annotated_type);
    } else {
        // Bind variable in environment with inferred type
        env->bind(decl.name.name, *value_type);
    }
    
    return registry_.get_unit_type();
}

std::expected<std::shared_ptr<meta::MetaType>, TypeError>
TypeChecker::check_struct_definition(const parser::ast::struct_definition& def,
                                    std::shared_ptr<TypeEnvironment> env) {
    // Create struct type
    std::vector<meta::Field> fields;
    for (const auto& field : def.fields) {
        auto field_type = resolve_type_annotation(field.type);
        if (!field_type) {
            return std::unexpected(make_error(
                "Unknown type '" + field.type.type_name.name + "' for field '" + field.name.name + "'"
            ));
        }
        fields.emplace_back(field.name.name, field_type, field.is_mutable);
    }
    
    // Validate properties: immutable properties cannot have custom setters
    for (const auto& prop : def.properties) {
        if (!prop.is_mutable && prop.has_setter) {
            return std::unexpected(make_error(
                "Immutable property '" + prop.name.name + "' cannot have a custom setter"
            ));
        }
    }

    auto struct_type = meta::MetaType::create_struct(def.name.name, std::move(fields));
    registry_.register_type(def.name.name, struct_type);
    env->bind(def.name.name, struct_type);
    
    return struct_type;
}

std::expected<std::shared_ptr<meta::MetaType>, TypeError>
TypeChecker::check_class_definition(const parser::ast::class_definition& def,
                                   std::shared_ptr<TypeEnvironment> env) {
    // Create class type
    std::vector<meta::Field> fields;
    for (const auto& field : def.fields) {
        auto field_type = resolve_type_annotation(field.type);
        if (!field_type) {
            return std::unexpected(make_error(
                "Unknown type '" + field.type.type_name.name + "' for field '" + field.name.name + "'"
            ));
        }
        fields.emplace_back(field.name.name, field_type, field.is_mutable);
    }
    
    // Validate properties: immutable properties cannot have custom setters
    for (const auto& prop : def.properties) {
        if (!prop.is_mutable && prop.has_setter) {
            return std::unexpected(make_error(
                "Immutable property '" + prop.name.name + "' cannot have a custom setter"
            ));
        }
    }

    std::vector<meta::Method> methods;  // TODO: Parse methods from class body
    auto class_type = meta::MetaType::create_class(def.name.name, std::move(fields), std::move(methods));
    registry_.register_type(def.name.name, class_type);
    env->bind(def.name.name, class_type);
    
    return class_type;
}

std::expected<std::shared_ptr<meta::MetaType>, TypeError>
TypeChecker::check_enum_definition(const parser::ast::enum_definition& def,
                                  std::shared_ptr<TypeEnvironment> env) {
    // Create enum variants
    std::vector<meta::EnumVariant> variants;
    for (const auto& variant : def.variants) {
        std::vector<meta::Field> variant_fields;
        
        // Process associated data fields
        for (const auto& field : variant.associated_fields) {
            auto field_type = resolve_type_annotation(field.type);
            if (!field_type) {
                return std::unexpected(make_error(
                    "Unknown type '" + field.type.type_name.name + "' for field '" + field.name.name + 
                    "' in variant '" + variant.name.name + "'"
                ));
            }
            variant_fields.emplace_back(field.name.name, field_type, false); // Enum fields are immutable
        }
        
        variants.emplace_back(variant.name.name, std::move(variant_fields));
    }
    
    auto enum_type = meta::MetaType::create_enum(def.name.name, std::move(variants));
    registry_.register_type(def.name.name, enum_type);
    env->bind(def.name.name, enum_type);
    
    return enum_type;
}

std::expected<std::shared_ptr<meta::MetaType>, TypeError>
TypeChecker::check_function_definition(const parser::ast::function_definition& def,
                                      std::shared_ptr<TypeEnvironment> env) {
    // Create function environment with parameters
    auto func_env = env->create_child();
    
    std::vector<std::shared_ptr<meta::MetaType>> param_types;
    for (const auto& param : def.parameters) {
        auto param_type = resolve_type_annotation(param.type);
        if (!param_type) {
            return std::unexpected(make_error(
                "Unknown type '" + param.type.type_name.name + "' for parameter '" + param.name.name + "'"
            ));
        }
        param_types.push_back(param_type);
        func_env->bind(param.name.name, param_type);
    }
    
    // Determine return type
    std::shared_ptr<meta::MetaType> return_type;
    if (def.has_return_type) {
        return_type = resolve_type_annotation(def.return_type);
        if (!return_type) {
            return std::unexpected(make_error(
                "Unknown return type '" + def.return_type.type_name.name + "'"
            ));
        }
    } else {
        return_type = registry_.get_unit_type();
    }
    
    // Check contracts if present
    if (def.has_contracts) {
        auto contract_checker = std::make_unique<compiler::ContractChecker>(
            std::shared_ptr<types::TypeRegistry>(&registry_, [](types::TypeRegistry*){}));
        
        auto contract_result = contract_checker->verifyContracts(def);
        if (!contract_result.is_valid) {
            // Convert contract errors to type errors
            for (const auto& error : contract_result.errors) {
                return std::unexpected(make_error("Contract error in function '" + def.name.name + "': " + error));
            }
        }
        
        // Log contract warnings
        for (const auto& warning : contract_result.warnings) {
            // In a real implementation, we would log these warnings
            // For now, we'll just continue
        }
    }
    
    // Check function body for nil returns from non-optional return types (Req 14A-NIL.20)
    if (def.has_return_type && !registry_.is_nullable_type(*return_type)) {
        auto null_type = registry_.get_null_type();
        const auto& body = def.body.get();
        for (const auto& stmt : body.statements) {
            using RetStmt = boost::spirit::x3::forward_ast<parser::ast::return_statement>;
            if (auto* ret_ptr = boost::get<RetStmt>(&stmt.get())) {
                const auto& ret = ret_ptr->get();
                if (ret.has_expression) {
                    auto ret_type = check_expression(ret.expr, func_env);
                    if (ret_type && (*ret_type).get() == null_type.get()) {
                        return std::unexpected(make_error(
                            "cannot return nil from function '" + def.name.name + "' with non-nullable return type " + return_type->name(),
                            "Consider using optional[" + return_type->name() + "] or " + return_type->name() + "? as the return type"
                        ));
                    }
                }
            }
        }
    }
    
    // Create function type and bind it
    auto func_type = registry_.create_function_type(param_types, return_type);
    env->bind(def.name.name, func_type);
    
    return func_type;
}

std::expected<std::shared_ptr<meta::MetaType>, TypeError>
TypeChecker::check_lambda_expression(const parser::ast::lambda_expression& lambda,
                                    std::shared_ptr<TypeEnvironment> env) {
    // Create lambda environment
    auto lambda_env = env->create_child();
    
    std::vector<std::shared_ptr<meta::MetaType>> param_types;
    for (const auto& param : lambda.parameters) {
        std::shared_ptr<meta::MetaType> param_type;
        if (param.has_type) {
            param_type = resolve_type_annotation(param.type);
            if (!param_type) {
                return std::unexpected(make_error(
                    "Unknown type '" + param.type.type_name.name + "' for lambda parameter"
                ));
            }
        } else {
            // Type inference needed - for now, use dynamic type
            param_type = registry_.get_type("dynamic").value_or(registry_.get_unit_type());
        }
        param_types.push_back(param_type);
        lambda_env->bind(param.name.name, param_type);
    }
    
    // Check lambda body
    auto body_type = check_expression(lambda.body.get(), lambda_env);
    if (!body_type) return std::unexpected(body_type.error());
    
    // Create function type
    return registry_.create_function_type(param_types, *body_type);
}

std::expected<std::shared_ptr<meta::MetaType>, TypeError>
TypeChecker::check_tuple_literal(const parser::ast::tuple_literal& tuple,
                                std::shared_ptr<TypeEnvironment> env) {
    // Check each element and collect types
    std::vector<std::shared_ptr<meta::MetaType>> element_types;
    for (const auto& elem : tuple.elements) {
        auto elem_type = check_expression(elem.value, env);
        if (!elem_type) return std::unexpected(elem_type.error());
        element_types.push_back(*elem_type);
    }
    
    // Create tuple type
    // For now, we'll represent tuples as a special type
    // In a full implementation, we'd have a TupleMetaType
    return registry_.get_unit_type();  // Placeholder
}

std::expected<std::shared_ptr<meta::MetaType>, TypeError>
TypeChecker::check_anonymous_array_literal(const parser::ast::anonymous_array_literal& array,
                                          std::shared_ptr<TypeEnvironment> env) {
    if (array.elements.empty()) {
        // Empty array - type inference needed or explicit annotation required
        return std::unexpected(make_error("Empty array literal requires type annotation"));
    }
    
    // Check first element to determine array element type
    auto first_type = check_expression(array.elements[0], env);
    if (!first_type) return std::unexpected(first_type.error());
    
    // Check that all elements have the same type (homogeneous array)
    for (size_t i = 1; i < array.elements.size(); ++i) {
        auto elem_type = check_expression(array.elements[i], env);
        if (!elem_type) return std::unexpected(elem_type.error());
        
        if (!is_compatible(**first_type, **elem_type)) {
            return std::unexpected(make_error(
                "Array elements must have homogeneous types. Expected " + 
                (*first_type)->name() + " but found " + (*elem_type)->name()));
        }
    }
    
    // Create AnonymousArrayType
    return std::make_shared<types::AnonymousArrayType>(*first_type);
}

std::expected<std::shared_ptr<meta::MetaType>, TypeError>
TypeChecker::check_array_indexing(const parser::ast::array_indexing& indexing,
                                  std::shared_ptr<TypeEnvironment> env) {
    // Check the array expression
    auto array_type = check_expression(indexing.array, env);
    if (!array_type) return std::unexpected(array_type.error());
    
    // Check the index expression
    auto index_type = check_expression(indexing.index, env);
    if (!index_type) return std::unexpected(index_type.error());
    
    // Verify index is an integer type
    auto int_type = registry_.get_int_type();
    if (!is_compatible(*int_type, **index_type)) {
        return std::unexpected(make_error(
            "Array index must be an integer type, got " + (*index_type)->name()
        ));
    }
    
    // Check if the array type is actually an array type
    auto* anonymous_array = (*array_type)->as<types::AnonymousArrayType>();
    if (anonymous_array) {
        // Return the element type of the array
        return anonymous_array->elementType();
    }
    
    // For now, also support indexing into other collection types
    // TODO: Add support for other indexable types (List, Vector, etc.)
    return std::unexpected(make_error(
        "Cannot index into type '" + (*array_type)->name() + "'. Only arrays support indexing."
    ));
}

std::expected<std::shared_ptr<meta::MetaType>, TypeError>
TypeChecker::check_tuple_indexing(const parser::ast::tuple_indexing& indexing,
                                 std::shared_ptr<TypeEnvironment> env) {
    // Check the tuple expression
    auto tuple_type = check_expression(indexing.tuple, env);
    if (!tuple_type) return std::unexpected(tuple_type.error());
    
    // Reject direct field/method access on optional[T] types (Req 14A-NIL.22)
    if (registry_.is_nullable_type(**tuple_type)) {
        auto inner = registry_.get_inner_type(**tuple_type);
        std::string inner_name = inner ? (*inner)->name() : "T";
        return std::unexpected(make_error(
            "cannot access member '" + indexing.index + "' on optional type optional[" + inner_name + "]",
            "Use safe navigation (?.), elvis (?:), force unwrap (!!), or narrow the type first"
        ));
    }
    
    // Check if it's an anonymous tuple type
    auto* anonymous_tuple = (*tuple_type)->as<types::AnonymousTupleType>();
    if (anonymous_tuple) {
        if (indexing.is_numeric) {
            // Numeric indexing: tuple.0, tuple.1, etc.
            try {
                size_t index = std::stoull(indexing.index);
                auto element_type = anonymous_tuple->getElementType(index);
                if (element_type) {
                    return *element_type;
                } else {
                    return std::unexpected(make_error(
                        "Index " + indexing.index + " is out of bounds for tuple with " + 
                        std::to_string(anonymous_tuple->elementCount()) + " elements"
                    ));
                }
            } catch (const std::exception&) {
                return std::unexpected(make_error(
                    "Invalid numeric index: " + indexing.index
                ));
            }
        } else {
            // Named field access: tuple.x, tuple.y, etc.
            auto element_type = anonymous_tuple->getElementType(indexing.index);
            if (element_type) {
                return *element_type;
            } else {
                return std::unexpected(make_error(
                    "Tuple has no field named '" + indexing.index + "'"
                ));
            }
        }
    }
    
    // For other tuple types (regular tuple_literal), return placeholder for now
    // TODO: Implement proper type checking for regular tuples
    return registry_.get_unit_type();
}

std::expected<std::shared_ptr<meta::MetaType>, TypeError>
TypeChecker::check_anonymous_tuple_literal(const parser::ast::anonymous_tuple_literal& tuple,
                                         std::shared_ptr<TypeEnvironment> env) {
    // Check each element and collect types
    std::vector<types::AnonymousTupleElement> tuple_elements;
    
    for (size_t i = 0; i < tuple.elements.size(); ++i) {
        const auto& elem = tuple.elements[i];
        auto elem_type = check_expression(elem.value, env);
        if (!elem_type) return std::unexpected(elem_type.error());
        
        // Create tuple element with proper name and type
        tuple_elements.emplace_back(i, *elem_type, elem.name);
    }
    
    // Create and return proper tuple type
    return std::make_shared<types::AnonymousTupleType>(tuple_elements);
}

std::expected<std::shared_ptr<meta::MetaType>, TypeError>
TypeChecker::check_match_expression(const parser::ast::match_expression& match,
                                   std::shared_ptr<TypeEnvironment> env) {
    // Check matched value
    auto value_type = check_expression(match.matched_value.get(), env);
    if (!value_type) return std::unexpected(value_type.error());
    
    // Check each case and ensure all return the same type
    if (match.cases.empty()) {
        return std::unexpected(make_error("Match expression must have at least one case"));
    }
    
    auto first_case_type = check_expression(match.cases[0].result_expression.get(), env);
    if (!first_case_type) return std::unexpected(first_case_type.error());
    
    for (size_t i = 1; i < match.cases.size(); ++i) {
        auto case_type = check_expression(match.cases[i].result_expression.get(), env);
        if (!case_type) return std::unexpected(case_type.error());
        
        // All cases must return compatible types
        if (!is_compatible(**first_case_type, **case_type)) {
            return std::unexpected(make_error(
                "Match case " + std::to_string(i) + " returns incompatible type",
                "Expected: " + (*first_case_type)->name() + ", Got: " + (*case_type)->name()
            ));
        }
    }
    
    return *first_case_type;
}

std::expected<std::shared_ptr<meta::MetaType>, TypeError>
TypeChecker::check_initialization_block(const parser::ast::initialization_block& init,
                                       std::shared_ptr<TypeEnvironment> env) {
    // Look up the type being initialized
    auto type = registry_.get_type(init.type_name.name);
    if (!type) {
        return std::unexpected(make_error(
            "Unknown type '" + init.type_name.name + "' in initialization block"
        ));
    }
    
    // Check that all parameters match fields of the type
    // This requires the type to be a struct or class
    auto struct_type = (*type)->as<meta::StructMetaType>();
    auto class_type = (*type)->as<meta::ClassMetaType>();
    
    if (!struct_type && !class_type) {
        return std::unexpected(make_error(
            "Type '" + init.type_name.name + "' cannot be initialized with a block"
        ));
    }
    
    // For now, just return the type
    // In a full implementation, we'd check each parameter against the type's fields
    return *type;
}

std::shared_ptr<meta::MetaType> 
TypeChecker::resolve_type_annotation(const parser::ast::type_annotation& annotation) {
    // Handle union types (Type | Type)
    if (annotation.is_union) {
        std::vector<std::shared_ptr<meta::MetaType>> union_types;
        for (const auto& union_type_ast : annotation.union_types) {
            auto resolved = resolve_type_annotation(union_type_ast.get());
            if (!resolved) {
                return nullptr;
            }
            union_types.push_back(resolved);
        }
        return meta::MetaType::create_union(std::move(union_types));
    }
    
    // Handle intersection types (Type & Type)
    if (annotation.is_intersection) {
        std::vector<std::shared_ptr<meta::MetaType>> intersection_types;
        for (const auto& intersection_type_ast : annotation.intersection_types) {
            auto resolved = resolve_type_annotation(intersection_type_ast.get());
            if (!resolved) {
                return nullptr;
            }
            intersection_types.push_back(resolved);
        }
        return meta::MetaType::create_intersection(std::move(intersection_types));
    }
    
    // Handle simple types
    auto type = registry_.get_type(annotation.type_name.name);
    if (!type) {
        return nullptr;
    }
    
    // Handle generic type instantiation (e.g., List[Int], Map[String, Int])
    if (annotation.has_type_arguments && !annotation.type_arguments.empty()) {
        std::vector<std::shared_ptr<meta::MetaType>> type_args;
        for (const auto& type_arg_ast : annotation.type_arguments) {
            auto resolved = resolve_type_annotation(type_arg_ast.get());
            if (!resolved) {
                return nullptr;
            }
            type_args.push_back(resolved);
        }
        
        // Instantiate the generic type with the type arguments
        auto result = instantiate_generic_type(*type, type_args);
        if (!result) {
            return nullptr;
        }
        type = *result;
    }
    
    // Handle nullable types
    if (annotation.is_nullable) {
        return registry_.create_optional_type(*type);
    }
    
    return *type;
}

TypeError TypeChecker::make_error(const std::string& message,
                                 const std::string& context) const {
    return TypeError(message, current_file_, 0, 0, context);
}

void TypeChecker::add_error(const TypeError& error) {
    errors_.push_back(error);
}

// Generic type instantiation with multiple type arguments
std::expected<std::shared_ptr<meta::MetaType>, TypeError>
TypeChecker::instantiate_generic_type(
    std::shared_ptr<meta::MetaType> generic_type,
    const std::vector<std::shared_ptr<meta::MetaType>>& type_arguments) {
    
    // For now, we'll create a specialized type name
    // In a full implementation, we'd need to track generic type definitions
    // and perform proper substitution
    
    std::string specialized_name = generic_type->name() + "[";
    for (size_t i = 0; i < type_arguments.size(); ++i) {
        if (i > 0) specialized_name += ", ";
        specialized_name += type_arguments[i]->name();
    }
    specialized_name += "]";
    
    // Check if this specialized type already exists
    auto existing = registry_.get_type(specialized_name);
    if (existing) {
        return *existing;
    }
    
    // For collection types like array, list, map, create the specialized type
    if (generic_type->name() == "array" || generic_type->name() == "list") {
        if (type_arguments.size() != 1) {
            return std::unexpected(make_error(
                "array/list requires exactly 1 type argument, got " + 
                std::to_string(type_arguments.size())
            ));
        }
        auto specialized = registry_.create_array_type(type_arguments[0]);
        registry_.register_type(specialized_name, specialized);
        return specialized;
    }
    
    // For other generic types, we need more sophisticated handling
    // For now, just return a placeholder
    return std::unexpected(make_error(
        "Generic type instantiation not fully implemented for type: " + generic_type->name()
    ));
}

// Type substitution - replace type parameters with concrete types
std::shared_ptr<meta::MetaType> TypeChecker::substitute_type_parameters(
    std::shared_ptr<meta::MetaType> type,
    const std::map<std::string, std::shared_ptr<meta::MetaType>>& substitutions) {
    
    // If this is a generic type parameter, substitute it
    if (auto* generic = type->as<meta::GenericMetaType>()) {
        auto it = substitutions.find(generic->name());
        if (it != substitutions.end()) {
            return it->second;
        }
        return type;  // No substitution found
    }
    
    // If this is a union type, substitute in all members
    if (auto* union_type = type->as<meta::UnionMetaType>()) {
        std::vector<std::shared_ptr<meta::MetaType>> substituted_types;
        for (const auto& member : union_type->types()) {
            substituted_types.push_back(substitute_type_parameters(member, substitutions));
        }
        return meta::MetaType::create_union(std::move(substituted_types));
    }
    
    // If this is an intersection type, substitute in all members
    if (auto* intersection_type = type->as<meta::IntersectionMetaType>()) {
        std::vector<std::shared_ptr<meta::MetaType>> substituted_types;
        for (const auto& member : intersection_type->types()) {
            substituted_types.push_back(substitute_type_parameters(member, substitutions));
        }
        return meta::MetaType::create_intersection(std::move(substituted_types));
    }
    
    // For other types, return as-is
    // In a full implementation, we'd need to handle struct/class fields recursively
    return type;
}

// Check type parameter bounds
std::expected<void, TypeError> TypeChecker::check_type_parameter_bounds(
    const std::vector<parser::ast::generic_type_parameter>& type_params,
    const std::vector<std::shared_ptr<meta::MetaType>>& type_arguments) {
    
    if (type_params.size() != type_arguments.size()) {
        return std::unexpected(make_error(
            "Type parameter count mismatch: expected " + 
            std::to_string(type_params.size()) + ", got " + 
            std::to_string(type_arguments.size())
        ));
    }
    
    for (size_t i = 0; i < type_params.size(); ++i) {
        const auto& param = type_params[i];
        const auto& arg = type_arguments[i];
        
        // Check bound constraint if present
        if (param.has_bound) {
            auto bound_type = resolve_type_annotation(param.bound.get());
            if (!bound_type) {
                return std::unexpected(make_error(
                    "Could not resolve bound for type parameter '" + param.name.name + "'"
                ));
            }
            
            // Check if the type argument satisfies the bound
            if (!arg->is_subtype_of(*bound_type)) {
                return std::unexpected(make_error(
                    "Type argument '" + arg->name() + 
                    "' does not satisfy bound '" + bound_type->name() + 
                    "' for type parameter '" + param.name.name + "'"
                ));
            }
        }
        
        // Check variance constraints
        // For now, we'll just validate that variance is one of: "", "in", "out"
        if (!param.variance.empty() && 
            param.variance != "in" && 
            param.variance != "out") {
            return std::unexpected(make_error(
                "Invalid variance '" + param.variance + 
                "' for type parameter '" + param.name.name + "'"
            ));
        }
    }
    
    return {};
}


std::expected<std::shared_ptr<meta::MetaType>, TypeError>
TypeChecker::check_typealias_declaration(const parser::ast::typealias_declaration& decl,
                                        std::shared_ptr<TypeEnvironment> env) {
    // Resolve the target type from the type annotation
    auto target_type = resolve_type_annotation(decl.target_type);
    
    if (!target_type) {
        return std::unexpected(make_error(
            "Could not resolve target type for type alias '" + decl.alias_name.name + "'"
        ));
    }
    
    // Register the type alias in the type registry
    try {
        registry_.register_type_alias(decl.alias_name.name, target_type);
    } catch (const std::exception& e) {
        return std::unexpected(make_error(
            "Failed to register type alias '" + decl.alias_name.name + "': " + e.what()
        ));
    }
    
    // Type aliases don't have a runtime value, so we return Unit type
    return registry_.get_unit_type();
}

std::expected<std::shared_ptr<meta::MetaType>, TypeError>
TypeChecker::check_newtype_declaration(const parser::ast::newtype_declaration& decl,
                                      std::shared_ptr<TypeEnvironment> env) {
    // Resolve the wrapped type from the type annotation
    auto wrapped_type = resolve_type_annotation(decl.wrapped_type);
    
    if (!wrapped_type) {
        return std::unexpected(make_error(
            "Could not resolve wrapped type for newtype '" + decl.wrapper_name.name + "'"
        ));
    }
    
    // Create a newtype wrapper that provides zero-cost abstraction
    // The newtype is distinct from its wrapped type for type safety
    // but compiles to the same representation for zero-cost
    auto newtype_meta = meta::MetaType::create_newtype(
        decl.wrapper_name.name, 
        wrapped_type,
        decl.is_transparent  // Zero-cost optimization flag
    );
    
    // Register the newtype in the type registry
    try {
        registry_.register_type(decl.wrapper_name.name, newtype_meta);
    } catch (const std::exception& e) {
        return std::unexpected(make_error(
            "Failed to register newtype '" + decl.wrapper_name.name + "': " + e.what()
        ));
    }
    
    // Newtype declarations don't have a runtime value, so we return Unit type
    return registry_.get_unit_type();
}

std::expected<std::shared_ptr<meta::MetaType>, TypeError>
TypeChecker::check_extension_block(const parser::ast::extension_block& ext,
                                  std::shared_ptr<TypeEnvironment> env) {
    // Resolve the target type
    auto target_type = resolve_type_annotation(ext.target_type);
    if (!target_type) {
        return std::unexpected(make_error(
            "Unknown type '" + ext.target_type.type_name.name + "' in extension block"
        ));
    }
    
    // Check each extension method
    for (const auto& method : ext.methods) {
        // Create method environment with 'this' parameter
        auto method_env = std::make_shared<TypeEnvironment>(env);
        method_env->bind("this", target_type);
        
        // Check method parameters
        std::vector<std::shared_ptr<meta::MetaType>> param_types;
        for (const auto& param : method.parameters) {
            std::shared_ptr<meta::MetaType> param_type;
            if (param.type.type_name.name.empty()) {
                // Type inference needed
                param_type = registry_.get_type("dynamic").value_or(registry_.get_unit_type());
            } else {
                auto resolved_type = resolve_type_annotation(param.type);
                if (!resolved_type) {
                    return std::unexpected(make_error(
                        "Unknown type '" + param.type.type_name.name + "' for parameter '" + param.name.name + "'"
                    ));
                }
                param_type = resolved_type;
            }
            param_types.push_back(param_type);
            method_env->bind(param.name.name, param_type);
        }
        
        // Check method return type
        std::shared_ptr<meta::MetaType> return_type;
        if (method.has_return_type) {
            auto resolved_return = resolve_type_annotation(method.return_type);
            if (!resolved_return) {
                return std::unexpected(make_error(
                    "Unknown return type '" + method.return_type.type_name.name + "' for method '" + method.name.name + "'"
                ));
            }
            return_type = resolved_return;
        } else {
            // Infer return type from body — block_expression is not an expression,
            // so we check each statement and use the last one's type.
            const auto& body = method.body.get();
            std::shared_ptr<meta::MetaType> last_type;
            for (const auto& stmt : body.statements) {
                auto stmt_type = check_expression(stmt.get(), method_env);
                if (!stmt_type) return std::unexpected(stmt_type.error());
                last_type = *stmt_type;
            }
            return_type = last_type ? last_type : registry_.get_unit_type();
        }
        
        // Create function type for the extension method
        auto func_type = registry_.create_function_type(param_types, return_type);
        
        // Register the extension method
        auto& ext_registry = kernel::ExtensionRegistry::instance();
        
        // Convert parameter types to string names for registration
        std::vector<std::string> param_type_names;
        for (const auto& param_type : param_types) {
            param_type_names.push_back(param_type->name());
        }
        
        // Create a placeholder implementation (actual implementation would be in IR generation)
        auto implementation = [](const std::vector<kernel::Value>&) -> kernel::Value {
            return kernel::Value(); // Placeholder
        };
        
        ext_registry.register_extension(
            target_type->name(),
            method.name.name,
            implementation,
            param_type_names,
            return_type->name()
        );
    }
    
    // Extension blocks don't have a meaningful return type
    return registry_.get_unit_type();
}

std::expected<std::shared_ptr<meta::MetaType>, TypeError>
TypeChecker::check_pipeline_expression(const parser::ast::pipeline_expression& pipe,
                                      std::shared_ptr<TypeEnvironment> env) {
    // Check the value being piped
    auto value_type = check_expression(pipe.value.get(), env);
    if (!value_type) return std::unexpected(value_type.error());
    
    // Check the function being applied
    auto func_type = check_expression(pipe.function.get(), env);
    if (!func_type) return std::unexpected(func_type.error());
    
    // Verify that the function can accept the value as its first parameter
    auto* function_meta = dynamic_cast<meta::FunctionMetaType*>(func_type->get());
    if (!function_meta) {
        return std::unexpected(make_error(
            "Pipeline operator requires a function on the right side"
        ));
    }
    
    // Check if the function can be called with the value as first argument
    std::vector<std::shared_ptr<meta::MetaType>> arg_types = { *value_type };
    
    // For pipeline, we only check the first parameter
    if (function_meta->param_types().empty()) {
        return std::unexpected(make_error(
            "Pipeline function must accept at least one parameter"
        ));
    }
    
    if (!function_meta->param_types()[0]->is_assignable_from(**value_type)) {
        return std::unexpected(make_error(
            "Pipeline value type '" + (*value_type)->name() + 
            "' is not compatible with function parameter type '" + 
            function_meta->param_types()[0]->name() + "'"
        ));
    }
    
    // Return the function's return type
    return function_meta->return_type();
}

std::expected<std::shared_ptr<meta::MetaType>, TypeError>
TypeChecker::check_namespace_declaration(const parser::ast::namespace_declaration& decl,
                                        std::shared_ptr<TypeEnvironment> env) {
    // Get the namespace registry
    auto& registry = kernel::NamespaceRegistry::instance();
    
    // Create or get the namespace
    auto ns = registry.get_or_create_namespace(decl.name_parts);
    
    // Push the namespace onto the stack
    registry.push_namespace(ns);
    
    // Create a new type environment for the namespace
    auto namespace_env = env->create_child();
    
    // Type check all declarations in the namespace body
    std::vector<std::shared_ptr<meta::MetaType>> body_types;
    for (const auto& expr : decl.body) {
        auto result = check_expression(expr, namespace_env);
        if (!result) {
            // Pop the namespace before returning error
            registry.pop_namespace();
            return std::unexpected(result.error());
        }
        body_types.push_back(result.value());
    }
    
    // Pop the namespace from the stack
    registry.pop_namespace();
    
    // Namespace declarations don't have a meaningful type, return Unit
    return registry_.get_unit_type();
}

std::expected<std::shared_ptr<meta::MetaType>, TypeError>
TypeChecker::check_import_declaration(const parser::ast::import_declaration& decl,
                                     std::shared_ptr<TypeEnvironment> env) {
    // Get the namespace registry
    auto& registry = kernel::NamespaceRegistry::instance();
    
    // Validate that the imported namespace exists
    if (decl.import_type == parser::ast::ImportType::WILDCARD || 
        decl.import_type == parser::ast::ImportType::ALIASED) {
        // For wildcard and aliased imports, check if the namespace exists
        auto ns = registry.get_or_create_namespace(decl.namespace_path);
        if (!ns) {
            return std::unexpected(make_error(
                "Cannot import from non-existent namespace: " + 
                [&]() {
                    std::ostringstream oss;
                    for (size_t i = 0; i < decl.namespace_path.size(); ++i) {
                        if (i > 0) oss << ".";
                        oss << decl.namespace_path[i];
                    }
                    return oss.str();
                }()
            ));
        }
    } else if (decl.import_type == parser::ast::ImportType::SPECIFIC) {
        // For specific imports, check if the symbol exists
        if (decl.namespace_path.size() < 2) {
            return std::unexpected(make_error(
                "Invalid import path: must specify at least namespace.symbol"
            ));
        }
        
        // Extract namespace path and symbol name
        std::vector<std::string> ns_path(decl.namespace_path.begin(), decl.namespace_path.end() - 1);
        std::string symbol_name = decl.namespace_path.back();
        
        auto ns = registry.get_or_create_namespace(ns_path);
        if (!ns) {
            return std::unexpected(make_error(
                "Cannot import from non-existent namespace: " + 
                [&]() {
                    std::ostringstream oss;
                    for (size_t i = 0; i < ns_path.size(); ++i) {
                        if (i > 0) oss << ".";
                        oss << ns_path[i];
                    }
                    return oss.str();
                }()
            ));
        }
        
        // Check if the symbol exists in the namespace
        auto symbol = ns->lookup_local(symbol_name);
        if (!symbol) {
            return std::unexpected(make_error(
                "Symbol '" + symbol_name + "' not found in namespace " + ns->qualified_name()
            ));
        }
    }
    
    // Register the import with the namespace registry
    registry.register_import(
        decl.namespace_path,
        decl.import_type == parser::ast::ImportType::WILDCARD,
        decl.has_alias ? decl.alias : ""
    );
    
    // Import declarations don't have a meaningful type, return Unit
    return registry_.get_unit_type();
}

} // namespace meld::compiler
