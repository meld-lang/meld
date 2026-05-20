#include <gtest/gtest.h>
#include "../../include/meld/compiler/cpp_backend.hpp"
#include "../../include/meld/compiler/ir.hpp"
#include <memory>

using namespace meld::compiler;

class CppBackendTest : public ::testing::Test {
protected:
    void SetUp() override {
        options_.namespace_name = "meld::test";
        options_.cpp_standard = "17";
        options_.use_smart_pointers = true;
        backend_ = std::make_unique<CppBackend>(options_);
    }

    CppGenerationOptions options_;
    std::unique_ptr<CppBackend> backend_;
};

TEST_F(CppBackendTest, BasicTypeMapping) {
    // Test basic type mappings
    auto int_type = backend_->map_meld_type_to_cpp(ir::ValueType::Int);
    EXPECT_EQ(int_type.cpp_type, "std::int64_t");
    EXPECT_TRUE(int_type.is_primitive);
    EXPECT_FALSE(int_type.needs_smart_pointer);

    auto float_type = backend_->map_meld_type_to_cpp(ir::ValueType::Float);
    EXPECT_EQ(float_type.cpp_type, "double");
    EXPECT_TRUE(float_type.is_primitive);

    auto bool_type = backend_->map_meld_type_to_cpp(ir::ValueType::Bool);
    EXPECT_EQ(bool_type.cpp_type, "bool");
    EXPECT_TRUE(bool_type.is_primitive);

    auto string_type = backend_->map_meld_type_to_cpp(ir::ValueType::String);
    EXPECT_EQ(string_type.cpp_type, "std::string");
    EXPECT_FALSE(string_type.is_primitive);
}

TEST_F(CppBackendTest, IdentifierSanitization) {
    // Test C++ identifier sanitization
    EXPECT_EQ(backend_->sanitize_cpp_identifier("hello-world"), "hello_world");
    EXPECT_EQ(backend_->sanitize_cpp_identifier("class"), "meld_class");
    EXPECT_EQ(backend_->sanitize_cpp_identifier("namespace"), "meld_namespace");
    EXPECT_EQ(backend_->sanitize_cpp_identifier("123invalid"), "_123invalid");
    EXPECT_EQ(backend_->sanitize_cpp_identifier("valid_name"), "valid_name");
}

TEST_F(CppBackendTest, NamespaceGeneration) {
    // Test namespace declaration generation
    std::string ns_decl = backend_->get_cpp_namespace_declaration();
    EXPECT_EQ(ns_decl, "namespace meld::test");
}

TEST_F(CppBackendTest, IncludeGeneration) {
    // Test include generation
    std::string includes = backend_->get_cpp_includes();
    EXPECT_TRUE(includes.find("#include <memory>") != std::string::npos);
    EXPECT_TRUE(includes.find("#include <string>") != std::string::npos);
    EXPECT_TRUE(includes.find("#include <cstdint>") != std::string::npos);
}

TEST_F(CppBackendTest, SimpleModuleGeneration) {
    // Create a simple IR module
    ir::Module module("TestModule");
    
    // Add a simple function
    auto func = std::make_shared<ir::Function>();
    func->name = "test_function";
    
    // Add return value
    auto return_val = std::make_shared<ir::Value>();
    return_val->name = "result";
    return_val->type = ir::ValueType::Int;
    func->return_value = return_val;
    
    // Add a parameter
    auto param = std::make_shared<ir::Value>();
    param->name = "input";
    param->type = ir::ValueType::Int;
    func->parameters.push_back(param);
    
    // Add basic block
    auto block = std::make_shared<ir::BasicBlock>();
    block->label = "entry";
    
    // Add a simple return instruction
    auto ret_inst = std::make_shared<ir::Instruction>();
    ret_inst->opcode = ir::Opcode::Return;
    ret_inst->operands.push_back(param);
    block->instructions.push_back(ret_inst);
    
    func->basic_blocks.push_back(block);
    module.functions.push_back(func);
    
    // Generate C++ files
    auto files = backend_->generate_cpp_files(module);
    
    // Check that files were generated
    EXPECT_TRUE(files.find("TestModule.hpp") != files.end());
    EXPECT_TRUE(files.find("TestModule.cpp") != files.end());
    EXPECT_TRUE(files.find("CMakeLists.txt") != files.end());
    
    // Check header content
    std::string header = files["TestModule.hpp"];
    EXPECT_TRUE(header.find("#pragma once") != std::string::npos);
    EXPECT_TRUE(header.find("namespace meld::test") != std::string::npos);
    EXPECT_TRUE(header.find("class TestModule") != std::string::npos);
    EXPECT_TRUE(header.find("test_function") != std::string::npos);
    
    // Check implementation content
    std::string impl = files["TestModule.cpp"];
    EXPECT_TRUE(impl.find("#include \"TestModule.hpp\"") != std::string::npos);
    EXPECT_TRUE(impl.find("namespace meld::test") != std::string::npos);
}

