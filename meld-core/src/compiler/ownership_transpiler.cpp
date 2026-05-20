#include "meld/compiler/ownership_transpiler.hpp"
#include <sstream>

namespace meld::compiler {

OwnershipTranspiler::OwnershipTranspiler(OwnershipTarget target)
    : target_(target) {}

// ---------------------------------------------------------------------------
// Type mapping
// ---------------------------------------------------------------------------

std::string OwnershipTranspiler::map_owned_type(const std::string& inner_type) const {
    switch (target_) {
        case OwnershipTarget::Cpp:
            return "std::unique_ptr<" + inner_type + ">";
        case OwnershipTarget::Java:
            return "@Owned " + inner_type;
        case OwnershipTarget::Rust:
            return inner_type; // Rust owns by default
        case OwnershipTarget::Wasm:
            return "i32"; // linear memory pointer
    }
    return inner_type;
}

std::string OwnershipTranspiler::map_borrowed_type(const std::string& inner_type,
                                                    BorrowType borrow_type) const {
    switch (target_) {
        case OwnershipTarget::Cpp:
            return (borrow_type == BorrowType::Mutable)
                       ? inner_type + "&"
                       : "const " + inner_type + "&";
        case OwnershipTarget::Java:
            return (borrow_type == BorrowType::Mutable)
                       ? "@BorrowedMut " + inner_type
                       : "@Borrowed " + inner_type;
        case OwnershipTarget::Rust:
            return (borrow_type == BorrowType::Mutable)
                       ? "&mut " + inner_type
                       : "&" + inner_type;
        case OwnershipTarget::Wasm:
            // pointer with lifetime metadata word
            return "i32"; // pointer into linear memory
    }
    return inner_type;
}

OwnershipTypeMapping OwnershipTranspiler::get_type_mapping(const std::string& inner_type) const {
    return OwnershipTypeMapping(
        map_owned_type(inner_type),
        map_borrowed_type(inner_type, BorrowType::Immutable),
        map_borrowed_type(inner_type, BorrowType::Mutable),
        "", // filled by emit helpers
        ""  // filled by emit helpers
    );
}

// ---------------------------------------------------------------------------
// Ownership transfer (move)
// ---------------------------------------------------------------------------

TranspileResult OwnershipTranspiler::emit_ownership_transfer(
    const std::string& source_var, const std::string& dest_var,
    const std::string& type_name) const {

    switch (target_) {
        case OwnershipTarget::Cpp:  return emit_cpp_transfer(source_var, dest_var, type_name);
        case OwnershipTarget::Java: return emit_java_transfer(source_var, dest_var, type_name);
        case OwnershipTarget::Rust: return emit_rust_transfer(source_var, dest_var, type_name);
        case OwnershipTarget::Wasm: return emit_wasm_transfer(source_var, dest_var, type_name);
    }
    return TranspileResult{};
}

// ---------------------------------------------------------------------------
// Drop / cleanup
// ---------------------------------------------------------------------------

TranspileResult OwnershipTranspiler::emit_drop(const std::string& var_name,
                                                const std::string& type_name) const {
    switch (target_) {
        case OwnershipTarget::Cpp:  return emit_cpp_drop(var_name, type_name);
        case OwnershipTarget::Java: return emit_java_drop(var_name, type_name);
        case OwnershipTarget::Rust: return emit_rust_drop(var_name, type_name);
        case OwnershipTarget::Wasm: return emit_wasm_drop(var_name, type_name);
    }
    return TranspileResult{};
}

// ---------------------------------------------------------------------------
// Runtime safety checks
// ---------------------------------------------------------------------------

bool OwnershipTranspiler::needs_runtime_safety_checks() const {
    return target_ == OwnershipTarget::Java || target_ == OwnershipTarget::Wasm;
}

TranspileResult OwnershipTranspiler::emit_use_after_move_check(
    const std::string& var_name) const {

    TranspileResult result;
    if (!needs_runtime_safety_checks()) {
        // C++ and Rust enforce this at compile time
        result.code = "// use-after-move checked at compile time";
        return result;
    }

    if (target_ == OwnershipTarget::Java) {
        std::ostringstream oss;
        oss << "if (" << var_name << " == null) {\n"
            << "    throw new IllegalStateException(\"Use of moved value: " << var_name << "\");\n"
            << "}";
        result.code = oss.str();
        result.safety_checks.push_back("use_after_move:" + var_name);
    } else if (target_ == OwnershipTarget::Wasm) {
        std::ostringstream oss;
        oss << "local.get $" << var_name << "\n"
            << "i32.eqz\n"
            << "if\n"
            << "  call $__meld_trap_use_after_move\n"
            << "end";
        result.code = oss.str();
        result.safety_checks.push_back("use_after_move:" + var_name);
    }
    return result;
}

TranspileResult OwnershipTranspiler::emit_borrow_check(
    const std::string& var_name, BorrowType borrow_type) const {

    TranspileResult result;
    if (!needs_runtime_safety_checks()) {
        result.code = "// borrow validity checked at compile time";
        return result;
    }

    std::string borrow_kind = (borrow_type == BorrowType::Mutable) ? "mutable" : "immutable";

    if (target_ == OwnershipTarget::Java) {
        std::ostringstream oss;
        oss << "MeldOwnership.checkBorrow(" << var_name << ", \""
            << borrow_kind << "\");";
        result.code = oss.str();
        result.required_imports.push_back("meld.runtime.MeldOwnership");
        result.safety_checks.push_back("borrow_check:" + var_name + ":" + borrow_kind);
    } else if (target_ == OwnershipTarget::Wasm) {
        std::ostringstream oss;
        oss << "local.get $" << var_name << "\n"
            << "i32.const " << (borrow_type == BorrowType::Mutable ? "1" : "0") << "\n"
            << "call $__meld_check_borrow";
        result.code = oss.str();
        result.safety_checks.push_back("borrow_check:" + var_name + ":" + borrow_kind);
    }
    return result;
}

// ---------------------------------------------------------------------------
// Required imports
// ---------------------------------------------------------------------------

std::vector<std::string> OwnershipTranspiler::get_required_imports() const {
    switch (target_) {
        case OwnershipTarget::Cpp:
            return {"<memory>", "<utility>"};
        case OwnershipTarget::Java:
            return {"meld.runtime.Owned", "meld.runtime.Borrowed",
                    "meld.runtime.BorrowedMut", "meld.runtime.MeldOwnership"};
        case OwnershipTarget::Rust:
            return {}; // ownership is built-in
        case OwnershipTarget::Wasm:
            return {}; // imports declared in module header
    }
    return {};
}


// ---------------------------------------------------------------------------
// C++ target helpers
// ---------------------------------------------------------------------------

TranspileResult OwnershipTranspiler::emit_cpp_transfer(
    const std::string& source, const std::string& dest,
    const std::string& type) const {

    TranspileResult result;
    std::ostringstream oss;
    oss << "std::unique_ptr<" << type << "> " << dest
        << " = std::move(" << source << ");";
    result.code = oss.str();
    result.required_imports.push_back("<memory>");
    result.required_imports.push_back("<utility>");
    return result;
}

TranspileResult OwnershipTranspiler::emit_cpp_drop(
    const std::string& var, const std::string& type) const {

    TranspileResult result;
    // C++ uses RAII — unique_ptr destructor handles cleanup automatically.
    // We emit an explicit reset for clarity / when early drop is needed.
    std::ostringstream oss;
    oss << var << ".reset(); // drop " << type;
    result.code = oss.str();
    return result;
}

// ---------------------------------------------------------------------------
// Java target helpers
// ---------------------------------------------------------------------------

TranspileResult OwnershipTranspiler::emit_java_transfer(
    const std::string& source, const std::string& dest,
    const std::string& type) const {

    TranspileResult result;
    std::ostringstream oss;
    // Java move: assign to dest, null out source
    oss << type << " " << dest << " = " << source << ";\n"
        << source << " = null; // ownership transferred";
    result.code = oss.str();
    result.safety_checks.push_back("null_on_move:" + source);
    return result;
}

TranspileResult OwnershipTranspiler::emit_java_drop(
    const std::string& var, const std::string& type) const {

    TranspileResult result;
    std::ostringstream oss;
    // Java: use try-finally to ensure cleanup
    oss << "try {\n"
        << "    if (" << var << " instanceof AutoCloseable) {\n"
        << "        ((AutoCloseable) " << var << ").close();\n"
        << "    }\n"
        << "} finally {\n"
        << "    " << var << " = null; // drop " << type << "\n"
        << "}";
    result.code = oss.str();
    return result;
}

// ---------------------------------------------------------------------------
// Rust target helpers
// ---------------------------------------------------------------------------

TranspileResult OwnershipTranspiler::emit_rust_transfer(
    const std::string& source, const std::string& dest,
    const std::string& type) const {

    TranspileResult result;
    std::ostringstream oss;
    // Rust move is the natural assignment — the compiler invalidates source
    oss << "let " << dest << ": " << type << " = " << source << ";";
    result.code = oss.str();
    return result;
}

TranspileResult OwnershipTranspiler::emit_rust_drop(
    const std::string& var, const std::string& /*type*/) const {

    TranspileResult result;
    std::ostringstream oss;
    // Rust: explicit drop via std::mem::drop
    oss << "drop(" << var << ");";
    result.code = oss.str();
    return result;
}

// ---------------------------------------------------------------------------
// WebAssembly target helpers
// ---------------------------------------------------------------------------

TranspileResult OwnershipTranspiler::emit_wasm_transfer(
    const std::string& source, const std::string& dest,
    const std::string& /*type*/) const {

    TranspileResult result;
    std::ostringstream oss;
    // Wasm: copy the pointer, zero out the source to prevent double-free
    oss << "local.get $" << source << "\n"
        << "local.set $" << dest << "\n"
        << "i32.const 0\n"
        << "local.set $" << source << " ;; ownership transferred";
    result.code = oss.str();
    result.safety_checks.push_back("null_on_move:" + source);
    return result;
}

TranspileResult OwnershipTranspiler::emit_wasm_drop(
    const std::string& var, const std::string& /*type*/) const {

    TranspileResult result;
    std::ostringstream oss;
    // Wasm: free the linear memory allocation
    oss << "local.get $" << var << "\n"
        << "call $free\n"
        << "i32.const 0\n"
        << "local.set $" << var << " ;; dropped";
    result.code = oss.str();
    return result;
}

} // namespace meld::compiler
