#pragma once

#include "meld/parser/ast.hpp"
#include "meld/meta/metatype.hpp"
#include "meld/kernel/primitives.hpp"
#include <expected>
#include <string>
#include <unordered_map>

namespace meld::compiler {

// Compile-time refinement type validation
class RefinementChecker {
public:
    // Check if a value expression can be statically validated against a refinement type
    std::expected<bool, std::string> validate_static_assignment(
        const parser::ast::expression& value_expr,
        std::shared_ptr<meta::RefinementMetaType> refinement_type
    );
    
    // Analyze a refinement predicate for static validation opportunities
    struct PredicateAnalysis {
        bool is_statically_analyzable;
        std::string analysis_error;
        
        // For simple predicates we can analyze
        enum class PredicateType {
            UNKNOWN,
            COMPARISON,      // it > 0, it >= 5, etc.
            RANGE_CHECK,     // it >= 0 && it <= 100
            STRING_LENGTH,   // it.length > 0
            REGEX_MATCH,     // it.matches(/pattern/)
            FUNCTION_CALL    // custom_validator(it)
        } type = PredicateType::UNKNOWN;
        
        // For comparison predicates
        std::string operator_symbol;  // ">", ">=", "==", etc.
        kernel::Value comparison_value;
        
        // For range checks
        kernel::Value min_value;
        kernel::Value max_value;
        bool has_min = false;
        bool has_max = false;
    };
    
    PredicateAnalysis analyze_predicate(const parser::ast::expression& predicate);
    
    // Check if a literal value satisfies a refinement constraint at compile time
    std::expected<bool, std::string> validate_literal_value(
        const parser::ast::expression& literal,
        const PredicateAnalysis& analysis
    );
    
    // Generate compile-time error messages for refinement violations
    std::string generate_violation_message(
        const parser::ast::expression& value_expr,
        std::shared_ptr<meta::RefinementMetaType> refinement_type,
        const std::string& violation_reason
    );
    
    // Check if an expression is a compile-time constant
    bool is_compile_time_constant(const parser::ast::expression& expr);
    
    // Evaluate compile-time constant expressions
    std::expected<kernel::Value, std::string> evaluate_constant(const parser::ast::expression& expr);
    
private:
    // Helper methods for predicate analysis
    PredicateAnalysis analyze_binary_operation(const parser::ast::binary_operation& binop);
    PredicateAnalysis analyze_function_call(const parser::ast::function_call& call);
    PredicateAnalysis analyze_identifier_access(const parser::ast::identifier& id);
    
    // Helper methods for constant evaluation
    std::expected<kernel::Value, std::string> evaluate_literal(const parser::ast::expression& expr);
    std::expected<kernel::Value, std::string> evaluate_binary_op(const parser::ast::binary_operation& binop);
    
    // Cache for analyzed predicates
    std::unordered_map<std::string, PredicateAnalysis> predicate_cache_;
};

// Compile-time refinement type errors
class RefinementValidationError : public std::exception {
public:
    explicit RefinementValidationError(std::string message, 
                                       std::string location = "",
                                       std::string suggestion = "")
        : message_(std::move(message))
        , location_(std::move(location))
        , suggestion_(std::move(suggestion)) {}
    
    const char* what() const noexcept override {
        return message_.c_str();
    }
    
    const std::string& message() const { return message_; }
    const std::string& location() const { return location_; }
    const std::string& suggestion() const { return suggestion_; }
    
private:
    std::string message_;
    std::string location_;
    std::string suggestion_;
};

} // namespace meld::compiler