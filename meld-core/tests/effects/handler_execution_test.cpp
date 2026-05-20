#include "../../include/meld/effects/effect.hpp"
#include "../../include/meld/stdlib/effects.hpp"
#include "../../include/meld/kernel/primitives.hpp"
#include <iostream>
#include <cassert>
#include <sstream>

using namespace meld;

// Test helper to capture console output
class ConsoleCapture {
public:
    ConsoleCapture() {
        old_cout = std::cout.rdbuf();
        std::cout.rdbuf(buffer.rdbuf());
    }
    
    ~ConsoleCapture() {
        std::cout.rdbuf(old_cout);
    }
    
    std::string get_output() {
        return buffer.str();
    }
    
private:
    std::ostringstream buffer;
    std::streambuf* old_cout;
};

// Test 1: Basic handler execution with parameter inspection
void test_parameter_inspection() {
    std::cout << "=== Test 1: Parameter Inspection ===" << std::endl;
    
    // Create a custom effect for testing
    auto test_effect = std::make_shared<effects::EffectDefinition>("TestEffect");
    test_effect->add_operation("process", {"string", "int"}, "string");
    
    auto handler = std::make_shared<effects::EffectHandler>(test_effect);
    
    // Set a handler that inspects parameters
    handler->set_handler("process",
        [](const std::vector<kernel::Value>& args, kernel::Continuation& cont) -> kernel::Value {
            // REQUIREMENT 41.15: Inspect effect parameters
            std::cout << "Handler received " << args.size() << " parameters" << std::endl;
            
            if (args.size() != 2) {
                throw std::runtime_error("Expected 2 parameters");
            }
            
            // Inspect first parameter (string)
            auto str_result = args[0].try_as<kernel::String>();
            if (!str_result.has_value()) {
                throw std::runtime_error("First parameter must be string");
            }
            std::cout << "String parameter: " << str_result.value()->value() << std::endl;
            
            // Inspect second parameter (int)
            auto int_result = args[1].try_as<kernel::Integer>();
            if (!int_result.has_value()) {
                throw std::runtime_error("Second parameter must be integer");
            }
            std::cout << "Integer parameter: " << int_result.value()->value() << std::endl;
            
            // REQUIREMENT 41.16: Modify return values
            std::string result = "Processed: " + str_result.value()->value() + 
                               " with count " + std::to_string(int_result.value()->value());
            
            return cont.resume(kernel::Value(std::make_shared<kernel::String>(result)));
        });
    
    // Test the handler
    std::vector<kernel::Value> args = {
        kernel::Value(std::make_shared<kernel::String>("test")),
        kernel::Value(std::make_shared<kernel::Integer>(42))
    };
    
    // Create a mock continuation for testing
    kernel::Continuation test_cont([](kernel::Value v) -> kernel::Value {
        return v; // Just return the value
    }, "test");
    
    auto result = handler->handle("process", args, test_cont);
    
    // Verify the result
    auto result_str = result.try_as<kernel::String>();
    assert(result_str.has_value());
    assert(result_str.value()->value() == "Processed: test with count 42");
    
    std::cout << "Result: " << result_str.value()->value() << std::endl;
    std::cout << "✓ Parameter inspection test passed" << std::endl << std::endl;
}

// Test 2: Resume with and without values
void test_resume_variations() {
    std::cout << "=== Test 2: Resume Variations ===" << std::endl;
    
    auto test_effect = std::make_shared<effects::EffectDefinition>("ResumeTest");
    test_effect->add_operation("no_value", {}, "void");
    test_effect->add_operation("with_value", {}, "string");
    
    auto handler = std::make_shared<effects::EffectHandler>(test_effect);
    
    // Handler that resumes without a value (uses Empty)
    handler->set_handler("no_value",
        [](const std::vector<kernel::Value>& args, kernel::Continuation& cont) -> kernel::Value {
            std::cout << "Resuming without specific value" << std::endl;
            // REQUIREMENT 41.5: Resume execution from suspension point
            return cont.resume(kernel::Value(kernel::Empty::instance()));
        });
    
    // Handler that resumes with a specific value
    handler->set_handler("with_value",
        [](const std::vector<kernel::Value>& args, kernel::Continuation& cont) -> kernel::Value {
            std::cout << "Resuming with specific value" << std::endl;
            // REQUIREMENT 41.16: Resume with specific return value
            return cont.resume(kernel::Value(std::make_shared<kernel::String>("custom result")));
        });
    
    // Test resuming without value
    kernel::Continuation test_cont1([](kernel::Value v) -> kernel::Value {
        return v;
    }, "test1");
    
    auto result1 = handler->handle("no_value", {}, test_cont1);
    assert(result1.is<kernel::Empty>());
    std::cout << "✓ Resume without value works" << std::endl;
    
    // Test resuming with value
    kernel::Continuation test_cont2([](kernel::Value v) -> kernel::Value {
        return v;
    }, "test2");
    
    auto result2 = handler->handle("with_value", {}, test_cont2);
    auto result2_str = result2.try_as<kernel::String>();
    assert(result2_str.has_value());
    assert(result2_str.value()->value() == "custom result");
    std::cout << "✓ Resume with value works" << std::endl << std::endl;
}

