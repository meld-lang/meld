#include "meld/compiler/wasm_backend.hpp"
#include <sstream>
#include <algorithm>
#include <set>
#include <filesystem>
#include <cctype>
#include <iomanip>

namespace meld::compiler {

WasmBackend::WasmBackend(const WasmGenerationOptions& options)
    : options_(options), next_local_index_(0) {
    initialize_type_mappings();
}

std::map<std::string, std::string> WasmBackend::generate_wasm_files(const ir::Module& module) {
    std::map<std::string, std::string> files;
    
    // Generate WebAssembly Text (WAT) format
    if (options_.generate_wat) {
        std::string wat_content = generate_wat_module(module);
        files[options_.module_name + ".wat"] = wat_content;
    }
    
    // Generate JavaScript bindings
    if (options_.generate_js_bindings) {
        std::string js_content = generate_js_bindings(module);
        files[options_.module_name + ".js"] = js_content;
    }
    
    // Generate TypeScript definitions
    if (options_.generate_ts_definitions) {
        std::string ts_content = generate_ts_definitions(module);
        files[options_.module_name + ".d.ts"] = ts_content;
    }
    
    // Generate runtime support files if needed
    WasmRuntimeGenerator runtime_gen(options_);
    auto runtime_files = runtime_gen.generate_runtime_files();
    for (const auto& [filename, content] : runtime_files) {
        files[filename] = content;
    }
    
    return files;
}

std::string WasmBackend::generate_wat_module(const ir::Module& module) {
    std::ostringstream oss;
    
    // Module header
    oss << ";; Generated WebAssembly module from Meld source" << std::endl;
    oss << "(module" << std::endl;
    
    // Memory declaration
    oss << "  ;; Memory (1 page = 64KB)" << std::endl;
    oss << "  (memory " << options_.memory_size << ")" << std::endl;
    if (options_.export_memory) {
        oss << "  (export \"memory\" (memory 0))" << std::endl;
    }
    oss << std::endl;
    
    // Imports
    std::string imports = get_wasm_imports();
    if (!imports.empty()) {
        oss << "  ;; Imports" << std::endl;
        oss << imports << std::endl;
    }
    
    // Global variables
    if (!module.globals.empty()) {
        oss << "  ;; Global variables" << std::endl;
        for (const auto& global : module.globals) {
            WasmTypeInfo type_info = map_meld_type_to_wasm(global->type, global->meta_type);
            oss << "  (global $" << sanitize_wasm_identifier(global->name) 
                << " (mut " << type_info.wasm_type << ") ";
            oss << "(" << type_info.wasm_type << ".const " 
                << get_default_value_for_type(type_info) << "))" << std::endl;
        }
        oss << std::endl;
    }
    
    // Function declarations
    oss << "  ;; Function declarations" << std::endl;
    for (const auto& function : module.functions) {
        oss << generate_wasm_function(*function) << std::endl;
    }
    
    // Exports
    std::string exports = get_wasm_exports(module);
    if (!exports.empty()) {
        oss << "  ;; Exports" << std::endl;
        oss << exports << std::endl;
    }
    
    oss << ")" << std::endl;
    
    return oss.str();
}

std::string WasmBackend::generate_js_bindings(const ir::Module& module) {
    std::ostringstream oss;
    
    oss << "// Generated JavaScript bindings for " << options_.module_name << std::endl;
    oss << "class " << options_.module_name << " {" << std::endl;
    oss << "  constructor() {" << std::endl;
    oss << "    this.instance = null;" << std::endl;
    oss << "    this.memory = null;" << std::endl;
    oss << "    this.textDecoder = new TextDecoder();" << std::endl;
    oss << "    this.textEncoder = new TextEncoder();" << std::endl;
    oss << "  }" << std::endl << std::endl;
    
    // Load method
    oss << "  async load(wasmPath) {" << std::endl;
    oss << "    const wasmModule = await WebAssembly.instantiateStreaming(" << std::endl;
    oss << "      fetch(wasmPath)," << std::endl;
    oss << "      {" << std::endl;
    oss << "        env: {" << std::endl;
    oss << "          print: (ptr) => console.log(this.readString(ptr))," << std::endl;
    oss << "          abort: () => { throw new Error('WebAssembly abort'); }" << std::endl;
    oss << "        }" << std::endl;
    oss << "      }" << std::endl;
    oss << "    );" << std::endl;
    oss << "    this.instance = wasmModule.instance;" << std::endl;
    oss << "    this.memory = this.instance.exports.memory;" << std::endl;
    oss << "  }" << std::endl << std::endl;
    
    // Memory utilities
    oss << "  readString(ptr) {" << std::endl;
    oss << "    const memory = new Uint8Array(this.memory.buffer);" << std::endl;
    oss << "    let length = 0;" << std::endl;
    oss << "    while (memory[ptr + length] !== 0) length++;" << std::endl;
    oss << "    return this.textDecoder.decode(memory.slice(ptr, ptr + length));" << std::endl;
    oss << "  }" << std::endl << std::endl;
    
    oss << "  writeString(str) {" << std::endl;
    oss << "    const bytes = this.textEncoder.encode(str + '\\0');" << std::endl;
    oss << "    const ptr = this.instance.exports.malloc(bytes.length);" << std::endl;
    oss << "    const memory = new Uint8Array(this.memory.buffer);" << std::endl;
    oss << "    memory.set(bytes, ptr);" << std::endl;
    oss << "    return ptr;" << std::endl;
    oss << "  }" << std::endl << std::endl;
    
    // Function wrappers
    for (const auto& function : module.functions) {
        if (function->name != "main") {  // Skip internal functions
            oss << generate_js_function_wrapper(*function) << std::endl;
        }
    }
    
    // Main function wrapper
    oss << "  main() {" << std::endl;
    oss << "    if (this.instance && this.instance.exports.main) {" << std::endl;
    oss << "      return this.instance.exports.main();" << std::endl;
    oss << "    }" << std::endl;
    oss << "    throw new Error('Main function not found');" << std::endl;
    oss << "  }" << std::endl;
    
    oss << "}" << std::endl << std::endl;
    
    // Export for Node.js and browser
    oss << "if (typeof module !== 'undefined' && module.exports) {" << std::endl;
    oss << "  module.exports = " << options_.module_name << ";" << std::endl;
    oss << "} else if (typeof window !== 'undefined') {" << std::endl;
    oss << "  window." << options_.module_name << " = " << options_.module_name << ";" << std::endl;
    oss << "}" << std::endl;
    
    return oss.str();
}

std::string WasmBackend::generate_ts_definitions(const ir::Module& module) {
    std::ostringstream oss;
    
    oss << "// Generated TypeScript definitions for " << options_.module_name << std::endl;
    oss << "declare class " << options_.module_name << " {" << std::endl;
    oss << "  constructor();" << std::endl;
    oss << "  load(wasmPath: string): Promise<void>;" << std::endl;
    oss << "  readString(ptr: number): string;" << std::endl;
    oss << "  writeString(str: string): number;" << std::endl;
    oss << "  main(): number;" << std::endl;
    
    // Function signatures
    for (const auto& function : module.functions) {
        if (function->name != "main") {
            oss << "  " << generate_ts_function_signature(*function) << ";" << std::endl;
        }
    }
    
    oss << "}" << std::endl << std::endl;
    
    // Module exports
    oss << "export = " << options_.module_name << ";" << std::endl;
    
    return oss.str();
}

std::string WasmBackend::generate_wasm_function(const ir::Function& function) {
    std::ostringstream oss;
    
    // Reset local variable tracking for this function
    local_variables_.clear();
    next_local_index_ = 0;
    
    // Function signature
    oss << "  (func $" << mangle_function_name(function.name);
    
    // Parameters
    if (!function.parameters.empty()) {
        for (const auto& param : function.parameters) {
            WasmTypeInfo param_type = map_meld_type_to_wasm(param->type, param->meta_type);
            oss << " (param $" << sanitize_wasm_identifier(param->name) 
                << " " << param_type.wasm_type << ")";
            allocate_local_variable(param->name, param_type);
        }
    }
    
    // Return type
    if (function.return_value) {
        WasmTypeInfo return_type = map_meld_type_to_wasm(function.return_value->type, function.return_value->meta_type);
        oss << " (result " << return_type.wasm_type << ")";
    }
    
    oss << std::endl;
    
    // Local variables (collect from function body)
    std::set<std::string> locals_declared;
    for (const auto& block : function.basic_blocks) {
        for (const auto& inst : block->instructions) {
            if (inst->result && inst->result->name != function.return_value->name) {
                WasmTypeInfo type_info = map_meld_type_to_wasm(inst->result->type, inst->result->meta_type);
                if (locals_declared.find(inst->result->name) == locals_declared.end()) {
                    oss << "    (local $" << sanitize_wasm_identifier(inst->result->name) 
                        << " " << type_info.wasm_type << ")" << std::endl;
                    locals_declared.insert(inst->result->name);
                    allocate_local_variable(inst->result->name, type_info);
                }
            }
        }
    }
    
    // Function body
    oss << generate_function_body(function);
    
    oss << "  )";
    
    return oss.str();
}

std::string WasmBackend::generate_function_body(const ir::Function& function) {
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
                WasmTypeInfo return_type = map_meld_type_to_wasm(function.return_value->type, function.return_value->meta_type);
                oss << "    " << return_type.wasm_type << ".const " 
                    << get_default_value_for_type(return_type) << std::endl;
            }
        }
    }
    
    return oss.str();
}

