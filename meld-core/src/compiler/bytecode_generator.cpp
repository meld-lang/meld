#include "meld/compiler/bytecode_generator.hpp"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cstring>

namespace meld::compiler {

// ============================================================================
// BytecodeInstruction
// ============================================================================

std::vector<uint8_t> BytecodeInstruction::serialize() const {
    std::vector<uint8_t> bytes;
    bytes.push_back(static_cast<uint8_t>(opcode));
    for (auto op : operands) {
        bytes.push_back(static_cast<uint8_t>((op >> 24) & 0xFF));
        bytes.push_back(static_cast<uint8_t>((op >> 16) & 0xFF));
        bytes.push_back(static_cast<uint8_t>((op >> 8) & 0xFF));
        bytes.push_back(static_cast<uint8_t>(op & 0xFF));
    }
    return bytes;
}

BytecodeInstruction BytecodeInstruction::deserialize(const uint8_t* data, size_t& offset) {
    auto op = static_cast<BytecodeOp>(data[offset++]);
    BytecodeInstruction inst(op);
    // Determine operand count based on opcode
    size_t operand_count = 0;
    switch (op) {
        case BytecodeOp::LOAD_CONST_INT:
        case BytecodeOp::LOAD_CONST_FLOAT:
        case BytecodeOp::LOAD_CONST_BOOL:
        case BytecodeOp::LOAD_CONST_STRING:
        case BytecodeOp::LOAD_LOCAL:
        case BytecodeOp::STORE_LOCAL:
        case BytecodeOp::LOAD_GLOBAL:
        case BytecodeOp::STORE_GLOBAL:
        case BytecodeOp::JUMP:
        case BytecodeOp::JUMP_IF_TRUE:
        case BytecodeOp::JUMP_IF_FALSE:
            operand_count = 1;
            break;
        case BytecodeOp::CALL:
            operand_count = 2; // function index + arg count
            break;
        default:
            operand_count = 0;
            break;
    }
    for (size_t i = 0; i < operand_count; ++i) {
        uint32_t val = (static_cast<uint32_t>(data[offset]) << 24)
                     | (static_cast<uint32_t>(data[offset + 1]) << 16)
                     | (static_cast<uint32_t>(data[offset + 2]) << 8)
                     | static_cast<uint32_t>(data[offset + 3]);
        offset += 4;
        inst.operands.push_back(val);
    }
    return inst;
}

std::string BytecodeInstruction::to_string() const {
    std::ostringstream oss;
    switch (opcode) {
        case BytecodeOp::LOAD_CONST_INT:   oss << "LOAD_CONST_INT"; break;
        case BytecodeOp::LOAD_CONST_FLOAT: oss << "LOAD_CONST_FLOAT"; break;
        case BytecodeOp::LOAD_CONST_BOOL:  oss << "LOAD_CONST_BOOL"; break;
        case BytecodeOp::LOAD_CONST_STRING:oss << "LOAD_CONST_STRING"; break;
        case BytecodeOp::LOAD_NULL:        oss << "LOAD_NULL"; break;
        case BytecodeOp::LOAD_LOCAL:       oss << "LOAD_LOCAL"; break;
        case BytecodeOp::STORE_LOCAL:      oss << "STORE_LOCAL"; break;
        case BytecodeOp::LOAD_GLOBAL:      oss << "LOAD_GLOBAL"; break;
        case BytecodeOp::STORE_GLOBAL:     oss << "STORE_GLOBAL"; break;
        case BytecodeOp::ADD:              oss << "ADD"; break;
        case BytecodeOp::SUB:              oss << "SUB"; break;
        case BytecodeOp::MUL:              oss << "MUL"; break;
        case BytecodeOp::DIV:              oss << "DIV"; break;
        case BytecodeOp::MOD:              oss << "MOD"; break;
        case BytecodeOp::NEG:              oss << "NEG"; break;
        case BytecodeOp::AND:              oss << "AND"; break;
        case BytecodeOp::OR:               oss << "OR"; break;
        case BytecodeOp::NOT:              oss << "NOT"; break;
        case BytecodeOp::EQ:               oss << "EQ"; break;
        case BytecodeOp::NE:               oss << "NE"; break;
        case BytecodeOp::LT:               oss << "LT"; break;
        case BytecodeOp::LE:               oss << "LE"; break;
        case BytecodeOp::GT:               oss << "GT"; break;
        case BytecodeOp::GE:               oss << "GE"; break;
        case BytecodeOp::JUMP:             oss << "JUMP"; break;
        case BytecodeOp::JUMP_IF_TRUE:     oss << "JUMP_IF_TRUE"; break;
        case BytecodeOp::JUMP_IF_FALSE:    oss << "JUMP_IF_FALSE"; break;
        case BytecodeOp::CALL:             oss << "CALL"; break;
        case BytecodeOp::RETURN:           oss << "RETURN"; break;
        case BytecodeOp::POP:              oss << "POP"; break;
        case BytecodeOp::DUP:              oss << "DUP"; break;
        case BytecodeOp::SWAP:             oss << "SWAP"; break;
        case BytecodeOp::ALLOC:            oss << "ALLOC"; break;
        case BytecodeOp::LOAD_FIELD:       oss << "LOAD_FIELD"; break;
        case BytecodeOp::STORE_FIELD:      oss << "STORE_FIELD"; break;
        case BytecodeOp::TYPEOF:           oss << "TYPEOF"; break;
        case BytecodeOp::CAST:             oss << "CAST"; break;
        case BytecodeOp::NOP:              oss << "NOP"; break;
    }
    for (size_t i = 0; i < operands.size(); ++i) {
        oss << " " << operands[i];
    }
    return oss.str();
}

// ============================================================================
// BytecodeFunction
// ============================================================================

std::vector<uint8_t> BytecodeFunction::serialize() const {
    std::vector<uint8_t> bytes;
    // Name: length-prefixed UTF-8
    uint32_t name_len = static_cast<uint32_t>(name.size());
    bytes.push_back(static_cast<uint8_t>((name_len >> 8) & 0xFF));
    bytes.push_back(static_cast<uint8_t>(name_len & 0xFF));
    bytes.insert(bytes.end(), name.begin(), name.end());
    // Parameter count
    bytes.push_back(static_cast<uint8_t>((parameter_count >> 8) & 0xFF));
    bytes.push_back(static_cast<uint8_t>(parameter_count & 0xFF));
    // Local count
    bytes.push_back(static_cast<uint8_t>((local_count >> 8) & 0xFF));
    bytes.push_back(static_cast<uint8_t>(local_count & 0xFF));
    // Instructions
    uint32_t inst_count = static_cast<uint32_t>(instructions.size());
    bytes.push_back(static_cast<uint8_t>((inst_count >> 24) & 0xFF));
    bytes.push_back(static_cast<uint8_t>((inst_count >> 16) & 0xFF));
    bytes.push_back(static_cast<uint8_t>((inst_count >> 8) & 0xFF));
    bytes.push_back(static_cast<uint8_t>(inst_count & 0xFF));
    for (const auto& inst : instructions) {
        auto inst_bytes = inst.serialize();
        bytes.insert(bytes.end(), inst_bytes.begin(), inst_bytes.end());
    }
    return bytes;
}

BytecodeFunction BytecodeFunction::deserialize(const uint8_t* data, size_t& offset) {
    // Name
    uint32_t name_len = (static_cast<uint32_t>(data[offset]) << 8)
                      | static_cast<uint32_t>(data[offset + 1]);
    offset += 2;
    std::string name(reinterpret_cast<const char*>(data + offset), name_len);
    offset += name_len;
    BytecodeFunction func(std::move(name));
    // Parameter count
    func.parameter_count = (static_cast<uint32_t>(data[offset]) << 8)
                         | static_cast<uint32_t>(data[offset + 1]);
    offset += 2;
    // Local count
    func.local_count = (static_cast<uint32_t>(data[offset]) << 8)
                     | static_cast<uint32_t>(data[offset + 1]);
    offset += 2;
    // Instructions
    uint32_t inst_count = (static_cast<uint32_t>(data[offset]) << 24)
                        | (static_cast<uint32_t>(data[offset + 1]) << 16)
                        | (static_cast<uint32_t>(data[offset + 2]) << 8)
                        | static_cast<uint32_t>(data[offset + 3]);
    offset += 4;
    for (uint32_t i = 0; i < inst_count; ++i) {
        func.instructions.push_back(BytecodeInstruction::deserialize(data, offset));
    }
    return func;
}

std::string BytecodeFunction::to_string() const {
    std::ostringstream oss;
    oss << "function " << name << "(params=" << parameter_count
        << ", locals=" << local_count << "):\n";
    for (size_t i = 0; i < instructions.size(); ++i) {
        oss << "  " << i << ": " << instructions[i].to_string() << "\n";
    }
    return oss.str();
}

// ============================================================================
// BytecodeModule
// ============================================================================

uint32_t BytecodeModule::add_constant(int64_t value) {
    for (uint32_t i = 0; i < constants.size(); ++i) {
        if (auto* v = std::get_if<int64_t>(&constants[i]); v && *v == value) return i;
    }
    constants.push_back(value);
    return static_cast<uint32_t>(constants.size() - 1);
}

uint32_t BytecodeModule::add_constant(double value) {
    for (uint32_t i = 0; i < constants.size(); ++i) {
        if (auto* v = std::get_if<double>(&constants[i]); v && *v == value) return i;
    }
    constants.push_back(value);
    return static_cast<uint32_t>(constants.size() - 1);
}

uint32_t BytecodeModule::add_constant(bool value) {
    for (uint32_t i = 0; i < constants.size(); ++i) {
        if (auto* v = std::get_if<bool>(&constants[i]); v && *v == value) return i;
    }
    constants.push_back(value);
    return static_cast<uint32_t>(constants.size() - 1);
}

uint32_t BytecodeModule::add_constant(const std::string& value) {
    for (uint32_t i = 0; i < constants.size(); ++i) {
        if (auto* v = std::get_if<std::string>(&constants[i]); v && *v == value) return i;
    }
    constants.push_back(value);
    return static_cast<uint32_t>(constants.size() - 1);
}

uint32_t BytecodeModule::add_global(const std::string& name) {
    for (uint32_t i = 0; i < global_names.size(); ++i) {
        if (global_names[i] == name) return i;
    }
    global_names.push_back(name);
    return static_cast<uint32_t>(global_names.size() - 1);
}

std::vector<uint8_t> BytecodeModule::serialize() const {
    std::vector<uint8_t> bytes;
    // Magic: MLDC
    bytes.push_back('M'); bytes.push_back('L');
    bytes.push_back('D'); bytes.push_back('C');
    // Version: 1.0
    bytes.push_back(0x00); bytes.push_back(0x01);
    
    // Constant pool
    uint32_t const_count = static_cast<uint32_t>(constants.size());
    bytes.push_back(static_cast<uint8_t>((const_count >> 24) & 0xFF));
    bytes.push_back(static_cast<uint8_t>((const_count >> 16) & 0xFF));
    bytes.push_back(static_cast<uint8_t>((const_count >> 8) & 0xFF));
    bytes.push_back(static_cast<uint8_t>(const_count & 0xFF));
    for (const auto& c : constants) {
        std::visit([&bytes](const auto& val) {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, int64_t>) {
                bytes.push_back(0x01); // type tag: int
                for (int i = 7; i >= 0; --i)
                    bytes.push_back(static_cast<uint8_t>((val >> (i * 8)) & 0xFF));
            } else if constexpr (std::is_same_v<T, double>) {
                bytes.push_back(0x02); // type tag: float
                uint64_t bits;
                std::memcpy(&bits, &val, sizeof(bits));
                for (int i = 7; i >= 0; --i)
                    bytes.push_back(static_cast<uint8_t>((bits >> (i * 8)) & 0xFF));
            } else if constexpr (std::is_same_v<T, bool>) {
                bytes.push_back(0x03); // type tag: bool
                bytes.push_back(val ? 1 : 0);
            } else if constexpr (std::is_same_v<T, std::string>) {
                bytes.push_back(0x04); // type tag: string
                uint32_t len = static_cast<uint32_t>(val.size());
                bytes.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
                bytes.push_back(static_cast<uint8_t>(len & 0xFF));
                bytes.insert(bytes.end(), val.begin(), val.end());
            }
        }, c);
    }
    
