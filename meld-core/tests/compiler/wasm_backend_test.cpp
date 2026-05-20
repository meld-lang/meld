#include <gtest/gtest.h>
#include "meld/compiler/wasm_backend.hpp"
#include "meld/compiler/ir.hpp"
#include <memory>

using namespace meld::compiler;

class WasmBackendTest : public ::testing::Test {
protected:
    void SetUp() override {
        options_.module_name = "test_module";
        options_.output_directory = "./test_output";
        options_.generate_wat = true;
        options_.generate_wasm = false;  // Skip binary generation for tests
        options_.generate_js_bindings = true;
        options_.generate_ts_definitions = true;
        
        backend_ = std::make_unique<WasmBackend>(options_);
    }
    
    std::shared_ptr<ir::Module> create_test_module() {
        auto module = std::make_shared<ir::Module>("test");
        module->name = "test_module";
        
        // Create a simple function: add(a: int, b: int) -> int
        auto function = std::make_shared<ir::Function>();
        function->name = "add";
        
        // Parameters
        auto param_a = std::make_shared<ir::Value>();
        param_a->name = "a";
        param_a->type = ir::ValueType::Int;
        function->parameters.push_back(param_a);
        
        auto param_b = std::make_shared<ir::Value>();
        param_b->name = "b";
        param_b->type = ir::ValueType::Int;
        function->parameters.push_back(param_b);
        
        // Return value
        auto return_val = std::make_shared<ir::Value>();
        return_val->name = "result";
        return_val->type = ir::ValueType::Int;
        function->return_value = return_val;
        
        // Basic block
        auto block = std::make_shared<ir::BasicBlock>();
        block->label = "entry";
        
        // Add instruction: result = a + b
        auto add_inst = std::make_shared<ir::Instruction>();
        add_inst->opcode = ir::Opcode::Add;
        add_inst->operands.push_back(param_a);
        add_inst->operands.push_back(param_b);
        add_inst->result = return_val;
        block->instructions.push_back(add_inst);
        
        // Return instruction
        auto ret_inst = std::make_shared<ir::Instruction>();
        ret_inst->opcode = ir::Opcode::Return;
        ret_inst->operands.push_back(return_val);
        block->instructions.push_back(ret_inst);
        
        function->basic_blocks.push_back(block);
        module->functions.push_back(function);
        
        return module;
    }
    
    WasmGenerationOptions options_;
    std::unique_ptr<WasmBackend> backend_;
};

TEST_F(WasmBackendTest, GenerateWatModule) {
    auto module = create_test_module();
    
    std::string wat_content = backend_->generate_wat_module(*module);
    
    // Check that the WAT content contains expected elements
    EXPECT_TRUE(wat_content.find("(module") != std::string::npos);
    EXPECT_TRUE(wat_content.find("(func $add") != std::string::npos);
    EXPECT_TRUE(wat_content.find("(param $a i64)") != std::string::npos);
    EXPECT_TRUE(wat_content.find("(param $b i64)") != std::string::npos);
    EXPECT_TRUE(wat_content.find("(result i64)") != std::string::npos);
    EXPECT_TRUE(wat_content.find("i64.add") != std::string::npos);
    EXPECT_TRUE(wat_content.find("(export \"add\"") != std::string::npos);
}

TEST_F(WasmBackendTest, GenerateJsBindings) {
    auto module = create_test_module();
    
    std::string js_content = backend_->generate_js_bindings(*module);
    
    // Check that the JS content contains expected elements
    EXPECT_TRUE(js_content.find("class test_module") != std::string::npos);
    EXPECT_TRUE(js_content.find("async load(wasmPath)") != std::string::npos);
    EXPECT_TRUE(js_content.find("WebAssembly.instantiateStreaming") != std::string::npos);
    EXPECT_TRUE(js_content.find("add(") != std::string::npos);
    EXPECT_TRUE(js_content.find("readString") != std::string::npos);
    EXPECT_TRUE(js_content.find("writeString") != std::string::npos);
}

TEST_F(WasmBackendTest, GenerateTsDefinitions) {
    auto module = create_test_module();
    
    std::string ts_content = backend_->generate_ts_definitions(*module);
    
    // Check that the TypeScript content contains expected elements
    EXPECT_TRUE(ts_content.find("declare class test_module") != std::string::npos);
    EXPECT_TRUE(ts_content.find("load(wasmPath: string): Promise<void>") != std::string::npos);
    EXPECT_TRUE(ts_content.find("add(") != std::string::npos);
    EXPECT_TRUE(ts_content.find("readString(ptr: number): string") != std::string::npos);
    EXPECT_TRUE(ts_content.find("writeString(str: string): number") != std::string::npos);
    EXPECT_TRUE(ts_content.find("export = test_module") != std::string::npos);
}