TEST_F(CppBackendTest, FunctionSignatureGeneration) {
    // Create a function with parameters and return type
    ir::Function func;
    func.name = "calculate";
    
    // Return value
    auto return_val = std::make_shared<ir::Value>();
    return_val->name = "result";
    return_val->type = ir::ValueType::Float;
    func.return_value = return_val;
    
    // Parameters
    auto param1 = std::make_shared<ir::Value>();
    param1->name = "x";
    param1->type = ir::ValueType::Int;
    func.parameters.push_back(param1);
    
    auto param2 = std::make_shared<ir::Value>();
    param2->name = "y";
    param2->type = ir::ValueType::Float;
    func.parameters.push_back(param2);
    
    // Generate function signature
    std::string signature = backend_->generate_cpp_function(func, true);
    
    EXPECT_TRUE(signature.find("static double calculate") != std::string::npos);
    EXPECT_TRUE(signature.find("std::int64_t x") != std::string::npos);
    EXPECT_TRUE(signature.find("double y") != std::string::npos);
}

TEST_F(CppBackendTest, ConstantGeneration) {
    // Test constant instruction generation
    ir::Instruction inst;
    inst.opcode = ir::Opcode::ConstInt;
    inst.constant_value = static_cast<int64_t>(42);
    
    auto result = std::make_shared<ir::Value>();
    result->name = "const_val";
    result->type = ir::ValueType::Int;
    inst.result = result;
    
    std::string code = backend_->generate_cpp_expression(inst);
    EXPECT_TRUE(code.find("std::int64_t const_val = 42LL") != std::string::npos);
}

TEST_F(CppBackendTest, ArithmeticGeneration) {
    // Test arithmetic instruction generation
    ir::Instruction inst;
    inst.opcode = ir::Opcode::Add;
    
    auto operand1 = std::make_shared<ir::Value>();
    operand1->name = "a";
    operand1->type = ir::ValueType::Int;
    
    auto operand2 = std::make_shared<ir::Value>();
    operand2->name = "b";
    operand2->type = ir::ValueType::Int;
    
    auto result = std::make_shared<ir::Value>();
    result->name = "sum";
    result->type = ir::ValueType::Int;
    
    inst.operands.push_back(operand1);
    inst.operands.push_back(operand2);
    inst.result = result;
    
    std::string code = backend_->generate_cpp_expression(inst);
    EXPECT_TRUE(code.find("std::int64_t sum = a + b") != std::string::npos);
}

TEST_F(CppBackendTest, SmartPointerGeneration) {
    // Test smart pointer generation when enabled
    options_.use_smart_pointers = true;
    backend_->set_options(options_);
    
    auto ptr_type = backend_->map_meld_type_to_cpp(ir::ValueType::Pointer);
    EXPECT_TRUE(ptr_type.cpp_type.find("std::shared_ptr") != std::string::npos);
    EXPECT_TRUE(ptr_type.needs_smart_pointer);
}

TEST_F(CppBackendTest, CMakeGeneration) {
    // Test CMakeLists.txt generation
    ir::Module module("TestProject");
    
    std::string cmake_content = backend_->generate_cmake_file(module);
    
    EXPECT_TRUE(cmake_content.find("cmake_minimum_required(VERSION 3.17)") != std::string::npos);
    EXPECT_TRUE(cmake_content.find("project(TestProject VERSION 1.0.0)") != std::string::npos);
    EXPECT_TRUE(cmake_content.find("set(CMAKE_CXX_STANDARD 17)") != std::string::npos);
    EXPECT_TRUE(cmake_content.find("add_executable(${PROJECT_NAME}") != std::string::npos);
}

TEST_F(CppBackendTest, RuntimeGeneration) {
    // Test runtime library generation
    CppRuntimeGenerator runtime_gen(options_);
    auto runtime_files = runtime_gen.generate_runtime_files();
    
    EXPECT_TRUE(runtime_files.find("MeldResult.hpp") != runtime_files.end());
    EXPECT_TRUE(runtime_files.find("MeldOption.hpp") != runtime_files.end());
    EXPECT_TRUE(runtime_files.find("MeldRuntime.hpp") != runtime_files.end());
    
    // Check Result template content
    std::string result_header = runtime_files["MeldResult.hpp"];
    EXPECT_TRUE(result_header.find("template<typename T, typename E>") != std::string::npos);
    EXPECT_TRUE(result_header.find("class Result") != std::string::npos);
    EXPECT_TRUE(result_header.find("static Result<T, E> success") != std::string::npos);
    EXPECT_TRUE(result_header.find("static Result<T, E> error") != std::string::npos);
    
    // Check Option template content
    std::string option_header = runtime_files["MeldOption.hpp"];
    EXPECT_TRUE(option_header.find("template<typename T>") != std::string::npos);
    EXPECT_TRUE(option_header.find("class Option") != std::string::npos);
    EXPECT_TRUE(option_header.find("static Option<T> some") != std::string::npos);
    EXPECT_TRUE(option_header.find("static Option<T> none") != std::string::npos);
}

