#pragma once

#include <memory>
#include <map>
#include <vector>
#include <functional>
#include <optional>
#include <stdexcept>

namespace meld {

// Persistent immutable map with structural sharing
// Uses a simple persistent map implementation based on shared pointers
template<typename K, typename V>
class PersistentMap {
private:
    // Node structure for structural sharing
    struct Node {
        std::map<K, V> data;
        std::shared_ptr<Node> parent;
        
        Node() : parent(nullptr) {}
        Node(const std::map<K, V>& d) : data(d), parent(nullptr) {}
        Node(std::map<K, V>&& d) : data(std::move(d)), parent(nullptr) {}
        Node(const std::map<K, V>& d, std::shared_ptr<Node> p) 
            : data(d), parent(p) {}
    };
    
    std::shared_ptr<Node> root_;
    size_t size_;
    
    // Helper to collect all entries (later entries override earlier ones)
    std::map<K, V> collectAll() const {
        std::map<K, V> result;
        
        // Collect from parent to root (reverse order)
        std::vector<std::shared_ptr<Node>> nodes;
        auto current = root_;
        while (current) {
            nodes.push_back(current);
            current = current->parent;
        }
        
        // Apply in reverse order so newer entries override older ones
        for (auto it = nodes.rbegin(); it != nodes.rend(); ++it) {
            for (const auto& [key, value] : (*it)->data) {
                result[key] = value;
            }
        }
        
        return result;
    }
    
public:
    // Constructors
    PersistentMap() : root_(std::make_shared<Node>()), size_(0) {}
    
    explicit PersistentMap(const std::map<K, V>& data) 
        : root_(std::make_shared<Node>(data)), size_(data.size()) {}
    
    explicit PersistentMap(std::map<K, V>&& data) {
        size_ = data.size();
        root_ = std::make_shared<Node>(std::move(data));
    }
    
    // Factory methods
    static PersistentMap<K, V> of(const std::map<K, V>& data) {
        return PersistentMap<K, V>(data);
    }
    
    static PersistentMap<K, V> empty() {
        return PersistentMap<K, V>();
    }
    
    // Put key-value pair (returns new map with structural sharing)
    PersistentMap<K, V> put(const K& key, const V& value) const {
        PersistentMap<K, V> result;
        
        // Check if key already exists
        bool keyExists = false;
        auto current = root_;
        while (current) {
            if (current->data.find(key) != current->data.end()) {
                keyExists = true;
                break;
            }
            current = current->parent;
        }
        
        std::map<K, V> newData;
        newData[key] = value;
        result.root_ = std::make_shared<Node>(std::move(newData), root_);
        result.size_ = keyExists ? size_ : size_ + 1;
        
        return result;
    }
    
    // Put multiple key-value pairs
    PersistentMap<K, V> putAll(const std::map<K, V>& entries) const {
        if (entries.empty()) {
            return *this;
        }
        
        PersistentMap<K, V> result;
        result.root_ = std::make_shared<Node>(entries, root_);
        
        // Calculate new size
        auto allData = result.collectAll();
        result.size_ = allData.size();
        
        return result;
    }
    
    // Get value for key
    std::optional<V> get(const K& key) const {
        auto current = root_;
        while (current) {
            auto it = current->data.find(key);
            if (it != current->data.end()) {
                return it->second;
            }
            current = current->parent;
        }
        return std::nullopt;
    }
    
    // Get value or default
    V getOrDefault(const K& key, const V& defaultValue) const {
        auto value = get(key);
        return value.has_value() ? value.value() : defaultValue;
    }
    
    // Remove key (returns new map)
    PersistentMap<K, V> remove(const K& key) const {
        auto allData = collectAll();
        allData.erase(key);
        return PersistentMap<K, V>(std::move(allData));
    }
    
    // Contains key check
    bool containsKey(const K& key) const {
        return get(key).has_value();
    }
    
    // Contains value check
    bool containsValue(const V& value) const {
        auto current = root_;
        while (current) {
            for (const auto& [k, v] : current->data) {
                if (v == value) {
                    return true;
                }
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
    
    // Get all keys
    std::vector<K> keys() const {
        std::vector<K> result;
        auto allData = collectAll();
        for (const auto& [key, value] : allData) {
            result.push_back(key);
        }
        return result;
    }
    
    // Get all values
    std::vector<V> values() const {
        std::vector<V> result;
        auto allData = collectAll();
        for (const auto& [key, value] : allData) {
            result.push_back(value);
        }
        return result;
    }
    
    // Get all entries
    std::vector<std::pair<K, V>> entries() const {
        std::vector<std::pair<K, V>> result;
        auto allData = collectAll();
        for (const auto& [key, value] : allData) {
            result.push_back({key, value});
        }
        return result;
    }
    
    // Map operation (transform values)
    template<typename U>
    PersistentMap<K, U> mapValues(std::function<U(const V&)> fn) const {
        std::map<K, U> result;
        auto allData = collectAll();
        
        for (const auto& [key, value] : allData) {
            result[key] = fn(value);
        }
        
        return PersistentMap<K, U>(std::move(result));
    }
    
    // Filter operation
    PersistentMap<K, V> filter(std::function<bool(const K&, const V&)> predicate) const {
        std::map<K, V> result;
        auto allData = collectAll();
        
        for (const auto& [key, value] : allData) {
            if (predicate(key, value)) {
                result[key] = value;
            }
        }
        
        return PersistentMap<K, V>(std::move(result));
    }
    
    // ForEach operation
    void forEach(std::function<void(const K&, const V&)> fn) const {
        auto allData = collectAll();
        for (const auto& [key, value] : allData) {
            fn(key, value);
        }
    }
    
    // Reduce operation
    template<typename U>
    U reduce(const U& initial, std::function<U(const U&, const K&, const V&)> fn) const {
        U result = initial;
        forEach([&result, &fn](const K& key, const V& value) {
            result = fn(result, key, value);
        });
        return result;
    }
    
    // Convert to std::map
    std::map<K, V> toMap() const {
        return collectAll();
    }
    
    // Merge two maps (entries from other override this)
    PersistentMap<K, V> merge(const PersistentMap<K, V>& other) const {
        auto allData = collectAll();
        auto otherData = other.collectAll();
        
        for (const auto& [key, value] : otherData) {
            allData[key] = value;
        }
        
        return PersistentMap<K, V>(std::move(allData));
    }
};

// Type alias for convenience
template<typename K, typename V>
using Map = PersistentMap<K, V>;

} // namespace meld
