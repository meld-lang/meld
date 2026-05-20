#include <gtest/gtest.h>
#include "../../include/meld/compiler/ir.hpp"
#include "../../include/meld/compiler/ir_optimization.hpp"
#include <memory>

using namespace meld::compiler::ir;

class IROptimizationTest : public ::testing::Test {
protected:
    void SetUp() override {
        module = std::make_shared<Module>("test_module");
        builder = std::make_unique<IRBuilder>(module);
    }
    
    std::shared_ptr<Module> module;
    std::unique_ptr<IRBuilder> builder;
};

// Test Constant Folding Pass
TEST_F(IROptimizationTest, ConstantFoldingBasicArithmetic) {
    // Create function: compute() = 10 + 20
    auto func = module->create_function("compute");
    auto entry = func->create_block("entry");
    
    builder->set_current_function(func);
    builder->set_insert_point(entry);
    
    auto const10 = builder->build_const_int(10);
    auto const20 = builder->build_const_int(20);
    auto add_result = builder->build_add(const10, const20);
    builder->build_return(add_result);
    
    // Verify we have an Add instruction before optimization
    ASSERT_EQ(entry->instructions.size(), 3);  // const10, const20, add
    EXPECT_EQ(entry->instructions[2]->opcode, Opcode::Add);
    
    // Apply constant folding
    ConstantFoldingPass pass;
    bool changed = pass.run_on_module(module);
    
    EXPECT_TRUE(changed);
    
    // Verify the Add instruction was folded to a constant
    EXPECT_EQ(entry->instructions[2]->opcode, Opcode::ConstInt);
    EXPECT_EQ(std::get<int64_t>(entry->instructions[2]->constant_value), 30);
}

TEST_F(IROptimizationTest, ConstantFoldingMultiplication) {
    // Create function: compute() = 6 * 7
    auto func = module->create_function("compute");
    auto entry = func->create_block("entry");
    
    builder->set_current_function(func);
    builder->set_insert_point(entry);
    
    auto const6 = builder->build_const_int(6);
    auto const7 = builder->build_const_int(7);
    auto mul_result = builder->build_mul(const6, const7);
    builder->build_return(mul_result);
    
    // Apply constant folding
    ConstantFoldingPass pass;
    bool changed = pass.run_on_module(module);
    
    EXPECT_TRUE(changed);
    
    // Verify the multiplication was folded
    EXPECT_EQ(entry->instructions[2]->opcode, Opcode::ConstInt);
    EXPECT_EQ(std::get<int64_t>(entry->instructions[2]->constant_value), 42);
}

TEST_F(IROptimizationTest, ConstantFoldingBooleanLogic) {
    // Create function: compute() = true && false
    auto func = module->create_function("compute");
    auto entry = func->create_block("entry");
    
    builder->set_current_function(func);
    builder->set_insert_point(entry);
    
    auto const_true = builder->build_const_bool(true);
    auto const_false = builder->build_const_bool(false);
    auto and_result = builder->build_and(const_true, const_false);
    builder->build_return(and_result);
    
    // Apply constant folding
    ConstantFoldingPass pass;
    bool changed = pass.run_on_module(module);
    
    EXPECT_TRUE(changed);
    
    // Verify the AND was folded to false
    EXPECT_EQ(entry->instructions[2]->opcode, Opcode::ConstBool);
    EXPECT_EQ(std::get<bool>(entry->instructions[2]->constant_value), false);
}

// Test Algebraic Simplification Pass
TEST_F(IROptimizationTest, AlgebraicSimplificationAddZero) {
    // Create function: simplify(x) = x + 0
    auto func = module->create_function("simplify");
    auto param_x = std::make_shared<Value>("x", ValueType::Int);
    func->parameters.push_back(param_x);
    
    auto entry = func->create_block("entry");
    builder->set_current_function(func);
    builder->set_insert_point(entry);
    
    auto const0 = builder->build_const_int(0);
    auto add_result = builder->build_add(param_x, const0);
    builder->build_return(add_result);
    
    // Apply algebraic simplification
    AlgebraicSimplificationPass pass;
    bool changed = pass.run_on_module(module);
    
    EXPECT_TRUE(changed);
    
    // Verify the addition was simplified (instruction should be marked as Nop or eliminated)
    EXPECT_EQ(entry->instructions[1]->opcode, Opcode::Nop);
}

