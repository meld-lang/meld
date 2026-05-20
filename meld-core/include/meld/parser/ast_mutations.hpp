#pragma once

#include "ast.hpp"
#include "ast_node.hpp"
#include "ast_parent_setter.hpp"

namespace meld::parser::ast {

/**
 * AST Mutation Utilities
 *
 * Provides safe mutation operations on AST nodes that automatically
 * maintain parent pointer invariants. These are the building blocks
 * for macro expansion — when a macro generates new nodes and injects
 * them into the tree, these functions ensure parent pointers stay
 * consistent.
 *
 * Key invariant: After any mutation, every child node's .parent()
 * returns its actual enclosing parent in the tree.
 *
 * Requirements: 2.7, 25B.13
 */

// ─── Field mutations ────────────────────────────────────────────

/**
 * Add a field to a class_definition, wiring its parent pointer.
 */
inline void add_field(class_definition& cls, field_declaration field) {
    cls.fields.push_back(std::move(field));
    auto& added = cls.fields.back();
    detail::wire_child(&added, &cls);
    detail::wire_child(&added.name, &added);
}

/**
 * Add a field to a struct_definition, wiring its parent pointer.
 */
inline void add_field(struct_definition& s, field_declaration field) {
    s.fields.push_back(std::move(field));
    auto& added = s.fields.back();
    detail::wire_child(&added, &s);
    detail::wire_child(&added.name, &added);
}

// ─── Method mutations ───────────────────────────────────────────

/**
 * Add a method to a class_definition, wiring its parent pointer.
 * This is the core API for field-level macros like @Getter that
 * generate methods and inject them into the enclosing class via
 * parent_class.add_method().
 *
 * Requirements: 25B.13
 */
inline void add_method(class_definition& cls, function_definition method) {
    cls.methods.push_back(std::move(method));
    auto& added = cls.methods.back();
    detail::wire_child(&added, &cls);
    detail::wire_child(&added.name, &added);
    // Wire parameters
    for (auto& p : added.parameters) {
        detail::wire_child(&p, &added);
        detail::wire_child(&p.name, &p);
    }
}

/**
 * Add a method to a struct_definition, wiring its parent pointer.
 */
inline void add_method(struct_definition& s, function_definition method) {
    s.methods.push_back(std::move(method));
    auto& added = s.methods.back();
    detail::wire_child(&added, &s);
    detail::wire_child(&added.name, &added);
    for (auto& p : added.parameters) {
        detail::wire_child(&p, &added);
        detail::wire_child(&p.name, &p);
    }
}

// ─── Field removal / reparenting ────────────────────────────────

/**
 * Remove a field from a class by index, detaching its parent pointer.
 * Returns the removed field (detached from the tree).
 */
inline field_declaration remove_field(class_definition& cls, std::size_t index) {
    auto it = cls.fields.begin() + static_cast<std::ptrdiff_t>(index);
    it->detach();
    it->name.detach();
    field_declaration removed = std::move(*it);
    cls.fields.erase(it);
    return removed;
}

/**
 * Remove a field from a struct by index, detaching its parent pointer.
 * Returns the removed field (detached from the tree).
 */
inline field_declaration remove_field(struct_definition& s, std::size_t index) {
    auto it = s.fields.begin() + static_cast<std::ptrdiff_t>(index);
    it->detach();
    it->name.detach();
    field_declaration removed = std::move(*it);
    s.fields.erase(it);
    return removed;
}

// ─── Bulk re-wiring after structural changes ────────────────────

/**
 * Re-wire all parent pointers for children of a class_definition.
 * Call this after any bulk mutation (e.g., reordering fields,
 * splicing in macro-generated nodes) to restore the invariant.
 *
 * Requirements: 2.7
 */
inline void rewire_children(class_definition& cls) {
    detail::wire_child(&cls.name, &cls);
    for (auto& f : cls.fields) {
        detail::wire_child(&f, &cls);
        detail::wire_child(&f.name, &f);
    }
    for (auto& m : cls.methods) {
        detail::wire_child(&m, &cls);
        detail::wire_child(&m.name, &m);
        for (auto& p : m.parameters) {
            detail::wire_child(&p, &m);
            detail::wire_child(&p.name, &p);
        }
    }
}

/**
 * Re-wire all parent pointers for children of a struct_definition.
 */
inline void rewire_children(struct_definition& s) {
    detail::wire_child(&s.name, &s);
    for (auto& f : s.fields) {
        detail::wire_child(&f, &s);
        detail::wire_child(&f.name, &f);
    }
    for (auto& m : s.methods) {
        detail::wire_child(&m, &s);
        detail::wire_child(&m.name, &m);
        for (auto& p : m.parameters) {
            detail::wire_child(&p, &m);
            detail::wire_child(&p.name, &p);
        }
    }
}

} // namespace meld::parser::ast
