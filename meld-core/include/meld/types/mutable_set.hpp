#pragma once

#include <set>
#include <vector>
#include <functional>
#include <algorithm>
#include <iterator>

namespace meld {

// Mutable set - allows in-place modifications
template<typename T>
class MutableSet {
private:
    std::set<T> data_;
    
public:
    // Constructors
    MutableSet() = default;
    
    explicit MutableSet(const std::set<T>& data) : data_(data) {}
    
    explicit MutableSet(std::set<T>&& data) : data_(std::move(data)) {}
    
    explicit MutableSet(const std::vector<T>& data) {
        data_.insert(data.begin(), data.end());
    }
    
    // Factory methods
    static MutableSet<T> of(std::initializer_list<T> data) {
        return MutableSet<T>(std::set<T>(data));
    }

    static MutableSet<T> of(const std::set<T>& data) {
        return MutableSet<T>(data);
    }
    
    static MutableSet<T> of(const std::vector<T>& data) {
        return MutableSet<T>(data);
    }
    
    static MutableSet<T> empty() {
        return MutableSet<T>();
    }
    
    // Add element (mutates in place)
    // Returns true if element was added, false if already present
    bool add(const T& value) {
        return data_.insert(value).second;
    }
    
    // Add multiple elements
    void addAll(std::initializer_list<T> values) {
        data_.insert(values);
    }

    void addAll(const std::vector<T>& values) {
        data_.insert(values.begin(), values.end());
    }
    
    void addAll(const std::set<T>& values) {
        data_.insert(values.begin(), values.end());
    }
    
    // Remove element (mutates in place)
    // Returns true if element was removed, false if not present
    bool remove(const T& value) {
        return data_.erase(value) > 0;
    }
    
    // Clear all elements
    void clear() {
        data_.clear();
    }
    
    // Contains check
    bool contains(const T& value) const {
        return data_.find(value) != data_.end();
    }
    
    // Size and emptiness
    size_t size() const { return data_.size(); }
    bool isEmpty() const { return data_.empty(); }
    
    // Map operation (returns new set)
    template<typename U>
    MutableSet<U> map(std::function<U(const T&)> fn) const {
        std::set<U> result;
        
        for (const auto& item : data_) {
            result.insert(fn(item));
        }
        
        return MutableSet<U>(std::move(result));
    }
    
    // Filter operation (returns new set)
    MutableSet<T> filter(std::function<bool(const T&)> predicate) const {
        std::set<T> result;
        
        for (const auto& item : data_) {
            if (predicate(item)) {
                result.insert(item);
            }
        }
        
        return MutableSet<T>(std::move(result));
    }
    
    // ForEach operation
    void forEach(std::function<void(const T&)> fn) const {
        for (const auto& item : data_) {
            fn(item);
        }
    }
    
    // Reduce operation
    template<typename U>
    U reduce(const U& initial, std::function<U(const U&, const T&)> fn) const {
        U result = initial;
        for (const auto& item : data_) {
            result = fn(result, item);
        }
        return result;
    }
    
    // Convert to vector
    std::vector<T> toVector() const {
        return std::vector<T>(data_.begin(), data_.end());
    }
    
    // Convert to std::set
    std::set<T> toSet() const {
        return data_;
    }
    
    // Get underlying set (const)
    const std::set<T>& data() const {
        return data_;
    }
    
    // Union with another set (mutates in place)
    void unionWith(const MutableSet<T>& other) {
        data_.insert(other.data_.begin(), other.data_.end());
    }
    
    // Union operation (returns new set)
    MutableSet<T> unionCopy(const MutableSet<T>& other) const {
        std::set<T> result = data_;
        result.insert(other.data_.begin(), other.data_.end());
        return MutableSet<T>(std::move(result));
    }
    
    // Intersection (mutates in place)
    void intersectWith(const MutableSet<T>& other) {
        std::set<T> result;
        std::set_intersection(
            data_.begin(), data_.end(),
            other.data_.begin(), other.data_.end(),
            std::inserter(result, result.begin())
        );
        data_ = std::move(result);
    }
    
    // Intersection operation (returns new set)
    MutableSet<T> intersect(const MutableSet<T>& other) const {
        std::set<T> result;
        std::set_intersection(
            data_.begin(), data_.end(),
            other.data_.begin(), other.data_.end(),
            std::inserter(result, result.begin())
        );
        return MutableSet<T>(std::move(result));
    }
    
    // Difference (mutates in place)
    void differenceWith(const MutableSet<T>& other) {
        std::set<T> result;
        std::set_difference(
            data_.begin(), data_.end(),
            other.data_.begin(), other.data_.end(),
            std::inserter(result, result.begin())
        );
        data_ = std::move(result);
    }
    
    // Difference operation (returns new set)
    MutableSet<T> difference(const MutableSet<T>& other) const {
        std::set<T> result;
        std::set_difference(
            data_.begin(), data_.end(),
            other.data_.begin(), other.data_.end(),
            std::inserter(result, result.begin())
        );
        return MutableSet<T>(std::move(result));
    }
    
    // Check if this is a subset of other
    bool isSubsetOf(const MutableSet<T>& other) const {
        return std::includes(
            other.data_.begin(), other.data_.end(),
            data_.begin(), data_.end()
        );
    }
    
    // Check if this is a superset of other
    bool isSupersetOf(const MutableSet<T>& other) const {
        return other.isSubsetOf(*this);
    }
    
    // Iterators for range-based for loops
    typename std::set<T>::iterator begin() { return data_.begin(); }
    typename std::set<T>::iterator end() { return data_.end(); }
    typename std::set<T>::const_iterator begin() const { return data_.begin(); }
    typename std::set<T>::const_iterator end() const { return data_.end(); }
};

} // namespace meld
