#include "meld/cli/performance_manager.hpp"
#include <thread>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#include <fstream>
#endif

namespace meld::cli {

PerformanceManager::PerformanceManager() 
    : string_cache_(std::make_unique<LRUCache<std::string, std::string>>(1000, std::chrono::minutes(10)))
    , default_cache_ttl_(std::chrono::minutes(10)) {
    
    // Initialize with reasonable defaults
    metrics_ = PerformanceMetrics{};
    
    // Start background cleanup if needed
    start_background_cleanup();
}

PerformanceManager::~PerformanceManager() {
    stop_background_cleanup();
}

void PerformanceManager::start_startup_timer() {
    startup_start_ = std::chrono::steady_clock::now();
}

void PerformanceManager::end_startup_timer() {
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - startup_start_);
    
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    metrics_.startup_time = duration;
}

void PerformanceManager::start_command_timer() {
    command_start_ = std::chrono::steady_clock::now();
}

void PerformanceManager::end_command_timer() {
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - command_start_);
    
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    metrics_.command_execution_time = duration;
}

void PerformanceManager::update_memory_usage() {
    if (!memory_monitoring_enabled_.load()) {
        return;
    }
    
    size_t usage = calculate_memory_usage();
    current_memory_usage_.store(usage);
    
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    metrics_.memory_usage_bytes = usage;
}

size_t PerformanceManager::get_current_memory_usage() const {
    return current_memory_usage_.load();
}

void PerformanceManager::record_cache_hit() {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    ++metrics_.cache_hit_count;
}

void PerformanceManager::record_cache_miss() {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    ++metrics_.cache_miss_count;
}

void PerformanceManager::start_background_cleanup() {
    if (cleanup_running_.load()) {
        return;
    }
    
    cleanup_running_.store(true);
    cleanup_thread_ = std::make_unique<std::thread>(&PerformanceManager::cleanup_worker, this);
}

void PerformanceManager::stop_background_cleanup() {
    if (!cleanup_running_.load()) {
        return;
    }
    
    cleanup_running_.store(false);
    {
        std::lock_guard<std::mutex> lock(cleanup_mutex_);
        cleanup_cv_.notify_all();
    }
    if (cleanup_thread_ && cleanup_thread_->joinable()) {
        cleanup_thread_->join();
    }
    cleanup_thread_.reset();
}

void PerformanceManager::reset_metrics() {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    metrics_ = PerformanceMetrics{};
}

void PerformanceManager::set_cache_size(size_t size) {
    // Recreate cache with new size
    string_cache_ = std::make_unique<LRUCache<std::string, std::string>>(size, default_cache_ttl_);
}

void PerformanceManager::set_cache_ttl(std::chrono::milliseconds ttl) {
    default_cache_ttl_ = ttl;
    // Recreate cache with new TTL
    size_t current_size = string_cache_->size();
    string_cache_ = std::make_unique<LRUCache<std::string, std::string>>(1000, ttl);
}

void PerformanceManager::enable_memory_monitoring(bool enable) {
    memory_monitoring_enabled_.store(enable);
}

void PerformanceManager::cleanup_worker() {
    while (cleanup_running_.load()) {
        // Clean up expired cache entries
        string_cache_->cleanup_expired();
        
        // Update memory usage
        update_memory_usage();
        
        // Sleep for cleanup interval, but wake immediately on stop
        std::unique_lock<std::mutex> lock(cleanup_mutex_);
        cleanup_cv_.wait_for(lock, std::chrono::seconds(30),
            [this] { return !cleanup_running_.load(); });
    }
}

size_t PerformanceManager::calculate_memory_usage() const {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
    return 0;
#else
    // Try to read from /proc/self/status first (Linux)
    std::ifstream status_file("/proc/self/status");
    if (status_file.is_open()) {
        std::string line;
        while (std::getline(status_file, line)) {
            if (line.substr(0, 6) == "VmRSS:") {
                size_t kb = 0;
                if (sscanf(line.c_str(), "VmRSS: %zu kB", &kb) == 1) {
                    return kb * 1024; // Convert KB to bytes
                }
            }
        }
    }
    
    // Fallback to getrusage
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        // ru_maxrss is in KB on Linux, bytes on macOS
        #ifdef __APPLE__
        return usage.ru_maxrss;
        #else
        return usage.ru_maxrss * 1024;
        #endif
    }
    
    return 0;
#endif
}

} // namespace meld::cli