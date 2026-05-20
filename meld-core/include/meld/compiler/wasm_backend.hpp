#pragma once

#include "meld/compiler/ir.hpp"
#include <memory>
#include <string>
#include <sstream>
#include <map>
#include <vector>
#include <set>

namespace meld::compiler {

// WebAssembly code generation options
struct WasmGenerationOptions {
    std::string module_name = "meld_module";
    std::string output_directory = "./generated";
    bool generate_wat = true;        // Generate WebAssembly Text format
    bool generate_wasm = true;       // Generate binary .wasm file
    bool generate_js_bindings = true; // Generate JavaScript bindings
    bool generate_ts_definitions = true; // Generate TypeScript definitions
    bool optimize = true;            // Enable optimizations
    bool include_debug_info = false;
    std::string memory_size = "1";   // Memory pages (64KB each)
    bool export_memory = true;       // Export memory to host
};

// WebAssembly type mapping information
struct WasmTypeInfo {
    std::string wasm_type;          // i32, i64, f32, f64, externref
    std::string js_type;            // JavaScript type for bindings
    std::string ts_type;            // TypeScript type for definitions
    bool is_primitive;
    bool needs_memory_allocation;
    
    WasmTypeInfo() : is_primitive(false), needs_memory_allocation(false) {}
    WasmTypeInfo(std::string wasm_t, std::string js_t, std::string ts_t, 
                 bool primitive = false, bool needs_alloc = false)
        : wasm_type(std::move(wasm_t)), js_type(std::move(js_t)), ts_type(std::move(ts_t)),
          is_primitive(primitive), needs_memory_allocation(needs_alloc) {}
};

// WebAssembly backend - transpiles IR to WebAssembly Text (WAT) format
class WasmBackend {
public:
    explicit WasmBackend(const WasmGenerationOptions& options = {});
    
    // Generate WebAssembly files for a module
    std::map<std::string, std::string> generate_wasm_files(const ir::Module& module);
    
    // Generate WebAssembly Text (WAT) format
    std::string generate_wat_module(const ir::Module& module);
    
    // Generate JavaScript bindings for WebAssembly module
    std::string generate_js_bindings(const ir::Module& module);
    
    // Generate TypeScript definition file
    std::string generate_ts_definitions(const ir::Module& module);
    
    // Generate WebAssembly function from IR function
    std::string generate_wasm_function(const ir::Function& function);
    
    // Generate WebAssembly expression from IR instruction
    std::string generate_wasm_expression(const ir::Instruction& inst);
    
    // Type mapping utilities
    WasmTypeInfo map_meld_type_to_wasm(ir::ValueType meld_type, 
                                       std::shared_ptr<meta::MetaType> meta_type = nullptr);
    
    // Set generation options
    void set_options(const WasmGenerationOptions& options);
    const WasmGenerationOptions& get_options() const { return options_; }
    
    // Utility methods
    std::string get_wasm_module_header() const;
    std::string get_wasm_imports() const;
    std::string get_wasm_exports(const ir::Module& module) const;
    std::string sanitize_wasm_identifier(const std::string& name) const;
    std::string mangle_function_name(const std::string& name) const;

private:
    WasmGenerationOptions options_;
    std::set<std::string> required_imports_;
    std::map<std::string, WasmTypeInfo> type_mapping_;
    std::map<std::string, int> local_variables_;
    int next_local_index_;
    
    // Internal generation methods
    std::string generate_function_signature(const ir::Function& function);
    std::string generate_function_body(const ir::Function& function);
    std::string generate_basic_block(const ir::BasicBlock& block);
    std::string generate_instruction(const ir::Instruction& inst);
    
    // WebAssembly-specific code generation
    std::string generate_wasm_constant(const ir::Instruction& inst);
    std::string generate_wasm_arithmetic(const ir::Instruction& inst);
    std::string generate_wasm_comparison(const ir::Instruction& inst);
    std::string generate_wasm_logical(const ir::Instruction& inst);
    std::string generate_wasm_memory_access(const ir::Instruction& inst);
    std::string generate_wasm_control_flow(const ir::Instruction& inst);
    std::string generate_wasm_function_call(const ir::Instruction& inst);
    
    // Helper methods
    void initialize_type_mappings();
    void add_required_import(const std::string& import_name, const std::string& import_sig);
    std::string get_wasm_operator(ir::Opcode opcode);
    std::string get_wasm_value_name(const ir::Value& value);
    std::string get_default_value_for_type(const WasmTypeInfo& type_info);
    int allocate_local_variable(const std::string& name, const WasmTypeInfo& type_info);
    
    // Memory management helpers
    std::string generate_memory_allocation(size_t size);
    std::string generate_memory_deallocation(const std::string& ptr);
    std::string generate_string_allocation(const std::string& str);
    
    // JavaScript interop helpers
    std::string generate_js_function_wrapper(const ir::Function& function);
    std::string generate_js_type_conversion(const WasmTypeInfo& type_info, 
                                          const std::string& value, 
                                          bool to_wasm = true);
    
    // TypeScript definition helpers
    std::string generate_ts_function_signature(const ir::Function& function);
    std::string generate_ts_interface(const ir::Module& module);
};

// WebAssembly runtime library generator
class WasmRuntimeGenerator {
public:
    explicit WasmRuntimeGenerator(const WasmGenerationOptions& options);
    
    // Generate runtime support files
    std::map<std::string, std::string> generate_runtime_files();
    
    // Generate specific runtime components
    std::string generate_memory_manager();
    std::string generate_string_utilities();
    std::string generate_type_system_support();
    std::string generate_result_type_support();
    std::string generate_collection_support();
    
private:
    WasmGenerationOptions options_;
    
    std::string generate_wat_template(const std::string& content);
    std::string generate_js_template(const std::string& content);
};

} // namespace meld::compiler