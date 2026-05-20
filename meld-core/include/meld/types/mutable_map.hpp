#pragma once

#include <map>
#include <vector>
#include <functional>
#include <optional>

namespace meld {

// Mutable map - allows in-place modifications
template<typename K, typename V>
class MutableMap {
private:
    std::map<K, V> data_;
    
public:
    // Constructors
    MutableMap() = default;
    
    explicit MutableMap(const std::map<K, V>& data) : data_(data) {}
    
    explicit MutableMap(std::map<K, V>&& data) : data_(std::move(data)) {}
    
    // Factory methods
    static MutableMap<K, V> of(const std::map<K, V>& data) {
        return MutableMap<K, V>(data);
    }
    
    static MutableMap<K, V> empty() {
        return MutableMap<K, V>();
    }
    
    // Put key-value pair (mutates in place)
    void put(const K& key, const V& value) {
        data_[key] = value;
    }
    
    // Put multiple key-value pairs
    void putAll(const std::map<K, V>& entries) {
        for (const auto& [key, value] : entries) {
            data_[key] = value;
        }
    }
    
    // Get value for key
    std::optional<V> get(const K& key) const {
        auto it = data_.find(key);
        if (it != data_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    // Get value or default
    V getOrDefault(const K& key, const V& defaultValue) const {
        auto it = data_.find(key);
        if (it != data_.end()) {
            return it->second;
        }
        return defaultValue;
    }
    
    // Operator[] for convenient access (creates entry if not exists)
    V& operator[](const K& key) {
        return data_[key];
    }
    
    // Remove key (mutates in place)
    bool remove(const K& key) {
        return data_.erase(key) > 0;
    }
    
    // Clear all entries
    void clear() {
        data_.clear();
    }
    
    // Contains key check
    bool containsKey(const K& key) const {
        return data_.find(key) != data_.end();
    }
    
    // Contains value check
    bool containsValue(const V& value) const {
        for (const auto& [k, v] : data_) {
            if (v == value) {
                return true;
            }
        }
        return false;
    }
    
    // Size and emptiness
    size_t size() const { return data_.size(); }
    bool isEmpty() const { return data_.empty(); }
    
    // Get all keys
    std::vector<K> keys() const {
        std::vector<K> result;
        result.reserve(data_.size());
        for (const auto& [key, value] : data_) {
            result.push_back(key);
        }
        return result;
    }
    
    // Get all values
    std::vector<V> values() const {
        std::vector<V> result;
        result.reserve(data_.size());
        for (const auto& [key, value] : data_) {
            result.push_back(value);
        }
        return result;
    }
    
    // Get all entries
    std::vector<std::pair<K, V>> entries() const {
        std::vector<std::pair<K, V>> result;
        result.reserve(data_.size());
        for (const auto& [key, value] : data_) {
            result.push_back({key, value});
        }
        return result;
    }
    
    // Map operation (returns new map)
    template<typename U>
    MutableMap<K, U> mapValues(std::function<U(const V&)> fn) const {
        std::map<K, U> result;
        
        for (const auto& [key, value] : data_) {
            result[key] = fn(value);
        }
        
        return MutableMap<K, U>(std::move(result));
    }
    
    // Filter operation (returns new map)
    MutableMap<K, V> filter(std::function<bool(const K&, const V&)> predicate) const {
        std::map<K, V> result;
        
        for (const auto& [key, value] : data_) {
            if (predicate(key, value)) {
                result[key] = value;
            }
        }
        
        return MutableMap<K, V>(std::move(result));
    }
    
    // ForEach operation
    void forEach(std::function<void(const K&, const V&)> fn) const {
        for (const auto& [key, value] : data_) {
            fn(key, value);
        }
    }
    
    // ForEach with mutable access to values
    void forEachMut(std::function<void(const K&, V&)> fn) {
        for (auto& [key, value] : data_) {
            fn(key, value);
        }
    }
    
    // Reduce operation
    template<typename U>
    U reduce(const U& initial, std::function<U(const U&, const K&, const V&)> fn) const {
        U result = initial;
        for (const auto& [key, value] : data_) {
            result = fn(result, key, value);
        }
        return result;
    }
    
    // Convert to std::map
    std::map<K, V> toMap() const {
        return data_;
    }
    
    // Get underlying map (const)
    const std::map<K, V>& data() const {
        return data_;
    }
    
    // Merge another map (entries from other override this)
    void merge(const MutableMap<K, V>& other) {
        for (const auto& [key, value] : other.data_) {
            data_[key] = value;
        }
    }
    
    // Iterators for range-based for loops
    typename std::map<K, V>::iterator begin() { return data_.begin(); }
    typename std::map<K, V>::iterator end() { return data_.end(); }
    typename std::map<K, V>::const_iterator begin() const { return data_.begin(); }
    typename std::map<K, V>::const_iterator end() const { return data_.end(); }
};

} // namespace meld
