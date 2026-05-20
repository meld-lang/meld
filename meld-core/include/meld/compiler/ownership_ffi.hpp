#pragma once

#include "meld/compiler/borrow_checker.hpp"
#include "meld/compiler/ownership_metadata.hpp"
#include "meld/kernel/primitives.hpp"
#include <string>
#include <vector>
#include <map>
#include <expected>
#include <functional>
#include <chrono>
#include <mutex>

namespace meld::compiler {

// Ownership annotation for FFI function parameters
enum class FFIOwnership {
    Owned,        // Caller transfers ownership to native code
    Borrowed,     // Caller lends an immutable reference
    BorrowedMut   // Caller lends a mutable reference
};

// Describes a single parameter in an extern function declaration
struct FFIParamDecl {
    std::string name;
    std::string type_name;
    FFIOwnership ownership;

    FFIParamDecl() = default;
    FFIParamDecl(std::string n, std::string t, FFIOwnership o)
        : name(std::move(n)), type_name(std::move(t)), ownership(o) {}
};

// Describes an extern function registered with the ownership-aware FFI
struct FFIFunctionDecl {
    std::string name;
    std::vector<FFIParamDecl> params;
    std::string return_type;
    FFIOwnership return_ownership;  // Ownership of the returned value

    FFIFunctionDecl() : return_ownership(FFIOwnership::Owned) {}
};

// Validation error produced when an FFI call violates ownership rules
struct FFIValidationError {
    enum class Kind {
        MovedValue,           // Argument was already moved
        BorrowConflict,       // Mutable borrow while immutable borrows exist (or vice-versa)
        MutableBorrowConflict,// Multiple mutable borrows
        InvalidOwnership,     // Ownership annotation mismatch
        UndeclaredFunction    // Function not registered
    };

    Kind kind;
    std::string message;
    std::string param_name;   // Which parameter triggered the error

    FFIValidationError() = default;
    FFIValidationError(Kind k, std::string msg, std::string param = "")
        : kind(k), message(std::move(msg)), param_name(std::move(param)) {}
};

// Result of wrapping an FFI call — contains generated wrapper code
struct FFIWrapperResult {
    std::string pre_call_code;    // Validation / setup before native call
    std::string call_code;        // The actual native invocation
    std::string post_call_code;   // Resource tracking after native call
    std::string error_boundary;   // Exception-to-Result conversion code
    std::vector<std::string> tracked_resources;  // Resources to track

    FFIWrapperResult() = default;
};

// ============================================================================
// FFIResourceTracker — tracks resources allocated by native code
// ============================================================================

struct TrackedResource {
    std::string name;
    std::string type_name;
    std::chrono::steady_clock::time_point allocated_at;
    bool released;

    TrackedResource() : released(false) {}
    TrackedResource(std::string n, std::string t)
        : name(std::move(n)), type_name(std::move(t)),
          allocated_at(std::chrono::steady_clock::now()), released(false) {}
};

class FFIResourceTracker {
public:
    FFIResourceTracker() = default;

    // Register a resource allocated by native code
    void track_resource(const std::string& name, const std::string& type_name);

    // Mark a resource as released / cleaned up
    bool release_resource(const std::string& name);

    // Check if a resource is currently tracked and not released
    bool is_tracked(const std::string& name) const;

    // Get all resources that have not been released (potential leaks)
    std::vector<TrackedResource> get_leaked_resources() const;

    // Get all tracked resources (including released ones)
    std::vector<TrackedResource> get_all_resources() const;

    // Release all tracked resources (cleanup)
    size_t release_all();

    // Get count of active (non-released) resources
    size_t active_count() const;

    // Clear all tracking data
    void clear();

private:
    std::map<std::string, TrackedResource> resources_;
    mutable std::mutex mutex_;
};

// ============================================================================
// OwnershipAwareFFI — wraps FFI calls with ownership tracking
// ============================================================================

class OwnershipAwareFFI {
public:
    OwnershipAwareFFI();
    explicit OwnershipAwareFFI(BorrowChecker& borrow_checker);

    // Register an extern function with ownership annotations on parameters
    void declare_extern_function(const FFIFunctionDecl& decl);

    // Check if a function has been declared
    bool is_declared(const std::string& function_name) const;

    // Get the declaration for a function
    std::expected<FFIFunctionDecl, std::string> get_declaration(
        const std::string& function_name) const;

    // Validate that ownership rules are respected before calling native code
    std::expected<void, FFIValidationError> validate_ffi_call(
        const std::string& function_name,
        const std::vector<std::string>& arg_symbols) const;

    // Generate wrapper code that tracks resource ownership across the FFI boundary
    std::expected<FFIWrapperResult, std::string> wrap_ffi_call(
        const std::string& function_name,
        const std::vector<std::string>& arg_symbols) const;

    // Access the resource tracker
    FFIResourceTracker& resource_tracker();
    const FFIResourceTracker& resource_tracker() const;

    // Access the borrow checker (if one was provided)
    BorrowChecker* borrow_checker();
    const BorrowChecker* borrow_checker() const;

    // Get count of declared functions
    size_t declared_function_count() const;

private:
    std::map<std::string, FFIFunctionDecl> declarations_;
    FFIResourceTracker resource_tracker_;
    BorrowChecker* borrow_checker_;  // Non-owning; may be null

    // Internal helpers
    std::expected<void, FFIValidationError> validate_param_ownership(
        const FFIParamDecl& param,
        const std::string& arg_symbol) const;

    std::string generate_pre_call_validation(
        const FFIFunctionDecl& decl,
        const std::vector<std::string>& arg_symbols) const;

    std::string generate_post_call_tracking(
        const FFIFunctionDecl& decl) const;

    std::string generate_error_boundary(
        const FFIFunctionDecl& decl) const;
};

} // namespace meld::compiler
