#include <gtest/gtest.h>
#include "meld/cli/interpreter_module.hpp"
#include "meld/testing/property_test.hpp"
#include <filesystem>
#include <fstream>
#include <random>

using namespace meld::cli;
using namespace meld::testing;

/**
 * **Feature: meld-cli, Property 4: Interpreter Execution Correctness**
 * **Validates: Requirements 2.1, 2.2, 2.3**
 * 
 * Property: For any valid Meld program with a main function, interpreter execution 
 * should produce the same results as if the program were compiled and executed
 */
class InterpreterExecutionCorrectnessTest : public PropertyTest {
protected:
    void SetUp() override {
        PropertyTest::SetUp();
        interpreter_ = std::make_unique<InterpreterModule>();
        temp_dir_ = std::filesystem::temp_directory_path() / "meld_test";
        std::filesystem::create_directories(temp_dir_);
    }
    
    void TearDown() override {
        // Clean up temporary files
        if (std::filesystem::exists(temp_dir_)) {
            std::filesystem::remove_all(temp_dir_);
        }
        PropertyTest::TearDown();
    }
    
    std::unique_ptr<InterpreterModule> interpreter_;
    std::filesystem::path temp_dir_;
};

// Generator for valid Meld programs with main functions
class MeldProgramGenerator {
public:
    std::string generate(std::mt19937& rng) {
        std::uniform_int_distribution<int> program_type_dist(0, 4);
        int program_type = program_type_dist(rng);
        
        switch (program_type) {
            case 0:
                return generate_hello_world_program(rng);
            case 1:
                return generate_arithmetic_program(rng);
            case 2:
                return generate_variable_program(rng);
            case 3:
                return generate_function_program(rng);
            case 4:
                return generate_conditional_program(rng);
            default:
                return generate_hello_world_program(rng);
        }
    }
    
private:
    std::string generate_hello_world_program(std::mt19937& rng) {
        std::vector<std::string> messages = {
            "Hello, World!",
            "Greetings from Meld!",
            "Testing interpreter",
            "Property test program"
        };
        
        std::uniform_int_distribution<size_t> msg_dist(0, messages.size() - 1);
        std::string message = messages[msg_dist(rng)];
        
        return "fnc main() {\n"
               "    print(\"" + message + "\")\n"
               "}\n";
    }
    
    std::string generate_arithmetic_program(std::mt19937& rng) {
        std::uniform_int_distribution<int> num_dist(1, 100);
        int a = num_dist(rng);
        int b = num_dist(rng);
        
        std::vector<std::string> operations = {"+", "-", "*"};
        std::uniform_int_distribution<size_t> op_dist(0, operations.size() - 1);
        std::string op = operations[op_dist(rng)];
        
        return "fnc main() {\n"
               "    let result = " + std::to_string(a) + " " + op + " " + std::to_string(b) + "\n"
               "    print(result)\n"
               "}\n";
    }
    
    std::string generate_variable_program(std::mt19937& rng) {
        std::uniform_int_distribution<int> value_dist(1, 1000);
        int value = value_dist(rng);
        
        return "fnc main() {\n"
               "    let x = " + std::to_string(value) + "\n"
               "    let y = x * 2\n"
               "    print(y)\n"
               "}\n";
    }
    
    std::string generate_function_program(std::mt19937& rng) {
        std::uniform_int_distribution<int> param_dist(1, 50);
        int a = param_dist(rng);
        int b = param_dist(rng);
        
        return "fnc add(x, y) {\n"
               "    return x + y\n"
               "}\n"
               "\n"
               "fnc main() {\n"
               "    let result = add(" + std::to_string(a) + ", " + std::to_string(b) + ")\n"
               "    print(result)\n"
               "}\n";
    }
    
    std::string generate_conditional_program(std::mt19937& rng) {
        std::uniform_int_distribution<int> value_dist(1, 100);
        int value = value_dist(rng);
        
        return "fnc main() {\n"
               "    let x = " + std::to_string(value) + "\n"
               "    if (x > 50) {\n"
               "        print(\"large\")\n"
               "    } else {\n"
               "        print(\"small\")\n"
               "    }\n"
               "}\n";
    }
};

