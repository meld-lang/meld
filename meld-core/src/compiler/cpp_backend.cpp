#include "meld/compiler/cpp_backend.hpp"
#include <sstream>
#include <algorithm>
#include <set>
#include <filesystem>
#include <cctype>

namespace meld::compiler {

CppBackend::CppBackend(const CppGenerationOptions& options)
    : options_(options) {
    initialize_type_mappings();
}

std::map<std::string, std::string> CppBackend::generate_cpp_files(const ir::Module& module) {
    std::map<std::string, std::string> files;
    
    // Generate main module class
    std::string main_class_name = sanitize_cpp_identifier(module.name);
    if (main_class_name.empty()) {
        main_class_name = "MeldModule";
    }
    
    // Generate header file
    if (options_.generate_headers) {
        std::string header_content = generate_cpp_header(module, main_class_name);
        files[main_class_name + ".hpp"] = header_content;
    }
    
    // Generate implementation file
    if (options_.generate_implementations) {
        std::string impl_content = generate_cpp_implementation(module, main_class_name);
        files[main_class_name + ".cpp"] = impl_content;
    }
    
    // Generate runtime support files if needed
    CppRuntimeGenerator runtime_gen(options_);
    auto runtime_files = runtime_gen.generate_runtime_files();
    for (const auto& [filename, content] : runtime_files) {
        files[filename] = content;
    }
    
    // Generate CMakeLists.txt if requested
    if (options_.generate_cmake) {
        files["CMakeLists.txt"] = generate_cmake_file(module);
    }
    
    return files;
}

std::string CppBackend::generate_cpp_header(const ir::Module& module, const std::string& class_name) {
    std::ostringstream oss;
    
    // Header guards
    std::string guard_name = sanitize_cpp_identifier(class_name);
    std::transform(guard_name.begin(), guard_name.end(), guard_name.begin(), ::toupper);
    oss << "#pragma once" << std::endl << std::endl;
    
    // Includes
    std::string includes = get_cpp_includes();
    if (!includes.empty()) {
        oss << includes << std::endl;
    }
    
    // Namespace
    std::string ns_decl = get_cpp_namespace_declaration();
    if (!ns_decl.empty()) {
        oss << ns_decl << " {" << std::endl << std::endl;
    }
    
    // Forward declarations
    oss << "// Forward declarations" << std::endl;
    oss << "template<typename T, typename E> class Result;" << std::endl;
    oss << "template<typename T> class Option;" << std::endl << std::endl;
    
    // Class declaration
    oss << "class " << sanitize_cpp_identifier(class_name) << " {" << std::endl;
    oss << "public:" << std::endl;
    
    // Global variables as static members
    if (!module.globals.empty()) {
        oss << "    // Global variables" << std::endl;
        for (const auto& global : module.globals) {
            CppTypeInfo type_info = map_meld_type_to_cpp(global->type, global->meta_type);
            oss << "    static " << type_info.cpp_type << " " 
                << sanitize_cpp_identifier(global->name) << ";" << std::endl;
        }
        oss << std::endl;
    }
    
    // Function declarations
    oss << "    // Function declarations" << std::endl;
    for (const auto& function : module.functions) {
        oss << "    " << generate_cpp_function(*function, true) << ";" << std::endl;
    }
    
    // Main function if this is the main module
    if (class_name == "MeldModule" || class_name == sanitize_cpp_identifier(module.name)) {
        oss << std::endl << "    // Entry point" << std::endl;
        oss << "    static int main(int argc, char* argv[]);" << std::endl;
    }
    
    oss << "};" << std::endl << std::endl;
    
    // Close namespace
    if (!ns_decl.empty()) {
        oss << "} // namespace " << options_.namespace_name << std::endl;
    }
    
    return oss.str();
}

std::string CppBackend::generate_cpp_implementation(const ir::Module& module, const std::string& class_name) {
    std::ostringstream oss;
    
    // Include corresponding header
    oss << "#include \"" << sanitize_cpp_identifier(class_name) << ".hpp\"" << std::endl;
    
    // Additional includes for implementation
    add_required_include("<iostream>");
    add_required_include("<memory>");
    add_required_include("<stdexcept>");
    
    std::string includes = get_cpp_includes();
    if (!includes.empty()) {
        oss << includes << std::endl;
    }
    
    // Namespace
    std::string ns_decl = get_cpp_namespace_declaration();
    if (!ns_decl.empty()) {
        oss << ns_decl << " {" << std::endl << std::endl;
    }
    
    // Global variable definitions
    if (!module.globals.empty()) {
        oss << "// Global variable definitions" << std::endl;
        for (const auto& global : module.globals) {
            CppTypeInfo type_info = map_meld_type_to_cpp(global->type, global->meta_type);
            oss << type_info.cpp_type << " " << sanitize_cpp_identifier(class_name) 
                << "::" << sanitize_cpp_identifier(global->name);
            
            // Initialize with default value
            std::string default_val = get_default_value_for_type(type_info);
            if (!default_val.empty()) {
                oss << " = " << default_val;
            }
            oss << ";" << std::endl;
        }
        oss << std::endl;
    }
    
    // Function implementations
    for (const auto& function : module.functions) {
        oss << generate_cpp_function(*function, false) << std::endl << std::endl;
    }
    
    // Main function implementation if this is the main module
    if (class_name == "MeldModule" || class_name == sanitize_cpp_identifier(module.name)) {
        oss << "int " << sanitize_cpp_identifier(class_name) << "::main(int argc, char* argv[]) {" << std::endl;
        oss << "    try {" << std::endl;
        
        // Look for a main function
        bool found_main = false;
        for (const auto& function : module.functions) {
            if (function->name == "main") {
                oss << "        " << mangle_function_name(function->name) << "();" << std::endl;
                found_main = true;
                break;
            }
        }
        
        if (!found_main) {
            oss << "        std::cout << \"No main function found\" << std::endl;" << std::endl;
        }
        
        oss << "        return 0;" << std::endl;
        oss << "    } catch (const std::exception& e) {" << std::endl;
        oss << "        std::cerr << \"Error: \" << e.what() << std::endl;" << std::endl;
        oss << "        return 1;" << std::endl;
        oss << "    }" << std::endl;
        oss << "}" << std::endl << std::endl;
    }
    
    // Close namespace
    if (!ns_decl.empty()) {
        oss << "} // namespace " << options_.namespace_name << std::endl;
    }
    
    return oss.str();
}

std::string CppBackend::generate_cpp_struct(const ir::Module& module, const std::string& struct_name) {
    std::ostringstream oss;
    
    oss << "struct " << sanitize_cpp_identifier(struct_name) << " {" << std::endl;
    
    // Struct fields
    for (const auto& global : module.globals) {
        if (global->name.find("field_") == 0) {  // Struct fields
            CppTypeInfo type_info = map_meld_type_to_cpp(global->type, global->meta_type);
            oss << "    " << type_info.cpp_type << " " 
                << sanitize_cpp_identifier(global->name.substr(6)) << ";" << std::endl;
        }
    }
    
    // Default constructor
    oss << std::endl << "    " << sanitize_cpp_identifier(struct_name) << "() = default;" << std::endl;
    
    // Parameterized constructor
    oss << "    " << sanitize_cpp_identifier(struct_name) << "(";
    bool first = true;
    for (const auto& global : module.globals) {
        if (global->name.find("field_") == 0) {
            if (!first) oss << ", ";
            CppTypeInfo type_info = map_meld_type_to_cpp(global->type, global->meta_type);
            std::string field_name = sanitize_cpp_identifier(global->name.substr(6));
            oss << "const " << type_info.cpp_type << "& " << field_name;
            first = false;
        }
    }
    oss << ")" << std::endl << "        : ";
    
    first = true;
    for (const auto& global : module.globals) {
        if (global->name.find("field_") == 0) {
            if (!first) oss << ", ";
            std::string field_name = sanitize_cpp_identifier(global->name.substr(6));
            oss << field_name << "(" << field_name << ")";
            first = false;
        }
    }
    oss << " {}" << std::endl;
    
    // Copy and move constructors (defaulted)
    oss << std::endl << "    " << sanitize_cpp_identifier(struct_name) 
        << "(const " << sanitize_cpp_identifier(struct_name) << "&) = default;" << std::endl;
    oss << "    " << sanitize_cpp_identifier(struct_name) 
        << "(" << sanitize_cpp_identifier(struct_name) << "&&) = default;" << std::endl;
    
    // Assignment operators (defaulted)
    oss << "    " << sanitize_cpp_identifier(struct_name) 
        << "& operator=(const " << sanitize_cpp_identifier(struct_name) << "&) = default;" << std::endl;
    oss << "    " << sanitize_cpp_identifier(struct_name) 
        << "& operator=(" << sanitize_cpp_identifier(struct_name) << "&&) = default;" << std::endl;
    
    oss << "};" << std::endl;
    
    return oss.str();
}

std::string CppBackend::generate_cpp_class(const ir::Module& module, const std::string& class_name) {
    std::ostringstream oss;
    
    oss << "class " << sanitize_cpp_identifier(class_name) << " {" << std::endl;
    oss << "private:" << std::endl;
    
    // Class fields
    for (const auto& global : module.globals) {
        if (global->name.find("field_") == 0) {  // Class fields
            CppTypeInfo type_info = map_meld_type_to_cpp(global->type, global->meta_type);
            oss << "    " << type_info.cpp_type << " " 
                << sanitize_cpp_identifier(global->name.substr(6)) << "_;" << std::endl;
        }
    }
    
    oss << std::endl << "public:" << std::endl;
    
    // Constructor
    oss << "    " << sanitize_cpp_identifier(class_name) << "(";
    bool first = true;
    for (const auto& global : module.globals) {
        if (global->name.find("field_") == 0) {
            if (!first) oss << ", ";
            CppTypeInfo type_info = map_meld_type_to_cpp(global->type, global->meta_type);
            std::string field_name = sanitize_cpp_identifier(global->name.substr(6));
            oss << "const " << type_info.cpp_type << "& " << field_name;
            first = false;
        }
    }
    oss << ")" << std::endl << "        : ";
    
    first = true;
    for (const auto& global : module.globals) {
        if (global->name.find("field_") == 0) {
            if (!first) oss << ", ";
            std::string field_name = sanitize_cpp_identifier(global->name.substr(6));
            oss << field_name << "_(" << field_name << ")";
            first = false;
        }
    }
    oss << " {}" << std::endl;
    
    // Getters
    for (const auto& global : module.globals) {
        if (global->name.find("field_") == 0) {
            CppTypeInfo type_info = map_meld_type_to_cpp(global->type, global->meta_type);
            std::string field_name = sanitize_cpp_identifier(global->name.substr(6));
            oss << std::endl << "    const " << type_info.cpp_type << "& " << field_name << "() const {" << std::endl;
            oss << "        return " << field_name << "_;" << std::endl;
            oss << "    }" << std::endl;
        }
    }
    
    // Virtual destructor for polymorphism
    oss << std::endl << "    virtual ~" << sanitize_cpp_identifier(class_name) << "() = default;" << std::endl;
    
    oss << "};" << std::endl;
    
    return oss.str();
}

std::string CppBackend::generate_cpp_function(const ir::Function& function, bool is_declaration) {
    std::ostringstream oss;
    
    if (!is_declaration) {
        // Full function implementation
        oss << generate_function_signature(function, false) << " {" << std::endl;
        oss << generate_function_body(function);
        oss << "}";
    } else {
        // Just the signature for header
        oss << "static " << generate_function_signature(function, true);
    }
    
    return oss.str();
}

std::string CppBackend::generate_function_signature(const ir::Function& function, bool is_declaration) {
    std::ostringstream oss;
    
    // Return type
    if (function.return_value) {
        CppTypeInfo return_type = map_meld_type_to_cpp(function.return_value->type, function.return_value->meta_type);
        oss << return_type.cpp_type;
    } else {
        oss << "void";
    }
    
    oss << " " << mangle_function_name(function.name) << "(";
    
    // Parameters
    for (size_t i = 0; i < function.parameters.size(); ++i) {
        if (i > 0) oss << ", ";
        const auto& param = function.parameters[i];
        CppTypeInfo param_type = map_meld_type_to_cpp(param->type, param->meta_type);
        
        // Use const reference for non-primitive types
        if (!param_type.is_primitive) {
            oss << "const " << param_type.cpp_type << "& ";
        } else {
            oss << param_type.cpp_type << " ";
        }
        oss << sanitize_cpp_identifier(param->name);
    }
    
    oss << ")";
    
    return oss.str();
}

std::string CppBackend::generate_function_body(const ir::Function& function) {
    std::ostringstream oss;
    
    // Generate code for each basic block
    for (const auto& block : function.basic_blocks) {
        oss << generate_basic_block(*block);
    }
    
    // Ensure function returns if it should
    if (function.return_value && !function.basic_blocks.empty()) {
        const auto& last_block = function.basic_blocks.back();
        if (!last_block->instructions.empty()) {
            const auto& last_inst = last_block->instructions.back();
            if (last_inst->opcode != ir::Opcode::Return) {
                CppTypeInfo return_type = map_meld_type_to_cpp(function.return_value->type, function.return_value->meta_type);
                std::string default_val = get_default_value_for_type(return_type);
                oss << "    return " << default_val << ";" << std::endl;
            }
        }
    }
    
    return oss.str();
}

std::string CppBackend::generate_basic_block(const ir::BasicBlock& block) {
    std::ostringstream oss;
    
    // Generate label (as comment since C++ doesn't have goto in modern style)
    if (block.label != "entry") {
        oss << "    // Block: " << block.label << std::endl;
    }
    
    // Generate instructions
    for (const auto& inst : block.instructions) {
        std::string inst_code = generate_instruction(*inst);
        if (!inst_code.empty()) {
            oss << "    " << inst_code << ";" << std::endl;
        }
    }
    
    return oss.str();
}

std::string CppBackend::generate_instruction(const ir::Instruction& inst) {
    switch (inst.opcode) {
        // Constants
        case ir::Opcode::ConstInt:
        case ir::Opcode::ConstFloat:
        case ir::Opcode::ConstBool:
        case ir::Opcode::ConstString:
        case ir::Opcode::ConstNull:
            return generate_cpp_constant(inst);
            
        // Arithmetic
        case ir::Opcode::Add:
        case ir::Opcode::Sub:
        case ir::Opcode::Mul:
        case ir::Opcode::Div:
        case ir::Opcode::Mod:
        case ir::Opcode::Neg:
            return generate_cpp_arithmetic(inst);
            
        // Comparison
        case ir::Opcode::Eq:
        case ir::Opcode::Ne:
        case ir::Opcode::Lt:
        case ir::Opcode::Le:
        case ir::Opcode::Gt:
        case ir::Opcode::Ge:
            return generate_cpp_comparison(inst);
            
        // Logical
        case ir::Opcode::And:
        case ir::Opcode::Or:
        case ir::Opcode::Not:
            return generate_cpp_logical(inst);
            
        // Memory
        case ir::Opcode::Alloca:
        case ir::Opcode::Load:
        case ir::Opcode::Store:
        case ir::Opcode::GetField:
        case ir::Opcode::SetField:
            return generate_cpp_memory_access(inst);
            
        // Control flow
        case ir::Opcode::Branch:
        case ir::Opcode::CondBranch:
        case ir::Opcode::Return:
            return generate_cpp_control_flow(inst);
            
        // Function calls
        case ir::Opcode::Call:
            return generate_cpp_function_call(inst);
            
        default:
            return "/* Unsupported opcode: " + std::to_string(static_cast<int>(inst.opcode)) + " */";
    }
}

std::string CppBackend::generate_cpp_constant(const ir::Instruction& inst) {
    std::ostringstream oss;
    
    if (inst.result) {
        CppTypeInfo type_info = map_meld_type_to_cpp(inst.result->type, inst.result->meta_type);
        oss << type_info.cpp_type << " " << get_cpp_value_name(*inst.result) << " = ";
        
        switch (inst.opcode) {
            case ir::Opcode::ConstInt:
                oss << std::get<int64_t>(inst.constant_value) << "LL";
                break;
            case ir::Opcode::ConstFloat:
                oss << std::get<double>(inst.constant_value);
                break;
            case ir::Opcode::ConstBool:
                oss << (std::get<bool>(inst.constant_value) ? "true" : "false");
                break;
            case ir::Opcode::ConstString:
                oss << "std::string(\"" << std::get<std::string>(inst.constant_value) << "\")";
                break;
            case ir::Opcode::ConstNull:
                oss << "nullptr";
                break;
            default:
                oss << "nullptr";
                break;
        }
    }
    
    return oss.str();
}

std::string CppBackend::generate_cpp_arithmetic(const ir::Instruction& inst) {
    std::ostringstream oss;
    
    if (inst.result) {
        CppTypeInfo type_info = map_meld_type_to_cpp(inst.result->type, inst.result->meta_type);
        oss << type_info.cpp_type << " " << get_cpp_value_name(*inst.result) << " = ";
        
        if (inst.opcode == ir::Opcode::Neg) {
            // Unary operation
            oss << "-" << get_cpp_value_name(*inst.operands[0]);
        } else {
            // Binary operation
            oss << get_cpp_value_name(*inst.operands[0]);
            oss << " " << get_cpp_operator(inst.opcode) << " ";
            oss << get_cpp_value_name(*inst.operands[1]);
        }
    }
    
    return oss.str();
}

std::string CppBackend::generate_cpp_comparison(const ir::Instruction& inst) {
    std::ostringstream oss;
    
    if (inst.result) {
        oss << "bool " << get_cpp_value_name(*inst.result) << " = ";
        oss << get_cpp_value_name(*inst.operands[0]);
        oss << " " << get_cpp_operator(inst.opcode) << " ";
        oss << get_cpp_value_name(*inst.operands[1]);
    }
    
    return oss.str();
}

std::string CppBackend::generate_cpp_logical(const ir::Instruction& inst) {
    std::ostringstream oss;
    
    if (inst.result) {
        oss << "bool " << get_cpp_value_name(*inst.result) << " = ";
        
        if (inst.opcode == ir::Opcode::Not) {
            oss << "!" << get_cpp_value_name(*inst.operands[0]);
        } else {
            oss << get_cpp_value_name(*inst.operands[0]);
            oss << " " << get_cpp_operator(inst.opcode) << " ";
            oss << get_cpp_value_name(*inst.operands[1]);
        }
    }
    
    return oss.str();
}

std::string CppBackend::generate_cpp_memory_access(const ir::Instruction& inst) {
    std::ostringstream oss;
    
    switch (inst.opcode) {
        case ir::Opcode::Alloca:
            if (inst.result) {
                CppTypeInfo type_info = map_meld_type_to_cpp(inst.result->type, inst.result->meta_type);
                oss << type_info.cpp_type << " " << get_cpp_value_name(*inst.result);
                std::string default_val = get_default_value_for_type(type_info);
                if (!default_val.empty()) {
                    oss << " = " << default_val;
                }
            }
            break;
            
        case ir::Opcode::Load:
            if (inst.result && !inst.operands.empty()) {
                CppTypeInfo type_info = map_meld_type_to_cpp(inst.result->type, inst.result->meta_type);
                oss << type_info.cpp_type << " " << get_cpp_value_name(*inst.result);
                oss << " = " << get_cpp_value_name(*inst.operands[0]);
            }
            break;
            
        case ir::Opcode::Store:
            if (inst.operands.size() >= 2) {
                oss << get_cpp_value_name(*inst.operands[1]);
                oss << " = " << get_cpp_value_name(*inst.operands[0]);
            }
            break;
            
        case ir::Opcode::GetField:
            if (inst.result && !inst.operands.empty()) {
                CppTypeInfo type_info = map_meld_type_to_cpp(inst.result->type, inst.result->meta_type);
                oss << type_info.cpp_type << " " << get_cpp_value_name(*inst.result);
                oss << " = " << get_cpp_value_name(*inst.operands[0]) << ".field";
            }
            break;
            
        case ir::Opcode::SetField:
            if (inst.operands.size() >= 2) {
                oss << get_cpp_value_name(*inst.operands[0]) << ".field";
                oss << " = " << get_cpp_value_name(*inst.operands[1]);
            }
            break;
            
        default:
            break;
    }
    
    return oss.str();
}

std::string CppBackend::generate_cpp_control_flow(const ir::Instruction& inst) {
    std::ostringstream oss;
    
    switch (inst.opcode) {
        case ir::Opcode::Return:
            oss << "return";
            if (!inst.operands.empty()) {
                oss << " " << get_cpp_value_name(*inst.operands[0]);
            }
            break;
            
        case ir::Opcode::Branch:
            oss << "// goto " << inst.target_label;
            break;
            
        case ir::Opcode::CondBranch:
            if (!inst.operands.empty()) {
                oss << "if (" << get_cpp_value_name(*inst.operands[0]) << ") {";
                oss << " /* goto " << inst.target_label << " */ }";
                oss << " else { /* goto " << inst.else_label << " */ }";
            }
            break;
            
        default:
            break;
    }
    
    return oss.str();
}

std::string CppBackend::generate_cpp_function_call(const ir::Instruction& inst) {
    std::ostringstream oss;
    
    if (!inst.operands.empty()) {
        if (inst.result) {
            CppTypeInfo type_info = map_meld_type_to_cpp(inst.result->type, inst.result->meta_type);
            oss << type_info.cpp_type << " " << get_cpp_value_name(*inst.result) << " = ";
        }
        
        oss << get_cpp_value_name(*inst.operands[0]) << "(";
        for (size_t i = 1; i < inst.operands.size(); ++i) {
            if (i > 1) oss << ", ";
            oss << get_cpp_value_name(*inst.operands[i]);
        }
        oss << ")";
    }
    
    return oss.str();
}

// Helper methods implementation
void CppBackend::initialize_type_mappings() {
    type_mapping_["int"] = CppTypeInfo("std::int64_t", "<cstdint>", true, false);
    type_mapping_["float"] = CppTypeInfo("double", "", true, false);
    type_mapping_["bool"] = CppTypeInfo("bool", "", true, false);
    type_mapping_["string"] = CppTypeInfo("std::string", "<string>", false, false);
    type_mapping_["void"] = CppTypeInfo("void", "", true, false);
    
    // Add common includes
    required_includes_.insert("<memory>");
    required_includes_.insert("<string>");
    required_includes_.insert("<cstdint>");
}

CppTypeInfo CppBackend::map_meld_type_to_cpp(ir::ValueType meld_type, std::shared_ptr<meta::MetaType> meta_type) {
    switch (meld_type) {
        case ir::ValueType::Int:
            add_required_include("<cstdint>");
            return CppTypeInfo("std::int64_t", "<cstdint>", true, false);
        case ir::ValueType::Float:
            return CppTypeInfo("double", "", true, false);
        case ir::ValueType::Bool:
            return CppTypeInfo("bool", "", true, false);
        case ir::ValueType::String:
            add_required_include("<string>");
            return CppTypeInfo("std::string", "<string>", false, false);
        case ir::ValueType::Void:
            return CppTypeInfo("void", "", true, false);
        case ir::ValueType::Pointer:
            if (options_.use_smart_pointers) {
                add_required_include("<memory>");
                return CppTypeInfo("std::shared_ptr<void>", "<memory>", false, true);
            } else {
                return CppTypeInfo("void*", "", false, false);
            }
        case ir::ValueType::Struct:
            return CppTypeInfo("struct", "", false, false);
        case ir::ValueType::Function:
            add_required_include("<functional>");
            return CppTypeInfo("std::function<void()>", "<functional>", false, false);
        default:
            return CppTypeInfo("void*", "", false, false);
    }
}

std::string CppBackend::get_cpp_namespace_declaration() const {
    if (options_.namespace_name.empty()) {
        return "";
    }
    return "namespace " + options_.namespace_name;
}

std::string CppBackend::get_cpp_includes() const {
    std::ostringstream oss;
    for (const auto& include : required_includes_) {
        oss << "#include " << include << std::endl;
    }
    return oss.str();
}

std::string CppBackend::sanitize_cpp_identifier(const std::string& name) const {
    std::string result = name;
    
    // Replace invalid characters
    std::replace(result.begin(), result.end(), '-', '_');
    std::replace(result.begin(), result.end(), '.', '_');
    std::replace(result.begin(), result.end(), '%', '_');
    
    // Ensure it starts with a letter or underscore
    if (!result.empty() && !std::isalpha(result[0]) && result[0] != '_') {
        result = "_" + result;
    }
    
    // Check for C++ keywords and prefix if necessary
    static const std::set<std::string> cpp_keywords = {
        "alignas", "alignof", "and", "and_eq", "asm", "atomic_cancel", "atomic_commit",
        "atomic_noexcept", "auto", "bitand", "bitor", "bool", "break", "case", "catch",
        "char", "char8_t", "char16_t", "char32_t", "class", "compl", "concept", "const",
        "consteval", "constexpr", "constinit", "const_cast", "continue", "co_await",
        "co_return", "co_yield", "decltype", "default", "delete", "do", "double",
        "dynamic_cast", "else", "enum", "explicit", "export", "extern", "false",
        "float", "for", "friend", "goto", "if", "inline", "int", "long", "mutable",
        "namespace", "new", "noexcept", "not", "not_eq", "nullptr", "operator", "or",
        "or_eq", "private", "protected", "public", "reflexpr", "register",
        "reinterpret_cast", "requires", "return", "short", "signed", "sizeof", "static",
        "static_assert", "static_cast", "struct", "switch", "synchronized", "template",
        "this", "thread_local", "throw", "true", "try", "typedef", "typeid", "typename",
        "union", "unsigned", "using", "virtual", "void", "volatile", "wchar_t", "while",
        "xor", "xor_eq"
    };
    
    if (cpp_keywords.count(result)) {
        result = "meld_" + result;
    }
    
    return result;
}

std::string CppBackend::mangle_function_name(const std::string& name) const {
    return sanitize_cpp_identifier(name);
}

std::string CppBackend::get_cpp_operator(ir::Opcode opcode) {
    switch (opcode) {
        case ir::Opcode::Add: return "+";
        case ir::Opcode::Sub: return "-";
        case ir::Opcode::Mul: return "*";
        case ir::Opcode::Div: return "/";
        case ir::Opcode::Mod: return "%";
        case ir::Opcode::And: return "&&";
        case ir::Opcode::Or: return "||";
        case ir::Opcode::Eq: return "==";
        case ir::Opcode::Ne: return "!=";
        case ir::Opcode::Lt: return "<";
        case ir::Opcode::Le: return "<=";
        case ir::Opcode::Gt: return ">";
        case ir::Opcode::Ge: return ">=";
        default: return "?";
    }
}

std::string CppBackend::get_cpp_value_name(const ir::Value& value) {
    return sanitize_cpp_identifier(value.name);
}

std::string CppBackend::get_default_value_for_type(const CppTypeInfo& type_info) {
    if (type_info.is_primitive) {
        if (type_info.cpp_type == "bool") return "false";
        if (type_info.cpp_type == "std::int64_t") return "0LL";
        if (type_info.cpp_type == "double") return "0.0";
        if (type_info.cpp_type == "int") return "0";
        if (type_info.cpp_type == "float") return "0.0f";
        if (type_info.cpp_type == "char") return "'\\0'";
    }
    if (type_info.cpp_type == "std::string") return "std::string()";
    if (type_info.needs_smart_pointer) return "nullptr";
    return "{}";
}

std::string CppBackend::generate_cmake_file(const ir::Module& module) const {
    std::ostringstream oss;
    
    oss << "cmake_minimum_required(VERSION 3.17)" << std::endl;
    oss << "project(" << sanitize_cpp_identifier(module.name) << " VERSION 1.0.0)" << std::endl << std::endl;
    
    oss << "# Set C++ standard" << std::endl;
    oss << "set(CMAKE_CXX_STANDARD " << options_.cpp_standard << ")" << std::endl;
    oss << "set(CMAKE_CXX_STANDARD_REQUIRED ON)" << std::endl;
    oss << "set(CMAKE_CXX_EXTENSIONS OFF)" << std::endl << std::endl;
    
    oss << "# Compiler flags" << std::endl;
    oss << "set(CMAKE_CXX_FLAGS_DEBUG \"-g -O0 -Wall -Wextra\")" << std::endl;
    oss << "set(CMAKE_CXX_FLAGS_RELEASE \"-O3 -DNDEBUG\")" << std::endl << std::endl;
    
    oss << "# Find required packages" << std::endl;
    oss << "# Add any required packages here" << std::endl << std::endl;
    
    oss << "# Source files" << std::endl;
    oss << "set(SOURCES" << std::endl;
    oss << "    " << sanitize_cpp_identifier(module.name) << ".cpp" << std::endl;
    oss << "    MeldResult.cpp" << std::endl;
    oss << "    MeldOption.cpp" << std::endl;
    oss << "    MeldRuntime.cpp" << std::endl;
    oss << ")" << std::endl << std::endl;
    
    oss << "# Header files" << std::endl;
    oss << "set(HEADERS" << std::endl;
    oss << "    " << sanitize_cpp_identifier(module.name) << ".hpp" << std::endl;
    oss << "    MeldResult.hpp" << std::endl;
    oss << "    MeldOption.hpp" << std::endl;
    oss << "    MeldRuntime.hpp" << std::endl;
    oss << ")" << std::endl << std::endl;
    
    oss << "# Create executable" << std::endl;
    oss << "add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})" << std::endl << std::endl;
    
    oss << "# Include directories" << std::endl;
    oss << "target_include_directories(${PROJECT_NAME} PRIVATE .)" << std::endl << std::endl;
    
    oss << "# Link libraries" << std::endl;
    oss << "# target_link_libraries(${PROJECT_NAME} PRIVATE library_name)" << std::endl;
    
    return oss.str();
}

void CppBackend::add_required_include(const std::string& include) {
    required_includes_.insert(include);
}

void CppBackend::set_options(const CppGenerationOptions& options) {
    options_ = options;
}

std::string CppBackend::wrap_with_smart_pointer(const std::string& type, bool is_unique) {
    if (is_unique) {
        return "std::unique_ptr<" + type + ">";
    } else {
        return "std::shared_ptr<" + type + ">";
    }
}

std::string CppBackend::generate_smart_pointer_creation(const std::string& type, const std::string& args) {
    if (args.empty()) {
        return "std::make_shared<" + type + ">()";
    } else {
        return "std::make_shared<" + type + ">(" + args + ")";
    }
}

// CppRuntimeGenerator implementation
CppRuntimeGenerator::CppRuntimeGenerator(const CppGenerationOptions& options)
    : options_(options) {
}

std::map<std::string, std::string> CppRuntimeGenerator::generate_runtime_files() {
    std::map<std::string, std::string> files;
    
    files["MeldResult.hpp"] = generate_result_header();
    files["MeldResult.cpp"] = generate_result_implementation();
    files["MeldOption.hpp"] = generate_option_header();
    files["MeldOption.cpp"] = generate_option_implementation();
    files["MeldRuntime.hpp"] = generate_meld_runtime_header();
    files["MeldRuntime.cpp"] = generate_meld_runtime_implementation();
    
    return files;
}

std::string CppRuntimeGenerator::generate_result_header() {
    std::vector<std::string> includes = {"<functional>", "<stdexcept>", "<type_traits>"};
    
    std::string content = R"(
template<typename T, typename E>
class Result {
private:
    union {
        T value_;
        E error_;
    };
    bool is_success_;
    
public:
    // Constructors
    static Result<T, E> success(const T& value) {
        return Result(value, true);
    }
    
    static Result<T, E> success(T&& value) {
        return Result(std::move(value), true);
    }
    
    static Result<T, E> error(const E& error) {
        return Result(error, false);
    }
    
    static Result<T, E> error(E&& error) {
        return Result(std::move(error), false);
    }
    
    // Copy constructor
    Result(const Result& other) : is_success_(other.is_success_) {
        if (is_success_) {
            new (&value_) T(other.value_);
        } else {
            new (&error_) E(other.error_);
        }
    }
    
    // Move constructor
    Result(Result&& other) noexcept : is_success_(other.is_success_) {
        if (is_success_) {
            new (&value_) T(std::move(other.value_));
        } else {
            new (&error_) E(std::move(other.error_));
        }
    }
    
    // Assignment operators
    Result& operator=(const Result& other) {
        if (this != &other) {
            this->~Result();
            new (this) Result(other);
        }
        return *this;
    }
    
    Result& operator=(Result&& other) noexcept {
        if (this != &other) {
            this->~Result();
            new (this) Result(std::move(other));
        }
        return *this;
    }
    
    // Destructor
    ~Result() {
        if (is_success_) {
            value_.~T();
        } else {
            error_.~E();
        }
    }
    
    // Query methods
    bool is_success() const { return is_success_; }
    bool is_error() const { return !is_success_; }
    
    // Value access
    const T& value() const {
        if (!is_success_) {
            throw std::runtime_error("Cannot get value from error result");
        }
        return value_;
    }
    
    T& value() {
        if (!is_success_) {
            throw std::runtime_error("Cannot get value from error result");
        }
        return value_;
    }
    
    const E& error() const {
        if (is_success_) {
            throw std::runtime_error("Cannot get error from success result");
        }
        return error_;
    }
    
    E& error() {
        if (is_success_) {
            throw std::runtime_error("Cannot get error from success result");
        }
        return error_;
    }
    
    // Monadic operations
    template<typename F>
    auto map(F&& f) -> Result<decltype(f(std::declval<T>())), E> {
        using U = decltype(f(std::declval<T>()));
        if (is_success_) {
            return Result<U, E>::success(f(value_));
        } else {
            return Result<U, E>::error(error_);
        }
    }
    
    template<typename F>
    auto flat_map(F&& f) -> decltype(f(std::declval<T>())) {
        if (is_success_) {
            return f(value_);
        } else {
            using ResultType = decltype(f(std::declval<T>()));
            return ResultType::error(error_);
        }
    }

private:
    Result(const T& value, bool) : value_(value), is_success_(true) {}
    Result(T&& value, bool) : value_(std::move(value)), is_success_(true) {}
    Result(const E& error, bool) : error_(error), is_success_(false) {}
    Result(E&& error, bool) : error_(std::move(error)), is_success_(false) {}
};
)";
    