    // Global names
    uint32_t global_count = static_cast<uint32_t>(global_names.size());
    bytes.push_back(static_cast<uint8_t>((global_count >> 8) & 0xFF));
    bytes.push_back(static_cast<uint8_t>(global_count & 0xFF));
    for (const auto& g : global_names) {
        uint32_t len = static_cast<uint32_t>(g.size());
        bytes.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        bytes.push_back(static_cast<uint8_t>(len & 0xFF));
        bytes.insert(bytes.end(), g.begin(), g.end());
    }
    
    // Functions
    uint32_t func_count = static_cast<uint32_t>(functions.size());
    bytes.push_back(static_cast<uint8_t>((func_count >> 8) & 0xFF));
    bytes.push_back(static_cast<uint8_t>(func_count & 0xFF));
    for (const auto& f : functions) {
        auto func_bytes = f.serialize();
        bytes.insert(bytes.end(), func_bytes.begin(), func_bytes.end());
    }
    
    return bytes;
}

BytecodeModule BytecodeModule::deserialize(const uint8_t* data, size_t size) {
    size_t offset = 0;
    // Validate magic
    if (size < 6 || data[0] != 'M' || data[1] != 'L' || data[2] != 'D' || data[3] != 'C') {
        throw std::runtime_error("Invalid bytecode: bad magic number");
    }
    offset = 4;
    // Version
    uint16_t version = (static_cast<uint16_t>(data[offset]) << 8)
                     | static_cast<uint16_t>(data[offset + 1]);
    offset += 2;
    if (version != 1) {
        throw std::runtime_error("Unsupported bytecode version: " + std::to_string(version));
    }
    
    BytecodeModule mod("deserialized");
    
    // Constant pool
    uint32_t const_count = (static_cast<uint32_t>(data[offset]) << 24)
                         | (static_cast<uint32_t>(data[offset + 1]) << 16)
                         | (static_cast<uint32_t>(data[offset + 2]) << 8)
                         | static_cast<uint32_t>(data[offset + 3]);
    offset += 4;
    for (uint32_t i = 0; i < const_count; ++i) {
        uint8_t tag = data[offset++];
        if (tag == 0x01) { // int
            int64_t val = 0;
            for (int j = 7; j >= 0; --j)
                val |= static_cast<int64_t>(data[offset++]) << (j * 8);
            mod.constants.push_back(val);
        } else if (tag == 0x02) { // float
            uint64_t bits = 0;
            for (int j = 7; j >= 0; --j)
                bits |= static_cast<uint64_t>(data[offset++]) << (j * 8);
            double val;
            std::memcpy(&val, &bits, sizeof(val));
            mod.constants.push_back(val);
        } else if (tag == 0x03) { // bool
            mod.constants.push_back(static_cast<bool>(data[offset++]));
        } else if (tag == 0x04) { // string
            uint32_t len = (static_cast<uint32_t>(data[offset]) << 8)
                         | static_cast<uint32_t>(data[offset + 1]);
            offset += 2;
            std::string val(reinterpret_cast<const char*>(data + offset), len);
            offset += len;
            mod.constants.push_back(std::move(val));
        }
    }
    
    // Global names
    uint32_t global_count = (static_cast<uint32_t>(data[offset]) << 8)
                          | static_cast<uint32_t>(data[offset + 1]);
    offset += 2;
    for (uint32_t i = 0; i < global_count; ++i) {
        uint32_t len = (static_cast<uint32_t>(data[offset]) << 8)
                     | static_cast<uint32_t>(data[offset + 1]);
        offset += 2;
        mod.global_names.emplace_back(reinterpret_cast<const char*>(data + offset), len);
        offset += len;
    }
    
    // Functions
    uint32_t func_count = (static_cast<uint32_t>(data[offset]) << 8)
                        | static_cast<uint32_t>(data[offset + 1]);
    offset += 2;
    for (uint32_t i = 0; i < func_count; ++i) {
        mod.functions.push_back(BytecodeFunction::deserialize(data, offset));
    }
    
    return mod;
}

