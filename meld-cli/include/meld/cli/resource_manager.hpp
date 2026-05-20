#pragma once

#include <memory>
#include <vector>
#include <functional>
#include <mutex>
#include <atomic>
#include <string>
#include <unordered_map>
#include <thread>
#include <condition_variable>
#include <queue>
#include <cstdio>

namespace meld::cli {

/**
 * Resource cleanup callback
 */
using CleanupCallback = std::function<void()>;

/**
 * Resource handle for tracking managed resources
 */
class ResourceHandle {
public:
    explicit ResourceHandle(const std::string& name, CleanupCallback cleanup = nullptr)
        : name_(name), cleanup_(std::move(cleanup)), active_(true) {}
    
    ~ResourceHandle() {
        release();
    }
    
    // Non-copyable but movable
    ResourceHandle(const ResourceHandle&) = delete;
    ResourceHandle& operator=(const ResourceHandle&) = delete;
    
    ResourceHandle(ResourceHandle&& other) noexcept
        : name_(std::move(other.name_))
        , cleanup_(std::move(other.cleanup_))
        , active_(other.active_.load()) {
        other.active_.store(false);
    }
    
    ResourceHandle& operator=(ResourceHandle&& other) noexcept {
        if (this != &other) {
            release();
            name_ = std::move(other.name_);
            cleanup_ = std::move(other.cleanup_);
            active_.store(other.active_.load());
            other.active_.store(false);
        }
        return *this;
    }
    
    void release() {
        if (active_.exchange(false) && cleanup_) {
            cleanup_();
        }
    }
    
    bool is_active() const { return active_.load(); }
    const std::string& name() const { return name_; }

private:
    std::string name_;
    CleanupCallback cleanup_;
    std::atomic<bool> active_;
};

/**
 * RAII scope guard for automatic cleanup
 */
class ScopeGuard {
public:
    explicit ScopeGuard(CleanupCallback cleanup) : cleanup_(std::move(cleanup)) {}
    
    ~ScopeGuard() {
        if (cleanup_) {
            cleanup_();
        }
    }
    
    // Non-copyable, non-movable
    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;
    ScopeGuard(ScopeGuard&&) = delete;
    ScopeGuard& operator=(ScopeGuard&&) = delete;
    
    void dismiss() {
        cleanup_ = nullptr;
    }

private:
    CleanupCallback cleanup_;
};

/**
 * Thread pool for background operations
 */
class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency());
    ~ThreadPool();
    
    // Submit a task to the thread pool
    template<typename F>
    void submit(F&& task);
    
    // Wait for all tasks to complete
    void wait_for_all();
    
    // Shutdown the thread pool
    void shutdown();
    
    size_t size() const { return threads_.size(); }
    size_t pending_tasks() const;

private:
    std::vector<std::thread> threads_;
    std::queue<std::function<void()>> tasks_;
    mutable std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::atomic<bool> shutdown_requested_{false};
    std::atomic<size_t> active_tasks_{0};
    
    void worker_thread();
};

/**
 * Resource manager for CLI application lifecycle
 */
class ResourceManager {
public:
    ResourceManager();
    ~ResourceManager();
    
    // Resource registration
    ResourceHandle register_resource(const std::string& name, CleanupCallback cleanup = nullptr);
    void register_cleanup(CleanupCallback cleanup);
    
    // Shutdown handling
    void register_shutdown_handler(std::function<void()> handler);
    void shutdown();
    bool is_shutdown() const { return shutdown_requested_.load(); }
    
    // Thread pool access
    ThreadPool& get_thread_pool() { return thread_pool_; }
    
    // Memory management
    void set_memory_limit(size_t bytes) { memory_limit_ = bytes; }
    size_t get_memory_limit() const { return memory_limit_; }
    bool check_memory_limit() const;
    
    // File handle management
    void register_file_handle(const std::string& path, std::shared_ptr<std::FILE> file);
    void close_file_handle(const std::string& path);
    void close_all_file_handles();
    
