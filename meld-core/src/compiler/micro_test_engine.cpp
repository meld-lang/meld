#include "meld/compiler/micro_test_engine.hpp"
#include "meld/kernel/operations.hpp"
#include "meld/compat/visit.hpp"
#include <boost/variant/get.hpp>
#include <chrono>
#include <sstream>
#include <random>

namespace meld::compiler {

// Alias so that unqualified kernel:: resolves to meld::kernel
// (sibling namespace lookup is not automatic in C++)
namespace kernel = ::meld::kernel;

MicroTestEngine::MicroTestEngine() = default;
MicroTestEngine::~MicroTestEngine() = default;

FunctionTestResult MicroTestEngine::execute_function_tests(
    const parser::ast::function_definition& function,
    const std::shared_ptr<kernel::Environment>& env
) {
    FunctionTestResult result;
    result.function_name = function.name.name;
    result.total_tests = function.tests.size();
    result.passed_tests = 0;
    result.failed_tests = 0;
    result.all_passed = true;
    
    if (!function.has_tests) {
        return result;
    }
    
    stats_.total_functions_tested++;
    
    // Execute each test block
    for (size_t i = 0; i < function.tests.size(); ++i) {
        const auto& test_block = function.tests[i];
        
        std::string test_name = function.name.name + "_test_" + std::to_string(i);
        if (test_block.has_description) {
            test_name += "_" + test_block.description;
        }
        
        TestResult test_result = execute_test_block(test_block, test_name, env);
        result.test_results.push_back(test_result);
        
        if (test_result.passed) {
            result.passed_tests++;
        } else {
            result.failed_tests++;
            result.all_passed = false;
            
            if (config_.stop_on_first_failure) {
                break;
            }
        }
        
        update_statistics(test_result);
    }
    
    return result;
}

TestResult MicroTestEngine::execute_test_block(
    const parser::ast::test_block& test,
    const std::string& function_name,
    const std::shared_ptr<kernel::Environment>& env
) {
    TestResult result;
    result.test_name = function_name;
    result.description = test.has_description ? test.description : "";
    result.passed = true;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    stats_.total_test_blocks++;
    
    try {
        // Check if this is a property-based test
        if (test.is_property_test && config_.enable_property_tests) {
            return execute_property_test_block(test, function_name, env);
        }
        
        // Execute each statement in the test block
        for (const auto& stmt : test.body.get().statements) {
            // Check if this is an assertion — use boost::get since expression
            // is a boost::spirit::x3::variant, not std::variant
            auto* assertion = boost::get<boost::spirit::x3::forward_ast<parser::ast::assertion_expression>>(&stmt.get());
            if (assertion) {
                AssertionResult assertion_result = execute_assertion(assertion->get(), env);
                result.assertion_results.push_back(assertion_result);
                
                if (!assertion_result.passed) {
                    result.passed = false;
                }
            } else {
                // For now, skip non-assertion statements
                // In a full implementation, these would be evaluated
            }
        }
        
    } catch (const std::exception& e) {
        result.passed = false;
        result.error_message = "Test execution error: " + std::string(e.what());
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    result.execution_time_ms = duration.count() / 1000.0;
    
    return result;
}

TestResult MicroTestEngine::execute_property_test_block(
    const parser::ast::test_block& test,
    const std::string& function_name,
    const std::shared_ptr<kernel::Environment>& env
) {
    TestResult result;
    result.test_name = function_name + "_property";
    result.description = test.has_description ? test.description : "property test";
    result.passed = true;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    stats_.total_test_blocks++;
    
    try {
        // Execute property test with multiple iterations
        bool property_passed = execute_property_test(test, env, result);
        result.passed = property_passed;
        
    } catch (const std::exception& e) {
        result.passed = false;
        result.error_message = "Property test execution error: " + std::string(e.what());
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    result.execution_time_ms = duration.count() / 1000.0;
    
    return result;
}

AssertionResult MicroTestEngine::execute_assertion(
    const parser::ast::assertion_expression& assertion,
    const std::shared_ptr<kernel::Environment>& env
) {
    AssertionResult result;
    result.condition_text = expression_to_string(assertion.condition.get());
    result.line_number = 0; // TODO: Get from AST position
    result.column_number = 0; // TODO: Get from AST position
    
    stats_.total_assertions++;
    
    try {
        kernel::Value condition_value = evaluate_expression(assertion.condition.get(), env);
        result.passed = is_truthy(condition_value);
        
        if (result.passed) {
            result.message = "Assertion passed";
            stats_.passed_assertions++;
        } else {
            if (assertion.has_message) {
                result.message = assertion.message;
            } else {
                result.message = format_assertion_error(assertion, condition_value);
            }
            stats_.failed_assertions++;
        }
        
    } catch (const std::exception& e) {
        result.passed = false;
        result.message = "Assertion evaluation error: " + std::string(e.what());
        stats_.failed_assertions++;
    }
    
    return result;
}

kernel::Value MicroTestEngine::evaluate_expression(
    const parser::ast::expression& expr,
    const std::shared_ptr<kernel::Environment>& env
) {
    // Simple evaluation for basic literals using meld::compat::visit
    // since expression is a boost::spirit::x3::variant, not std::variant.
    return meld::compat::visit<kernel::Value>([&](auto const& node) -> kernel::Value {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, parser::ast::boolean_literal>) {
            return kernel::Value(kernel::Boolean::from(node.value));
        }
        else if constexpr (std::is_same_v<T, parser::ast::integer_literal>) {
            return kernel::Value(std::make_shared<kernel::Integer>(node.value));
        }
        else if constexpr (std::is_same_v<T, parser::ast::string_literal>) {
            return kernel::Value(std::make_shared<kernel::String>(node.value));
        }
        else {
            // For now, return a default value for unsupported expressions
            return kernel::Value(kernel::Boolean::from(false));
        }
    }, expr);
}

bool MicroTestEngine::is_truthy(const kernel::Value& value) {
    // Use the Value class's own is<T>()/as<T>() API
    if (value.is<kernel::Boolean>()) {
        return value.as<kernel::Boolean>()->value();
    }
    
    // Nil is falsy
    if (value.is<kernel::Empty>()) {
        return false;
    }
    
    // Numbers: 0 is falsy, everything else is truthy
    if (value.is<kernel::Integer>()) {
        return value.as<kernel::Integer>()->value() != 0;
    }
    
    // Strings: empty string is falsy
    if (value.is<kernel::String>()) {
        return !value.as<kernel::String>()->value().empty();
    }
    
    // Everything else is truthy
    return true;
}

std::string MicroTestEngine::expression_to_string(const parser::ast::expression& expr) {
    // Use meld::compat::visit since expression is a boost variant
    return meld::compat::visit<std::string>([&](auto const& node) -> std::string {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, parser::ast::identifier>) {
            return node.name;
        }
        else if constexpr (std::is_same_v<T, parser::ast::integer_literal>) {
            return std::to_string(node.value);
        }
        else if constexpr (std::is_same_v<T, parser::ast::float_literal>) {
            return std::to_string(node.value);
        }
        else if constexpr (std::is_same_v<T, parser::ast::string_literal>) {
            return "\"" + node.value + "\"";
        }
        else if constexpr (std::is_same_v<T, parser::ast::boolean_literal>) {
            return node.value ? "true" : "false";
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::binary_operation>>) {
            const auto& bin_op = node.get();
            return expression_to_string(bin_op.left.get()) + " " + bin_op.op + " " + expression_to_string(bin_op.right.get());
        }
        else {
            return "<expression>";
        }
    }, expr);
}

void MicroTestEngine::update_statistics(const TestResult& result) {
    stats_.total_execution_time_ms += result.execution_time_ms;
}

std::string MicroTestEngine::format_assertion_error(
    const parser::ast::assertion_expression& assertion,
    const kernel::Value& actual_value
) {
    std::ostringstream oss;
    oss << "Assertion failed: " << expression_to_string(assertion.condition.get());
    
    // Try to provide more context about the failure
    auto* bin_op_ptr = boost::get<boost::spirit::x3::forward_ast<parser::ast::binary_operation>>(&assertion.condition.get());
    if (bin_op_ptr) {
        oss << " (expected " << bin_op_ptr->get().op << " to be true)";
    }
    
    return oss.str();
}

kernel::Value MicroTestEngine::generate_value_for_type(const std::string& type_name) {
    // Simple value generation for basic types
    // In a full implementation, this would be more sophisticated
    
    static std::random_device rd;
    static std::mt19937 gen(rd());
    
    if (type_name == "int") {
        std::uniform_int_distribution<int> dist(-100, 100);
        return kernel::Value(std::make_shared<kernel::Integer>(dist(gen)));
    }
    
    if (type_name == "bool") {
        std::uniform_int_distribution<int> dist(0, 1);
        return kernel::Value(kernel::Boolean::from(dist(gen) == 1));
    }
    
    if (type_name == "string") {
        std::uniform_int_distribution<size_t> length_dist(0, 10);
        std::uniform_int_distribution<int> char_dist('a', 'z');
        
        size_t length = length_dist(gen);
        std::string result;
        for (size_t i = 0; i < length; ++i) {
            result += static_cast<char>(char_dist(gen));
        }
        return kernel::Value(std::make_shared<kernel::String>(result));
    }
    
    if (type_name == "float") {
        std::uniform_real_distribution<double> dist(-100.0, 100.0);
        return kernel::Value(std::make_shared<kernel::Integer>(static_cast<int64_t>(dist(gen))));
    }
    
    // Default to integer for unknown types
    return kernel::Value(std::make_shared<kernel::Integer>(42));
}

bool MicroTestEngine::execute_property_test(
    const parser::ast::test_block& test,
    const std::shared_ptr<kernel::Environment>& env,
    TestResult& result
) {
    int iterations = config_.property_test_iterations;
    bool all_passed = true;
    
    for (int i = 0; i < iterations; ++i) {
        // Create a new environment for this iteration
        auto test_env = env; // In a full implementation, this would be a copy
        
        // Generate values for forall variables
        for (const auto& var : test.forall_variables) {
            // For now, assume all variables are integers
            // In a full implementation, this would use type information
            kernel::Value generated_value = generate_value_for_type("int");
            
            // Bind the variable in the test environment
            // In a full implementation, this would use proper environment binding
        }
        
        // Execute assertions with generated values
        for (const auto& stmt : test.body.get().statements) {
            auto* assertion = boost::get<boost::spirit::x3::forward_ast<parser::ast::assertion_expression>>(&stmt.get());
            if (assertion) {
                AssertionResult assertion_result = execute_assertion(assertion->get(), test_env);
                
                if (!assertion_result.passed) {
                    // Property test failed - record the failure
                    assertion_result.message = "Property test failed on iteration " + 
                                             std::to_string(i + 1) + ": " + assertion_result.message;
                    result.assertion_results.push_back(assertion_result);
                    all_passed = false;
                    
                    if (config_.stop_on_first_failure) {
                        return false;
                    }
                }
            }
        }
        
        if (!all_passed && config_.stop_on_first_failure) {
            break;
        }
    }
    
    // If all iterations passed, record a successful property test
    if (all_passed) {
        AssertionResult success_result;
        success_result.passed = true;
        success_result.message = "Property test passed for " + std::to_string(iterations) + " iterations";
        success_result.condition_text = "forall property";
        result.assertion_results.push_back(success_result);
    }
    
    return all_passed;
}

std::vector<parser::ast::test_block> MicroTestEngine::collect_inherited_tests(
    const parser::ast::class_definition& class_def,
    const std::vector<parser::ast::class_definition>& all_classes
) {
    std::vector<parser::ast::test_block> inherited_tests;
    
    // For now, this is a placeholder implementation
    // In a full implementation, this would:
    // 1. Find the parent class(es) of class_def
    // 2. Recursively collect tests from parent classes
    // 3. Handle test overriding and visibility rules
    // 4. Merge tests appropriately
    
    // Simple implementation: just return the class's own tests
    // This would be enhanced when the class system is fully implemented
    
    return inherited_tests;
}

FunctionTestResult MicroTestEngine::execute_class_method_tests(
    const parser::ast::function_definition& method,
    const parser::ast::class_definition& class_def,
    const std::vector<parser::ast::class_definition>& all_classes,
    const std::shared_ptr<kernel::Environment>& env
) {
    FunctionTestResult result;
    result.function_name = class_def.name.name + "::" + method.name.name;
    result.total_tests = method.tests.size();
    result.passed_tests = 0;
    result.failed_tests = 0;
    result.all_passed = true;
    
    if (!method.has_tests) {
        return result;
    }
    
    stats_.total_functions_tested++;
    
    // Collect inherited tests
    auto inherited_tests = collect_inherited_tests(class_def, all_classes);
    
    // Execute method's own tests
    for (size_t i = 0; i < method.tests.size(); ++i) {
        const auto& test_block = method.tests[i];
        
        std::string test_name = result.function_name + "_test_" + std::to_string(i);
        if (test_block.has_description) {
            test_name += "_" + test_block.description;
        }
        
        TestResult test_result = execute_test_block(test_block, test_name, env);
        result.test_results.push_back(test_result);
        
        if (test_result.passed) {
            result.passed_tests++;
        } else {
            result.failed_tests++;
            result.all_passed = false;
            
            if (config_.stop_on_first_failure) {
                break;
            }
        }
        
        update_statistics(test_result);
    }
    
    // Execute inherited tests (if any)
    for (size_t i = 0; i < inherited_tests.size(); ++i) {
        const auto& test_block = inherited_tests[i];
        
        std::string test_name = result.function_name + "_inherited_test_" + std::to_string(i);
        if (test_block.has_description) {
            test_name += "_" + test_block.description;
        }
        
        TestResult test_result = execute_test_block(test_block, test_name, env);
        result.test_results.push_back(test_result);
        
        if (test_result.passed) {
            result.passed_tests++;
        } else {
            result.failed_tests++;
            result.all_passed = false;
            
            if (config_.stop_on_first_failure) {
                break;
            }
        }
        
        update_statistics(test_result);
    }
    
    result.total_tests = result.passed_tests + result.failed_tests;
    
    return result;
}

} // namespace meld::compiler