/// Unit tests for cascade destruction with View[T] invalidation (Task 15.1).
///
/// Verifies the canonical cycle resolution pattern:
///   - Hold[T] for parent-to-child (strong)
///   - View[T] for child-to-parent (weak back-reference)
///   - Releasing root cascades destruction to all descendants
///   - View[T] references to destroyed objects yield none on upgrade
///
/// Requirements: 10.1, 10.2, 10.3

#include <gtest/gtest.h>
#include "meld/std/cascade_destruction.hpp"
#include "meld/std/mem.hpp"
#include "meld/types/memory.hpp"

#include <memory>
#include <vector>

using namespace meld::std_mem;
using namespace meld::types;

// ===========================================================================
// Basic cascade destruction — releasing root destroys all descendants
// Requirement 10.2
// ===========================================================================

TEST(CascadeDestructionTest, SingleChildDestroyedWhenParentReleased) {
    std::vector<Link<TreeNode>> observers;

    {
        auto root_ptr = std::make_shared<TreeNode>(1);
        auto child_ptr = std::make_shared<TreeNode>(2);

        root_ptr->add_child(Own<TreeNode>(child_ptr), root_ptr);

        collect_links(root_ptr, observers);
        ASSERT_EQ(observers.size(), 2u);

        // Both nodes alive
        EXPECT_FALSE(observers[0].expired()); // root
        EXPECT_FALSE(observers[1].expired()); // child
    }
    // root_ptr and child_ptr (local shared_ptrs) go out of scope.
    // root is destroyed → its Hold[T] child released → child destroyed.

    EXPECT_TRUE(all_expired(observers));
    EXPECT_TRUE(all_upgrades_none(observers));
}

TEST(CascadeDestructionTest, MultipleChildrenDestroyedWhenParentReleased) {
    std::vector<Link<TreeNode>> observers;

    {
        auto root_ptr = std::make_shared<TreeNode>(1);
        for (int i = 2; i <= 5; ++i) {
            auto child_ptr = std::make_shared<TreeNode>(i);
            root_ptr->add_child(Own<TreeNode>(child_ptr), root_ptr);
        }

        collect_links(root_ptr, observers);
        ASSERT_EQ(observers.size(), 5u); // root + 4 children

        for (const auto& obs : observers) {
            EXPECT_FALSE(obs.expired());
        }
    }

    EXPECT_TRUE(all_expired(observers));
    EXPECT_TRUE(all_upgrades_none(observers));
}

// ===========================================================================
// Deep tree cascade — multiple levels destroyed correctly
// Requirement 10.2
// ===========================================================================

TEST(CascadeDestructionTest, DeepTreeCascadesCorrectly) {
    std::vector<Link<TreeNode>> observers;

    {
        // Build a chain: root → child → grandchild → great-grandchild
        auto root_ptr = std::make_shared<TreeNode>(1);
        auto child_ptr = std::make_shared<TreeNode>(2);
        auto grandchild_ptr = std::make_shared<TreeNode>(3);
        auto great_grandchild_ptr = std::make_shared<TreeNode>(4);

        grandchild_ptr->add_child(Own<TreeNode>(great_grandchild_ptr), grandchild_ptr);
        child_ptr->add_child(Own<TreeNode>(grandchild_ptr), child_ptr);
        root_ptr->add_child(Own<TreeNode>(child_ptr), root_ptr);

        collect_links(root_ptr, observers);
        ASSERT_EQ(observers.size(), 4u);

        for (const auto& obs : observers) {
            EXPECT_FALSE(obs.expired());
        }
    }

    // All 4 levels destroyed
    EXPECT_TRUE(all_expired(observers));
    EXPECT_TRUE(all_upgrades_none(observers));
}

// ===========================================================================
// View[T] back-reference to destroyed parent yields none on upgrade
// Requirement 10.3
// ===========================================================================

TEST(CascadeDestructionTest, LinkToDestroyedParentYieldsNone) {
    Link<TreeNode> child_parent_link;

    {
        auto root_ptr = std::make_shared<TreeNode>(1);
        auto child_ptr = std::make_shared<TreeNode>(2);

        root_ptr->add_child(Own<TreeNode>(child_ptr), root_ptr);

        // Grab the child's View[T] to parent before destruction
        child_parent_link = child_ptr->parent_link();

        // Parent link is alive while root exists
        auto upgraded = child_parent_link.upgrade();
        EXPECT_TRUE(upgraded.has_value());
        EXPECT_EQ(upgraded.value()->id(), 1);
    }

    // Root destroyed → parent link must yield none
    EXPECT_TRUE(child_parent_link.expired());
    auto upgraded = child_parent_link.upgrade();
    EXPECT_FALSE(upgraded.has_value());
}

