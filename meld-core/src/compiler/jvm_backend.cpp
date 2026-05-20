#include "meld/compiler/jvm_backend.hpp"
#include <sstream>
#include <algorithm>
#include <set>
#include <filesystem>

namespace meld::compiler {

JVMBackend::JVMBackend(const JVMGenerationOptions& options)
    : options_(options) {
    initialize_type_mappings();
}

std::map<std::string, std::string> JVMBackend::generate_java_files(const ir::Module& module) {
    std::map<std::string, std::string> files;
    
    // Generate main module class
    std::string main_class_name = sanitize_java_identifier(module.name);
    if (main_class_name.empty()) {
        main_class_name = "MeldModule";
    }
    
    std::string main_class_content = generate_java_class(module, main_class_name);
    files[main_class_name + ".java"] = main_class_content;
    
    // Generate runtime support classes if needed
    JVMRuntimeGenerator runtime_gen(options_);
    auto runtime_classes = runtime_gen.generate_runtime_classes();
    for (const auto& [class_name, content] : runtime_classes) {
        files[class_name + ".java"] = content;
    }
    
    return files;
}

std::string JVMBackend::generate_java_class(const ir::Module& module, const std::string& class_name) {
    std::ostringstream oss;
    
    // Package declaration
    oss << get_java_package_declaration() << std::endl << std::endl;
    
    // Imports
    std::string imports = get_java_imports();
    if (!imports.empty()) {
        oss << imports << std::endl;
    }
    
    // Class header
    oss << generate_class_header(class_name) << std::endl;
    
    // Global variables as static fields
    if (!module.globals.empty()) {
        oss << "    // Global variables" << std::endl;
        for (const auto& global : module.globals) {
            JavaTypeInfo type_info = map_meld_type_to_java(global->type, global->meta_type);
            oss << "    private static " << type_info.java_type << " " 
                << sanitize_java_identifier(global->name);
            
            // Initialize with default value
            std::string default_val = get_default_value_for_type(type_info);
            if (!default_val.empty()) {
                oss << " = " << default_val;
            }
            oss << ";" << std::endl;
        }
        oss << std::endl;
    }
    
    // Generate methods from functions
    for (const auto& function : module.functions) {
        oss << generate_java_method(*function, true) << std::endl;
    }
    
    // Main method if this is the main module
    if (class_name == "MeldModule" || class_name == sanitize_java_identifier(module.name)) {
        oss << "    public static void main(String[] args) {" << std::endl;
        oss << "        // Entry point - call main function if it exists" << std::endl;
        
        // Look for a main function
        for (const auto& function : module.functions) {
            if (function->name == "main") {
                oss << "        " << mangle_method_name(function->name) << "();" << std::endl;
                break;
            }
        }
        
        oss << "    }" << std::endl << std::endl;
    }
    
    oss << "}" << std::endl;
    
    return oss.str();
}

std::string JVMBackend::generate_java_interface(const ir::Module& module, const std::string& interface_name) {
    std::ostringstream oss;
    
    oss << get_java_package_declaration() << std::endl << std::endl;
    oss << get_java_imports() << std::endl;
    
    oss << "public interface " << sanitize_java_identifier(interface_name) << " {" << std::endl;
    
    // Generate abstract method signatures
    for (const auto& function : module.functions) {
        if (function->name.find("trait_") == 0) {  // Trait methods
            std::string signature = generate_method_signature(*function, false);
            oss << "    " << signature << ";" << std::endl;
        }
    }
    
    oss << "}" << std::endl;
    
    return oss.str();
}

std::string JVMBackend::generate_java_record(const ir::Module& module, const std::string& record_name) {
    std::ostringstream oss;
    
    oss << get_java_package_declaration() << std::endl << std::endl;
    oss << get_java_imports() << std::endl;
    
    // Java records require Java 14+
    if (options_.java_version >= 14 && options_.use_modern_java) {
        oss << "public record " << sanitize_java_identifier(record_name) << "(";
        
        // Record components from struct fields
        bool first = true;
        for (const auto& global : module.globals) {
            if (global->name.find("field_") == 0) {  // Struct fields
                if (!first) oss << ", ";
                JavaTypeInfo type_info = map_meld_type_to_java(global->type, global->meta_type);
                oss << type_info.java_type << " " << sanitize_java_identifier(global->name.substr(6));
                first = false;
            }
        }
        
        oss << ") {" << std::endl;
        oss << "    // Record methods can be added here" << std::endl;
        oss << "}" << std::endl;
    } else {
        // Generate traditional class for older Java versions
        oss << "public final class " << sanitize_java_identifier(record_name) << " {" << std::endl;
        
        // Fields
        for (const auto& global : module.globals) {
            if (global->name.find("field_") == 0) {
                JavaTypeInfo type_info = map_meld_type_to_java(global->type, global->meta_type);
                oss << "    private final " << type_info.java_type << " " 
                    << sanitize_java_identifier(global->name.substr(6)) << ";" << std::endl;
            }
        }
        
        // Constructor
        oss << std::endl << "    public " << sanitize_java_identifier(record_name) << "(";
        bool first = true;
        for (const auto& global : module.globals) {
            if (global->name.find("field_") == 0) {
                if (!first) oss << ", ";
                JavaTypeInfo type_info = map_meld_type_to_java(global->type, global->meta_type);
                oss << type_info.java_type << " " << sanitize_java_identifier(global->name.substr(6));
                first = false;
            }
        }
        oss << ") {" << std::endl;
        
        for (const auto& global : module.globals) {
            if (global->name.find("field_") == 0) {
                std::string field_name = sanitize_java_identifier(global->name.substr(6));
                oss << "        this." << field_name << " = " << field_name << ";" << std::endl;
            }
        }
        oss << "    }" << std::endl;
        
        // Getters
        for (const auto& global : module.globals) {
            if (global->name.find("field_") == 0) {
                JavaTypeInfo type_info = map_meld_type_to_java(global->type, global->meta_type);
                std::string field_name = sanitize_java_identifier(global->name.substr(6));
                oss << std::endl << "    public " << type_info.java_type << " " << field_name << "() {" << std::endl;
                oss << "        return " << field_name << ";" << std::endl;
                oss << "    }" << std::endl;
            }
        }
        
        oss << "}" << std::endl;
    }
    
    return oss.str();
}

std::string JVMBackend::generate_java_method(const ir::Function& function, bool is_static) {
    std::ostringstream oss;
    
    // Method signature
    oss << "    " << generate_method_signature(function, is_static) << " {" << std::endl;
    
    // Method body
    oss << generate_method_body(function);
    
    oss << "    }" << std::endl;
    
    return oss.str();
}

std::string JVMBackend::generate_method_signature(const ir::Function& function, bool is_static) {
    std::ostringstream oss;
    
    oss << "public ";
    if (is_static) {
        oss << "static ";
    }
    
    // Return type
    if (function.return_value) {
        JavaTypeInfo return_type = map_meld_type_to_java(function.return_value->type, function.return_value->meta_type);
        oss << return_type.java_type;
    } else {
        oss << "void";
    }
    
    oss << " " << mangle_method_name(function.name) << "(";
    
    // Parameters
    for (size_t i = 0; i < function.parameters.size(); ++i) {
        if (i > 0) oss << ", ";
        const auto& param = function.parameters[i];
        JavaTypeInfo param_type = map_meld_type_to_java(param->type, param->meta_type);
        oss << param_type.java_type << " " << sanitize_java_identifier(param->name);
    }
    
    oss << ")";
    
    return oss.str();
}

std::string JVMBackend::generate_method_body(const ir::Function& function) {
    std::ostringstream oss;
    
    // Generate code for each basic block
    for (const auto& block : function.basic_blocks) {
        oss << generate_basic_block(*block);
    }
    
    // Ensure method returns if it should
    if (function.return_value && !function.basic_blocks.empty()) {
        const auto& last_block = function.basic_blocks.back();
        if (!last_block->instructions.empty()) {
            const auto& last_inst = last_block->instructions.back();
            if (last_inst->opcode != ir::Opcode::Return) {
                JavaTypeInfo return_type = map_meld_type_to_java(function.return_value->type, function.return_value->meta_type);
                std::string default_val = get_default_value_for_type(return_type);
                oss << "        return " << default_val << ";" << std::endl;
            }
        }
    }
    
    return oss.str();
}

std::string JVMBackend::generate_basic_block(const ir::BasicBlock& block) {
    std::ostringstream oss;
    
    // Generate label (as comment since Java doesn't have goto)
    if (block.label != "entry") {
        oss << "        // Block: " << block.label << std::endl;
    }
    
    // Generate instructions
    for (const auto& inst : block.instructions) {
        std::string inst_code = generate_instruction(*inst);
        if (!inst_code.empty()) {
            oss << "        " << inst_code << ";" << std::endl;
        }
    }
    
    return oss.str();
}

std::string JVMBackend::generate_instruction(const ir::Instruction& inst) {
    switch (inst.opcode) {
        // Constants
        case ir::Opcode::ConstInt:
        case ir::Opcode::ConstFloat:
        case ir::Opcode::ConstBool:
        case ir::Opcode::ConstString:
        case ir::Opcode::ConstNull:
            return generate_java_constant(inst);
            
        // Arithmetic
        case ir::Opcode::Add:
        case ir::Opcode::Sub:
        case ir::Opcode::Mul:
        case ir::Opcode::Div:
        case ir::Opcode::Mod:
        case ir::Opcode::Neg:
            return generate_java_arithmetic(inst);
            
        // Comparison
        case ir::Opcode::Eq:
        case ir::Opcode::Ne:
        case ir::Opcode::Lt:
        case ir::Opcode::Le:
        case ir::Opcode::Gt:
        case ir::Opcode::Ge:
            return generate_java_comparison(inst);
            
        // Logical
        case ir::Opcode::And:
        case ir::Opcode::Or:
        case ir::Opcode::Not:
            return generate_java_logical(inst);
            
        // Memory
        case ir::Opcode::Alloca:
        case ir::Opcode::Load:
        case ir::Opcode::Store:
        case ir::Opcode::GetField:
        case ir::Opcode::SetField:
            return generate_java_memory_access(inst);
            
        // Control flow
        case ir::Opcode::Branch:
        case ir::Opcode::CondBranch:
        case ir::Opcode::Return:
            return generate_java_control_flow(inst);
            
        // Function calls
        case ir::Opcode::Call:
            return generate_java_function_call(inst);
            
        default:
            return "/* Unsupported opcode: " + std::to_string(static_cast<int>(inst.opcode)) + " */";
    }
}

std::string JVMBackend::generate_java_constant(const ir::Instruction& inst) {
    std::ostringstream oss;
    
    if (inst.result) {
        JavaTypeInfo type_info = map_meld_type_to_java(inst.result->type, inst.result->meta_type);
        oss << type_info.java_type << " " << get_java_value_name(*inst.result) << " = ";
        
        switch (inst.opcode) {
            case ir::Opcode::ConstInt:
                oss << std::get<int64_t>(inst.constant_value) << "L";
                break;
            case ir::Opcode::ConstFloat:
                oss << std::get<double>(inst.constant_value);
                break;
            case ir::Opcode::ConstBool:
                oss << (std::get<bool>(inst.constant_value) ? "true" : "false");
                break;
            case ir::Opcode::ConstString:
                oss << "\"" << std::get<std::string>(inst.constant_value) << "\"";
                break;
            case ir::Opcode::ConstNull:
                oss << "null";
                break;
            default:
                oss << "null";
                break;
        }
    }
    
    return oss.str();
}

std::string JVMBackend::generate_java_arithmetic(const ir::Instruction& inst) {
    std::ostringstream oss;
    
    if (inst.result) {
        JavaTypeInfo type_info = map_meld_type_to_java(inst.result->type, inst.result->meta_type);
        oss << type_info.java_type << " " << get_java_value_name(*inst.result) << " = ";
        
        if (inst.opcode == ir::Opcode::Neg) {
            // Unary operation
            oss << "-" << get_java_value_name(*inst.operands[0]);
        } else {
            // Binary operation
            oss << get_java_value_name(*inst.operands[0]);
            oss << " " << get_java_operator(inst.opcode) << " ";
            oss << get_java_value_name(*inst.operands[1]);
        }
    }
    
    return oss.str();
}

std::string JVMBackend::generate_java_comparison(const ir::Instruction& inst) {
    std::ostringstream oss;
    
    if (inst.result) {
        oss << "boolean " << get_java_value_name(*inst.result) << " = ";
        oss << get_java_value_name(*inst.operands[0]);
        oss << " " << get_java_operator(inst.opcode) << " ";
        oss << get_java_value_name(*inst.operands[1]);
    }
    
    return oss.str();
}

std::string JVMBackend::generate_java_logical(const ir::Instruction& inst) {
    std::ostringstream oss;
    
    if (inst.result) {
        oss << "boolean " << get_java_value_name(*inst.result) << " = ";
        
        if (inst.opcode == ir::Opcode::Not) {
            oss << "!" << get_java_value_name(*inst.operands[0]);
        } else {
            oss << get_java_value_name(*inst.operands[0]);
            oss << " " << get_java_operator(inst.opcode) << " ";
            oss << get_java_value_name(*inst.operands[1]);
        }
    }
    
    return oss.str();
}

std::string JVMBackend::generate_java_memory_access(const ir::Instruction& inst) {
    std::ostringstream oss;
    
    switch (inst.opcode) {
        case ir::Opcode::Alloca:
            if (inst.result) {
                JavaTypeInfo type_info = map_meld_type_to_java(inst.result->type, inst.result->meta_type);
                oss << type_info.java_type << " " << get_java_value_name(*inst.result);
                std::string default_val = get_default_value_for_type(type_info);
                if (!default_val.empty()) {
                    oss << " = " << default_val;
                }
            }
            break;
            
        case ir::Opcode::Load:
            if (inst.result && !inst.operands.empty()) {
                JavaTypeInfo type_info = map_meld_type_to_java(inst.result->type, inst.result->meta_type);
                oss << type_info.java_type << " " << get_java_value_name(*inst.result);
                oss << " = " << get_java_value_name(*inst.operands[0]);
            }
            break;
            
        case ir::Opcode::Store:
            if (inst.operands.size() >= 2) {
                oss << get_java_value_name(*inst.operands[1]);
                oss << " = " << get_java_value_name(*inst.operands[0]);
            }
            break;
            
        case ir::Opcode::GetField:
            if (inst.result && !inst.operands.empty()) {
                JavaTypeInfo type_info = map_meld_type_to_java(inst.result->type, inst.result->meta_type);
                oss << type_info.java_type << " " << get_java_value_name(*inst.result);
                oss << " = " << get_java_value_name(*inst.operands[0]) << ".field";
            }
            break;
            
        case ir::Opcode::SetField:
            if (inst.operands.size() >= 2) {
                oss << get_java_value_name(*inst.operands[0]) << ".field";
                oss << " = " << get_java_value_name(*inst.operands[1]);
            }
            break;
            
        default:
            break;
    }
    
    return oss.str();
}

std::string JVMBackend::generate_java_control_flow(const ir::Instruction& inst) {
    std::ostringstream oss;
    
    switch (inst.opcode) {
        case ir::Opcode::Return:
            oss << "return";
            if (!inst.operands.empty()) {
                oss << " " << get_java_value_name(*inst.operands[0]);
            }
            break;
            
        case ir::Opcode::Branch:
            oss << "// goto " << inst.target_label;
            break;
            
        case ir::Opcode::CondBranch:
            if (!inst.operands.empty()) {
                oss << "if (" << get_java_value_name(*inst.operands[0]) << ") {";
                oss << " /* goto " << inst.target_label << " */ }";
                oss << " else { /* goto " << inst.else_label << " */ }";
            }
            break;
            
        default:
            break;
    }
    
    return oss.str();
}

std::string JVMBackend::generate_java_function_call(const ir::Instruction& inst) {
    std::ostringstream oss;
    
    if (!inst.operands.empty()) {
        if (inst.result) {
            JavaTypeInfo type_info = map_meld_type_to_java(inst.result->type, inst.result->meta_type);
            oss << type_info.java_type << " " << get_java_value_name(*inst.result) << " = ";
        }
        
        oss << get_java_value_name(*inst.operands[0]) << "(";
        for (size_t i = 1; i < inst.operands.size(); ++i) {
            if (i > 1) oss << ", ";
            oss << get_java_value_name(*inst.operands[i]);
        }
        oss << ")";
    }
    
    return oss.str();
}

// Helper methods implementation
void JVMBackend::initialize_type_mappings() {
    type_mapping_["int"] = JavaTypeInfo("long", "", true, false);
    type_mapping_["float"] = JavaTypeInfo("double", "", true, false);
    type_mapping_["bool"] = JavaTypeInfo("boolean", "", true, false);
    type_mapping_["string"] = JavaTypeInfo("String", "", false, false);
    type_mapping_["void"] = JavaTypeInfo("void", "", true, false);
    
    // Add common imports
    required_imports_.insert("java.util.*");
    required_imports_.insert("java.util.function.*");
}

JavaTypeInfo JVMBackend::map_meld_type_to_java(ir::ValueType meld_type, std::shared_ptr<meta::MetaType> meta_type) {
    switch (meld_type) {
        case ir::ValueType::Int:
            return JavaTypeInfo("long", "", true, false);
        case ir::ValueType::Float:
            return JavaTypeInfo("double", "", true, false);
        case ir::ValueType::Bool:
            return JavaTypeInfo("boolean", "", true, false);
        case ir::ValueType::String:
            return JavaTypeInfo("String", "", false, false);
        case ir::ValueType::Void:
            return JavaTypeInfo("void", "", true, false);
        case ir::ValueType::Pointer:
            return JavaTypeInfo("Object", "", false, false);
        case ir::ValueType::Struct:
            return JavaTypeInfo("Object", "", false, false);
        case ir::ValueType::Function:
            add_required_import("java.util.function.Function");
            return JavaTypeInfo("Function<Object, Object>", "java.util.function.Function", false, false);
        default:
            return JavaTypeInfo("Object", "", false, false);
    }
}

std::string JVMBackend::get_java_package_declaration() const {
    if (options_.package_name.empty()) {
        return "";
    }
    return "package " + options_.package_name + ";";
}

std::string JVMBackend::get_java_imports() const {
    std::ostringstream oss;
    for (const auto& import : required_imports_) {
        oss << "import " << import << ";" << std::endl;
    }
    return oss.str();
}

std::string JVMBackend::generate_class_header(const std::string& class_name) {
    return "public class " + sanitize_java_identifier(class_name) + " {";
}

std::string JVMBackend::sanitize_java_identifier(const std::string& name) const {
    std::string result = name;
    
    // Replace invalid characters
    std::replace(result.begin(), result.end(), '-', '_');
    std::replace(result.begin(), result.end(), '.', '_');
    std::replace(result.begin(), result.end(), '%', '_');
    
    // Ensure it starts with a letter or underscore
    if (!result.empty() && !std::isalpha(result[0]) && result[0] != '_') {
        result = "_" + result;
    }
    
    // Check for Java keywords and prefix if necessary
    static const std::set<std::string> java_keywords = {
        "abstract", "assert", "boolean", "break", "byte", "case", "catch", "char",
        "class", "const", "continue", "default", "do", "double", "else", "enum",
        "extends", "final", "finally", "float", "for", "goto", "if", "implements",
        "import", "instanceof", "int", "interface", "long", "native", "new", "null",
        "package", "private", "protected", "public", "return", "short", "static",
        "strictfp", "super", "switch", "synchronized", "this", "throw", "throws",
        "transient", "try", "void", "volatile", "while"
    };
    
    if (java_keywords.count(result)) {
        result = "meld_" + result;
    }
    
    return result;
}

std::string JVMBackend::mangle_method_name(const std::string& name) const {
    return sanitize_java_identifier(name);
}

std::string JVMBackend::get_java_operator(ir::Opcode opcode) {
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

std::string JVMBackend::get_java_value_name(const ir::Value& value) {
    return sanitize_java_identifier(value.name);
}

std::string JVMBackend::get_default_value_for_type(const JavaTypeInfo& type_info) {
    if (type_info.is_primitive) {
        if (type_info.java_type == "boolean") return "false";
        if (type_info.java_type == "long") return "0L";
        if (type_info.java_type == "double") return "0.0";
        if (type_info.java_type == "int") return "0";
        if (type_info.java_type == "float") return "0.0f";
        if (type_info.java_type == "char") return "'\\0'";
        if (type_info.java_type == "byte") return "(byte)0";
        if (type_info.java_type == "short") return "(short)0";
    }
    return "null";
}

void JVMBackend::add_required_import(const std::string& import) {
    required_imports_.insert(import);
}

void JVMBackend::set_options(const JVMGenerationOptions& options) {
    options_ = options;
}

// JVMRuntimeGenerator implementation
JVMRuntimeGenerator::JVMRuntimeGenerator(const JVMGenerationOptions& options)
    : options_(options) {
}

std::map<std::string, std::string> JVMRuntimeGenerator::generate_runtime_classes() {
    std::map<std::string, std::string> classes;
    
    classes["MeldResult"] = generate_result_class();
    classes["MeldOption"] = generate_option_class();
    classes["MeldRuntime"] = generate_meld_runtime_class();
    
    return classes;
}

std::string JVMRuntimeGenerator::generate_result_class() {
    std::vector<std::string> imports = {"java.util.function.Function"};
    
    std::string class_body = R"(
    private final T value;
    private final E error;
    private final boolean isSuccess;
    
    private MeldResult(T value, E error, boolean isSuccess) {
        this.value = value;
        this.error = error;
        this.isSuccess = isSuccess;
    }
    
    public static <T, E> MeldResult<T, E> success(T value) {
        return new MeldResult<>(value, null, true);
    }
    
    public static <T, E> MeldResult<T, E> error(E error) {
        return new MeldResult<>(null, error, false);
    }
    
    public boolean isSuccess() {
        return isSuccess;
    }
    
    public boolean isError() {
        return !isSuccess;
    }
    
    public T getValue() {
        if (!isSuccess) {
            throw new IllegalStateException("Cannot get value from error result");
        }
        return value;
    }
    
    public E getError() {
        if (isSuccess) {
            throw new IllegalStateException("Cannot get error from success result");
        }
        return error;
    }
    
    public <U> MeldResult<U, E> map(Function<T, U> mapper) {
        if (isSuccess) {
            return MeldResult.success(mapper.apply(value));
        } else {
            return MeldResult.error(error);
        }
    }
    
    public <U> MeldResult<U, E> flatMap(Function<T, MeldResult<U, E>> mapper) {
        if (isSuccess) {
            return mapper.apply(value);
        } else {
            return MeldResult.error(error);
        }
    }
)";
    
    return generate_class_template("MeldResult<T, E>", class_body, imports);
}

std::string JVMRuntimeGenerator::generate_option_class() {
    std::vector<std::string> imports = {"java.util.function.Function"};
    
    std::string class_body = R"(
    private final T value;
    private final boolean hasValue;
    
    private MeldOption(T value, boolean hasValue) {
        this.value = value;
        this.hasValue = hasValue;
    }
    
    public static <T> MeldOption<T> some(T value) {
        return new MeldOption<>(value, true);
    }
    
    public static <T> MeldOption<T> none() {
        return new MeldOption<>(null, false);
    }
    
    public boolean isSome() {
        return hasValue;
    }
    
    public boolean isNone() {
        return !hasValue;
    }
    
    public T getValue() {
        if (!hasValue) {
            throw new IllegalStateException("Cannot get value from None");
        }
        return value;
    }
    
    public T getOrElse(T defaultValue) {
        return hasValue ? value : defaultValue;
    }
    
    public <U> MeldOption<U> map(Function<T, U> mapper) {
        if (hasValue) {
            return MeldOption.some(mapper.apply(value));
        } else {
            return MeldOption.none();
        }
    }
)";
    
