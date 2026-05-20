#pragma once

/// @file collection_iterators.hpp
/// @brief Specialized collection types and iterators for Hold[T] and View[T]
///
/// Provides HoldVector<T> with a solid iterator (yields T& directly, no null
/// check) and ViewVector<T> with a filtering iterator (skips dead entries)
/// plus an .entries() method returning an EntriesIterator that yields
/// optional<reference_wrapper<T>> for every slot including dead ones.
///
/// The iterator behavior is driven by the Storable trait's is_owning() result:
///   - Hold[T] → solid iterator, all n elements guaranteed non-null and alive
///   - View[T] → filtering iterator, skips expired weak refs
///
/// Requirements: 6.1, 6.2, 6.3, 6.4, 6.5

#include "meld/std/meld_storable_concept.hpp"
#include "meld/std/mem.hpp"
#include "meld/types/memory.hpp"

#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <vector>

namespace meld::std_mem {

// ===========================================================================
// HoldVector<T> — vec[Hold[T]] with solid iterator
// ===========================================================================

/// Managed collection for Hold[T] elements. Every element is guaranteed
/// non-null and alive. The default iterator yields T& directly with no
/// null check required.
///
/// Requirement 6.1: default iterator yields T directly, each element
/// guaranteed non-null and alive.
template<typename T>
class HoldVector {
    static_assert(std::is_base_of_v<types::ManagedObject, T>,
                  "HoldVector<T> requires T to derive from ManagedObject");

public:
    HoldVector() = default;

    /// Add an owning element.
    void push(Own<T> element) {
        elements_.push_back(std::move(element));
    }

    /// Number of elements.
    size_t size() const { return elements_.size(); }

    /// Whether the vector is empty.
    bool empty() const { return elements_.empty(); }

    // -------------------------------------------------------------------
    // Solid iterator — yields T& directly, no null check
    // -------------------------------------------------------------------

    class Iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type        = T;
        using difference_type   = std::ptrdiff_t;
        using pointer           = T*;
        using reference         = T&;

        Iterator() : vec_(nullptr), index_(0) {}
        Iterator(HoldVector* vec, size_t index)
            : vec_(vec), index_(index) {}

        /// Yields T& directly — guaranteed alive because Hold[T] is non-null.
        reference operator*() const {
            return vec_->elements_[index_].get();
        }

        pointer operator->() const {
            return &(vec_->elements_[index_].get());
        }

        Iterator& operator++() {
            ++index_;
            return *this;
        }

        Iterator operator++(int) {
            Iterator tmp = *this;
            ++index_;
            return tmp;
        }

        bool operator==(const Iterator& other) const {
            return index_ == other.index_;
        }

        bool operator!=(const Iterator& other) const {
            return index_ != other.index_;
        }

    private:
        HoldVector* vec_;
        size_t index_;
    };

    // Const version of the solid iterator
    class ConstIterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type        = T;
        using difference_type   = std::ptrdiff_t;
        using pointer           = const T*;
        using reference         = const T&;

        ConstIterator() : vec_(nullptr), index_(0) {}
        ConstIterator(const HoldVector* vec, size_t index)
            : vec_(vec), index_(index) {}

        reference operator*() const {
            return vec_->elements_[index_].get();
        }

        pointer operator->() const {
            return &(vec_->elements_[index_].get());
        }

        ConstIterator& operator++() {
            ++index_;
            return *this;
        }

        ConstIterator operator++(int) {
            ConstIterator tmp = *this;
            ++index_;
            return tmp;
        }

        bool operator==(const ConstIterator& other) const {
            return index_ == other.index_;
        }

        bool operator!=(const ConstIterator& other) const {
            return index_ != other.index_;
        }

    private:
        const HoldVector* vec_;
        size_t index_;
    };

    Iterator begin() { return Iterator(this, 0); }
    Iterator end()   { return Iterator(this, elements_.size()); }
    ConstIterator begin() const  { return ConstIterator(this, 0); }
    ConstIterator end() const    { return ConstIterator(this, elements_.size()); }
    ConstIterator cbegin() const { return ConstIterator(this, 0); }
    ConstIterator cend() const   { return ConstIterator(this, elements_.size()); }

    /// Access underlying Own<T> element by index.
    std::optional<std::reference_wrapper<Own<T>>> get_own(size_t index) {
        if (index < elements_.size()) {
            return std::ref(elements_[index]);
        }
        return std::nullopt;
    }

private:
    std::vector<Own<T>> elements_;
};

