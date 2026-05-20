#pragma once

#include "meld/meta/metatype.hpp"
#include <memory>
#include <string>
#include <vector>
#include <variant>
#include <optional>

namespace meld::compiler::ir {

// Forward declarations
struct Instruction;
struct BasicBlock;
struct Function;
struct Module;

// ─── Debug info metadata (DWARF/PDB emission support, Req 12B) ──────

/// Source location attached to IR instructions for debug info mapping.
struct DILocation {
    std::string file;
    size_t line = 0;
    size_t column = 0;
};

/// Debug info type descriptor — maps Meld/kernel types to DWARF type entries.
struct DIType {
    std::string name;
    std::string encoding;
    size_t size_bits = 0;
};

/// Debug info for a local variable (val/var declaration).
struct DILocalVariable {
    std::string name;
    DIType type;
    DILocation location;
    bool is_parameter = false;
    int arg_index = -1;
};

/// Debug info for a function — maps to DWARF DISubprogram.
struct DISubprogram {
    std::string name;
    std::string linkage_name;
    DILocation location;
    DIType return_type;
    std::vector<DILocalVariable> variables;
    bool is_definition = true;
};

// IR value types
enum class ValueType {
    Void,
    Unit,        // Unit type (empty tuple)
    Int,
    Float,
    Bool,
    String,
    Pointer,
    Struct,
    Function
};

// IR value - represents a computed value or variable
struct Value {
    std::string name;
    ValueType type;
    std::shared_ptr<meta::MetaType> meta_type;  // Link to Meld type system
    
    // For constant values, store the constant data
    std::optional<std::variant<int64_t, double, bool, std::string>> constant_data;
    
    Value() : type(ValueType::Void) {}
    Value(std::string n, ValueType t, std::shared_ptr<meta::MetaType> mt = nullptr)
        : name(std::move(n)), type(t), meta_type(std::move(mt)) {}
        
    Value(ValueType t, std::string n)
        : name(std::move(n)), type(t) {}
    
    // Check if this value is a compile-time constant
    bool is_constant() const { return constant_data.has_value(); }
    
    // Get constant integer value (if applicable)
    std::optional<int64_t> get_constant_int() const {
        if (constant_data && std::holds_alternative<int64_t>(*constant_data)) {
            return std::get<int64_t>(*constant_data);
        }
        return std::nullopt;
    }
    
    // Get constant boolean value (if applicable)
    std::optional<bool> get_constant_bool() const {
        if (constant_data && std::holds_alternative<bool>(*constant_data)) {
            return std::get<bool>(*constant_data);
        }
        return std::nullopt;
    }
};

// IR instruction opcodes
enum class Opcode {
    // Arithmetic
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    Neg,
    
    // Logical
    And,
    Or,
    Not,
    
    // Comparison
    Eq,
    Ne,
    Lt,
    Le,
    Gt,
    Ge,
    
    // Memory
    Alloca,      // Allocate stack memory
    Load,        // Load from memory
    Store,       // Store to memory
    GetField,    // Get struct/class field
    SetField,    // Set struct/class field
    
    // Control flow
    Branch,      // Unconditional branch
    CondBranch,  // Conditional branch
    Return,      // Return from function
    Call,        // Function call
    
    // Constants
    ConstInt,
    ConstFloat,
    ConstBool,
    ConstString,
    ConstNull,
    
    // Type operations
    Cast,        // Type cast
    TypeOf,      // Get type of value
    
    // Special
    Phi,         // SSA phi node
    Nop,         // No operation
    
    // ARC (Automatic Reference Counting) — Req 19
    IntrinsicRetain,   // Increment reference count
    IntrinsicRelease   // Decrement reference count (free if zero)
};

// IR instruction
struct Instruction {
    Opcode opcode;
    std::shared_ptr<Value> result;  // Result value (if any)
    std::vector<std::shared_ptr<Value>> operands;  // Input operands
    std::vector<std::string> metadata;  // Additional metadata

    // Debug info (Req 12B) — source location for this instruction
    std::optional<DILocation> debug_loc;
    
