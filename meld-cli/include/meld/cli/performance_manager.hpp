#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <functional>
#include <thread>
#include <atomic>
#include <list>

namespace meld::cli {

/**
 * Performance metrics for monitoring CLI operations
 */
struct PerformanceMetrics {
    std::chrono::milliseconds startup_time{0};
    std::chrono::milliseconds command_execution_time{0};
    size_t memory_usage_bytes = 0;
    size_t cache_hit_count = 0;
    size_t cache_miss_count = 0;
    size_t files_processed = 0;
    
    double get_cache_hit_ratio() const {
        size_t total = cache_hit_count + cache_miss_count;
        return total > 0 ? static_cast<double>(cache_hit_count) / total : 0.0;
    }
};

/**
 * Cache entry with TTL and access tracking
 */
template<typename T>
struct CacheEntry {
    T value;
    std::chrono::steady_clock::time_point created_at;
    std::chrono::steady_clock::time_point last_accessed;
    std::chrono::milliseconds ttl;
    size_t access_count = 0;
    
    bool is_expired() const {
        auto now = std::chrono::steady_clock::now();
        return (now - created_at) > ttl;
    }
    
    void touch() {
        last_accessed = std::chrono::steady_clock::now();
        ++access_count;
    }
};

/**
 * Thread-safe LRU cache with TTL support
 */
template<typename K, typename V>
class LRUCache {
public:
    explicit LRUCache(size_t max_size, std::chrono::milliseconds default_ttl = std::chrono::minutes(10))
        : max_size_(max_size), default_ttl_(default_ttl) {}
    
    void put(const K& key, const V& value, std::optional<std::chrono::milliseconds> ttl = std::nullopt) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto now = std::chrono::steady_clock::now();
        auto entry_ttl = ttl.value_or(default_ttl_);
        
        cache_[key] = CacheEntry<V>{value, now, now, entry_ttl, 0};
        access_order_.remove(key);
        access_order_.push_front(key);
        
        // Evict if necessary
        while (cache_.size() > max_size_) {
            evict_lru();
        }
    }
    
    std::optional<V> get(const K& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = cache_.find(key);
        if (it == cache_.end()) {
            return std::nullopt;
        }
        
        if (it->second.is_expired()) {
            cache_.erase(it);
            access_order_.remove(key);
            return std::nullopt;
        }
        
        it->second.touch();
        access_order_.remove(key);
        access_order_.push_front(key);
        
        return it->second.value;
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_.clear();
        access_order_.clear();
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_.size();
    }
    
    void cleanup_expired() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = cache_.begin();
        while (it != cache_.end()) {
            if (it->second.is_expired()) {
                access_order_.remove(it->first);
                it = cache_.erase(it);
            } else {
                ++it;
            }
        }
    }

private:
    mutable std::mutex mutex_;
    size_t max_size_;
    std::chrono::milliseconds default_ttl_;
    std::unordered_map<K, CacheEntry<V>> cache_;
    std::list<K> access_order_;
    
    void evict_lru() {
        if (!access_order_.empty()) {
            K lru_key = access_order_.back();
            access_order_.pop_back();
            cache_.erase(lru_key);
        }
    }
};

/**
 * Resource pool for reusing expensive objects
 */
template<typename T>
class ResourcePool {
public:
    using Factory = std::function<std::unique_ptr<T>()>;
    using Validator = std::function<bool(const T&)>;
    
    explicit ResourcePool(Factory factory, size_t max_size = 10, Validator validator = nullptr)
        : factory_(std::move(factory)), max_size_(max_size), validator_(std::move(validator)) {}
    
    std::unique_ptr<T> acquire() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Try to reuse an existing resource
        while (!pool_.empty()) {
            auto resource = std::move(pool_.back());
            pool_.pop_back();
            
            if (!validator_ || validator_(*resource)) {
                return resource;
            }
        }
        
        // Create new resource if pool is empty
        return factory_();
    }
    
    void release(std::unique_ptr<T> resource) {
        if (!resource) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (pool_.size() < max_size_ && (!validator_ || validator_(*resource))) {
            pool_.push_back(std::move(resource));
        }
        // Resource will be destroyed if pool is full or invalid
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        pool_.clear();
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return pool_.size();
    }

private:
    mutable std::mutex mutex_;
    Factory factory_;
    size_t max_size_;
    Validator validator_;
    std::vector<std::unique_ptr<T>> pool_;
};

