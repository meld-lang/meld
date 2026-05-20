#pragma once

/// @file mem.hpp
/// @brief std.mem module — Hold[T], View[T], Storable trait, link(), move()
///
/// This header defines the Meld language-level memory management types that
/// sit on top of the C++ ManagedObject/WeakRef infrastructure. These are
/// library types, not language keywords. The compiler discovers them through
/// @intrinsic annotations on the Storable trait.
///
/// - Hold[T]  → strong owning reference (backs to MeldRef<T> / shared_ptr<T>)
/// - View[T] → non-owning observation reference (backs to WeakRef<T>)
/// - Storable trait → @intrinsic(memory_strategy) marker
/// - link()  → create View[T] from Hold[T]
/// - move()  → zero-cost ownership transfer, @intrinsic(memory_move)

#include "meld/types/memory.hpp"
#include <memory>
#include <optional>
#include <string>
#include <type_traits>

namespace meld::std_mem {

// ---------------------------------------------------------------------------
// Intrinsic annotation tags (compile-time markers)
// ---------------------------------------------------------------------------

/// Intrinsic annotation types used by the Semantic Analyzer to discover
/// memory management types without hardcoding names.
struct IntrinsicTag {
    /// Marks the Storable trait as the memory management strategy contract.
    static constexpr const char* memory_strategy = "memory_strategy";

    /// Marks collection types that require Storable elements.
    static constexpr const char* managed_container = "managed_container";

    /// Marks std.mem.move() as the ownership transfer intrinsic.
    static constexpr const char* memory_move = "memory_move";
};

// ---------------------------------------------------------------------------
// Storable trait — @intrinsic(memory_strategy)
// ---------------------------------------------------------------------------

/// The Storable trait is the compiler's entry point for memory management.
/// Annotated with @intrinsic(memory_strategy), it declares the contract that
/// Hold[T] and View[T] implement. The compiler uses is_owning() to determine
/// iterator behavior and access enforcement.
///
/// Meld surface:
///   @intrinsic(memory_strategy)
///   trt Storable {
///       fnc access() -> optional[ptr]
///       fnc is_owning() -> bool
///   }
class Storable {
public:
    virtual ~Storable() = default;

    /// Returns a pointer to the managed object, or nullopt if the object
    /// is dead (View[T] to a destroyed object).
    virtual std::optional<void*> access() const = 0;

    /// Returns true for Hold[T] (strong ownership), false for View[T]
    /// (non-owning observation). Drives iterator behavior and access rules.
    virtual bool is_owning() const = 0;

    /// The intrinsic annotation identifier for this trait.
    static constexpr const char* intrinsic_annotation() {
        return IntrinsicTag::memory_strategy;
    }
};

// Forward declaration
template<typename T> class Link;

// ---------------------------------------------------------------------------
// Hold[T] — Strong ownership reference
// ---------------------------------------------------------------------------

/// Strong owning reference. Guarantees the referenced object is non-null
/// and alive. Lowers to MeldRef<T> (shared_ptr<T>) in the C++ backend.
///
/// Meld surface:
///   class Hold[T] { ... }
///   impl[T] Storable for Hold[T] { is_owning() -> true }
template<typename T>
class Own : public Storable {
    static_assert(std::is_base_of_v<types::ManagedObject, T>,
                  "Hold[T] requires T to derive from ManagedObject");

public:
    /// Construct from a shared_ptr (the MeldRef<T> equivalent).
    explicit Own(std::shared_ptr<T> ptr) : ptr_(std::move(ptr)) {}

    /// Move constructor — pointer transfer, no retain/release.
    Own(Own&& other) noexcept : ptr_(std::move(other.ptr_)) {}

    /// Copy constructor — increments strong_count (retain).
    Own(const Own& other) : ptr_(other.ptr_) {}

    /// Move assignment — pointer transfer.
    Own& operator=(Own&& other) noexcept {
        if (this != &other) {
            ptr_ = std::move(other.ptr_);
        }
        return *this;
    }

    /// Copy assignment — retain new, release old.
    Own& operator=(const Own& other) {
        if (this != &other) {
            ptr_ = other.ptr_;
        }
        return *this;
    }

    ~Own() override = default;

    /// Direct member access — no nil check needed. The object is guaranteed
    /// alive while any Hold[T] exists.
    T& get() const { return *ptr_; }

    /// Dereference operators for ergonomic C++ usage.
    T* operator->() const { return ptr_.get(); }
    T& operator*() const { return *ptr_; }

    /// Create a non-owning observation reference (View[T]).
    /// Increments weak_count without touching strong_count.
    Link<T> link() const;

    /// Access the underlying shared_ptr (MeldRef<T>).
    const std::shared_ptr<T>& raw() const { return ptr_; }

    /// Check if the reference is valid (non-null).
    explicit operator bool() const { return ptr_ != nullptr; }

    // -- Storable trait implementation --

