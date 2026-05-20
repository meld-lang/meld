#pragma once

#include "meld/compiler/ir.hpp"
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace meld::compiler {

// Bytecode instruction opcodes
enum class BytecodeOp : uint8_t {
    // Constants
    LOAD_CONST_INT = 0x01,
    LOAD_CONST_FLOAT = 0x02,
    LOAD_CONST_BOOL = 0x03,
    LOAD_CONST_STRING = 0x04,
    LOAD_NULL = 0x05,
    
    // Variables
    LOAD_LOCAL = 0x10,
    STORE_LOCAL = 0x11,
    LOAD_GLOBAL = 0x12,
    STORE_GLOBAL = 0x13,
    
    // Arithmetic
    ADD = 0x20,
    SUB = 0x21,
    MUL = 0x22,
    DIV = 0x23,
    MOD = 0x24,
    NEG = 0x25,
    
    // Logical
    AND = 0x30,
    OR = 0x31,
    NOT = 0x32,
    
    // Comparison
    EQ = 0x40,
    NE = 0x41,
    LT = 0x42,
    LE = 0x43,
    GT = 0x44,
    GE = 0x45,
    
    // Control flow
    JUMP = 0x50,
    JUMP_IF_TRUE = 0x51,
    JUMP_IF_FALSE = 0x52,
    CALL = 0x53,
    RETURN = 0x54,
    
    // Stack operations
    POP = 0x60,
    DUP = 0x61,
    SWAP = 0x62,
    
    // Memory
    ALLOC = 0x70,
    LOAD_FIELD = 0x71,
    STORE_FIELD = 0x72,
    
    // Type operations
    TYPEOF = 0x80,
    CAST = 0x81,
    
    // Special
    NOP = 0xFF
};

// Bytecode instruction
struct BytecodeInstruction {
    BytecodeOp opcode;
    std::vector<uint32_t> operands;
    
    BytecodeInstruction(BytecodeOp op) : opcode(op) {}
    BytecodeInstruction(BytecodeOp op, uint32_t operand) : opcode(op), operands{operand} {}
    BytecodeInstruction(BytecodeOp op, uint32_t op1, uint32_t op2) : opcode(op), operands{op1, op2} {}
    
    // Serialize to bytes
    std::vector<uint8_t> serialize() const;
    
    // Deserialize from bytes
    static BytecodeInstruction deserialize(const uint8_t* data, size_t& offset);
    
    std::string to_string() const;
};

// Bytecode function
struct BytecodeFunction {
    std::string name;
    uint32_t parameter_count;
    uint32_t local_count;
    std::vector<BytecodeInstruction> instructions;
    std::vector<std::string> local_names;  // For debugging
    
    BytecodeFunction(std::string n) : name(std::move(n)), parameter_count(0), local_count(0) {}
    
    // Serialize to bytes
    std::vector<uint8_t> serialize() const;
    
    // Deserialize from bytes
    static BytecodeFunction deserialize(const uint8_t* data, size_t& offset);
    
    std::string to_string() const;
};

// Bytecode module
struct BytecodeModule {
    std::string name;
    std::vector<BytecodeFunction> functions;
    std::vector<std::variant<int64_t, double, bool, std::string>> constants;
    std::vector<std::string> global_names;
    
    explicit BytecodeModule(std::string n) : name(std::move(n)) {}
    
    // Add constant and return its index
    uint32_t add_constant(int64_t value);
    uint32_t add_constant(double value);
    uint32_t add_constant(bool value);
    uint32_t add_constant(const std::string& value);
    
    // Add global and return its index
    uint32_t add_global(const std::string& name);
    
    // Serialize to bytes
    std::vector<uint8_t> serialize() const;
    
    // Deserialize from bytes
    static BytecodeModule deserialize(const uint8_t* data, size_t size);
    
    std::string to_string() const;
};

// Bytecode generator - converts IR to bytecode
class BytecodeGenerator {
public:
    BytecodeGenerator() = default;
    
    // Generate bytecode for a module
    BytecodeModule generate(const ir::Module& module);
    
    // Generate bytecode for a function
    BytecodeFunction generate_function(const ir::Function& function);
    
    // Generate bytecode for a basic block
    void generate_block(const ir::BasicBlock& block, BytecodeFunction& func);
    
    // Generate bytecode for an instruction
    void generate_instruction(const ir::Instruction& inst, BytecodeFunction& func);
    
    // Set optimization level
    void set_optimization_level(int level) { optimization_level_ = level; }
    
    // Enable/disable debug information
    void set_debug_info(bool enabled) { debug_info_ = enabled; }
    
private:
    int optimization_level_ = 0;
    bool debug_info_ = false;
    
    // Current generation state
    BytecodeModule* current_module_ = nullptr;
    std::map<std::string, uint32_t> value_to_local_;  // IR value name -> local index
    std::map<std::string, uint32_t> label_to_address_;  // Block label -> instruction address
    std::vector<std::pair<uint32_t, std::string>> pending_jumps_;  // Address -> target label
    
    // Helper methods
    uint32_t get_or_create_local(const std::string& name, BytecodeFunction& func);
    uint32_t get_constant_index(const ir::Instruction& inst);
    void resolve_jumps(BytecodeFunction& func);
    
    // Instruction generation helpers
    void generate_arithmetic(const ir::Instruction& inst, BytecodeFunction& func);
    void generate_logical(const ir::Instruction& inst, BytecodeFunction& func);
    void generate_comparison(const ir::Instruction& inst, BytecodeFunction& func);
    void generate_memory(const ir::Instruction& inst, BytecodeFunction& func);
    void generate_control_flow(const ir::Instruction& inst, BytecodeFunction& func);
    void generate_constant(const ir::Instruction& inst, BytecodeFunction& func);
};

// Bytecode optimizer
class BytecodeOptimizer {
public:
    BytecodeOptimizer() = default;
    
    // Optimize a bytecode module
    void optimize(BytecodeModule& module);
    
    // Optimize a bytecode function
    void optimize_function(BytecodeFunction& function);
    
private:
    // Optimization passes
    void eliminate_dead_code(BytecodeFunction& function);
    void fold_constants(BytecodeFunction& function);
    void optimize_jumps(BytecodeFunction& function);
    void eliminate_redundant_loads(BytecodeFunction& function);
    
    // Helper methods
    bool is_dead_instruction(const BytecodeInstruction& inst, size_t index, const BytecodeFunction& function);
    bool can_fold_constants(const BytecodeInstruction& inst1, const BytecodeInstruction& inst2);
    std::optional<BytecodeInstruction> fold_binary_op(BytecodeOp op, const std::variant<int64_t, double, bool, std::string>& left, 
                                                     const std::variant<int64_t, double, bool, std::string>& right);
};

// Bytecode interpreter (for testing and debugging)
class BytecodeInterpreter {
public:
    BytecodeInterpreter() = default;
    
    // Execute a bytecode module
    int execute(const BytecodeModule& module, const std::vector<std::string>& args = {});
    
    // Execute a specific function
    std::variant<int64_t, double, bool, std::string> execute_function(
        const BytecodeFunction& function, 
        const std::vector<std::variant<int64_t, double, bool, std::string>>& args = {}
    );
    
    // Set debug mode
    void set_debug_mode(bool enabled) { debug_mode_ = enabled; }
    
private:
    bool debug_mode_ = false;
    
    // Runtime state
    std::vector<std::variant<int64_t, double, bool, std::string>> stack_;
    std::vector<std::variant<int64_t, double, bool, std::string>> globals_;
    std::vector<std::variant<int64_t, double, bool, std::string>> locals_;
    
    // Execution helpers
    void push(const std::variant<int64_t, double, bool, std::string>& value);
    std::variant<int64_t, double, bool, std::string> pop();
    std::variant<int64_t, double, bool, std::string> peek();
    
    // Instruction execution
    void execute_instruction(const BytecodeInstruction& inst, const BytecodeModule& module);
    
    // Arithmetic operations
    std::variant<int64_t, double, bool, std::string> add_values(
        const std::variant<int64_t, double, bool, std::string>& left,
        const std::variant<int64_t, double, bool, std::string>& right
    );
    
    std::variant<int64_t, double, bool, std::string> sub_values(
        const std::variant<int64_t, double, bool, std::string>& left,
        const std::variant<int64_t, double, bool, std::string>& right
    );
    
    std::variant<int64_t, double, bool, std::string> mul_values(
        const std::variant<int64_t, double, bool, std::string>& left,
        const std::variant<int64_t, double, bool, std::string>& right
    );
    
    std::variant<int64_t, double, bool, std::string> div_values(
        const std::variant<int64_t, double, bool, std::string>& left,
        const std::variant<int64_t, double, bool, std::string>& right
    );
    
    // Comparison operations
    bool compare_values(
        const std::variant<int64_t, double, bool, std::string>& left,
        const std::variant<int64_t, double, bool, std::string>& right,
        BytecodeOp op
    );
    
    // Debug helpers
    void print_stack() const;
    void print_instruction(const BytecodeInstruction& inst, size_t pc) const;
};

} // namespace meld::compiler