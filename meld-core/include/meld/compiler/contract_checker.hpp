#pragma once

#include "meld/parser/ast.hpp"
#include "meld/types/type_registry.hpp"
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>

namespace meld::compiler {

// Contract violation exception
class ContractViolationException : public std::exception {
public:
    enum class Type {
        PRECONDITION,
        POSTCONDITION
    };

    ContractViolationException(Type type, const std::string& message, const std::string& function_name)
        : type_(type), message_(message), function_name_(function_name) {}

    const char* what() const noexcept override {
        return message_.c_str();
    }

    Type getType() const { return type_; }
    const std::string& getFunctionName() const { return function_name_; }

private:
    Type type_;
    std::string message_;
    std::string function_name_;
};

// Contract verification result
struct ContractVerificationResult {
    bool is_valid = true;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    
    void addError(const std::string& error) {
        errors.push_back(error);
        is_valid = false;
    }
    
    void addWarning(const std::string& warning) {
        warnings.push_back(warning);
    }
};

// Pre-state value storage for old() expressions
class PreStateStorage {
public:
    void captureValue(const std::string& expression, const std::any& value);
    std::any getValue(const std::string& expression) const;
    bool hasValue(const std::string& expression) const;
    void clear();

private:
    std::unordered_map<std::string, std::any> values_;
};

// Contract checker for Design by Contract support
class ContractChecker {
public:
    ContractChecker(std::shared_ptr<types::TypeRegistry> type_registry);

    // Verify contracts at compile-time where possible
    ContractVerificationResult verifyContracts(const parser::ast::function_definition& func_def);
    
    // Check if a contract can be verified at compile-time
    bool canVerifyAtCompileTime(const parser::ast::expression& condition) const;
    
    // Generate runtime contract checking code
    std::string generateRuntimeChecks(const parser::ast::contract_block& contracts, 
                                    const std::string& function_name) const;
    
    // Validate contract inheritance in class hierarchies
    ContractVerificationResult validateContractInheritance(
        const parser::ast::class_definition& derived_class,
        const parser::ast::class_definition& base_class) const;
    
    // Process old() expressions and generate pre-state capture code
    std::vector<std::string> extractOldExpressions(const parser::ast::contract_block& contracts) const;
    
    // Generate pre-state capture code
    std::string generatePreStateCapture(const std::vector<std::string>& old_expressions) const;
    
    // Generate postcondition checking code
    std::string generatePostconditionChecks(const parser::ast::contract_block& contracts,
                                          const std::string& function_name) const;

private:
    std::shared_ptr<types::TypeRegistry> type_registry_;
    
    // Helper methods
    bool isConstantExpression(const parser::ast::expression& expr) const;
    bool evaluateConstantCondition(const parser::ast::expression& condition) const;
    std::string expressionToString(const parser::ast::expression& expr) const;
    void findOldExpressions(const parser::ast::expression& expr, 
                           std::vector<std::string>& old_expressions) const;
    
    // Contract inheritance validation helpers
    bool areContractsCompatible(const parser::ast::contract_block& derived,
                               const parser::ast::contract_block& base) const;
    bool isPreconditionWeaker(const parser::ast::require_clause& derived,
                             const parser::ast::require_clause& base) const;
    bool isPostconditionStronger(const parser::ast::ensure_clause& derived,
                                const parser::ast::ensure_clause& base) const;
};

// Runtime contract evaluation support
class RuntimeContractEvaluator {
public:
    RuntimeContractEvaluator();
    
    // Evaluate a contract condition at runtime
    bool evaluateCondition(const parser::ast::expression& condition,
                          const std::unordered_map<std::string, std::any>& variables) const;
    
    // Check preconditions before function execution
    void checkPreconditions(const parser::ast::contract_block& contracts,
                           const std::unordered_map<std::string, std::any>& parameters,
                           const std::string& function_name) const;
    
    // Check postconditions after function execution
    void checkPostconditions(const parser::ast::contract_block& contracts,
                            const std::unordered_map<std::string, std::any>& parameters,
                            const std::any& return_value,
                            const PreStateStorage& pre_state,
                            const std::string& function_name) const;

private:
    // Expression evaluation helpers
    std::any evaluateExpression(const parser::ast::expression& expr,
                               const std::unordered_map<std::string, std::any>& variables,
                               const PreStateStorage* pre_state = nullptr) const;
    
    std::any evaluateOldExpression(const parser::ast::old_expression& old_expr,
                                  const PreStateStorage& pre_state) const;
    
    std::any evaluateBinaryOperation(const parser::ast::binary_operation& bin_op,
                                    const std::unordered_map<std::string, std::any>& variables,
                                    const PreStateStorage* pre_state) const;
    
    std::any evaluateUnaryOperation(const parser::ast::unary_operation& unary_op,
                                   const std::unordered_map<std::string, std::any>& variables,
                                   const PreStateStorage* pre_state) const;
    
    std::any evaluateFunctionCall(const parser::ast::function_call& func_call,
                                 const std::unordered_map<std::string, std::any>& variables,
                                 const PreStateStorage* pre_state) const;
};

} // namespace meld::compiler