#pragma once

#include <memory>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <vector>
#include <functional>

namespace meld::types {

// Forward declarations
class ClassInstance;
class ManagedObject;

// Weak reference to break reference cycles
template<typename T>
class WeakRef {
public:
    WeakRef() = default;
    explicit WeakRef(std::shared_ptr<T> ptr) : weak_ptr_(ptr) {}
    
    // Try to get a strong reference
    std::shared_ptr<T> lock() const {
        return weak_ptr_.lock();
    }
    
    // Check if the referenced object still exists
    bool expired() const {
        return weak_ptr_.expired();
    }
    
    // Reset the weak reference
    void reset() {
        weak_ptr_.reset();
    }
    
private:
    std::weak_ptr<T> weak_ptr_;
};

// Base class for managed objects with reference counting
class ManagedObject : public std::enable_shared_from_this<ManagedObject> {
public:
    virtual ~ManagedObject() = default;
    
    // Reference counting
    void retain() {
        ++ref_count_;
    }
    
    void release() {
        if (--ref_count_ == 0) {
            on_deallocate();
        }
    }
    
    size_t ref_count() const {
        return ref_count_.load();
    }
    
    // Weak reference support
    WeakRef<ManagedObject> weak_ref() {
        return WeakRef<ManagedObject>(shared_from_this());
    }
    
protected:
    ManagedObject() : ref_count_(1) {}
    
    // Called when object is about to be deallocated
    virtual void on_deallocate() {}
    
private:
    std::atomic<size_t> ref_count_;
};

// Heap allocator for class instances
class HeapAllocator {
public:
    static HeapAllocator& instance() {
        static HeapAllocator allocator;
        return allocator;
    }
    
    // Allocate a new object on the heap
    template<typename T, typename... Args>
    std::shared_ptr<T> allocate(Args&&... args) {
        auto ptr = std::make_shared<T>(std::forward<Args>(args)...);
        register_allocation(ptr);
        return ptr;
    }
    
    // Register an allocation for tracking
    void register_allocation(std::shared_ptr<ManagedObject> obj);
    
    // Unregister an allocation
    void unregister_allocation(ManagedObject* obj);
    
    // Get statistics
    size_t allocated_count() const;
    size_t total_allocated_bytes() const;
    
    // Garbage collection (optional - for future GC implementation)
    void collect_garbage();
    
    // Clear all allocations (for testing)
    void clear();
    
private:
    HeapAllocator() = default;
    
    mutable std::mutex mutex_;
    std::unordered_map<ManagedObject*, std::weak_ptr<ManagedObject>> allocations_;
    std::atomic<size_t> total_bytes_{0};
};

// RAII wrapper for automatic reference counting
template<typename T>
class Ref {
public:
    Ref() = default;
    
    explicit Ref(std::shared_ptr<T> ptr) : ptr_(std::move(ptr)) {
        if (ptr_) {
            if (auto* managed = dynamic_cast<ManagedObject*>(ptr_.get())) {
                managed->retain();
            }
        }
    }
    
    Ref(const Ref& other) : ptr_(other.ptr_) {
        if (ptr_) {
            if (auto* managed = dynamic_cast<ManagedObject*>(ptr_.get())) {
                managed->retain();
            }
        }
    }
    
    Ref(Ref&& other) noexcept : ptr_(std::move(other.ptr_)) {}
    
    ~Ref() {
        if (ptr_) {
            if (auto* managed = dynamic_cast<ManagedObject*>(ptr_.get())) {
                managed->release();
            }
        }
    }
    
    Ref& operator=(const Ref& other) {
        if (this != &other) {
            if (ptr_) {
                if (auto* managed = dynamic_cast<ManagedObject*>(ptr_.get())) {
                    managed->release();
                }
            }
            ptr_ = other.ptr_;
            if (ptr_) {
                if (auto* managed = dynamic_cast<ManagedObject*>(ptr_.get())) {
                    managed->retain();
                }
            }
        }
        return *this;
    }
    
    Ref& operator=(Ref&& other) noexcept {
        if (this != &other) {
            if (ptr_) {
                if (auto* managed = dynamic_cast<ManagedObject*>(ptr_.get())) {
                    managed->release();
                }
            }
            ptr_ = std::move(other.ptr_);
        }
        return *this;
    }
    
    T* get() const { return ptr_.get(); }
    T* operator->() const { return ptr_.get(); }
    T& operator*() const { return *ptr_; }
    
    explicit operator bool() const { return ptr_ != nullptr; }
    
    std::shared_ptr<T> shared_ptr() const { return ptr_; }
    
private:
    std::shared_ptr<T> ptr_;
};

// Memory pool for small object optimization
template<size_t BlockSize = 64>
class MemoryPool {
public:
    MemoryPool(size_t initial_blocks = 1024) {
        allocate_blocks(initial_blocks);
    }
    
    ~MemoryPool() {
        for (auto* block : blocks_) {
            ::operator delete(block);
        }
    }
    
    void* allocate() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (free_list_.empty()) {
            allocate_blocks(blocks_.size());
        }
        
        void* ptr = free_list_.back();
        free_list_.pop_back();
        return ptr;
    }
    
    void deallocate(void* ptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        free_list_.push_back(ptr);
    }
    
private:
    void allocate_blocks(size_t count) {
        for (size_t i = 0; i < count; ++i) {
            void* block = ::operator new(BlockSize);
            blocks_.push_back(block);
            free_list_.push_back(block);
        }
    }
    
    std::mutex mutex_;
    std::vector<void*> blocks_;
    std::vector<void*> free_list_;
};

// Cycle detector for finding reference cycles
class CycleDetector {
public:
    // Check if an object is part of a reference cycle
    static bool has_cycle(ManagedObject* obj);
    
    // Find all objects in a cycle
    static std::vector<ManagedObject*> find_cycle(ManagedObject* obj);
    
private:
    static bool dfs(ManagedObject* obj, 
                   std::unordered_map<ManagedObject*, int>& visited,
                   std::vector<ManagedObject*>& path);
};

} // namespace meld::types
