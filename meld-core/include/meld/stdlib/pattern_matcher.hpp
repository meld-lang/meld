#pragma once

#include <functional>
#include <vector>
#include <memory>
#include <optional>
#include <type_traits>
#include <typeinfo>
#include <iostream>

namespace meld::stdlib {

// Forward declarations
template<typename T, typename R>
class PatternMatcher;

template<typename T, typename R>
class PatternBuilder;

template<typename T, typename R>
class GuardedPatternBuilder;

template<typename T, typename U, typename R>
class TypePatternBuilder;

template<typename T, typename U, typename R>
class DestructureBuilder;

// Pattern case - represents a single pattern with its handler
template<typename T, typename R>
class PatternCase {
public:
    using Predicate = std::function<bool(const T&)>;
    using Handler = std::function<R(const T&)>;
    using Guard = std::function<bool(const T&)>;
    
    PatternCase(Predicate pred, Handler handler, std::optional<Guard> guard = std::nullopt)
        : predicate_(std::move(pred))
        , handler_(std::move(handler))
        , guard_(std::move(guard)) {}
    
    bool matches(const T& value) const {
        if (!predicate_(value)) {
            return false;
        }
        if (guard_ && !(*guard_)(value)) {
            return false;
        }
        return true;
    }
    
    R execute(const T& value) const {
        return handler_(value);
    }
    
    bool has_guard() const {
        return guard_.has_value();
    }

private:
    Predicate predicate_;
    Handler handler_;
    std::optional<Guard> guard_;
};

// PatternMatcher - main class for building and executing pattern matches
template<typename T, typename R>
class PatternMatcher {
public:
    explicit PatternMatcher(const T& value) : value_(value) {}
    
    // Literal pattern matching: on(value)
    PatternBuilder<T, R> on(const T& literal) {
        return PatternBuilder<T, R>(*this, [literal](const T& v) {
            return v == literal;
        });
    }
    
    // Multiple literal patterns: on(v1, v2, v3)
    template<typename... Args>
    PatternBuilder<T, R> on(const T& first, const Args&... rest) {
        return PatternBuilder<T, R>(*this, [first, rest...](const T& v) {
            return (v == first) || ((v == rest) || ...);
        });
    }
    
    // Type pattern matching: on<Type>()
    template<typename U>
    TypePatternBuilder<T, U, R> on() {
        return TypePatternBuilder<T, U, R>(*this);
    }
    
    // Destructuring pattern: destructure<Type>()
    template<typename U>
    DestructureBuilder<T, U, R> destructure() {
        return DestructureBuilder<T, U, R>(*this);
    }
    
    // Default case: otherwise
    R otherwise(std::function<R()> handler) {
        // Try all registered cases
        for (const auto& case_item : cases_) {
            if (case_item.matches(value_)) {
                return case_item.execute(value_);
            }
        }
        
        // No match found, use default handler
        has_otherwise_ = true;
        return handler();
    }
    
    // Execute without otherwise clause - for exhaustiveness checking
    std::optional<R> execute_without_otherwise() {
        for (const auto& case_item : cases_) {
            if (case_item.matches(value_)) {
                return case_item.execute(value_);
            }
        }
        return std::nullopt;
    }
    
    // Check if pattern match is exhaustive
    bool is_exhaustive() const {
        // Simple heuristic: if we have an otherwise clause, it's exhaustive
        // More sophisticated checking would analyze the pattern space
        return has_otherwise_ || is_boolean_exhaustive() || is_enum_exhaustive();
    }
    
    // Check if boolean patterns are exhaustive
    bool is_boolean_exhaustive() const {
        if constexpr (std::is_same_v<T, bool>) {
            bool has_true = false, has_false = false;
            for (const auto& case_item : cases_) {
                if (case_item.matches(true)) has_true = true;
                if (case_item.matches(false)) has_false = true;
            }
            return has_true && has_false;
        }
        return false;
    }
    
    // Placeholder for enum exhaustiveness checking
    bool is_enum_exhaustive() const {
        // TODO: Implement enum exhaustiveness checking when enums are available
        return false;
    }
    
    // Add a pattern case
    void add_case(PatternCase<T, R> case_item) {
        cases_.push_back(std::move(case_item));
    }
    
    const T& value() const { return value_; }
    
