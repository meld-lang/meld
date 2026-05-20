#include "meld/std/ownership_intrinsics.hpp"
#include "meld/std/ownership.hpp"
#include "meld/kernel/primitives.hpp"
#include "meld/compiler/borrow_checker.hpp"
#include "meld/compiler/ownership_metadata.hpp"
#include <format>
#include <chrono>

namespace meld::std_lib::intrinsics {

// Global state for ownership tracking
namespace {
    // Thread-local ownership state
    thread_local std::unordered_map<std::string, compiler::OwnershipInfo> ownership_state;
    thread_local std::unordered_map<std::string, std::vector<compiler::BorrowInfo>> active_borrows;
    thread_local size_t next_lifetime_id = 1;
    
    // Helper to generate unique lifetime IDs
    compiler::LifetimeId create_lifetime(const std::string& name = "") {
        return compiler::LifetimeId(next_lifetime_id++, name);
    }
    
    // Helper to check ownership state
    bool can_move_symbol(const std::string& symbol) {
        auto it = active_borrows.find(symbol);
        return it == active_borrows.end() || it->second.empty();
    }
    
    bool can_borrow_symbol(const std::string& symbol, compiler::BorrowType borrow_type) {
        auto it = active_borrows.find(symbol);
        if (it == active_borrows.end()) {
            return true; // No active borrows
        }
        
        if (borrow_type == compiler::BorrowType::Mutable) {
            return it->second.empty(); // Mutable borrow requires no other borrows
        } else {
            // Immutable borrow allowed if no mutable borrows
            for (const auto& borrow : it->second) {
                if (borrow.borrow_type == compiler::BorrowType::Mutable) {
                    return false;
                }
            }
            return true;
        }
    }
    
    // Helper to track ownership operations
    void track_move(const std::string& symbol) {
        ownership_state[symbol].is_moved = true;
        ownership_state[symbol].is_owned = false;
        active_borrows.erase(symbol); // Clear any borrows
    }
    
    void track_borrow(const std::string& symbol, compiler::BorrowType borrow_type, compiler::LifetimeId lifetime) {
        parser::ast::identifier location;
        location.name = symbol;
        
        compiler::BorrowInfo borrow_info(lifetime, borrow_type, location, symbol);
        active_borrows[symbol].push_back(borrow_info);
    }
    
