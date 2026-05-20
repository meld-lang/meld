#pragma once

#include "meld/compiler/borrow_checker.hpp"
#include "meld/compiler/ownership_metadata.hpp"
#include <string>
#include <map>
#include <vector>
#include <expected>

namespace meld::compiler {

// Target languages for ownership transpilation
enum class OwnershipTarget {
    Cpp,
    Java,
    Rust,
    Wasm
};

// Represents a transpiled ownership type mapping
struct OwnershipTypeMapping {
    std::string owned_type;      // How Owned<T> maps to target
    std::string borrowed_type;   // How Borrowed<T> (immutable) maps
    std::string borrowed_mut_type; // How Borrowed<T> (mutable) maps
    std::string move_expr;       // How move is expressed
    std::string drop_expr;       // How drop/cleanup is expressed

    OwnershipTypeMapping() = default;
    OwnershipTypeMapping(std::string owned, std::string borrowed, std::string borrowed_mut,
                         std::string move_e, std::string drop_e)
        : owned_type(std::move(owned)), borrowed_type(std::move(borrowed)),
          borrowed_mut_type(std::move(borrowed_mut)), move_expr(std::move(move_e)),
          drop_expr(std::move(drop_e)) {}
};

// Result of a transpilation operation
struct TranspileResult {
    std::string code;            // Generated code
    std::vector<std::string> required_imports; // Imports needed
    std::vector<std::string> safety_checks;    // Runtime safety checks generated

    TranspileResult() = default;
    explicit TranspileResult(std::string c) : code(std::move(c)) {}
};

// Transpiles Meld ownership concepts to target language representations
class OwnershipTranspiler {
public:
    explicit OwnershipTranspiler(OwnershipTarget target);

    // Get the current target language
    OwnershipTarget target() const { return target_; }

    // Map an Owned<T> type to the target language representation
    std::string map_owned_type(const std::string& inner_type) const;

    // Map a Borrowed<T> type to the target language representation
    std::string map_borrowed_type(const std::string& inner_type, BorrowType borrow_type) const;

    // Emit code for an ownership transfer (move)
    TranspileResult emit_ownership_transfer(const std::string& source_var,
                                            const std::string& dest_var,
                                            const std::string& type_name) const;

    // Emit code for resource cleanup (drop)
    TranspileResult emit_drop(const std::string& var_name,
                              const std::string& type_name) const;

    // Emit a runtime safety check for use-after-move
    TranspileResult emit_use_after_move_check(const std::string& var_name) const;

    // Emit a runtime safety check for borrow validity
    TranspileResult emit_borrow_check(const std::string& var_name,
                                      BorrowType borrow_type) const;

    // Get the full type mapping for the current target
    OwnershipTypeMapping get_type_mapping(const std::string& inner_type) const;

    // Check if the target language needs runtime safety checks
    // (Java and Wasm need them; C++ and Rust have compile-time guarantees)
    bool needs_runtime_safety_checks() const;

    // Get required imports/headers for ownership support in the target
    std::vector<std::string> get_required_imports() const;

private:
    OwnershipTarget target_;

    // Target-specific emission helpers
    TranspileResult emit_cpp_transfer(const std::string& source, const std::string& dest,
                                      const std::string& type) const;
    TranspileResult emit_java_transfer(const std::string& source, const std::string& dest,
                                       const std::string& type) const;
    TranspileResult emit_rust_transfer(const std::string& source, const std::string& dest,
                                       const std::string& type) const;
    TranspileResult emit_wasm_transfer(const std::string& source, const std::string& dest,
                                       const std::string& type) const;

    TranspileResult emit_cpp_drop(const std::string& var, const std::string& type) const;
    TranspileResult emit_java_drop(const std::string& var, const std::string& type) const;
    TranspileResult emit_rust_drop(const std::string& var, const std::string& type) const;
    TranspileResult emit_wasm_drop(const std::string& var, const std::string& type) const;
};

} // namespace meld::compiler