// Test 3: Handler validation and error handling
void test_handler_validation() {
    std::cout << "=== Test 3: Handler Validation ===" << std::endl;
    
    auto test_effect = std::make_shared<effects::EffectDefinition>("ValidationTest");
    test_effect->add_operation("valid_op", {"string"}, "string");
    
    auto handler = std::make_shared<effects::EffectHandler>(test_effect);
    
    // Set handler for valid operation
    handler->set_handler("valid_op",
        [](const std::vector<kernel::Value>& args, kernel::Continuation& cont) -> kernel::Value {
            return cont.resume(kernel::Value(std::make_shared<kernel::String>("success")));
        });
    
    // Test valid operation
    assert(handler->has_handler("valid_op"));
    std::cout << "✓ Valid operation handler exists" << std::endl;
    
    // Test invalid operation
    assert(!handler->has_handler("invalid_op"));
    std::cout << "✓ Invalid operation handler does not exist" << std::endl;
    
    // Test operation not in effect definition
    kernel::Continuation test_cont([](kernel::Value v) -> kernel::Value {
        return v;
    }, "test");
    
    try {
        handler->handle("undefined_op", {}, test_cont);
        assert(false && "Should have thrown exception");
    } catch (const std::runtime_error& e) {
        std::cout << "✓ Correctly rejected undefined operation: " << e.what() << std::endl;
    }
    
    // Test missing handler
    try {
        handler->handle("missing_handler", {}, test_cont);
        assert(false && "Should have thrown exception");
    } catch (const std::runtime_error& e) {
        std::cout << "✓ Correctly rejected missing handler: " << e.what() << std::endl;
    }
    
    std::cout << std::endl;
}

// Test 4: Mock filesystem handler with AI safety patterns
void test_mock_filesystem_safety() {
    std::cout << "=== Test 4: Mock Filesystem AI Safety ===" << std::endl;
    
    ConsoleCapture capture;
    
    auto handler = stdlib::create_mock_filesystem_handler();
    
    // Test write operation (should log to console)
    std::vector<kernel::Value> write_args = {
        kernel::Value(std::make_shared<kernel::String>("/tmp/test.txt")),
        kernel::Value(std::make_shared<kernel::String>("Hello, World!"))
    };
    
    kernel::Continuation write_cont([](kernel::Value v) -> kernel::Value {
        return v;
    }, "write_test");
    
    auto write_result = handler->handle("write", write_args, write_cont);
    assert(write_result.is<kernel::Empty>());
    
    // Test read operation (should add sandbox metadata)
    std::vector<kernel::Value> read_args = {
        kernel::Value(std::make_shared<kernel::String>("/tmp/test.txt"))
    };
    
    kernel::Continuation read_cont([](kernel::Value v) -> kernel::Value {
        return v;
    }, "read_test");
    
    auto read_result = handler->handle("read", read_args, read_cont);
    auto read_str = read_result.try_as<kernel::String>();
    assert(read_str.has_value());
    
    // Verify sandbox metadata was added
    std::string content = read_str.value()->value();
    assert(content.find("[SANDBOX]") == 0);
    std::cout << "Read result: " << content << std::endl;
    
    std::string output = capture.get_output();
    assert(output.find("[SANDBOX]") != std::string::npos);
    std::cout << "✓ Mock filesystem safety patterns work" << std::endl << std::endl;
}

// Test 5: Handler introspection capabilities
void test_handler_introspection() {
    std::cout << "=== Test 5: Handler Introspection ===" << std::endl;
    
    auto test_effect = std::make_shared<effects::EffectDefinition>("IntrospectionTest");
    test_effect->add_operation("op1", {}, "void");
    test_effect->add_operation("op2", {}, "void");
    test_effect->add_operation("op3", {}, "void");
    
    auto handler = std::make_shared<effects::EffectHandler>(test_effect);
    
    // Add handlers for some operations
    handler->set_handler("op1", [](const std::vector<kernel::Value>&, kernel::Continuation& cont) {
        return cont.resume(kernel::Value(kernel::Empty::instance()));
    });
    handler->set_handler("op2", [](const std::vector<kernel::Value>&, kernel::Continuation& cont) {
        return cont.resume(kernel::Value(kernel::Empty::instance()));
    });
    
    // Test introspection methods
    auto handler_names = handler->get_handler_names();
    assert(handler_names.size() == 2);
    std::cout << "Handler names: ";
    for (const auto& name : handler_names) {
        std::cout << name << " ";
    }
    std::cout << std::endl;
    
    // Test completeness check
    assert(!handler->is_complete());
    std::cout << "✓ Handler is not complete (missing op3)" << std::endl;
    
    // Test missing handlers
    auto missing = handler->get_missing_handlers();
    assert(missing.size() == 1);
    assert(missing[0] == "op3");
    std::cout << "Missing handlers: " << missing[0] << std::endl;
    
    // Add the missing handler
    handler->set_handler("op3", [](const std::vector<kernel::Value>&, kernel::Continuation& cont) {
        return cont.resume(kernel::Value(kernel::Empty::instance()));
    });
    
    assert(handler->is_complete());
    std::cout << "✓ Handler is now complete" << std::endl << std::endl;
}

// int main() {
//     std::cout << "Testing Effect Handler Execution (Task 35.6)" << std::endl;
//     std::cout << "=============================================" << std::endl << std::endl;
//     
//     try {
//         test_parameter_inspection();
//         test_resume_variations();
//         test_handler_validation();
//         test_mock_filesystem_safety();
//         test_handler_introspection();
//         
//         std::cout << "🎉 All handler execution tests passed!" << std::endl;
//         std::cout << std::endl;
//         std::cout << "Requirements validated:" << std::endl;
//         std::cout << "✓ 41.5: Handler execution with continuation support" << std::endl;
//         std::cout << "✓ 41.14: Execute handler code and capture continuation" << std::endl;
//         std::cout << "✓ 41.15: Enable handlers to inspect effect parameters" << std::endl;
//         std::cout << "✓ 41.16: Enable handlers to modify return values" << std::endl;
//         
//         return 0;
//     } catch (const std::exception& e) {
//         std::cerr << "❌ Test failed: " << e.what() << std::endl;
//         return 1;
//     }
// }