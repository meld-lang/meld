#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <vector>
#include <set>
#include <map>
#include <utility>
#include <algorithm>

namespace meld {

// Forward declaration
template<typename T>
class Collection;

template<typename T>
class VectorCollection;

// Lazy sequence - defers execution until terminal operation
template<typename T>
class LazySequence {
private:
    // Operation types
    enum class OpType {
        MAP,
        FILTER,
        TAKE,
        SORTED
    };
    
    struct Operation {
        OpType type;
        std::function<T(const T&)> mapFn;
        std::function<bool(const T&)> filterFn;
        std::function<bool(const T&, const T&)> comparatorFn;
        size_t takeCount;
        
        Operation(OpType t) : type(t), takeCount(0) {}
    };
    
    std::shared_ptr<Collection<T>> source_;
    std::vector<Operation> operations_;
    bool isParallel_;
    
public:
    explicit LazySequence(std::shared_ptr<Collection<T>> source, bool parallel = false)
        : source_(source), isParallel_(parallel) {}
    
    // Intermediate operations (lazy - return new LazySequence)
    std::shared_ptr<LazySequence<T>> map(std::function<T(const T&)> fn) {
        auto newSeq = std::make_shared<LazySequence<T>>(source_, isParallel_);
        newSeq->operations_ = operations_;
        
        Operation op(OpType::MAP);
        op.mapFn = fn;
        newSeq->operations_.push_back(op);
        
        return newSeq;
    }
    
    template<typename U>
    std::shared_ptr<LazySequence<U>> map(std::function<U(const T&)> fn);
    
    std::shared_ptr<LazySequence<T>> filter(std::function<bool(const T&)> predicate) {
        auto newSeq = std::make_shared<LazySequence<T>>(source_, isParallel_);
        newSeq->operations_ = operations_;
        
        Operation op(OpType::FILTER);
        op.filterFn = predicate;
        newSeq->operations_.push_back(op);
        
        return newSeq;
    }
    
    std::shared_ptr<LazySequence<T>> take(size_t n) {
        auto newSeq = std::make_shared<LazySequence<T>>(source_, isParallel_);
        newSeq->operations_ = operations_;
        
        Operation op(OpType::TAKE);
        op.takeCount = n;
        newSeq->operations_.push_back(op);
        
        return newSeq;
    }
    
    std::shared_ptr<LazySequence<T>> sorted() {
        auto newSeq = std::make_shared<LazySequence<T>>(source_, isParallel_);
        newSeq->operations_ = operations_;
        
        Operation op(OpType::SORTED);
        newSeq->operations_.push_back(op);
        
        return newSeq;
    }
    
    std::shared_ptr<LazySequence<T>> sorted(std::function<bool(const T&, const T&)> comparator) {
        auto newSeq = std::make_shared<LazySequence<T>>(source_, isParallel_);
        newSeq->operations_ = operations_;
        
        Operation op(OpType::SORTED);
        op.comparatorFn = comparator;
        newSeq->operations_.push_back(op);
        
        return newSeq;
    }
    
    std::shared_ptr<LazySequence<T>> parallel() {
        auto newSeq = std::make_shared<LazySequence<T>>(source_, true);
        newSeq->operations_ = operations_;
        return newSeq;
    }
    
    // Grouping operation (lazy)
    template<typename K>
    std::shared_ptr<LazySequence<std::pair<K, std::vector<T>>>> groupBy(std::function<K(const T&)> keySelector) {
        // For groupBy, we need to evaluate and create a new sequence
        auto evaluated = evaluate();
        std::map<K, std::vector<T>> groups;
        
        for (const auto& item : evaluated) {
            K key = keySelector(item);
            groups[key].push_back(item);
        }
        
        std::vector<std::pair<K, std::vector<T>>> result;
        for (auto& pair : groups) {
            result.emplace_back(std::move(pair.first), std::move(pair.second));
        }
        
        auto newCollection = std::make_shared<VectorCollection<std::pair<K, std::vector<T>>>>(std::move(result), isParallel_);
        return std::make_shared<LazySequence<std::pair<K, std::vector<T>>>>(newCollection, isParallel_);
    }
    
