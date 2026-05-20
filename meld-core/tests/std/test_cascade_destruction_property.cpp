/// Property-based tests for cascade destruction with View[T] invalidation (Task 15.2).
///
/// Feature: own-link-memory-model, Property 12: Cascade destruction with View[T] invalidation
///
/// For any object graph where a parent holds Hold[T] references to children and
/// children hold View[T] references back to the parent, releasing the last
/// external Hold[T] to the parent shall:
///   (a) destroy the parent,
///   (b) cascade destruction to all children via their Hold[T] edges, and
///   (c) cause all View[T] upgrades on the destroyed objects to yield none.
///
/// Uses rapidcheck for property-based testing with Google Test integration.

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "meld/std/cascade_destruction.hpp"
#include "meld/std/mem.hpp"
#include "meld/types/memory.hpp"

#include <functional>
#include <memory>
#include <vector>

using namespace meld::std_mem;
using namespace meld::types;

// ---------------------------------------------------------------------------
// Helper: recursively build a random tree given a depth and branching spec
// ---------------------------------------------------------------------------

namespace {

/// Recursively builds a tree node with random branching.
/// depth_remaining: how many more levels to create below this node.
/// max_branch: maximum children per node (1-4).
/// next_id: monotonically increasing node id counter.
/// all_links: collects View[T] observers for every node created.
void build_random_subtree(
    const std::shared_ptr<TreeNode>& parent,
    int depth_remaining,
    int max_branch,
    int& next_id,
    std::vector<Link<TreeNode>>& all_links)
{
    if (depth_remaining <= 0) return;

    int num_children = *rc::gen::inRange(1, max_branch + 1);
    for (int i = 0; i < num_children; ++i) {
        int child_id = next_id++;
        auto child_ptr = std::make_shared<TreeNode>(child_id);
        all_links.push_back(Link<TreeNode>(child_ptr));

        build_random_subtree(child_ptr, depth_remaining - 1, max_branch, next_id, all_links);
        parent->add_child(Own<TreeNode>(child_ptr), parent);
    }
}

} // anonymous namespace

// ===========================================================================
// Property 12: Cascade destruction with View[T] invalidation
// **Validates: Requirements 2.4, 10.2, 10.3**
//
// Generate random parent-child trees with Hold[T] down and View[T] up;
// release root; verify all nodes destroyed and View[T] upgrades return none.
// ===========================================================================

TEST(CascadeDestructionPropertyTest, ReleaseRootDestroysAllNodesAndInvalidatesLinks) {
    // Feature: own-link-memory-model, Property 12: Cascade destruction with View[T] invalidation
    // **Validates: Requirements 2.4, 10.2, 10.3**
    rc::check("releasing root Hold[T] destroys all descendants and View[T] upgrades yield none",
        []() {
            // Generate random tree parameters
            int depth = *rc::gen::inRange(1, 6);        // depth 1-5
            int max_branch = *rc::gen::inRange(1, 5);   // branching 1-4

            // Build the tree
            int next_id = 1;
            auto root_ptr = std::make_shared<TreeNode>(0);

            std::vector<Link<TreeNode>> all_links;
            all_links.push_back(Link<TreeNode>(root_ptr)); // observe root

            build_random_subtree(root_ptr, depth, max_branch, next_id, all_links);

            // Precondition: all nodes are alive
            for (const auto& lnk : all_links) {
                RC_ASSERT(!lnk.expired());
                auto upgraded = lnk.upgrade();
                RC_ASSERT(upgraded.has_value());
            }

            size_t total_nodes = all_links.size();
            RC_ASSERT(total_nodes >= 2u); // at least root + 1 child

            // Release the root — cascade destruction
            root_ptr.reset();

            // Post-condition: ALL View[T] references must be expired
            for (const auto& lnk : all_links) {
                RC_ASSERT(lnk.expired());
            }

            // Post-condition: ALL View[T] upgrade() calls must return none
            for (const auto& lnk : all_links) {
                auto upgraded = lnk.upgrade();
                RC_ASSERT(!upgraded.has_value());
            }
        }
    );
}

TEST(CascadeDestructionPropertyTest, ChildParentLinksInvalidatedOnCascade) {
    // Feature: own-link-memory-model, Property 12: Cascade destruction with View[T] invalidation
    // **Validates: Requirements 2.4, 10.2, 10.3**
    rc::check("child View[T] back-references to parent yield none after cascade destruction",
        []() {
            int depth = *rc::gen::inRange(1, 6);
            int max_branch = *rc::gen::inRange(1, 5);

            int next_id = 1;
            auto root_ptr = std::make_shared<TreeNode>(0);

            std::vector<Link<TreeNode>> all_links;
            all_links.push_back(Link<TreeNode>(root_ptr));

            build_random_subtree(root_ptr, depth, max_branch, next_id, all_links);

            // Collect parent back-references from all non-root nodes
            // by traversing the tree and grabbing each child's parent_link()
            std::vector<Link<TreeNode>> parent_back_refs;
            std::function<void(const std::shared_ptr<TreeNode>&)> collect_parent_links;
            collect_parent_links = [&](const std::shared_ptr<TreeNode>& node) {
                for (size_t i = 0; i < node->child_count(); ++i) {
                    const auto& child_own = node->child(i);
                    parent_back_refs.push_back(child_own->parent_link());
                    collect_parent_links(child_own.raw());
                }
            };
            collect_parent_links(root_ptr);

            // Precondition: all parent back-refs are alive
            for (const auto& lnk : parent_back_refs) {
                RC_ASSERT(!lnk.expired());
            }

            // Release root — cascade
            root_ptr.reset();

            // Post-condition: all parent back-references yield none
            for (const auto& lnk : parent_back_refs) {
                RC_ASSERT(lnk.expired());
                RC_ASSERT(!lnk.upgrade().has_value());
            }
        }
    );
}