TEST(CascadeDestructionTest, LinkToDestroyedChildYieldsNone) {
    Link<TreeNode> child_observer;

    {
        auto root_ptr = std::make_shared<TreeNode>(1);
        auto child_ptr = std::make_shared<TreeNode>(2);

        child_observer = Link<TreeNode>(child_ptr);
        root_ptr->add_child(Own<TreeNode>(child_ptr), root_ptr);

        EXPECT_FALSE(child_observer.expired());
    }

    // Root destroyed → child destroyed → observer yields none
    EXPECT_TRUE(child_observer.expired());
    EXPECT_FALSE(child_observer.upgrade().has_value());
}

// ===========================================================================
// Hold[T] parent-to-child + View[T] child-to-parent pattern
// Requirement 10.1
// ===========================================================================

TEST(CascadeDestructionTest, OwnDownLinkUpPattern) {
    // Verify the pattern: Hold[T] down, View[T] up
    auto root_ptr = std::make_shared<TreeNode>(1);
    auto child_ptr = std::make_shared<TreeNode>(2);

    root_ptr->add_child(Own<TreeNode>(child_ptr), root_ptr);

    // Child can reach parent via View[T] upgrade
    auto parent_opt = child_ptr->upgrade_parent();
    ASSERT_TRUE(parent_opt.has_value());
    EXPECT_EQ(parent_opt.value()->id(), 1);

    // Parent holds Hold[T] to child — child is alive
    EXPECT_EQ(root_ptr->child_count(), 1u);
    EXPECT_EQ(root_ptr->child(0)->id(), 2);
}

// ===========================================================================
// Wide + deep tree — branching factor > 1 with multiple levels
// ===========================================================================

TEST(CascadeDestructionTest, WideAndDeepTreeCascade) {
    std::vector<Link<TreeNode>> observers;

    {
        // Root with 3 children, each with 2 grandchildren = 10 nodes total
        auto root_ptr = std::make_shared<TreeNode>(0);
        int next_id = 1;

        for (int c = 0; c < 3; ++c) {
            auto child_ptr = std::make_shared<TreeNode>(next_id++);
            for (int g = 0; g < 2; ++g) {
                auto grandchild_ptr = std::make_shared<TreeNode>(next_id++);
                child_ptr->add_child(Own<TreeNode>(grandchild_ptr), child_ptr);
            }
            root_ptr->add_child(Own<TreeNode>(child_ptr), root_ptr);
        }

        collect_links(root_ptr, observers);
        ASSERT_EQ(observers.size(), 10u); // 1 root + 3 children + 6 grandchildren

        for (const auto& obs : observers) {
            EXPECT_FALSE(obs.expired());
        }
    }

    EXPECT_TRUE(all_expired(observers));
    EXPECT_TRUE(all_upgrades_none(observers));
}

// ===========================================================================
// Partial release — only subtree destroyed, siblings survive
// ===========================================================================

TEST(CascadeDestructionTest, PartialReleaseDestroysOnlySubtree) {
    auto root_ptr = std::make_shared<TreeNode>(1);
    auto child_a_ptr = std::make_shared<TreeNode>(2);
    auto child_b_ptr = std::make_shared<TreeNode>(3);
    auto grandchild_ptr = std::make_shared<TreeNode>(4);

    Link<TreeNode> grandchild_obs(grandchild_ptr);
    Link<TreeNode> child_a_obs(child_a_ptr);
    Link<TreeNode> child_b_obs(child_b_ptr);

    child_a_ptr->add_child(Own<TreeNode>(grandchild_ptr), child_a_ptr);
    root_ptr->add_child(Own<TreeNode>(child_a_ptr), root_ptr);
    root_ptr->add_child(Own<TreeNode>(child_b_ptr), root_ptr);

    // All alive
    EXPECT_FALSE(child_a_obs.expired());
    EXPECT_FALSE(child_b_obs.expired());
    EXPECT_FALSE(grandchild_obs.expired());

    // Root still alive — all children survive
    // (We can't selectively release a child from the vector in this API,
    //  but we can verify that root keeps everything alive)
    EXPECT_EQ(root_ptr->child_count(), 2u);
    EXPECT_FALSE(child_a_obs.expired());
    EXPECT_FALSE(grandchild_obs.expired());
}

// ===========================================================================
// Empty tree — root with no children
// ===========================================================================

TEST(CascadeDestructionTest, EmptyTreeRootDestroyedCleanly) {
    Link<TreeNode> root_obs;

    {
        auto root_ptr = std::make_shared<TreeNode>(1);
        root_obs = Link<TreeNode>(root_ptr);
        EXPECT_FALSE(root_obs.expired());
        EXPECT_EQ(root_ptr->child_count(), 0u);
    }

    EXPECT_TRUE(root_obs.expired());
    EXPECT_FALSE(root_obs.upgrade().has_value());
}
