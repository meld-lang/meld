#include "meld/compiler/ownership_ffi.hpp"
#include <format>
#include <sstream>
#include <algorithm>

namespace meld::compiler {

// ============================================================================
// FFIResourceTracker implementation
// ============================================================================

void FFIResourceTracker::track_resource(const std::string& name,
                                        const std::string& type_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    resources_[name] = TrackedResource(name, type_name);
}

bool FFIResourceTracker::release_resource(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = resources_.find(name);
    if (it == resources_.end() || it->second.released) {
        return false;
    }
    it->second.released = true;
    return true;
}

bool FFIResourceTracker::is_tracked(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = resources_.find(name);
    return it != resources_.end() && !it->second.released;
}

std::vector<TrackedResource> FFIResourceTracker::get_leaked_resources() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<TrackedResource> leaked;
    for (const auto& [_, res] : resources_) {
        if (!res.released) {
            leaked.push_back(res);
        }
    }
    return leaked;
}

std::vector<TrackedResource> FFIResourceTracker::get_all_resources() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<TrackedResource> all;
    all.reserve(resources_.size());
    for (const auto& [_, res] : resources_) {
        all.push_back(res);
    }
    return all;
}

size_t FFIResourceTracker::release_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t count = 0;
    for (auto& [_, res] : resources_) {
        if (!res.released) {
            res.released = true;
            ++count;
        }
    }
    return count;
}

size_t FFIResourceTracker::active_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t count = 0;
    for (const auto& [_, res] : resources_) {
        if (!res.released) {
            ++count;
        }
    }
    return count;
}

void FFIResourceTracker::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    resources_.clear();
}

// ============================================================================
// OwnershipAwareFFI implementation
// ============================================================================

OwnershipAwareFFI::OwnershipAwareFFI()
    : borrow_checker_(nullptr) {}

OwnershipAwareFFI::OwnershipAwareFFI(BorrowChecker& borrow_checker)
    : borrow_checker_(&borrow_checker) {}

void OwnershipAwareFFI::declare_extern_function(const FFIFunctionDecl& decl) {
    declarations_[decl.name] = decl;
}

bool OwnershipAwareFFI::is_declared(const std::string& function_name) const {
    return declarations_.contains(function_name);
}

std::expected<FFIFunctionDecl, std::string>
OwnershipAwareFFI::get_declaration(const std::string& function_name) const {
    auto it = declarations_.find(function_name);
    if (it == declarations_.end()) {
        return std::unexpected(
            std::format("FFI function '{}' has not been declared", function_name));
    }
    return it->second;
}

size_t OwnershipAwareFFI::declared_function_count() const {
    return declarations_.size();
}

FFIResourceTracker& OwnershipAwareFFI::resource_tracker() {
    return resource_tracker_;
}

const FFIResourceTracker& OwnershipAwareFFI::resource_tracker() const {
    return resource_tracker_;
}

BorrowChecker* OwnershipAwareFFI::borrow_checker() {
    return borrow_checker_;
}

const BorrowChecker* OwnershipAwareFFI::borrow_checker() const {
    return borrow_checker_;
}

// ---------------------------------------------------------------------------
// validate_ffi_call — pre-call ownership validation
// ---------------------------------------------------------------------------

std::expected<void, FFIValidationError>
OwnershipAwareFFI::validate_ffi_call(
    const std::string& function_name,
    const std::vector<std::string>& arg_symbols) const {

    // 1. Check function is declared
    auto it = declarations_.find(function_name);
    if (it == declarations_.end()) {
        return std::unexpected(FFIValidationError(
            FFIValidationError::Kind::UndeclaredFunction,
            std::format("FFI function '{}' has not been declared", function_name)));
    }

    const auto& decl = it->second;

    // 2. Check argument count matches parameter count
    if (arg_symbols.size() != decl.params.size()) {
        return std::unexpected(FFIValidationError(
            FFIValidationError::Kind::InvalidOwnership,
            std::format("FFI function '{}' expects {} arguments, got {}",
                        function_name, decl.params.size(), arg_symbols.size())));
    }

    // 3. Validate each argument against its parameter's ownership annotation
    for (size_t i = 0; i < decl.params.size(); ++i) {
        auto result = validate_param_ownership(decl.params[i], arg_symbols[i]);
        if (!result.has_value()) {
            return result;
        }
    }

    return {};
}

