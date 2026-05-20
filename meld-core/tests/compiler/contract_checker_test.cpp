#include <gtest/gtest.h>
#include "meld/compiler/contract_checker.hpp"
#include "meld/parser/ast.hpp"
#include "meld/types/type_registry.hpp"
#include <memory>

using namespace meld::compiler;
using namespace meld::parser::ast;

class ContractCheckerTest : public ::testing::Test {
protected:
    void SetUp() override {
        type_registry_ = std::shared_ptr<meld::types::TypeRegistry>(
            &meld::types::TypeRegistry::instance(), [](auto*){});
        checker_ = std::make_unique<ContractChecker>(type_registry_);
    }

    std::shared_ptr<meld::types::TypeRegistry> type_registry_;
    std::unique_ptr<ContractChecker> checker_;
};

// Test contract verification for functions without contracts
TEST_F(ContractCheckerTest, NoContractsReturnsValid) {
    function_definition func_def;
    func_def.name.name = "test_function";
    func_def.has_contracts = false;
    
    auto result = checker_->verifyContracts(func_def);
    
    EXPECT_TRUE(result.is_valid);
    EXPECT_TRUE(result.errors.empty());
    EXPECT_TRUE(result.warnings.empty());
}

// Test contract verification with simple precondition
TEST_F(ContractCheckerTest, SimplePreconditionVerification) {
    function_definition func_def;
    func_def.name.name = "test_function";
    func_def.has_contracts = true;
    
    // Create a simple require clause
    require_clause req;
    
    // Create condition: x > 0
    binary_operation condition;
    condition.op = ">";
    
    identifier left_operand;
    left_operand.name = "x";
    condition.left = expression(left_operand);
    
    integer_literal right_operand;
    right_operand.value = 0;
    condition.right = expression(right_operand);
    
    req.condition = expression(condition);
    req.message = "x must be positive";
    req.has_message = true;
    
    func_def.contracts.preconditions.push_back(req);
    func_def.contracts.has_requires = true;
    
    auto result = checker_->verifyContracts(func_def);
    
    // Should be valid (cannot verify at compile-time, but no obvious errors)
    EXPECT_TRUE(result.is_valid);
}

// Test contract verification with simple postcondition
TEST_F(ContractCheckerTest, SimplePostconditionVerification) {
    function_definition func_def;
    func_def.name.name = "test_function";
    func_def.has_contracts = true;
    
    // Create a simple ensure clause
    ensure_clause ens;
    
    // Create condition: result > 0
    binary_operation condition;
    condition.op = ">";
    
    identifier left_operand;
    left_operand.name = "result";
    condition.left = expression(left_operand);
    
    integer_literal right_operand;
    right_operand.value = 0;
    condition.right = expression(right_operand);
    
    ens.condition = expression(condition);
    ens.message = "result must be positive";
    ens.has_message = true;
    
    func_def.contracts.postconditions.push_back(ens);
    func_def.contracts.has_ensures = true;
    
    auto result = checker_->verifyContracts(func_def);
    
    // Should be valid (cannot verify at compile-time, but no obvious errors)
    EXPECT_TRUE(result.is_valid);
}

// Test runtime checks generation
TEST_F(ContractCheckerTest, GenerateRuntimeChecks) {
    contract_block contracts;
    
    // Create a require clause
    require_clause req;
    binary_operation req_condition;
    req_condition.op = ">";
    
    identifier req_left;
    req_left.name = "x";
    req_condition.left = expression(req_left);
    
    integer_literal req_right;
    req_right.value = 0;
    req_condition.right = expression(req_right);
    
    req.condition = expression(req_condition);
    req.message = "x must be positive";
    req.has_message = true;
    
    contracts.preconditions.push_back(req);
    contracts.has_requires = true;
    
    std::string runtime_checks = checker_->generateRuntimeChecks(contracts, "test_function");
    
    EXPECT_FALSE(runtime_checks.empty());
    EXPECT_NE(runtime_checks.find("Precondition checks"), std::string::npos);
    EXPECT_NE(runtime_checks.find("test_function"), std::string::npos);
}

