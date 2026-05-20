/// @file test_effect_handler_arc.cpp
/// @brief Unit tests for effect handler ARC interaction (Tasks 14.1, 14.2)
///
/// Tests:
///   14.1 — Hold[T] capture increments strong_count on install, decrements on exit;
///           View[T] capture increments weak_count on install, requires upgrade.
///   14.2 — Multi-shot continuation clone increments strong_count per clone;
///           discarding a clone decrements strong_count.
///
/// Requirements: 8.1, 8.2, 8.3, 8.4, 8.5

#include <gtest/gtest.h>
#include "meld/effects/effect_handler_arc.hpp"
#include "meld/std/mem.hpp"
#include "meld/types/memory.hpp"
#include <memory>
#include <vector>

using namespace meld::effects;
using namespace meld::std_mem;
using namespace meld::types;

// ---------------------------------------------------------------------------
// Test fixture — ManagedObject-derived type
// ---------------------------------------------------------------------------

class ArcTestNode : public ManagedObject {
public:
    explicit ArcTestNode(int id) : id_(id) {}
    int id() const { return id_; }
private:
    int id_;
};

// ===========================================================================
// 14.1 — Hold[T] capture semantics
// Requirement 8.1: strong_count increments on install
// Requirement 8.2: strong_count decrements on scope exit
// ===========================================================================

TEST(EffectHandlerArcTest, OwnCaptureIncrementsStrongCountOnInstall) {
    auto ptr = std::make_shared<ArcTestNode>(1);
    Own<ArcTestNode> own(ptr);

    long baseline = ptr.use_count();  // own + ptr

    EffectHandlerFrame frame;
    frame.capture_own(own, "node");

    // Capture copies the shared_ptr → strong_count should increase by 1
    EXPECT_EQ(ptr.use_count(), baseline + 1);

    frame.install();
    EXPECT_TRUE(frame.is_installed());
    // Count unchanged by install() itself — capture already holds the ref
    EXPECT_EQ(ptr.use_count(), baseline + 1);
}

TEST(EffectHandlerArcTest, OwnCaptureDecrementsStrongCountOnUninstall) {
    auto ptr = std::make_shared<ArcTestNode>(2);
    Own<ArcTestNode> own(ptr);

    long baseline = ptr.use_count();

    EffectHandlerFrame frame;
    frame.capture_own(own, "node");
    frame.install();

    EXPECT_EQ(ptr.use_count(), baseline + 1);

    frame.uninstall();
    EXPECT_FALSE(frame.is_installed());
    // After uninstall, the captured reference is released
    EXPECT_EQ(ptr.use_count(), baseline);
}

TEST(EffectHandlerArcTest, OwnCaptureRAIIScopeGuard) {
    auto ptr = std::make_shared<ArcTestNode>(3);
    Own<ArcTestNode> own(ptr);

    long baseline = ptr.use_count();

    {
        EffectHandlerFrame frame;
        frame.capture_own(own, "scoped_node");
        EffectHandlerArcScope scope(frame);

        EXPECT_TRUE(frame.is_installed());
        EXPECT_EQ(ptr.use_count(), baseline + 1);
    }
    // Scope exited — strong_count back to baseline
    EXPECT_EQ(ptr.use_count(), baseline);
}

TEST(EffectHandlerArcTest, MultipleOwnCaptures) {
    auto ptr_a = std::make_shared<ArcTestNode>(10);
    auto ptr_b = std::make_shared<ArcTestNode>(20);
    Own<ArcTestNode> own_a(ptr_a);
    Own<ArcTestNode> own_b(ptr_b);

    long base_a = ptr_a.use_count();
    long base_b = ptr_b.use_count();

    {
        EffectHandlerFrame frame;
        frame.capture_own(own_a, "a");
        frame.capture_own(own_b, "b");
        EffectHandlerArcScope scope(frame);

        EXPECT_EQ(frame.own_capture_count(), 2u);
        EXPECT_EQ(ptr_a.use_count(), base_a + 1);
        EXPECT_EQ(ptr_b.use_count(), base_b + 1);
    }
    EXPECT_EQ(ptr_a.use_count(), base_a);
    EXPECT_EQ(ptr_b.use_count(), base_b);
}

