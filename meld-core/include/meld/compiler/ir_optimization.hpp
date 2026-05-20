#pragma once

#include "meld/compiler/ir.hpp"
#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace meld::compiler::ir {

// Forward declarations
class OptimizationPass;
class PassManager;

// Base class for all optimization passes
class OptimizationPass {
public:
    virtual ~OptimizationPass() = default;
    
    // Run the optimization pass on a module
    virtual bool run_on_module(std::shared_ptr<Module> module) = 0;
    
    // Get the name of this pass (for debugging/logging)
    virtual std::string get_name() const = 0;
    
    // Check if this pass preserves certain properties
    virtual bool preserves_cfg() const { return true; }  // Control Flow Graph
    virtual bool preserves_dominance() const { return true; }
    virtual bool is_analysis_pass() const { return false; }
};

// Pass manager for running optimization passes
class PassManager {
public:
    PassManager() = default;
    
    // Add a pass to the pipeline
    void add_pass(std::unique_ptr<OptimizationPass> pass);
    
    // Run all passes on a module
    bool run_passes(std::shared_ptr<Module> module);
    
    // Clear all passes
    void clear();
    
    // Get number of passes
    size_t pass_count() const { return passes_.size(); }
    
private:
    std::vector<std::unique_ptr<OptimizationPass>> passes_;
};

// Dead Code Elimination Pass
class DeadCodeEliminationPass : public OptimizationPass {
public:
    bool run_on_module(std::shared_ptr<Module> module) override;
    std::string get_name() const override { return "DeadCodeElimination"; }
    
private:
    bool run_on_function(std::shared_ptr<Function> function);
    void mark_live_instructions(std::shared_ptr<Function> function, 
                               std::unordered_set<Instruction*>& live_instructions);
    bool remove_dead_instructions(std::shared_ptr<Function> function,
                                 const std::unordered_set<Instruction*>& live_instructions);
};

// Constant Folding Pass
class ConstantFoldingPass : public OptimizationPass {
public:
    bool run_on_module(std::shared_ptr<Module> module) override;
    std::string get_name() const override { return "ConstantFolding"; }
    
private:
    bool run_on_function(std::shared_ptr<Function> function);
    bool fold_instruction(std::shared_ptr<Instruction> inst);
    std::optional<int64_t> get_constant_int(std::shared_ptr<Value> value);
    std::optional<bool> get_constant_bool(std::shared_ptr<Value> value);
};

// Copy Propagation Pass
class CopyPropagationPass : public OptimizationPass {
public:
    bool run_on_module(std::shared_ptr<Module> module) override;
    std::string get_name() const override { return "CopyPropagation"; }
    
private:
    bool run_on_function(std::shared_ptr<Function> function);
    void build_copy_map(std::shared_ptr<Function> function,
                       std::unordered_map<Value*, Value*>& copy_map);
    bool propagate_copies(std::shared_ptr<Function> function,
                         const std::unordered_map<Value*, Value*>& copy_map);
};

// Common Subexpression Elimination Pass
class CommonSubexpressionEliminationPass : public OptimizationPass {
public:
    bool run_on_module(std::shared_ptr<Module> module) override;
    std::string get_name() const override { return "CommonSubexpressionElimination"; }
    
private:
    bool run_on_function(std::shared_ptr<Function> function);
    
    struct ExpressionKey {
        Opcode opcode;
        std::vector<Value*> operands;
        
        bool operator==(const ExpressionKey& other) const;
    };
    
    struct ExpressionKeyHash {
        size_t operator()(const ExpressionKey& key) const;
    };
    
    bool eliminate_in_block(std::shared_ptr<BasicBlock> block,
                           std::unordered_map<ExpressionKey, Value*, ExpressionKeyHash>& available_expressions);
};

// Unreachable Code Elimination Pass
class UnreachableCodeEliminationPass : public OptimizationPass {
public:
    bool run_on_module(std::shared_ptr<Module> module) override;
    std::string get_name() const override { return "UnreachableCodeElimination"; }
    
private:
    bool run_on_function(std::shared_ptr<Function> function);
    void mark_reachable_blocks(std::shared_ptr<Function> function,
                              std::unordered_set<BasicBlock*>& reachable_blocks);
    bool remove_unreachable_blocks(std::shared_ptr<Function> function,
                                  const std::unordered_set<BasicBlock*>& reachable_blocks);
};

// Algebraic Simplification Pass
class AlgebraicSimplificationPass : public OptimizationPass {
public:
    bool run_on_module(std::shared_ptr<Module> module) override;
    std::string get_name() const override { return "AlgebraicSimplification"; }
    
private:
    bool run_on_function(std::shared_ptr<Function> function);
    bool simplify_instruction(std::shared_ptr<Instruction> inst);
    
    // Specific simplification rules
    bool simplify_add(std::shared_ptr<Instruction> inst);
    bool simplify_sub(std::shared_ptr<Instruction> inst);
    bool simplify_mul(std::shared_ptr<Instruction> inst);
    bool simplify_div(std::shared_ptr<Instruction> inst);
    bool simplify_and(std::shared_ptr<Instruction> inst);
    bool simplify_or(std::shared_ptr<Instruction> inst);
};

// Inline Small Functions Pass
class InlineSmallFunctionsPass : public OptimizationPass {
public:
    explicit InlineSmallFunctionsPass(size_t max_instructions = 10) 
        : max_instructions_(max_instructions) {}
    
    bool run_on_module(std::shared_ptr<Module> module) override;
    std::string get_name() const override { return "InlineSmallFunctions"; }
    
private:
    size_t max_instructions_;
    
    bool should_inline_function(std::shared_ptr<Function> function);
    bool inline_function_calls(std::shared_ptr<Module> module, std::shared_ptr<Function> target_function);
    bool inline_call_site(std::shared_ptr<Instruction> call_inst, std::shared_ptr<Function> target_function);
};

// Utility functions for optimization passes
namespace optimization_utils {
    // Check if an instruction has side effects
    bool has_side_effects(const Instruction& inst);
    
    // Check if an instruction is a terminator
    bool is_terminator(const Instruction& inst);
    
    // Get all uses of a value
    std::vector<Instruction*> get_uses(Value* value, std::shared_ptr<Function> function);
    
    // Replace all uses of old_value with new_value
    void replace_all_uses(Value* old_value, Value* new_value, std::shared_ptr<Function> function);
    
    // Check if a value is a constant
    bool is_constant(std::shared_ptr<Value> value);
    
    // Get the constant value if it's a constant integer
    std::optional<int64_t> get_constant_int_value(std::shared_ptr<Value> value);
    
    // Get the constant value if it's a constant boolean
    std::optional<bool> get_constant_bool_value(std::shared_ptr<Value> value);
    
    // Build control flow graph edges
    void build_cfg_edges(std::shared_ptr<Function> function);
    
    // Verify IR integrity (for debugging)
    bool verify_function(std::shared_ptr<Function> function);
}

} // namespace meld::compiler::ir