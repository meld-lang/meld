#pragma once

#include <vector>
#include <functional>
#include <optional>
#include <stdexcept>
#include <algorithm>

namespace meld {

// Mutable list - allows in-place modifications
template<typename T>
class MutableList {
private:
    std::vector<T> data_;
    
public:
    // Constructors
    MutableList() = default;
    
    explicit MutableList(const std::vector<T>& data) : data_(data) {}
    
    explicit MutableList(std::vector<T>&& data) : data_(std::move(data)) {}
    
    // Factory methods
    static MutableList<T> of(const std::vector<T>& data) {
        return MutableList<T>(data);
    }
    
    static MutableList<T> empty() {
        return MutableList<T>();
    }
    
    // Add element (mutates in place)
    void add(const T& value) {
        data_.push_back(value);
    }
    
    // Add multiple elements
    void addAll(const std::vector<T>& values) {
        data_.insert(data_.end(), values.begin(), values.end());
    }
    
    // Insert at index
    void insert(size_t index, const T& value) {
        if (index > data_.size()) {
            throw std::out_of_range("Index out of bounds");
        }
        data_.insert(data_.begin() + index, value);
    }
    
    // Get element at index
    T& get(size_t index) {
        if (index >= data_.size()) {
            throw std::out_of_range("Index out of bounds");
        }
        return data_[index];
    }
    
    const T& get(size_t index) const {
        if (index >= data_.size()) {
            throw std::out_of_range("Index out of bounds");
        }
        return data_[index];
    }
    
    // Operator[] for convenient access
    T& operator[](size_t index) {
        return get(index);
    }
    
    const T& operator[](size_t index) const {
        return get(index);
    }
    
    // Set element at index
    void set(size_t index, const T& value) {
        if (index >= data_.size()) {
            throw std::out_of_range("Index out of bounds");
        }
        data_[index] = value;
    }
    
    // Remove element at index
    void remove(size_t index) {
        if (index >= data_.size()) {
            throw std::out_of_range("Index out of bounds");
        }
        data_.erase(data_.begin() + index);
    }
    
    // Remove element by value (first occurrence)
    bool removeValue(const T& value) {
        auto it = std::find(data_.begin(), data_.end(), value);
        if (it != data_.end()) {
            data_.erase(it);
            return true;
        }
        return false;
    }
    
    // Clear all elements
    void clear() {
        data_.clear();
    }
    
    // Size and emptiness
    size_t size() const { return data_.size(); }
    bool isEmpty() const { return data_.empty(); }
    
    // First and last elements
    std::optional<T> first() const {
        if (isEmpty()) {
            return std::nullopt;
        }
        return data_.front();
    }
    
    std::optional<T> last() const {
        if (isEmpty()) {
            return std::nullopt;
        }
        return data_.back();
    }
    
    // Contains check
    bool contains(const T& value) const {
        return std::find(data_.begin(), data_.end(), value) != data_.end();
    }
    
    // Map operation (returns new list)
    template<typename U>
    MutableList<U> map(std::function<U(const T&)> fn) const {
        std::vector<U> result;
        result.reserve(data_.size());
        
        for (const auto& item : data_) {
            result.push_back(fn(item));
        }
        
        return MutableList<U>(std::move(result));
    }
    
    // Filter operation (returns new list)
    MutableList<T> filter(std::function<bool(const T&)> predicate) const {
        std::vector<T> result;
        
        for (const auto& item : data_) {
            if (predicate(item)) {
                result.push_back(item);
            }
        }
        
        return MutableList<T>(std::move(result));
    }
    
    // ForEach operation
    void forEach(std::function<void(const T&)> fn) const {
        for (const auto& item : data_) {
            fn(item);
        }
    }
    
    // ForEach with mutable access
    void forEachMut(std::function<void(T&)> fn) {
        for (auto& item : data_) {
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
    
    // Sort in place
    void sort() {
        std::sort(data_.begin(), data_.end());
    }
    
    void sort(std::function<bool(const T&, const T&)> comparator) {
        std::sort(data_.begin(), data_.end(), comparator);
    }
    
    // Reverse in place
    void reverse() {
        std::reverse(data_.begin(), data_.end());
    }
    
    // Convert to vector
    std::vector<T> toVector() const {
        return data_;
    }
    
    // Get underlying vector (const)
    const std::vector<T>& data() const {
        return data_;
    }
    
    // Iterators for range-based for loops
    typename std::vector<T>::iterator begin() { return data_.begin(); }
    typename std::vector<T>::iterator end() { return data_.end(); }
    typename std::vector<T>::const_iterator begin() const { return data_.begin(); }
    typename std::vector<T>::const_iterator end() const { return data_.end(); }
};

} // namespace meld