std::expected<void, FFIValidationError>
OwnershipAwareFFI::validate_param_ownership(
    const FFIParamDecl& param,
    const std::string& arg_symbol) const {

    if (!borrow_checker_) {
        // Without a borrow checker we can only do structural validation
        return {};
    }

    auto ownership = borrow_checker_->get_ownership_info(arg_symbol);

    // If the borrow checker has ownership info, validate against it
    if (ownership.has_value()) {
        // Reject moved values for any ownership annotation
        if (ownership->is_moved) {
            return std::unexpected(FFIValidationError(
                FFIValidationError::Kind::MovedValue,
                std::format("Cannot pass '{}' to FFI parameter '{}': value has been moved",
                            arg_symbol, param.name),
                param.name));
        }

        // For mutable borrow parameters, check no other borrows exist
        if (param.ownership == FFIOwnership::BorrowedMut) {
            if (borrow_checker_->is_borrowed(arg_symbol)) {
                auto borrows = borrow_checker_->get_active_borrows(arg_symbol);
                if (!borrows.empty()) {
                    return std::unexpected(FFIValidationError(
                        FFIValidationError::Kind::BorrowConflict,
                        std::format("Cannot pass '{}' as mutable borrow to FFI parameter '{}': "
                                    "active borrows exist",
                                    arg_symbol, param.name),
                        param.name));
                }
            }
        }

        // For immutable borrow parameters, check no mutable borrows exist
        if (param.ownership == FFIOwnership::Borrowed) {
            if (borrow_checker_->is_borrowed(arg_symbol)) {
                auto borrows = borrow_checker_->get_active_borrows(arg_symbol);
                for (const auto& b : borrows) {
                    if (b.borrow_type == BorrowType::Mutable) {
                        return std::unexpected(FFIValidationError(
                            FFIValidationError::Kind::MutableBorrowConflict,
                            std::format("Cannot pass '{}' as immutable borrow to FFI parameter '{}': "
                                        "a mutable borrow is active",
                                        arg_symbol, param.name),
                            param.name));
                    }
                }
            }
        }
    }

    return {};
}

// ---------------------------------------------------------------------------
// wrap_ffi_call — generate wrapper code with ownership tracking
// ---------------------------------------------------------------------------

std::expected<FFIWrapperResult, std::string>
OwnershipAwareFFI::wrap_ffi_call(
    const std::string& function_name,
    const std::vector<std::string>& arg_symbols) const {

    auto it = declarations_.find(function_name);
    if (it == declarations_.end()) {
        return std::unexpected(
            std::format("FFI function '{}' has not been declared", function_name));
    }

    const auto& decl = it->second;

    if (arg_symbols.size() != decl.params.size()) {
        return std::unexpected(
            std::format("FFI function '{}' expects {} arguments, got {}",
                        function_name, decl.params.size(), arg_symbols.size()));
    }

    FFIWrapperResult result;
    result.pre_call_code = generate_pre_call_validation(decl, arg_symbols);
    result.post_call_code = generate_post_call_tracking(decl);
    result.error_boundary = generate_error_boundary(decl);

    // Build the call expression
    std::ostringstream call;
    call << "native_call(\"" << function_name << "\", {";
    for (size_t i = 0; i < arg_symbols.size(); ++i) {
        if (i > 0) call << ", ";
        call << arg_symbols[i];
    }
    call << "})";
    result.call_code = call.str();

    // Track resources for owned return values
    if (decl.return_ownership == FFIOwnership::Owned &&
        !decl.return_type.empty() && decl.return_type != "void") {
        result.tracked_resources.push_back(
            std::format("__ffi_result_{}", function_name));
    }

    // Track resources for owned parameters (ownership transferred to native)
    for (size_t i = 0; i < decl.params.size(); ++i) {
        if (decl.params[i].ownership == FFIOwnership::Owned) {
            result.tracked_resources.push_back(arg_symbols[i]);
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// Code generation helpers
// ---------------------------------------------------------------------------

std::string OwnershipAwareFFI::generate_pre_call_validation(
    const FFIFunctionDecl& decl,
    const std::vector<std::string>& arg_symbols) const {

    std::ostringstream code;
    code << "// Pre-call ownership validation for " << decl.name << "\n";

    for (size_t i = 0; i < decl.params.size(); ++i) {
        const auto& param = decl.params[i];
        const auto& sym = arg_symbols[i];

        switch (param.ownership) {
            case FFIOwnership::Owned:
                code << "meld::ownership::assert_not_moved(" << sym << ");\n";
                code << "meld::ownership::transfer_to_native(" << sym << ");\n";
                break;
            case FFIOwnership::Borrowed:
                code << "meld::ownership::assert_not_moved(" << sym << ");\n";
                code << "meld::ownership::assert_no_mutable_borrow(" << sym << ");\n";
                break;
            case FFIOwnership::BorrowedMut:
                code << "meld::ownership::assert_not_moved(" << sym << ");\n";
                code << "meld::ownership::assert_no_borrows(" << sym << ");\n";
                break;
        }
    }

    return code.str();
}

std::string OwnershipAwareFFI::generate_post_call_tracking(
    const FFIFunctionDecl& decl) const {

    std::ostringstream code;
    code << "// Post-call resource tracking for " << decl.name << "\n";

    // Track returned resource as owned by Meld
    if (decl.return_ownership == FFIOwnership::Owned &&
        !decl.return_type.empty() && decl.return_type != "void") {
        code << "meld::ownership::track_native_resource(__ffi_result_"
             << decl.name << ", \"" << decl.return_type << "\");\n";
    }

    return code.str();
}

std::string OwnershipAwareFFI::generate_error_boundary(
    const FFIFunctionDecl& decl) const {

    std::ostringstream code;
    code << "// Error boundary for " << decl.name << "\n";
    code << "try {\n";
    code << "    auto __ffi_result_" << decl.name << " = ";
    code << "native_call(\"" << decl.name << "\", args);\n";
    code << "    return Result::ok(__ffi_result_" << decl.name << ");\n";
    code << "} catch (const std::exception& e) {\n";
    code << "    return Result::err(std::string(e.what()));\n";
    code << "} catch (...) {\n";
    code << "    return Result::err(std::string(\"Unknown native exception in "
         << decl.name << "\"));\n";
    code << "}\n";

    return code.str();
}

} // namespace meld::compiler
