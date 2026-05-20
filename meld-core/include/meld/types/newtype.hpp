#pragma once

#include <string>
#include <memory>
#include <optional>
#include <functional>
#include <type_traits>

namespace meld {
namespace types {

// Newtype wrapper for zero-cost type safety
template<typename T, typename Tag>
class Newtype {
public:
    using ValueType = T;
    using TagType = Tag;
    
    // Constructors
    explicit Newtype(const T& value) : value_(value) {}
    explicit Newtype(T&& value) : value_(std::move(value)) {}
    
    // Access inner value
    const T& get() const { return value_; }
    T& get_mut() { return value_; }
    
    // Unwrap (consume and return inner value)
    T unwrap() && { return std::move(value_); }
    
    // Comparison operators (only if inner type supports them)
    template<typename U = T>
    typename std::enable_if<std::is_same<U, T>::value, bool>::type
    operator==(const Newtype& other) const {
        return value_ == other.value_;
    }
    
    template<typename U = T>
    typename std::enable_if<std::is_same<U, T>::value, bool>::type
    operator!=(const Newtype& other) const {
        return value_ != other.value_;
    }
    
    template<typename U = T>
    typename std::enable_if<std::is_same<U, T>::value, bool>::type
    operator<(const Newtype& other) const {
        return value_ < other.value_;
    }
    
    // Map operation
    template<typename F>
    auto map(F&& f) const -> Newtype<decltype(f(value_)), Tag> {
        return Newtype<decltype(f(value_)), Tag>(f(value_));
    }
    
private:
    T value_;
};

// Helper macro for defining newtypes
#define DEFINE_NEWTYPE(Name, InnerType) \
    struct Name##Tag {}; \
    using Name = meld::types::Newtype<InnerType, Name##Tag>;

// Common newtype patterns
namespace newtypes {

// ID types for type-safe identifiers
template<typename Entity>
struct IdTag {};

template<typename Entity>
using Id = Newtype<int, IdTag<Entity>>;

// Quantity types with units
struct MetersTag {};
struct SecondsTag {};
struct KilogramsTag {};

using Meters = Newtype<double, MetersTag>;
using Seconds = Newtype<double, SecondsTag>;
using Kilograms = Newtype<double, KilogramsTag>;

// String newtypes for semantic clarity
struct EmailTag {};
struct UrlTag {};
struct PathTag {};

using Email = Newtype<std::string, EmailTag>;
using Url = Newtype<std::string, UrlTag>;
using Path = Newtype<std::string, PathTag>;

} // namespace newtypes

// Newtype compiler integration
class NewtypeCompiler {
public:
    struct NewtypeInfo {
        std::string name;
        std::string inner_type;
        std::string tag_type;
        bool is_zero_cost;
        size_t size_bytes;
    };
    
    // Analyze newtype definition
    static NewtypeInfo analyze_newtype(
        const std::string& name,
        const std::string& inner_type
    );
    
    // Verify zero-cost property
    static bool verify_zero_cost(const NewtypeInfo& info);
    
    // Generate optimized code
    static std::string generate_optimized_code(const NewtypeInfo& info);
    
    // Type checking for newtype operations
    static bool check_type_safety(
        const std::string& operation,
        const NewtypeInfo& lhs,
        const NewtypeInfo& rhs
    );
};

// Newtype optimization analyzer
class NewtypeOptimizer {
public:
    struct OptimizationResult {
        bool can_inline;
        bool can_elide_wrapper;
        bool requires_runtime_check;
        std::string optimization_strategy;
    };
    
    // Analyze optimization opportunities
    static OptimizationResult analyze(const NewtypeCompiler::NewtypeInfo& info);
    
    // Apply optimizations
    static std::string apply_optimizations(
        const std::string& code,
        const OptimizationResult& result
    );
    
    // Verify optimization preserves semantics
    static bool verify_optimization(
        const std::string& original,
        const std::string& optimized
    );
};

// Newtype conversion utilities
template<typename T, typename Tag>
class NewtypeConversions {
public:
    // Safe conversion from inner type
    static Newtype<T, Tag> from(const T& value) {
        return Newtype<T, Tag>(value);
    }
    
    // Safe conversion to inner type
    static T into(Newtype<T, Tag>&& newtype) {
        return std::move(newtype).unwrap();
    }
    
    // Try conversion with validation
    using Validator = std::function<bool(const T&)>;
    
    static std::optional<Newtype<T, Tag>> try_from(
        const T& value,
        Validator validator
    ) {
        if (validator(value)) {
            return Newtype<T, Tag>(value);
        }
        return std::nullopt;
    }
};

// Newtype traits for compile-time properties
template<typename T>
struct is_newtype : std::false_type {};

template<typename T, typename Tag>
struct is_newtype<Newtype<T, Tag>> : std::true_type {};

template<typename T>
constexpr bool is_newtype_v = is_newtype<T>::value;

// Extract inner type from newtype
template<typename T>
struct newtype_inner_type;

template<typename T, typename Tag>
struct newtype_inner_type<Newtype<T, Tag>> {
    using type = T;
};

template<typename T>
using newtype_inner_type_t = typename newtype_inner_type<T>::type;

// Newtype validation
template<typename T, typename Tag>
class NewtypeValidator {
public:
    using ValidatorFn = std::function<bool(const T&)>;
    
    // Add validation rule
    static void add_validator(const std::string& name, ValidatorFn validator);
    
    // Validate value
    static bool validate(const T& value);
    
    // Get validation errors
    static std::vector<std::string> get_errors(const T& value);
    
private:
    static std::vector<std::pair<std::string, ValidatorFn>> validators_;
};

// Newtype derivation for common traits
template<typename T, typename Tag>
class NewtypeDerive {
public:
    // Derive Debug trait
    static std::string debug(const Newtype<T, Tag>& nt) {
        return "Newtype(" + std::to_string(nt.get()) + ")";
    }
    
    // Derive Clone trait
    static Newtype<T, Tag> clone(const Newtype<T, Tag>& nt) {
        return Newtype<T, Tag>(nt.get());
    }
    
    // Derive Hash trait
    static size_t hash(const Newtype<T, Tag>& nt) {
        return std::hash<T>{}(nt.get());
    }
};

} // namespace types
} // namespace meld