// ===========================================================================
// ViewVector<T> — vec[View[T]] with filtering iterator + entries()
// ===========================================================================

/// Managed collection for View[T] elements. The default iterator skips
/// dead entries (expired weak refs), yielding only live T& values.
/// The .entries() method returns an EntriesIterator that yields
/// optional<reference_wrapper<T>> for every slot, including none for dead.
///
/// Requirements: 6.2, 6.3, 6.4, 6.5
template<typename T>
class ViewVector {
    static_assert(std::is_base_of_v<types::ManagedObject, T>,
                  "ViewVector<T> requires T to derive from ManagedObject");

public:
    ViewVector() = default;

    /// Add a link element.
    void push(Link<T> element) {
        elements_.push_back(std::move(element));
    }

    /// Total number of slots (including dead entries).
    size_t size() const { return elements_.size(); }

    /// Whether the vector has no slots.
    bool empty() const { return elements_.empty(); }

    /// Count of currently alive entries.
    size_t live_count() const {
        size_t count = 0;
        for (const auto& elem : elements_) {
            if (!elem.expired()) ++count;
        }
        return count;
    }

    // -------------------------------------------------------------------
    // Filtering iterator — skips dead entries, yields T& for live ones
    // Requirement 6.2: default iterator skips dead entries
    // Requirement 6.4: skips dead entries without error
    // -------------------------------------------------------------------

    class Iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type        = T;
        using difference_type   = std::ptrdiff_t;
        using pointer           = T*;
        using reference         = T&;

        Iterator() : vec_(nullptr), index_(0) {}
        Iterator(ViewVector* vec, size_t index)
            : vec_(vec), index_(index)
        {
            // Advance past any initial dead entries
            skip_dead();
        }

        /// Yields T& — guaranteed alive because we skipped dead entries.
        /// The upgrade (lock) is held via the cached shared_ptr.
        reference operator*() const {
            return *cached_lock_;
        }

        pointer operator->() const {
            return cached_lock_.get();
        }

        Iterator& operator++() {
            ++index_;
            skip_dead();
            return *this;
        }

        Iterator operator++(int) {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const Iterator& other) const {
            return index_ == other.index_;
        }

        bool operator!=(const Iterator& other) const {
            return index_ != other.index_;
        }

    private:
        /// Advance index_ past dead entries, caching the lock for the
        /// first live entry found.
        void skip_dead() {
            cached_lock_.reset();
            while (index_ < vec_->elements_.size()) {
                auto upgraded = vec_->elements_[index_].upgrade();
                if (upgraded.has_value()) {
                    cached_lock_ = upgraded.value();
                    return;
                }
                ++index_; // skip dead entry without error (Req 6.4)
            }
        }