std::string WasmBackend::generate_basic_block(const ir::BasicBlock& block) {
    std::ostringstream oss;
    
    // Generate label (as comment since WebAssembly uses structured control flow)
    if (block.label != "entry") {
        oss << "    ;; Block: " << block.label << std::endl;
    }
    
    // Generate instructions
    for (const auto& inst : block.instructions) {
        std::string inst_code = generate_instruction(*inst);
        if (!inst_code.empty()) {
            oss << "    " << inst_code << std::endl;
        }
    }
    
    return oss.str();
}

std::string WasmBackend::generate_instruction(const ir::Instruction& inst) {
    switch (inst.opcode) {
        // Constants
        case ir::Opcode::ConstInt:
        case ir::Opcode::ConstFloat:
        case ir::Opcode::ConstBool:
        case ir::Opcode::ConstString:
        case ir::Opcode::ConstNull:
            return generate_wasm_constant(inst);
            
        // Arithmetic
        case ir::Opcode::Add:
        case ir::Opcode::Sub:
        case ir::Opcode::Mul:
        case ir::Opcode::Div:
        case ir::Opcode::Mod:
        case ir::Opcode::Neg:
            return generate_wasm_arithmetic(inst);
            
        // Comparison
        case ir::Opcode::Eq:
        case ir::Opcode::Ne:
        case ir::Opcode::Lt:
        case ir::Opcode::Le:
        case ir::Opcode::Gt:
        case ir::Opcode::Ge:
            return generate_wasm_comparison(inst);
            
        // Logical
        case ir::Opcode::And:
        case ir::Opcode::Or:
        case ir::Opcode::Not:
            return generate_wasm_logical(inst);
            
        // Memory
        case ir::Opcode::Alloca:
        case ir::Opcode::Load:
        case ir::Opcode::Store:
        case ir::Opcode::GetField:
        case ir::Opcode::SetField:
            return generate_wasm_memory_access(inst);
            
        // Control flow
        case ir::Opcode::Branch:
        case ir::Opcode::CondBranch:
        case ir::Opcode::Return:
            return generate_wasm_control_flow(inst);
            
        // Function calls
        case ir::Opcode::Call:
            return generate_wasm_function_call(inst);
            
        default:
            return ";; Unsupported opcode: " + std::to_string(static_cast<int>(inst.opcode));
    }
}

