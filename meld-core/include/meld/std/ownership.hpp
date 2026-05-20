#pragma once

#include "meld/kernel/primitives.hpp"
#include "meld/compiler/borrow_checker.hpp"
#include "meld/compiler/ownership_metadata.hpp"
#include "meld/provenance/provenance.hpp"
#include <memory>
#include <optional>
#include <type_traits>
#include <concepts>

namespace meld::std_lib {

// Forward declarations
template<typename T> class Owned;
template<typename T> class Borrowed;

// Concept for types that can be owned
template<typename T>
concept Ownable = requires {
    typename T;
    // Type must be movable and destructible
    std::is_move_constructible_v<T>;
    std::is_destructible_v<T>;
};

// Concept for types that can be borrowed
template<typename T>
concept Borrowable = requires {
    typename T;
    // Type must be copyable for immutable borrows or movable for mutable borrows
    std::is_copy_constructible_v<T> || std::is_move_constructible_v<T>;
};

// Ownership information attached to values
struct OwnershipInfo {
    bool is_owned = true;
    bool is_moved = false;
    bool is_copyable = false;
    std::optional<compiler::LifetimeId> lifetime;
    provenance::ProvenanceMetadata provenance;
    
    OwnershipInfo() = default;
    
    OwnershipInfo(bool owned, bool copyable = false)
        : is_owned(owned), is_copyable(copyable) {}
    
    // Create ownership info for moved value
    static OwnershipInfo create_moved(const std::string& symbol) {
        OwnershipInfo info(false, false);
        info.is_moved = true;
        return info;
    }
    
    // Create ownership info for borrowed value
    static OwnershipInfo create_borrowed(compiler::BorrowType borrow_type,
                                        compiler::LifetimeId lifetime) {
        OwnershipInfo info(false, false);
        info.lifetime = lifetime;
        return info;
    }
};

// Owned<T> - Represents exclusive ownership of a value
// This is a zero-cost wrapper that provides compile-time ownership tracking
template<Ownable T>
class Owned {
public:
    // Constructors
    explicit Owned(T value) 
        : value_(std::move(value))
        , ownership_info_(true, std::is_copy_constructible_v<T>) {
        // Track provenance if available
        track_creation_provenance();
    }
    
    // Move constructor (transfers ownership)
    Owned(Owned&& other) noexcept 
        : value_(std::move(other.value_))
        , ownership_info_(std::move(other.ownership_info_)) {
        // Mark the source as moved
        other.ownership_info_.is_moved = true;
        other.ownership_info_.is_owned = false;
    }
    
    // Copy constructor (only available for copyable types)
    Owned(const Owned& other) requires std::is_copy_constructible_v<T>
        : value_(other.value_)
        , ownership_info_(other.ownership_info_) {
        // Both copies own their values
        ownership_info_.is_owned = true;
        ownership_info_.is_moved = false;
    }
    
    // Assignment operators
    Owned& operator=(Owned&& other) noexcept {
        if (this != &other) {
            value_ = std::move(other.value_);
            ownership_info_ = std::move(other.ownership_info_);
            
            // Mark the source as moved
            other.ownership_info_.is_moved = true;
            other.ownership_info_.is_owned = false;
        }
        return *this;
    }
    
    Owned& operator=(const Owned& other) requires std::is_copy_constructible_v<T> {
        if (this != &other) {
            value_ = other.value_;
            ownership_info_ = other.ownership_info_;
            ownership_info_.is_owned = true;
            ownership_info_.is_moved = false;
        }
        return *this;
    }
    
    // Destructor
    ~Owned() {
        // Automatic cleanup when owner goes out of scope
        if (ownership_info_.is_owned && !ownership_info_.is_moved) {
            // Value is automatically destroyed by C++ RAII
            track_destruction_provenance();
        }
    }
    
    // Access methods
    T& get() & {
        check_not_moved();
        return value_;
    }
    
