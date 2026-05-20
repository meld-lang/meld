#pragma once

#include "meld/kernel/primitives.hpp"
#include "meld/compiler/borrow_checker.hpp"
#include "meld/std/ownership.hpp"
#include <string>

namespace meld::std_lib::intrinsics {

// Compiler intrinsics for ownership operations
// These functions are recognized by the Meld compiler and integrated with
// the borrow checker for static analysis and optimization

// Move intrinsic - transfers ownership of a value
// This is the fundamental operation for Rust-inspired move semantics
// 
// Usage in Meld:
//   val moved_value = move(original_value)
//   // original_value is now invalid and cannot be used
//
// The compiler will:
// 1. Check that original_value is not currently borrowed
// 2. Mark original_value as moved (invalid for further use)
// 3. Transfer ownership to moved_value
// 4. Track provenance information through the move
kernel::Value primitive_move(const kernel::Value& value, const std::string& symbol_name = "");

// Borrow intrinsic - creates an immutable reference to a value
// This enables safe sharing of data without transferring ownership
//
// Usage in Meld:
//   val borrowed_ref = borrow(owned_value)
//   // owned_value remains valid and can be used
//   // borrowed_ref provides read-only access
//
// The compiler will:
// 1. Check that owned_value is not currently mutably borrowed
// 2. Create a new immutable borrow with appropriate lifetime
// 3. Track the borrow to prevent conflicting mutable borrows
// 4. Ensure the borrow doesn't outlive the owned value
kernel::Value primitive_borrow(const kernel::Value& value, const std::string& symbol_name = "");

// Borrow mutable intrinsic - creates a mutable reference to a value
// This enables safe mutation of data without transferring ownership
//
// Usage in Meld:
//   val mut_ref = borrow_mut(owned_value)
//   // owned_value cannot be accessed while mut_ref exists
//   // mut_ref provides read-write access
//
// The compiler will:
// 1. Check that owned_value is not currently borrowed (mutable or immutable)
// 2. Create a new mutable borrow with appropriate lifetime
// 3. Track the borrow to prevent any other borrows
// 4. Ensure the borrow doesn't outlive the owned value
kernel::Value primitive_borrow_mut(const kernel::Value& value, const std::string& symbol_name = "");

// Clone intrinsic - creates a deep copy of a value (for copyable types)
// This creates a new owned value that is independent of the original
//
// Usage in Meld:
//   val cloned_value = clone(original_value)
//   // Both original_value and cloned_value are valid and independent
//
// The compiler will:
// 1. Check that the type implements the Copy trait
// 2. Create a new owned value with the same data
// 3. Preserve provenance information in the clone
// 4. Track both values independently for ownership
kernel::Value primitive_clone(const kernel::Value& value, const std::string& symbol_name = "");

// Drop intrinsic - explicitly destroys a value and releases resources
// This is usually automatic but can be called explicitly for deterministic cleanup
//
// Usage in Meld:
//   drop(owned_value)
//   // owned_value is now invalid and resources are released
//
// The compiler will:
// 1. Check that owned_value is not currently borrowed
// 2. Mark the value as moved/invalid
// 3. Release any resources held by the value
// 4. Track the destruction for debugging/analysis
void primitive_drop(const kernel::Value& value, const std::string& symbol_name = "");

// Lifetime intrinsics for advanced lifetime management

// Create a new lifetime scope
// This is used internally by the compiler for lifetime inference
kernel::Value primitive_lifetime_begin(const std::string& lifetime_name = "");

// End a lifetime scope
// This is used internally by the compiler for lifetime inference
void primitive_lifetime_end(const kernel::Value& lifetime_token);

// Extend a lifetime to match another lifetime
// This is used for lifetime subtyping and variance
kernel::Value primitive_lifetime_extend(const kernel::Value& shorter_lifetime, 
                                       const kernel::Value& longer_lifetime);

// Ownership checking intrinsics for runtime validation

// Check if a value is currently owned (not moved)
bool primitive_is_owned(const kernel::Value& value);

// Check if a value is currently borrowed
bool primitive_is_borrowed(const kernel::Value& value);

// Check if a value has been moved
bool primitive_is_moved(const kernel::Value& value);

// Get the current borrow count for a value
int64_t primitive_borrow_count(const kernel::Value& value);

// Get the lifetime ID of a borrowed value
std::optional<compiler::LifetimeId> primitive_get_lifetime(const kernel::Value& value);

// Provenance tracking intrinsics for AI-native development

// Mark a value as AI-generated with confidence level
kernel::Value primitive_mark_ai_generated(const kernel::Value& value, 
                                         double confidence,
                                         const std::string& model_info = "");

// Mark a value as human-written
kernel::Value primitive_mark_human_written(const kernel::Value& value,
                                          const std::string& author_info = "");

// Get the provenance information for a value
std::optional<provenance::ProvenanceMetadata> primitive_get_provenance(const kernel::Value& value);

// Check if a value was AI-generated
bool primitive_is_ai_generated(const kernel::Value& value);

// Integration with Meld's algebraic effects system

// Suspend with ownership transfer - for async/await with owned values
kernel::Value primitive_suspend_with_move(const std::string& effect_id,
                                         const kernel::Value& value,
                                         const std::string& symbol_name = "");

// Resume with ownership transfer - for async/await with owned values
kernel::Value primitive_resume_with_move(const kernel::Value& continuation,
                                        const kernel::Value& value);

// Effect handler with ownership tracking
kernel::Value primitive_handle_with_ownership(const std::string& effect_id,
                                             std::function<kernel::Value()> computation,
                                             std::function<kernel::Value(kernel::Value)> handler);

// Utility functions for integration with existing Meld systems

// Convert between ownership wrapper types and kernel values
template<typename T>
kernel::Value to_kernel_value(const Owned<T>& owned);

template<typename T>
kernel::Value to_kernel_value(const Borrowed<T>& borrowed);

template<typename T>
Owned<T> from_kernel_value_owned(const kernel::Value& value);

template<typename T>
Borrowed<T> from_kernel_value_borrowed(const kernel::Value& value);

// Type checking for ownership operations
bool is_ownable_type(const kernel::Value& value);
bool is_borrowable_type(const kernel::Value& value);
bool is_copyable_type(const kernel::Value& value);
bool is_droppable_type(const kernel::Value& value);

// Error handling for ownership violations
class OwnershipError : public std::runtime_error {
public:
    enum class Type {
        UseAfterMove,
        BorrowConflict,
        LifetimeViolation,
        TypeNotOwnable,
        TypeNotBorrowable,
        TypeNotCopyable
    };
    
    OwnershipError(Type type, const std::string& message, const std::string& symbol = "")
        : std::runtime_error(message), type_(type), symbol_(symbol) {}
    
    Type type() const { return type_; }
    const std::string& symbol() const { return symbol_; }
    
private:
    Type type_;
    std::string symbol_;
};

// Throw ownership-specific errors
[[noreturn]] void throw_use_after_move(const std::string& symbol);
[[noreturn]] void throw_borrow_conflict(const std::string& symbol, const std::string& conflict_type);
[[noreturn]] void throw_lifetime_violation(const std::string& symbol, const std::string& details);
[[noreturn]] void throw_type_not_ownable(const std::string& symbol);
[[noreturn]] void throw_type_not_borrowable(const std::string& symbol);
[[noreturn]] void throw_type_not_copyable(const std::string& symbol);

} // namespace meld::std_lib::intrinsics