std::string WasmBackend::generate_wasm_constant(const ir::Instruction& inst) {
    std::ostringstream oss;
    
    if (inst.result) {
        WasmTypeInfo type_info = map_meld_type_to_wasm(inst.result->type, inst.result->meta_type);
        
        switch (inst.opcode) {
            case ir::Opcode::ConstInt:
                oss << type_info.wasm_type << ".const " << std::get<int64_t>(inst.constant_value) << std::endl;
                oss << "    local.set $" << sanitize_wasm_identifier(inst.result->name);
                break;
            case ir::Opcode::ConstFloat:
                oss << type_info.wasm_type << ".const " << std::get<double>(inst.constant_value) << std::endl;
                oss << "    local.set $" << sanitize_wasm_identifier(inst.result->name);
                break;
            case ir::Opcode::ConstBool:
                oss << "i32.const " << (std::get<bool>(inst.constant_value) ? "1" : "0") << std::endl;
                oss << "    local.set $" << sanitize_wasm_identifier(inst.result->name);
                break;
            case ir::Opcode::ConstString: {
                // For strings, we need to allocate memory and store the string
                std::string str_value = std::get<std::string>(inst.constant_value);
                oss << ";; String constant: \"" << str_value << "\"" << std::endl;
                oss << "    " << generate_string_allocation(str_value) << std::endl;
                oss << "    local.set $" << sanitize_wasm_identifier(inst.result->name);
                break;
            }
            case ir::Opcode::ConstNull:
                oss << "i32.const 0" << std::endl;
                oss << "    local.set $" << sanitize_wasm_identifier(inst.result->name);
                break;
            default:
                oss << "i32.const 0" << std::endl;
                oss << "    local.set $" << sanitize_wasm_identifier(inst.result->name);
                break;
        }
    }
    
    return oss.str();
}