    const std::vector<PatternCase<T, R>>& cases() const { return cases_; }

private:
    const T& value_;
    std::vector<PatternCase<T, R>> cases_;
    bool has_otherwise_ = false;
};

// PatternBuilder - fluent builder for patterns
template<typename T, typename R>
class PatternBuilder {
public:
    using Predicate = typename PatternCase<T, R>::Predicate;
    using Handler = typename PatternCase<T, R>::Handler;
    
    PatternBuilder(PatternMatcher<T, R>& matcher, Predicate pred)
        : matcher_(matcher), predicate_(std::move(pred)) {}
    
    // Direct handler: on(value) { handler }
    PatternMatcher<T, R>& then(std::function<R()> handler) {
        matcher_.add_case(PatternCase<T, R>(
            predicate_,
            [handler](const T&) { return handler(); }
        ));
        return matcher_;
    }
    
    // Handler with value: on(value) { v => handler(v) }
    PatternMatcher<T, R>& then(Handler handler) {
        matcher_.add_case(PatternCase<T, R>(
            predicate_,
            std::move(handler)
        ));
        return matcher_;
    }
    
    // Add guard: on(value).when(guard).then(handler)
    GuardedPatternBuilder<T, R> when(std::function<bool(const T&)> guard) {
        return GuardedPatternBuilder<T, R>(matcher_, predicate_, std::move(guard));
    }

private:
    PatternMatcher<T, R>& matcher_;
    Predicate predicate_;
};

// GuardedPatternBuilder - builder for patterns with guards
template<typename T, typename R>
class GuardedPatternBuilder {
public:
    using Predicate = typename PatternCase<T, R>::Predicate;
    using Handler = typename PatternCase<T, R>::Handler;
    using Guard = typename PatternCase<T, R>::Guard;
    
    GuardedPatternBuilder(PatternMatcher<T, R>& matcher, Predicate pred, Guard guard)
        : matcher_(matcher), predicate_(std::move(pred)), guard_(std::move(guard)) {}
    
    // Handler after guard
    PatternMatcher<T, R>& then(std::function<R()> handler) {
        matcher_.add_case(PatternCase<T, R>(
            predicate_,
            [handler](const T&) { return handler(); },
            guard_
        ));
        return matcher_;
    }
    
    PatternMatcher<T, R>& then(Handler handler) {
        matcher_.add_case(PatternCase<T, R>(
            predicate_,
            std::move(handler),
            guard_
        ));
        return matcher_;
    }

private:
    PatternMatcher<T, R>& matcher_;
    Predicate predicate_;
    Guard guard_;
};

// TypePatternBuilder - builder for type-based patterns
template<typename T, typename U, typename R>
class TypePatternBuilder {
public:
    TypePatternBuilder(PatternMatcher<T, R>& matcher) : matcher_(matcher) {}
    
    // Type pattern with handler: on<Int> { n => handler(n) }
    PatternMatcher<T, R>& then(std::function<R(const U&)> handler) {
        matcher_.add_case(PatternCase<T, R>(
            [](const T& v) {
                // Type check - handle both polymorphic and primitive types
                if constexpr (std::is_same_v<T, U>) {
                    return true; // Same type, always matches
                } else if constexpr (std::is_polymorphic_v<T> && std::is_polymorphic_v<U>) {
                    return dynamic_cast<const U*>(&v) != nullptr;
                } else {
                    return false; // Different primitive types don't match
                }
            },
            [handler](const T& v) {
                if constexpr (std::is_same_v<T, U>) {
                    return handler(v);
                } else if constexpr (std::is_polymorphic_v<T> && std::is_polymorphic_v<U>) {
                    const U& typed_value = dynamic_cast<const U&>(v);
                    return handler(typed_value);
                } else {
                    // This should never be reached due to predicate check
                    throw std::runtime_error("Type mismatch in pattern handler");
                }
            }
        ));
        return matcher_;
    }
    
    // Type pattern with guard: on<Int>.when(guard).then(handler)
    GuardedPatternBuilder<T, R> when(std::function<bool(const U&)> guard) {
        auto predicate = [](const T& v) {
            if constexpr (std::is_same_v<T, U>) {
                return true;
            } else if constexpr (std::is_polymorphic_v<T> && std::is_polymorphic_v<U>) {
                return dynamic_cast<const U*>(&v) != nullptr;
            } else {
                return false;
            }
        };
        
        auto typed_guard = [guard](const T& v) {
            if constexpr (std::is_same_v<T, U>) {
                return guard(v);
            } else if constexpr (std::is_polymorphic_v<T> && std::is_polymorphic_v<U>) {
                const U& typed_value = dynamic_cast<const U&>(v);
                return guard(typed_value);
            } else {
                return false;
            }
        };
        
        return GuardedPatternBuilder<T, R>(matcher_, predicate, typed_guard);
    }

private:
    PatternMatcher<T, R>& matcher_;
};

// DestructureBuilder - builder for destructuring patterns
template<typename T, typename U, typename R>
class DestructureBuilder {
public:
    DestructureBuilder(PatternMatcher<T, R>& matcher) : matcher_(matcher) {}
    
