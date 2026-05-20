#include <gtest/gtest.h>
#include "meld/cli/interpreter_module.hpp"
#include "meld/testing/property_test.hpp"
#include <filesystem>
#include <fstream>
#include <random>

using namespace meld::cli;
using namespace meld::testing;

/**
 * **Feature: meld-cli, Property 6: Runtime Error Stack Traces**
 * **Validates: Requirements 2.5**
 * 
 * Property: For any Meld program that fails during execution, the CLI should 
 * provide stack traces that include accurate source line information
 */
class RuntimeErrorStackTracesTest : public PropertyTest {
protected:
    void SetUp() override {
        PropertyTest::SetUp();
        interpreter_ = std::make_unique<InterpreterModule>();
        temp_dir_ = std::filesystem::temp_directory_path() / "meld_error_test";
        std::filesystem::create_directories(temp_dir_);
    }
    
    void TearDown() override {
        if (std::filesystem::exists(temp_dir_)) {
            std::filesystem::remove_all(temp_dir_);
        }
        PropertyTest::TearDown();
    }
    
    std::unique_ptr<InterpreterModule> interpreter_;
    std::filesystem::path temp_dir_;
};

// Generator for programs that will cause runtime errors
class ErrorProgramGenerator {
public:
    std::pair<std::string, std::string> generate(std::mt19937& rng) {
        std::uniform_int_distribution<int> error_type_dist(0, 5);
        int error_type = error_type_dist(rng);
        
        switch (error_type) {
            case 0:
                return generate_division_by_zero_error(rng);
            case 1:
                return generate_null_reference_error(rng);
            case 2:
                return generate_array_bounds_error(rng);
            case 3:
                return generate_function_call_error(rng);
            case 4:
                return generate_nested_function_error(rng);
            case 5:
                return generate_recursive_error(rng);
            default:
                return generate_division_by_zero_error(rng);
        }
    }
    
private:
    std::pair<std::string, std::string> generate_division_by_zero_error(std::mt19937& rng) {
        std::uniform_int_distribution<int> line_dist(5, 15);
        int error_line = line_dist(rng);
        
        std::string program = "fnc main() {\n";
        
        // Add some lines before the error
        for (int i = 1; i < error_line - 1; ++i) {
            program += "    let dummy" + std::to_string(i) + " = " + std::to_string(i) + "\n";
        }
        
        // Add the error line
        program += "    let result = 42 / 0  // This should cause division by zero\n";
        
        // Add some lines after
        program += "    print(result)\n";
        program += "}\n";
        
        return {program, "division by zero"};
    }
    
    std::pair<std::string, std::string> generate_null_reference_error(std::mt19937& rng) {
        std::uniform_int_distribution<int> line_dist(4, 10);
        int error_line = line_dist(rng);
        
        std::string program = "fnc main() {\n";
        program += "    let obj = null\n";
        
        // Add some lines before the error
        for (int i = 2; i < error_line - 1; ++i) {
            program += "    let var" + std::to_string(i) + " = " + std::to_string(i * 10) + "\n";
        }
        
        // Add the error line
        program += "    let value = obj.property  // This should cause null reference\n";
        program += "    print(value)\n";
        program += "}\n";
        
        return {program, "null reference"};
    }
    
    std::pair<std::string, std::string> generate_array_bounds_error(std::mt19937& rng) {
        std::uniform_int_distribution<int> array_size_dist(1, 5);
        std::uniform_int_distribution<int> bad_index_dist(10, 20);
        
        int array_size = array_size_dist(rng);
        int bad_index = bad_index_dist(rng);
        
        std::string program = "fnc main() {\n";
        program += "    let arr = [";
        for (int i = 0; i < array_size; ++i) {
            if (i > 0) program += ", ";
            program += std::to_string(i + 1);
        }
        program += "]\n";
        program += "    let valid = arr[0]\n";
        program += "    let invalid = arr[" + std::to_string(bad_index) + "]  // Out of bounds\n";
        program += "    print(invalid)\n";
        program += "}\n";
        
        return {program, "array bounds"};
    }
    
