#pragma once

#include "meld/compiler/ir.hpp"
#include "meld/meta/metatype.hpp"
#include <memory>
#include <string>
#include <sstream>
#include <map>

namespace meld::compiler {

// Code generation target
enum class CodeGenTarget {
    CPP,        // C++ code
    LLVM_IR,    // LLVM IR
    Bytecode,   // Custom bytecode
    JavaScript, // JavaScript (for WebAssembly interop)
    JVM,        // Java source code
    Go          // Go source code
};

// Code generator - translates IR to target code
class CodeGenerator {
public:
    explicit CodeGenerator(CodeGenTarget target = CodeGenTarget::CPP);
    
    // Generate code for a module
    std::string generate(const ir::Module& module);
    
    // Generate code for a function
    std::string generate_function(const ir::Function& function);
    
    // Generate code for a basic block
    std::string generate_block(const ir::BasicBlock& block);
    
    // Generate code for an instruction
    std::string generate_instruction(const ir::Instruction& inst);
    
    // Set optimization level (0-3)
    void set_optimization_level(int level);
    
    // Enable/disable debug information
    void set_debug_info(bool enabled);
    
private:
    // Target-specific code generation
    std::string generate_cpp(const ir::Module& module);
    std::string generate_cpp_function(const ir::Function& function);
    std::string generate_cpp_instruction(const ir::Instruction& inst);
    
    std::string generate_jvm(const ir::Module& module);
    
    std::string generate_go(const ir::Module& module);
    std::string generate_go_function(const ir::Function& function);
    
    // Helper methods
    std::string cpp_type_name(ir::ValueType type);
    std::string cpp_type_name(const std::string& type_name);  // For Meld type names
    std::string cpp_value_name(const ir::Value& value);
    std::string mangle_name(const std::string& name);
    
    // Transpiler mapping support
    std::string get_native_type_name(const std::string& meld_type_name, CodeGenTarget target);
    std::string map_transpiler_annotation(const std::shared_ptr<meta::MetaType>& type, CodeGenTarget target);
    std::string get_default_type_mapping(const std::string& type_name, CodeGenTarget target);
    std::string cpp_type_name_fallback(const std::string& type_name);
    std::string java_type_name_fallback(const std::string& type_name);
    std::string js_type_name_fallback(const std::string& type_name);
    std::string go_type_name_fallback(const std::string& type_name);
    
    // State
    CodeGenTarget target_;
    int optimization_level_ = 0;
    bool debug_info_ = false;
    std::map<std::string, std::string> value_map_;  // IR value -> generated code variable
};

// Runtime library linker
class RuntimeLinker {
public:
    RuntimeLinker() = default;
    
    // Link runtime library with generated code
    std::string link_runtime(const std::string& generated_code);
    
    // Get runtime library header
    std::string get_runtime_header();
    
    // Get runtime library implementation
    std::string get_runtime_impl();
    
private:
    std::string generate_memory_management();
    std::string generate_type_system();
    std::string generate_builtin_functions();
};

} // namespace meld::compiler