std::string WasmBackend::generate_wasm_arithmetic(const ir::Instruction& inst) {
    std::ostringstream oss;
    
    if (inst.result) {
        WasmTypeInfo type_info = map_meld_type_to_wasm(inst.result->type, inst.result->meta_type);
        
        if (inst.opcode == ir::Opcode::Neg) {
            // Unary operation
            oss << "local.get $" << sanitize_wasm_identifier(inst.operands[0]->name) << std::endl;
            if (type_info.wasm_type == "i64") {
                oss << "    i64.const -1" << std::endl;
                oss << "    i64.mul";
            } else if (type_info.wasm_type == "f64") {
                oss << "    f64.neg";
            } else {
                oss << "    i32.const -1" << std::endl;
                oss << "    i32.mul";
            }
        } else {
            // Binary operation
            oss << "local.get $" << sanitize_wasm_identifier(inst.operands[0]->name) << std::endl;
            oss << "    local.get $" << sanitize_wasm_identifier(inst.operands[1]->name) << std::endl;
            oss << "    " << get_wasm_operator(inst.opcode);
        }
        
        oss << std::endl << "    local.set $" << sanitize_wasm_identifier(inst.result->name);
    }
    
    return oss.str();
}

std::string WasmBackend::generate_wasm_comparison(const ir::Instruction& inst) {
    std::ostringstream oss;
    
    if (inst.result) {
        oss << "local.get $" << sanitize_wasm_identifier(inst.operands[0]->name) << std::endl;
        oss << "    local.get $" << sanitize_wasm_identifier(inst.operands[1]->name) << std::endl;
        oss << "    " << get_wasm_operator(inst.opcode) << std::endl;
        oss << "    local.set $" << sanitize_wasm_identifier(inst.result->name);
    }
    
    return oss.str();
}

std::string WasmBackend::generate_wasm_logical(const ir::Instruction& inst) {
    std::ostringstream oss;
    
    if (inst.result) {
        if (inst.opcode == ir::Opcode::Not) {
            oss << "local.get $" << sanitize_wasm_identifier(inst.operands[0]->name) << std::endl;
            oss << "    i32.eqz";
        } else {
            oss << "local.get $" << sanitize_wasm_identifier(inst.operands[0]->name) << std::endl;
            oss << "    local.get $" << sanitize_wasm_identifier(inst.operands[1]->name) << std::endl;
            oss << "    " << get_wasm_operator(inst.opcode);
        }
        
        oss << std::endl << "    local.set $" << sanitize_wasm_identifier(inst.result->name);
    }
    
    return oss.str();
}