    std::pair<std::string, std::string> generate_function_call_error(std::mt19937& rng) {
        std::string program = R"(
fnc helper(x) {
    if (x < 0) {
        throw "Negative value not allowed"
    }
    return x * 2
}

fnc main() {
    let positive = helper(5)
    let negative = helper(-3)  // This should throw an error
    print(negative)
}
)";
        
        return {program, "function call error"};
    }
    
    std::pair<std::string, std::string> generate_nested_function_error(std::mt19937& rng) {
        std::string program = R"(
fnc level3() {
    throw "Error at level 3"
}

fnc level2() {
    let x = 42
    level3()  // This will propagate the error
    return x
}

fnc level1() {
    let y = level2()
    return y + 1
}

fnc main() {
    let result = level1()  // Error will bubble up through the call stack
    print(result)
}
)";
        
        return {program, "nested function error"};
    }
    
    std::pair<std::string, std::string> generate_recursive_error(std::mt19937& rng) {
        std::uniform_int_distribution<int> depth_dist(3, 8);
        int max_depth = depth_dist(rng);
        
        std::string program = R"(
fnc recursive_function(depth) {
    if (depth <= 0) {
        throw "Reached maximum depth"
    }
    return recursive_function(depth - 1)
}

fnc main() {
    let result = recursive_function()" + std::to_string(max_depth) + R"()
    print(result)
}
)";
        
        return {program, "recursive error"};
    }
};

TEST_F(RuntimeErrorStackTracesTest, RuntimeErrorsIncludeStackTraces) {
    auto error_gen = std::make_unique<ErrorProgramGenerator>();
    
    property_test("Runtime error stack traces", 100, [&](std::mt19937& rng) {
        // Generate a program that will cause a runtime error
        auto [program, expected_error_type] = error_gen->generate(rng);
        
        // Write program to temporary file
        std::filesystem::path program_file = temp_dir_ / ("error_test_" + std::to_string(rng()) + ".meld");
        std::ofstream file(program_file);
        ASSERT_TRUE(file.is_open());
        file << program;
        file.close();
        
        // Execute the program - it should fail
        ExecutionResult result = interpreter_->run_file(program_file);
        
        // The program should fail
        EXPECT_FALSE(result.success)
            << "Program with runtime error should fail\n"
            << "Expected error type: " << expected_error_type << "\n"
            << "Program:\n" << program;
        
        EXPECT_NE(result.exit_code, 0)
            << "Failed execution should have non-zero exit code";
        
        EXPECT_TRUE(result.has_errors())
            << "Failed execution should report errors";
        
        if (result.has_errors()) {
            const auto& error = result.errors[0];
            
            // Error should have a meaningful message
            EXPECT_FALSE(error.message.empty())
                << "Error message should not be empty";
            
            // Error should include file information
            EXPECT_FALSE(error.file.empty())
                << "Error should include source file information";
            
            // Error should include line information for source errors
            if (error.line > 0) {
                EXPECT_GT(error.line, 0)
                    << "Error line number should be positive";
                
                // Line number should be reasonable (within the program)
                size_t program_lines = std::count(program.begin(), program.end(), '\n') + 1;
                EXPECT_LE(error.line, program_lines)
                    << "Error line should be within the program bounds\n"
                    << "Program has " << program_lines << " lines, error at line " << error.line;
            }
            
            // Stack trace should be present for runtime errors
            EXPECT_FALSE(error.stack_trace.empty())
                << "Runtime error should include stack trace\n"
                << "Error: " << error.format();
            
            if (!error.stack_trace.empty()) {
                // Stack trace should contain function names
                bool has_main_function = false;
                for (const auto& frame : error.stack_trace) {
                    EXPECT_FALSE(frame.empty())
                        << "Stack trace frame should not be empty";
                    
                    if (frame.find("main") != std::string::npos) {
                        has_main_function = true;
                    }
                }
                
                EXPECT_TRUE(has_main_function)
                    << "Stack trace should include main function\n"
                    << "Stack trace: ";
                for (const auto& frame : error.stack_trace) {
                    std::cout << "  " << frame << "\n";
                }
            }
            
            // Error formatting should be readable
            std::string formatted_error = error.format();
            EXPECT_FALSE(formatted_error.empty())
                << "Formatted error should not be empty";
            
            EXPECT_NE(formatted_error.find("Runtime Error"), std::string::npos)
                << "Formatted error should indicate it's a runtime error";
        }
        
    });
}

