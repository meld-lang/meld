#pragma once

#include "meld/kernel/primitives.hpp"
#include "meld/parser/ast.hpp"
#include <string>
#include <memory>
#include <vector>
#include <map>
#include <expected>
#include <variant>
#include <optional>

namespace meld::macro {

// ============================================================================
// CONST GENERICS SYSTEM
// Task 9.2: Add const generics support
// Requirements: 6.3
// ============================================================================

// Const generic parameter value types
using ConstValue = std::variant<
    int64_t,        // Integer constants
    uint64_t,       // Unsigned integer constants
    bool,           // Boolean constants
    std::string     // String constants (for advanced use cases)
>;

// Const generic parameter definition
struct ConstGenericParam {
    std::string name;           // Parameter name (e.g., "N")
    std::string type;           // Parameter type (e.g., "usize", "bool")
    std::optional<ConstValue> default_value;  // Optional default value
    
    ConstGenericParam(std::string n, std::string t, std::optional<ConstValue> def = std::nullopt)
        : name(std::move(n)), type(std::move(t)), default_value(std::move(def)) {}
};

// Const generic instantiation
// Maps parameter names to their concrete values
using ConstGenericArgs = std::map<std::string, ConstValue>;

// Const generic context
// Tracks const generic parameters and their values during compilation
class ConstGenericContext {
public:
    ConstGenericContext() = default;
    
    // Add a const generic parameter definition
    void add_parameter(const ConstGenericParam& param);
    
    // Set a const generic parameter value
    void set_value(const std::string& name, const ConstValue& value);
    
    // Get a const generic parameter value
    std::expected<ConstValue, std::string> get_value(const std::string& name) const;
    
    // Check if a parameter is defined
    bool has_parameter(const std::string& name) const;
    
    // Get all parameter definitions
    const std::vector<ConstGenericParam>& parameters() const { return parameters_; }
    
    // Get all parameter values
    const ConstGenericArgs& values() const { return values_; }
    
    // Clear all parameters and values
    void clear();
    
    // Create a child context (for nested scopes)
    ConstGenericContext create_child() const;
    
private:
    std::vector<ConstGenericParam> parameters_;
    ConstGenericArgs values_;
};

// Const generic type checker
// Validates const generic parameters and their usage
class ConstGenericChecker {
public:
    // Check if a const value matches a type
    static bool check_type(const ConstValue& value, const std::string& type);
    
    // Validate const generic arguments against parameters
    static std::expected<void, std::string> 
    validate_args(const std::vector<ConstGenericParam>& params,
                  const ConstGenericArgs& args);
    
    // Evaluate a const expression at compile time
    static std::expected<ConstValue, std::string> 
    evaluate_const_expr(const kernel::Value& expr, const ConstGenericContext& context);
    
    // Check if two const values are equal
    static bool values_equal(const ConstValue& a, const ConstValue& b);
    
    // Convert const value to string representation
    static std::string value_to_string(const ConstValue& value);
    
    // Parse const value from string
    static std::expected<ConstValue, std::string> 
    parse_value(const std::string& str, const std::string& type);
};

// Const generic monomorphization
// Generates specialized versions of generic types/functions for specific const values
class ConstGenericMonomorphizer {
public:
    // Generate a monomorphized name for a type/function
    // e.g., "Array<T, 10>" -> "Array_T_10"
    static std::string generate_monomorphized_name(
        const std::string& base_name,
        const ConstGenericArgs& args
    );
    
    // Monomorphize a type definition with const generic parameters
    static std::expected<kernel::Value, std::string> 
    monomorphize_type(const parser::ast::class_definition& type_def,
                     const ConstGenericArgs& args);
    
    // Monomorphize a function definition with const generic parameters
    static std::expected<kernel::Value, std::string> 
    monomorphize_function(const parser::ast::function_declaration& func_def,
                         const ConstGenericArgs& args);
    
    // Cache for monomorphized instances
    // Maps (base_name, args) -> monomorphized_ast
    using MonomorphizationCache = std::map<
        std::pair<std::string, ConstGenericArgs>,
        kernel::Value
    >;
    
    // Get or create a monomorphized instance
    static std::expected<kernel::Value, std::string> 
    get_or_create_instance(const std::string& base_name,
                          const ConstGenericArgs& args,
                          MonomorphizationCache& cache,
                          std::function<std::expected<kernel::Value, std::string>()> generator);
};

// Const generic expression evaluator
// Evaluates compile-time constant expressions
class ConstExprEvaluator {
public:
    // Evaluate a binary operation
    static std::expected<ConstValue, std::string> 
    eval_binary_op(const std::string& op, const ConstValue& left, const ConstValue& right);
    
    // Evaluate a unary operation
    static std::expected<ConstValue, std::string> 
    eval_unary_op(const std::string& op, const ConstValue& operand);
    
    // Evaluate a conditional expression
    static std::expected<ConstValue, std::string> 
    eval_conditional(const ConstValue& condition, 
                    const ConstValue& true_val, 
                    const ConstValue& false_val);
    
    // Check if an expression is a compile-time constant
    static bool is_const_expr(const kernel::Value& expr, const ConstGenericContext& context);
};

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

// Parse const generic parameters from type/function definition
// e.g., "Array<T, const N: usize>" -> [ConstGenericParam("N", "usize")]
std::vector<ConstGenericParam> parse_const_generic_params(
    const std::string& definition
);

// Parse const generic arguments from type instantiation
// e.g., "Array<i32, 10>" -> {"N": 10}
std::expected<ConstGenericArgs, std::string> parse_const_generic_args(
    const std::string& instantiation,
    const std::vector<ConstGenericParam>& params
);

// Substitute const generic parameters in AST
// Replaces parameter references with their concrete values
kernel::Value substitute_const_params(
    const kernel::Value& ast,
    const ConstGenericArgs& args
);

// Generate compile-time assertions for const generic constraints
// e.g., "const N: usize where N > 0" -> runtime check
kernel::Value generate_const_assertions(
    const std::vector<ConstGenericParam>& params,
    const ConstGenericArgs& args
);

} // namespace meld::macro
