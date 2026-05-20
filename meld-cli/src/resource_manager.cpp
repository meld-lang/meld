#include "meld/cli/resource_manager.hpp"
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <queue>
#include <cstdio>
#include <ctime>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#endif

namespace meld::cli {

// ThreadPool implementation

ThreadPool::ThreadPool(size_t num_threads) {
    for (size_t i = 0; i < num_threads; ++i) {
        threads_.emplace_back(&ThreadPool::worker_thread, this);
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::wait_for_all() {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    condition_.wait(lock, [this] {
        return tasks_.empty() && active_tasks_.load() == 0;
    });
}

void ThreadPool::shutdown() {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        shutdown_requested_.store(true);
    }
    condition_.notify_all();
    
    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    threads_.clear();
}

size_t ThreadPool::pending_tasks() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return tasks_.size();
}

void ThreadPool::worker_thread() {
    while (true) {
        std::function<void()> task;
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            condition_.wait(lock, [this] {
                return !tasks_.empty() || shutdown_requested_.load();
            });
            
            if (shutdown_requested_.load() && tasks_.empty()) {
                break;
            }
            
            if (!tasks_.empty()) {
                task = std::move(tasks_.front());
                tasks_.pop();
                active_tasks_.fetch_add(1);
            }
        }
        
        if (task) {
            try {
                task();
            } catch (...) {
                // Swallow exceptions to prevent thread termination
            }
            active_tasks_.fetch_sub(1);
            condition_.notify_all();
        }
    }
}

// ResourceManager implementation

ResourceManager::ResourceManager() : thread_pool_(std::thread::hardware_concurrency()) {
}

ResourceManager::~ResourceManager() {
    shutdown();
}

ResourceHandle ResourceManager::register_resource(const std::string& name, CleanupCallback cleanup) {
    std::lock_guard<std::mutex> lock(resources_mutex_);
    
    auto handle = ResourceHandle(name, cleanup);
    return handle;
}

void ResourceManager::register_cleanup(CleanupCallback cleanup) {
    std::lock_guard<std::mutex> lock(resources_mutex_);
    cleanup_callbacks_.push_back(std::move(cleanup));
}

void ResourceManager::register_shutdown_handler(std::function<void()> handler) {
    std::lock_guard<std::mutex> lock(resources_mutex_);
    shutdown_handlers_.push_back(std::move(handler));
}

void ResourceManager::shutdown() {
    if (shutdown_requested_.exchange(true)) {
        return; // Already shutting down
    }
    
    // Call shutdown handlers first
    {
        std::lock_guard<std::mutex> lock(resources_mutex_);
        for (auto& handler : shutdown_handlers_) {
            try {
                handler();
            } catch (...) {
                // Continue with other handlers even if one fails
            }
        }
    }
    
    // Clean up all resources
    cleanup_all_resources();
    
    // Close all file handles
    close_all_file_handles();
    
    // Clean up temporary files
    cleanup_temp_files();
    
    // Shutdown thread pool
    thread_pool_.shutdown();
}

bool ResourceManager::check_memory_limit() const {
    if (memory_limit_ == 0) {
        return true; // No limit set
    }
    
    // This would need to be implemented with actual memory usage tracking
    // For now, just return true
    return true;
}

void ResourceManager::register_file_handle(const std::string& path, std::shared_ptr<std::FILE> file) {
    std::lock_guard<std::mutex> lock(files_mutex_);
    open_files_[path] = std::move(file);
}

void ResourceManager::close_file_handle(const std::string& path) {
    std::lock_guard<std::mutex> lock(files_mutex_);
    open_files_.erase(path);
}

void ResourceManager::close_all_file_handles() {
    std::lock_guard<std::mutex> lock(files_mutex_);
    open_files_.clear();
}

std::string ResourceManager::create_temp_file(const std::string& prefix) {
    std::string filename = generate_temp_filename(prefix);
    
    {
        std::lock_guard<std::mutex> lock(temp_files_mutex_);
        temp_files_.push_back(filename);
    }
    
    return filename;
}

void ResourceManager::cleanup_temp_files() {
    std::lock_guard<std::mutex> lock(temp_files_mutex_);
    
    for (const auto& file : temp_files_) {
        try {
            std::filesystem::remove(file);
        } catch (...) {
            // Continue with other files even if one fails
        }
    }
    
    temp_files_.clear();
}

size_t ResourceManager::get_active_resource_count() const {
    std::lock_guard<std::mutex> lock(resources_mutex_);
    return resources_.size();
}

std::vector<std::string> ResourceManager::get_active_resource_names() const {
    std::lock_guard<std::mutex> lock(resources_mutex_);
    
    std::vector<std::string> names;
    for (const auto& resource : resources_) {
        if (resource && resource->is_active()) {
            names.push_back(resource->name());
        }
    }
    
    return names;
}

