#pragma once

#include "meld/parser/ast.hpp"
#include "meld/parser/ast_node.hpp"
#include "ast_abort.hpp"
#include <string>

namespace meld::macro {

/**
 * Safe parent access for field-level macros.
 *
 * Implements the Meld pattern:
 *   val parent_class = node.parent() ?: ast.abort("message")
 *
 * In C++ macro code, this translates to:
 *   auto& parent = require_parent<class_definition>(field, "message");
 *
 * If the node has no parent, ast_abort() is called with a structured
 * error message including the source location. This ensures field-level
 * macros applied outside a class produce clear compiler errors.
 *
 * Requirements: 2.11
 */

/**
 * Get the parent of an AST node, or abort with a structured error.
 *
 * This is the C++ implementation of the `node.parent() ?: ast.abort(...)`
 * pattern from the Meld macro authoring API. It combines the optional
 * parent lookup with the elvis fallback into a single ergonomic call.
 *
 * @tparam ParentT  The expected parent node type (e.g., class_definition)
 * @param node      The AST node whose parent is needed
 * @param message   Error message if parent is absent
 * @param location  Source location for structured error reporting
 * @return          Reference to the parent node, cast to ParentT
 * @throws AstAbortError if the node has no parent
 */
template<typename ParentT>
ParentT& require_parent(parser::ast::ASTNode& node,
                         const std::string& message,
                         AbortSourceLocation location = {}) {
    auto parent_opt = node.parent();
    if (!parent_opt.has_value()) {
        ast_abort(message, std::move(location));
    }
    return *static_cast<ParentT*>(parent_opt.value());
}

/**
 * Const overload for read-only parent access.
 */
template<typename ParentT>
const ParentT& require_parent(const parser::ast::ASTNode& node,
                               const std::string& message,
                               AbortSourceLocation location = {}) {
    auto parent_opt = node.parent();
    if (!parent_opt.has_value()) {
        ast_abort(message, std::move(location));
    }
    return *static_cast<const ParentT*>(
        static_cast<const parser::ast::ASTNode*>(parent_opt.value()));
}

/**
 * Non-template variant: get the raw parent ASTNode or abort.
 *
 * Useful when the caller doesn't know the parent type upfront
 * and needs to inspect it before casting.
 *
 * @param node      The AST node whose parent is needed
 * @param message   Error message if parent is absent
 * @param location  Source location for structured error reporting
 * @return          Pointer to the parent ASTNode
 * @throws AstAbortError if the node has no parent
 */
inline parser::ast::ASTNode* require_parent_node(
    parser::ast::ASTNode& node,
    const std::string& message,
    AbortSourceLocation location = {}) {
    auto parent_opt = node.parent();
    if (!parent_opt.has_value()) {
        ast_abort(message, std::move(location));
    }
    return parent_opt.value();
}

} // namespace meld::macro