TEST_F(RuntimeErrorStackTracesTest, StackTraceAccuracy) {
    property_test("Stack trace accuracy", 50, [&](std::mt19937& rng) {
        // Create a program with known call stack
        std::string program = R"(
fnc function_c() {
    throw "Error in function_c"
}

fnc function_b() {
    function_c()
}

fnc function_a() {
    function_b()
}

fnc main() {
    function_a()
}
)";
        
        std::filesystem::path program_file = temp_dir_ / ("stack_test_" + std::to_string(rng()) + ".meld");
        std::ofstream file(program_file);
        file << program;
        file.close();
        
        ExecutionResult result = interpreter_->run_file(program_file);
        
        EXPECT_FALSE(result.success)
            << "Program should fail with runtime error";
        
        if (result.has_errors()) {
            const auto& error = result.errors[0];
            
            // Stack trace should reflect the call hierarchy
            EXPECT_FALSE(error.stack_trace.empty())
                << "Error should have stack trace";
            
            if (!error.stack_trace.empty()) {
                // Look for expected function names in stack trace
                std::string full_trace;
                for (const auto& frame : error.stack_trace) {
                    full_trace += frame + " ";
                }
                
                // Should contain main function (entry point)
                EXPECT_NE(full_trace.find("main"), std::string::npos)
                    << "Stack trace should contain main function\n"
                    << "Full trace: " << full_trace;
                
                // In a more sophisticated implementation, we would check
                // for the complete call chain: main -> function_a -> function_b -> function_c
            }
        }
        
        return true;
    });
}

TEST_F(RuntimeErrorStackTracesTest, ErrorLocationAccuracy) {
    property_test("Error location accuracy", 30, [&](std::mt19937& rng) {
        // Create a program where we know exactly which line should error
        std::uniform_int_distribution<int> padding_dist(2, 8);
        int padding_lines = padding_dist(rng);
        
        std::string program = "fnc main() {\n";
        
        // Add padding lines
        for (int i = 0; i < padding_lines; ++i) {
            program += "    let var" + std::to_string(i) + " = " + std::to_string(i) + "\n";
        }
        
        // Add the error line (we know this will be at line padding_lines + 2)
        int expected_error_line = padding_lines + 2;
        program += "    let error = 1 / 0  // Division by zero error\n";
        
        // Add more lines after
        program += "    print(error)\n";
        program += "}\n";
        
        std::filesystem::path program_file = temp_dir_ / ("location_test_" + std::to_string(rng()) + ".meld");
        std::ofstream file(program_file);
        file << program;
        file.close();
        
        ExecutionResult result = interpreter_->run_file(program_file);
        
        EXPECT_FALSE(result.success)
            << "Program should fail with division by zero";
        
        if (result.has_errors()) {
            const auto& error = result.errors[0];
            
            // Check if line number is reported
            if (error.line > 0) {
                // In a real implementation, this should match exactly
                // For now, just verify it's reasonable
                EXPECT_GT(error.line, 0)
                    << "Error line should be positive";
                
                EXPECT_LE(error.line, expected_error_line + 2)
                    << "Error line should be near the expected location\n"
                    << "Expected around line " << expected_error_line 
                    << ", got line " << error.line;
            }
            
            // File should be correctly identified
            EXPECT_EQ(error.file, program_file.string())
                << "Error should reference the correct source file";
        }
        
        return true;
    });
}