    // For branch instructions
    std::string target_label;
    std::string else_label;
    
    // For constant instructions
    std::variant<int64_t, double, bool, std::string> constant_value;
    
    Instruction() : opcode(Opcode::Nop) {}
    Instruction(Opcode op) : opcode(op) {}
    
    // Helper constructors
    static std::shared_ptr<Instruction> create_binary_op(
        Opcode op,
        std::shared_ptr<Value> result,
        std::shared_ptr<Value> left,
        std::shared_ptr<Value> right
    );
    
    static std::shared_ptr<Instruction> create_unary_op(
        Opcode op,
        std::shared_ptr<Value> result,
        std::shared_ptr<Value> operand
    );
    
    static std::shared_ptr<Instruction> create_const_int(
        std::shared_ptr<Value> result,
        int64_t value
    );
    
    static std::shared_ptr<Instruction> create_const_bool(
        std::shared_ptr<Value> result,
        bool value
    );
    
    static std::shared_ptr<Instruction> create_const_string(
        std::shared_ptr<Value> result,
        std::string value
    );
    
    static std::shared_ptr<Instruction> create_load(
        std::shared_ptr<Value> result,
        std::shared_ptr<Value> address
    );
    
    static std::shared_ptr<Instruction> create_store(
        std::shared_ptr<Value> value,
        std::shared_ptr<Value> address
    );
    
    static std::shared_ptr<Instruction> create_call(
        std::shared_ptr<Value> result,
        std::shared_ptr<Value> function,
        std::vector<std::shared_ptr<Value>> args
    );
    
    static std::shared_ptr<Instruction> create_return(
        std::shared_ptr<Value> value = nullptr
    );
    
    static std::shared_ptr<Instruction> create_branch(
        std::string target
    );
    
    static std::shared_ptr<Instruction> create_cond_branch(
        std::shared_ptr<Value> condition,
        std::string true_target,
        std::string false_target
    );
    
    // ARC intrinsics (Req 19)
    static std::shared_ptr<Instruction> create_intrinsic_retain(
        std::shared_ptr<Value> value
    );
    
    static std::shared_ptr<Instruction> create_intrinsic_release(
        std::shared_ptr<Value> value
    );
    
    // Returns true if this instruction has side effects (cannot be eliminated)
    bool has_side_effects() const;
    
    // Pretty printing
    std::string to_string() const;
};

// Basic block - sequence of instructions with single entry and exit
struct BasicBlock {
    std::string label;
    std::vector<std::shared_ptr<Instruction>> instructions;
    std::vector<std::string> predecessors;  // Labels of predecessor blocks
    std::vector<std::string> successors;    // Labels of successor blocks
    
    BasicBlock() = default;
    explicit BasicBlock(std::string lbl) : label(std::move(lbl)) {}
    
    void add_instruction(std::shared_ptr<Instruction> inst);
    bool is_terminated() const;  // Check if block ends with terminator (return/branch)
    
    std::string to_string() const;
};

// IR function
struct Function {
    std::string name;
    std::vector<std::shared_ptr<Value>> parameters;
    std::shared_ptr<Value> return_value;
    std::vector<std::shared_ptr<BasicBlock>> basic_blocks;
    std::shared_ptr<meta::MetaType> function_type;

    // Debug info (Req 12B) — maps to DWARF DISubprogram
    std::optional<DISubprogram> debug_info;
    
    Function() = default;
    explicit Function(std::string n) : name(std::move(n)) {}
    
    std::shared_ptr<BasicBlock> create_block(const std::string& label);
    std::shared_ptr<BasicBlock> get_block(const std::string& label);
    std::shared_ptr<BasicBlock> entry_block();
    
    std::string to_string() const;
};

/// Compile-unit level debug info.
struct DICompileUnit {
    std::string file;                          // Source file path
    std::string directory;                     // Source directory
    std::string producer = "meld";             // Compiler identifier
    bool is_optimized = false;
};

// IR module - collection of functions and global data
struct Module {
    std::string name;
    std::vector<std::shared_ptr<Function>> functions;
    std::vector<std::shared_ptr<Value>> globals;
    std::map<std::string, std::shared_ptr<meta::MetaType>> types;
    
