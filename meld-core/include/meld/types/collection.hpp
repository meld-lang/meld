#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <vector>
#include <map>
#include <algorithm>
#include <numeric>
#include "meld/types/parallel_executor.hpp"

namespace meld {

// Forward declarations
template<typename T>
class Collection;

template<typename T>
class LazySequence;

template<typename T>
class VectorCollection;

// Collection trait - provides fluent API for collection operations
template<typename T>
class Collection {
public:
    virtual ~Collection() = default;
    
    // Core iteration
    virtual void forEach(std::function<void(const T&)> fn) const = 0;
    
    // Transformation operations (return new collections)
    virtual std::shared_ptr<Collection<T>> map(std::function<T(const T&)> fn) const = 0;
    
    template<typename U>
    std::shared_ptr<Collection<U>> map(std::function<U(const T&)> fn) const;
    
    virtual std::shared_ptr<Collection<T>> filter(std::function<bool(const T&)> predicate) const = 0;
    
    // Reduction operations
    virtual T reduce(const T& initial, std::function<T(const T&, const T&)> fn) const = 0;
    
    template<typename U>
    U reduce(const U& initial, std::function<U(const U&, const T&)> fn) const;
    
    // Grouping and joining
    template<typename K>
    std::map<K, std::vector<T>> groupBy(std::function<K(const T&)> keySelector) const;
    
    template<typename U>
    std::shared_ptr<Collection<std::pair<T, U>>> join(
        const Collection<U>& other,
        std::function<bool(const T&, const U&)> predicate) const;
    
    // Sorting
    virtual std::shared_ptr<Collection<T>> sorted() const = 0;
    virtual std::shared_ptr<Collection<T>> sorted(std::function<bool(const T&, const T&)> comparator) const = 0;
    
    // Limiting
    virtual std::shared_ptr<Collection<T>> take(size_t n) const = 0;
    
    // Terminal operations
    virtual std::vector<T> toList() const = 0;
    virtual size_t count() const = 0;
    virtual bool isEmpty() const = 0;
    virtual std::optional<T> first() const = 0;
    virtual std::optional<T> last() const = 0;
    
    // Lazy evaluation support
    virtual std::shared_ptr<LazySequence<T>> lazy() const = 0;
    
    // Parallel execution support
    virtual std::shared_ptr<Collection<T>> parallel() const = 0;
};

// Concrete implementation using std::vector as backing store
template<typename T>
class VectorCollection : public Collection<T> {
private:
    std::vector<T> data_;
    bool isParallel_;
    
public:
    explicit VectorCollection(const std::vector<T>& data, bool parallel = false)
        : data_(data), isParallel_(parallel) {}
    
    explicit VectorCollection(std::vector<T>&& data, bool parallel = false)
        : data_(std::move(data)), isParallel_(parallel) {}
    
    void forEach(std::function<void(const T&)> fn) const override {
        if (isParallel_) {
            ParallelExecutor::forEach(data_, fn);
        } else {
            for (const auto& item : data_) {
                fn(item);
            }
        }
    }
    
    std::shared_ptr<Collection<T>> map(std::function<T(const T&)> fn) const override {
        std::vector<T> result;
        
        if (isParallel_) {
            result = ParallelExecutor::map(data_, fn);
        } else {
            result.reserve(data_.size());
            for (const auto& item : data_) {
                result.push_back(fn(item));
            }
        }
        
        return std::make_shared<VectorCollection<T>>(std::move(result), isParallel_);
    }
    
    std::shared_ptr<Collection<T>> filter(std::function<bool(const T&)> predicate) const override {
        std::vector<T> result;
        
        if (isParallel_) {
            result = ParallelExecutor::filter(data_, predicate);
        } else {
            for (const auto& item : data_) {
                if (predicate(item)) {
                    result.push_back(item);
                }
            }
        }
        
        return std::make_shared<VectorCollection<T>>(std::move(result), isParallel_);
    }
    
    T reduce(const T& initial, std::function<T(const T&, const T&)> fn) const override {
        if (isParallel_) {
            return ParallelExecutor::reduce(data_, initial, fn);
        } else {
            T result = initial;
            for (const auto& item : data_) {
                result = fn(result, item);
            }
            return result;
        }
    }
    
