#include "../../include/meld/compiler/jvm_backend.hpp"
#include "../../include/meld/compiler/ir.hpp"
#include <iostream>
#include <cassert>

using namespace meld::compiler;
using namespace meld::compiler::ir;

void test_basic_jvm_generation() {
    std::cout << "Testing basic JVM code generation..." << std::endl;
    
    // Create a simple IR module
    auto module = std::make_shared<Module>("TestModule");
    
    // Create a simple function: int add(int a, int b) { return a + b; }
    auto func = module->create_function("add");
    func->return_value = std::make_shared<Value>("result", ValueType::Int);
    func->parameters.push_back(std::make_shared<Value>("a", ValueType::Int));
    func->parameters.push_back(std::make_shared<Value>("b", ValueType::Int));
    
    // Create entry block
    auto entry = func->create_block("entry");
    
    // Create add instruction
    auto result = std::make_shared<Value>("sum", ValueType::Int);
    auto add_inst = Instruction::create_binary_op(
        Opcode::Add,
        result,
        func->parameters[0],
        func->parameters[1]
    );
    entry->add_instruction(add_inst);
    
    // Create return instruction
    auto ret_inst = Instruction::create_return(result);
    entry->add_instruction(ret_inst);
    
    // Generate Java code
    JVMGenerationOptions options;
    options.package_name = "test.generated";
    options.use_modern_java = true;
    options.java_version = 17;
    
    JVMBackend backend(options);
    auto java_files = backend.generate_java_files(*module);
    
    // Verify output
    assert(!java_files.empty());
    std::cout << "Generated " << java_files.size() << " Java files" << std::endl;
    
    for (const auto& [filename, content] : java_files) {
        std::cout << "\n=== " << filename << " ===" << std::endl;
        std::cout << content << std::endl;
        
        // Basic validation
        assert(content.find("package test.generated;") != std::string::npos);
        assert(content.find("public class") != std::string::npos);
    }
    
    std::cout << "Basic JVM generation test passed!" << std::endl;
}

void test_type_mapping() {
    std::cout << "\nTesting type mapping..." << std::endl;
    
    JVMGenerationOptions options;
    JVMBackend backend(options);
    
    // Test primitive type mappings
    auto int_type = backend.map_meld_type_to_java(ValueType::Int);
    assert(int_type.java_type == "long");
    assert(int_type.is_primitive);
    
    auto float_type = backend.map_meld_type_to_java(ValueType::Float);
    assert(float_type.java_type == "double");
    assert(float_type.is_primitive);
    
    auto bool_type = backend.map_meld_type_to_java(ValueType::Bool);
    assert(bool_type.java_type == "boolean");
    assert(bool_type.is_primitive);
    
    auto string_type = backend.map_meld_type_to_java(ValueType::String);
    assert(string_type.java_type == "String");
    assert(!string_type.is_primitive);
    
    std::cout << "Type mapping test passed!" << std::endl;
}

void test_identifier_sanitization() {
    std::cout << "\nTesting identifier sanitization..." << std::endl;
    
    JVMGenerationOptions options;
    JVMBackend backend(options);
    
    // Test kebab-case conversion
    std::string kebab = backend.sanitize_java_identifier("my-function-name");
    assert(kebab == "my_function_name");
    
    // Test Java keyword handling
    std::string keyword = backend.sanitize_java_identifier("class");
    assert(keyword == "meld_class");
    
    // Test invalid starting character
    std::string invalid_start = backend.sanitize_java_identifier("123name");
    assert(invalid_start[0] == '_');
    
    std::cout << "Identifier sanitization test passed!" << std::endl;
}

void test_runtime_generation() {
    std::cout << "\nTesting runtime class generation..." << std::endl;
    
    JVMGenerationOptions options;
    options.package_name = "test.runtime";
    
    JVMRuntimeGenerator runtime_gen(options);
    auto runtime_classes = runtime_gen.generate_runtime_classes();
    
    // Verify runtime classes
    assert(runtime_classes.count("MeldResult") > 0);
    assert(runtime_classes.count("MeldOption") > 0);
    assert(runtime_classes.count("MeldRuntime") > 0);
    
    // Verify Result class content
    const auto& result_class = runtime_classes["MeldResult"];
    assert(result_class.find("package test.runtime;") != std::string::npos);
    assert(result_class.find("public class MeldResult") != std::string::npos);
    assert(result_class.find("success") != std::string::npos);
    assert(result_class.find("error") != std::string::npos);
    assert(result_class.find("map") != std::string::npos);
    assert(result_class.find("flatMap") != std::string::npos);
    
    std::cout << "Runtime generation test passed!" << std::endl;
}

void test_method_generation() {
    std::cout << "\nTesting method generation..." << std::endl;
    
    // Create a function with multiple operations
    auto module = std::make_shared<Module>("TestModule");
    auto func = module->create_function("calculate");
    func->return_value = std::make_shared<Value>("result", ValueType::Int);
    func->parameters.push_back(std::make_shared<Value>("x", ValueType::Int));
    func->parameters.push_back(std::make_shared<Value>("y", ValueType::Int));
    
    auto entry = func->create_block("entry");
    
    // x + y
    auto sum = std::make_shared<Value>("sum", ValueType::Int);
    auto add_inst = Instruction::create_binary_op(Opcode::Add, sum, func->parameters[0], func->parameters[1]);
    entry->add_instruction(add_inst);
    
    // sum * 2
    auto two = std::make_shared<Value>("two", ValueType::Int);
    auto const_inst = Instruction::create_const_int(two, 2);
    entry->add_instruction(const_inst);
    
    auto result = std::make_shared<Value>("result", ValueType::Int);
    auto mul_inst = Instruction::create_binary_op(Opcode::Mul, result, sum, two);
    entry->add_instruction(mul_inst);
    
    // return result
    auto ret_inst = Instruction::create_return(result);
    entry->add_instruction(ret_inst);
    
    // Generate Java method
    JVMGenerationOptions options;
    JVMBackend backend(options);
    std::string method_code = backend.generate_java_method(*func, true);
    
    // Verify method structure
    assert(method_code.find("public static long calculate") != std::string::npos);
    assert(method_code.find("long x") != std::string::npos);
    assert(method_code.find("long y") != std::string::npos);
    assert(method_code.find("return") != std::string::npos);
    
    std::cout << "Method generation test passed!" << std::endl;
}