std::string BytecodeModule::to_string() const {
    std::ostringstream oss;
    oss << "module " << name << "\n";
    oss << "constants: " << constants.size() << "\n";
    oss << "globals: " << global_names.size() << "\n";
    for (const auto& f : functions) {
        oss << f.to_string();
    }
    return oss.str();
}

// ============================================================================
// BytecodeGenerator
// ============================================================================

BytecodeModule BytecodeGenerator::generate(const ir::Module& module) {
    BytecodeModule bc_module(module.name);
    current_module_ = &bc_module;
    
    for (const auto& func : module.functions) {
        bc_module.functions.push_back(generate_function(*func));
    }
    
    current_module_ = nullptr;
    return bc_module;
}

BytecodeFunction BytecodeGenerator::generate_function(const ir::Function& function) {
    BytecodeFunction bc_func(function.name);
    bc_func.parameter_count = static_cast<uint32_t>(function.parameters.size());
    
    // Reset per-function state
    value_to_local_.clear();
    label_to_address_.clear();
    pending_jumps_.clear();
    
    // Map parameters to locals
    for (const auto& param : function.parameters) {
        get_or_create_local(param->name, bc_func);
    }
    
    // Generate bytecode for each basic block
    for (const auto& block : function.basic_blocks) {
        generate_block(*block, bc_func);
    }
    
    // Resolve forward jump targets
    resolve_jumps(bc_func);
    
    bc_func.local_count = static_cast<uint32_t>(value_to_local_.size());
    return bc_func;
}

void BytecodeGenerator::generate_block(const ir::BasicBlock& block, BytecodeFunction& func) {
    // Record label → instruction address
    label_to_address_[block.label] = static_cast<uint32_t>(func.instructions.size());
    
    for (const auto& inst : block.instructions) {
        generate_instruction(*inst, func);
    }
}

