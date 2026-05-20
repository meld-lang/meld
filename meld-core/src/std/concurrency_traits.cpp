#include "meld/std/concurrency_traits.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <map>

namespace meld::stdx {

// Explicit instantiations for common standard library types

// String types are Send and Sync
template<> struct Send<std::string> { static constexpr bool value = true; };
template<> struct Sync<std::string> { static constexpr bool value = true; };

// Container types are Send/Sync if their elements are Send/Sync
template<typename T, typename Alloc>
struct Send<std::vector<T, Alloc>> {
    static constexpr bool value = Send<T>::value;
};

template<typename T, typename Alloc>
struct Sync<std::vector<T, Alloc>> {
    static constexpr bool value = Sync<T>::value;
};

template<typename K, typename V, typename Compare, typename Alloc>
struct Send<std::map<K, V, Compare, Alloc>> {
    static constexpr bool value = Send<K>::value && Send<V>::value;
};

template<typename K, typename V, typename Compare, typename Alloc>
struct Sync<std::map<K, V, Compare, Alloc>> {
    static constexpr bool value = Sync<K>::value && Sync<V>::value;
};

// Utility functions for runtime thread safety checking
namespace detail {

thread_local bool in_send_check = false;
thread_local bool in_sync_check = false;

void log_send_violation(const std::string& type_name, const std::string& reason) {
    if (!in_send_check) {
        in_send_check = true;
        std::cerr << "Send violation for type " << type_name << ": " << reason << std::endl;
        in_send_check = false;
    }
}

void log_sync_violation(const std::string& type_name, const std::string& reason) {
    if (!in_sync_check) {
        in_sync_check = true;
        std::cerr << "Sync violation for type " << type_name << ": " << reason << std::endl;
        in_sync_check = false;
    }
}

} // namespace detail

// Runtime checking functions for debugging
template<typename T>
bool runtime_check_send(const T& value, const std::string& context = "") {
    if constexpr (!is_send_v<T>) {
        detail::log_send_violation(typeid(T).name(), 
            "Type is not Send" + (context.empty() ? "" : " in context: " + context));
        return false;
    }
    return true;
}

template<typename T>
bool runtime_check_sync(const T& value, const std::string& context = "") {
    if constexpr (!is_sync_v<T>) {
        detail::log_sync_violation(typeid(T).name(), 
            "Type is not Sync" + (context.empty() ? "" : " in context: " + context));
        return false;
    }
    return true;
}

// Explicit instantiations for common types to ensure they're available
template bool runtime_check_send<int>(const int&, const std::string&);
template bool runtime_check_send<std::string>(const std::string&, const std::string&);
template bool runtime_check_sync<int>(const int&, const std::string&);
template bool runtime_check_sync<std::string>(const std::string&, const std::string&);

} // namespace meld::stdx