#include "meld/compiler/refinement_checker.hpp"
#include "meld/types/refinement.hpp"
#include "meld/compat/visit.hpp"
#include <format>

using namespace meld::compiler;
using namespace meld::parser::ast;
using namespace meld::meta;
using namespace meld::kernel;

// Check if a value expression can be statically validated against a refinement type
std::expected<bool, std::string> RefinementChecker::validate_static_assignment(
    const expression& value_expr,
    std::shared_ptr<RefinementMetaType> refinement_type) {
    
    // First, check if the value is a compile-time constant
    if (!is_compile_time_constant(value_expr)) {
        // Cannot validate at compile time - will need runtime check
        return false;
    }
    
    // Evaluate the constant value
    auto constant_value = evaluate_constant(value_expr);
    if (!constant_value.has_value()) {
        return std::unexpected("Failed to evaluate compile-time constant: " + constant_value.error());
    }
    
    // Validate the constant against the refinement type
    auto validation_result = refinement_type->validate_value(*constant_value);
    if (!validation_result.has_value()) {
        return std::unexpected("Refinement validation failed: " + validation_result.error());
    }
    
    if (!validation_result.value()) {
        return std::unexpected(generate_violation_message(value_expr, refinement_type, 
            "Compile-time constant does not satisfy refinement constraint"));
    }
    
    return true;
}

// Analyze a refinement predicate for static validation opportunities
RefinementChecker::PredicateAnalysis RefinementChecker::analyze_predicate(const expression& predicate) {
    PredicateAnalysis analysis;
    
    // Check if we've already analyzed this predicate
    std::string predicate_key = ""; // Would need to implement expression serialization
    
    return meld::compat::visit<PredicateAnalysis>([this](const auto& e) -> PredicateAnalysis {
        using T = std::decay_t<decltype(e)>;
        if constexpr (std::is_same_v<T, std::shared_ptr<binary_operation>>) {
            return analyze_binary_operation(*e);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<function_call>>) {
            return analyze_function_call(*e);
        } else if constexpr (std::is_same_v<T, identifier>) {
            return analyze_identifier_access(e);
        } else {
            PredicateAnalysis a;
            a.is_statically_analyzable = false;
            a.analysis_error = "Unsupported predicate type for static analysis";
            return a;
        }
    }, predicate);
}

// Analyze binary operations (comparisons, logical operations)
RefinementChecker::PredicateAnalysis RefinementChecker::analyze_binary_operation(const binary_operation& binop) {
    PredicateAnalysis analysis;
    
    // Check for simple comparison operations: it > 0, it >= 5, etc.
    if (binop.op == ">" || binop.op == ">=" || binop.op == "<" || binop.op == "<=" || 
        binop.op == "==" || binop.op == "!=") {
        
        // Check if left side is 'it' (the parameter being validated)
        bool left_is_it = meld::compat::visit<bool>([](const auto& e) -> bool {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, identifier>) {
                return e.name == "it";
            }
            return false;
        }, binop.left.get());
        
        if (left_is_it) {
            // Check if right side is a constant
            if (is_compile_time_constant(binop.right.get())) {
                auto constant_value = evaluate_constant(binop.right.get());
                if (constant_value.has_value()) {
                    analysis.is_statically_analyzable = true;
                    analysis.type = PredicateAnalysis::PredicateType::COMPARISON;
                    analysis.operator_symbol = binop.op;
                    analysis.comparison_value = *constant_value;
                    return analysis;
                }
            }
        }
    }
    
    // Check for range checks: it >= 0 && it <= 100
    if (binop.op == "&&") {
        auto left_analysis = analyze_predicate(binop.left.get());
        auto right_analysis = analyze_predicate(binop.right.get());
        
        if (left_analysis.is_statically_analyzable && right_analysis.is_statically_analyzable &&
            left_analysis.type == PredicateAnalysis::PredicateType::COMPARISON &&
            right_analysis.type == PredicateAnalysis::PredicateType::COMPARISON) {
            
            analysis.is_statically_analyzable = true;
            analysis.type = PredicateAnalysis::PredicateType::RANGE_CHECK;
            
            // Determine min and max based on operators
            if (left_analysis.operator_symbol == ">=" || left_analysis.operator_symbol == ">") {
                analysis.has_min = true;
                analysis.min_value = left_analysis.comparison_value;
            }
            if (right_analysis.operator_symbol == "<=" || right_analysis.operator_symbol == "<") {
                analysis.has_max = true;
                analysis.max_value = right_analysis.comparison_value;
            }
            
            return analysis;
        }
    }
    
    analysis.is_statically_analyzable = false;
    analysis.analysis_error = "Complex binary operation not supported for static analysis";
    return analysis;
}

