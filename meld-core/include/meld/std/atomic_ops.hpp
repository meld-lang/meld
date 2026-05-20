#pragma once

#include "meld/std/concurrency_traits.hpp"
#include <atomic>
#include <memory>
#include <type_traits>
#include <chrono>

namespace meld::stdx {

// Memory ordering enumeration matching Rust's atomic ordering
enum class MemoryOrdering {
    Relaxed,    // No synchronization or ordering constraints
    Acquire,    // Acquire load operation
    Release,    // Release store operation
    AcqRel,     // Both acquire and release
    SeqCst      // Sequential consistency (strongest)
};

// Convert our enum to std::memory_order
constexpr std::memory_order to_std_memory_order(MemoryOrdering order) {
    switch (order) {
        case MemoryOrdering::Relaxed: return std::memory_order_relaxed;
        case MemoryOrdering::Acquire: return std::memory_order_acquire;
        case MemoryOrdering::Release: return std::memory_order_release;
        case MemoryOrdering::AcqRel:  return std::memory_order_acq_rel;
        case MemoryOrdering::SeqCst:  return std::memory_order_seq_cst;
        default: return std::memory_order_seq_cst;
    }
}

// Atomic wrapper for primitive types
template<typename T>
class Atomic {
    static_assert(std::is_trivially_copyable_v<T>, "Atomic type must be trivially copyable");
    static_assert(std::is_copy_constructible_v<T>, "Atomic type must be copy constructible");
    static_assert(std::is_move_constructible_v<T>, "Atomic type must be move constructible");
    static_assert(std::is_copy_assignable_v<T>, "Atomic type must be copy assignable");
    static_assert(std::is_move_assignable_v<T>, "Atomic type must be move assignable");
    
public:
    // Constructors
    Atomic() = default;
    
    explicit Atomic(T value) : atomic_(value) {}
    
    // Copy constructor (loads the value)
    Atomic(const Atomic& other) : atomic_(other.load()) {}
    
    // Move constructor
    Atomic(Atomic&& other) noexcept : atomic_(other.load()) {}
    
    // Assignment operators
    Atomic& operator=(const Atomic& other) {
        store(other.load());
        return *this;
    }
    
    Atomic& operator=(Atomic&& other) noexcept {
        store(other.load());
        return *this;
    }
    
    Atomic& operator=(T value) {
        store(value);
        return *this;
    }
    
    // Load operations
    T load(MemoryOrdering order = MemoryOrdering::SeqCst) const {
        return atomic_.load(to_std_memory_order(order));
    }
    
    operator T() const {
        return load();
    }
    
    // Store operations
    void store(T value, MemoryOrdering order = MemoryOrdering::SeqCst) {
        atomic_.store(value, to_std_memory_order(order));
    }
    
    // Exchange operations
    T exchange(T value, MemoryOrdering order = MemoryOrdering::SeqCst) {
        return atomic_.exchange(value, to_std_memory_order(order));
    }
    
    // Compare and exchange operations
    bool compare_exchange_weak(T& expected, T desired, 
                              MemoryOrdering success_order = MemoryOrdering::SeqCst,
                              MemoryOrdering failure_order = MemoryOrdering::SeqCst) {
        return atomic_.compare_exchange_weak(expected, desired,
                                           to_std_memory_order(success_order),
                                           to_std_memory_order(failure_order));
    }
    
    bool compare_exchange_strong(T& expected, T desired,
                                MemoryOrdering success_order = MemoryOrdering::SeqCst,
                                MemoryOrdering failure_order = MemoryOrdering::SeqCst) {
        return atomic_.compare_exchange_strong(expected, desired,
                                             to_std_memory_order(success_order),
                                             to_std_memory_order(failure_order));
    }
    
    // Arithmetic operations (for numeric types)
    template<typename U = T>
    std::enable_if_t<std::is_arithmetic_v<U>, T> 
    fetch_add(T value, MemoryOrdering order = MemoryOrdering::SeqCst) {
        return atomic_.fetch_add(value, to_std_memory_order(order));
    }
    
    template<typename U = T>
    std::enable_if_t<std::is_arithmetic_v<U>, T>
    fetch_sub(T value, MemoryOrdering order = MemoryOrdering::SeqCst) {
        return atomic_.fetch_sub(value, to_std_memory_order(order));
    }
    
    template<typename U = T>
    std::enable_if_t<std::is_integral_v<U>, T>
    fetch_and(T value, MemoryOrdering order = MemoryOrdering::SeqCst) {
        return atomic_.fetch_and(value, to_std_memory_order(order));
    }
    
    template<typename U = T>
    std::enable_if_t<std::is_integral_v<U>, T>
    fetch_or(T value, MemoryOrdering order = MemoryOrdering::SeqCst) {
        return atomic_.fetch_or(value, to_std_memory_order(order));
    }
    
    template<typename U = T>
    std::enable_if_t<std::is_integral_v<U>, T>
    fetch_xor(T value, MemoryOrdering order = MemoryOrdering::SeqCst) {
        return atomic_.fetch_xor(value, to_std_memory_order(order));
    }
    
