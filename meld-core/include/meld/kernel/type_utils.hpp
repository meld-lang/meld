#pragma once

#include <typeinfo>
#include <string>
#include <type_traits>

namespace meld::kernel {

/**
 * TypeComparator - Utility class for comparing and working with C++ type_info
 * 
 * This class provides utilities for RTTI-based type operations including
 * type comparison, ordering, and hashing.
 */
class TypeComparator {
public:
    /**
     * Check if two types are the same
     * 
     * @param t1 First type_info
     * @param t2 Second type_info
     * @return true if types are identical
     */
    static bool are_same(const std::type_info& t1, const std::type_info& t2) {
        return t1 == t2;
    }
    
    /**
     * Check if Derived is derived from Base at compile time
     * 
     * @tparam Base Base class type
     * @tparam Derived Potentially derived class type
     * @return true if Derived inherits from Base
     */
    template<typename Base, typename Derived>
    static constexpr bool is_derived_from() {
        return std::is_base_of_v<Base, Derived>;
    }
    
    /**
     * Get type ordering for sorted containers
     * 
     * @param t1 First type_info
     * @param t2 Second type_info
     * @return true if t1 comes before t2 in implementation-defined order
     */
    static bool before(const std::type_info& t1, const std::type_info& t2) {
        return t1.before(t2);
    }
    
    /**
     * Get hash code for a type
     * 
     * @param t type_info to hash
     * @return Hash code for the type
     */
    static size_t hash(const std::type_info& t) {
        return t.hash_code();
    }
    
    /**
     * Get human-readable type name
     * 
     * Note: The name returned by type_info::name() is implementation-defined
     * and may be mangled. This function returns it as-is.
     * 
     * @param t type_info to get name from
     * @return Type name string (may be mangled)
     */
    static std::string get_name(const std::type_info& t) {
        return t.name();
    }
    
    /**
     * Compare two type_info objects for equality
     * 
     * @param t1 First type_info
     * @param t2 Second type_info
     * @return true if types are equal
     */
    static bool equals(const std::type_info& t1, const std::type_info& t2) {
        return t1 == t2;
    }
    
    /**
     * Compare two type_info objects for inequality
     * 
     * @param t1 First type_info
     * @param t2 Second type_info
     * @return true if types are not equal
     */
    static bool not_equals(const std::type_info& t1, const std::type_info& t2) {
        return t1 != t2;
    }
};

} // namespace meld::kernel