    return generate_header_template("MeldResult.hpp", content, includes);
}

std::string CppRuntimeGenerator::generate_result_implementation() {
    std::string content = R"(
// Result implementation is header-only (template class)
// All methods are defined in the header file
)";
    
    return generate_implementation_template("MeldResult.hpp", content);
}

std::string CppRuntimeGenerator::generate_option_header() {
    std::vector<std::string> includes = {"<functional>", "<stdexcept>", "<type_traits>"};
    
    std::string content = R"(
template<typename T>
class Option {
private:
    union {
        T value_;
    };
    bool has_value_;
    
public:
    // Constructors
    static Option<T> some(const T& value) {
        return Option(value);
    }
    
    static Option<T> some(T&& value) {
        return Option(std::move(value));
    }
    
    static Option<T> none() {
        return Option();
    }
    
    // Default constructor (none)
    Option() : has_value_(false) {}
    
    // Copy constructor
    Option(const Option& other) : has_value_(other.has_value_) {
        if (has_value_) {
            new (&value_) T(other.value_);
        }
    }
    
    // Move constructor
    Option(Option&& other) noexcept : has_value_(other.has_value_) {
        if (has_value_) {
            new (&value_) T(std::move(other.value_));
        }
    }
    
    // Assignment operators
    Option& operator=(const Option& other) {
        if (this != &other) {
            this->~Option();
            new (this) Option(other);
        }
        return *this;
    }
    
    Option& operator=(Option&& other) noexcept {
        if (this != &other) {
            this->~Option();
            new (this) Option(std::move(other));
        }
        return *this;
    }
    
    // Destructor
    ~Option() {
        if (has_value_) {
            value_.~T();
        }
    }
    
    // Query methods
    bool is_some() const { return has_value_; }
    bool is_none() const { return !has_value_; }
    
    // Value access
    const T& value() const {
        if (!has_value_) {
            throw std::runtime_error("Cannot get value from None");
        }
        return value_;
    }
    
    T& value() {
        if (!has_value_) {
            throw std::runtime_error("Cannot get value from None");
        }
        return value_;
    }
    
    T value_or(const T& default_value) const {
        return has_value_ ? value_ : default_value;
    }
    
    // Monadic operations
    template<typename F>
    auto map(F&& f) -> Option<decltype(f(std::declval<T>()))> {
        using U = decltype(f(std::declval<T>()));
        if (has_value_) {
            return Option<U>::some(f(value_));
        } else {
            return Option<U>::none();
        }
    }

private:
    explicit Option(const T& value) : value_(value), has_value_(true) {}
    explicit Option(T&& value) : value_(std::move(value)), has_value_(true) {}
};
)";
    
    return generate_header_template("MeldOption.hpp", content, includes);
}