    const T& get() const & {
        check_not_moved();
        return value_;
    }
    
    T&& get() && {
        check_not_moved();
        ownership_info_.is_moved = true;
        ownership_info_.is_owned = false;
        return std::move(value_);
    }
    
    // Dereference operators
    T& operator*() & { return get(); }
    const T& operator*() const & { return get(); }
    T&& operator*() && { return std::move(*this).get(); }
    
    T* operator->() { 
        check_not_moved();
        return &value_; 
    }
    
    const T* operator->() const { 
        check_not_moved();
        return &value_; 
    }
    
    // Ownership queries
    bool is_owned() const { return ownership_info_.is_owned; }
    bool is_moved() const { return ownership_info_.is_moved; }
    bool is_copyable() const { return ownership_info_.is_copyable; }
    
    // Get ownership information
    const OwnershipInfo& ownership_info() const { return ownership_info_; }
    
    // Move the value out (consumes the Owned<T>)
    T into_inner() && {
        check_not_moved();
        ownership_info_.is_moved = true;
        ownership_info_.is_owned = false;
        return std::move(value_);
    }
    
    // Create a borrowed reference (immutable)
    Borrowed<T> borrow() const & requires Borrowable<T> {
        check_not_moved();
        auto lifetime = create_lifetime();
        return Borrowed<T>(*this, compiler::BorrowType::Immutable, lifetime);
    }
    
    // Create a mutable borrowed reference
    Borrowed<T> borrow_mut() & requires Borrowable<T> {
        check_not_moved();
        auto lifetime = create_lifetime();
        return Borrowed<T>(*this, compiler::BorrowType::Mutable, lifetime);
    }
    
    // Integration with Meld kernel primitives
    kernel::Value to_meld_value() const {
        // Convert to appropriate Meld kernel type
        if constexpr (std::is_same_v<T, int64_t>) {
            return kernel::Value::from_int(value_);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return kernel::Value::from_string(value_);
        } else if constexpr (std::is_same_v<T, bool>) {
            return kernel::Value::from_bool(value_);
        } else {
            // For complex types, we'd need more sophisticated conversion
            // For now, convert to string representation
            return kernel::Value::from_string(std::to_string(reinterpret_cast<uintptr_t>(&value_)));
        }
    }
    
    // Create from Meld kernel value
    static Owned<T> from_meld_value(const kernel::Value& value) {
        if constexpr (std::is_same_v<T, int64_t>) {
            return Owned<T>(value.as_int());
        } else if constexpr (std::is_same_v<T, std::string>) {
            return Owned<T>(value.as_string());
        } else if constexpr (std::is_same_v<T, bool>) {
            return Owned<T>(value.as_bool());
        } else {
            static_assert(std::is_same_v<T, void>, "Unsupported type for Meld value conversion");
        }
    }
    
private:
    T value_;
    OwnershipInfo ownership_info_;
    
    void check_not_moved() const {
        if (ownership_info_.is_moved) {
            throw std::runtime_error("Use of moved value");
        }
    }
    
    compiler::LifetimeId create_lifetime() const {
        // In a full implementation, this would integrate with the borrow checker
        // to create proper lifetime identifiers
        static size_t next_id = 1;
        return compiler::LifetimeId(next_id++, "owned_borrow");
    }
    
    void track_creation_provenance() {
        // Track provenance information for AI-generated code
        // This integrates with Meld's provenance tracking system
        ownership_info_.provenance.origin = provenance::CodeOrigin::Human; // Default
        ownership_info_.provenance.confidence = 1.0;
        ownership_info_.provenance.timestamp = std::chrono::system_clock::now();
    }
    
