#include "meld/compiler/ir.hpp"
#include <sstream>
#include <algorithm>

namespace meld::compiler::ir {

// Instruction helper constructors
std::shared_ptr<Instruction> Instruction::create_binary_op(
    Opcode op,
    std::shared_ptr<Value> result,
    std::shared_ptr<Value> left,
    std::shared_ptr<Value> right) {
    auto inst = std::make_shared<Instruction>(op);
    inst->result = result;
    inst->operands = {left, right};
    return inst;
}

std::shared_ptr<Instruction> Instruction::create_unary_op(
    Opcode op,
    std::shared_ptr<Value> result,
    std::shared_ptr<Value> operand) {
    auto inst = std::make_shared<Instruction>(op);
    inst->result = result;
    inst->operands = {operand};
    return inst;
}

std::shared_ptr<Instruction> Instruction::create_const_int(
    std::shared_ptr<Value> result,
    int64_t value) {
    auto inst = std::make_shared<Instruction>(Opcode::ConstInt);
    inst->result = result;
    inst->constant_value = value;
    
    // Also store the constant in the result value
    if (result) {
        result->constant_data = value;
    }
    
    return inst;
}

std::shared_ptr<Instruction> Instruction::create_const_bool(
    std::shared_ptr<Value> result,
    bool value) {
    auto inst = std::make_shared<Instruction>(Opcode::ConstBool);
    inst->result = result;
    inst->constant_value = value;
    
    // Also store the constant in the result value
    if (result) {
        result->constant_data = value;
    }
    
    return inst;
}

std::shared_ptr<Instruction> Instruction::create_const_string(
    std::shared_ptr<Value> result,
    std::string value) {
    auto inst = std::make_shared<Instruction>(Opcode::ConstString);
    inst->result = result;
    inst->constant_value = std::move(value);
    
    // Also store the constant in the result value
    if (result) {
        result->constant_data = std::get<std::string>(inst->constant_value);
    }
    
    return inst;
}

std::shared_ptr<Instruction> Instruction::create_load(
    std::shared_ptr<Value> result,
    std::shared_ptr<Value> address) {
    auto inst = std::make_shared<Instruction>(Opcode::Load);
    inst->result = result;
    inst->operands = {address};
    return inst;
}

std::shared_ptr<Instruction> Instruction::create_store(
    std::shared_ptr<Value> value,
    std::shared_ptr<Value> address) {
    auto inst = std::make_shared<Instruction>(Opcode::Store);
    inst->operands = {value, address};
    return inst;
}

std::shared_ptr<Instruction> Instruction::create_call(
    std::shared_ptr<Value> result,
    std::shared_ptr<Value> function,
    std::vector<std::shared_ptr<Value>> args) {
    auto inst = std::make_shared<Instruction>(Opcode::Call);
    inst->result = result;
    inst->operands.push_back(function);
    inst->operands.insert(inst->operands.end(), args.begin(), args.end());
    return inst;
}

std::shared_ptr<Instruction> Instruction::create_return(
    std::shared_ptr<Value> value) {
    auto inst = std::make_shared<Instruction>(Opcode::Return);
    if (value) {
        inst->operands = {value};
    }
    return inst;
}

std::shared_ptr<Instruction> Instruction::create_branch(
    std::string target) {
    auto inst = std::make_shared<Instruction>(Opcode::Branch);
    inst->target_label = std::move(target);
    return inst;
}

std::shared_ptr<Instruction> Instruction::create_cond_branch(
    std::shared_ptr<Value> condition,
    std::string true_target,
    std::string false_target) {
    auto inst = std::make_shared<Instruction>(Opcode::CondBranch);
    inst->operands = {condition};
    inst->target_label = std::move(true_target);
    inst->else_label = std::move(false_target);
    return inst;
}

std::shared_ptr<Instruction> Instruction::create_intrinsic_retain(
    std::shared_ptr<Value> value) {
    auto inst = std::make_shared<Instruction>(Opcode::IntrinsicRetain);
    inst->operands = {value};
    return inst;
}

std::shared_ptr<Instruction> Instruction::create_intrinsic_release(
    std::shared_ptr<Value> value) {
    auto inst = std::make_shared<Instruction>(Opcode::IntrinsicRelease);
    inst->operands = {value};
    return inst;
}

bool Instruction::has_side_effects() const {
    switch (opcode) {
        case Opcode::Store:
        case Opcode::SetField:
        case Opcode::Call:
        case Opcode::Return:
        case Opcode::Branch:
        case Opcode::CondBranch:
        case Opcode::IntrinsicRetain:
        case Opcode::IntrinsicRelease:
            return true;
        default:
            return false;
    }
}

std::string Instruction::to_string() const {
    std::ostringstream oss;
    
    // Result
    if (result) {
        oss << result->name << " = ";
    }
    
    // Opcode
    switch (opcode) {
        case Opcode::Add: oss << "add"; break;
        case Opcode::Sub: oss << "sub"; break;
        case Opcode::Mul: oss << "mul"; break;
        case Opcode::Div: oss << "div"; break;
        case Opcode::Mod: oss << "mod"; break;
        case Opcode::Neg: oss << "neg"; break;
        case Opcode::And: oss << "and"; break;
        case Opcode::Or: oss << "or"; break;
        case Opcode::Not: oss << "not"; break;
        case Opcode::Eq: oss << "eq"; break;
        case Opcode::Ne: oss << "ne"; break;
        case Opcode::Lt: oss << "lt"; break;
        case Opcode::Le: oss << "le"; break;
        case Opcode::Gt: oss << "gt"; break;
        case Opcode::Ge: oss << "ge"; break;
        case Opcode::Alloca: oss << "alloca"; break;
        case Opcode::Load: oss << "load"; break;
        case Opcode::Store: oss << "store"; break;
        case Opcode::GetField: oss << "getfield"; break;
        case Opcode::SetField: oss << "setfield"; break;
        case Opcode::Branch: oss << "br"; break;
        case Opcode::CondBranch: oss << "br"; break;
        case Opcode::Return: oss << "ret"; break;
        case Opcode::Call: oss << "call"; break;
        case Opcode::ConstInt: oss << "const.int"; break;
        case Opcode::ConstFloat: oss << "const.float"; break;
        case Opcode::ConstBool: oss << "const.bool"; break;
        case Opcode::ConstString: oss << "const.string"; break;
        case Opcode::ConstNull: oss << "const.null"; break;
        case Opcode::Cast: oss << "cast"; break;
        case Opcode::TypeOf: oss << "typeof"; break;
        case Opcode::Phi: oss << "phi"; break;
        case Opcode::Nop: oss << "nop"; break;
        case Opcode::IntrinsicRetain: oss << "intrinsic_retain"; break;
        case Opcode::IntrinsicRelease: oss << "intrinsic_release"; break;
    }
    
    // Operands
    if (!operands.empty()) {
        oss << " ";
        for (size_t i = 0; i < operands.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << operands[i]->name;
        }
    }
    
    // Constants
    if (opcode == Opcode::ConstInt) {
        oss << " " << std::get<int64_t>(constant_value);
    } else if (opcode == Opcode::ConstBool) {
        oss << " " << (std::get<bool>(constant_value) ? "true" : "false");
    } else if (opcode == Opcode::ConstString) {
        oss << " \"" << std::get<std::string>(constant_value) << "\"";
    }
    
    // Branch targets
    if (opcode == Opcode::Branch) {
        oss << " " << target_label;
    } else if (opcode == Opcode::CondBranch) {
        oss << " " << target_label << ", " << else_label;
    }
    
    return oss.str();
}

// BasicBlock implementation
void BasicBlock::add_instruction(std::shared_ptr<Instruction> inst) {
    instructions.push_back(std::move(inst));
}

bool BasicBlock::is_terminated() const {
    if (instructions.empty()) return false;
    auto last_op = instructions.back()->opcode;
    return last_op == Opcode::Return || 
           last_op == Opcode::Branch || 
           last_op == Opcode::CondBranch;
}

std::string BasicBlock::to_string() const {
    std::ostringstream oss;
    oss << label << ":" << std::endl;
    for (const auto& inst : instructions) {
        oss << "  " << inst->to_string() << std::endl;
    }
    return oss.str();
}

// Function implementation
std::shared_ptr<BasicBlock> Function::create_block(const std::string& label) {
    auto block = std::make_shared<BasicBlock>(label);
    basic_blocks.push_back(block);
    return block;
}

std::shared_ptr<BasicBlock> Function::get_block(const std::string& label) {
    auto it = std::find_if(basic_blocks.begin(), basic_blocks.end(),
        [&label](const auto& block) { return block->label == label; });
    return it != basic_blocks.end() ? *it : nullptr;
}

std::shared_ptr<BasicBlock> Function::entry_block() {
    return basic_blocks.empty() ? nullptr : basic_blocks.front();
}

std::string Function::to_string() const {
    std::ostringstream oss;
    oss << "function " << name << "(";
    for (size_t i = 0; i < parameters.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << parameters[i]->name;
    }
    oss << ") {" << std::endl;
    
    for (const auto& block : basic_blocks) {
        oss << block->to_string();
    }
    
    oss << "}" << std::endl;
    return oss.str();
}

// Module implementation
std::shared_ptr<Function> Module::create_function(const std::string& name) {
    auto func = std::make_shared<Function>(name);
    functions.push_back(func);
    return func;
}

std::shared_ptr<Function> Module::get_function(const std::string& name) {
    auto it = std::find_if(functions.begin(), functions.end(),
        [&name](const auto& func) { return func->name == name; });
    return it != functions.end() ? *it : nullptr;
}

void Module::add_global(std::shared_ptr<Value> global) {
    globals.push_back(std::move(global));
}

void Module::register_type(const std::string& name, std::shared_ptr<meta::MetaType> type) {
    types[name] = std::move(type);
}

std::string Module::to_string() const {
    std::ostringstream oss;
    oss << "module " << name << std::endl << std::endl;
    
    // Globals
    if (!globals.empty()) {
        oss << "; Globals" << std::endl;
        for (const auto& global : globals) {
            oss << "global " << global->name << std::endl;
        }
        oss << std::endl;
    }
    
    // Functions
    for (const auto& func : functions) {
        oss << func->to_string() << std::endl;
    }
    
    return oss.str();
}

// IRBuilder implementation
IRBuilder::IRBuilder(std::shared_ptr<Module> mod)
    : module_(std::move(mod)) {}

void IRBuilder::set_current_function(std::shared_ptr<Function> func) {
    current_function_ = std::move(func);
}

void IRBuilder::set_insert_point(std::shared_ptr<BasicBlock> block) {
    insert_point_ = std::move(block);
}

std::shared_ptr<Value> IRBuilder::create_temp(ValueType type, std::shared_ptr<meta::MetaType> meta_type) {
    return std::make_shared<Value>("%t" + std::to_string(temp_counter_++), type, meta_type);
}

std::shared_ptr<Value> IRBuilder::create_named_value(const std::string& name, ValueType type, std::shared_ptr<meta::MetaType> meta_type) {
    return std::make_shared<Value>(name, type, meta_type);
}

void IRBuilder::insert_instruction(std::shared_ptr<Instruction> inst) {
    if (insert_point_) {
        insert_point_->add_instruction(std::move(inst));
    }
}

std::shared_ptr<Value> IRBuilder::build_add(std::shared_ptr<Value> left, std::shared_ptr<Value> right) {
    auto result = create_temp(ValueType::Int);
    insert_instruction(Instruction::create_binary_op(Opcode::Add, result, left, right));
    return result;
}

std::shared_ptr<Value> IRBuilder::build_sub(std::shared_ptr<Value> left, std::shared_ptr<Value> right) {
    auto result = create_temp(ValueType::Int);
    insert_instruction(Instruction::create_binary_op(Opcode::Sub, result, left, right));
    return result;
}

std::shared_ptr<Value> IRBuilder::build_mul(std::shared_ptr<Value> left, std::shared_ptr<Value> right) {
    auto result = create_temp(ValueType::Int);
    insert_instruction(Instruction::create_binary_op(Opcode::Mul, result, left, right));
    return result;
}

std::shared_ptr<Value> IRBuilder::build_div(std::shared_ptr<Value> left, std::shared_ptr<Value> right) {
    auto result = create_temp(ValueType::Int);
    insert_instruction(Instruction::create_binary_op(Opcode::Div, result, left, right));
    return result;
}

std::shared_ptr<Value> IRBuilder::build_neg(std::shared_ptr<Value> operand) {
    auto result = create_temp(ValueType::Int);
    insert_instruction(Instruction::create_unary_op(Opcode::Neg, result, operand));
    return result;
}

std::shared_ptr<Value> IRBuilder::build_and(std::shared_ptr<Value> left, std::shared_ptr<Value> right) {
    auto result = create_temp(ValueType::Bool);
    insert_instruction(Instruction::create_binary_op(Opcode::And, result, left, right));
    return result;
}

std::shared_ptr<Value> IRBuilder::build_or(std::shared_ptr<Value> left, std::shared_ptr<Value> right) {
    auto result = create_temp(ValueType::Bool);
    insert_instruction(Instruction::create_binary_op(Opcode::Or, result, left, right));
    return result;
}

std::shared_ptr<Value> IRBuilder::build_not(std::shared_ptr<Value> operand) {
    auto result = create_temp(ValueType::Bool);
    insert_instruction(Instruction::create_unary_op(Opcode::Not, result, operand));
    return result;
}

std::shared_ptr<Value> IRBuilder::build_eq(std::shared_ptr<Value> left, std::shared_ptr<Value> right) {
    auto result = create_temp(ValueType::Bool);
    insert_instruction(Instruction::create_binary_op(Opcode::Eq, result, left, right));
    return result;
}

std::shared_ptr<Value> IRBuilder::build_ne(std::shared_ptr<Value> left, std::shared_ptr<Value> right) {
    auto result = create_temp(ValueType::Bool);
    insert_instruction(Instruction::create_binary_op(Opcode::Ne, result, left, right));
    return result;
}

std::shared_ptr<Value> IRBuilder::build_lt(std::shared_ptr<Value> left, std::shared_ptr<Value> right) {
    auto result = create_temp(ValueType::Bool);
    insert_instruction(Instruction::create_binary_op(Opcode::Lt, result, left, right));
    return result;
}

std::shared_ptr<Value> IRBuilder::build_le(std::shared_ptr<Value> left, std::shared_ptr<Value> right) {
    auto result = create_temp(ValueType::Bool);
    insert_instruction(Instruction::create_binary_op(Opcode::Le, result, left, right));
    return result;
}

std::shared_ptr<Value> IRBuilder::build_gt(std::shared_ptr<Value> left, std::shared_ptr<Value> right) {
    auto result = create_temp(ValueType::Bool);
    insert_instruction(Instruction::create_binary_op(Opcode::Gt, result, left, right));
    return result;
}

std::shared_ptr<Value> IRBuilder::build_ge(std::shared_ptr<Value> left, std::shared_ptr<Value> right) {
    auto result = create_temp(ValueType::Bool);
    insert_instruction(Instruction::create_binary_op(Opcode::Ge, result, left, right));
    return result;
}

std::shared_ptr<Value> IRBuilder::build_const_int(int64_t value) {
    auto result = create_temp(ValueType::Int);
    insert_instruction(Instruction::create_const_int(result, value));
    return result;
}

std::shared_ptr<Value> IRBuilder::build_const_bool(bool value) {
    auto result = create_temp(ValueType::Bool);
    insert_instruction(Instruction::create_const_bool(result, value));
    return result;
}

std::shared_ptr<Value> IRBuilder::build_const_string(const std::string& value) {
    auto result = create_temp(ValueType::String);
    insert_instruction(Instruction::create_const_string(result, value));
    return result;
}

std::shared_ptr<Value> IRBuilder::build_alloca(ValueType type, std::shared_ptr<meta::MetaType> meta_type) {
    auto result = create_temp(ValueType::Pointer, meta_type);
    auto inst = std::make_shared<Instruction>(Opcode::Alloca);
    inst->result = result;
    insert_instruction(inst);
    return result;
}

std::shared_ptr<Value> IRBuilder::build_load(std::shared_ptr<Value> address) {
    auto result = create_temp(address->type);
    insert_instruction(Instruction::create_load(result, address));
    return result;
}

void IRBuilder::build_store(std::shared_ptr<Value> value, std::shared_ptr<Value> address) {
    insert_instruction(Instruction::create_store(value, address));
}

std::shared_ptr<Value> IRBuilder::build_call(std::shared_ptr<Value> function, std::vector<std::shared_ptr<Value>> args) {
    auto result = create_temp(ValueType::Void);  // TODO: Get actual return type
    insert_instruction(Instruction::create_call(result, function, std::move(args)));
    return result;
}

void IRBuilder::build_return(std::shared_ptr<Value> value) {
    insert_instruction(Instruction::create_return(value));
}

void IRBuilder::build_branch(const std::string& target) {
    insert_instruction(Instruction::create_branch(target));
}

void IRBuilder::build_cond_branch(std::shared_ptr<Value> condition, const std::string& true_target, const std::string& false_target) {
    insert_instruction(Instruction::create_cond_branch(condition, true_target, false_target));
}

} // namespace meld::compiler::ir