std::string CppRuntimeGenerator::generate_option_implementation() {
    std::string content = R"(
// Option implementation is header-only (template class)
// All methods are defined in the header file
)";
    
    return generate_implementation_template("MeldOption.hpp", content);
}

std::string CppRuntimeGenerator::generate_meld_runtime_header() {
    std::vector<std::string> includes = {"<iostream>", "<string>", "<memory>", "<typeinfo>"};
    
    std::string content = R"(
class MeldRuntime {
public:
    // I/O operations
    static void println(const std::string& message);
    static void print(const std::string& message);
    static std::string read_line();
    
    // Type system support
    template<typename T>
    static std::string type_of(const T& obj) {
        return typeid(T).name();
    }
    
    // Memory management helpers
    template<typename T>
    static std::shared_ptr<T[]> allocate_array(size_t size) {
        return std::shared_ptr<T[]>(new T[size]);
    }
    
    template<typename T, typename... Args>
    static std::shared_ptr<T> make_shared(Args&&... args) {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }
    
    template<typename T, typename... Args>
    static std::unique_ptr<T> make_unique(Args&&... args) {
        return std::make_unique<T>(std::forward<Args>(args)...);
    }
};
)";
    
    return generate_header_template("MeldRuntime.hpp", content, includes);
}

std::string CppRuntimeGenerator::generate_meld_runtime_implementation() {
    std::vector<std::string> includes = {"<iostream>", "<string>"};
    
    std::string content = R"(
void MeldRuntime::println(const std::string& message) {
    std::cout << message << std::endl;
}

void MeldRuntime::print(const std::string& message) {
    std::cout << message;
}

std::string MeldRuntime::read_line() {
    std::string line;
    std::getline(std::cin, line);
    return line;
}
)";
    
    return generate_implementation_template("MeldRuntime.hpp", content, includes);
}

