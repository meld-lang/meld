#include "meld/macro/const_generics.hpp"
#include "meld/kernel/operations.hpp"
#include <format>
#include <sstream>
#include <algorithm>
#include <cmath>

namespace meld::macro {

// ============================================================================
// ConstGenericContext Implementation
// ============================================================================

void ConstGenericContext::add_parameter(const ConstGenericParam& param) {
    parameters_.push_back(param);
    
    // If parameter has a default value, set it
    if (param.default_value) {
        values_[param.name] = *param.default_value;
    }
}

void ConstGenericContext::set_value(const std::string& name, const ConstValue& value) {
    values_[name] = value;
}

std::expected<ConstValue, std::string> 
ConstGenericContext::get_value(const std::string& name) const {
    auto it = values_.find(name);
    if (it != values_.end()) {
        return it->second;
    }
    return std::unexpected(std::format("Const generic parameter '{}' not found", name));
}

bool ConstGenericContext::has_parameter(const std::string& name) const {
    return std::any_of(parameters_.begin(), parameters_.end(),
                      [&name](const ConstGenericParam& p) { return p.name == name; });
}

void ConstGenericContext::clear() {
    parameters_.clear();
    values_.clear();
}

ConstGenericContext ConstGenericContext::create_child() const {
    ConstGenericContext child;
    child.parameters_ = parameters_;
    child.values_ = values_;
    return child;
}

// ============================================================================
// ConstGenericChecker Implementation
// ============================================================================

bool ConstGenericChecker::check_type(const ConstValue& value, const std::string& type) {
    if (type == "usize" || type == "isize" || type == "i64") {
        return std::holds_alternative<int64_t>(value);
    } else if (type == "u64") {
        return std::holds_alternative<uint64_t>(value);
    } else if (type == "bool") {
        return std::holds_alternative<bool>(value);
    } else if (type == "str" || type == "String") {
        return std::holds_alternative<std::string>(value);
    }
    return false;
}

std::expected<void, std::string> 
ConstGenericChecker::validate_args(
    const std::vector<ConstGenericParam>& params,
    const ConstGenericArgs& args) {
    
    // Check that all required parameters have values
    for (const auto& param : params) {
        if (!param.default_value && args.find(param.name) == args.end()) {
            return std::unexpected(
                std::format("Missing value for const generic parameter '{}'", param.name)
            );
        }
    }
    
    // Check that all provided arguments match parameter types
    for (const auto& [name, value] : args) {
        auto param_it = std::find_if(params.begin(), params.end(),
                                     [&name](const ConstGenericParam& p) { 
                                         return p.name == name; 
                                     });
        
        if (param_it == params.end()) {
            return std::unexpected(
                std::format("Unknown const generic parameter '{}'", name)
            );
        }
        
        if (!check_type(value, param_it->type)) {
            return std::unexpected(
                std::format("Type mismatch for const generic parameter '{}': expected {}, got {}",
                           name, param_it->type, value_to_string(value))
            );
        }
    }
    
    return {};
}

std::expected<ConstValue, std::string> 
ConstGenericChecker::evaluate_const_expr(
    const kernel::Value& expr, 
    const ConstGenericContext& context) {
    
    // Simple constant evaluation
    // In a full implementation, this would handle complex expressions
    
    if (expr.is<int64_t>()) {
        return ConstValue(expr.as<int64_t>());
    } else if (expr.is<bool>()) {
        return ConstValue(expr.as<bool>());
    } else if (expr.is<std::string>()) {
        return ConstValue(expr.as<std::string>());
    } else if (expr.is<std::shared_ptr<kernel::Symbol>>()) {
        // Look up symbol in context
        auto sym = expr.as<std::shared_ptr<kernel::Symbol>>();
        return context.get_value(sym->name());
    }
    
    return std::unexpected("Cannot evaluate expression as const");
}

bool ConstGenericChecker::values_equal(const ConstValue& a, const ConstValue& b) {
    if (a.index() != b.index()) {
        return false;
    }
    
    return std::visit([&b](auto&& arg) -> bool {
        using T = std::decay_t<decltype(arg)>;
        if (auto* b_val = std::get_if<T>(&b)) {
            return arg == *b_val;
        }
        return false;
    }, a);
}

std::string ConstGenericChecker::value_to_string(const ConstValue& value) {
    return std::visit([](auto&& arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, int64_t>) {
            return std::to_string(arg);
        } else if constexpr (std::is_same_v<T, uint64_t>) {
            return std::to_string(arg) + "u";
        } else if constexpr (std::is_same_v<T, bool>) {
            return arg ? "true" : "false";
        } else if constexpr (std::is_same_v<T, std::string>) {
            return "\"" + arg + "\"";
        }
        return "<unknown>";
    }, value);
}