    // Increment/decrement operators
    template<typename U = T>
    std::enable_if_t<std::is_arithmetic_v<U>, T> operator++() {
        return fetch_add(1) + 1;
    }
    
    template<typename U = T>
    std::enable_if_t<std::is_arithmetic_v<U>, T> operator++(int) {
        return fetch_add(1);
    }
    
    template<typename U = T>
    std::enable_if_t<std::is_arithmetic_v<U>, T> operator--() {
        return fetch_sub(1) - 1;
    }
    
    template<typename U = T>
    std::enable_if_t<std::is_arithmetic_v<U>, T> operator--(int) {
        return fetch_sub(1);
    }
    
    // Compound assignment operators
    template<typename U = T>
    std::enable_if_t<std::is_arithmetic_v<U>, T> operator+=(T value) {
        return fetch_add(value) + value;
    }
    
    template<typename U = T>
    std::enable_if_t<std::is_arithmetic_v<U>, T> operator-=(T value) {
        return fetch_sub(value) - value;
    }
    
    template<typename U = T>
    std::enable_if_t<std::is_integral_v<U>, T> operator&=(T value) {
        return fetch_and(value) & value;
    }
    
    template<typename U = T>
    std::enable_if_t<std::is_integral_v<U>, T> operator|=(T value) {
        return fetch_or(value) | value;
    }
    
    template<typename U = T>
    std::enable_if_t<std::is_integral_v<U>, T> operator^=(T value) {
        return fetch_xor(value) ^ value;
    }
    
    // Utility methods
    bool is_lock_free() const noexcept {
        return atomic_.is_lock_free();
    }
    
private:
    std::atomic<T> atomic_;
};

// Atomic pointer specialization
template<typename T>
class Atomic<T*> {
public:
    Atomic() = default;
    explicit Atomic(T* ptr) : atomic_(ptr) {}
    
    Atomic(const Atomic& other) : atomic_(other.load()) {}
    Atomic(Atomic&& other) noexcept : atomic_(other.load()) {}
    
    Atomic& operator=(const Atomic& other) {
        store(other.load());
        return *this;
    }
    
    Atomic& operator=(Atomic&& other) noexcept {
        store(other.load());
        return *this;
    }
    
    Atomic& operator=(T* ptr) {
        store(ptr);
        return *this;
    }
    
    T* load(MemoryOrdering order = MemoryOrdering::SeqCst) const {
        return atomic_.load(to_std_memory_order(order));
    }
    
    operator T*() const {
        return load();
    }
    
    void store(T* ptr, MemoryOrdering order = MemoryOrdering::SeqCst) {
        atomic_.store(ptr, to_std_memory_order(order));
    }
    
    T* exchange(T* ptr, MemoryOrdering order = MemoryOrdering::SeqCst) {
        return atomic_.exchange(ptr, to_std_memory_order(order));
    }
    
    bool compare_exchange_weak(T*& expected, T* desired,
                              MemoryOrdering success_order = MemoryOrdering::SeqCst,
                              MemoryOrdering failure_order = MemoryOrdering::SeqCst) {
        return atomic_.compare_exchange_weak(expected, desired,
                                           to_std_memory_order(success_order),
                                           to_std_memory_order(failure_order));
    }
    
    bool compare_exchange_strong(T*& expected, T* desired,
                                MemoryOrdering success_order = MemoryOrdering::SeqCst,
                                MemoryOrdering failure_order = MemoryOrdering::SeqCst) {
        return atomic_.compare_exchange_strong(expected, desired,
                                             to_std_memory_order(success_order),
                                             to_std_memory_order(failure_order));
    }
    
    T* fetch_add(std::ptrdiff_t value, MemoryOrdering order = MemoryOrdering::SeqCst) {
        return atomic_.fetch_add(value, to_std_memory_order(order));
    }
    
    T* fetch_sub(std::ptrdiff_t value, MemoryOrdering order = MemoryOrdering::SeqCst) {
        return atomic_.fetch_sub(value, to_std_memory_order(order));
    }
    
    T* operator++() {
        return fetch_add(1) + 1;
    }
    
    T* operator++(int) {
        return fetch_add(1);
    }
    
    T* operator--() {
        return fetch_sub(1) - 1;
    }
    
    T* operator--(int) {
        return fetch_sub(1);
    }
    
    T* operator+=(std::ptrdiff_t value) {
        return fetch_add(value) + value;
    }
    
    T* operator-=(std::ptrdiff_t value) {
        return fetch_sub(value) - value;
    }
    
    bool is_lock_free() const noexcept {
        return atomic_.is_lock_free();
    }
    
private:
    std::atomic<T*> atomic_;
};

// Type aliases for common atomic types
using AtomicBool = Atomic<bool>;
using AtomicI8 = Atomic<int8_t>;
using AtomicI16 = Atomic<int16_t>;
using AtomicI32 = Atomic<int32_t>;
using AtomicI64 = Atomic<int64_t>;
using AtomicU8 = Atomic<uint8_t>;
using AtomicU16 = Atomic<uint16_t>;
using AtomicU32 = Atomic<uint32_t>;
using AtomicU64 = Atomic<uint64_t>;
using AtomicISize = Atomic<std::ptrdiff_t>;
using AtomicUSize = Atomic<std::size_t>;
using AtomicFloat = Atomic<float>;
using AtomicDouble = Atomic<double>;

// Atomic reference counting pointer
template<typename T>
class AtomicPtr {
public:
    AtomicPtr() = default;
    explicit AtomicPtr(std::shared_ptr<T> ptr) : atomic_(std::move(ptr)) {}
    