void BytecodeGenerator::generate_instruction(const ir::Instruction& inst, BytecodeFunction& func) {
    switch (inst.opcode) {
        case ir::Opcode::Add:
        case ir::Opcode::Sub:
        case ir::Opcode::Mul:
        case ir::Opcode::Div:
        case ir::Opcode::Mod:
        case ir::Opcode::Neg:
            generate_arithmetic(inst, func);
            break;
        case ir::Opcode::And:
        case ir::Opcode::Or:
        case ir::Opcode::Not:
            generate_logical(inst, func);
            break;
        case ir::Opcode::Eq:
        case ir::Opcode::Ne:
        case ir::Opcode::Lt:
        case ir::Opcode::Le:
        case ir::Opcode::Gt:
        case ir::Opcode::Ge:
            generate_comparison(inst, func);
            break;
        case ir::Opcode::Alloca:
        case ir::Opcode::Load:
        case ir::Opcode::Store:
            generate_memory(inst, func);
            break;
        case ir::Opcode::Branch:
        case ir::Opcode::CondBranch:
        case ir::Opcode::Call:
        case ir::Opcode::Return:
            generate_control_flow(inst, func);
            break;
        case ir::Opcode::ConstInt:
        case ir::Opcode::ConstFloat:
        case ir::Opcode::ConstBool:
        case ir::Opcode::ConstString:
        case ir::Opcode::ConstNull:
            generate_constant(inst, func);
            break;
        case ir::Opcode::Nop:
            func.instructions.emplace_back(BytecodeOp::NOP);
            break;
        default:
            // Unsupported opcodes emit NOP
            func.instructions.emplace_back(BytecodeOp::NOP);
            break;
    }
}

void BytecodeGenerator::generate_arithmetic(const ir::Instruction& inst, BytecodeFunction& func) {
    // Operands are already on the stack from prior instructions.
    // For binary ops: load left, load right, then emit op.
    // The IR builder already emitted Load instructions for operands,
    // so we just emit the arithmetic opcode here.
    
    BytecodeOp op;
    switch (inst.opcode) {
        case ir::Opcode::Add: op = BytecodeOp::ADD; break;
        case ir::Opcode::Sub: op = BytecodeOp::SUB; break;
        case ir::Opcode::Mul: op = BytecodeOp::MUL; break;
        case ir::Opcode::Div: op = BytecodeOp::DIV; break;
        case ir::Opcode::Mod: op = BytecodeOp::MOD; break;
        case ir::Opcode::Neg: op = BytecodeOp::NEG; break;
        default: return;
    }
    
    // Load operands onto stack
    for (const auto& operand : inst.operands) {
        uint32_t local = get_or_create_local(operand->name, func);
        func.instructions.emplace_back(BytecodeOp::LOAD_LOCAL, local);
    }
    
    func.instructions.emplace_back(op);
    
    // Store result
    if (inst.result) {
        uint32_t result_local = get_or_create_local(inst.result->name, func);
        func.instructions.emplace_back(BytecodeOp::STORE_LOCAL, result_local);
    }
}

void BytecodeGenerator::generate_logical(const ir::Instruction& inst, BytecodeFunction& func) {
    BytecodeOp op;
    switch (inst.opcode) {
        case ir::Opcode::And: op = BytecodeOp::AND; break;
        case ir::Opcode::Or:  op = BytecodeOp::OR; break;
        case ir::Opcode::Not: op = BytecodeOp::NOT; break;
        default: return;
    }
    
    for (const auto& operand : inst.operands) {
        uint32_t local = get_or_create_local(operand->name, func);
        func.instructions.emplace_back(BytecodeOp::LOAD_LOCAL, local);
    }
    
    func.instructions.emplace_back(op);
    
    if (inst.result) {
        uint32_t result_local = get_or_create_local(inst.result->name, func);
        func.instructions.emplace_back(BytecodeOp::STORE_LOCAL, result_local);
    }
}

void BytecodeGenerator::generate_comparison(const ir::Instruction& inst, BytecodeFunction& func) {
    BytecodeOp op;
    switch (inst.opcode) {
        case ir::Opcode::Eq: op = BytecodeOp::EQ; break;
        case ir::Opcode::Ne: op = BytecodeOp::NE; break;
        case ir::Opcode::Lt: op = BytecodeOp::LT; break;
        case ir::Opcode::Le: op = BytecodeOp::LE; break;
        case ir::Opcode::Gt: op = BytecodeOp::GT; break;
        case ir::Opcode::Ge: op = BytecodeOp::GE; break;
        default: return;
    }
    
    for (const auto& operand : inst.operands) {
        uint32_t local = get_or_create_local(operand->name, func);
        func.instructions.emplace_back(BytecodeOp::LOAD_LOCAL, local);
    }
    
    func.instructions.emplace_back(op);
    
    if (inst.result) {
        uint32_t result_local = get_or_create_local(inst.result->name, func);
        func.instructions.emplace_back(BytecodeOp::STORE_LOCAL, result_local);
    }
}

void BytecodeGenerator::generate_memory(const ir::Instruction& inst, BytecodeFunction& func) {
    switch (inst.opcode) {
        case ir::Opcode::Alloca: {
            // Allocate a local slot — just reserve the name
            if (inst.result) {
                get_or_create_local(inst.result->name, func);
            }
            break;
        }
        case ir::Opcode::Load: {
            // Load from a local variable onto the stack
            if (!inst.operands.empty()) {
                uint32_t addr = get_or_create_local(inst.operands[0]->name, func);
                func.instructions.emplace_back(BytecodeOp::LOAD_LOCAL, addr);
            }
            if (inst.result) {
                uint32_t result_local = get_or_create_local(inst.result->name, func);
                func.instructions.emplace_back(BytecodeOp::STORE_LOCAL, result_local);
            }
            break;
        }
        case ir::Opcode::Store: {
            // Store value into address: operands[0] = value, operands[1] = address
            if (inst.operands.size() >= 2) {
                uint32_t val_local = get_or_create_local(inst.operands[0]->name, func);
                uint32_t addr_local = get_or_create_local(inst.operands[1]->name, func);
                func.instructions.emplace_back(BytecodeOp::LOAD_LOCAL, val_local);
                func.instructions.emplace_back(BytecodeOp::STORE_LOCAL, addr_local);
            }
            break;
        }
        default:
            break;
    }
}