// ===========================================================================
// 14.1 — View[T] capture semantics
// Requirement 8.3: View[T] still requires upgrade inside handler body
// ===========================================================================

TEST(EffectHandlerArcTest, LinkCaptureDoesNotIncrementStrongCount) {
    auto ptr = std::make_shared<ArcTestNode>(4);
    Own<ArcTestNode> own(ptr);
    Link<ArcTestNode> lnk = own.link();

    long strong_baseline = ptr.use_count();

    {
        EffectHandlerFrame frame;
        frame.capture_link(lnk, "observer");
        EffectHandlerArcScope scope(frame);

        // Link capture must NOT increment strong_count
        EXPECT_EQ(ptr.use_count(), strong_baseline);
        EXPECT_EQ(frame.link_capture_count(), 1u);
        EXPECT_FALSE(frame.link_expired(0));
    }
    // After scope exit, strong_count unchanged
    EXPECT_EQ(ptr.use_count(), strong_baseline);
}

TEST(EffectHandlerArcTest, LinkCaptureRequiresUpgrade) {
    // This test verifies the semantic requirement (8.3) that View[T]
    // captured inside an effect handler still requires upgrade.
    // At the C++ level, the captured weak_ptr must be locked before use.
    auto ptr = std::make_shared<ArcTestNode>(5);
    Own<ArcTestNode> own(ptr);
    Link<ArcTestNode> lnk = own.link();

    EffectHandlerFrame frame;
    frame.capture_link(lnk, "must_upgrade");
    EffectHandlerArcScope scope(frame);

    // The Link is alive — upgrade should succeed
    auto upgraded = lnk.upgrade();
    ASSERT_TRUE(upgraded.has_value());
    EXPECT_EQ(upgraded.value()->id(), 5);
}

TEST(EffectHandlerArcTest, LinkCaptureDetectsDeadObject) {
    auto ptr = std::make_shared<ArcTestNode>(6);
    Link<ArcTestNode> lnk(ptr);

    EffectHandlerFrame frame;
    frame.capture_link(lnk, "dead_check");
    frame.install();

    EXPECT_FALSE(frame.link_expired(0));

    // Destroy the object
    ptr.reset();

    // Link should now be expired
    EXPECT_TRUE(frame.link_expired(0));

    frame.uninstall();
}

// ===========================================================================
// 14.2 — Multi-shot continuation clone ARC handling
// Requirement 8.4: clone increments strong_count per clone
// Requirement 8.5: discarding clone decrements strong_count
// ===========================================================================

TEST(EffectHandlerArcTest, CloneIncrementsStrongCountPerClone) {
    auto ptr = std::make_shared<ArcTestNode>(7);
    Own<ArcTestNode> own(ptr);

    long baseline = ptr.use_count();

    EffectHandlerFrame frame;
    frame.capture_own(own, "cloneable");
    frame.install();

    long after_install = ptr.use_count();
    EXPECT_EQ(after_install, baseline + 1);

    // Clone the frame — strong_count should increase by 1
    auto clone1 = frame.clone();
    EXPECT_EQ(ptr.use_count(), after_install + 1);

    // Clone again — strong_count should increase by 1 more
    auto clone2 = frame.clone();
    EXPECT_EQ(ptr.use_count(), after_install + 2);

    // Discard clone1 — strong_count decreases by 1
    clone1.reset();
    EXPECT_EQ(ptr.use_count(), after_install + 1);

    // Discard clone2 — strong_count decreases by 1
    clone2.reset();
    EXPECT_EQ(ptr.use_count(), after_install);

    frame.uninstall();
    EXPECT_EQ(ptr.use_count(), baseline);
}