        ViewVector* vec_;
        size_t index_;
        std::shared_ptr<T> cached_lock_; // holds strong ref for current live element
    };

    // Const filtering iterator
    class ConstIterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type        = T;
        using difference_type   = std::ptrdiff_t;
        using pointer           = const T*;
        using reference         = const T&;

        ConstIterator() : vec_(nullptr), index_(0) {}
        ConstIterator(const ViewVector* vec, size_t index)
            : vec_(vec), index_(index)
        {
            skip_dead();
        }

        reference operator*() const {
            return *cached_lock_;
        }

        pointer operator->() const {
            return cached_lock_.get();
        }

        ConstIterator& operator++() {
            ++index_;
            skip_dead();
            return *this;
        }

        ConstIterator operator++(int) {
            ConstIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const ConstIterator& other) const {
            return index_ == other.index_;
        }

        bool operator!=(const ConstIterator& other) const {
            return index_ != other.index_;
        }

    private:
        void skip_dead() {
            cached_lock_.reset();
            while (index_ < vec_->elements_.size()) {
                auto upgraded = vec_->elements_[index_].upgrade();
                if (upgraded.has_value()) {
                    cached_lock_ = upgraded.value();
                    return;
                }
                ++index_;
            }
        }

        const ViewVector* vec_;
        size_t index_;
        std::shared_ptr<T> cached_lock_;
    };

    Iterator begin() { return Iterator(this, 0); }
    Iterator end()   { return Iterator(this, elements_.size()); }
    ConstIterator begin() const  { return ConstIterator(this, 0); }
    ConstIterator end() const    { return ConstIterator(this, elements_.size()); }
    ConstIterator cbegin() const { return ConstIterator(this, 0); }
    ConstIterator cend() const   { return ConstIterator(this, elements_.size()); }

    // -------------------------------------------------------------------
    // EntriesIterator — yields optional<reference_wrapper<T>> for every slot
    // Requirement 6.3: .entries() returns iterator yielding optional[T]
    // Requirement 6.5: dead entries yield none
    // -------------------------------------------------------------------

    class EntriesIterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type        = std::optional<std::reference_wrapper<T>>;
        using difference_type   = std::ptrdiff_t;
        using pointer           = void;
        using reference         = value_type;

        EntriesIterator() : vec_(nullptr), index_(0) {}
        EntriesIterator(ViewVector* vec, size_t index)
            : vec_(vec), index_(index) {}

        /// Yields optional<reference_wrapper<T>>:
        ///   - Live entry → reference to T
        ///   - Dead entry → nullopt (Req 6.5)
        value_type operator*() const {
            auto upgraded = vec_->elements_[index_].upgrade();
            if (upgraded.has_value()) {
                // Hold the lock and return a reference
                cached_lock_ = upgraded.value();
                return std::ref(*cached_lock_);
            }
            cached_lock_.reset();
            return std::nullopt;
        }

        EntriesIterator& operator++() {
            ++index_;
            return *this;
        }

        EntriesIterator operator++(int) {
            EntriesIterator tmp = *this;
            ++index_;
            return tmp;
        }

        bool operator==(const EntriesIterator& other) const {
            return index_ == other.index_;
        }

        bool operator!=(const EntriesIterator& other) const {
            return index_ != other.index_;
        }

    private:
        ViewVector* vec_;
        size_t index_;
        mutable std::shared_ptr<T> cached_lock_;
    };

    // Const entries iterator
    class ConstEntriesIterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type        = std::optional<std::reference_wrapper<const T>>;
        using difference_type   = std::ptrdiff_t;
        using pointer           = void;
        using reference         = value_type;

        ConstEntriesIterator() : vec_(nullptr), index_(0) {}
        ConstEntriesIterator(const ViewVector* vec, size_t index)
            : vec_(vec), index_(index) {}

        value_type operator*() const {
            auto upgraded = vec_->elements_[index_].upgrade();
            if (upgraded.has_value()) {
                cached_lock_ = upgraded.value();
                return std::cref(static_cast<const T&>(*cached_lock_));
            }
            cached_lock_.reset();
            return std::nullopt;
        }

        ConstEntriesIterator& operator++() {
            ++index_;
            return *this;
        }

        ConstEntriesIterator operator++(int) {
            ConstEntriesIterator tmp = *this;
            ++index_;
            return tmp;
        }

        bool operator==(const ConstEntriesIterator& other) const {
            return index_ == other.index_;
        }

        bool operator!=(const ConstEntriesIterator& other) const {
            return index_ != other.index_;
        }

    private:
        const ViewVector* vec_;
        size_t index_;
        mutable std::shared_ptr<T> cached_lock_;
    };

    // -------------------------------------------------------------------
    // Entries range wrapper for range-based for loops
    // -------------------------------------------------------------------

    class EntriesRange {
    public:
        explicit EntriesRange(ViewVector* vec) : vec_(vec) {}
        EntriesIterator begin() { return EntriesIterator(vec_, 0); }
        EntriesIterator end()   { return EntriesIterator(vec_, vec_->elements_.size()); }
    private:
        ViewVector* vec_;
    };

    class ConstEntriesRange {
    public:
        explicit ConstEntriesRange(const ViewVector* vec) : vec_(vec) {}
        ConstEntriesIterator begin() const { return ConstEntriesIterator(vec_, 0); }
        ConstEntriesIterator end() const   { return ConstEntriesIterator(vec_, vec_->elements_.size()); }
    private:
        const ViewVector* vec_;
    };

    /// Returns a range over all slots, yielding optional<reference_wrapper<T>>.
    /// Dead entries yield nullopt; live entries yield a reference.
    /// Requirement 6.3
    EntriesRange entries() { return EntriesRange(this); }
    ConstEntriesRange entries() const { return ConstEntriesRange(this); }

    /// Access underlying Link<T> element by index.
    std::optional<std::reference_wrapper<Link<T>>> get_link(size_t index) {
        if (index < elements_.size()) {
            return std::ref(elements_[index]);
        }
        return std::nullopt;
    }

private:
    std::vector<Link<T>> elements_;

    // Allow iterators to access elements_
    friend class Iterator;
    friend class ConstIterator;
    friend class EntriesIterator;
    friend class ConstEntriesIterator;
};

} // namespace meld::std_mem