// Test old() expression extraction
TEST_F(ContractCheckerTest, ExtractOldExpressions) {
    contract_block contracts;
    
    // Create an ensure clause with old() expression
    ensure_clause ens;
    
    // Create condition with old() expression: result == old(x) + 1
    binary_operation condition;
    condition.op = "==";
    
    identifier left_operand;
    left_operand.name = "result";
    condition.left = expression(left_operand);
    
    // Create old(x) + 1 expression
    binary_operation right_operand;
    right_operand.op = "+";
    
    old_expression old_expr;
    identifier old_var;
    old_var.name = "x";
    old_expr.expression = expression(old_var);
    right_operand.left = expression(old_expr);
    
    integer_literal one;
    one.value = 1;
    right_operand.right = expression(one);
    
    condition.right = expression(right_operand);
    ens.condition = expression(condition);
    
    contracts.postconditions.push_back(ens);
    contracts.has_ensures = true;
    
    auto old_expressions = checker_->extractOldExpressions(contracts);
    
    // Should find at least one old() expression
    EXPECT_FALSE(old_expressions.empty());
}

// Test contract inheritance validation
TEST_F(ContractCheckerTest, ContractInheritanceValidation) {
    class_definition base_class;
    base_class.name.name = "BaseClass";
    
    class_definition derived_class;
    derived_class.name.name = "DerivedClass";
    
    // Add a method to both classes
    function_definition base_method;
    base_method.name.name = "test_method";
    base_class.methods.push_back(base_method);
    
    function_definition derived_method;
    derived_method.name.name = "test_method";
    derived_class.methods.push_back(derived_method);
    
    auto result = checker_->validateContractInheritance(derived_class, base_class);
    
    // Should be valid for methods without contracts
    EXPECT_TRUE(result.is_valid);
}

// Test PreStateStorage functionality
TEST_F(ContractCheckerTest, PreStateStorage) {
    PreStateStorage storage;
    
    // Test capturing and retrieving values
    std::string expr = "x";
    int value = 42;
    
    storage.captureValue(expr, value);
    
    EXPECT_TRUE(storage.hasValue(expr));
    
    auto retrieved = storage.getValue(expr);
    EXPECT_EQ(std::any_cast<int>(retrieved), value);
    
    // Test clearing storage
    storage.clear();
    EXPECT_FALSE(storage.hasValue(expr));
}

// Test RuntimeContractEvaluator
TEST_F(ContractCheckerTest, RuntimeContractEvaluator) {
    RuntimeContractEvaluator evaluator;
    
    // Create a simple condition: x > 0
    binary_operation condition;
    condition.op = ">";
    
    identifier left_operand;
    left_operand.name = "x";
    condition.left = expression(left_operand);
    
    integer_literal right_operand;
    right_operand.value = 0;
    condition.right = expression(right_operand);
    
    // Test with valid value
    std::unordered_map<std::string, std::any> valid_vars;
    valid_vars["x"] = 5;
    
    // Note: This test is simplified since the actual expression evaluation
    // would require a more complete implementation
    // For now, we just test that the evaluator can be created
    EXPECT_NO_THROW(RuntimeContractEvaluator());
}

// Test ContractViolationException
TEST_F(ContractCheckerTest, ContractViolationException) {
    ContractViolationException precondition_ex(
        ContractViolationException::Type::PRECONDITION,
        "Test precondition violation",
        "test_function"
    );
    
    EXPECT_EQ(precondition_ex.getType(), ContractViolationException::Type::PRECONDITION);
    EXPECT_EQ(precondition_ex.getFunctionName(), "test_function");
    EXPECT_STREQ(precondition_ex.what(), "Test precondition violation");
    
    ContractViolationException postcondition_ex(
        ContractViolationException::Type::POSTCONDITION,
        "Test postcondition violation",
        "test_function"
    );
    
    EXPECT_EQ(postcondition_ex.getType(), ContractViolationException::Type::POSTCONDITION);
}

// Test contract verification result
TEST_F(ContractCheckerTest, ContractVerificationResult) {
    ContractVerificationResult result;
    
    EXPECT_TRUE(result.is_valid);
    EXPECT_TRUE(result.errors.empty());
    EXPECT_TRUE(result.warnings.empty());
    
    result.addError("Test error");
    EXPECT_FALSE(result.is_valid);
    EXPECT_EQ(result.errors.size(), 1);
    EXPECT_EQ(result.errors[0], "Test error");
    
    result.addWarning("Test warning");
    EXPECT_EQ(result.warnings.size(), 1);
    EXPECT_EQ(result.warnings[0], "Test warning");
}