TEST_F(CppBackendTest, StructGeneration) {
    // Test struct generation
    ir::Module module("TestStruct");
    
    // Add struct fields
    auto field1 = std::make_shared<ir::Value>();
    field1->name = "field_x";
    field1->type = ir::ValueType::Int;
    module.globals.push_back(field1);
    
    auto field2 = std::make_shared<ir::Value>();
    field2->name = "field_y";
    field2->type = ir::ValueType::Float;
    module.globals.push_back(field2);
    
    std::string struct_code = backend_->generate_cpp_struct(module, "Point");
    
    EXPECT_TRUE(struct_code.find("struct Point") != std::string::npos);
    EXPECT_TRUE(struct_code.find("std::int64_t x;") != std::string::npos);
    EXPECT_TRUE(struct_code.find("double y;") != std::string::npos);
    EXPECT_TRUE(struct_code.find("Point() = default;") != std::string::npos);
}

TEST_F(CppBackendTest, ClassGeneration) {
    // Test class generation
    ir::Module module("TestClass");
    
    // Add class fields
    auto field1 = std::make_shared<ir::Value>();
    field1->name = "field_name";
    field1->type = ir::ValueType::String;
    module.globals.push_back(field1);
    
    auto field2 = std::make_shared<ir::Value>();
    field2->name = "field_age";
    field2->type = ir::ValueType::Int;
    module.globals.push_back(field2);
    
    std::string class_code = backend_->generate_cpp_class(module, "Person");
    
    EXPECT_TRUE(class_code.find("class Person") != std::string::npos);
    EXPECT_TRUE(class_code.find("std::string name_;") != std::string::npos);
    EXPECT_TRUE(class_code.find("std::int64_t age_;") != std::string::npos);
    EXPECT_TRUE(class_code.find("const std::string& name() const") != std::string::npos);
    EXPECT_TRUE(class_code.find("virtual ~Person() = default;") != std::string::npos);
}

// Property-based test for type mapping consistency
TEST_F(CppBackendTest, TypeMappingConsistency) {
    // **Property: Type Mapping Consistency**
    // **Validates: Requirements 34.3**
    
    // For all Meld types, mapping to C++ should be consistent and valid
    std::vector<ir::ValueType> test_types = {
        ir::ValueType::Int,
        ir::ValueType::Float,
        ir::ValueType::Bool,
        ir::ValueType::String,
        ir::ValueType::Void,
        ir::ValueType::Pointer,
        ir::ValueType::Struct,
        ir::ValueType::Function
    };
    
    for (auto meld_type : test_types) {
        auto cpp_type = backend_->map_meld_type_to_cpp(meld_type);
        
        // All mapped types should have non-empty names
        EXPECT_FALSE(cpp_type.cpp_type.empty()) 
            << "Type mapping for " << static_cast<int>(meld_type) << " should not be empty";
        
        // Primitive types should not need smart pointers
        if (cpp_type.is_primitive) {
            EXPECT_FALSE(cpp_type.needs_smart_pointer)
                << "Primitive type " << cpp_type.cpp_type << " should not need smart pointers";
        }
        
        // Non-primitive types should have proper C++ syntax
        if (!cpp_type.is_primitive) {
            EXPECT_TRUE(cpp_type.cpp_type.find("std::") != std::string::npos || 
                       cpp_type.cpp_type.find("*") != std::string::npos ||
                       cpp_type.cpp_type == "struct")
                << "Non-primitive type " << cpp_type.cpp_type << " should use std:: or be a pointer";
        }
    }
}

// Property-based test for identifier sanitization
TEST_F(CppBackendTest, IdentifierSanitizationProperty) {
    // **Property: Identifier Sanitization Safety**
    // **Validates: Requirements 34.3**
    
    // For all input strings, sanitized identifiers should be valid C++ identifiers
    std::vector<std::string> test_identifiers = {
        "hello-world",
        "class",
        "namespace", 
        "123invalid",
        "valid_name",
        "my-function-name",
        "struct",
        "template",
        "const",
        "auto",
        "special.chars%here",
        "_underscore_start",
        "CamelCase",
        "snake_case"
    };
    
    for (const auto& input : test_identifiers) {
        std::string sanitized = backend_->sanitize_cpp_identifier(input);
        
        // Should not be empty
        EXPECT_FALSE(sanitized.empty()) 
            << "Sanitized identifier for '" << input << "' should not be empty";
        
        // Should start with letter or underscore
        EXPECT_TRUE(std::isalpha(sanitized[0]) || sanitized[0] == '_')
            << "Sanitized identifier '" << sanitized << "' should start with letter or underscore";
        
        // Should not be a C++ keyword (if it was, should be prefixed)
        static const std::set<std::string> cpp_keywords = {
            "class", "namespace", "struct", "template", "const", "auto"
        };
        
        if (cpp_keywords.count(input)) {
            EXPECT_TRUE(sanitized.find("meld_") == 0)
                << "C++ keyword '" << input << "' should be prefixed with 'meld_'";
        }
        
        // Should only contain valid identifier characters
        for (char c : sanitized) {
            EXPECT_TRUE(std::isalnum(c) || c == '_')
                << "Sanitized identifier '" << sanitized << "' contains invalid character '" << c << "'";
        }
    }
}