std::expected<ConstValue, std::string> 
ConstGenericChecker::parse_value(const std::string& str, const std::string& type) {
    try {
        if (type == "usize" || type == "isize" || type == "i64") {
            return ConstValue(std::stoll(str));
        } else if (type == "u64") {
            return ConstValue(std::stoull(str));
        } else if (type == "bool") {
            if (str == "true") return ConstValue(true);
            if (str == "false") return ConstValue(false);
            return std::unexpected("Invalid boolean value: " + str);
        } else if (type == "str" || type == "String") {
            return ConstValue(str);
        }
    } catch (const std::exception& e) {
        return std::unexpected(std::format("Failed to parse value '{}' as {}: {}", 
                                          str, type, e.what()));
    }
    
    return std::unexpected("Unknown type: " + type);
}

// ============================================================================
// ConstGenericMonomorphizer Implementation
// ============================================================================

std::string ConstGenericMonomorphizer::generate_monomorphized_name(
    const std::string& base_name,
    const ConstGenericArgs& args) {
    
    std::ostringstream oss;
    oss << base_name;
    
    // Sort args by name for consistent naming
    std::vector<std::pair<std::string, ConstValue>> sorted_args(args.begin(), args.end());
    std::sort(sorted_args.begin(), sorted_args.end());
    
    for (const auto& [name, value] : sorted_args) {
        oss << "_" << ConstGenericChecker::value_to_string(value);
    }
    
    return oss.str();
}

std::expected<kernel::Value, std::string> 
ConstGenericMonomorphizer::monomorphize_type(
    const parser::ast::class_definition& type_def,
    const ConstGenericArgs& args) {
    
    // Generate specialized type name
    std::string specialized_name = generate_monomorphized_name(type_def.name.name, args);
    
    // Create specialized type AST
    // In a full implementation, this would:
    // 1. Substitute const parameters in field types
    // 2. Substitute const parameters in method bodies
    // 3. Generate optimized code based on const values
    
    auto name_sym = std::make_shared<kernel::Symbol>(specialized_name);
    return kernel::Value(name_sym);
}

std::expected<kernel::Value, std::string> 
ConstGenericMonomorphizer::monomorphize_function(
    const parser::ast::function_declaration& func_def,
    const ConstGenericArgs& args) {
    
    // Generate specialized function name
    std::string specialized_name = generate_monomorphized_name(func_def.name.name, args);
    
    // Create specialized function AST
    // In a full implementation, this would:
    // 1. Substitute const parameters in parameter types
    // 2. Substitute const parameters in return type
    // 3. Substitute const parameters in function body
    // 4. Generate optimized code based on const values
    
    auto name_sym = std::make_shared<kernel::Symbol>(specialized_name);
    return kernel::Value(name_sym);
}

std::expected<kernel::Value, std::string> 
ConstGenericMonomorphizer::get_or_create_instance(
    const std::string& base_name,
    const ConstGenericArgs& args,
    MonomorphizationCache& cache,
    std::function<std::expected<kernel::Value, std::string>()> generator) {
    
    auto key = std::make_pair(base_name, args);
    
    // Check cache
    auto it = cache.find(key);
    if (it != cache.end()) {
        return it->second;
    }
    
    // Generate new instance
    auto result = generator();
    if (!result) {
        return result;
    }
    
    // Cache the result
    cache[key] = *result;
    return *result;
}

// ============================================================================
// ConstExprEvaluator Implementation
// ============================================================================

