#include "meld/compiler/contract_checker.hpp"
#include "meld/parser/ast.hpp"
#include "meld/compat/visit.hpp"
#include <sstream>
#include <algorithm>

namespace meld::compiler {

// PreStateStorage implementation
void PreStateStorage::captureValue(const std::string& expression, const std::any& value) {
    values_[expression] = value;
}

std::any PreStateStorage::getValue(const std::string& expression) const {
    auto it = values_.find(expression);
    if (it != values_.end()) {
        return it->second;
    }
    throw std::runtime_error("Pre-state value not found for expression: " + expression);
}

bool PreStateStorage::hasValue(const std::string& expression) const {
    return values_.find(expression) != values_.end();
}

void PreStateStorage::clear() {
    values_.clear();
}

// ContractChecker implementation
ContractChecker::ContractChecker(std::shared_ptr<types::TypeRegistry> type_registry)
    : type_registry_(type_registry) {}

ContractVerificationResult ContractChecker::verifyContracts(const parser::ast::function_definition& func_def) {
    ContractVerificationResult result;
    
    if (!func_def.has_contracts) {
        return result; // No contracts to verify
    }
    
    const auto& contracts = func_def.contracts;
    
    // Verify preconditions
    for (const auto& require_clause : contracts.preconditions) {
        if (canVerifyAtCompileTime(require_clause.condition)) {
            try {
                bool is_valid = evaluateConstantCondition(require_clause.condition);
                if (!is_valid) {
                    std::string error = "Precondition always fails: " + expressionToString(require_clause.condition);
                    if (require_clause.has_message) {
                        error += " (" + require_clause.message + ")";
                    }
                    result.addError(error);
                }
            } catch (const std::exception& e) {
                result.addWarning("Cannot verify precondition at compile-time: " + std::string(e.what()));
            }
        }
    }
    
    // Verify postconditions
    for (const auto& ensure_clause : contracts.postconditions) {
        if (canVerifyAtCompileTime(ensure_clause.condition)) {
            try {
                bool is_valid = evaluateConstantCondition(ensure_clause.condition);
                if (!is_valid) {
                    std::string error = "Postcondition always fails: " + expressionToString(ensure_clause.condition);
                    if (ensure_clause.has_message) {
                        error += " (" + ensure_clause.message + ")";
                    }
                    result.addError(error);
                }
            } catch (const std::exception& e) {
                result.addWarning("Cannot verify postcondition at compile-time: " + std::string(e.what()));
            }
        }
    }
    
    return result;
}

bool ContractChecker::canVerifyAtCompileTime(const parser::ast::expression& condition) const {
    return isConstantExpression(condition);
}

std::string ContractChecker::generateRuntimeChecks(const parser::ast::contract_block& contracts, 
                                                  const std::string& function_name) const {
    std::ostringstream code;
    
    // Generate precondition checks
    if (contracts.has_requires) {
        code << "    // Precondition checks\n";
        for (size_t i = 0; i < contracts.preconditions.size(); ++i) {
            const auto& require_clause = contracts.preconditions[i];
            code << "    if (!(" << expressionToString(require_clause.condition) << ")) {\n";
            code << "        std::string message = \"Precondition violation in " << function_name << "\";\n";
            if (require_clause.has_message) {
                code << "        message += \": " << require_clause.message << "\";\n";
            }
            code << "        throw meld::compiler::ContractViolationException(\n";
            code << "            meld::compiler::ContractViolationException::Type::PRECONDITION,\n";
            code << "            message, \"" << function_name << "\");\n";
            code << "    }\n";
        }
    }
    
    // Generate pre-state capture for old() expressions
    auto old_expressions = extractOldExpressions(contracts);
    if (!old_expressions.empty()) {
        code << "\n    // Capture pre-state values for old() expressions\n";
        code << "    meld::compiler::PreStateStorage pre_state;\n";
        for (const auto& expr : old_expressions) {
            code << "    pre_state.captureValue(\"" << expr << "\", " << expr << ");\n";
        }
    }
    
    return code.str();
}

std::string ContractChecker::generatePostconditionChecks(const parser::ast::contract_block& contracts,
                                                        const std::string& function_name) const {
    std::ostringstream code;
    
    if (contracts.has_ensures) {
        code << "    // Postcondition checks\n";
        for (size_t i = 0; i < contracts.postconditions.size(); ++i) {
            const auto& ensure_clause = contracts.postconditions[i];
            code << "    if (!(" << expressionToString(ensure_clause.condition) << ")) {\n";
            code << "        std::string message = \"Postcondition violation in " << function_name << "\";\n";
            if (ensure_clause.has_message) {
                code << "        message += \": " << ensure_clause.message << "\";\n";
            }
            code << "        throw meld::compiler::ContractViolationException(\n";
            code << "            meld::compiler::ContractViolationException::Type::POSTCONDITION,\n";
            code << "            message, \"" << function_name << "\");\n";
            code << "    }\n";
        }
    }
    
    return code.str();
}

ContractVerificationResult ContractChecker::validateContractInheritance(
    const parser::ast::class_definition& derived_class,
    const parser::ast::class_definition& base_class) const {
    
    ContractVerificationResult result;
    
    // class_definition does not yet have a 'methods' member in the AST.
    // Contract inheritance validation will be implemented once the AST
    // supports method definitions on classes.
    (void)derived_class;
    (void)base_class;
    
    return result;
}

std::vector<std::string> ContractChecker::extractOldExpressions(const parser::ast::contract_block& contracts) const {
    std::vector<std::string> old_expressions;
    
    // Extract old() expressions from postconditions
    for (const auto& ensure_clause : contracts.postconditions) {
        findOldExpressions(ensure_clause.condition, old_expressions);
    }
    
    // Remove duplicates
    std::sort(old_expressions.begin(), old_expressions.end());
    old_expressions.erase(std::unique(old_expressions.begin(), old_expressions.end()), old_expressions.end());
    
    return old_expressions;
}

std::string ContractChecker::generatePreStateCapture(const std::vector<std::string>& old_expressions) const {
    std::ostringstream code;
    
    if (!old_expressions.empty()) {
        code << "    // Capture pre-state values\n";
        code << "    meld::compiler::PreStateStorage pre_state;\n";
        for (const auto& expr : old_expressions) {
            code << "    pre_state.captureValue(\"" << expr << "\", " << expr << ");\n";
        }
    }
    
    return code.str();
}

// Helper methods
bool ContractChecker::isConstantExpression(const parser::ast::expression& expr) const {
    // This is a simplified implementation
    // In a full implementation, we would need to analyze the expression tree
    // to determine if it contains only constants and no variable references
    
    // For now, we'll return false to indicate that compile-time verification
    // is not available for most expressions
    return false;
}

bool ContractChecker::evaluateConstantCondition(const parser::ast::expression& condition) const {
    // This is a placeholder implementation
    // In a full implementation, we would evaluate constant expressions
    throw std::runtime_error("Constant expression evaluation not yet implemented");
}

std::string ContractChecker::expressionToString(const parser::ast::expression& expr) const {
    // This is a simplified implementation that converts AST expressions to strings
    // In a full implementation, we would need a proper AST-to-string converter
    
    return meld::compat::visit<std::string>([this](const auto& e) -> std::string {
        using T = std::decay_t<decltype(e)>;
        
        if constexpr (std::is_same_v<T, parser::ast::identifier>) {
            return e.name;
        }
        else if constexpr (std::is_same_v<T, parser::ast::integer_literal>) {
            return std::to_string(e.value);
        }
        else if constexpr (std::is_same_v<T, parser::ast::float_literal>) {
            return std::to_string(e.value);
        }
        else if constexpr (std::is_same_v<T, parser::ast::string_literal>) {
            return "\"" + e.value + "\"";
        }
        else if constexpr (std::is_same_v<T, parser::ast::boolean_literal>) {
            return e.value ? "true" : "false";
        }
        else if constexpr (std::is_same_v<T, parser::ast::old_expression>) {
            return "old(" + expressionToString(e.expression) + ")";
        }
        else {
            return "/* complex expression */";
        }
    }, expr);
}

void ContractChecker::findOldExpressions(const parser::ast::expression& expr, 
                                        std::vector<std::string>& old_expressions) const {
    meld::compat::visit([this, &old_expressions](const auto& e) {
        using T = std::decay_t<decltype(e)>;
        
        if constexpr (std::is_same_v<T, parser::ast::old_expression>) {
            old_expressions.push_back(expressionToString(e.expression));
        }
        // For other expression types, we would recursively search for old() expressions
        // This is a simplified implementation
    }, expr);
}

bool ContractChecker::areContractsCompatible(const parser::ast::contract_block& derived,
                                           const parser::ast::contract_block& base) const {
    // Contract inheritance rules:
    // 1. Derived class can weaken preconditions (add OR conditions)
    // 2. Derived class can strengthen postconditions (add AND conditions)
    
    // For simplicity, we'll just check that the number of contracts doesn't decrease
    // A full implementation would need semantic analysis of the conditions
    
    return derived.preconditions.size() <= base.preconditions.size() && 
           derived.postconditions.size() >= base.postconditions.size();
}

bool ContractChecker::isPreconditionWeaker(const parser::ast::require_clause& derived,
                                         const parser::ast::require_clause& base) const {
    // This would require semantic analysis to determine if the derived precondition
    // is logically weaker than the base precondition
    // For now, we'll return true as a placeholder
    return true;
}

bool ContractChecker::isPostconditionStronger(const parser::ast::ensure_clause& derived,
                                            const parser::ast::ensure_clause& base) const {
    // This would require semantic analysis to determine if the derived postcondition
    // is logically stronger than the base postcondition
    // For now, we'll return true as a placeholder
    return true;
}

// RuntimeContractEvaluator implementation
RuntimeContractEvaluator::RuntimeContractEvaluator() {}

bool RuntimeContractEvaluator::evaluateCondition(const parser::ast::expression& condition,
                                               const std::unordered_map<std::string, std::any>& variables) const {
    try {
        auto result = evaluateExpression(condition, variables);
        // Convert result to boolean
        if (result.type() == typeid(bool)) {
            return std::any_cast<bool>(result);
        }
        // For other types, consider non-zero/non-empty as true
        return true; // Simplified implementation
    } catch (const std::exception&) {
        return false; // If evaluation fails, consider condition false
    }
}

void RuntimeContractEvaluator::checkPreconditions(const parser::ast::contract_block& contracts,
                                                const std::unordered_map<std::string, std::any>& parameters,
                                                const std::string& function_name) const {
    for (const auto& require_clause : contracts.preconditions) {
        if (!evaluateCondition(require_clause.condition, parameters)) {
            std::string message = "Precondition violation in " + function_name;
            if (require_clause.has_message) {
                message += ": " + require_clause.message;
            }
            throw ContractViolationException(ContractViolationException::Type::PRECONDITION, 
                                           message, function_name);
        }
    }
}

void RuntimeContractEvaluator::checkPostconditions(const parser::ast::contract_block& contracts,
                                                  const std::unordered_map<std::string, std::any>& parameters,
                                                  const std::any& return_value,
                                                  const PreStateStorage& pre_state,
                                                  const std::string& function_name) const {
    // Create variable map including parameters, return value, and pre-state
    auto variables = parameters;
    variables["result"] = return_value;
    
    for (const auto& ensure_clause : contracts.postconditions) {
        if (!evaluateCondition(ensure_clause.condition, variables)) {
            std::string message = "Postcondition violation in " + function_name;
            if (ensure_clause.has_message) {
                message += ": " + ensure_clause.message;
            }
            throw ContractViolationException(ContractViolationException::Type::POSTCONDITION, 
                                           message, function_name);
        }
    }
}

std::any RuntimeContractEvaluator::evaluateExpression(const parser::ast::expression& expr,
                                                    const std::unordered_map<std::string, std::any>& variables,
                                                    const PreStateStorage* pre_state) const {
    return meld::compat::visit<std::any>([this, &variables, pre_state](const auto& e) -> std::any {
        using T = std::decay_t<decltype(e)>;
        
        if constexpr (std::is_same_v<T, parser::ast::identifier>) {
            auto it = variables.find(e.name);
            if (it != variables.end()) {
                return it->second;
            }
            throw std::runtime_error("Variable not found: " + e.name);
        }
        else if constexpr (std::is_same_v<T, parser::ast::integer_literal>) {
            return e.value;
        }
        else if constexpr (std::is_same_v<T, parser::ast::float_literal>) {
            return e.value;
        }
        else if constexpr (std::is_same_v<T, parser::ast::string_literal>) {
            return e.value;
        }
        else if constexpr (std::is_same_v<T, parser::ast::boolean_literal>) {
            return e.value;
        }
        else if constexpr (std::is_same_v<T, parser::ast::old_expression>) {
            if (pre_state) {
                return evaluateOldExpression(e, *pre_state);
            }
            throw std::runtime_error("old() expression used without pre-state storage");
        }
        else {
            throw std::runtime_error("Unsupported expression type in contract evaluation");
        }
    }, expr);
}

std::any RuntimeContractEvaluator::evaluateOldExpression(const parser::ast::old_expression& old_expr,
                                                       const PreStateStorage& pre_state) const {
    std::string expr_str = ""; // Would need proper expression-to-string conversion
    return pre_state.getValue(expr_str);
}

std::any RuntimeContractEvaluator::evaluateBinaryOperation(const parser::ast::binary_operation& bin_op,
                                                         const std::unordered_map<std::string, std::any>& variables,
                                                         const PreStateStorage* pre_state) const {
    // This would implement binary operation evaluation
    // For now, return a placeholder
    return true;
}

std::any RuntimeContractEvaluator::evaluateUnaryOperation(const parser::ast::unary_operation& unary_op,
                                                        const std::unordered_map<std::string, std::any>& variables,
                                                        const PreStateStorage* pre_state) const {
    // This would implement unary operation evaluation
    // For now, return a placeholder
    return true;
}

std::any RuntimeContractEvaluator::evaluateFunctionCall(const parser::ast::function_call& func_call,
                                                      const std::unordered_map<std::string, std::any>& variables,
                                                      const PreStateStorage* pre_state) const {
    // This would implement function call evaluation
    // For now, return a placeholder
    return true;
}

} // namespace meld::compiler