    explicit Module(std::string n) : name(std::move(n)) {}
    
    // Debug info (Req 12B)
    std::optional<DICompileUnit> compile_unit;
    std::vector<DIType> debug_types;           // Registered debug type descriptors
    bool emit_debug_info = false;              // Whether to emit DWARF/PDB metadata

    std::shared_ptr<Function> create_function(const std::string& name);
    std::shared_ptr<Function> get_function(const std::string& name);
    
    void add_global(std::shared_ptr<Value> global);
    void register_type(const std::string& name, std::shared_ptr<meta::MetaType> type);
    
    std::string to_string() const;
};

// IR Builder - helper for constructing IR
class IRBuilder {
public:
    explicit IRBuilder(std::shared_ptr<Module> mod);
    
    // Function building
    void set_current_function(std::shared_ptr<Function> func);
    void set_insert_point(std::shared_ptr<BasicBlock> block);
    
    // Value creation
    std::shared_ptr<Value> create_temp(ValueType type, std::shared_ptr<meta::MetaType> meta_type = nullptr);
    std::shared_ptr<Value> create_named_value(const std::string& name, ValueType type, std::shared_ptr<meta::MetaType> meta_type = nullptr);
    
    // Instruction building
    std::shared_ptr<Value> build_add(std::shared_ptr<Value> left, std::shared_ptr<Value> right);
    std::shared_ptr<Value> build_sub(std::shared_ptr<Value> left, std::shared_ptr<Value> right);
    std::shared_ptr<Value> build_mul(std::shared_ptr<Value> left, std::shared_ptr<Value> right);
    std::shared_ptr<Value> build_div(std::shared_ptr<Value> left, std::shared_ptr<Value> right);
    std::shared_ptr<Value> build_neg(std::shared_ptr<Value> operand);
    
    std::shared_ptr<Value> build_and(std::shared_ptr<Value> left, std::shared_ptr<Value> right);
    std::shared_ptr<Value> build_or(std::shared_ptr<Value> left, std::shared_ptr<Value> right);
    std::shared_ptr<Value> build_not(std::shared_ptr<Value> operand);
    
    std::shared_ptr<Value> build_eq(std::shared_ptr<Value> left, std::shared_ptr<Value> right);
    std::shared_ptr<Value> build_ne(std::shared_ptr<Value> left, std::shared_ptr<Value> right);
    std::shared_ptr<Value> build_lt(std::shared_ptr<Value> left, std::shared_ptr<Value> right);
    std::shared_ptr<Value> build_le(std::shared_ptr<Value> left, std::shared_ptr<Value> right);
    std::shared_ptr<Value> build_gt(std::shared_ptr<Value> left, std::shared_ptr<Value> right);
    std::shared_ptr<Value> build_ge(std::shared_ptr<Value> left, std::shared_ptr<Value> right);
    
    std::shared_ptr<Value> build_const_int(int64_t value);
    std::shared_ptr<Value> build_const_bool(bool value);
    std::shared_ptr<Value> build_const_string(const std::string& value);
    
    std::shared_ptr<Value> build_alloca(ValueType type, std::shared_ptr<meta::MetaType> meta_type = nullptr);
    std::shared_ptr<Value> build_load(std::shared_ptr<Value> address);
    void build_store(std::shared_ptr<Value> value, std::shared_ptr<Value> address);
    
    std::shared_ptr<Value> build_call(std::shared_ptr<Value> function, std::vector<std::shared_ptr<Value>> args);
    void build_return(std::shared_ptr<Value> value = nullptr);
    void build_branch(const std::string& target);
    void build_cond_branch(std::shared_ptr<Value> condition, const std::string& true_target, const std::string& false_target);
    
private:
    std::shared_ptr<Module> module_;
    std::shared_ptr<Function> current_function_;
    std::shared_ptr<BasicBlock> insert_point_;
    int temp_counter_ = 0;
    
    void insert_instruction(std::shared_ptr<Instruction> inst);
};

} // namespace meld::compiler::ir