std::expected<ConstValue, std::string> 
ConstExprEvaluator::eval_binary_op(
    const std::string& op, 
    const ConstValue& left, 
    const ConstValue& right) {
    
    // Handle integer operations
    if (std::holds_alternative<int64_t>(left) && std::holds_alternative<int64_t>(right)) {
        int64_t l = std::get<int64_t>(left);
        int64_t r = std::get<int64_t>(right);
        
        if (op == "+") return ConstValue(l + r);
        if (op == "-") return ConstValue(l - r);
        if (op == "*") return ConstValue(l * r);
        if (op == "/") {
            if (r == 0) return std::unexpected("Division by zero");
            return ConstValue(l / r);
        }
        if (op == "%") {
            if (r == 0) return std::unexpected("Modulo by zero");
            return ConstValue(l % r);
        }
        if (op == "==") return ConstValue(l == r);
        if (op == "!=") return ConstValue(l != r);
        if (op == "<") return ConstValue(l < r);
        if (op == "<=") return ConstValue(l <= r);
        if (op == ">") return ConstValue(l > r);
        if (op == ">=") return ConstValue(l >= r);
    }
    
    // Handle boolean operations
    if (std::holds_alternative<bool>(left) && std::holds_alternative<bool>(right)) {
        bool l = std::get<bool>(left);
        bool r = std::get<bool>(right);
        
        if (op == "&&") return ConstValue(l && r);
        if (op == "||") return ConstValue(l || r);
        if (op == "==") return ConstValue(l == r);
        if (op == "!=") return ConstValue(l != r);
    }
    
    return std::unexpected(std::format("Cannot apply operator '{}' to operands", op));
}

std::expected<ConstValue, std::string> 
ConstExprEvaluator::eval_unary_op(const std::string& op, const ConstValue& operand) {
    
    // Handle integer operations
    if (std::holds_alternative<int64_t>(operand)) {
        int64_t val = std::get<int64_t>(operand);
        
        if (op == "-") return ConstValue(-val);
        if (op == "+") return ConstValue(val);
    }
    
    // Handle boolean operations
    if (std::holds_alternative<bool>(operand)) {
        bool val = std::get<bool>(operand);
        
        if (op == "!") return ConstValue(!val);
    }
    
    return std::unexpected(std::format("Cannot apply operator '{}' to operand", op));
}

std::expected<ConstValue, std::string> 
ConstExprEvaluator::eval_conditional(
    const ConstValue& condition, 
    const ConstValue& true_val, 
    const ConstValue& false_val) {
    
    if (!std::holds_alternative<bool>(condition)) {
        return std::unexpected("Condition must be a boolean");
    }
    
    bool cond = std::get<bool>(condition);
    return cond ? true_val : false_val;
}

bool ConstExprEvaluator::is_const_expr(
    const kernel::Value& expr, 
    const ConstGenericContext& context) {
    
    // Check if expression is a compile-time constant
    if (expr.is<int64_t>() || expr.is<bool>() || expr.is<std::string>()) {
        return true;
    }
    
    if (expr.is<std::shared_ptr<kernel::Symbol>>()) {
        auto sym = expr.as<std::shared_ptr<kernel::Symbol>>();
        return context.has_parameter(sym->name());
    }
    
    // In a full implementation, would recursively check sub-expressions
    return false;
}

// ============================================================================
// Helper Functions
// ============================================================================

std::vector<ConstGenericParam> parse_const_generic_params(const std::string& definition) {
    std::vector<ConstGenericParam> params;
    
    // Simple parser for "const N: usize" style parameters
    // In a full implementation, this would use the actual parser
    
    // Placeholder implementation
    return params;
}

std::expected<ConstGenericArgs, std::string> parse_const_generic_args(
    const std::string& instantiation,
    const std::vector<ConstGenericParam>& params) {
    
    ConstGenericArgs args;
    
    // Simple parser for const generic arguments
    // In a full implementation, this would use the actual parser
    
    // Placeholder implementation
    return args;
}

kernel::Value substitute_const_params(
    const kernel::Value& ast,
    const ConstGenericArgs& args) {
    
    // Substitute const parameter references with their values
    // In a full implementation, this would recursively traverse the AST
    
    if (ast.is<std::shared_ptr<kernel::Symbol>>()) {
        auto sym = ast.as<std::shared_ptr<kernel::Symbol>>();
        auto it = args.find(sym->name());
        if (it != args.end()) {
            // Replace symbol with const value
            return std::visit([](auto&& arg) -> kernel::Value {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, int64_t>) {
                    return kernel::Value(arg);
                } else if constexpr (std::is_same_v<T, bool>) {
                    return kernel::Value(arg);
                } else if constexpr (std::is_same_v<T, std::string>) {
                    return kernel::Value(arg);
                }
                return kernel::Value(kernel::nil());
            }, it->second);
        }
    }
    
    return ast;
}

kernel::Value generate_const_assertions(
    const std::vector<ConstGenericParam>& params,
    const ConstGenericArgs& args) {
    
    // Generate compile-time assertions for const generic constraints
    // e.g., assert(N > 0, "Array size must be positive")
    
    // In a full implementation, this would generate actual assertion code
    
    auto assert_sym = std::make_shared<kernel::Symbol>("const_assert");
    return kernel::Value(assert_sym);
}

} // namespace meld::macro