void BytecodeGenerator::generate_control_flow(const ir::Instruction& inst, BytecodeFunction& func) {
    switch (inst.opcode) {
        case ir::Opcode::Branch: {
            uint32_t jump_addr = static_cast<uint32_t>(func.instructions.size());
            func.instructions.emplace_back(BytecodeOp::JUMP, 0); // placeholder
            pending_jumps_.emplace_back(jump_addr, inst.target_label);
            break;
        }
        case ir::Opcode::CondBranch: {
            // Load condition
            if (!inst.operands.empty()) {
                uint32_t cond = get_or_create_local(inst.operands[0]->name, func);
                func.instructions.emplace_back(BytecodeOp::LOAD_LOCAL, cond);
            }
            // Jump if false to else_label
            uint32_t jif_addr = static_cast<uint32_t>(func.instructions.size());
            func.instructions.emplace_back(BytecodeOp::JUMP_IF_FALSE, 0);
            pending_jumps_.emplace_back(jif_addr, inst.else_label);
            // Fall through to target_label (or explicit jump)
            uint32_t jmp_addr = static_cast<uint32_t>(func.instructions.size());
            func.instructions.emplace_back(BytecodeOp::JUMP, 0);
            pending_jumps_.emplace_back(jmp_addr, inst.target_label);
            break;
        }
        case ir::Opcode::Call: {
            // Push arguments onto stack
            for (size_t i = 1; i < inst.operands.size(); ++i) {
                uint32_t arg = get_or_create_local(inst.operands[i]->name, func);
                func.instructions.emplace_back(BytecodeOp::LOAD_LOCAL, arg);
            }
            // Function index (first operand)
            uint32_t func_idx = 0;
            if (!inst.operands.empty()) {
                // Use the function name to find its index in the module
                func_idx = get_or_create_local(inst.operands[0]->name, func);
            }
            uint32_t arg_count = static_cast<uint32_t>(inst.operands.size() > 0 ? inst.operands.size() - 1 : 0);
            func.instructions.emplace_back(BytecodeOp::CALL, func_idx, arg_count);
            // Store result
            if (inst.result) {
                uint32_t result_local = get_or_create_local(inst.result->name, func);
                func.instructions.emplace_back(BytecodeOp::STORE_LOCAL, result_local);
            }
            break;
        }
        case ir::Opcode::Return: {
            if (!inst.operands.empty()) {
                uint32_t val = get_or_create_local(inst.operands[0]->name, func);
                func.instructions.emplace_back(BytecodeOp::LOAD_LOCAL, val);
            }
            func.instructions.emplace_back(BytecodeOp::RETURN);
            break;
        }
        default:
            break;
    }
}

void BytecodeGenerator::generate_constant(const ir::Instruction& inst, BytecodeFunction& func) {
    if (!current_module_) return;
    
    uint32_t const_idx = get_constant_index(inst);
    
    BytecodeOp op;
    switch (inst.opcode) {
        case ir::Opcode::ConstInt:    op = BytecodeOp::LOAD_CONST_INT; break;
        case ir::Opcode::ConstFloat:  op = BytecodeOp::LOAD_CONST_FLOAT; break;
        case ir::Opcode::ConstBool:   op = BytecodeOp::LOAD_CONST_BOOL; break;
        case ir::Opcode::ConstString: op = BytecodeOp::LOAD_CONST_STRING; break;
        case ir::Opcode::ConstNull:   op = BytecodeOp::LOAD_NULL; break;
        default: return;
    }
    
    func.instructions.emplace_back(op, const_idx);
    
    // Store result in local
    if (inst.result) {
        uint32_t result_local = get_or_create_local(inst.result->name, func);
        func.instructions.emplace_back(BytecodeOp::STORE_LOCAL, result_local);
    }
}

uint32_t BytecodeGenerator::get_or_create_local(const std::string& name, BytecodeFunction& func) {
    auto it = value_to_local_.find(name);
    if (it != value_to_local_.end()) return it->second;
    
    uint32_t idx = static_cast<uint32_t>(value_to_local_.size());
    value_to_local_[name] = idx;
    if (debug_info_) {
        func.local_names.push_back(name);
    }
    return idx;
}

uint32_t BytecodeGenerator::get_constant_index(const ir::Instruction& inst) {
    if (!current_module_) return 0;
    
    return std::visit([this](const auto& val) -> uint32_t {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, int64_t>) {
            return current_module_->add_constant(val);
        } else if constexpr (std::is_same_v<T, double>) {
            return current_module_->add_constant(val);
        } else if constexpr (std::is_same_v<T, bool>) {
            return current_module_->add_constant(val);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return current_module_->add_constant(val);
        }
        return 0;
    }, inst.constant_value);
}

void BytecodeGenerator::resolve_jumps(BytecodeFunction& func) {
    for (const auto& [addr, label] : pending_jumps_) {
        auto it = label_to_address_.find(label);
        if (it != label_to_address_.end()) {
            // Patch the operand of the jump instruction
            if (addr < func.instructions.size() && !func.instructions[addr].operands.empty()) {
                func.instructions[addr].operands[0] = it->second;
            }
        }
    }
}

// ============================================================================
// BytecodeOptimizer
// ============================================================================

void BytecodeOptimizer::optimize(BytecodeModule& module) {
    for (auto& func : module.functions) {
        optimize_function(func);
    }
}

void BytecodeOptimizer::optimize_function(BytecodeFunction& function) {
    eliminate_dead_code(function);
    fold_constants(function);
    optimize_jumps(function);
    eliminate_redundant_loads(function);
}

void BytecodeOptimizer::eliminate_dead_code(BytecodeFunction& function) {
    // Remove instructions whose results are never used.
    // A STORE_LOCAL is dead if the local is never subsequently LOAD_LOCAL'd.
    // Simple single-pass: collect used locals, then remove dead stores.
    
    std::vector<bool> keep(function.instructions.size(), true);
    
    for (size_t i = 0; i < function.instructions.size(); ++i) {
        if (is_dead_instruction(function.instructions[i], i, function)) {
            keep[i] = false;
        }
    }
    
    std::vector<BytecodeInstruction> filtered;
    filtered.reserve(function.instructions.size());
    for (size_t i = 0; i < function.instructions.size(); ++i) {
        if (keep[i]) {
            filtered.push_back(std::move(function.instructions[i]));
        }
    }
    function.instructions = std::move(filtered);
}

