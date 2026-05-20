#include "meld/types/memory.hpp"
#include <algorithm>
#include <unordered_set>

namespace meld::types {

// HeapAllocator implementation

void HeapAllocator::register_allocation(std::shared_ptr<ManagedObject> obj) {
    std::lock_guard<std::mutex> lock(mutex_);
    allocations_[obj.get()] = obj;
    // Estimate size (simplified)
    total_bytes_ += sizeof(ManagedObject);
}

void HeapAllocator::unregister_allocation(ManagedObject* obj) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = allocations_.find(obj);
    if (it != allocations_.end()) {
        allocations_.erase(it);
        total_bytes_ -= sizeof(ManagedObject);
    }
}

size_t HeapAllocator::allocated_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return allocations_.size();
}

size_t HeapAllocator::total_allocated_bytes() const {
    return total_bytes_.load();
}

void HeapAllocator::collect_garbage() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Remove expired weak pointers
    std::vector<ManagedObject*> to_remove;
    for (auto& [ptr, weak] : allocations_) {
        if (weak.expired()) {
            to_remove.push_back(ptr);
        }
    }
    
    for (auto* ptr : to_remove) {
        allocations_.erase(ptr);
    }
}

void HeapAllocator::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    allocations_.clear();
    total_bytes_ = 0;
}

// CycleDetector implementation

bool CycleDetector::has_cycle(ManagedObject* obj) {
    if (!obj) return false;
    
    std::unordered_map<ManagedObject*, int> visited;
    std::vector<ManagedObject*> path;
    
    return dfs(obj, visited, path);
}

std::vector<ManagedObject*> CycleDetector::find_cycle(ManagedObject* obj) {
    if (!obj) return {};
    
    std::unordered_map<ManagedObject*, int> visited;
    std::vector<ManagedObject*> path;
    
    if (dfs(obj, visited, path)) {
        return path;
    }
    
    return {};
}

bool CycleDetector::dfs(ManagedObject* obj,
                       std::unordered_map<ManagedObject*, int>& visited,
                       std::vector<ManagedObject*>& path) {
    if (!obj) return false;
    
    // Check if we've seen this object before
    auto it = visited.find(obj);
    if (it != visited.end()) {
        if (it->second == 1) {
            // Found a cycle - object is in current path
            return true;
        }
        // Already fully explored
        return false;
    }
    
    // Mark as being explored
    visited[obj] = 1;
    path.push_back(obj);
    
    // In a full implementation, we would traverse all references from this object
    // For now, this is a simplified version
    
    // Mark as fully explored
    visited[obj] = 2;
    path.pop_back();
    
    return false;
}

} // namespace meld::types
