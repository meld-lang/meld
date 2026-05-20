#pragma once

#include <memory>
#include <set>
#include <vector>
#include <functional>
#include <optional>
#include <algorithm>
#include <iterator>

namespace meld {

// Persistent immutable set with structural sharing
// Uses a simple persistent set implementation based on shared pointers
template<typename T>
class PersistentSet {
private:
    // Node structure for structural sharing
    struct Node {
        std::set<T> data;
        std::shared_ptr<Node> parent;
        
        Node() : parent(nullptr) {}
        Node(const std::set<T>& d) : data(d), parent(nullptr) {}
        Node(std::set<T>&& d) : data(std::move(d)), parent(nullptr) {}
        Node(const std::set<T>& d, std::shared_ptr<Node> p) 
            : data(d), parent(p) {}
    };
    
    std::shared_ptr<Node> root_;
    size_t size_;
    
    // Helper to collect all elements
    std::set<T> collectAll() const {
        std::set<T> result;
        
        auto current = root_;
        while (current) {
            result.insert(current->data.begin(), current->data.end());
            current = current->parent;
        }
        
        return result;
    }
    
public:
    // Constructors
    PersistentSet() : root_(std::make_shared<Node>()), size_(0) {}
    
    explicit PersistentSet(const std::set<T>& data) 
        : root_(std::make_shared<Node>(data)), size_(data.size()) {}
    
    explicit PersistentSet(std::set<T>&& data) {
        size_ = data.size();
        root_ = std::make_shared<Node>(std::move(data));
    }
    
    explicit PersistentSet(const std::vector<T>& data) {
        std::set<T> s(data.begin(), data.end());
        size_ = s.size();
        root_ = std::make_shared<Node>(std::move(s));
    }
    
    // Factory methods
    static PersistentSet<T> of(std::initializer_list<T> data) {
        return PersistentSet<T>(std::set<T>(data));
    }

    static PersistentSet<T> of(const std::set<T>& data) {
        return PersistentSet<T>(data);
    }
    
    static PersistentSet<T> of(const std::vector<T>& data) {
        return PersistentSet<T>(data);
    }
    
    static PersistentSet<T> empty() {
        return PersistentSet<T>();
    }
    
    // Add element (returns new set with structural sharing)
    PersistentSet<T> add(const T& value) const {
        // Check if value already exists
        if (contains(value)) {
            return *this;
        }
        
        PersistentSet<T> result;
        std::set<T> newData;
        newData.insert(value);
        result.root_ = std::make_shared<Node>(std::move(newData), root_);
        result.size_ = size_ + 1;
        
        return result;
    }
    
    // Add multiple elements
    PersistentSet<T> addAll(const std::vector<T>& values) const {
        if (values.empty()) {
            return *this;
        }
        
        std::set<T> newData;
        for (const auto& value : values) {
            if (!contains(value)) {
                newData.insert(value);
            }
        }
        
        if (newData.empty()) {
            return *this;
        }
        
        PersistentSet<T> result;
        result.root_ = std::make_shared<Node>(std::move(newData), root_);
        result.size_ = size_ + newData.size();
        
        return result;
    }
    
    // Remove element (returns new set)
    PersistentSet<T> remove(const T& value) const {
        if (!contains(value)) {
            return *this;
        }
        
        auto allData = collectAll();
        allData.erase(value);
        return PersistentSet<T>(std::move(allData));
    }
    
    // Contains check
    bool contains(const T& value) const {
        auto current = root_;
        while (current) {
            if (current->data.find(value) != current->data.end()) {
                return true;
            }
            current = current->parent;
        }
        return false;
    }
    
    // Size and emptiness
    size_t size() const { 
        // Recalculate size to ensure accuracy
        return collectAll().size();
    }
    
    bool isEmpty() const { 
        return size() == 0;
    }
    
    // Map operation
    template<typename U>
    PersistentSet<U> map(std::function<U(const T&)> fn) const {
        std::set<U> result;
        auto allData = collectAll();
        
        for (const auto& item : allData) {
            result.insert(fn(item));
        }
        
        return PersistentSet<U>(std::move(result));
    }
    
    // Filter operation
    PersistentSet<T> filter(std::function<bool(const T&)> predicate) const {
        std::set<T> result;
        auto allData = collectAll();
        
        for (const auto& item : allData) {
            if (predicate(item)) {
                result.insert(item);
            }
        }
        
        return PersistentSet<T>(std::move(result));
    }
    
    // ForEach operation
    void forEach(std::function<void(const T&)> fn) const {
        auto allData = collectAll();
        for (const auto& item : allData) {
            fn(item);
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
        auto allData = collectAll();
        return std::vector<T>(allData.begin(), allData.end());
    }
    
    // Convert to std::set
    std::set<T> toSet() const {
        return collectAll();
    }
    
    // Union of two sets
    PersistentSet<T> unionWith(const PersistentSet<T>& other) const {
        auto allData = collectAll();
        auto otherData = other.collectAll();
        
        allData.insert(otherData.begin(), otherData.end());
        return PersistentSet<T>(std::move(allData));
    }
    
    // Intersection of two sets
    PersistentSet<T> intersect(const PersistentSet<T>& other) const {
        std::set<T> result;
        auto allData = collectAll();
        auto otherData = other.collectAll();
        
        std::set_intersection(
            allData.begin(), allData.end(),
            otherData.begin(), otherData.end(),
            std::inserter(result, result.begin())
        );
        
        return PersistentSet<T>(std::move(result));
    }
    
    // Difference of two sets (elements in this but not in other)
    PersistentSet<T> difference(const PersistentSet<T>& other) const {
        std::set<T> result;
        auto allData = collectAll();
        auto otherData = other.collectAll();
        
        std::set_difference(
            allData.begin(), allData.end(),
            otherData.begin(), otherData.end(),
            std::inserter(result, result.begin())
        );
        
        return PersistentSet<T>(std::move(result));
    }
    
    // Check if this is a subset of other
    bool isSubsetOf(const PersistentSet<T>& other) const {
        auto allData = collectAll();
        auto otherData = other.collectAll();
        
        return std::includes(
            otherData.begin(), otherData.end(),
            allData.begin(), allData.end()
        );
    }
    
    // Check if this is a superset of other
    bool isSupersetOf(const PersistentSet<T>& other) const {
        return other.isSubsetOf(*this);
    }
};

// Type alias for convenience
template<typename T>
using Set = PersistentSet<T>;

} // namespace meld
