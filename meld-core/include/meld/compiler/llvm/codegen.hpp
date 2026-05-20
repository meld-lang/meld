#pragma once
#include "meld/parser/ast.hpp"
#include <string>
#include <vector>
#include <sstream>

namespace meld::compiler::llvm_backend {

// LLVM IR text emitter — generates LLVM IR as text without LLVM library dependency.
// When LLVM is linked, this can be replaced with proper IRBuilder usage.
class LLVMCodegen {
public:
    LLVMCodegen();

    // Emit LLVM IR for a complete program
    std::string emit_program(const std::vector<parser::ast::expression>& ast);

    // Emit a single function definition
    void emit_function(const parser::ast::function_definition& fn);

    // Emit a println call (maps to puts/printf)
    void emit_println(const std::string& value);

    // Get the generated IR
    std::string get_ir() const { return ir_.str(); }

private:
    std::ostringstream ir_;
    int string_counter_ = 0;
    std::vector<std::string> string_constants_;

    void emit_prelude();
    void emit_string_constants();
    std::string next_string_id();
};

} // namespace meld::compiler::llvm_backend