std::string WasmBackend::generate_wasm_memory_access(const ir::Instruction& inst) {
    std::ostringstream oss;
    
    switch (inst.opcode) {
        case ir::Opcode::Alloca:
            if (inst.result) {
                WasmTypeInfo type_info = map_meld_type_to_wasm(inst.result->type, inst.result->meta_type);
                if (type_info.needs_memory_allocation) {
                    oss << generate_memory_allocation(8) << std::endl;  // Allocate 8 bytes by default
                    oss << "    local.set $" << sanitize_wasm_identifier(inst.result->name);
                } else {
                    oss << type_info.wasm_type << ".const " << get_default_value_for_type(type_info) << std::endl;
                    oss << "    local.set $" << sanitize_wasm_identifier(inst.result->name);
                }
            }
            break;
            
        case ir::Opcode::Load:
            if (inst.result && !inst.operands.empty()) {
                WasmTypeInfo type_info = map_meld_type_to_wasm(inst.result->type, inst.result->meta_type);
                if (type_info.needs_memory_allocation) {
                    oss << "local.get $" << sanitize_wasm_identifier(inst.operands[0]->name) << std::endl;
                    oss << "    i32.load" << std::endl;
                } else {
                    oss << "local.get $" << sanitize_wasm_identifier(inst.operands[0]->name);
                }
                oss << std::endl << "    local.set $" << sanitize_wasm_identifier(inst.result->name);
            }
            break;
            
        case ir::Opcode::Store:
            if (inst.operands.size() >= 2) {
                WasmTypeInfo type_info = map_meld_type_to_wasm(inst.operands[0]->type, inst.operands[0]->meta_type);
                if (type_info.needs_memory_allocation) {
                    oss << "local.get $" << sanitize_wasm_identifier(inst.operands[1]->name) << std::endl;
                    oss << "    local.get $" << sanitize_wasm_identifier(inst.operands[0]->name) << std::endl;
                    oss << "    i32.store";
                } else {
                    oss << "local.get $" << sanitize_wasm_identifier(inst.operands[0]->name) << std::endl;
                    oss << "    local.set $" << sanitize_wasm_identifier(inst.operands[1]->name);
                }
            }
            break;
            
        case ir::Opcode::GetField:
            if (inst.result && !inst.operands.empty()) {
                oss << "local.get $" << sanitize_wasm_identifier(inst.operands[0]->name) << std::endl;
                oss << "    i32.load offset=0" << std::endl;  // Assume field at offset 0
                oss << "    local.set $" << sanitize_wasm_identifier(inst.result->name);
            }
            break;
            
        case ir::Opcode::SetField:
            if (inst.operands.size() >= 2) {
                oss << "local.get $" << sanitize_wasm_identifier(inst.operands[0]->name) << std::endl;
                oss << "    local.get $" << sanitize_wasm_identifier(inst.operands[1]->name) << std::endl;
                oss << "    i32.store offset=0";  // Assume field at offset 0
            }
            break;
            
        default:
            break;
    }
    
    return oss.str();
}

std::string WasmBackend::generate_wasm_control_flow(const ir::Instruction& inst) {
    std::ostringstream oss;
    
    switch (inst.opcode) {
        case ir::Opcode::Return:
            if (!inst.operands.empty()) {
                oss << "local.get $" << sanitize_wasm_identifier(inst.operands[0]->name) << std::endl;
                oss << "    return";
            } else {
                oss << "return";
            }
            break;
            
        case ir::Opcode::Branch:
            oss << ";; Unconditional branch to " << inst.target_label << std::endl;
            oss << "    br 0";  // WebAssembly uses structured control flow
            break;
            
        case ir::Opcode::CondBranch:
            if (!inst.operands.empty()) {
                oss << "local.get $" << sanitize_wasm_identifier(inst.operands[0]->name) << std::endl;
                oss << "    if" << std::endl;
                oss << "      ;; Branch to " << inst.target_label << std::endl;
                oss << "      br 0" << std::endl;
                oss << "    else" << std::endl;
                oss << "      ;; Branch to " << inst.else_label << std::endl;
                oss << "      br 0" << std::endl;
                oss << "    end";
            }
            break;
            
        default:
            break;
    }
    
    return oss.str();
}

std::string WasmBackend::generate_wasm_function_call(const ir::Instruction& inst) {
    std::ostringstream oss;
    
    if (!inst.operands.empty()) {
        // Push arguments onto stack
        for (size_t i = 1; i < inst.operands.size(); ++i) {
            oss << "local.get $" << sanitize_wasm_identifier(inst.operands[i]->name) << std::endl;
            oss << "    ";
        }
        
        // Call function
        oss << "call $" << mangle_function_name(inst.operands[0]->name);
        
        // Store result if needed
        if (inst.result) {
            oss << std::endl << "    local.set $" << sanitize_wasm_identifier(inst.result->name);
        }
    }
    
    return oss.str();
}

// Helper methods implementation
void WasmBackend::initialize_type_mappings() {
    type_mapping_["int"] = WasmTypeInfo("i64", "number", "number", true, false);
    type_mapping_["float"] = WasmTypeInfo("f64", "number", "number", true, false);
    type_mapping_["bool"] = WasmTypeInfo("i32", "boolean", "boolean", true, false);
    type_mapping_["string"] = WasmTypeInfo("i32", "string", "string", false, true);  // Pointer to string
    type_mapping_["void"] = WasmTypeInfo("", "void", "void", true, false);
}