TEST_F(IROptimizationTest, AlgebraicSimplificationMultiplyByOne) {
    // Create function: simplify(x) = x * 1
    auto func = module->create_function("simplify");
    auto param_x = std::make_shared<Value>("x", ValueType::Int);
    func->parameters.push_back(param_x);
    
    auto entry = func->create_block("entry");
    builder->set_current_function(func);
    builder->set_insert_point(entry);
    
    auto const1 = builder->build_const_int(1);
    auto mul_result = builder->build_mul(param_x, const1);
    builder->build_return(mul_result);
    
    // Apply algebraic simplification
    AlgebraicSimplificationPass pass;
    bool changed = pass.run_on_module(module);
    
    EXPECT_TRUE(changed);
    
    // Verify the multiplication was simplified
    EXPECT_EQ(entry->instructions[1]->opcode, Opcode::Nop);
}

TEST_F(IROptimizationTest, AlgebraicSimplificationMultiplyByZero) {
    // Create function: simplify(x) = x * 0
    auto func = module->create_function("simplify");
    auto param_x = std::make_shared<Value>("x", ValueType::Int);
    func->parameters.push_back(param_x);
    
    auto entry = func->create_block("entry");
    builder->set_current_function(func);
    builder->set_insert_point(entry);
    
    auto const0 = builder->build_const_int(0);
    auto mul_result = builder->build_mul(param_x, const0);
    builder->build_return(mul_result);
    
    // Apply algebraic simplification
    AlgebraicSimplificationPass pass;
    bool changed = pass.run_on_module(module);
    
    EXPECT_TRUE(changed);
    
    // Verify the multiplication was replaced with constant 0
    EXPECT_EQ(entry->instructions[1]->opcode, Opcode::ConstInt);
    EXPECT_EQ(std::get<int64_t>(entry->instructions[1]->constant_value), 0);
}

// Test Dead Code Elimination Pass
TEST_F(IROptimizationTest, DeadCodeEliminationUnusedComputation) {
    // Create function with dead code
    auto func = module->create_function("with_dead_code");
    auto entry = func->create_block("entry");
    
    builder->set_current_function(func);
    builder->set_insert_point(entry);
    
    // Live code
    auto const42 = builder->build_const_int(42);
    
    // Dead code (result not used)
    auto const100 = builder->build_const_int(100);
    auto const200 = builder->build_const_int(200);
    auto dead_add = builder->build_add(const100, const200);
    
    // More live code
    builder->build_return(const42);
    
    size_t initial_instruction_count = entry->instructions.size();
    
    // Apply dead code elimination
    DeadCodeEliminationPass pass;
    bool changed = pass.run_on_module(module);
    
    EXPECT_TRUE(changed);
    
    // Verify dead instructions were removed
    EXPECT_LT(entry->instructions.size(), initial_instruction_count);
    
    // Verify the return instruction is still there
    EXPECT_EQ(entry->instructions.back()->opcode, Opcode::Return);
}

// Test Unreachable Code Elimination Pass
TEST_F(IROptimizationTest, UnreachableCodeEliminationBasic) {
    // Create function with unreachable blocks
    auto func = module->create_function("with_unreachable");
    
    auto entry = func->create_block("entry");
    auto unreachable_block = func->create_block("unreachable");
    
    // Entry block - always returns
    builder->set_current_function(func);
    builder->set_insert_point(entry);
    auto const42 = builder->build_const_int(42);
    builder->build_return(const42);
    
    // Unreachable block
    builder->set_insert_point(unreachable_block);
    auto dead_const = builder->build_const_int(100);
    builder->build_return(dead_const);
    
    ASSERT_EQ(func->basic_blocks.size(), 2);
    
    // Apply unreachable code elimination
    UnreachableCodeEliminationPass pass;
    bool changed = pass.run_on_module(module);
    
    EXPECT_TRUE(changed);
    
    // Verify unreachable block was removed
    EXPECT_EQ(func->basic_blocks.size(), 1);
    EXPECT_EQ(func->basic_blocks[0]->label, "entry");
}

