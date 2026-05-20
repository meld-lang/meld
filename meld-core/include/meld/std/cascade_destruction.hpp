#pragma once

/// @file cascade_destruction.hpp
/// @brief Cascade destruction pattern for Hold[T] / View[T] tree structures.
///
/// Demonstrates and documents the canonical cycle resolution pattern in Meld:
///   - Parent holds Hold[T] (shared_ptr) to children → children stay alive
///   - Children hold View[T] (weak_ptr) to parent → doesn't prevent parent destruction
///   - When parent's last Hold[T] is released → parent destroyed → its Hold[T] children
///     released → children destroyed (cascade)
///   - View[T] references to destroyed objects return nullopt on upgrade()
///
/// Requirements: 10.1, 10.2, 10.3

#include "meld/std/mem.hpp"
#include "meld/types/memory.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

namespace meld::std_mem {

// ===========================================================================
// TreeNode — Example class demonstrating the cascade destruction pattern
// ===========================================================================

/// A tree node that holds Hold[T] references to children (strong, parent-to-child)
/// and a View[T] back-reference to its parent (weak, child-to-parent).
///
/// This is the canonical pattern for resolving reference cycles in Meld:
///   - Hold[T] for the strong direction (parent → child)
///   - View[T] for the back-reference direction (child → parent)
///
/// When the root's last external Hold[T] is released:
///   1. Root is destroyed
///   2. Root's Hold[T] children are released → children destroyed
///   3. Children's Hold[T] grandchildren released → grandchildren destroyed
///   4. All View[T] back-references to any destroyed node yield none on upgrade
///
/// Requirement 10.1: Hold[T] for parent-to-child, View[T] for child-to-parent
/// Requirement 10.2: Releasing last Hold[T] to parent cascades destruction
/// Requirement 10.3: View[T] to destroyed parent yields none on upgrade
class TreeNode : public types::ManagedObject {
public:
    explicit TreeNode(int id) : id_(id) {}

    int id() const { return id_; }

    /// Add a child node. The parent takes ownership via Hold[T] (strong ref).
    /// The child gets a View[T] back-reference to this parent (weak ref).
    void add_child(Own<TreeNode> child, const std::shared_ptr<TreeNode>& self_ptr) {
        // Set the child's parent back-reference as View[T] (weak)
        child->parent_link_ = Link<TreeNode>(self_ptr);
        children_.push_back(std::move(child));
    }

    /// Access the parent back-reference. Returns nullopt if parent is destroyed.
    std::optional<std::shared_ptr<TreeNode>> upgrade_parent() const {
        return parent_link_.upgrade();
    }

    /// Check if the parent back-reference is expired (parent destroyed).
    bool parent_expired() const {
        return parent_link_.expired();
    }

    /// Get the View[T] to the parent (for external observation).
    const Link<TreeNode>& parent_link() const { return parent_link_; }

    /// Number of direct children.
    size_t child_count() const { return children_.size(); }

    /// Access a child's Hold[T] by index.
    const Own<TreeNode>& child(size_t index) const { return children_[index]; }

private:
    int id_;
    Link<TreeNode> parent_link_;         // weak back-reference (child → parent)
    std::vector<Own<TreeNode>> children_; // strong ownership (parent → child)
};

// ===========================================================================
// Helper: build a tree and collect View[T] observers for verification
// ===========================================================================

/// Collects View[T] references to every node in a tree rooted at `root`.
/// Useful for verifying that all nodes are destroyed after root release.
inline void collect_links(
    const std::shared_ptr<TreeNode>& node,
    std::vector<Link<TreeNode>>& out)
{
    out.push_back(Link<TreeNode>(node));
    for (size_t i = 0; i < node->child_count(); ++i) {
        collect_links(node->child(i).raw(), out);
    }
}

/// Returns true if all View[T] references in the vector are expired
/// (i.e., all observed objects have been destroyed).
inline bool all_expired(const std::vector<Link<TreeNode>>& links) {
    for (const auto& lnk : links) {
        if (!lnk.expired()) return false;
    }
    return true;
}

/// Returns true if all View[T] upgrade() calls return nullopt.
inline bool all_upgrades_none(const std::vector<Link<TreeNode>>& links) {
    for (const auto& lnk : links) {
        if (lnk.upgrade().has_value()) return false;
    }
    return true;
}

} // namespace meld::std_mem