    // Temporary file management
    std::string create_temp_file(const std::string& prefix = "meld_");
    void cleanup_temp_files();
    
    // Resource monitoring
    size_t get_active_resource_count() const;
    std::vector<std::string> get_active_resource_names() const;
    
    // Emergency cleanup (for signal handlers)
    void emergency_cleanup();

private:
    mutable std::mutex resources_mutex_;
    std::vector<std::unique_ptr<ResourceHandle>> resources_;
    std::vector<CleanupCallback> cleanup_callbacks_;
    std::vector<std::function<void()>> shutdown_handlers_;
    
    ThreadPool thread_pool_;
    std::atomic<bool> shutdown_requested_{false};
    
    // Memory management
    size_t memory_limit_ = 0; // 0 means no limit
    
    // File handle management
    mutable std::mutex files_mutex_;
    std::unordered_map<std::string, std::shared_ptr<std::FILE>> open_files_;
    
    // Temporary file management
    mutable std::mutex temp_files_mutex_;
    std::vector<std::string> temp_files_;
    
    void cleanup_all_resources();
    std::string generate_temp_filename(const std::string& prefix) const;
};

/**
 * Large project processor for handling big codebases efficiently
 */
class LargeProjectProcessor {
public:
    explicit LargeProjectProcessor(ResourceManager& resource_manager, size_t batch_size = 100)
        : resource_manager_(resource_manager), batch_size_(batch_size) {}
    
    // Process files in batches
    template<typename FileProcessor>
    void process_files_batched(const std::vector<std::string>& files, FileProcessor processor);
    
    // Stream processing for very large files
    template<typename LineProcessor>
    void process_file_streaming(const std::string& file_path, LineProcessor processor);
    
    // Parallel directory traversal
    std::vector<std::string> find_files_parallel(const std::string& root_path, 
                                                const std::string& pattern = "*.meld");
    
    // Memory-efficient file reading
    class FileReader {
    public:
        explicit FileReader(const std::string& path, size_t buffer_size = 64 * 1024);
        ~FileReader();
        
        bool read_chunk(std::string& chunk);
        bool read_line(std::string& line);
        bool is_eof() const;
        void reset();
        
    private:
        std::string path_;
        std::unique_ptr<std::FILE, decltype(&std::fclose)> file_;
        std::vector<char> buffer_;
        size_t buffer_pos_ = 0;
        size_t buffer_size_ = 0;
        bool eof_ = false;
    };
    
    void set_batch_size(size_t size) { batch_size_ = size; }
    size_t get_batch_size() const { return batch_size_; }

private:
    ResourceManager& resource_manager_;
    size_t batch_size_;
};

// Template implementations

template<typename F>
void ThreadPool::submit(F&& task) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (shutdown_requested_.load()) {
            return; // Don't accept new tasks after shutdown
        }
        tasks_.emplace(std::forward<F>(task));
    }
    condition_.notify_one();
}

template<typename FileProcessor>
void LargeProjectProcessor::process_files_batched(const std::vector<std::string>& files, FileProcessor processor) {
    auto& thread_pool = resource_manager_.get_thread_pool();
    
    for (size_t i = 0; i < files.size(); i += batch_size_) {
        size_t end = std::min(i + batch_size_, files.size());
        std::vector<std::string> batch(files.begin() + i, files.begin() + end);
        
        thread_pool.submit([batch = std::move(batch), processor]() {
            for (const auto& file : batch) {
                if (!processor(file)) {
                    break; // Allow early termination
                }
            }
        });
    }
    
    thread_pool.wait_for_all();
}

template<typename LineProcessor>
void LargeProjectProcessor::process_file_streaming(const std::string& file_path, LineProcessor processor) {
    FileReader reader(file_path);
    std::string line;
    size_t line_number = 0;
    
    while (reader.read_line(line)) {
        ++line_number;
        if (!processor(line, line_number)) {
            break; // Allow early termination
        }
    }
}

} // namespace meld::cli