    return generate_class_template("MeldOption<T>", class_body, imports);
}

std::string JVMRuntimeGenerator::generate_meld_runtime_class() {
    std::string class_body = R"(
    public static void println(String message) {
        System.out.println(message);
    }
    
    public static void print(String message) {
        System.out.print(message);
    }
    
    public static String readLine() {
        return System.console().readLine();
    }
    
    // Type system support
    public static String typeOf(Object obj) {
        return obj != null ? obj.getClass().getSimpleName() : "null";
    }
    
    // Memory management helpers
    public static <T> T[] allocateArray(Class<T> type, int size) {
        return (T[]) java.lang.reflect.Array.newInstance(type, size);
    }
)";
    
    return generate_class_template("MeldRuntime", class_body);
}

std::string JVMRuntimeGenerator::get_package_declaration() const {
    if (options_.package_name.empty()) {
        return "";
    }
    return "package " + options_.package_name + ";";
}

std::string JVMRuntimeGenerator::generate_class_template(const std::string& class_name, 
                                                       const std::string& class_body,
                                                       const std::vector<std::string>& imports) {
    std::ostringstream oss;
    
    oss << get_package_declaration() << std::endl << std::endl;
    
    for (const auto& import : imports) {
        oss << "import " << import << ";" << std::endl;
    }
    if (!imports.empty()) {
        oss << std::endl;
    }
    
    oss << "public class " << class_name << " {" << class_body << std::endl << "}" << std::endl;
    
    return oss.str();
}

} // namespace meld::compiler