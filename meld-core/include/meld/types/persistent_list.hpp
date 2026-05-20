#pragma once

#include <memory>
#include <vector>
#include <functional>
#include <optional>
#include <stdexcept>

namespace meld {

// Persistent immutable list with structural sharing
// Uses a simple persistent vector implementation based on shared pointers
template<typename T>
class PersistentList {
private:
    // Node structure for structural sharing
    struct Node {
        std::vector<T> data;
        std::shared_ptr<Node> tail;
        
        Node(const std::vector<T>& d) : data(d), tail(nullptr) {}
        Node(std::vector<T>&& d) : data(std::move(d)), tail(nullptr) {}
        Node(const std::vector<T>& d, std::shared_ptr<Node> t) 
            : data(d), tail(t) {}
    };
    
    std::shared_ptr<Node> root_;
    size_t size_;
    
    // Helper to collect all elements
    std::vector<T> collectAll() const {
        std::vector<T> result;
        result.reserve(size_);
        
        auto current = root_;
        while (current) {
            result.insert(result.end(), current->data.begin(), current->data.end());
            current = current->tail;
        }
        
        return result;
    }
    
public:
    // Constructors
    PersistentList() : root_(nullptr), size_(0) {}
    
    explicit PersistentList(const std::vector<T>& data) 
        : root_(std::make_shared<Node>(data)), size_(data.size()) {}
    
    explicit PersistentList(std::vector<T>&& data) {
        size_ = data.size();
        root_ = std::make_shared<Node>(std::move(data));
    }
    
    // Factory method
    static PersistentList<T> of(const std::vector<T>& data) {
        return PersistentList<T>(data);
    }
    
    static PersistentList<T> empty() {
        return PersistentList<T>();
    }
    
    // Add element (returns new list with structural sharing)
    PersistentList<T> add(const T& value) const {
        PersistentList<T> result;
        result.root_ = std::make_shared<Node>(std::vector<T>{value}, root_);
        result.size_ = size_ + 1;
        return result;
    }
    
    // Add multiple elements
    PersistentList<T> addAll(const std::vector<T>& values) const {
        if (values.empty()) {
            return *this;
        }
        
        PersistentList<T> result;
        result.root_ = std::make_shared<Node>(values, root_);
        result.size_ = size_ + values.size();
        return result;
    }
    
    // Get element at index
    T get(size_t index) const {
        if (index >= size_) {
            throw std::out_of_range("Index out of bounds");
        }
        
        size_t currentIndex = 0;
        auto current = root_;
        
        while (current) {
            if (index < currentIndex + current->data.size()) {
                return current->data[index - currentIndex];
            }
            currentIndex += current->data.size();
            current = current->tail;
        }
        
        throw std::out_of_range("Index out of bounds");
    }
    
    // Set element at index (returns new list)
    PersistentList<T> set(size_t index, const T& value) const {
        if (index >= size_) {
            throw std::out_of_range("Index out of bounds");
        }
        
        auto allData = collectAll();
        allData[index] = value;
        return PersistentList<T>(std::move(allData));
    }
    
    // Remove element at index (returns new list)
    PersistentList<T> remove(size_t index) const {
        if (index >= size_) {
            throw std::out_of_range("Index out of bounds");
        }
        
        auto allData = collectAll();
        allData.erase(allData.begin() + index);
        return PersistentList<T>(std::move(allData));
    }
    
    // Size and emptiness
    size_t size() const { return size_; }
    bool isEmpty() const { return size_ == 0; }
    
    // First and last elements
    std::optional<T> first() const {
        if (isEmpty()) {
            return std::nullopt;
        }
        return get(0);
    }
    
    std::optional<T> last() const {
        if (isEmpty()) {
            return std::nullopt;
        }
        return get(size_ - 1);
    }
    
    // Contains check
    bool contains(const T& value) const {
        auto current = root_;
        while (current) {
            for (const auto& item : current->data) {
                if (item == value) {
                    return true;
                }
            }
            current = current->tail;
        }
        return false;
    }
    
    // Map operation
    template<typename U>
    PersistentList<U> map(std::function<U(const T&)> fn) const {
        std::vector<U> result;
        result.reserve(size_);
        
        auto current = root_;
        while (current) {
            for (const auto& item : current->data) {
                result.push_back(fn(item));
            }
            current = current->tail;
        }
        
        return PersistentList<U>(std::move(result));
    }
    
    // Filter operation
    PersistentList<T> filter(std::function<bool(const T&)> predicate) const {
        std::vector<T> result;
        
        auto current = root_;
        while (current) {
            for (const auto& item : current->data) {
                if (predicate(item)) {
                    result.push_back(item);
                }
            }
            current = current->tail;
        }
        
        return PersistentList<T>(std::move(result));
    }
    
    // ForEach operation
    void forEach(std::function<void(const T&)> fn) const {
        auto current = root_;
        while (current) {
            for (const auto& item : current->data) {
                fn(item);
            }
            current = current->tail;
        }
    }
    
    // Reduce operation
    template<typename U>
    U reduce(const U& initial, std::function<U(const U&, const T&)> fn) const {
        U result = initial;
        forEach([&result, &fn](const T& item) {
            result = fn(result, item);
        });
        return result;
    }
    
    // Convert to vector
    std::vector<T> toVector() const {
        return collectAll();
    }
    
    // Concatenate two lists
    PersistentList<T> concat(const PersistentList<T>& other) const {
        if (other.isEmpty()) {
            return *this;
        }
        if (isEmpty()) {
            return other;
        }
        
        auto allData = collectAll();
        auto otherData = other.collectAll();
        allData.insert(allData.end(), otherData.begin(), otherData.end());
        return PersistentList<T>(std::move(allData));
    }
    
    // Take first n elements
    PersistentList<T> take(size_t n) const {
        if (n >= size_) {
            return *this;
        }
        
        std::vector<T> result;
        result.reserve(n);
        
        size_t count = 0;
        auto current = root_;
        
        while (current && count < n) {
            for (const auto& item : current->data) {
                if (count >= n) break;
                result.push_back(item);
                count++;
            }
            current = current->tail;
        }
        
        return PersistentList<T>(std::move(result));
    }
    
    // Drop first n elements
    PersistentList<T> drop(size_t n) const {
        if (n >= size_) {
            return PersistentList<T>::empty();
        }
        if (n == 0) {
            return *this;
        }
        
        auto allData = collectAll();
        std::vector<T> result(allData.begin() + n, allData.end());
        return PersistentList<T>(std::move(result));
    }
};

// Type alias for convenience
template<typename T>
using List = PersistentList<T>;

} // namespace meld
