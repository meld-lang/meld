#include "meld/compiler/ir_optimization.hpp"
#include <algorithm>
#include <queue>
#include <iostream>

namespace meld::compiler::ir {

// PassManager implementation
void PassManager::add_pass(std::unique_ptr<OptimizationPass> pass) {
    passes_.push_back(std::move(pass));
}

bool PassManager::run_passes(std::shared_ptr<Module> module) {
    bool changed = false;
    
    for (auto& pass : passes_) {
        std::cout << "Running pass: " << pass->get_name() << std::endl;
        bool pass_changed = pass->run_on_module(module);
        changed |= pass_changed;
        
        if (pass_changed) {
            std::cout << "  Pass made changes" << std::endl;
        }
    }
    
    return changed;
}

void PassManager::clear() {
    passes_.clear();
}

// Dead Code Elimination Pass
bool DeadCodeEliminationPass::run_on_module(std::shared_ptr<Module> module) {
    bool changed = false;
    
    for (auto& function : module->functions) {
        changed |= run_on_function(function);
    }
    
    return changed;
}

bool DeadCodeEliminationPass::run_on_function(std::shared_ptr<Function> function) {
    std::unordered_set<Instruction*> live_instructions;
    
    // Mark all live instructions
    mark_live_instructions(function, live_instructions);
    
    // Remove dead instructions
    return remove_dead_instructions(function, live_instructions);
}

void DeadCodeEliminationPass::mark_live_instructions(std::shared_ptr<Function> function,
                                                    std::unordered_set<Instruction*>& live_instructions) {
    std::queue<Instruction*> worklist;
    
    // Mark all instructions with side effects as live
    for (auto& block : function->basic_blocks) {
        for (auto& inst : block->instructions) {
            if (optimization_utils::has_side_effects(*inst) || optimization_utils::is_terminator(*inst)) {
                live_instructions.insert(inst.get());
                worklist.push(inst.get());
            }
        }
    }
    
    // Propagate liveness backwards
    while (!worklist.empty()) {
        auto* inst = worklist.front();
        worklist.pop();
        
        // Mark all operands as live
        for (auto& operand : inst->operands) {
            // Find the instruction that defines this operand
            for (auto& block : function->basic_blocks) {
                for (auto& candidate_inst : block->instructions) {
                    if (candidate_inst->result && candidate_inst->result.get() == operand.get()) {
                        if (live_instructions.find(candidate_inst.get()) == live_instructions.end()) {
                            live_instructions.insert(candidate_inst.get());
                            worklist.push(candidate_inst.get());
                        }
                        break;
                    }
                }
            }
        }
    }
}

bool DeadCodeEliminationPass::remove_dead_instructions(std::shared_ptr<Function> function,
                                                      const std::unordered_set<Instruction*>& live_instructions) {
    bool changed = false;
    
    for (auto& block : function->basic_blocks) {
        auto it = block->instructions.begin();
        while (it != block->instructions.end()) {
            if (live_instructions.find(it->get()) == live_instructions.end()) {
                it = block->instructions.erase(it);
                changed = true;
            } else {
                ++it;
            }
        }
    }
    
    return changed;
}

// Constant Folding Pass
bool ConstantFoldingPass::run_on_module(std::shared_ptr<Module> module) {
    bool changed = false;
    
    for (auto& function : module->functions) {
        changed |= run_on_function(function);
    }
    
    return changed;
}

bool ConstantFoldingPass::run_on_function(std::shared_ptr<Function> function) {
    bool changed = false;
    
    for (auto& block : function->basic_blocks) {
        for (auto& inst : block->instructions) {
            changed |= fold_instruction(inst);
        }
    }
    
    return changed;
}

bool ConstantFoldingPass::fold_instruction(std::shared_ptr<Instruction> inst) {
    switch (inst->opcode) {
        case Opcode::Add: {
            auto left = get_constant_int(inst->operands[0]);
            auto right = get_constant_int(inst->operands[1]);
            if (left && right) {
                inst->opcode = Opcode::ConstInt;
                inst->constant_value = *left + *right;
                inst->operands.clear();
                return true;
            }
            break;
        }
        case Opcode::Sub: {
            auto left = get_constant_int(inst->operands[0]);
            auto right = get_constant_int(inst->operands[1]);
            if (left && right) {
                inst->opcode = Opcode::ConstInt;
                inst->constant_value = *left - *right;
                inst->operands.clear();
                return true;
            }
            break;
        }
        case Opcode::Mul: {
            auto left = get_constant_int(inst->operands[0]);
            auto right = get_constant_int(inst->operands[1]);
            if (left && right) {
                inst->opcode = Opcode::ConstInt;
                inst->constant_value = *left * *right;
                inst->operands.clear();
                return true;
            }
            break;
        }
        case Opcode::Div: {
            auto left = get_constant_int(inst->operands[0]);
            auto right = get_constant_int(inst->operands[1]);
            if (left && right && *right != 0) {
                inst->opcode = Opcode::ConstInt;
                inst->constant_value = *left / *right;
                inst->operands.clear();
                return true;
            }
            break;
        }
        case Opcode::And: {
            auto left = get_constant_bool(inst->operands[0]);
            auto right = get_constant_bool(inst->operands[1]);
            if (left && right) {
                inst->opcode = Opcode::ConstBool;
                inst->constant_value = *left && *right;
                inst->operands.clear();
                return true;
            }
            break;
        }
        case Opcode::Or: {
            auto left = get_constant_bool(inst->operands[0]);
            auto right = get_constant_bool(inst->operands[1]);
            if (left && right) {
                inst->opcode = Opcode::ConstBool;
                inst->constant_value = *left || *right;
                inst->operands.clear();
                return true;
            }
            break;
        }
        case Opcode::Eq: {
            auto left = get_constant_int(inst->operands[0]);
            auto right = get_constant_int(inst->operands[1]);
            if (left && right) {
                inst->opcode = Opcode::ConstBool;
                inst->constant_value = *left == *right;
                inst->operands.clear();
                return true;
            }
            break;
        }
        case Opcode::Lt: {
            auto left = get_constant_int(inst->operands[0]);
            auto right = get_constant_int(inst->operands[1]);
            if (left && right) {
                inst->opcode = Opcode::ConstBool;
                inst->constant_value = *left < *right;
                inst->operands.clear();
                return true;
            }
            break;
        }
        default:
            break;
    }
    
    return false;
}

std::optional<int64_t> ConstantFoldingPass::get_constant_int(std::shared_ptr<Value> value) {
    return optimization_utils::get_constant_int_value(value);
}

std::optional<bool> ConstantFoldingPass::get_constant_bool(std::shared_ptr<Value> value) {
    return optimization_utils::get_constant_bool_value(value);
}

// Copy Propagation Pass
bool CopyPropagationPass::run_on_module(std::shared_ptr<Module> module) {
    bool changed = false;
    
    for (auto& function : module->functions) {
        changed |= run_on_function(function);
    }
    
    return changed;
}

bool CopyPropagationPass::run_on_function(std::shared_ptr<Function> function) {
    std::unordered_map<Value*, Value*> copy_map;
    
    // Build copy map (find all copy operations)
    build_copy_map(function, copy_map);
    
    // Propagate copies
    return propagate_copies(function, copy_map);
}

void CopyPropagationPass::build_copy_map(std::shared_ptr<Function> function,
                                        std::unordered_map<Value*, Value*>& copy_map) {
    for (auto& block : function->basic_blocks) {
        for (auto& inst : block->instructions) {
            // Look for simple copy operations (load from a store, or direct assignment)
            if (inst->opcode == Opcode::Load && inst->operands.size() == 1) {
                // This is a simplified copy detection - in a real implementation,
                // we'd need to track store-load pairs more carefully
                // For now, we'll skip this optimization
            }
        }
    }
}

bool CopyPropagationPass::propagate_copies(std::shared_ptr<Function> function,
                                          const std::unordered_map<Value*, Value*>& copy_map) {
    bool changed = false;
    
    for (auto& block : function->basic_blocks) {
        for (auto& inst : block->instructions) {
            for (auto& operand : inst->operands) {
                auto it = copy_map.find(operand.get());
                if (it != copy_map.end()) {
                    // Replace operand with the copied value
                    // Note: This is simplified - we'd need to ensure the replacement is valid
                    changed = true;
                }
            }
        }
    }
    
    return changed;
}

// Common Subexpression Elimination Pass
bool CommonSubexpressionEliminationPass::run_on_module(std::shared_ptr<Module> module) {
    bool changed = false;
    
    for (auto& function : module->functions) {
        changed |= run_on_function(function);
    }
    
    return changed;
}

bool CommonSubexpressionEliminationPass::run_on_function(std::shared_ptr<Function> function) {
    bool changed = false;
    
    for (auto& block : function->basic_blocks) {
        std::unordered_map<ExpressionKey, Value*, ExpressionKeyHash> available_expressions;
        changed |= eliminate_in_block(block, available_expressions);
    }
    
    return changed;
}

bool CommonSubexpressionEliminationPass::ExpressionKey::operator==(const ExpressionKey& other) const {
    if (opcode != other.opcode || operands.size() != other.operands.size()) {
        return false;
    }
    
    for (size_t i = 0; i < operands.size(); ++i) {
        if (operands[i] != other.operands[i]) {
            return false;
        }
    }
    
    return true;
}

size_t CommonSubexpressionEliminationPass::ExpressionKeyHash::operator()(const ExpressionKey& key) const {
    size_t hash = std::hash<int>{}(static_cast<int>(key.opcode));
    
    for (auto* operand : key.operands) {
        hash ^= std::hash<void*>{}(operand) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }
    
    return hash;
}

bool CommonSubexpressionEliminationPass::eliminate_in_block(
    std::shared_ptr<BasicBlock> block,
    std::unordered_map<ExpressionKey, Value*, ExpressionKeyHash>& available_expressions) {
    
    bool changed = false;
    
    for (auto& inst : block->instructions) {
        // Only consider pure expressions (no side effects)
        if (!optimization_utils::has_side_effects(*inst) && inst->result) {
            ExpressionKey key;
            key.opcode = inst->opcode;
            
            for (auto& operand : inst->operands) {
                key.operands.push_back(operand.get());
            }
            
            auto it = available_expressions.find(key);
            if (it != available_expressions.end()) {
                // Found a common subexpression - replace uses of this result with the previous one
                optimization_utils::replace_all_uses(inst->result.get(), it->second, 
                                                   block->instructions[0]->result ? 
                                                   nullptr : nullptr); // Simplified
                changed = true;
            } else {
                // Add this expression to available expressions
                available_expressions[key] = inst->result.get();
            }
        }
    }
    
    return changed;
}

// Unreachable Code Elimination Pass
bool UnreachableCodeEliminationPass::run_on_module(std::shared_ptr<Module> module) {
    bool changed = false;
    
    for (auto& function : module->functions) {
        changed |= run_on_function(function);
    }
    
    return changed;
}

bool UnreachableCodeEliminationPass::run_on_function(std::shared_ptr<Function> function) {
    std::unordered_set<BasicBlock*> reachable_blocks;
    
    // Mark reachable blocks
    mark_reachable_blocks(function, reachable_blocks);
    
    // Remove unreachable blocks
    return remove_unreachable_blocks(function, reachable_blocks);
}

void UnreachableCodeEliminationPass::mark_reachable_blocks(std::shared_ptr<Function> function,
                                                          std::unordered_set<BasicBlock*>& reachable_blocks) {
    if (function->basic_blocks.empty()) return;
    
    std::queue<BasicBlock*> worklist;
    
    // Start from entry block
    auto* entry = function->basic_blocks[0].get();
    reachable_blocks.insert(entry);
    worklist.push(entry);
    
    while (!worklist.empty()) {
        auto* block = worklist.front();
        worklist.pop();
        
        // Mark successors as reachable
        for (const auto& successor_label : block->successors) {
            auto successor_block = function->get_block(successor_label);
            if (successor_block && reachable_blocks.find(successor_block.get()) == reachable_blocks.end()) {
                reachable_blocks.insert(successor_block.get());
                worklist.push(successor_block.get());
            }
        }
    }
}

bool UnreachableCodeEliminationPass::remove_unreachable_blocks(std::shared_ptr<Function> function,
                                                              const std::unordered_set<BasicBlock*>& reachable_blocks) {
    bool changed = false;
    
    auto it = function->basic_blocks.begin();
    while (it != function->basic_blocks.end()) {
        if (reachable_blocks.find(it->get()) == reachable_blocks.end()) {
            it = function->basic_blocks.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }
    
    return changed;
}

// Algebraic Simplification Pass
bool AlgebraicSimplificationPass::run_on_module(std::shared_ptr<Module> module) {
    bool changed = false;
    
    for (auto& function : module->functions) {
        changed |= run_on_function(function);
    }
    
    return changed;
}

bool AlgebraicSimplificationPass::run_on_function(std::shared_ptr<Function> function) {
    bool changed = false;
    
    for (auto& block : function->basic_blocks) {
        for (auto& inst : block->instructions) {
            changed |= simplify_instruction(inst);
        }
    }
    
    return changed;
}

bool AlgebraicSimplificationPass::simplify_instruction(std::shared_ptr<Instruction> inst) {
    switch (inst->opcode) {
        case Opcode::Add:
            return simplify_add(inst);
        case Opcode::Sub:
            return simplify_sub(inst);
        case Opcode::Mul:
            return simplify_mul(inst);
        case Opcode::Div:
            return simplify_div(inst);
        case Opcode::And:
            return simplify_and(inst);
        case Opcode::Or:
            return simplify_or(inst);
        default:
            return false;
    }
}

bool AlgebraicSimplificationPass::simplify_add(std::shared_ptr<Instruction> inst) {
    if (inst->operands.size() != 2) return false;
    
    auto left_const = optimization_utils::get_constant_int_value(inst->operands[0]);
    auto right_const = optimization_utils::get_constant_int_value(inst->operands[1]);
    
    // x + 0 = x
    if (left_const && *left_const == 0) {
        // Replace with right operand
        inst->opcode = Opcode::Nop; // Simplified replacement
        return true;
    }
    if (right_const && *right_const == 0) {
        // Replace with left operand
        inst->opcode = Opcode::Nop; // Simplified replacement
        return true;
    }
    
    return false;
}

bool AlgebraicSimplificationPass::simplify_sub(std::shared_ptr<Instruction> inst) {
    if (inst->operands.size() != 2) return false;
    
    auto right_const = optimization_utils::get_constant_int_value(inst->operands[1]);
    
    // x - 0 = x
    if (right_const && *right_const == 0) {
        inst->opcode = Opcode::Nop; // Simplified replacement
        return true;
    }
    
    // x - x = 0
    if (inst->operands[0].get() == inst->operands[1].get()) {
        inst->opcode = Opcode::ConstInt;
        inst->constant_value = int64_t(0);
        inst->operands.clear();
        return true;
    }
    
    return false;
}

bool AlgebraicSimplificationPass::simplify_mul(std::shared_ptr<Instruction> inst) {
    if (inst->operands.size() != 2) return false;
    
    auto left_const = optimization_utils::get_constant_int_value(inst->operands[0]);
    auto right_const = optimization_utils::get_constant_int_value(inst->operands[1]);
    
    // x * 0 = 0
    if ((left_const && *left_const == 0) || (right_const && *right_const == 0)) {
        inst->opcode = Opcode::ConstInt;
        inst->constant_value = int64_t(0);
        inst->operands.clear();
        return true;
    }
    
    // x * 1 = x
    if (left_const && *left_const == 1) {
        inst->opcode = Opcode::Nop; // Simplified replacement
        return true;
    }
    if (right_const && *right_const == 1) {
        inst->opcode = Opcode::Nop; // Simplified replacement
        return true;
    }
    
    return false;
}

bool AlgebraicSimplificationPass::simplify_div(std::shared_ptr<Instruction> inst) {
    if (inst->operands.size() != 2) return false;
    
    auto right_const = optimization_utils::get_constant_int_value(inst->operands[1]);
    
    // x / 1 = x
    if (right_const && *right_const == 1) {
        inst->opcode = Opcode::Nop; // Simplified replacement
        return true;
    }
    
    return false;
}

bool AlgebraicSimplificationPass::simplify_and(std::shared_ptr<Instruction> inst) {
    if (inst->operands.size() != 2) return false;
    
    auto left_const = optimization_utils::get_constant_bool_value(inst->operands[0]);
    auto right_const = optimization_utils::get_constant_bool_value(inst->operands[1]);
    
    // x && false = false
    if ((left_const && !*left_const) || (right_const && !*right_const)) {
        inst->opcode = Opcode::ConstBool;
        inst->constant_value = false;
        inst->operands.clear();
        return true;
    }
    
    // x && true = x
    if (left_const && *left_const) {
        inst->opcode = Opcode::Nop; // Simplified replacement
        return true;
    }
    if (right_const && *right_const) {
        inst->opcode = Opcode::Nop; // Simplified replacement
        return true;
    }
    
    return false;
}

bool AlgebraicSimplificationPass::simplify_or(std::shared_ptr<Instruction> inst) {
    if (inst->operands.size() != 2) return false;
    
    auto left_const = optimization_utils::get_constant_bool_value(inst->operands[0]);
    auto right_const = optimization_utils::get_constant_bool_value(inst->operands[1]);
    
    // x || true = true
    if ((left_const && *left_const) || (right_const && *right_const)) {
        inst->opcode = Opcode::ConstBool;
        inst->constant_value = true;
        inst->operands.clear();
        return true;
    }
    
    // x || false = x
    if (left_const && !*left_const) {
        inst->opcode = Opcode::Nop; // Simplified replacement
        return true;
    }
    if (right_const && !*right_const) {
        inst->opcode = Opcode::Nop; // Simplified replacement
        return true;
    }
    
    return false;
}

// Inline Small Functions Pass
bool InlineSmallFunctionsPass::run_on_module(std::shared_ptr<Module> module) {
    bool changed = false;
    
    // Find functions that should be inlined
    std::vector<std::shared_ptr<Function>> functions_to_inline;
    for (auto& function : module->functions) {
        if (should_inline_function(function)) {
            functions_to_inline.push_back(function);
        }
    }
    
    // Inline the functions
    for (auto& function : functions_to_inline) {
        changed |= inline_function_calls(module, function);
    }
    
    return changed;
}

bool InlineSmallFunctionsPass::should_inline_function(std::shared_ptr<Function> function) {
    size_t instruction_count = 0;
    
    for (auto& block : function->basic_blocks) {
        instruction_count += block->instructions.size();
    }
    
    return instruction_count <= max_instructions_;
}

bool InlineSmallFunctionsPass::inline_function_calls(std::shared_ptr<Module> module, 
                                                    std::shared_ptr<Function> target_function) {
    bool changed = false;
    
    for (auto& function : module->functions) {
        if (function == target_function) continue; // Don't inline into itself
        
        for (auto& block : function->basic_blocks) {
            for (auto& inst : block->instructions) {
                if (inst->opcode == Opcode::Call && inst->operands.size() > 0) {
                    // Check if this is a call to our target function
                    // This is simplified - in reality we'd need better function identification
                    changed |= inline_call_site(inst, target_function);
                }
            }
        }
    }
    
    return changed;
}

bool InlineSmallFunctionsPass::inline_call_site(std::shared_ptr<Instruction> call_inst, 
                                               std::shared_ptr<Function> target_function) {
    // This is a complex operation that would involve:
    // 1. Copying the target function's basic blocks
    // 2. Renaming all values to avoid conflicts
    // 3. Replacing parameter uses with argument values
    // 4. Replacing return instructions with branches to continuation
    // 5. Updating the control flow graph
    
    // For now, we'll just mark it as a potential optimization
    return false; // Simplified - not implemented
}

// Utility functions
namespace optimization_utils {

bool has_side_effects(const Instruction& inst) {
    switch (inst.opcode) {
        case Opcode::Store:
        case Opcode::Call:
        case Opcode::SetField:
            return true;
        default:
            return false;
    }
}

bool is_terminator(const Instruction& inst) {
    switch (inst.opcode) {
        case Opcode::Return:
        case Opcode::Branch:
        case Opcode::CondBranch:
            return true;
        default:
            return false;
    }
}

std::vector<Instruction*> get_uses(Value* value, std::shared_ptr<Function> function) {
    std::vector<Instruction*> uses;
    
    for (auto& block : function->basic_blocks) {
        for (auto& inst : block->instructions) {
            for (auto& operand : inst->operands) {
                if (operand.get() == value) {
                    uses.push_back(inst.get());
                }
            }
        }
    }
    
    return uses;
}

void replace_all_uses(Value* old_value, Value* new_value, std::shared_ptr<Function> function) {
    if (!function) return;
    
    for (auto& block : function->basic_blocks) {
        for (auto& inst : block->instructions) {
            for (auto& operand : inst->operands) {
                if (operand.get() == old_value) {
                    // This is simplified - we'd need to create a new shared_ptr properly
                    // operand = std::shared_ptr<Value>(new_value);
                }
            }
        }
    }
}

bool is_constant(std::shared_ptr<Value> value) {
    // In a real implementation, we'd track which values are constants
    // For now, we'll use a simple heuristic based on the name
    return value->name.find("const") != std::string::npos || 
           value->name.find("%t") == 0; // Temporary values from constant instructions
}

std::optional<int64_t> get_constant_int_value(std::shared_ptr<Value> value) {
    if (value && value->is_constant()) {
        return value->get_constant_int();
    }
    return std::nullopt;
}

std::optional<bool> get_constant_bool_value(std::shared_ptr<Value> value) {
    if (value && value->is_constant()) {
        return value->get_constant_bool();
    }
    return std::nullopt;
}

void build_cfg_edges(std::shared_ptr<Function> function) {
    // Clear existing edges
    for (auto& block : function->basic_blocks) {
        block->predecessors.clear();
        block->successors.clear();
    }
    
    // Build edges based on terminator instructions
    for (auto& block : function->basic_blocks) {
        if (block->instructions.empty()) continue;
        
        auto& last_inst = block->instructions.back();
        
        switch (last_inst->opcode) {
            case Opcode::Branch:
                if (!last_inst->target_label.empty()) {
                    block->successors.push_back(last_inst->target_label);
                    auto target = function->get_block(last_inst->target_label);
                    if (target) {
                        target->predecessors.push_back(block->label);
                    }
                }
                break;
                
            case Opcode::CondBranch:
                if (!last_inst->target_label.empty()) {
                    block->successors.push_back(last_inst->target_label);
                    auto target = function->get_block(last_inst->target_label);
                    if (target) {
                        target->predecessors.push_back(block->label);
                    }
                }
                if (!last_inst->else_label.empty()) {
                    block->successors.push_back(last_inst->else_label);
                    auto target = function->get_block(last_inst->else_label);
                    if (target) {
                        target->predecessors.push_back(block->label);
                    }
                }
                break;
                
            default:
                break;
        }
    }
}

bool verify_function(std::shared_ptr<Function> function) {
    // Basic verification checks
    if (!function) return false;
    
    // Check that all blocks are properly terminated
    for (auto& block : function->basic_blocks) {
        if (!block->is_terminated()) {
            std::cerr << "Block " << block->label << " is not properly terminated" << std::endl;
            return false;
        }
    }
    
    // Check that all branch targets exist
    for (auto& block : function->basic_blocks) {
        for (auto& successor : block->successors) {
            if (!function->get_block(successor)) {
                std::cerr << "Block " << block->label << " references non-existent block " << successor << std::endl;
                return false;
            }
        }
    }
    
    return true;
}

} // namespace optimization_utils

} // namespace meld::compiler::ir