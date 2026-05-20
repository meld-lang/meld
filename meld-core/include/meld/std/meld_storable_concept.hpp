#pragma once

/// @file meld_storable_concept.hpp
/// @brief C++20 MeldStorable concept and constrained Vector template
///
/// Mirrors the Meld-level Storable trait (@intrinsic(memory_strategy)) as a
/// C++20 concept in the generated C++ backend. This provides defense-in-depth:
/// the Meld Semantic Analyzer enforces Storable at the language level, and the
/// MeldStorable concept enforces it again at the C++ compilation level.
///
/// The concept requires two methods matching the Storable trait signatures:
///   - get_ref_count() -> uint64_t
///   - is_owning() -> bool
///
/// The Vector<T> template is constrained by MeldStorable, preventing raw
/// unmanaged types from entering collections at the C++ level.
///
/// Requirements: 9.1, 9.2, 9.3, 9.4

#include <concepts>
#include <cstdint>
#include <optional>
#include <vector>
#include <iterator>

namespace meld::std_mem {

// ---------------------------------------------------------------------------
// MeldStorable concept — C++20 mirror of the Storable trait
// ---------------------------------------------------------------------------

/// C++20 concept that mirrors the Meld Storable trait.
/// Requires:
///   - get_ref_count() returning uint64_t
///   - is_owning() returning bool
///
/// Hold[T] and View[T] satisfy this concept. Raw types (int, std::string, etc.)
/// do not, producing a concept constraint violation at compile time.
template<typename T>
concept MeldStorable = requires(T a) {
    { a.get_ref_count() } -> std::same_as<uint64_t>;
    { a.is_owning() } -> std::same_as<bool>;
};

// ---------------------------------------------------------------------------
// Vector<T> — Managed collection constrained by MeldStorable
// ---------------------------------------------------------------------------

/// A managed collection that only accepts elements satisfying the MeldStorable
/// concept. This is the C++ lowering target for Meld's vec[T: Storable].
///
/// Provides push, get, size, and standard begin/end iterators.
template<MeldStorable T>
class Vector {
public:
    Vector() = default;

    /// Add an element to the end of the vector.
    void push(T element) {
        elements_.push_back(std::move(element));
    }

    /// Access an element by index. Returns nullopt if out of bounds.
    std::optional<std::reference_wrapper<T>> get(size_t index) {
        if (index < elements_.size()) {
            return std::ref(elements_[index]);
        }
        return std::nullopt;
    }

    /// Access an element by index (const). Returns nullopt if out of bounds.
    std::optional<std::reference_wrapper<const T>> get(size_t index) const {
        if (index < elements_.size()) {
            return std::cref(elements_[index]);
        }
        return std::nullopt;
    }

    /// Returns the number of elements in the vector.
    size_t size() const { return elements_.size(); }

    /// Returns true if the vector is empty.
    bool empty() const { return elements_.empty(); }

    // -- Standard iterators --

    using iterator = typename std::vector<T>::iterator;
    using const_iterator = typename std::vector<T>::const_iterator;

    iterator begin() { return elements_.begin(); }
    iterator end() { return elements_.end(); }
    const_iterator begin() const { return elements_.begin(); }
    const_iterator end() const { return elements_.end(); }
    const_iterator cbegin() const { return elements_.cbegin(); }
    const_iterator cend() const { return elements_.cend(); }

private:
    std::vector<T> elements_;
};

} // namespace meld::std_mem