// Analyze function calls (like it.length, it.matches())
RefinementChecker::PredicateAnalysis RefinementChecker::analyze_function_call(const function_call& call) {
    PredicateAnalysis analysis;
    
    // Check for string length checks: it.length > 0
    if (call.function_name.name == "length") {
        analysis.is_statically_analyzable = true;
        analysis.type = PredicateAnalysis::PredicateType::STRING_LENGTH;
        return analysis;
    }
    
    // Check for regex matches: it.matches(/pattern/)
    if (call.function_name.name == "matches") {
        analysis.is_statically_analyzable = true;
        analysis.type = PredicateAnalysis::PredicateType::REGEX_MATCH;
        return analysis;
    }
    
    analysis.is_statically_analyzable = false;
    analysis.analysis_error = "Function call not supported for static analysis";
    return analysis;
}

// Analyze identifier access
RefinementChecker::PredicateAnalysis RefinementChecker::analyze_identifier_access(const identifier& id) {
    PredicateAnalysis analysis;
    
    // Simple boolean predicates
    if (id.name == "it") {
        analysis.is_statically_analyzable = true;
        analysis.type = PredicateAnalysis::PredicateType::COMPARISON;
        analysis.operator_symbol = "==";
        analysis.comparison_value = Value(Boolean::true_value());
        return analysis;
    }
    
    analysis.is_statically_analyzable = false;
    analysis.analysis_error = "Identifier access not supported for static analysis";
    return analysis;
}

// Check if a literal value satisfies a refinement constraint at compile time
std::expected<bool, std::string> RefinementChecker::validate_literal_value(
    const expression& literal,
    const PredicateAnalysis& analysis) {
    
    if (!analysis.is_statically_analyzable) {
        return std::unexpected("Predicate is not statically analyzable");
    }
    
    auto literal_value = evaluate_constant(literal);
    if (!literal_value.has_value()) {
        return std::unexpected("Failed to evaluate literal value");
    }
    
    switch (analysis.type) {
        case PredicateAnalysis::PredicateType::COMPARISON: {
            // Simple comparison validation
            if (literal_value->is<Integer>() && analysis.comparison_value.is<Integer>()) {
                int64_t lit_val = literal_value->as<Integer>()->value();
                int64_t comp_val = analysis.comparison_value.as<Integer>()->value();
                
                if (analysis.operator_symbol == ">") return lit_val > comp_val;
                if (analysis.operator_symbol == ">=") return lit_val >= comp_val;
                if (analysis.operator_symbol == "<") return lit_val < comp_val;
                if (analysis.operator_symbol == "<=") return lit_val <= comp_val;
                if (analysis.operator_symbol == "==") return lit_val == comp_val;
                if (analysis.operator_symbol == "!=") return lit_val != comp_val;
            }
            break;
        }
        
        case PredicateAnalysis::PredicateType::RANGE_CHECK: {
            if (literal_value->is<Integer>()) {
                int64_t lit_val = literal_value->as<Integer>()->value();
                
                if (analysis.has_min && analysis.min_value.is<Integer>()) {
                    int64_t min_val = analysis.min_value.as<Integer>()->value();
                    if (lit_val < min_val) return false;
                }
                
                if (analysis.has_max && analysis.max_value.is<Integer>()) {
                    int64_t max_val = analysis.max_value.as<Integer>()->value();
                    if (lit_val > max_val) return false;
                }
                
                return true;
            }
            break;
        }
        
        case PredicateAnalysis::PredicateType::STRING_LENGTH: {
            if (literal_value->is<String>()) {
                return !literal_value->as<String>()->value().empty();
            }
            break;
        }
        
        default:
            return std::unexpected("Unsupported predicate type for validation");
    }
    
    return std::unexpected("Type mismatch between literal and predicate");
}