// Test Pass Manager
TEST_F(IROptimizationTest, PassManagerMultiplePasses) {
    // Create function with multiple optimization opportunities
    auto func = module->create_function("complex");
    auto entry = func->create_block("entry");
    
    builder->set_current_function(func);
    builder->set_insert_point(entry);
    
    // Build: (10 + 20) + (x * 0) where x is a parameter
    auto param_x = std::make_shared<Value>("x", ValueType::Int);
    func->parameters.push_back(param_x);
    
    auto const10 = builder->build_const_int(10);
    auto const20 = builder->build_const_int(20);
    auto const0 = builder->build_const_int(0);
    
    auto add1 = builder->build_add(const10, const20);  // Should fold to 30
    auto mul1 = builder->build_mul(param_x, const0);   // Should simplify to 0
    auto add2 = builder->build_add(add1, mul1);        // Should become 30 + 0 = 30
    
    builder->build_return(add2);
    
    // Apply multiple optimization passes
    PassManager pass_manager;
    pass_manager.add_pass(std::make_unique<ConstantFoldingPass>());
    pass_manager.add_pass(std::make_unique<AlgebraicSimplificationPass>());
    pass_manager.add_pass(std::make_unique<DeadCodeEliminationPass>());
    
    bool changed = pass_manager.run_passes(module);
    
    EXPECT_TRUE(changed);
    EXPECT_EQ(pass_manager.pass_count(), 3);
}

// Test optimization utility functions
TEST_F(IROptimizationTest, OptimizationUtilsHasSideEffects) {
    // Test side effect detection
    auto store_inst = std::make_shared<Instruction>(Opcode::Store);
    auto add_inst = std::make_shared<Instruction>(Opcode::Add);
    auto call_inst = std::make_shared<Instruction>(Opcode::Call);
    
    EXPECT_TRUE(optimization_utils::has_side_effects(*store_inst));
    EXPECT_FALSE(optimization_utils::has_side_effects(*add_inst));
    EXPECT_TRUE(optimization_utils::has_side_effects(*call_inst));
}

TEST_F(IROptimizationTest, OptimizationUtilsIsTerminator) {
    // Test terminator detection
    auto return_inst = std::make_shared<Instruction>(Opcode::Return);
    auto branch_inst = std::make_shared<Instruction>(Opcode::Branch);
    auto add_inst = std::make_shared<Instruction>(Opcode::Add);
    
    EXPECT_TRUE(optimization_utils::is_terminator(*return_inst));
    EXPECT_TRUE(optimization_utils::is_terminator(*branch_inst));
    EXPECT_FALSE(optimization_utils::is_terminator(*add_inst));
}

TEST_F(IROptimizationTest, ValueConstantDetection) {
    // Test constant value detection in enhanced Value struct
    auto int_value = std::make_shared<Value>("test", ValueType::Int);
    int_value->constant_data = int64_t(42);
    
    auto bool_value = std::make_shared<Value>("test", ValueType::Bool);
    bool_value->constant_data = true;
    
    auto non_constant = std::make_shared<Value>("test", ValueType::Int);
    
    EXPECT_TRUE(int_value->is_constant());
    EXPECT_EQ(int_value->get_constant_int(), 42);
    
    EXPECT_TRUE(bool_value->is_constant());
    EXPECT_EQ(bool_value->get_constant_bool(), true);
    
    EXPECT_FALSE(non_constant->is_constant());
    EXPECT_EQ(non_constant->get_constant_int(), std::nullopt);
}