void ResourceManager::emergency_cleanup() {
    // This is called from signal handlers, so we need to be very careful
    // Only do essential cleanup that's signal-safe
    
    // Close file handles
    try {
        close_all_file_handles();
    } catch (...) {}
    
    // Clean up temp files
    try {
        cleanup_temp_files();
    } catch (...) {}
}

void ResourceManager::cleanup_all_resources() {
    std::lock_guard<std::mutex> lock(resources_mutex_);
    
    // Clean up registered resources
    for (auto& resource : resources_) {
        if (resource) {
            resource->release();
        }
    }
    resources_.clear();
    
    // Call cleanup callbacks
    for (auto& cleanup : cleanup_callbacks_) {
        try {
            cleanup();
        } catch (...) {
            // Continue with other cleanups even if one fails
        }
    }
    cleanup_callbacks_.clear();
}

std::string ResourceManager::generate_temp_filename(const std::string& prefix) const {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    
    std::ostringstream oss;
    oss << std::filesystem::temp_directory_path().string() << "/";
    oss << prefix;
    
    // Add timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    oss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S_");
    
    // Add random suffix
    for (int i = 0; i < 8; ++i) {
        oss << std::hex << dis(gen);
    }
    
    oss << ".tmp";
    return oss.str();
}

// LargeProjectProcessor::FileReader implementation

LargeProjectProcessor::FileReader::FileReader(const std::string& path, size_t buffer_size)
    : path_(path)
    , file_(std::fopen(path.c_str(), "rb"), &std::fclose)
    , buffer_(buffer_size) {
    
    if (!file_) {
        eof_ = true;
    }
}

LargeProjectProcessor::FileReader::~FileReader() = default;

bool LargeProjectProcessor::FileReader::read_chunk(std::string& chunk) {
    if (eof_ || !file_) {
        return false;
    }
    
    size_t bytes_read = std::fread(buffer_.data(), 1, buffer_.size(), file_.get());
    if (bytes_read == 0) {
        eof_ = true;
        return false;
    }
    
    chunk.assign(buffer_.data(), bytes_read);
    
    if (bytes_read < buffer_.size()) {
        eof_ = true;
    }
    
    return true;
}

bool LargeProjectProcessor::FileReader::read_line(std::string& line) {
    if (eof_ || !file_) {
        return false;
    }
    
    line.clear();
    
    while (true) {
        // Refill buffer if needed
        if (buffer_pos_ >= buffer_size_) {
            buffer_size_ = std::fread(buffer_.data(), 1, buffer_.size(), file_.get());
            buffer_pos_ = 0;
            
            if (buffer_size_ == 0) {
                eof_ = true;
                return !line.empty();
            }
        }
        
        // Find newline in current buffer
        for (size_t i = buffer_pos_; i < buffer_size_; ++i) {
            char c = buffer_[i];
            if (c == '\n') {
                line.append(buffer_.data() + buffer_pos_, i - buffer_pos_);
                buffer_pos_ = i + 1;
                return true;
            } else if (c == '\r') {
                line.append(buffer_.data() + buffer_pos_, i - buffer_pos_);
                buffer_pos_ = i + 1;
                // Check for \r\n
                if (buffer_pos_ < buffer_size_ && buffer_[buffer_pos_] == '\n') {
                    buffer_pos_++;
                }
                return true;
            }
        }
        
        // No newline found, append entire buffer to line
        line.append(buffer_.data() + buffer_pos_, buffer_size_ - buffer_pos_);
        buffer_pos_ = buffer_size_;
    }
}

bool LargeProjectProcessor::FileReader::is_eof() const {
    return eof_;
}

void LargeProjectProcessor::FileReader::reset() {
    if (file_) {
        std::rewind(file_.get());
        buffer_pos_ = 0;
        buffer_size_ = 0;
        eof_ = false;
    }
}

// LargeProjectProcessor implementation

std::vector<std::string> LargeProjectProcessor::find_files_parallel(const std::string& root_path, const std::string& pattern) {
    std::vector<std::string> files;
    std::mutex files_mutex;
    
    auto& thread_pool = resource_manager_.get_thread_pool();
    
    // Simple recursive directory traversal
    // In a real implementation, this would be more sophisticated
    std::function<void(const std::string&)> traverse_directory = [&](const std::string& dir_path) {
        try {
            for (const auto& entry : std::filesystem::directory_iterator(dir_path)) {
                if (entry.is_directory()) {
                    thread_pool.submit([&traverse_directory, path = entry.path().string()]() {
                        traverse_directory(path);
                    });
                } else if (entry.is_regular_file()) {
                    std::string filename = entry.path().filename().string();
                    // Simple pattern matching (just check extension for now)
                    if (pattern == "*.meld" && filename.ends_with(".meld")) {
                        std::lock_guard<std::mutex> lock(files_mutex);
                        files.push_back(entry.path().string());
                    }
                }
            }
        } catch (const std::filesystem::filesystem_error&) {
            // Skip directories we can't access
        }
    };
    
    traverse_directory(root_path);
    thread_pool.wait_for_all();
    
    return files;
}

} // namespace meld::cli