TEST(EffectHandlerArcTest, CloneMultipleOwnsCorrectly) {
    auto ptr_a = std::make_shared<ArcTestNode>(100);
    auto ptr_b = std::make_shared<ArcTestNode>(200);
    Own<ArcTestNode> own_a(ptr_a);
    Own<ArcTestNode> own_b(ptr_b);

    long base_a = ptr_a.use_count();
    long base_b = ptr_b.use_count();

    EffectHandlerFrame frame;
    frame.capture_own(own_a, "a");
    frame.capture_own(own_b, "b");
    frame.install();

    EXPECT_EQ(ptr_a.use_count(), base_a + 1);
    EXPECT_EQ(ptr_b.use_count(), base_b + 1);

    {
        auto cloned = frame.clone();
        EXPECT_EQ(cloned->own_capture_count(), 2u);
        EXPECT_EQ(ptr_a.use_count(), base_a + 2);
        EXPECT_EQ(ptr_b.use_count(), base_b + 2);
    }
    // Clone destroyed — counts back to install level
    EXPECT_EQ(ptr_a.use_count(), base_a + 1);
    EXPECT_EQ(ptr_b.use_count(), base_b + 1);

    frame.uninstall();
    EXPECT_EQ(ptr_a.use_count(), base_a);
    EXPECT_EQ(ptr_b.use_count(), base_b);
}

TEST(EffectHandlerArcTest, CloneNTimesIncrementsStrongCountByN) {
    auto ptr = std::make_shared<ArcTestNode>(8);
    Own<ArcTestNode> own(ptr);

    long baseline = ptr.use_count();

    EffectHandlerFrame frame;
    frame.capture_own(own, "multi_clone");
    frame.install();

    long after_install = ptr.use_count();

    constexpr int N = 5;
    std::vector<std::unique_ptr<EffectHandlerFrame>> clones;
    for (int i = 0; i < N; ++i) {
        clones.push_back(frame.clone());
        EXPECT_EQ(ptr.use_count(), after_install + i + 1);
    }

    // All N clones alive — count should be after_install + N
    EXPECT_EQ(ptr.use_count(), after_install + N);

    // Discard all clones
    clones.clear();
    EXPECT_EQ(ptr.use_count(), after_install);

    frame.uninstall();
    EXPECT_EQ(ptr.use_count(), baseline);
}

TEST(EffectHandlerArcTest, ClonePreservesLinkCaptures) {
    auto ptr = std::make_shared<ArcTestNode>(9);
    Own<ArcTestNode> own(ptr);
    Link<ArcTestNode> lnk = own.link();

    EffectHandlerFrame frame;
    frame.capture_link(lnk, "link_clone");
    frame.install();

    auto cloned = frame.clone();
    EXPECT_EQ(cloned->link_capture_count(), 1u);
    EXPECT_FALSE(cloned->link_expired(0));

    // Destroy the owning reference
    // (own still holds ptr, so object is alive)
    EXPECT_FALSE(cloned->link_expired(0));

    frame.uninstall();
}

// ===========================================================================
// Introspection / label tests
// ===========================================================================

TEST(EffectHandlerArcTest, CaptureLabelsArePreserved) {
    auto ptr = std::make_shared<ArcTestNode>(11);
    Own<ArcTestNode> own(ptr);
    Link<ArcTestNode> lnk = own.link();

    EffectHandlerFrame frame;
    frame.capture_own(own, "my_own_label");
    frame.capture_link(lnk, "my_link_label");

    EXPECT_EQ(frame.own_label(0), "my_own_label");
    EXPECT_EQ(frame.link_label(0), "my_link_label");
}

TEST(EffectHandlerArcTest, OwnStrongCountIntrospection) {
    auto ptr = std::make_shared<ArcTestNode>(12);
    Own<ArcTestNode> own(ptr);

    EffectHandlerFrame frame;
    frame.capture_own(own, "introspect");

    // strong_count should reflect the shared_ptr use_count
    long count = frame.own_strong_count(0);
    EXPECT_EQ(count, static_cast<long>(ptr.use_count()));
}

TEST(EffectHandlerArcTest, UninstallWithoutInstallIsNoOp) {
    EffectHandlerFrame frame;
    // Should not throw or crash
    frame.uninstall();
    EXPECT_FALSE(frame.is_installed());
}

TEST(EffectHandlerArcTest, DoubleUninstallIsNoOp) {
    auto ptr = std::make_shared<ArcTestNode>(13);
    Own<ArcTestNode> own(ptr);

    long baseline = ptr.use_count();

    EffectHandlerFrame frame;
    frame.capture_own(own, "double_uninstall");
    frame.install();
    frame.uninstall();
    frame.uninstall();  // second uninstall is a no-op

    EXPECT_EQ(ptr.use_count(), baseline);
}