    // Join operation (lazy)
    template<typename U>
    std::shared_ptr<LazySequence<std::pair<T, U>>> join(
        std::shared_ptr<Collection<U>> other,
        std::function<bool(const T&, const U&)> predicate) {
        
        // For join, we need to evaluate both sequences
        auto leftData = evaluate();
        auto rightData = other->toList();
        
        std::vector<std::pair<T, U>> result;
        for (const auto& leftItem : leftData) {
            for (const auto& rightItem : rightData) {
                if (predicate(leftItem, rightItem)) {
                    result.emplace_back(leftItem, rightItem);
                }
            }
        }
        
        auto newCollection = std::make_shared<VectorCollection<std::pair<T, U>>>(std::move(result), isParallel_);
        return std::make_shared<LazySequence<std::pair<T, U>>>(newCollection, isParallel_);
    }
    
    // Terminal operations (eager - trigger evaluation)
    std::vector<T> toList() const {
        return evaluate();
    }
    
    std::set<T> toSet() const {
        auto result = evaluate();
        return std::set<T>(result.begin(), result.end());
    }
    
    std::shared_ptr<Collection<T>> toCollection() const {
        return std::make_shared<VectorCollection<T>>(evaluate(), isParallel_);
    }
    
    void forEach(std::function<void(const T&)> fn) const {
        auto result = evaluate();
        for (const auto& item : result) {
            fn(item);
        }
    }
    
    T reduce(const T& initial, std::function<T(const T&, const T&)> fn) const {
        auto result = evaluate();
        T acc = initial;
        for (const auto& item : result) {
            acc = fn(acc, item);
        }
        return acc;
    }
    
    template<typename U>
    U reduce(const U& initial, std::function<U(const U&, const T&)> fn) const {
        auto result = evaluate();
        U acc = initial;
        for (const auto& item : result) {
            acc = fn(acc, item);
        }
        return acc;
    }
    
    size_t count() const {
        return evaluate().size();
    }
    
    bool isEmpty() const {
        return evaluate().empty();
    }
    
    std::optional<T> first() const {
        auto result = evaluate();
        if (result.empty()) {
            return std::nullopt;
        }
        return result.front();
    }
    
    std::optional<T> last() const {
        auto result = evaluate();
        if (result.empty()) {
            return std::nullopt;
        }
        return result.back();
    }
    
private:
    // Evaluate the lazy sequence by applying all operations
    std::vector<T> evaluate() const {
        // Start with source data
        std::vector<T> result = source_->toList();
        
        // Apply each operation in sequence
        for (const auto& op : operations_) {
            switch (op.type) {
                case OpType::MAP:
                    for (auto& item : result) {
                        item = op.mapFn(item);
                    }
                    break;
                    
                case OpType::FILTER: {
                    std::vector<T> filtered;
                    for (const auto& item : result) {
                        if (op.filterFn(item)) {
                            filtered.push_back(item);
                        }
                    }
                    result = std::move(filtered);
                    break;
                }
                    
                case OpType::TAKE: {
                    if (result.size() > op.takeCount) {
                        result.resize(op.takeCount);
                    }
                    break;
                }
                    
                case OpType::SORTED:
                    if (op.comparatorFn) {
                        std::sort(result.begin(), result.end(), op.comparatorFn);
                    } else {
                        std::sort(result.begin(), result.end());
                    }
                    break;
            }
        }
        
        return result;
    }
};

// Template specialization for type-transforming map
template<typename T>
template<typename U>
std::shared_ptr<LazySequence<U>> LazySequence<T>::map(std::function<U(const T&)> fn) {
    // For type transformation, we need to evaluate and create a new sequence
    auto evaluated = evaluate();
    std::vector<U> transformed;
    transformed.reserve(evaluated.size());
    
    for (const auto& item : evaluated) {
        transformed.push_back(fn(item));
    }
    
    auto newCollection = std::make_shared<VectorCollection<U>>(std::move(transformed), isParallel_);
    return std::make_shared<LazySequence<U>>(newCollection, isParallel_);
}

} // namespace meld
