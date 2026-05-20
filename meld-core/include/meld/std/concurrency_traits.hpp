#pragma once

#include <type_traits>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>

namespace meld::stdx {

/**
 * Send marker trait - indicates types that can be safely transferred between threads.
 * 
 * A type is Send if it can be moved to another thread safely. This means:
 * - The type doesn't contain thread-local data
 * - The type doesn't contain raw pointers to non-Send data
 * - The type's destructor can be called from any thread
 */
template<typename T>
struct Send {
    static constexpr bool value = true;
};

/**
 * Sync marker trait - indicates types that can be safely shared between threads.
 * 
 * A type is Sync if it can be accessed concurrently from multiple threads safely.
 * This means:
 * - The type is immutable, or
 * - The type provides internal synchronization (like Mutex<T>)
 * - References to the type can be shared between threads
 */
template<typename T>
struct Sync {
    static constexpr bool value = true;
};

// Helper type traits for easier usage
template<typename T>
constexpr bool is_send_v = Send<T>::value;

template<typename T>
constexpr bool is_sync_v = Sync<T>::value;

// Concept-like helpers for C++17 compatibility
template<typename T>
using RequireSend = std::enable_if_t<is_send_v<T>, int>;

template<typename T>
using RequireSync = std::enable_if_t<is_sync_v<T>, int>;

template<typename T>
using RequireSendSync = std::enable_if_t<is_send_v<T> && is_sync_v<T>, int>;

// Automatic trait derivation rules

// Raw pointers are not Send or Sync by default (unsafe)
template<typename T>
struct Send<T*> {
    static constexpr bool value = false;
};

template<typename T>
struct Sync<T*> {
    static constexpr bool value = false;
};

// References follow the same rules as the referenced type
template<typename T>
struct Send<T&> {
    static constexpr bool value = Send<T>::value;
};

template<typename T>
struct Sync<T&> {
    static constexpr bool value = Sync<T>::value;
};

// Const references are Sync if the underlying type is Sync
template<typename T>
struct Send<const T&> {
    static constexpr bool value = Send<T>::value;
};

template<typename T>
struct Sync<const T&> {
    static constexpr bool value = Sync<T>::value;
};

// Smart pointers are Send/Sync based on their contents
template<typename T>
struct Send<std::unique_ptr<T>> {
    static constexpr bool value = Send<T>::value;
};

template<typename T>
struct Sync<std::unique_ptr<T>> {
    static constexpr bool value = false; // unique_ptr is not Sync (exclusive ownership)
};

template<typename T>
struct Send<std::shared_ptr<T>> {
    static constexpr bool value = Send<T>::value && Sync<T>::value;
};

template<typename T>
struct Sync<std::shared_ptr<T>> {
    static constexpr bool value = Send<T>::value && Sync<T>::value;
};

// Atomic types are Send and Sync
template<typename T>
struct Send<std::atomic<T>> {
    static constexpr bool value = true;
};

template<typename T>
struct Sync<std::atomic<T>> {
    static constexpr bool value = true;
};

// Mutex types are Send but not Sync (they provide synchronization)
template<typename T>
struct Send<std::mutex> {
    static constexpr bool value = true;
};

template<typename T>
struct Sync<std::mutex> {
    static constexpr bool value = false; // Mutex itself is not Sync, but provides synchronization
};

// Thread handles are not Send or Sync
template<>
struct Send<std::thread> {
    static constexpr bool value = false;
};

template<>
struct Sync<std::thread> {
    static constexpr bool value = false;
};

// Fundamental types are Send and Sync
template<> struct Send<bool> { static constexpr bool value = true; };
template<> struct Sync<bool> { static constexpr bool value = true; };

template<> struct Send<char> { static constexpr bool value = true; };
template<> struct Sync<char> { static constexpr bool value = true; };

template<> struct Send<int> { static constexpr bool value = true; };
template<> struct Sync<int> { static constexpr bool value = true; };

template<> struct Send<long> { static constexpr bool value = true; };
template<> struct Sync<long> { static constexpr bool value = true; };

template<> struct Send<float> { static constexpr bool value = true; };
template<> struct Sync<float> { static constexpr bool value = true; };

template<> struct Send<double> { static constexpr bool value = true; };
template<> struct Sync<double> { static constexpr bool value = true; };

// Arrays and containers follow element rules
template<typename T, size_t N>
struct Send<T[N]> {
    static constexpr bool value = Send<T>::value;
};

template<typename T, size_t N>
struct Sync<T[N]> {
    static constexpr bool value = Sync<T>::value;
};

// Compile-time thread safety checking utilities
template<typename T>
constexpr void assert_send() {
    static_assert(is_send_v<T>, "Type must be Send to transfer between threads");
}

template<typename T>
constexpr void assert_sync() {
    static_assert(is_sync_v<T>, "Type must be Sync to share between threads");
}

template<typename T>
constexpr void assert_send_sync() {
    static_assert(is_send_v<T> && is_sync_v<T>, "Type must be Send + Sync for concurrent access");
}

// Trait derivation macros for user types
#define MELD_DERIVE_SEND(Type) \
    template<> \
    struct meld::stdx::Send<Type> { \
        static constexpr bool value = true; \
    };

#define MELD_DERIVE_SYNC(Type) \
    template<> \
    struct meld::stdx::Sync<Type> { \
        static constexpr bool value = true; \
    };

#define MELD_DERIVE_SEND_SYNC(Type) \
    MELD_DERIVE_SEND(Type) \
    MELD_DERIVE_SYNC(Type)

#define MELD_NOT_SEND(Type) \
    template<> \
    struct meld::stdx::Send<Type> { \
        static constexpr bool value = false; \
    };

#define MELD_NOT_SYNC(Type) \
    template<> \
    struct meld::stdx::Sync<Type> { \
        static constexpr bool value = false; \
    };

#define MELD_NOT_SEND_SYNC(Type) \
    MELD_NOT_SEND(Type) \
    MELD_NOT_SYNC(Type)

} // namespace meld::stdx