    /// Returns pointer to the managed object (always valid for Hold[T]).
    std::optional<void*> access() const override {
        if (ptr_) {
            return static_cast<void*>(ptr_.get());
        }
        return std::nullopt;
    }

    /// Hold[T] is an owning reference — always returns true.
    bool is_owning() const override { return true; }

    /// Returns the strong reference count of the managed object.
    /// Required by the MeldStorable C++20 concept (mirrors Storable trait).
    uint64_t get_ref_count() const {
        if (ptr_) {
            return static_cast<uint64_t>(ptr_.use_count());
        }
        return 0;
    }

private:
    std::shared_ptr<T> ptr_;

    // Allow move() to access internals for zero-cost transfer.
    template<typename U>
    friend Own<U> mem_move(Own<U>& source);
};

// ---------------------------------------------------------------------------
// View[T] — Non-owning observation reference
// ---------------------------------------------------------------------------

/// Non-owning observation reference. Does not prevent deallocation.
/// Lowers to WeakRef<T> (weak_ptr<T>) in the C++ backend.
/// Must be upgraded via `if val` or `match` before access.
///
/// Meld surface:
///   class View[T] { ... }
///   impl[T] Storable for View[T] { is_owning() -> false }
template<typename T>
class Link : public Storable {
    static_assert(std::is_base_of_v<types::ManagedObject, T>,
                  "View[T] requires T to derive from ManagedObject");

public:
    Link() = default;

    /// Construct from a WeakRef<T>.
    explicit Link(types::WeakRef<T> weak) : weak_(std::move(weak)) {}

    /// Construct from a shared_ptr (creates the weak reference).
    explicit Link(const std::shared_ptr<T>& ptr) : weak_(ptr) {}

    Link(const Link&) = default;
    Link(Link&&) noexcept = default;
    Link& operator=(const Link&) = default;
    Link& operator=(Link&&) noexcept = default;
    ~Link() override = default;

    /// Upgrade to a temporary strong reference. Used internally by
    /// `if val` / `match` syntax. Returns nullopt if the object is dead.
    ///
    /// Meld surface:
    ///   if val x = link_ref { ... } else { ... }
    ///   // lowers to: if (auto x = link_ref.lock()) { ... }
    std::optional<std::shared_ptr<T>> upgrade() const {
        auto locked = weak_.lock();
        if (locked) {
            return locked;
        }
        return std::nullopt;
    }

    /// Check if the referenced object has been destroyed.
    bool expired() const { return weak_.expired(); }

    /// Access the underlying WeakRef<T>.
    const types::WeakRef<T>& raw() const { return weak_; }

    // -- Storable trait implementation --

    /// Returns pointer if the object is alive, nullopt if dead.
    std::optional<void*> access() const override {
        auto locked = weak_.lock();
        if (locked) {
            return static_cast<void*>(locked.get());
        }
        return std::nullopt;
    }

    /// View[T] is a non-owning reference — always returns false.
    bool is_owning() const override { return false; }

    /// Returns the reference count of the observed object (0 if dead).
    /// Required by the MeldStorable C++20 concept (mirrors Storable trait).
    uint64_t get_ref_count() const {
        auto locked = weak_.lock();
        if (locked) {
            return static_cast<uint64_t>(locked.use_count() - 1); // subtract temporary lock
        }
        return 0;
    }

private:
    types::WeakRef<T> weak_;
};

// ---------------------------------------------------------------------------
// Hold[T]::link() implementation (needs Link<T> to be complete)
// ---------------------------------------------------------------------------

template<typename T>
Link<T> Own<T>::link() const {
    return Link<T>(ptr_);
}

// ---------------------------------------------------------------------------
// std.mem.link() — Create View[T] from Hold[T]
// ---------------------------------------------------------------------------

/// Create a View[T] from an Hold[T] reference.
/// Increments weak_count without touching strong_count.
///
/// Meld surface:
///   fnc link[T](owner: Hold[T]) -> View[T] { ... }
template<typename T>
Link<T> link(const Own<T>& owner) {
    return owner.link();
}

// ---------------------------------------------------------------------------
// std.mem.move() — Zero-cost ownership transfer
// ---------------------------------------------------------------------------

/// Transfer ownership without touching the reference count.
/// The source binding is invalidated (set to null). The compiler's move
/// tracking pass statically prevents any subsequent use of the source.
///
/// Annotated with @intrinsic(memory_move).
///
/// Meld surface:
///   @intrinsic(memory_move)
///   fnc move[T](source: Hold[T]) -> Hold[T] { ... }
///
/// Lowers to C++ move semantics — pointer transfer, no retain/release.
template<typename T>
Own<T> mem_move(Own<T>& source) {
    return Own<T>(std::move(source));
}

/// Intrinsic annotation accessor for move().
inline constexpr const char* move_intrinsic_annotation() {
    return IntrinsicTag::memory_move;
}

void register_std_mem_intrinsics();

} // namespace meld::std_mem
