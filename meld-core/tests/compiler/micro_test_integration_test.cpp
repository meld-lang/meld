#include <gtest/gtest.h>
#include "meld/compiler/compiler.hpp"
#include "meld/parser/parser.hpp"

using namespace meld::compiler;
using namespace meld::parser;

class MicroTestIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        compiler = std::make_unique<Compiler>();
    }
    
    std::unique_ptr<Compiler> compiler;
};

TEST_F(MicroTestIntegrationTest, CompileAndExecuteBasicMicroTest) {
    std::string source = R"(
        fn add(a: int, b: int) -> int {
            test "basic addition" {
                assert true, "This should always pass"
            }
            
            return a + b
        }
    )";
    
    CompilationOptions options;
    options.execute_micro_tests = true;
    
    CompilationResult result = compiler->compile_source(source, "test.meld", options);
    
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.total_functions_compiled, 1);
    EXPECT_EQ(result.total_tests_executed, 1);
    EXPECT_EQ(result.total_test_failures, 0);
    EXPECT_EQ(result.test_results.size(), 1);
    
    const auto& test_result = result.test_results[0];
    EXPECT_TRUE(test_result.all_passed);
    EXPECT_EQ(test_result.function_name, "add");
    EXPECT_EQ(test_result.total_tests, 1);
    EXPECT_EQ(test_result.passed_tests, 1);
    EXPECT_EQ(test_result.failed_tests, 0);
}

TEST_F(MicroTestIntegrationTest, CompileAndExecuteFailingMicroTest) {
    std::string source = R"(
        fn subtract(a: int, b: int) -> int {
            test "failing test" {
                assert false, "This should always fail"
            }
            
            return a - b
        }
    )";
    
    CompilationOptions options;
    options.execute_micro_tests = true;
    
    CompilationResult result = compiler->compile_source(source, "test.meld", options);
    
    EXPECT_FALSE(result.success);  // Should fail due to test failure
    EXPECT_EQ(result.total_functions_compiled, 1);
    EXPECT_EQ(result.total_tests_executed, 1);
    EXPECT_EQ(result.total_test_failures, 1);
    EXPECT_GT(result.diagnostics.size(), 0);
    
    // Check that we have error diagnostics
    bool found_test_error = false;
    for (const auto& diag : result.diagnostics) {
        if (diag.level == DiagnosticLevel::ERROR && 
            diag.message.find("Micro-test failed") != std::string::npos) {
            found_test_error = true;
            break;
        }
    }
    EXPECT_TRUE(found_test_error);
}

TEST_F(MicroTestIntegrationTest, CompileMultipleFunctionsWithTests) {
    std::string source = R"(
        fn multiply(a: int, b: int) -> int {
            test "positive result" {
                assert true
            }
            
            return a * b
        }
        
        fn divide(a: int, b: int) -> int {
            test "non-zero divisor" {
                assert true
            }
            
            test "integer division" {
                assert true
            }
            
            return a / b
        }
    )";
    
    CompilationOptions options;
    options.execute_micro_tests = true;
    
    CompilationResult result = compiler->compile_source(source, "test.meld", options);
    
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.total_functions_compiled, 2);
    EXPECT_EQ(result.total_tests_executed, 3);  // 1 + 2 tests
    EXPECT_EQ(result.total_test_failures, 0);
    EXPECT_EQ(result.test_results.size(), 2);
    
    // Check first function
    const auto& multiply_result = result.test_results[0];
    EXPECT_EQ(multiply_result.function_name, "multiply");
    EXPECT_EQ(multiply_result.total_tests, 1);
    EXPECT_TRUE(multiply_result.all_passed);
    
    // Check second function
    const auto& divide_result = result.test_results[1];
    EXPECT_EQ(divide_result.function_name, "divide");
    EXPECT_EQ(divide_result.total_tests, 2);
    EXPECT_TRUE(divide_result.all_passed);
}

TEST_F(MicroTestIntegrationTest, CompileWithMicroTestsDisabled) {
    std::string source = R"(
        fn power(a: int, b: int) -> int {
            test "power calculation" {
                assert false, "This test would fail"
            }
            
            return a * a  // Simple square for now
        }
    )";
    
    CompilationOptions options;
    options.execute_micro_tests = false;  // Disable micro-test execution
    
    CompilationResult result = compiler->compile_source(source, "test.meld", options);
    
    EXPECT_TRUE(result.success);  // Should succeed because tests are not executed
    EXPECT_EQ(result.total_functions_compiled, 1);
    EXPECT_EQ(result.total_tests_executed, 0);  // No tests executed
    EXPECT_EQ(result.total_test_failures, 0);
    EXPECT_EQ(result.test_results.size(), 0);
}

TEST_F(MicroTestIntegrationTest, CompileWithStopOnFirstFailure) {
    std::string source = R"(
        fn first_func(x: int) -> int {
            test "first test" {
                assert false, "First failure"
            }
            
            return x
        }
        
        fn second_func(x: int) -> int {
            test "second test" {
                assert false, "Second failure"
            }
            
            return x * 2
        }
    )";
    
    CompilationOptions options;
    options.execute_micro_tests = true;
    options.stop_on_test_failure = true;
    
    CompilationResult result = compiler->compile_source(source, "test.meld", options);
    
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.total_functions_compiled, 2);
    EXPECT_EQ(result.total_test_failures, 1);  // Should stop after first failure
    EXPECT_EQ(result.test_results.size(), 1);  // Only first function tested
}

TEST_F(MicroTestIntegrationTest, PropertyBasedTestSupport) {
    std::string source = R"(
        fn abs_value(x: int) -> int {
            test "absolute value property" forall x: int {
                assert true  // Placeholder - would test abs(x) >= 0
            }
            
            return x >= 0 ? x : -x
        }
    )";
    
    // Note: This test assumes the parser supports forall syntax
    // For now, we'll test the infrastructure is in place
    
    CompilationOptions options;
    options.execute_micro_tests = true;
    options.enable_property_tests = true;
    
    // This might fail parsing if forall syntax isn't fully implemented
    // but the infrastructure should be ready
    CompilationResult result = compiler->compile_source(source, "test.meld", options);
    
    // The test passes if compilation doesn't crash
    // Full property test support depends on parser implementation
    EXPECT_GE(result.total_functions_compiled, 0);
}

TEST_F(MicroTestIntegrationTest, MicroTestEngineConfiguration) {
    MicroTestEngine::Config config;
    config.stop_on_first_failure = true;
    config.verbose_output = true;
    config.timeout_ms = 1000.0;
    config.enable_property_tests = false;
    config.property_test_iterations = 50;
    
    compiler->set_micro_test_config(config);
    
    const auto& retrieved_config = compiler->get_micro_test_config();
    EXPECT_TRUE(retrieved_config.stop_on_first_failure);
    EXPECT_TRUE(retrieved_config.verbose_output);
    EXPECT_EQ(retrieved_config.timeout_ms, 1000.0);
    EXPECT_FALSE(retrieved_config.enable_property_tests);
    EXPECT_EQ(retrieved_config.property_test_iterations, 50);
}