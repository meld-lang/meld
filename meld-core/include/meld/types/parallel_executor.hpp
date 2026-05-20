#pragma once

#include <functional>
#include <vector>
#include <thread>
#include <future>
#include <algorithm>
#include <atomic>

#if __cplusplus >= 201703L && !defined(__APPLE__)
#include <execution>
#endif

namespace meld {

// Parallel execution engine for collection operations
class ParallelExecutor {
public:
    // Execute a function on each element in parallel
    template<typename T>
    static void forEach(const std::vector<T>& data, std::function<void(const T&)> fn) {
        size_t numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) numThreads = 4;  // Default fallback
        
        size_t chunkSize = (data.size() + numThreads - 1) / numThreads;
        std::vector<std::thread> threads;
        
        for (size_t i = 0; i < numThreads && i * chunkSize < data.size(); ++i) {
            size_t start = i * chunkSize;
            size_t end = std::min(start + chunkSize, data.size());
            
            threads.emplace_back([&data, fn, start, end]() {
                for (size_t j = start; j < end; ++j) {
                    fn(data[j]);
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
    }
    
    // Map operation in parallel
    template<typename T, typename U>
    static std::vector<U> map(const std::vector<T>& data, std::function<U(const T&)> fn) {
        std::vector<U> result(data.size());
        
        size_t numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) numThreads = 4;
        
        size_t chunkSize = (data.size() + numThreads - 1) / numThreads;
        std::vector<std::thread> threads;
        
        for (size_t i = 0; i < numThreads && i * chunkSize < data.size(); ++i) {
            size_t start = i * chunkSize;
            size_t end = std::min(start + chunkSize, data.size());
            
            threads.emplace_back([&data, &result, fn, start, end]() {
                for (size_t j = start; j < end; ++j) {
                    result[j] = fn(data[j]);
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        return result;
    }
    
    // Filter operation in parallel
    template<typename T>
    static std::vector<T> filter(const std::vector<T>& data, std::function<bool(const T&)> predicate) {
        size_t numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) numThreads = 4;
        
        size_t chunkSize = (data.size() + numThreads - 1) / numThreads;
        std::vector<std::future<std::vector<T>>> futures;
        
        for (size_t i = 0; i < numThreads && i * chunkSize < data.size(); ++i) {
            size_t start = i * chunkSize;
            size_t end = std::min(start + chunkSize, data.size());
            
            futures.push_back(std::async(std::launch::async, [&data, predicate, start, end]() {
                std::vector<T> localResult;
                for (size_t j = start; j < end; ++j) {
                    if (predicate(data[j])) {
                        localResult.push_back(data[j]);
                    }
                }
                return localResult;
            }));
        }
        
        // Combine results
        std::vector<T> result;
        for (auto& future : futures) {
            auto localResult = future.get();
            result.insert(result.end(), localResult.begin(), localResult.end());
        }
        
        return result;
    }
    
    // Reduce operation in parallel
    template<typename T, typename U>
    static U reduce(const std::vector<T>& data, const U& initial, std::function<U(const U&, const T&)> fn) {
        if (data.empty()) {
            return initial;
        }
        
        size_t numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) numThreads = 4;
        
        size_t chunkSize = (data.size() + numThreads - 1) / numThreads;
        std::vector<std::future<U>> futures;
        
        for (size_t i = 0; i < numThreads && i * chunkSize < data.size(); ++i) {
            size_t start = i * chunkSize;
            size_t end = std::min(start + chunkSize, data.size());
            
            futures.push_back(std::async(std::launch::async, [&data, initial, fn, start, end]() {
                U localResult = initial;
                for (size_t j = start; j < end; ++j) {
                    localResult = fn(localResult, data[j]);
                }
                return localResult;
            }));
        }
        
        // Combine partial results
        U result = initial;
        for (auto& future : futures) {
            U partialResult = future.get();
            if constexpr (std::is_arithmetic_v<U>) { result = result + partialResult - initial; } else { result = partialResult; }
        }
        
        return result;
    }
    
    // Sort operation (parallel execution policies not available on all platforms)
    template<typename T>
    static std::vector<T> sort(const std::vector<T>& data) {
        std::vector<T> result = data;
        std::sort(result.begin(), result.end());
        return result;
    }
    
    template<typename T>
    static std::vector<T> sort(const std::vector<T>& data, std::function<bool(const T&, const T&)> comparator) {
        std::vector<T> result = data;
        std::sort(result.begin(), result.end(), comparator);
        return result;
    }
};

} // namespace meld