TEST_F(WasmBackendTest, GenerateWasmFiles) {
    auto module = create_test_module();
    
    auto files = backend_->generate_wasm_files(*module);
    
    // Check that all expected files are generated
    EXPECT_TRUE(files.find("test_module.wat") != files.end());
    EXPECT_TRUE(files.find("test_module.js") != files.end());
    EXPECT_TRUE(files.find("test_module.d.ts") != files.end());
    
    // Check that files have content
    EXPECT_FALSE(files["test_module.wat"].empty());
    EXPECT_FALSE(files["test_module.js"].empty());
    EXPECT_FALSE(files["test_module.d.ts"].empty());
}

TEST_F(WasmBackendTest, TypeMapping) {
    // Test Meld type to WebAssembly type mapping
    auto int_type = backend_->map_meld_type_to_wasm(ir::ValueType::Int);
    EXPECT_EQ(int_type.wasm_type, "i64");
    EXPECT_EQ(int_type.js_type, "number");
    EXPECT_EQ(int_type.ts_type, "number");
    EXPECT_TRUE(int_type.is_primitive);
    EXPECT_FALSE(int_type.needs_memory_allocation);
    
    auto float_type = backend_->map_meld_type_to_wasm(ir::ValueType::Float);
    EXPECT_EQ(float_type.wasm_type, "f64");
    EXPECT_EQ(float_type.js_type, "number");
    EXPECT_EQ(float_type.ts_type, "number");
    
    auto bool_type = backend_->map_meld_type_to_wasm(ir::ValueType::Bool);
    EXPECT_EQ(bool_type.wasm_type, "i32");
    EXPECT_EQ(bool_type.js_type, "boolean");
    EXPECT_EQ(bool_type.ts_type, "boolean");
    
    auto string_type = backend_->map_meld_type_to_wasm(ir::ValueType::String);
    EXPECT_EQ(string_type.wasm_type, "i32");  // Pointer to string
    EXPECT_EQ(string_type.js_type, "string");
    EXPECT_EQ(string_type.ts_type, "string");
    EXPECT_FALSE(string_type.is_primitive);
    EXPECT_TRUE(string_type.needs_memory_allocation);
}

TEST_F(WasmBackendTest, IdentifierSanitization) {
    // Test that kebab-case identifiers are properly sanitized for WebAssembly
    EXPECT_EQ(backend_->sanitize_wasm_identifier("my-function"), "my_function");
    EXPECT_EQ(backend_->sanitize_wasm_identifier("test.value"), "test_value");
    EXPECT_EQ(backend_->sanitize_wasm_identifier("123invalid"), "_123invalid");
    EXPECT_EQ(backend_->sanitize_wasm_identifier("valid_name"), "valid_name");
}

TEST_F(WasmBackendTest, FunctionGeneration) {
    auto module = create_test_module();
    auto function = module->functions[0];
    
    std::string wasm_func = backend_->generate_wasm_function(*function);
    
    // Check function structure
    EXPECT_TRUE(wasm_func.find("(func $add") != std::string::npos);
    EXPECT_TRUE(wasm_func.find("(param $a i64)") != std::string::npos);
    EXPECT_TRUE(wasm_func.find("(param $b i64)") != std::string::npos);
    EXPECT_TRUE(wasm_func.find("(result i64)") != std::string::npos);
    EXPECT_TRUE(wasm_func.find("local.get $a") != std::string::npos);
    EXPECT_TRUE(wasm_func.find("local.get $b") != std::string::npos);
    EXPECT_TRUE(wasm_func.find("i64.add") != std::string::npos);
}

TEST_F(WasmBackendTest, RuntimeGeneration) {
    WasmRuntimeGenerator runtime_gen(options_);
    
    auto runtime_files = runtime_gen.generate_runtime_files();
    
    // Check that runtime files are generated
    EXPECT_TRUE(runtime_files.find("meld_runtime.wat") != runtime_files.end());
    EXPECT_TRUE(runtime_files.find("meld_runtime.js") != runtime_files.end());
    
    // Check that runtime files have content
    EXPECT_FALSE(runtime_files["meld_runtime.wat"].empty());
    EXPECT_FALSE(runtime_files["meld_runtime.js"].empty());
    
    // Check for memory management functions
    std::string wat_content = runtime_files["meld_runtime.wat"];
    EXPECT_TRUE(wat_content.find("$malloc") != std::string::npos);
    EXPECT_TRUE(wat_content.find("$free") != std::string::npos);
    
    // Check for JavaScript utilities
    std::string js_content = runtime_files["meld_runtime.js"];
    EXPECT_TRUE(js_content.find("createStringFromWasm") != std::string::npos);
    EXPECT_TRUE(js_content.find("writeStringToWasm") != std::string::npos);
}

// Property-based test for WebAssembly generation consistency
TEST_F(WasmBackendTest, GenerationConsistency) {
    auto module = create_test_module();
    
    // Generate files multiple times and ensure consistency
    auto files1 = backend_->generate_wasm_files(*module);
    auto files2 = backend_->generate_wasm_files(*module);
    
    EXPECT_EQ(files1.size(), files2.size());
    
    for (const auto& [filename, content1] : files1) {
        EXPECT_TRUE(files2.find(filename) != files2.end());
        EXPECT_EQ(content1, files2[filename]);
    }
}