bool BytecodeOptimizer::is_dead_instruction(const BytecodeInstruction& inst, size_t index,
                                            const BytecodeFunction& function) {
    // NOP is always dead
    if (inst.opcode == BytecodeOp::NOP) return true;
    
    // A STORE_LOCAL is dead if the local is never loaded after this point
    if (inst.opcode == BytecodeOp::STORE_LOCAL && !inst.operands.empty()) {
        uint32_t local_idx = inst.operands[0];
        for (size_t j = index + 1; j < function.instructions.size(); ++j) {
            const auto& later = function.instructions[j];
            if (later.opcode == BytecodeOp::LOAD_LOCAL &&
                !later.operands.empty() && later.operands[0] == local_idx) {
                return false; // It's used later
            }
        }
        // Check if it's a parameter (parameters are implicitly used)
        if (local_idx < function.parameter_count) return false;
        return true;
    }
    
    return false;
}

void BytecodeOptimizer::fold_constants(BytecodeFunction& function) {
    // Look for patterns: LOAD_CONST + LOAD_CONST + binary_op → LOAD_CONST(result)
    // This is a peephole optimization on instruction triples.
    
    if (function.instructions.size() < 3) return;
    
    // We can't easily fold without access to the constant pool values,
    // so this is a structural pass that identifies foldable patterns.
    // The actual folding would need the BytecodeModule's constant pool.
    // For now, mark patterns that could be folded.
    
    // Simple pattern: consecutive LOAD_CONST_INT, LOAD_CONST_INT, ADD/SUB/MUL/DIV
    // We leave this as a no-op since we'd need the module's constant pool to resolve values.
}

bool BytecodeOptimizer::can_fold_constants(const BytecodeInstruction& inst1,
                                           const BytecodeInstruction& inst2) {
    bool is_const1 = (inst1.opcode == BytecodeOp::LOAD_CONST_INT ||
                      inst1.opcode == BytecodeOp::LOAD_CONST_FLOAT ||
                      inst1.opcode == BytecodeOp::LOAD_CONST_BOOL);
    bool is_const2 = (inst2.opcode == BytecodeOp::LOAD_CONST_INT ||
                      inst2.opcode == BytecodeOp::LOAD_CONST_FLOAT ||
                      inst2.opcode == BytecodeOp::LOAD_CONST_BOOL);
    return is_const1 && is_const2;
}

std::optional<BytecodeInstruction> BytecodeOptimizer::fold_binary_op(
    BytecodeOp op,
    const std::variant<int64_t, double, bool, std::string>& left,
    const std::variant<int64_t, double, bool, std::string>& right) {
    
    auto* l = std::get_if<int64_t>(&left);
    auto* r = std::get_if<int64_t>(&right);
    if (!l || !r) return std::nullopt;
    
    int64_t result = 0;
    switch (op) {
        case BytecodeOp::ADD: result = *l + *r; break;
        case BytecodeOp::SUB: result = *l - *r; break;
        case BytecodeOp::MUL: result = *l * *r; break;
        case BytecodeOp::DIV:
            if (*r == 0) return std::nullopt;
            result = *l / *r;
            break;
        case BytecodeOp::MOD:
            if (*r == 0) return std::nullopt;
            result = *l % *r;
            break;
        default:
            return std::nullopt;
    }
    
    // Return a LOAD_CONST_INT with the folded result
    // The caller would need to add this to the constant pool
    BytecodeInstruction folded(BytecodeOp::LOAD_CONST_INT);
    folded.operands.push_back(static_cast<uint32_t>(result)); // simplified
    return folded;
}

void BytecodeOptimizer::optimize_jumps(BytecodeFunction& function) {
    // Collapse jump chains: if JUMP targets another JUMP, point to final target
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto& inst : function.instructions) {
            if (inst.opcode == BytecodeOp::JUMP && !inst.operands.empty()) {
                uint32_t target = inst.operands[0];
                if (target < function.instructions.size()) {
                    const auto& target_inst = function.instructions[target];
                    if (target_inst.opcode == BytecodeOp::JUMP && !target_inst.operands.empty()) {
                        inst.operands[0] = target_inst.operands[0];
                        changed = true;
                    }
                }
            }
        }
    }
}

void BytecodeOptimizer::eliminate_redundant_loads(BytecodeFunction& function) {
    // Remove consecutive identical LOAD_LOCAL instructions
    if (function.instructions.size() < 2) return;
    
    std::vector<BytecodeInstruction> optimized;
    optimized.reserve(function.instructions.size());
    optimized.push_back(function.instructions[0]);
    
    for (size_t i = 1; i < function.instructions.size(); ++i) {
        const auto& prev = optimized.back();
        const auto& curr = function.instructions[i];
        
        // Skip if this is an identical consecutive LOAD_LOCAL
        if (curr.opcode == BytecodeOp::LOAD_LOCAL &&
            prev.opcode == BytecodeOp::LOAD_LOCAL &&
            curr.operands == prev.operands) {
            // Replace with DUP instead
            optimized.emplace_back(BytecodeOp::DUP);
            continue;
        }
        
        optimized.push_back(curr);
    }
    
    function.instructions = std::move(optimized);
}

// ============================================================================
// BytecodeInterpreter
// ============================================================================

int BytecodeInterpreter::execute(const BytecodeModule& module, const std::vector<std::string>& args) {
    // Find main function
    const BytecodeFunction* main_func = nullptr;
    for (const auto& func : module.functions) {
        if (func.name == "main") {
            main_func = &func;
            break;
        }
    }
    
    if (!main_func) {
        throw std::runtime_error("No main function found");
    }
    
    // Convert args to variant values
    std::vector<std::variant<int64_t, double, bool, std::string>> arg_values;
    for (const auto& a : args) {
        arg_values.push_back(a);
    }
    
    auto result = execute_function(*main_func, arg_values);
    
    // Return exit code (0 for success, or integer result)
    if (auto* val = std::get_if<int64_t>(&result)) {
        return static_cast<int>(*val);
    }
    return 0;
}

