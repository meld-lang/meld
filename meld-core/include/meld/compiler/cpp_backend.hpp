#pragma once

#include "meld/compiler/ir.hpp"
#include <memory>
#include <string>
#include <sstream>
#include <map>
#include <vector>
#include <set>

namespace meld::compiler {

// C++ code generation options
struct CppGenerationOptions {
    std::string namespace_name = "meld::generated";
    std::string output_directory = "./generated";
    bool generate_headers = true;
    bool generate_implementations = true;
    bool use_smart_pointers = true;  // Use std::shared_ptr, std::unique_ptr
    std::string cpp_standard = "17"; // C++17 by default
    bool include_debug_info = false;
    bool use_exceptions = false;     // Meld uses Result<T,E> instead
    bool generate_cmake = true;      // Generate CMakeLists.txt
};

// C++ type mapping information
struct CppTypeInfo {
    std::string cpp_type;
    std::string header_include;
    bool is_primitive;
    bool needs_smart_pointer;
    bool is_const;
    
    CppTypeInfo() : is_primitive(false), needs_smart_pointer(false), is_const(false) {}
    CppTypeInfo(std::string type, std::string include = "", bool primitive = false, 
                bool smart_ptr = false, bool const_type = false)
        : cpp_type(std::move(type)), header_include(std::move(include)), 
          is_primitive(primitive), needs_smart_pointer(smart_ptr), is_const(const_type) {}
};

// C++ backend - transpiles IR to C++17 source code
class CppBackend {
public:
    explicit CppBackend(const CppGenerationOptions& options = {});
    
    // Generate C++ source files for a module
    std::map<std::string, std::string> generate_cpp_files(const ir::Module& module);
    
    // Generate C++ header file
    std::string generate_cpp_header(const ir::Module& module, const std::string& class_name);
    
    // Generate C++ implementation file
    std::string generate_cpp_implementation(const ir::Module& module, const std::string& class_name);
    
    // Generate C++ struct from Meld struct
    std::string generate_cpp_struct(const ir::Module& module, const std::string& struct_name);
    
    // Generate C++ class from Meld class
    std::string generate_cpp_class(const ir::Module& module, const std::string& class_name);
    
    // Generate C++ function from IR function
    std::string generate_cpp_function(const ir::Function& function, bool is_declaration = false);
    
    // Generate C++ expression from IR instruction
    std::string generate_cpp_expression(const ir::Instruction& inst);
    
    // Type mapping utilities
    CppTypeInfo map_meld_type_to_cpp(ir::ValueType meld_type, 
                                     std::shared_ptr<meta::MetaType> meta_type = nullptr);
    
    // Set generation options
    void set_options(const CppGenerationOptions& options);
    const CppGenerationOptions& get_options() const { return options_; }
    
    // Utility methods
    std::string get_cpp_namespace_declaration() const;
    std::string get_cpp_includes() const;
    std::string sanitize_cpp_identifier(const std::string& name) const;
    std::string mangle_function_name(const std::string& name) const;
    std::string generate_cmake_file(const ir::Module& module) const;

private:
    CppGenerationOptions options_;
    std::set<std::string> required_includes_;
    std::map<std::string, CppTypeInfo> type_mapping_;
    
    // Internal generation methods
    std::string generate_header_guards(const std::string& filename);
    std::string generate_function_signature(const ir::Function& function, bool is_declaration);
    std::string generate_function_body(const ir::Function& function);
    std::string generate_basic_block(const ir::BasicBlock& block);
    std::string generate_instruction(const ir::Instruction& inst);
    
    // C++-specific code generation
    std::string generate_cpp_constant(const ir::Instruction& inst);
    std::string generate_cpp_arithmetic(const ir::Instruction& inst);
    std::string generate_cpp_comparison(const ir::Instruction& inst);
    std::string generate_cpp_logical(const ir::Instruction& inst);
    std::string generate_cpp_memory_access(const ir::Instruction& inst);
    std::string generate_cpp_control_flow(const ir::Instruction& inst);
    std::string generate_cpp_function_call(const ir::Instruction& inst);
    
    // Helper methods
    void initialize_type_mappings();
    void add_required_include(const std::string& include);
    std::string get_cpp_operator(ir::Opcode opcode);
    std::string get_cpp_value_name(const ir::Value& value);
    std::string get_default_value_for_type(const CppTypeInfo& type_info);
    
    // Meld-specific mappings
    std::string map_meld_result_type(std::shared_ptr<meta::MetaType> meta_type);
    std::string map_meld_option_type(std::shared_ptr<meta::MetaType> meta_type);
    std::string map_meld_collection_type(std::shared_ptr<meta::MetaType> meta_type);
    
    // Smart pointer helpers
    std::string wrap_with_smart_pointer(const std::string& type, bool is_unique = false);
    std::string generate_smart_pointer_creation(const std::string& type, const std::string& args = "");
};

// C++ runtime library generator
class CppRuntimeGenerator {
public:
    explicit CppRuntimeGenerator(const CppGenerationOptions& options);
    
    // Generate runtime support files
    std::map<std::string, std::string> generate_runtime_files();
    
    // Generate specific runtime components
    std::string generate_result_header();
    std::string generate_result_implementation();
    std::string generate_option_header();
    std::string generate_option_implementation();
    std::string generate_meld_runtime_header();
    std::string generate_meld_runtime_implementation();
    std::string generate_type_system_headers();
    std::string generate_memory_management_headers();
    
private:
    CppGenerationOptions options_;
    
    std::string get_namespace_declaration() const;
    std::string generate_header_template(const std::string& filename, 
                                       const std::string& content,
                                       const std::vector<std::string>& includes = {});
    std::string generate_implementation_template(const std::string& header_name,
                                               const std::string& content,
                                               const std::vector<std::string>& includes = {});
};

} // namespace meld::compiler