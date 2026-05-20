#pragma once

#include "meld/parser/ast.hpp"
#include "meld/kernel/primitives.hpp"
#include <vector>
#include <string>
#include <memory>

// Forward declaration in the correct namespace
namespace meld::kernel {
    class Environment;
}

namespace meld::compiler {

// Alias so kernel:: resolves to meld::kernel:: (sibling namespace)
namespace kernel = ::meld::kernel;

// Test result for individual assertions
struct AssertionResult {
    bool passed;
    std::string message;
    std::string condition_text;
    size_t line_number;
    size_t column_number;
};

// Test result for a complete test block
struct TestResult {
    bool passed;
    std::string test_name;
    std::string description;
    std::vector<AssertionResult> assertion_results;
    std::string error_message;  // For compilation/runtime errors
    double execution_time_ms;
};

// Test result for a complete function
struct FunctionTestResult {
    bool all_passed;
    std::string function_name;
    std::vector<TestResult> test_results;
    size_t total_tests;
    size_t passed_tests;
    size_t failed_tests;
};

// Micro-test execution engine
class MicroTestEngine {
public:
    MicroTestEngine();
    ~MicroTestEngine();
    
    // Execute all test blocks for a function
    FunctionTestResult execute_function_tests(
        const parser::ast::function_definition& function,
        const std::shared_ptr<kernel::Environment>& env = nullptr
    );
    
    // Execute a single test block
    TestResult execute_test_block(
        const parser::ast::test_block& test,
        const std::string& function_name,
        const std::shared_ptr<kernel::Environment>& env = nullptr
    );
    
    // Execute a property-based test block with forall
    TestResult execute_property_test_block(
        const parser::ast::test_block& test,
        const std::string& function_name,
        const std::shared_ptr<kernel::Environment>& env = nullptr
    );
    
    // Execute a single assertion
    AssertionResult execute_assertion(
        const parser::ast::assertion_expression& assertion,
        const std::shared_ptr<kernel::Environment>& env = nullptr
    );
    
    // Evaluate an expression in the given environment
    kernel::Value evaluate_expression(
        const parser::ast::expression& expr,
        const std::shared_ptr<kernel::Environment>& env = nullptr
    );
    
    // Check if a value is truthy for assertion purposes
    bool is_truthy(const kernel::Value& value);
    
    // Convert expression to string for error reporting
    std::string expression_to_string(const parser::ast::expression& expr);
    
    // Get test execution statistics
    struct Statistics {
        size_t total_functions_tested = 0;
        size_t total_test_blocks = 0;
        size_t total_assertions = 0;
        size_t passed_assertions = 0;
        size_t failed_assertions = 0;
        double total_execution_time_ms = 0.0;
    };
    
    Statistics get_statistics() const { return stats_; }
    void reset_statistics() { stats_ = Statistics{}; }
    
    // Property-based testing support
    kernel::Value generate_value_for_type(const std::string& type_name);
    bool execute_property_test(
        const parser::ast::test_block& test,
        const std::shared_ptr<kernel::Environment>& env,
        TestResult& result
    );
    
    // Class hierarchy test inheritance support
    std::vector<parser::ast::test_block> collect_inherited_tests(
        const parser::ast::class_definition& class_def,
        const std::vector<parser::ast::class_definition>& all_classes
    );
    
    FunctionTestResult execute_class_method_tests(
        const parser::ast::function_definition& method,
        const parser::ast::class_definition& class_def,
        const std::vector<parser::ast::class_definition>& all_classes,
        const std::shared_ptr<kernel::Environment>& env = nullptr
    );
    
    // Configuration
    struct Config {
        bool stop_on_first_failure = false;
        bool verbose_output = false;
        double timeout_ms = 5000.0;  // 5 second timeout per test
        bool enable_property_tests = true;
        int property_test_iterations = 100;  // Default iterations for forall tests
        bool enable_shrinking = true;  // Enable counterexample shrinking
    };
    
    void set_config(const Config& config) { config_ = config; }
    const Config& get_config() const { return config_; }

private:
    Statistics stats_;
    Config config_;
    
    // Helper methods
    void update_statistics(const TestResult& result);
    std::string format_assertion_error(
        const parser::ast::assertion_expression& assertion,
        const kernel::Value& actual_value
    );
};

} // namespace meld::compiler