std::variant<int64_t, double, bool, std::string>
BytecodeInterpreter::execute_function(
    const BytecodeFunction& function,
    const std::vector<std::variant<int64_t, double, bool, std::string>>& args) {
    
    // Set up locals from args
    locals_.clear();
    locals_.resize(function.local_count, int64_t{0});
    for (size_t i = 0; i < args.size() && i < function.parameter_count; ++i) {
        locals_[i] = args[i];
    }
    
    // Execute instructions
    size_t pc = 0;
    while (pc < function.instructions.size()) {
        const auto& inst = function.instructions[pc];
        
        if (debug_mode_) {
            print_instruction(inst, pc);
        }
        
        if (inst.opcode == BytecodeOp::RETURN) {
            if (!stack_.empty()) {
                return pop();
            }
            return int64_t{0};
        }
        
        if (inst.opcode == BytecodeOp::JUMP && !inst.operands.empty()) {
            pc = inst.operands[0];
            continue;
        }
        
        if (inst.opcode == BytecodeOp::JUMP_IF_FALSE && !inst.operands.empty()) {
            auto cond = pop();
            bool is_false = false;
            if (auto* b = std::get_if<bool>(&cond)) is_false = !*b;
            else if (auto* i = std::get_if<int64_t>(&cond)) is_false = (*i == 0);
            if (is_false) {
                pc = inst.operands[0];
                continue;
            }
            ++pc;
            continue;
        }
        
        if (inst.opcode == BytecodeOp::JUMP_IF_TRUE && !inst.operands.empty()) {
            auto cond = pop();
            bool is_true = false;
            if (auto* b = std::get_if<bool>(&cond)) is_true = *b;
            else if (auto* i = std::get_if<int64_t>(&cond)) is_true = (*i != 0);
            if (is_true) {
                pc = inst.operands[0];
                continue;
            }
            ++pc;
            continue;
        }
        
        execute_instruction(inst, BytecodeModule(""));
        ++pc;
    }
    
    if (!stack_.empty()) return pop();
    return int64_t{0};
}

void BytecodeInterpreter::execute_instruction(const BytecodeInstruction& inst,
                                              const BytecodeModule& module) {
    switch (inst.opcode) {
        case BytecodeOp::LOAD_CONST_INT:
        case BytecodeOp::LOAD_CONST_FLOAT:
        case BytecodeOp::LOAD_CONST_BOOL:
        case BytecodeOp::LOAD_CONST_STRING: {
            if (!inst.operands.empty() && inst.operands[0] < module.constants.size()) {
                push(module.constants[inst.operands[0]]);
            } else {
                push(int64_t{0});
            }
            break;
        }
        case BytecodeOp::LOAD_NULL:
            push(int64_t{0});
            break;
        case BytecodeOp::LOAD_LOCAL: {
            if (!inst.operands.empty() && inst.operands[0] < locals_.size()) {
                push(locals_[inst.operands[0]]);
            } else {
                throw std::runtime_error("LOAD_LOCAL: invalid local index");
            }
            break;
        }
        case BytecodeOp::STORE_LOCAL: {
            if (!inst.operands.empty() && inst.operands[0] < locals_.size()) {
                locals_[inst.operands[0]] = pop();
            } else {
                throw std::runtime_error("STORE_LOCAL: invalid local index");
            }
            break;
        }
        case BytecodeOp::LOAD_GLOBAL: {
            if (!inst.operands.empty() && inst.operands[0] < globals_.size()) {
                push(globals_[inst.operands[0]]);
            } else {
                push(int64_t{0});
            }
            break;
        }
        case BytecodeOp::STORE_GLOBAL: {
            if (!inst.operands.empty()) {
                auto val = pop();
                if (inst.operands[0] >= globals_.size()) {
                    globals_.resize(inst.operands[0] + 1, int64_t{0});
                }
                globals_[inst.operands[0]] = val;
            }
            break;
        }
        case BytecodeOp::ADD: { auto r = pop(); auto l = pop(); push(add_values(l, r)); break; }
        case BytecodeOp::SUB: { auto r = pop(); auto l = pop(); push(sub_values(l, r)); break; }
        case BytecodeOp::MUL: { auto r = pop(); auto l = pop(); push(mul_values(l, r)); break; }
        case BytecodeOp::DIV: { auto r = pop(); auto l = pop(); push(div_values(l, r)); break; }
        case BytecodeOp::MOD: {
            auto r = pop(); auto l = pop();
            auto* li = std::get_if<int64_t>(&l);
            auto* ri = std::get_if<int64_t>(&r);
            if (li && ri) {
                if (*ri == 0) throw std::runtime_error("Division by zero");
                push(*li % *ri);
            } else {
                throw std::runtime_error("MOD: unsupported operand types");
            }
            break;
        }
        case BytecodeOp::NEG: {
            auto v = pop();
            if (auto* i = std::get_if<int64_t>(&v)) push(-*i);
            else if (auto* d = std::get_if<double>(&v)) push(-*d);
            else throw std::runtime_error("NEG: unsupported operand type");
            break;
        }
        case BytecodeOp::AND: {
            auto r = pop(); auto l = pop();
            auto* lb = std::get_if<bool>(&l);
            auto* rb = std::get_if<bool>(&r);
            if (lb && rb) push(*lb && *rb);
            else throw std::runtime_error("AND: unsupported operand types");
            break;
        }
        case BytecodeOp::OR: {
            auto r = pop(); auto l = pop();
            auto* lb = std::get_if<bool>(&l);
            auto* rb = std::get_if<bool>(&r);
            if (lb && rb) push(*lb || *rb);
            else throw std::runtime_error("OR: unsupported operand types");
            break;
        }
        case BytecodeOp::NOT: {
            auto v = pop();
            if (auto* b = std::get_if<bool>(&v)) push(!*b);
            else throw std::runtime_error("NOT: unsupported operand type");
            break;
        }
        case BytecodeOp::EQ:
        case BytecodeOp::NE:
        case BytecodeOp::LT:
        case BytecodeOp::LE:
        case BytecodeOp::GT:
        case BytecodeOp::GE: {
            auto r = pop(); auto l = pop();
            push(compare_values(l, r, inst.opcode));
            break;
        }
        case BytecodeOp::POP:
            if (!stack_.empty()) pop();
            break;
        case BytecodeOp::DUP:
            if (!stack_.empty()) push(peek());
            break;
        case BytecodeOp::SWAP: {
            if (stack_.size() >= 2) {
                auto a = pop(); auto b = pop();
                push(a); push(b);
            }
            break;
        }
        case BytecodeOp::NOP:
            break;
        default:
            throw std::runtime_error("Unknown opcode: " +
                std::to_string(static_cast<int>(inst.opcode)));
    }
}

void BytecodeInterpreter::push(const std::variant<int64_t, double, bool, std::string>& value) {
    stack_.push_back(value);
}