/**
 * Performance manager for CLI operations
 */
class PerformanceManager {
public:
    PerformanceManager();
    ~PerformanceManager();
    
    // Startup optimization
    void start_startup_timer();
    void end_startup_timer();
    
    // Command execution timing
    void start_command_timer();
    void end_command_timer();
    
    // Memory monitoring
    void update_memory_usage();
    size_t get_current_memory_usage() const;
    
    // Cache management
    template<typename T>
    void cache_put(const std::string& key, const T& value, std::optional<std::chrono::milliseconds> ttl = std::nullopt);
    
    template<typename T>
    std::optional<T> cache_get(const std::string& key);
    
    void record_cache_hit();
    void record_cache_miss();
    
    // Resource management
    void start_background_cleanup();
    void stop_background_cleanup();
    
    // Metrics
    const PerformanceMetrics& get_metrics() const { return metrics_; }
    void reset_metrics();
    
    // Configuration
    void set_cache_size(size_t size);
    void set_cache_ttl(std::chrono::milliseconds ttl);
    void enable_memory_monitoring(bool enable);
    
    // Large project optimization
    void set_batch_size(size_t size) { batch_size_ = size; }
    size_t get_batch_size() const { return batch_size_; }
    
    void set_max_concurrent_operations(size_t max) { max_concurrent_operations_ = max; }
    size_t get_max_concurrent_operations() const { return max_concurrent_operations_; }

private:
    mutable std::mutex metrics_mutex_;
    PerformanceMetrics metrics_;
    
    // Timing
    std::chrono::steady_clock::time_point startup_start_;
    std::chrono::steady_clock::time_point command_start_;
    
    // Caching
    std::unique_ptr<LRUCache<std::string, std::string>> string_cache_;
    std::chrono::milliseconds default_cache_ttl_;
    
    // Background cleanup
    std::atomic<bool> cleanup_running_{false};
    std::unique_ptr<std::thread> cleanup_thread_;
    std::mutex cleanup_mutex_;
    std::condition_variable cleanup_cv_;
    
    // Memory monitoring
    std::atomic<bool> memory_monitoring_enabled_{true};
    std::atomic<size_t> current_memory_usage_{0};
    
    // Large project handling
    size_t batch_size_ = 100;
    size_t max_concurrent_operations_ = std::thread::hardware_concurrency();
    
    void cleanup_worker();
    size_t calculate_memory_usage() const;
};

/**
 * RAII timer for automatic performance measurement
 */
class ScopedTimer {
public:
    explicit ScopedTimer(std::function<void(std::chrono::milliseconds)> callback)
        : callback_(std::move(callback)), start_(std::chrono::steady_clock::now()) {}
    
    ~ScopedTimer() {
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start_);
        if (callback_) {
            callback_(duration);
        }
    }

private:
    std::function<void(std::chrono::milliseconds)> callback_;
    std::chrono::steady_clock::time_point start_;
};

/**
 * Lazy loader for expensive resources
 */
template<typename T>
class LazyLoader {
public:
    using Factory = std::function<std::unique_ptr<T>()>;
    
    explicit LazyLoader(Factory factory) : factory_(std::move(factory)) {}
    
    T& get() {
        std::call_once(initialized_, [this]() {
            resource_ = factory_();
        });
        return *resource_;
    }
    
    bool is_loaded() const {
        return resource_ != nullptr;
    }
    
    void reset() {
        resource_.reset();
        initialized_ = std::once_flag{};
    }

private:
    Factory factory_;
    std::once_flag initialized_;
    std::unique_ptr<T> resource_;
};

// Template implementations
template<typename T>
void PerformanceManager::cache_put(const std::string& key, const T& value, std::optional<std::chrono::milliseconds> ttl) {
    // For now, only support string caching. Can be extended for other types.
    if constexpr (std::is_same_v<T, std::string>) {
        string_cache_->put(key, value, ttl);
    }
}

template<typename T>
std::optional<T> PerformanceManager::cache_get(const std::string& key) {
    // For now, only support string caching. Can be extended for other types.
    if constexpr (std::is_same_v<T, std::string>) {
        auto result = string_cache_->get(key);
        if (result.has_value()) {
            record_cache_hit();
            return result;
        } else {
            record_cache_miss();
            return std::nullopt;
        }
    }
    return std::nullopt;
}

} // namespace meld::cli