    void track_destruction_provenance() {
        // Track when owned values are destroyed for debugging/analysis
        // This could integrate with Meld's flight recorder system
    }
};

// Borrowed<T> - Represents a borrowed reference to a value
// This provides compile-time borrow checking and lifetime tracking
template<Borrowable T>
class Borrowed {
public:
    // Constructor (should only be called by Owned<T>::borrow methods)
    template<typename U>
    Borrowed(const Owned<U>& owner, compiler::BorrowType borrow_type, compiler::LifetimeId lifetime)
        requires std::is_same_v<T, U>
        : ref_(get_reference(owner, borrow_type))
        , borrow_type_(borrow_type)
        , lifetime_(lifetime)
        , is_valid_(true) {
        track_borrow_creation(owner);
    }
    
    // Copy constructor (for immutable borrows only)
    Borrowed(const Borrowed& other) 
        requires (std::is_copy_constructible_v<T>)
        : ref_(other.ref_)
        , borrow_type_(other.borrow_type_)
        , lifetime_(other.lifetime_)
        , is_valid_(other.is_valid_) {
        
        // Can only copy immutable borrows
        if (borrow_type_ == compiler::BorrowType::Mutable) {
            throw std::runtime_error("Cannot copy mutable borrow");
        }
    }
    
    // Move constructor
    Borrowed(Borrowed&& other) noexcept
        : ref_(other.ref_)
        , borrow_type_(other.borrow_type_)
        , lifetime_(other.lifetime_)
        , is_valid_(other.is_valid_) {
        other.is_valid_ = false;
    }
    
    // Assignment operators
    Borrowed& operator=(const Borrowed& other) {
        if (this != &other) {
            if (borrow_type_ == compiler::BorrowType::Mutable || 
                other.borrow_type_ == compiler::BorrowType::Mutable) {
                throw std::runtime_error("Cannot assign mutable borrows");
            }
            
            ref_ = other.ref_;
            borrow_type_ = other.borrow_type_;
            lifetime_ = other.lifetime_;
            is_valid_ = other.is_valid_;
        }
        return *this;
    }
    
    Borrowed& operator=(Borrowed&& other) noexcept {
        if (this != &other) {
            ref_ = other.ref_;
            borrow_type_ = other.borrow_type_;
            lifetime_ = other.lifetime_;
            is_valid_ = other.is_valid_;
            other.is_valid_ = false;
        }
        return *this;
    }
    
    // Destructor
    ~Borrowed() {
        if (is_valid_) {
            track_borrow_destruction();
        }
    }
    
    // Access methods
    const T& get() const {
        check_valid();
        return ref_;
    }
    
    T& get() requires (std::is_same_v<compiler::BorrowType, decltype(compiler::BorrowType::Mutable)>) {
        check_valid();
        if (borrow_type_ != compiler::BorrowType::Mutable) {
            throw std::runtime_error("Cannot get mutable reference from immutable borrow");
        }
        return const_cast<T&>(ref_);
    }
    
    // Dereference operators
    const T& operator*() const { return get(); }
    T& operator*() requires (std::is_same_v<compiler::BorrowType, decltype(compiler::BorrowType::Mutable)>) { 
        return get(); 
    }
    
    const T* operator->() const { 
        check_valid();
        return &ref_; 
    }
    
    T* operator->() requires (std::is_same_v<compiler::BorrowType, decltype(compiler::BorrowType::Mutable)>) {
        check_valid();
        if (borrow_type_ != compiler::BorrowType::Mutable) {
            throw std::runtime_error("Cannot get mutable pointer from immutable borrow");
        }
        return const_cast<T*>(&ref_);
    }
    
    // Borrow information
    compiler::BorrowType borrow_type() const { return borrow_type_; }
    compiler::LifetimeId lifetime() const { return lifetime_; }
    bool is_valid() const { return is_valid_; }
    bool is_mutable() const { return borrow_type_ == compiler::BorrowType::Mutable; }
    