WasmTypeInfo WasmBackend::map_meld_type_to_wasm(ir::ValueType meld_type, std::shared_ptr<meta::MetaType> meta_type) {
    switch (meld_type) {
        case ir::ValueType::Int:
            return WasmTypeInfo("i64", "number", "number", true, false);
        case ir::ValueType::Float:
            return WasmTypeInfo("f64", "number", "number", true, false);
        case ir::ValueType::Bool:
            return WasmTypeInfo("i32", "boolean", "boolean", true, false);
        case ir::ValueType::String:
            return WasmTypeInfo("i32", "string", "string", false, true);
        case ir::ValueType::Void:
            return WasmTypeInfo("", "void", "void", true, false);
        case ir::ValueType::Pointer:
            return WasmTypeInfo("i32", "number", "number", false, false);
        case ir::ValueType::Struct:
            return WasmTypeInfo("i32", "object", "any", false, true);
        case ir::ValueType::Function:
            return WasmTypeInfo("i32", "Function", "Function", false, false);
        default:
            return WasmTypeInfo("i32", "any", "any", false, false);
    }
}

std::string WasmBackend::get_wasm_imports() const {
    std::ostringstream oss;
    
    // Standard imports for console output and memory management
    oss << "  (import \"env\" \"print\" (func $print (param i32)))" << std::endl;
    oss << "  (import \"env\" \"abort\" (func $abort))" << std::endl;
    oss << "  (import \"env\" \"malloc\" (func $malloc (param i32) (result i32)))" << std::endl;
    oss << "  (import \"env\" \"free\" (func $free (param i32)))" << std::endl;
    
    return oss.str();
}

std::string WasmBackend::get_wasm_exports(const ir::Module& module) const {
    std::ostringstream oss;
    
    // Export all public functions
    for (const auto& function : module.functions) {
        oss << "  (export \"" << function->name << "\" (func $" 
            << mangle_function_name(function->name) << "))" << std::endl;
    }
    
    return oss.str();
}

std::string WasmBackend::sanitize_wasm_identifier(const std::string& name) const {
    std::string result = name;
    
    // Replace invalid characters
    std::replace(result.begin(), result.end(), '-', '_');
    std::replace(result.begin(), result.end(), '.', '_');
    std::replace(result.begin(), result.end(), '%', '_');
    
    // Ensure it starts with a letter or underscore
    if (!result.empty() && !std::isalpha(result[0]) && result[0] != '_') {
        result = "_" + result;
    }
    
    return result;
}

std::string WasmBackend::mangle_function_name(const std::string& name) const {
    return sanitize_wasm_identifier(name);
}

std::string WasmBackend::get_wasm_operator(ir::Opcode opcode) {
    switch (opcode) {
        case ir::Opcode::Add: return "i64.add";
        case ir::Opcode::Sub: return "i64.sub";
        case ir::Opcode::Mul: return "i64.mul";
        case ir::Opcode::Div: return "i64.div_s";
        case ir::Opcode::Mod: return "i64.rem_s";
        case ir::Opcode::And: return "i32.and";
        case ir::Opcode::Or: return "i32.or";
        case ir::Opcode::Eq: return "i64.eq";
        case ir::Opcode::Ne: return "i64.ne";
        case ir::Opcode::Lt: return "i64.lt_s";
        case ir::Opcode::Le: return "i64.le_s";
        case ir::Opcode::Gt: return "i64.gt_s";
        case ir::Opcode::Ge: return "i64.ge_s";
        default: return "nop";
    }
}

std::string WasmBackend::get_default_value_for_type(const WasmTypeInfo& type_info) {
    if (type_info.wasm_type == "i32" || type_info.wasm_type == "i64") return "0";
    if (type_info.wasm_type == "f32" || type_info.wasm_type == "f64") return "0.0";
    return "0";
}

int WasmBackend::allocate_local_variable(const std::string& name, const WasmTypeInfo& type_info) {
    int index = next_local_index_++;
    local_variables_[name] = index;
    return index;
}

std::string WasmBackend::generate_memory_allocation(size_t size) {
    return "i32.const " + std::to_string(size) + "\n    call $malloc";
}

std::string WasmBackend::generate_string_allocation(const std::string& str) {
    std::ostringstream oss;
    oss << "i32.const " << (str.length() + 1) << std::endl;  // +1 for null terminator
    oss << "    call $malloc" << std::endl;
    oss << "    ;; TODO: Copy string data to allocated memory";
    return oss.str();
}

