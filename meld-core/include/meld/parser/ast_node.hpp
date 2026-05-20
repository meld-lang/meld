#pragma once

#include <boost/spirit/home/x3/support/ast/position_tagged.hpp>
#include <optional>
#include <unordered_map>

namespace meld::parser {

/**
 * ASTParentMap - External parent pointer storage for AST nodes.
 *
 * Stores parent-child relationships for AST nodes WITHOUT modifying
 * the AST struct definitions' data layout. This is necessary because
 * Boost.Spirit X3 requires AST structs to work with BOOST_FUSION_ADAPT_STRUCT,
 * and adding data member fields to a base class can break Fusion adaptation.
 *
 * The map uses raw void* pointers keyed by the address of each child node.
 * This is safe because:
 * 1. AST nodes are value types owned by their parent's vectors/forward_ast
 * 2. The parent always outlives its children in the AST tree
 * 3. The map is rebuilt after parsing by set_parent_pointers()
 * 4. Raw pointers prevent ARC reference cycles (Requirement 2.8)
 *
 * Requirements: 2.7, 2.8
 */
class ASTParentMap {
public:
    static ASTParentMap& instance() {
        static ASTParentMap map;
        return map;
    }

    void set_parent(const void* child, const void* parent) {
        if (child) {
            parents_[child] = parent;
        }
    }

    const void* get_parent(const void* child) const {
        auto it = parents_.find(child);
        return (it != parents_.end()) ? it->second : nullptr;
    }

    template<typename T>
    const T* parent_as(const void* child) const {
        return static_cast<const T*>(get_parent(child));
    }

    template<typename T>
    T* mutable_parent_as(const void* child) const {
        return const_cast<T*>(static_cast<const T*>(get_parent(child)));
    }

    bool has_parent(const void* child) const {
        auto it = parents_.find(child);
        return it != parents_.end() && it->second != nullptr;
    }

    void detach(const void* child) {
        parents_.erase(child);
    }

    /**
     * Move a child from its current parent to a new parent.
     * This is the safe way to reparent a node during AST mutations
     * (e.g., macro expansion moving a field between classes).
     *
     * If new_parent is nullptr, the child is detached.
     * Requirements: 2.7
     */
    void reparent(const void* child, const void* new_parent) {
        if (!child) return;
        if (new_parent) {
            parents_[child] = new_parent;
        } else {
            parents_.erase(child);
        }
    }

    void clear() {
        parents_.clear();
    }

    /**
     * Returns the number of registered parent-child relationships.
     * Useful for testing and diagnostics.
     */
    std::size_t size() const {
        return parents_.size();
    }

private:
    ASTParentMap() = default;
    std::unordered_map<const void*, const void*> parents_;
};

} // namespace meld::parser

namespace meld::parser::ast {

namespace x3 = boost::spirit::x3;

/**
 * ASTNode - Mixin base class providing parent pointer access.
 *
 * This class adds NO data members. It provides convenience methods
 * that delegate to the ASTParentMap singleton. Because it has no
 * data members, it doesn't interfere with Boost.Fusion adaptation
 * or X3 parsing.
 *
 * All AST structs inherit: x3::position_tagged -> ASTNode -> concrete struct
 * The parent pointer methods enable bottom-up traversal needed by
 * field-level macros (e.g., @Getter navigating from field to class).
 */
struct ASTNode : x3::position_tagged {
    void set_parent(const void* parent_ptr) {
        if (parent_ptr) {
            ASTParentMap::instance().set_parent(this, parent_ptr);
        } else {
            ASTParentMap::instance().detach(this);
        }
    }

    bool has_parent() const {
        return ASTParentMap::instance().has_parent(this);
    }

    std::optional<ASTNode*> parent() const {
        auto& map = ASTParentMap::instance();
        if (!map.has_parent(this)) {
            return std::nullopt;
        }
        // The parent is stored as const void*, but we return a mutable
        // ASTNode* to match the test expectations for casting.
        return const_cast<ASTNode*>(
            static_cast<const ASTNode*>(map.get_parent(this)));
    }

    void detach() {
        ASTParentMap::instance().detach(this);
    }

    /**
     * Move this node to a new parent, updating the parent map atomically.
     * If new_parent is nullptr, the node is detached.
     * This is the safe mutation API for macro expansion.
     * Requirements: 2.7
     */
    void reparent(const void* new_parent) {
        ASTParentMap::instance().reparent(this, new_parent);
    }
};

} // namespace meld::parser::ast