    // Nested pattern matching within destructuring
    template<typename Handler>
    PatternMatcher<T, R>& with(Handler handler) {
        // Handler receives the destructured object and can do nested matching
        matcher_.add_case(PatternCase<T, R>(
            [](const T& v) {
                if constexpr (std::is_same_v<T, U>) {
                    return true;
                } else if constexpr (std::is_polymorphic_v<T> && std::is_polymorphic_v<U>) {
                    return dynamic_cast<const U*>(&v) != nullptr;
                } else {
                    return false;
                }
            },
            [handler](const T& v) {
                if constexpr (std::is_same_v<T, U>) {
                    return handler(v);
                } else if constexpr (std::is_polymorphic_v<T> && std::is_polymorphic_v<U>) {
                    const U& typed_value = dynamic_cast<const U&>(v);
                    return handler(typed_value);
                } else {
                    throw std::runtime_error("Type mismatch in destructure handler");
                }
            }
        ));
        return matcher_;
    }

private:
    PatternMatcher<T, R>& matcher_;
};

// Universal pattern matching function that works with any type
template<typename T, typename R, typename Builder>
R match(const T& value, Builder builder) {
    PatternMatcher<T, R> matcher(value);
    return builder(matcher);
}

// Convenience wrapper for type deduction
template<typename T>
struct MatchProxy {
    const T& value;
    
    explicit MatchProxy(const T& v) : value(v) {}
    
    template<typename Builder>
    auto operator()(Builder builder) const -> decltype(builder(std::declval<PatternMatcher<T, int>&>())) {
        using R = decltype(builder(std::declval<PatternMatcher<T, int>&>()));
        PatternMatcher<T, R> matcher(value);
        return builder(matcher);
    }
};

// Free function to create match proxy
template<typename T>
MatchProxy<T> make_match(const T& value) {
    return MatchProxy<T>(value);
}

// Extension method helper - to be used with template specialization
template<typename T>
class Matchable {
public:
    template<typename R, typename Builder>
    R match(Builder builder) {
        PatternMatcher<T, R> matcher(static_cast<const T&>(*this));
        return builder(matcher);
    }
};

// Exhaustiveness checking utilities
namespace exhaustiveness {
    
    // Compile-time warning helper
    template<typename T>
    struct ExhaustivenessChecker {
        static void check_exhaustiveness(const PatternMatcher<T, void>& matcher) {
            if (!matcher.is_exhaustive()) {
                // In a real implementation, this would generate a compile-time warning
                // For now, we'll use a runtime warning
                #ifdef DEBUG
                std::cerr << "Warning: Pattern match may not be exhaustive for type " 
                         << typeid(T).name() << std::endl;
                #endif
            }
        }
    };
    
    // Helper function to suggest missing patterns
    template<typename T>
    std::vector<std::string> suggest_missing_patterns(const PatternMatcher<T, void>& matcher) {
        std::vector<std::string> suggestions;
        
        if constexpr (std::is_same_v<T, bool>) {
            bool has_true = false, has_false = false;
            for (const auto& case_item : matcher.cases()) {
                if (case_item.matches(true)) has_true = true;
                if (case_item.matches(false)) has_false = true;
            }
            if (!has_true) suggestions.push_back("on(true) { ... }");
            if (!has_false) suggestions.push_back("on(false) { ... }");
        }
        
        if (suggestions.empty() && !matcher.is_exhaustive()) {
            suggestions.push_back("otherwise { ... }");
        }
        
        return suggestions;
    }
}

// Macro for exhaustiveness checking (to be used in debug builds)
#define CHECK_PATTERN_EXHAUSTIVENESS(matcher) \
    do { \
        if constexpr (std::is_same_v<decltype(matcher), PatternMatcher<bool, void>>) { \
            ::meld::stdlib::exhaustiveness::ExhaustivenessChecker<bool>::check_exhaustiveness(matcher); \
        } \
    } while(0)

} // namespace meld::stdlib