// Generate compile-time error messages for refinement violations
std::string RefinementChecker::generate_violation_message(
    const expression& value_expr,
    std::shared_ptr<RefinementMetaType> refinement_type,
    const std::string& violation_reason) {
    
    return std::format(
        "Refinement type violation: Cannot assign value to type '{}'. {}", 
        refinement_type->name(), 
        violation_reason
    );
}

// Check if an expression is a compile-time constant
bool RefinementChecker::is_compile_time_constant(const expression& expr) {
    return meld::compat::visit<bool>([this](const auto& e) -> bool {
        using T = std::decay_t<decltype(e)>;
        
        if constexpr (std::is_same_v<T, integer_literal> ||
                      std::is_same_v<T, float_literal> ||
                      std::is_same_v<T, string_literal> ||
                      std::is_same_v<T, boolean_literal>) {
            return true;
        } else if constexpr (std::is_same_v<T, std::shared_ptr<binary_operation>>) {
            // Binary operations on constants are constants
            return is_compile_time_constant(e->left.get()) && is_compile_time_constant(e->right.get());
        } else if constexpr (std::is_same_v<T, std::shared_ptr<unary_operation>>) {
            return is_compile_time_constant(e->operand.get());
        }
        
        return false;
    }, expr);
}

// Evaluate compile-time constant expressions
std::expected<Value, std::string> RefinementChecker::evaluate_constant(const expression& expr) {
    return meld::compat::visit<std::expected<Value, std::string>>([this](const auto& e) -> std::expected<Value, std::string> {
        using T = std::decay_t<decltype(e)>;
        
        if constexpr (std::is_same_v<T, integer_literal>) {
            return Value(std::make_shared<Integer>(e.value));
        } else if constexpr (std::is_same_v<T, float_literal>) {
            // For now, treat floats as integers for simplicity
            return Value(std::make_shared<Integer>(static_cast<int64_t>(e.value)));
        } else if constexpr (std::is_same_v<T, string_literal>) {
            return Value(std::make_shared<String>(e.value));
        } else if constexpr (std::is_same_v<T, boolean_literal>) {
            return Value(e.value ? Boolean::true_value() : Boolean::false_value());
        } else if constexpr (std::is_same_v<T, std::shared_ptr<binary_operation>>) {
            return evaluate_binary_op(*e);
        }
        
        return std::unexpected("Expression is not a compile-time constant");
    }, expr);
}

// Evaluate binary operations on constants
std::expected<Value, std::string> RefinementChecker::evaluate_binary_op(const binary_operation& binop) {
    auto left_val = evaluate_constant(binop.left.get());
    auto right_val = evaluate_constant(binop.right.get());
    
    if (!left_val.has_value() || !right_val.has_value()) {
        return std::unexpected("Cannot evaluate binary operation on non-constants");
    }
    
    // Simple integer arithmetic
    if (left_val->is<Integer>() && right_val->is<Integer>()) {
        int64_t left = left_val->as<Integer>()->value();
        int64_t right = right_val->as<Integer>()->value();
        
        if (binop.op == "+") return Value(std::make_shared<Integer>(left + right));
        if (binop.op == "-") return Value(std::make_shared<Integer>(left - right));
        if (binop.op == "*") return Value(std::make_shared<Integer>(left * right));
        if (binop.op == "/") {
            if (right == 0) return std::unexpected("Division by zero");
            return Value(std::make_shared<Integer>(left / right));
        }
        
        // Comparison operations
        if (binop.op == ">") return Value(left > right ? Boolean::true_value() : Boolean::false_value());
        if (binop.op == ">=") return Value(left >= right ? Boolean::true_value() : Boolean::false_value());
        if (binop.op == "<") return Value(left < right ? Boolean::true_value() : Boolean::false_value());
        if (binop.op == "<=") return Value(left <= right ? Boolean::true_value() : Boolean::false_value());
        if (binop.op == "==") return Value(left == right ? Boolean::true_value() : Boolean::false_value());
        if (binop.op == "!=") return Value(left != right ? Boolean::true_value() : Boolean::false_value());
    }
    
    return std::unexpected("Unsupported binary operation for constant evaluation");
}