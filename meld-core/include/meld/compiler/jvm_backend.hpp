#pragma once

#include "meld/compiler/ir.hpp"
#include <memory>
#include <set>
#include <string>
#include <sstream>
#include <map>
#include <vector>

namespace meld::compiler {

// JVM code generation options
struct JVMGenerationOptions {
    std::string package_name = "meld.generated";
    std::string output_directory = "./generated";
    bool generate_interfaces = true;
    bool generate_records = true;
    bool use_modern_java = true;  // Use Java 17+ features
    int java_version = 17;
    bool include_debug_info = false;
};

// Java type mapping information
struct JavaTypeInfo {
    std::string java_type;
    std::string import_statement;
    bool is_primitive;
    bool needs_boxing;
    
    JavaTypeInfo() : is_primitive(false), needs_boxing(false) {}
    JavaTypeInfo(std::string type, std::string import_stmt = "", bool primitive = false, bool boxing = false)
        : java_type(std::move(type)), import_statement(std::move(import_stmt)), 
          is_primitive(primitive), needs_boxing(boxing) {}
};

// JVM backend - transpiles IR to Java source code
class JVMBackend {
public:
    explicit JVMBackend(const JVMGenerationOptions& options = {});
    
    // Generate Java source files for a module
    std::map<std::string, std::string> generate_java_files(const ir::Module& module);
    
    // Generate Java source code for a single class
    std::string generate_java_class(const ir::Module& module, const std::string& class_name);
    
    // Generate Java interface from Meld trait
    std::string generate_java_interface(const ir::Module& module, const std::string& interface_name);
    
    // Generate Java record from Meld struct
    std::string generate_java_record(const ir::Module& module, const std::string& record_name);
    
    // Generate Java method from IR function
    std::string generate_java_method(const ir::Function& function, bool is_static = true);
    
    // Generate Java expression from IR instruction
    std::string generate_java_expression(const ir::Instruction& inst);
    
    // Type mapping utilities
    JavaTypeInfo map_meld_type_to_java(ir::ValueType meld_type, 
                                       std::shared_ptr<meta::MetaType> meta_type = nullptr);
    
    // Set generation options
    void set_options(const JVMGenerationOptions& options);
    const JVMGenerationOptions& get_options() const { return options_; }
    
    // Utility methods
    std::string get_java_package_declaration() const;
    std::string get_java_imports() const;
    std::string sanitize_java_identifier(const std::string& name) const;
    std::string mangle_method_name(const std::string& name) const;

private:
    JVMGenerationOptions options_;
    std::set<std::string> required_imports_;
    std::map<std::string, JavaTypeInfo> type_mapping_;
    
    // Internal generation methods
    std::string generate_class_header(const std::string& class_name);
    std::string generate_method_signature(const ir::Function& function, bool is_static);
    std::string generate_method_body(const ir::Function& function);
    std::string generate_basic_block(const ir::BasicBlock& block);
    std::string generate_instruction(const ir::Instruction& inst);
    
    // Java-specific code generation
    std::string generate_java_constant(const ir::Instruction& inst);
    std::string generate_java_arithmetic(const ir::Instruction& inst);
    std::string generate_java_comparison(const ir::Instruction& inst);
    std::string generate_java_logical(const ir::Instruction& inst);
    std::string generate_java_memory_access(const ir::Instruction& inst);
    std::string generate_java_control_flow(const ir::Instruction& inst);
    std::string generate_java_function_call(const ir::Instruction& inst);
    
    // Helper methods
    void initialize_type_mappings();
    void add_required_import(const std::string& import);
    std::string get_java_operator(ir::Opcode opcode);
    std::string get_java_value_name(const ir::Value& value);
    std::string get_default_value_for_type(const JavaTypeInfo& type_info);
    
    // Meld-specific mappings
    std::string map_meld_result_type(std::shared_ptr<meta::MetaType> meta_type);
    std::string map_meld_option_type(std::shared_ptr<meta::MetaType> meta_type);
    std::string map_meld_collection_type(std::shared_ptr<meta::MetaType> meta_type);
};

// JVM runtime library generator
class JVMRuntimeGenerator {
public:
    explicit JVMRuntimeGenerator(const JVMGenerationOptions& options);
    
    // Generate runtime support classes
    std::map<std::string, std::string> generate_runtime_classes();
    
    // Generate specific runtime components
    std::string generate_result_class();
    std::string generate_option_class();
    std::string generate_meld_runtime_class();
    std::string generate_type_system_classes();
    std::string generate_memory_management_classes();
    
private:
    JVMGenerationOptions options_;
    
    std::string get_package_declaration() const;
    std::string generate_class_template(const std::string& class_name, 
                                      const std::string& class_body,
                                      const std::vector<std::string>& imports = {});
};

} // namespace meld::compiler