std::variant<int64_t, double, bool, std::string> BytecodeInterpreter::pop() {
    if (stack_.empty()) {
        throw std::runtime_error("Stack underflow");
    }
    auto val = std::move(stack_.back());
    stack_.pop_back();
    return val;
}

std::variant<int64_t, double, bool, std::string> BytecodeInterpreter::peek() {
    if (stack_.empty()) {
        throw std::runtime_error("Stack underflow on peek");
    }
    return stack_.back();
}

std::variant<int64_t, double, bool, std::string>
BytecodeInterpreter::add_values(
    const std::variant<int64_t, double, bool, std::string>& left,
    const std::variant<int64_t, double, bool, std::string>& right) {
    if (auto* l = std::get_if<int64_t>(&left)) {
        if (auto* r = std::get_if<int64_t>(&right)) return *l + *r;
        if (auto* r = std::get_if<double>(&right)) return static_cast<double>(*l) + *r;
    }
    if (auto* l = std::get_if<double>(&left)) {
        if (auto* r = std::get_if<double>(&right)) return *l + *r;
        if (auto* r = std::get_if<int64_t>(&right)) return *l + static_cast<double>(*r);
    }
    if (auto* l = std::get_if<std::string>(&left)) {
        if (auto* r = std::get_if<std::string>(&right)) return *l + *r;
    }
    throw std::runtime_error("ADD: incompatible types");
}

std::variant<int64_t, double, bool, std::string>
BytecodeInterpreter::sub_values(
    const std::variant<int64_t, double, bool, std::string>& left,
    const std::variant<int64_t, double, bool, std::string>& right) {
    if (auto* l = std::get_if<int64_t>(&left)) {
        if (auto* r = std::get_if<int64_t>(&right)) return *l - *r;
        if (auto* r = std::get_if<double>(&right)) return static_cast<double>(*l) - *r;
    }
    if (auto* l = std::get_if<double>(&left)) {
        if (auto* r = std::get_if<double>(&right)) return *l - *r;
        if (auto* r = std::get_if<int64_t>(&right)) return *l - static_cast<double>(*r);
    }
    throw std::runtime_error("SUB: incompatible types");
}

std::variant<int64_t, double, bool, std::string>
BytecodeInterpreter::mul_values(
    const std::variant<int64_t, double, bool, std::string>& left,
    const std::variant<int64_t, double, bool, std::string>& right) {
    if (auto* l = std::get_if<int64_t>(&left)) {
        if (auto* r = std::get_if<int64_t>(&right)) return *l * *r;
        if (auto* r = std::get_if<double>(&right)) return static_cast<double>(*l) * *r;
    }
    if (auto* l = std::get_if<double>(&left)) {
        if (auto* r = std::get_if<double>(&right)) return *l * *r;
        if (auto* r = std::get_if<int64_t>(&right)) return *l * static_cast<double>(*r);
    }
    throw std::runtime_error("MUL: incompatible types");
}

std::variant<int64_t, double, bool, std::string>
BytecodeInterpreter::div_values(
    const std::variant<int64_t, double, bool, std::string>& left,
    const std::variant<int64_t, double, bool, std::string>& right) {
    if (auto* l = std::get_if<int64_t>(&left)) {
        if (auto* r = std::get_if<int64_t>(&right)) {
            if (*r == 0) throw std::runtime_error("Division by zero");
            return *l / *r;
        }
        if (auto* r = std::get_if<double>(&right)) {
            if (*r == 0.0) throw std::runtime_error("Division by zero");
            return static_cast<double>(*l) / *r;
        }
    }
    if (auto* l = std::get_if<double>(&left)) {
        if (auto* r = std::get_if<double>(&right)) {
            if (*r == 0.0) throw std::runtime_error("Division by zero");
            return *l / *r;
        }
        if (auto* r = std::get_if<int64_t>(&right)) {
            if (*r == 0) throw std::runtime_error("Division by zero");
            return *l / static_cast<double>(*r);
        }
    }
    throw std::runtime_error("DIV: incompatible types");
}

bool BytecodeInterpreter::compare_values(
    const std::variant<int64_t, double, bool, std::string>& left,
    const std::variant<int64_t, double, bool, std::string>& right,
    BytecodeOp op) {
    
    // Integer comparison
    if (auto* l = std::get_if<int64_t>(&left)) {
        if (auto* r = std::get_if<int64_t>(&right)) {
            switch (op) {
                case BytecodeOp::EQ: return *l == *r;
                case BytecodeOp::NE: return *l != *r;
                case BytecodeOp::LT: return *l < *r;
                case BytecodeOp::LE: return *l <= *r;
                case BytecodeOp::GT: return *l > *r;
                case BytecodeOp::GE: return *l >= *r;
                default: break;
            }
        }
    }
    // String comparison
    if (auto* l = std::get_if<std::string>(&left)) {
        if (auto* r = std::get_if<std::string>(&right)) {
            switch (op) {
                case BytecodeOp::EQ: return *l == *r;
                case BytecodeOp::NE: return *l != *r;
                case BytecodeOp::LT: return *l < *r;
                case BytecodeOp::LE: return *l <= *r;
                case BytecodeOp::GT: return *l > *r;
                case BytecodeOp::GE: return *l >= *r;
                default: break;
            }
        }
    }
    // Bool equality
    if (auto* l = std::get_if<bool>(&left)) {
        if (auto* r = std::get_if<bool>(&right)) {
            if (op == BytecodeOp::EQ) return *l == *r;
            if (op == BytecodeOp::NE) return *l != *r;
        }
    }
    throw std::runtime_error("Comparison: incompatible types");
}

void BytecodeInterpreter::print_stack() const {
    std::cerr << "  stack [" << stack_.size() << "]:";
    for (const auto& v : stack_) {
        std::visit([](const auto& val) {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, int64_t>) std::cerr << " " << val;
            else if constexpr (std::is_same_v<T, double>) std::cerr << " " << val;
            else if constexpr (std::is_same_v<T, bool>) std::cerr << " " << (val ? "true" : "false");
            else if constexpr (std::is_same_v<T, std::string>) std::cerr << " \"" << val << "\"";
        }, v);
    }
    std::cerr << "\n";
}

void BytecodeInterpreter::print_instruction(const BytecodeInstruction& inst, size_t pc) const {
    std::cerr << "  [" << pc << "] " << inst.to_string() << "\n";
    print_stack();
}

} // namespace meld::compiler
