#pragma once
// MSVC compatibility: boost::spirit::x3::variant wraps boost::variant,
// NOT std::variant.  std::visit does not work with it.  This header
// provides meld::compat::visit() which delegates to boost::apply_visitor
// so every call-site compiles identically on GCC, Clang, and MSVC.
//
// Generic lambdas with if-constexpr need a result_type wrapper for
// boost::apply_visitor to deduce the return type on MSVC.

#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/static_visitor.hpp>
#include <utility>
#include <type_traits>

namespace meld::compat {

// Wraps a callable (lambda / functor) so that boost::apply_visitor can
// deduce the return type via the result_type typedef.
template <typename R, typename F>
struct visitor_wrapper : boost::static_visitor<R> {
    F func;
    explicit visitor_wrapper(F&& f) : func(std::forward<F>(f)) {}
    explicit visitor_wrapper(const F& f) : func(f) {}

    template <typename T>
    R operator()(T&& arg) const { return func(std::forward<T>(arg)); }

    template <typename T>
    R operator()(T&& arg) { return func(std::forward<T>(arg)); }
};

// visit<R>(visitor, variant) — use when the visitor returns R
template <typename R, typename Visitor, typename Variant>
R visit(Visitor&& vis, Variant&& var) {
    visitor_wrapper<R, std::decay_t<Visitor>> w{std::forward<Visitor>(vis)};
    return boost::apply_visitor(w, std::forward<Variant>(var));
}

// visit(visitor, variant) — void overload (most common for side-effect visitors)
template <typename Visitor, typename Variant>
void visit(Visitor&& vis, Variant&& var) {
    visitor_wrapper<void, std::decay_t<Visitor>> w{std::forward<Visitor>(vis)};
    boost::apply_visitor(w, std::forward<Variant>(var));
}

} // namespace meld::compat