std::string CppRuntimeGenerator::get_namespace_declaration() const {
    if (options_.namespace_name.empty()) {
        return "";
    }
    return "namespace " + options_.namespace_name;
}

std::string CppRuntimeGenerator::generate_header_template(const std::string& filename, 
                                                        const std::string& content,
                                                        const std::vector<std::string>& includes) {
    std::ostringstream oss;
    
    oss << "#pragma once" << std::endl << std::endl;
    
    for (const auto& include : includes) {
        oss << "#include " << include << std::endl;
    }
    if (!includes.empty()) {
        oss << std::endl;
    }
    
    std::string ns_decl = get_namespace_declaration();
    if (!ns_decl.empty()) {
        oss << ns_decl << " {" << std::endl;
    }
    
    oss << content << std::endl;
    
    if (!ns_decl.empty()) {
        oss << "} // namespace " << options_.namespace_name << std::endl;
    }
    
    return oss.str();
}

std::string CppRuntimeGenerator::generate_implementation_template(const std::string& header_name,
                                                                const std::string& content,
                                                                const std::vector<std::string>& includes) {
    std::ostringstream oss;
    
    oss << "#include \"" << header_name << "\"" << std::endl;
    
    for (const auto& include : includes) {
        oss << "#include " << include << std::endl;
    }
    if (!includes.empty()) {
        oss << std::endl;
    }
    
    std::string ns_decl = get_namespace_declaration();
    if (!ns_decl.empty()) {
        oss << ns_decl << " {" << std::endl;
    }
    
    oss << content << std::endl;
    
    if (!ns_decl.empty()) {
        oss << "} // namespace " << options_.namespace_name << std::endl;
    }
    
    return oss.str();
}

std::string CppBackend::generate_cpp_expression(const ir::Instruction& inst) {
    // TODO: implement full expression generation
    return "/* expression */";
}

} // namespace meld::compiler