    std::shared_ptr<Collection<T>> sorted() const override {
        std::vector<T> result;
        
        if (isParallel_) {
            result = ParallelExecutor::sort(data_);
        } else {
            result = data_;
            std::sort(result.begin(), result.end());
        }
        
        return std::make_shared<VectorCollection<T>>(std::move(result), isParallel_);
    }
    
    std::shared_ptr<Collection<T>> sorted(std::function<bool(const T&, const T&)> comparator) const override {
        std::vector<T> result;
        
        if (isParallel_) {
            result = ParallelExecutor::sort(data_, comparator);
        } else {
            result = data_;
            std::sort(result.begin(), result.end(), comparator);
        }
        
        return std::make_shared<VectorCollection<T>>(std::move(result), isParallel_);
    }
    
    std::shared_ptr<Collection<T>> take(size_t n) const override {
        size_t count = std::min(n, data_.size());
        std::vector<T> result(data_.begin(), data_.begin() + count);
        return std::make_shared<VectorCollection<T>>(std::move(result), isParallel_);
    }
    
    std::vector<T> toList() const override {
        return data_;
    }
    
    size_t count() const override {
        return data_.size();
    }
    
    bool isEmpty() const override {
        return data_.empty();
    }
    
    std::optional<T> first() const override {
        if (data_.empty()) {
            return std::nullopt;
        }
        return data_.front();
    }
    
    std::optional<T> last() const override {
        if (data_.empty()) {
            return std::nullopt;
        }
        return data_.back();
    }
    
    std::shared_ptr<LazySequence<T>> lazy() const override;
    
    std::shared_ptr<Collection<T>> parallel() const override {
        return std::make_shared<VectorCollection<T>>(data_, true);
    }
    
    bool isParallel() const { return isParallel_; }
};

// Template method implementations for Collection base class
template<typename T>
template<typename U>
std::shared_ptr<Collection<U>> Collection<T>::map(std::function<U(const T&)> fn) const {
    std::vector<U> result;
    
    forEach([&result, &fn](const T& item) {
        result.push_back(fn(item));
    });
    
    return std::make_shared<VectorCollection<U>>(std::move(result));
}

template<typename T>
template<typename U>
U Collection<T>::reduce(const U& initial, std::function<U(const U&, const T&)> fn) const {
    U result = initial;
    forEach([&result, &fn](const T& item) {
        result = fn(result, item);
    });
    return result;
}

template<typename T>
template<typename K>
std::map<K, std::vector<T>> Collection<T>::groupBy(std::function<K(const T&)> keySelector) const {
    std::map<K, std::vector<T>> groups;
    
    forEach([&groups, &keySelector](const T& item) {
        K key = keySelector(item);
        groups[key].push_back(item);
    });
    
    return groups;
}

template<typename T>
template<typename U>
std::shared_ptr<Collection<std::pair<T, U>>> Collection<T>::join(
    const Collection<U>& other,
    std::function<bool(const T&, const U&)> predicate) const {
    
    std::vector<std::pair<T, U>> result;
    
    forEach([&result, &other, &predicate](const T& leftItem) {
        other.forEach([&result, &leftItem, &predicate](const U& rightItem) {
            if (predicate(leftItem, rightItem)) {
                result.push_back(std::make_pair(leftItem, rightItem));
            }
        });
    });
    
    return std::make_shared<VectorCollection<std::pair<T, U>>>(std::move(result));
}

// Helper function to create collections from vectors
template<typename T>
std::shared_ptr<Collection<T>> makeCollection(const std::vector<T>& data) {
    return std::make_shared<VectorCollection<T>>(data);
}

template<typename T>
std::shared_ptr<Collection<T>> makeCollection(std::vector<T>&& data) {
    return std::make_shared<VectorCollection<T>>(std::move(data));
}

} // namespace meld

// Include lazy sequence after collection definition
#include "meld/types/lazy_sequence.hpp"

namespace meld {

// Implementation of lazy() method (needs LazySequence definition)
template<typename T>
std::shared_ptr<LazySequence<T>> VectorCollection<T>::lazy() const {
    return std::make_shared<LazySequence<T>>(
        std::make_shared<VectorCollection<T>>(data_, isParallel_)
    );
}

} // namespace meld
