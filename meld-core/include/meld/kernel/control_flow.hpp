#pragma once

#include "meld/kernel/primitives.hpp"
#include "meld/kernel/extension_registry.hpp"
#include <functional>
#include <memory>

namespace meld::kernel {

// Forward declarations
class IfTrueBuilder;
class Range;
class Block;

// IfTrueBuilder - Helper class for chaining Boolean.ifTrue:ifFalse:
class IfTrueBuilder {
public:
    IfTrueBuilder(bool condition, std::function<Value()> true_block)
        : condition_(condition), true_block_(std::move(true_block)) {}
    
    // ifFalse: method for chaining
    Value ifFalse(std::function<Value()> false_block) {
        if (condition_) {
            return true_block_();
        } else {
            return false_block();
        }
    }
    
    std::string to_string() const {
        return "<IfTrueBuilder>";
    }
    
private:
    bool condition_;
    std::function<Value()> true_block_;
};

// Range - Represents a range of integers for iteration
class Range {
public:
    Range(int64_t start, int64_t end) : start_(start), end_(end) {}
    
    int64_t start() const { return start_; }
    int64_t end() const { return end_; }
    
    // do: method for iteration
    void do_iterate(std::function<void(int64_t)> block) {
        for (int64_t i = start_; i <= end_; ++i) {
            block(i);
        }
    }
    
    std::string to_string() const {
        return std::format("Range({}, {})", start_, end_);
    }
    
private:
    int64_t start_;
    int64_t end_;
};

// Block - Represents a closure/lambda for control flow
class Block {
public:
    explicit Block(std::function<bool()> condition)
        : condition_(std::move(condition)) {}
    
    // whileTrue: method for conditional loops
    void whileTrue(std::function<void()> body) {
        while (condition_()) {
            body();
        }
    }
    
    std::string to_string() const {
        return "<Block>";
    }
    
private:
    std::function<bool()> condition_;
};

// Collection - Basic collection type for forEach: method
template<typename T>
class Collection {
public:
    explicit Collection(std::vector<T> items) : items_(std::move(items)) {}
    
    // forEach: method for iteration
    void forEach(std::function<void(const T&)> block) {
        for (const auto& item : items_) {
            block(item);
        }
    }
    
    const std::vector<T>& items() const { return items_; }
    
    std::string to_string() const {
        return std::format("Collection(size={})", items_.size());
    }
    
private:
    std::vector<T> items_;
};

// Register all Smalltalk-style control flow extensions
void register_control_flow_extensions();

} // namespace meld::kernel
