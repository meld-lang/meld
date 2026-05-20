#include "meld/std/ownership.hpp"
#include "meld/compiler/borrow_checker.hpp"
#include "meld/compiler/ownership_metadata.hpp"
#include "meld/kernel/primitives.hpp"
#include <stdexcept>
#include <format>

namespace meld::std_lib {

// Global ownership tracking integration
namespace {
    // Get the global borrow checker instance for integration
    compiler::BorrowChecker& get_borrow_checker() {
        static compiler::BorrowChecker instance;
        return instance;
    }
    
    // Get the global ownership metadata manager
    compiler::OwnershipMetadataManager& get_ownership_metadata() {
        static compiler::OwnershipMetadataManager instance;
        return instance;
    }
}

// Integration functions for Meld compiler intrinsics

// Register move operation with borrow checker
void register_move_operation(const std::string& symbol, const void* obj_ptr) {
    // Create a dummy identifier for the move operation
    parser::ast::identifier location;
    location.name = symbol;
    
    // Track the move in the ownership metadata system
    get_ownership_metadata().track_move(symbol, location);
    
    // Set metadata on the kernel object
    if (obj_ptr) {
        kernel::MetadataStore::instance().set_metadata(
            obj_ptr, 
            "ownership_status", 
            std::string("moved")
        );
    }
}

// Register borrow operation with borrow checker
void register_borrow_operation(const std::string& symbol, 
                              compiler::BorrowType borrow_type,
                              compiler::LifetimeId lifetime,
                              const void* obj_ptr) {
    // Create a dummy identifier for the borrow operation
    parser::ast::identifier location;
    location.name = symbol;
    
    // Track the borrow in the ownership metadata system
    get_ownership_metadata().track_borrow(symbol, borrow_type, lifetime, location);
    
    // Set metadata on the kernel object
    if (obj_ptr) {
        std::string borrow_status = (borrow_type == compiler::BorrowType::Mutable) 
            ? "borrowed_mut" : "borrowed_immut";
        
        kernel::MetadataStore::instance().set_metadata(
            obj_ptr, 
            "borrow_status", 
            borrow_status
        );
        
        kernel::MetadataStore::instance().set_metadata(
            obj_ptr, 
            "lifetime_id", 
            static_cast<int64_t>(lifetime.id)
        );
    }
}

// Check if a value can be moved (no active borrows)
bool can_move_value(const std::string& symbol) {
    return !get_ownership_metadata().get_identifier_metadata(symbol).has_value() ||
           !get_ownership_metadata().get_identifier_metadata(symbol)->is_borrowed;
}

// Check if a value can be borrowed
bool can_borrow_value(const std::string& symbol, compiler::BorrowType borrow_type) {
    auto metadata = get_ownership_metadata().get_identifier_metadata(symbol);
    if (!metadata.has_value()) {
        return true; // No existing metadata, can borrow
    }
    
    if (metadata->is_moved) {
        return false; // Cannot borrow moved value
    }
    
    if (borrow_type == compiler::BorrowType::Mutable) {
        // Mutable borrow requires no other borrows
        return !metadata->is_borrowed;
    } else {
        // Immutable borrow allowed if no mutable borrows exist
        return !metadata->is_borrowed || 
               (metadata->borrow_type && 
                *metadata->borrow_type == compiler::BorrowType::Immutable);
    }
}

// Utility functions for provenance tracking

void track_ownership_provenance(const void* obj_ptr, 
                               const provenance::ProvenanceMetadata& provenance) {
    if (obj_ptr) {
        // Store provenance information in kernel metadata
        kernel::MetadataStore::instance().set_metadata(
            obj_ptr, 
            "provenance_origin", 
            static_cast<int>(provenance.origin)
        );
        
        kernel::MetadataStore::instance().set_metadata(
            obj_ptr, 
            "provenance_confidence", 
            provenance.confidence
        );
        
        if (!provenance.source_info.empty()) {
            kernel::MetadataStore::instance().set_metadata(
                obj_ptr, 
                "provenance_source", 
                provenance.source_info
            );
        }
    }
}

std::optional<provenance::ProvenanceMetadata> get_ownership_provenance(const void* obj_ptr) {
    if (!obj_ptr) {
        return std::nullopt;
    }
    
    auto origin_meta = kernel::MetadataStore::instance().get_metadata(obj_ptr, "provenance_origin");
    auto confidence_meta = kernel::MetadataStore::instance().get_metadata(obj_ptr, "provenance_confidence");
    
    if (!origin_meta.has_value() || !confidence_meta.has_value()) {
        return std::nullopt;
    }
    
    try {
        provenance::ProvenanceMetadata provenance;
        provenance.origin = static_cast<provenance::CodeOrigin>(std::any_cast<int>(origin_meta.value()));
        provenance.confidence = std::any_cast<double>(confidence_meta.value());
        
        auto source_meta = kernel::MetadataStore::instance().get_metadata(obj_ptr, "provenance_source");
        if (source_meta.has_value()) {
            provenance.source_info = std::any_cast<std::string>(source_meta.value());
        }
        
        return provenance;
    } catch (const std::bad_any_cast&) {
        return std::nullopt;
    }
}

// Integration with Meld kernel primitives for ownership tracking

// Create a Meld kernel value with ownership metadata
kernel::Value create_owned_kernel_value(const kernel::Value& value, const OwnershipInfo& ownership) {
    // Attach ownership metadata to the kernel value
    auto result = value;
    const void* obj_ptr = result.get_ptr();
    
    if (obj_ptr) {
        // Set ownership status
        kernel::MetadataStore::instance().set_metadata(
            obj_ptr, 
            "is_owned", 
            ownership.is_owned
        );
        
        kernel::MetadataStore::instance().set_metadata(
            obj_ptr, 
            "is_moved", 
            ownership.is_moved
        );
        
        kernel::MetadataStore::instance().set_metadata(
            obj_ptr, 
            "is_copyable", 
            ownership.is_copyable
        );
        
        // Set lifetime if available
        if (ownership.lifetime.has_value()) {
            kernel::MetadataStore::instance().set_metadata(
                obj_ptr, 
                "lifetime_id", 
                static_cast<int64_t>(ownership.lifetime->id)
            );
        }
        
        // Set provenance information
        track_ownership_provenance(obj_ptr, ownership.provenance);
    }
    
    return result;
}

// Extract ownership metadata from a Meld kernel value
std::optional<OwnershipInfo> extract_ownership_info(const kernel::Value& value) {
    const void* obj_ptr = value.get_ptr();
    if (!obj_ptr) {
        return std::nullopt;
    }
    
    auto is_owned_meta = kernel::MetadataStore::instance().get_metadata(obj_ptr, "is_owned");
    auto is_moved_meta = kernel::MetadataStore::instance().get_metadata(obj_ptr, "is_moved");
    auto is_copyable_meta = kernel::MetadataStore::instance().get_metadata(obj_ptr, "is_copyable");
    
    if (!is_owned_meta.has_value()) {
        return std::nullopt; // No ownership metadata
    }
    
    try {
        OwnershipInfo info;
        info.is_owned = std::any_cast<bool>(is_owned_meta.value());
        
        if (is_moved_meta.has_value()) {
            info.is_moved = std::any_cast<bool>(is_moved_meta.value());
        }
        
        if (is_copyable_meta.has_value()) {
            info.is_copyable = std::any_cast<bool>(is_copyable_meta.value());
        }
        
        // Extract lifetime if available
        auto lifetime_meta = kernel::MetadataStore::instance().get_metadata(obj_ptr, "lifetime_id");
        if (lifetime_meta.has_value()) {
            auto lifetime_id = std::any_cast<int64_t>(lifetime_meta.value());
            info.lifetime = compiler::LifetimeId(static_cast<size_t>(lifetime_id));
        }
        
        // Extract provenance if available
        auto provenance = get_ownership_provenance(obj_ptr);
        if (provenance.has_value()) {
            info.provenance = *provenance;
        }
        
        return info;
    } catch (const std::bad_any_cast&) {
        return std::nullopt;
    }
}

// Compiler intrinsics implementation

namespace intrinsics {

// Move intrinsic - integrates with borrow checker
kernel::Value move_value_intrinsic(const kernel::Value& value, const std::string& symbol) {
    // Check if the value can be moved
    if (!can_move_value(symbol)) {
        throw std::runtime_error(std::format("Cannot move '{}': value is currently borrowed", symbol));
    }
    
    // Register the move operation
    register_move_operation(symbol, value.get_ptr());
    
    // Create the moved value with updated ownership metadata
    auto ownership_info = extract_ownership_info(value);
    if (ownership_info.has_value()) {
        ownership_info->is_moved = true;
        ownership_info->is_owned = false;
        return create_owned_kernel_value(value, *ownership_info);
    }
    
    // If no existing ownership info, create new moved metadata
    OwnershipInfo new_info = OwnershipInfo::create_moved(symbol);
    return create_owned_kernel_value(value, new_info);
}

// Borrow intrinsic - integrates with borrow checker
kernel::Value borrow_value_intrinsic(const kernel::Value& value, 
                                    const std::string& symbol,
                                    compiler::BorrowType borrow_type) {
    // Check if the value can be borrowed
    if (!can_borrow_value(symbol, borrow_type)) {
        std::string borrow_type_str = (borrow_type == compiler::BorrowType::Mutable) ? "mutable" : "immutable";
        throw std::runtime_error(std::format("Cannot create {} borrow of '{}': conflicting borrow exists", 
                                           borrow_type_str, symbol));
    }
    
    // Create a new lifetime for this borrow
    static size_t next_lifetime_id = 1;
    compiler::LifetimeId lifetime(next_lifetime_id++, symbol + "_borrow");
    
    // Register the borrow operation
    register_borrow_operation(symbol, borrow_type, lifetime, value.get_ptr());
    
    // Create the borrowed value with updated ownership metadata
    auto ownership_info = extract_ownership_info(value);
    if (!ownership_info.has_value()) {
        ownership_info = OwnershipInfo(true, false); // Default ownership info
    }
    
    // Update ownership info for borrow
    OwnershipInfo borrow_info = OwnershipInfo::create_borrowed(borrow_type, lifetime);
    borrow_info.provenance = ownership_info->provenance; // Preserve provenance
    
    return create_owned_kernel_value(value, borrow_info);
}

// Clone intrinsic - creates a copy with new ownership
kernel::Value clone_value_intrinsic(const kernel::Value& value, const std::string& symbol) {
    auto ownership_info = extract_ownership_info(value);
    
    // Check if the value is copyable
    if (ownership_info.has_value() && !ownership_info->is_copyable) {
        throw std::runtime_error(std::format("Cannot clone '{}': value is not copyable", symbol));
    }
    
    // Create new ownership info for the clone
    OwnershipInfo clone_info(true, true); // Owned and copyable
    if (ownership_info.has_value()) {
        clone_info.provenance = ownership_info->provenance; // Preserve provenance
    }
    
    return create_owned_kernel_value(value, clone_info);
}

} // namespace intrinsics

// Standard library type implementations for common Meld types

// Owned<int64_t> specialization for Meld integers
template<>
kernel::Value Owned<int64_t>::to_meld_value() const {
    auto kernel_value = kernel::Value::from_int(value_);
    return create_owned_kernel_value(kernel_value, ownership_info_);
}

template<>
Owned<int64_t> Owned<int64_t>::from_meld_value(const kernel::Value& value) {
    auto int_value = value.as_int();
    Owned<int64_t> owned(int_value);
    
    // Extract and apply ownership metadata if available
    auto ownership_info = extract_ownership_info(value);
    if (ownership_info.has_value()) {
        owned.ownership_info_ = *ownership_info;
    }
    
    return owned;
}

// Owned<std::string> specialization for Meld strings
template<>
kernel::Value Owned<std::string>::to_meld_value() const {
    auto kernel_value = kernel::Value::from_string(value_);
    return create_owned_kernel_value(kernel_value, ownership_info_);
}

template<>
Owned<std::string> Owned<std::string>::from_meld_value(const kernel::Value& value) {
    auto string_value = value.as_string();
    Owned<std::string> owned(string_value);
    
    // Extract and apply ownership metadata if available
    auto ownership_info = extract_ownership_info(value);
    if (ownership_info.has_value()) {
        owned.ownership_info_ = *ownership_info;
    }
    
    return owned;
}

// Owned<bool> specialization for Meld booleans
template<>
kernel::Value Owned<bool>::to_meld_value() const {
    auto kernel_value = kernel::Value::from_bool(value_);
    return create_owned_kernel_value(kernel_value, ownership_info_);
}

template<>
Owned<bool> Owned<bool>::from_meld_value(const kernel::Value& value) {
    auto bool_value = value.as_bool();
    Owned<bool> owned(bool_value);
    
    // Extract and apply ownership metadata if available
    auto ownership_info = extract_ownership_info(value);
    if (ownership_info.has_value()) {
        owned.ownership_info_ = *ownership_info;
    }
    
    return owned;
}

} // namespace meld::std_lib