    // Convert to immutable borrow (if currently mutable)
    Borrowed<T> as_immutable() const {
        check_valid();
        if (borrow_type_ == compiler::BorrowType::Immutable) {
            return *this;
        }
        
        // Create new immutable borrow with same lifetime
        return Borrowed<T>(ref_, compiler::BorrowType::Immutable, lifetime_);
    }
    
    // Integration with Meld kernel primitives
    kernel::Value to_meld_value() const {
        check_valid();
        
        // Convert the referenced value to Meld kernel type
        if constexpr (std::is_same_v<T, int64_t>) {
            return kernel::Value::from_int(ref_);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return kernel::Value::from_string(ref_);
        } else if constexpr (std::is_same_v<T, bool>) {
            return kernel::Value::from_bool(ref_);
        } else {
            // For complex types, convert to string representation
            return kernel::Value::from_string(std::to_string(reinterpret_cast<uintptr_t>(&ref_)));
        }
    }
    
private:
    const T& ref_;
    compiler::BorrowType borrow_type_;
    compiler::LifetimeId lifetime_;
    bool is_valid_;
    
    // Private constructor for internal use
    Borrowed(const T& ref, compiler::BorrowType borrow_type, compiler::LifetimeId lifetime)
        : ref_(ref), borrow_type_(borrow_type), lifetime_(lifetime), is_valid_(true) {}
    
    template<typename U>
    static const T& get_reference(const Owned<U>& owner, compiler::BorrowType borrow_type) {
        if (borrow_type == compiler::BorrowType::Mutable) {
            // For mutable borrows, we need non-const access
            return const_cast<const T&>(const_cast<Owned<U>&>(owner).get());
        } else {
            return owner.get();
        }
    }
    
    void check_valid() const {
        if (!is_valid_) {
            throw std::runtime_error("Use of invalid borrow");
        }
    }
    
    template<typename U>
    void track_borrow_creation(const Owned<U>& owner) {
        // Track borrow creation for provenance and debugging
        // This integrates with Meld's borrow checker and metadata system
    }
    
    void track_borrow_destruction() {
        // Track when borrows are destroyed for lifetime analysis
    }
    
    friend class Owned<T>;
};

// Utility functions for working with ownership

// Move function - explicitly transfers ownership
template<Ownable T>
Owned<T> move_value(Owned<T>&& owned) {
    return std::move(owned);
}

// Borrow function - creates immutable borrow
template<Borrowable T>
Borrowed<T> borrow(const Owned<T>& owned) {
    return owned.borrow();
}

// Borrow mutable function - creates mutable borrow
template<Borrowable T>
Borrowed<T> borrow_mut(Owned<T>& owned) {
    return owned.borrow_mut();
}

// Clone function - creates a copy (only for copyable types)
template<Ownable T>
Owned<T> clone(const Owned<T>& owned) requires std::is_copy_constructible_v<T> {
    return Owned<T>(owned.get());
}

// Factory functions for creating owned values
template<typename T>
Owned<T> make_owned(T&& value) {
    return Owned<T>(std::forward<T>(value));
}

template<typename T, typename... Args>
Owned<T> make_owned(Args&&... args) {
    return Owned<T>(T(std::forward<Args>(args)...));
}

// Integration with Meld compiler intrinsics
namespace intrinsics {

// Compiler intrinsic for move operations
template<Ownable T>
Owned<T> move_intrinsic(Owned<T>&& value) {
    // This would be recognized by the Meld compiler for optimization
    // and borrow checking integration
    return std::move(value);
}

// Compiler intrinsic for borrow operations
template<Borrowable T>
Borrowed<T> borrow_intrinsic(const Owned<T>& value) {
    // This would be recognized by the Meld compiler for borrow checking
    return value.borrow();
}

// Compiler intrinsic for mutable borrow operations
template<Borrowable T>
Borrowed<T> borrow_mut_intrinsic(Owned<T>& value) {
    // This would be recognized by the Meld compiler for borrow checking
    return value.borrow_mut();
}

} // namespace intrinsics

} // namespace meld::std_lib