std::string WasmBackend::generate_js_function_wrapper(const ir::Function& function) {
    std::ostringstream oss;
    
    oss << "  " << sanitize_wasm_identifier(function.name) << "(";
    
    // Parameters
    for (size_t i = 0; i < function.parameters.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << sanitize_wasm_identifier(function.parameters[i]->name);
    }
    
    oss << ") {" << std::endl;
    
    // Convert parameters and call WebAssembly function
    oss << "    return this.instance.exports." << function.name << "(";
    for (size_t i = 0; i < function.parameters.size(); ++i) {
        if (i > 0) oss << ", ";
        WasmTypeInfo param_type = map_meld_type_to_wasm(function.parameters[i]->type, function.parameters[i]->meta_type);
        oss << generate_js_type_conversion(param_type, function.parameters[i]->name, true);
    }
    oss << ");" << std::endl;
    
    oss << "  }";
    
    return oss.str();
}

std::string WasmBackend::generate_js_type_conversion(const WasmTypeInfo& type_info, 
                                                   const std::string& value, 
                                                   bool to_wasm) {
    if (type_info.wasm_type == "i32" && type_info.js_type == "string") {
        if (to_wasm) {
            return "this.writeString(" + value + ")";
        } else {
            return "this.readString(" + value + ")";
        }
    }
    return value;  // No conversion needed for primitives
}

std::string WasmBackend::generate_ts_function_signature(const ir::Function& function) {
    std::ostringstream oss;
    
    oss << sanitize_wasm_identifier(function.name) << "(";
    
    // Parameters
    for (size_t i = 0; i < function.parameters.size(); ++i) {
        if (i > 0) oss << ", ";
        WasmTypeInfo param_type = map_meld_type_to_wasm(function.parameters[i]->type, function.parameters[i]->meta_type);
        oss << sanitize_wasm_identifier(function.parameters[i]->name) << ": " << param_type.ts_type;
    }
    
    oss << ")";
    
    // Return type
    if (function.return_value) {
        WasmTypeInfo return_type = map_meld_type_to_wasm(function.return_value->type, function.return_value->meta_type);
        oss << ": " << return_type.ts_type;
    } else {
        oss << ": void";
    }
    
    return oss.str();
}

void WasmBackend::set_options(const WasmGenerationOptions& options) {
    options_ = options;
}

// WasmRuntimeGenerator implementation
WasmRuntimeGenerator::WasmRuntimeGenerator(const WasmGenerationOptions& options)
    : options_(options) {
}

std::map<std::string, std::string> WasmRuntimeGenerator::generate_runtime_files() {
    std::map<std::string, std::string> files;
    
    files["meld_runtime.wat"] = generate_memory_manager();
    files["meld_runtime.js"] = generate_js_template(generate_string_utilities());
    
    return files;
}

std::string WasmRuntimeGenerator::generate_memory_manager() {
    return generate_wat_template(R"(
  ;; Simple memory allocator
  (global $heap_ptr (mut i32) (i32.const 1024))
  
  (func $malloc (param $size i32) (result i32)
    (local $ptr i32)
    global.get $heap_ptr
    local.set $ptr
    global.get $heap_ptr
    local.get $size
    i32.add
    global.set $heap_ptr
    local.get $ptr
  )
  
  (func $free (param $ptr i32)
    ;; Simple allocator - no actual freeing
    nop
  )
)");
}

std::string WasmRuntimeGenerator::generate_string_utilities() {
    return R"(
// String utilities for WebAssembly
function createStringFromWasm(instance, ptr) {
  const memory = new Uint8Array(instance.exports.memory.buffer);
  let length = 0;
  while (memory[ptr + length] !== 0) length++;
  return new TextDecoder().decode(memory.slice(ptr, ptr + length));
}

function writeStringToWasm(instance, str) {
  const bytes = new TextEncoder().encode(str + '\0');
  const ptr = instance.exports.malloc(bytes.length);
  const memory = new Uint8Array(instance.exports.memory.buffer);
  memory.set(bytes, ptr);
  return ptr;
}
)";
}

std::string WasmRuntimeGenerator::generate_wat_template(const std::string& content) {
    return ";; Meld WebAssembly Runtime\n(module\n" + content + "\n)";
}

std::string WasmRuntimeGenerator::generate_js_template(const std::string& content) {
    return "// Meld WebAssembly Runtime\n" + content;
}

} // namespace meld::compiler