    void track_drop(const std::string& symbol) {
        ownership_state.erase(symbol);
        active_borrows.erase(symbol);
    }
}

// Core ownership intrinsics implementation

kernel::Value primitive_move(const kernel::Value& value, const std::string& symbol_name) {
    std::string symbol = symbol_name.empty() ? "anonymous" : symbol_name;
    
    // Check if the value can be moved
    if (!can_move_symbol(symbol)) {
        throw_borrow_conflict(symbol, "cannot move borrowed value");
    }
    
    // Check if already moved
    auto it = ownership_state.find(symbol);
    if (it != ownership_state.end() && it->second.is_moved) {
        throw_use_after_move(symbol);
    }
    
    // Track the move operation
    track_move(symbol);
    
    // Create moved value with ownership metadata
    auto result = value;
    const void* obj_ptr = result.get_ptr();
    
    if (obj_ptr) {
        kernel::MetadataStore::instance().set_metadata(obj_ptr, "ownership_status", std::string("moved"));
        kernel::MetadataStore::instance().set_metadata(obj_ptr, "moved_from_symbol", symbol);
        kernel::MetadataStore::instance().set_metadata(obj_ptr, "move_timestamp", 
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
    }
    
    return result;
}

kernel::Value primitive_borrow(const kernel::Value& value, const std::string& symbol_name) {
    std::string symbol = symbol_name.empty() ? "anonymous" : symbol_name;
    
    // Check if the value can be borrowed
    if (!can_borrow_symbol(symbol, compiler::BorrowType::Immutable)) {
        throw_borrow_conflict(symbol, "cannot create immutable borrow while mutable borrow exists");
    }
    
    // Check if already moved
    auto it = ownership_state.find(symbol);
    if (it != ownership_state.end() && it->second.is_moved) {
        throw_use_after_move(symbol);
    }
    
    // Create lifetime for this borrow
    auto lifetime = create_lifetime(symbol + "_immut_borrow");
    
    // Track the borrow operation
    track_borrow(symbol, compiler::BorrowType::Immutable, lifetime);
    
    // Create borrowed value with ownership metadata
    auto result = value;
    const void* obj_ptr = result.get_ptr();
    
    if (obj_ptr) {
        kernel::MetadataStore::instance().set_metadata(obj_ptr, "borrow_status", std::string("borrowed_immut"));
        kernel::MetadataStore::instance().set_metadata(obj_ptr, "borrowed_from_symbol", symbol);
        kernel::MetadataStore::instance().set_metadata(obj_ptr, "lifetime_id", static_cast<int64_t>(lifetime.id));
        kernel::MetadataStore::instance().set_metadata(obj_ptr, "borrow_timestamp", 
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
    }
    
    return result;
}

kernel::Value primitive_borrow_mut(const kernel::Value& value, const std::string& symbol_name) {
    std::string symbol = symbol_name.empty() ? "anonymous" : symbol_name;
    
    // Check if the value can be mutably borrowed
    if (!can_borrow_symbol(symbol, compiler::BorrowType::Mutable)) {
        throw_borrow_conflict(symbol, "cannot create mutable borrow while other borrows exist");
    }
    
    // Check if already moved
    auto it = ownership_state.find(symbol);
    if (it != ownership_state.end() && it->second.is_moved) {
        throw_use_after_move(symbol);
    }
    
    // Create lifetime for this borrow
    auto lifetime = create_lifetime(symbol + "_mut_borrow");
    
    // Track the borrow operation
    track_borrow(symbol, compiler::BorrowType::Mutable, lifetime);
    
    // Create borrowed value with ownership metadata
    auto result = value;
    const void* obj_ptr = result.get_ptr();
    
    if (obj_ptr) {
        kernel::MetadataStore::instance().set_metadata(obj_ptr, "borrow_status", std::string("borrowed_mut"));
        kernel::MetadataStore::instance().set_metadata(obj_ptr, "borrowed_from_symbol", symbol);
        kernel::MetadataStore::instance().set_metadata(obj_ptr, "lifetime_id", static_cast<int64_t>(lifetime.id));
        kernel::MetadataStore::instance().set_metadata(obj_ptr, "borrow_timestamp", 
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
    }
    
    return result;
}

kernel::Value primitive_clone(const kernel::Value& value, const std::string& symbol_name) {
    std::string symbol = symbol_name.empty() ? "anonymous" : symbol_name;
    
    // Check if the type is copyable
    if (!is_copyable_type(value)) {
        throw_type_not_copyable(symbol);
    }
    
    // Create a new owned value (clone)
    auto result = value; // Shallow copy for now - in full implementation would deep copy
    const void* obj_ptr = result.get_ptr();
    
    if (obj_ptr) {
        kernel::MetadataStore::instance().set_metadata(obj_ptr, "ownership_status", std::string("owned"));
        kernel::MetadataStore::instance().set_metadata(obj_ptr, "cloned_from_symbol", symbol);
        kernel::MetadataStore::instance().set_metadata(obj_ptr, "clone_timestamp", 
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        
        // Preserve provenance from original
        auto original_provenance = primitive_get_provenance(value);
        if (original_provenance.has_value()) {
            kernel::MetadataStore::instance().set_metadata(obj_ptr, "provenance_origin", 
                static_cast<int>(original_provenance->origin));
            kernel::MetadataStore::instance().set_metadata(obj_ptr, "provenance_confidence", 
                original_provenance->confidence);
        }
    }
    
    return result;
}

void primitive_drop(const kernel::Value& value, const std::string& symbol_name) {
    std::string symbol = symbol_name.empty() ? "anonymous" : symbol_name;
    
    // Check if the value can be dropped (not borrowed)
    if (!can_move_symbol(symbol)) {
        throw_borrow_conflict(symbol, "cannot drop borrowed value");
    }
    
    // Check if already moved
    auto it = ownership_state.find(symbol);
    if (it != ownership_state.end() && it->second.is_moved) {
        throw_use_after_move(symbol);
    }
    
    // Track the drop operation
    track_drop(symbol);
    
    // Mark as dropped in metadata
    const void* obj_ptr = value.get_ptr();
    if (obj_ptr) {
        kernel::MetadataStore::instance().set_metadata(obj_ptr, "ownership_status", std::string("dropped"));
        kernel::MetadataStore::instance().set_metadata(obj_ptr, "drop_timestamp", 
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
    }
}

// Lifetime management intrinsics

kernel::Value primitive_lifetime_begin(const std::string& lifetime_name) {
    auto lifetime = create_lifetime(lifetime_name);
    
    // Create a lifetime token as a kernel value
    auto lifetime_value = kernel::Value::from_int(static_cast<int64_t>(lifetime.id));
    const void* obj_ptr = lifetime_value.get_ptr();
    
    if (obj_ptr) {
        kernel::MetadataStore::instance().set_metadata(obj_ptr, "is_lifetime_token", true);
        kernel::MetadataStore::instance().set_metadata(obj_ptr, "lifetime_name", lifetime_name);
        kernel::MetadataStore::instance().set_metadata(obj_ptr, "lifetime_begin_timestamp", 
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
    }
    
    return lifetime_value;
}

void primitive_lifetime_end(const kernel::Value& lifetime_token) {
    const void* obj_ptr = lifetime_token.get_ptr();
    if (obj_ptr) {
        kernel::MetadataStore::instance().set_metadata(obj_ptr, "lifetime_end_timestamp", 
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
    }
}

kernel::Value primitive_lifetime_extend(const kernel::Value& shorter_lifetime, 
                                       const kernel::Value& longer_lifetime) {
    // In a full implementation, this would perform lifetime subtyping
    // For now, we just return the longer lifetime
    return longer_lifetime;
}

// Ownership checking intrinsics

bool primitive_is_owned(const kernel::Value& value) {
    const void* obj_ptr = value.get_ptr();
    if (!obj_ptr) return false;
    
    auto status_meta = kernel::MetadataStore::instance().get_metadata(obj_ptr, "ownership_status");
    if (!status_meta.has_value()) return true; // Default to owned
    
    try {
        std::string status = std::any_cast<std::string>(status_meta.value());
        return status == "owned" || status.empty();
    } catch (const std::bad_any_cast&) {
        return true;
    }
}

bool primitive_is_borrowed(const kernel::Value& value) {
    const void* obj_ptr = value.get_ptr();
    if (!obj_ptr) return false;
    
    auto borrow_meta = kernel::MetadataStore::instance().get_metadata(obj_ptr, "borrow_status");
    if (!borrow_meta.has_value()) return false;
    
    try {
        std::string status = std::any_cast<std::string>(borrow_meta.value());
        return status == "borrowed_immut" || status == "borrowed_mut";
    } catch (const std::bad_any_cast&) {
        return false;
    }
}

bool primitive_is_moved(const kernel::Value& value) {
    const void* obj_ptr = value.get_ptr();
    if (!obj_ptr) return false;
    
    auto status_meta = kernel::MetadataStore::instance().get_metadata(obj_ptr, "ownership_status");
    if (!status_meta.has_value()) return false;
    
    try {
        std::string status = std::any_cast<std::string>(status_meta.value());
        return status == "moved";
    } catch (const std::bad_any_cast&) {
        return false;
    }
}

int64_t primitive_borrow_count(const kernel::Value& value) {
    // Count active borrows for this value
    // In a full implementation, this would track all active borrows
    return primitive_is_borrowed(value) ? 1 : 0;
}

std::optional<compiler::LifetimeId> primitive_get_lifetime(const kernel::Value& value) {
    const void* obj_ptr = value.get_ptr();
    if (!obj_ptr) return std::nullopt;
    
    auto lifetime_meta = kernel::MetadataStore::instance().get_metadata(obj_ptr, "lifetime_id");
    if (!lifetime_meta.has_value()) return std::nullopt;
    
    try {
        int64_t lifetime_id = std::any_cast<int64_t>(lifetime_meta.value());
        return compiler::LifetimeId(static_cast<size_t>(lifetime_id));
    } catch (const std::bad_any_cast&) {
        return std::nullopt;
    }
}

// Provenance tracking intrinsics

kernel::Value primitive_mark_ai_generated(const kernel::Value& value, 
                                         double confidence,
                                         const std::string& model_info) {
    const void* obj_ptr = value.get_ptr();
    if (obj_ptr) {
        kernel::MetadataStore::instance().set_metadata(obj_ptr, "provenance_origin", 
            static_cast<int>(provenance::CodeOrigin::Agent));
        kernel::MetadataStore::instance().set_metadata(obj_ptr, "provenance_confidence", confidence);
        if (!model_info.empty()) {
            kernel::MetadataStore::instance().set_metadata(obj_ptr, "provenance_model", model_info);
        }
        kernel::MetadataStore::instance().set_metadata(obj_ptr, "provenance_timestamp", 
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
    }
    
    return value;
}

kernel::Value primitive_mark_human_written(const kernel::Value& value,
                                          const std::string& author_info) {
    const void* obj_ptr = value.get_ptr();
    if (obj_ptr) {
        kernel::MetadataStore::instance().set_metadata(obj_ptr, "provenance_origin", 
            static_cast<int>(provenance::CodeOrigin::Human));
        kernel::MetadataStore::instance().set_metadata(obj_ptr, "provenance_confidence", 1.0);
        if (!author_info.empty()) {
            kernel::MetadataStore::instance().set_metadata(obj_ptr, "provenance_author", author_info);
        }
        kernel::MetadataStore::instance().set_metadata(obj_ptr, "provenance_timestamp", 
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
    }
    
    return value;
}

std::optional<provenance::ProvenanceMetadata> primitive_get_provenance(const kernel::Value& value) {
    const void* obj_ptr = value.get_ptr();
    if (!obj_ptr) return std::nullopt;
    
    auto origin_meta = kernel::MetadataStore::instance().get_metadata(obj_ptr, "provenance_origin");
    auto confidence_meta = kernel::MetadataStore::instance().get_metadata(obj_ptr, "provenance_confidence");
    
    if (!origin_meta.has_value() || !confidence_meta.has_value()) {
        return std::nullopt;
    }
    
    try {
        provenance::ProvenanceMetadata provenance;
        provenance.origin = static_cast<provenance::CodeOrigin>(std::any_cast<int>(origin_meta.value()));
        provenance.confidence = std::any_cast<double>(confidence_meta.value());
        
        auto timestamp_meta = kernel::MetadataStore::instance().get_metadata(obj_ptr, "provenance_timestamp");
        if (timestamp_meta.has_value()) {
            int64_t timestamp_ms = std::any_cast<int64_t>(timestamp_meta.value());
            provenance.timestamp = std::chrono::system_clock::time_point(
                std::chrono::milliseconds(timestamp_ms));
        }
        
        auto model_meta = kernel::MetadataStore::instance().get_metadata(obj_ptr, "provenance_model");
        if (model_meta.has_value()) {
            provenance.source_info = std::any_cast<std::string>(model_meta.value());
        }
        
        auto author_meta = kernel::MetadataStore::instance().get_metadata(obj_ptr, "provenance_author");
        if (author_meta.has_value()) {
            provenance.source_info = std::any_cast<std::string>(author_meta.value());
        }
        
        return provenance;
    } catch (const std::bad_any_cast&) {
        return std::nullopt;
    }
}

bool primitive_is_ai_generated(const kernel::Value& value) {
    auto provenance = primitive_get_provenance(value);
    return provenance.has_value() && provenance->origin == provenance::CodeOrigin::Agent;
}

// Integration with algebraic effects

kernel::Value primitive_suspend_with_move(const std::string& effect_id,
                                         const kernel::Value& value,
                                         const std::string& symbol_name) {
    // Move the value before suspending
    auto moved_value = primitive_move(value, symbol_name);
    
    // Use Meld's primitive_suspend with the moved value
    return kernel::primitive_suspend(effect_id, [moved_value](std::shared_ptr<kernel::Continuation> cont) {
        // The continuation now owns the moved value
        return moved_value;
    });
}

kernel::Value primitive_resume_with_move(const kernel::Value& continuation,
                                        const kernel::Value& value) {
    // Extract continuation and resume with moved value
    auto cont_result = continuation.try_as<kernel::Continuation>();
    if (!cont_result.has_value()) {
        throw std::runtime_error("primitive_resume_with_move: first argument must be Continuation");
    }
    
    return cont_result.value()->resume(value);
}

kernel::Value primitive_handle_with_ownership(const std::string& effect_id,
                                             std::function<kernel::Value()> computation,
                                             std::function<kernel::Value(kernel::Value)> handler) {
    // Set up effect handler with ownership tracking
    kernel::DelimitedContinuation::push_delimiter(effect_id, 
        [handler](kernel::Value v) { return handler(v); });
    
    try {
        auto result = computation();
        kernel::DelimitedContinuation::pop_delimiter();
        return result;
    } catch (...) {
        kernel::DelimitedContinuation::pop_delimiter();
        throw;
    }
}

// Type checking utilities

bool is_ownable_type(const kernel::Value& value) {
    // All Meld kernel types are ownable
    return true;
}

bool is_borrowable_type(const kernel::Value& value) {
    // All Meld kernel types are borrowable
    return true;
}

bool is_copyable_type(const kernel::Value& value) {
    // Check if type has copy semantics
    // For now, assume basic types are copyable
    return value.is<kernel::Integer>() || 
           value.is<kernel::Boolean>() || 
           value.is<kernel::String>() ||
           value.is<kernel::Symbol>();
}

bool is_droppable_type(const kernel::Value& value) {
    // All types are droppable
    return true;
}

// Error handling implementation

void throw_use_after_move(const std::string& symbol) {
    throw OwnershipError(OwnershipError::Type::UseAfterMove,
        std::format("Use of moved value '{}'", symbol), symbol);
}

void throw_borrow_conflict(const std::string& symbol, const std::string& conflict_type) {
    throw OwnershipError(OwnershipError::Type::BorrowConflict,
        std::format("Borrow conflict for '{}': {}", symbol, conflict_type), symbol);
}

void throw_lifetime_violation(const std::string& symbol, const std::string& details) {
    throw OwnershipError(OwnershipError::Type::LifetimeViolation,
        std::format("Lifetime violation for '{}': {}", symbol, details), symbol);
}

void throw_type_not_ownable(const std::string& symbol) {
    throw OwnershipError(OwnershipError::Type::TypeNotOwnable,
        std::format("Type of '{}' is not ownable", symbol), symbol);
}

void throw_type_not_borrowable(const std::string& symbol) {
    throw OwnershipError(OwnershipError::Type::TypeNotBorrowable,
        std::format("Type of '{}' is not borrowable", symbol), symbol);
}

void throw_type_not_copyable(const std::string& symbol) {
    throw OwnershipError(OwnershipError::Type::TypeNotCopyable,
        std::format("Type of '{}' is not copyable", symbol), symbol);
}

} // namespace meld::std_lib::intrinsics