    std::shared_ptr<T> load(MemoryOrdering order = MemoryOrdering::SeqCst) const {
        return std::atomic_load_explicit(&atomic_, to_std_memory_order(order));
    }
    
    void store(std::shared_ptr<T> ptr, MemoryOrdering order = MemoryOrdering::SeqCst) {
        std::atomic_store_explicit(&atomic_, std::move(ptr), to_std_memory_order(order));
    }
    
    std::shared_ptr<T> exchange(std::shared_ptr<T> ptr, MemoryOrdering order = MemoryOrdering::SeqCst) {
        return std::atomic_exchange_explicit(&atomic_, std::move(ptr), to_std_memory_order(order));
    }
    
    bool compare_exchange_weak(std::shared_ptr<T>& expected, std::shared_ptr<T> desired,
                              MemoryOrdering success_order = MemoryOrdering::SeqCst,
                              MemoryOrdering failure_order = MemoryOrdering::SeqCst) {
        return std::atomic_compare_exchange_weak_explicit(&atomic_, &expected, std::move(desired),
                                                        to_std_memory_order(success_order),
                                                        to_std_memory_order(failure_order));
    }
    
    bool compare_exchange_strong(std::shared_ptr<T>& expected, std::shared_ptr<T> desired,
                                MemoryOrdering success_order = MemoryOrdering::SeqCst,
                                MemoryOrdering failure_order = MemoryOrdering::SeqCst) {
        return std::atomic_compare_exchange_strong_explicit(&atomic_, &expected, std::move(desired),
                                                          to_std_memory_order(success_order),
                                                          to_std_memory_order(failure_order));
    }
    
    bool is_lock_free() const noexcept {
        return std::atomic_is_lock_free(&atomic_);
    }
    
private:
    std::shared_ptr<T> atomic_;
};

// Memory fence operations
inline void fence(MemoryOrdering order) {
    std::atomic_thread_fence(to_std_memory_order(order));
}

inline void compiler_fence(MemoryOrdering order) {
    std::atomic_signal_fence(to_std_memory_order(order));
}

// Spin wait utilities
class SpinWait {
public:
    SpinWait() : count_(0) {}
    
    void spin_once() {
        if (count_ < SPIN_THRESHOLD) {
            // CPU pause/yield instruction
            #if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
                _mm_pause();
            #elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
                __builtin_ia32_pause();
            #elif defined(__aarch64__)
                asm volatile("yield" ::: "memory");
            #else
                // Generic fallback
                std::this_thread::yield();
            #endif
            count_++;
        } else {
            // After spinning for a while, yield to the OS scheduler
            std::this_thread::yield();
            count_ = 0;
        }
    }
    
    void reset() {
        count_ = 0;
    }
    
private:
    static constexpr int SPIN_THRESHOLD = 10;
    int count_;
};

// Atomic flag for simple synchronization
class AtomicFlag {
public:
    AtomicFlag() : flag_(ATOMIC_FLAG_INIT) {}
    
    // Non-copyable, non-movable
    AtomicFlag(const AtomicFlag&) = delete;
    AtomicFlag& operator=(const AtomicFlag&) = delete;
    AtomicFlag(AtomicFlag&&) = delete;
    AtomicFlag& operator=(AtomicFlag&&) = delete;
    
    bool test_and_set(MemoryOrdering order = MemoryOrdering::SeqCst) {
        return flag_.test_and_set(to_std_memory_order(order));
    }
    
    void clear(MemoryOrdering order = MemoryOrdering::SeqCst) {
        flag_.clear(to_std_memory_order(order));
    }
    
    bool test(MemoryOrdering order = MemoryOrdering::SeqCst) const {
        return flag_.test(to_std_memory_order(order));
    }
    
    void wait(bool old, MemoryOrdering order = MemoryOrdering::SeqCst) const {
        flag_.wait(old, to_std_memory_order(order));
    }
    
    void notify_one() {
        flag_.notify_one();
    }
    
    void notify_all() {
        flag_.notify_all();
    }
    
private:
    std::atomic_flag flag_;
};

// Send/Sync trait implementations for atomic types
template<typename T>
struct Send<Atomic<T>> {
    static constexpr bool value = true;
};

template<typename T>
struct Sync<Atomic<T>> {
    static constexpr bool value = true;
};

template<typename T>
struct Send<AtomicPtr<T>> {
    static constexpr bool value = Send<T>::value;
};

template<typename T>
struct Sync<AtomicPtr<T>> {
    static constexpr bool value = Send<T>::value;
};

template<>
struct Send<AtomicFlag> {
    static constexpr bool value = true;
};

template<>
struct Sync<AtomicFlag> {
    static constexpr bool value = true;
};

} // namespace meld::stdx