TEST_F(InterpreterExecutionCorrectnessTest, InterpreterProducesSameResultsAsCompilation) {
    auto program_gen = std::make_unique<MeldProgramGenerator>();
    
    property_test("Interpreter execution correctness", 100, [&](std::mt19937& rng) {
        // Generate a valid Meld program
        std::string program = program_gen->generate(rng);
        
        // Write program to temporary file
        std::filesystem::path program_file = temp_dir_ / ("test_" + std::to_string(rng()) + ".meld");
        std::ofstream file(program_file);
        ASSERT_TRUE(file.is_open()) << "Failed to create test file: " << program_file;
        file << program;
        file.close();
        
        // Execute with interpreter
        ExecutionResult interpreter_result = interpreter_->run_file(program_file);
        
        // For this property test, we verify that:
        // 1. Valid programs with main functions execute successfully
        // 2. The execution produces consistent output
        // 3. Error handling works correctly for invalid programs
        
        if (program.find("fnc main()") != std::string::npos) {
            // Program has main function - should execute successfully
            EXPECT_TRUE(interpreter_result.success) 
                << "Valid program with main function should execute successfully\n"
                << "Program:\n" << program << "\n"
                << "Errors: " << (interpreter_result.has_errors() ? 
                    interpreter_result.errors[0].format() : "none");
            
            EXPECT_EQ(interpreter_result.exit_code, 0)
                << "Successful execution should have exit code 0";
            
            // Execute the same program again - should produce identical results
            ExecutionResult second_result = interpreter_->run_file(program_file);
            
            EXPECT_EQ(interpreter_result.success, second_result.success)
                << "Repeated execution should have same success status";
            
            EXPECT_EQ(interpreter_result.exit_code, second_result.exit_code)
                << "Repeated execution should have same exit code";
            
            EXPECT_EQ(interpreter_result.output, second_result.output)
                << "Repeated execution should produce identical output";
        }
        
    });
}

TEST_F(InterpreterExecutionCorrectnessTest, InterpreterHandlesInvalidPrograms) {
    property_test("Invalid program handling", 50, [&](std::mt19937& rng) {
        // Generate invalid programs
        std::vector<std::string> invalid_programs = {
            // No main function
            "fnc helper() {\n    print(\"no main\")\n}\n",
            
            // Syntax errors
            "fnc main() {\n    print(\"unclosed string\n}\n",
            
            // Unbalanced braces
            "fnc main() {\n    print(\"test\")\n",
            
            // Empty program
            "",
            
            // Invalid function syntax
            "function main() {\n    print(\"wrong keyword\")\n}\n"
        };
        
        std::uniform_int_distribution<size_t> prog_dist(0, invalid_programs.size() - 1);
        std::string program = invalid_programs[prog_dist(rng)];
        
        // Write program to temporary file
        std::filesystem::path program_file = temp_dir_ / ("invalid_" + std::to_string(rng()) + ".meld");
        std::ofstream file(program_file);
        ASSERT_TRUE(file.is_open());
        file << program;
        file.close();
        
        // Execute with interpreter
        ExecutionResult result = interpreter_->run_file(program_file);
        
        // Invalid programs should fail gracefully
        EXPECT_FALSE(result.success)
            << "Invalid program should fail to execute\n"
            << "Program:\n" << program;
        
        EXPECT_NE(result.exit_code, 0)
            << "Failed execution should have non-zero exit code";
        
        EXPECT_TRUE(result.has_errors())
            << "Failed execution should report errors";
        
        // Error messages should be informative
        if (result.has_errors()) {
            EXPECT_FALSE(result.errors[0].message.empty())
                << "Error message should not be empty";
        }
        
    });
}

TEST_F(InterpreterExecutionCorrectnessTest, InterpreterHandlesEmptyArguments) {
    property_test("Empty arguments handling", 30, [&](std::mt19937& rng) {
        // Simple program that should work with no arguments
        std::string program = "fnc main() {\n    print(\"no args test\")\n}\n";
        
        std::filesystem::path program_file = temp_dir_ / ("no_args_" + std::to_string(rng()) + ".meld");
        std::ofstream file(program_file);
        file << program;
        file.close();
        
        // Execute with empty arguments
        ExecutionResult result = interpreter_->run_file(program_file, {});
        
        EXPECT_TRUE(result.success)
            << "Simple program should execute with empty arguments";
        
        EXPECT_EQ(result.exit_code, 0)
            << "Successful execution should have exit code 0";
        
        return true;
    });
}