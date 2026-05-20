#include <gtest/gtest.h>
#include "meld/std/mem.hpp"
#include "meld/types/memory.hpp"

using namespace meld::std_mem;
using namespace meld::types;

// ---------------------------------------------------------------------------
// Test fixture: a simple ManagedObject-derived type for testing
// ---------------------------------------------------------------------------

class TestNode : public ManagedObject {
public:
    explicit TestNode(std::string name) : name_(std::move(name)) {}
    const std::string& name() const { return name_; }

private:
    std::string name_;
};

// ===========================================================================
// 1.1 — Storable trait with @intrinsic(memory_strategy)
// ===========================================================================

TEST(StorableTest, IntrinsicAnnotation) {
    // The Storable trait must expose the memory_strategy intrinsic tag
    EXPECT_STREQ(Storable::intrinsic_annotation(), "memory_strategy");
}

TEST(StorableTest, IntrinsicTagConstants) {
    EXPECT_STREQ(IntrinsicTag::memory_strategy, "memory_strategy");
    EXPECT_STREQ(IntrinsicTag::managed_container, "managed_container");
    EXPECT_STREQ(IntrinsicTag::memory_move, "memory_move");
}

// ===========================================================================
// 1.2 — Hold[T] generic type
// ===========================================================================

TEST(OwnTest, DirectMemberAccess) {
    auto ptr = std::make_shared<TestNode>("Alice");
    Own<TestNode> own(ptr);

    // get() provides direct access — no nil check
    EXPECT_EQ(own.get().name(), "Alice");
    EXPECT_EQ(own->name(), "Alice");
    EXPECT_EQ((*own).name(), "Alice");
}

TEST(OwnTest, IsOwningReturnsTrue) {
    auto ptr = std::make_shared<TestNode>("Bob");
    Own<TestNode> own(ptr);

    // Storable::is_owning() must return true for Hold[T]
    EXPECT_TRUE(own.is_owning());
}

TEST(OwnTest, AccessReturnsValidPointer) {
    auto ptr = std::make_shared<TestNode>("Carol");
    Own<TestNode> own(ptr);

    auto accessed = own.access();
    EXPECT_TRUE(accessed.has_value());
    EXPECT_NE(accessed.value(), nullptr);
}

TEST(OwnTest, LinkCreation) {
    auto ptr = std::make_shared<TestNode>("Dave");
    Own<TestNode> own(ptr);

    // link() creates a View[T] observation reference
    Link<TestNode> lnk = own.link();
    EXPECT_FALSE(lnk.expired());
}

TEST(OwnTest, CopySemanticsRetainCount) {
    auto ptr = std::make_shared<TestNode>("Eve");
    EXPECT_EQ(ptr.use_count(), 1);

    Own<TestNode> own1(ptr);
    EXPECT_EQ(ptr.use_count(), 2); // ptr + own1

    {
        Own<TestNode> own2(own1); // copy — retain
        EXPECT_EQ(ptr.use_count(), 3); // ptr + own1 + own2
    }
    // own2 destroyed — release
    EXPECT_EQ(ptr.use_count(), 2);
}

TEST(OwnTest, BoolConversion) {
    auto ptr = std::make_shared<TestNode>("Frank");
    Own<TestNode> own(ptr);
    EXPECT_TRUE(static_cast<bool>(own));
}

// ===========================================================================
// 1.3 — View[T] generic type
// ===========================================================================

TEST(LinkTest, UpgradeWhileAlive) {
    auto ptr = std::make_shared<TestNode>("Grace");
    Own<TestNode> own(ptr);
    Link<TestNode> lnk = own.link();

    // upgrade() returns a strong reference while the object is alive
    auto upgraded = lnk.upgrade();
    EXPECT_TRUE(upgraded.has_value());
    EXPECT_EQ(upgraded.value()->name(), "Grace");
}

TEST(LinkTest, UpgradeAfterDestruction) {
    Link<TestNode> lnk;
    {
        auto ptr = std::make_shared<TestNode>("Heidi");
        Own<TestNode> own(ptr);
        lnk = own.link();
        EXPECT_FALSE(lnk.expired());
    }
    // own and ptr destroyed — object is dead

    EXPECT_TRUE(lnk.expired());
    auto upgraded = lnk.upgrade();
    EXPECT_FALSE(upgraded.has_value());
}

TEST(LinkTest, IsOwningReturnsFalse) {
    auto ptr = std::make_shared<TestNode>("Ivan");
    Own<TestNode> own(ptr);
    Link<TestNode> lnk = own.link();

    // Storable::is_owning() must return false for View[T]
    EXPECT_FALSE(lnk.is_owning());
}

TEST(LinkTest, AccessReturnsValidWhileAlive) {
    auto ptr = std::make_shared<TestNode>("Judy");
    Own<TestNode> own(ptr);
    Link<TestNode> lnk = own.link();

    auto accessed = lnk.access();
    EXPECT_TRUE(accessed.has_value());
}

TEST(LinkTest, AccessReturnsNulloptWhenDead) {
    Link<TestNode> lnk;
    {
        auto ptr = std::make_shared<TestNode>("Karl");
        lnk = Link<TestNode>(ptr);
    }

    auto accessed = lnk.access();
    EXPECT_FALSE(accessed.has_value());
}

TEST(LinkTest, DoesNotPreventDeallocation) {
    std::weak_ptr<TestNode> observer;
    Link<TestNode> lnk;
    {
        auto ptr = std::make_shared<TestNode>("Liam");
        observer = ptr;
        lnk = Link<TestNode>(ptr);
        EXPECT_FALSE(observer.expired());
    }
    // View[T] does not keep the object alive
    EXPECT_TRUE(observer.expired());
    EXPECT_TRUE(lnk.expired());
}

// ===========================================================================
// 1.4 — std.mem.link() and std.mem.move()
// ===========================================================================

TEST(MemLinkTest, CreatesLinkFromOwn) {
    auto ptr = std::make_shared<TestNode>("Mallory");
    Own<TestNode> own(ptr);

    Link<TestNode> lnk = link(own);
    EXPECT_FALSE(lnk.expired());

    auto upgraded = lnk.upgrade();
    EXPECT_TRUE(upgraded.has_value());
    EXPECT_EQ(upgraded.value()->name(), "Mallory");
}

TEST(MemLinkTest, LinkDoesNotIncrementStrongCount) {
    auto ptr = std::make_shared<TestNode>("Niaj");
    long count_before = ptr.use_count();

    Own<TestNode> own(ptr);
    long count_after_own = ptr.use_count();

    Link<TestNode> lnk = link(own);
    long count_after_link = ptr.use_count();

    // link() should not change the strong count
    EXPECT_EQ(count_after_own, count_after_link);
    // But Own did increment it
    EXPECT_EQ(count_after_own, count_before + 1);
}

TEST(MemMoveTest, TransfersOwnership) {
    auto ptr = std::make_shared<TestNode>("Oscar");
    Own<TestNode> source(ptr);

    long count_before = ptr.use_count();
    Own<TestNode> dest = mem_move(source);
    long count_after = ptr.use_count();

    // Move should not change the reference count
    EXPECT_EQ(count_before, count_after);

    // Destination should be valid
    EXPECT_EQ(dest->name(), "Oscar");
}

TEST(MemMoveTest, MoveIntrinsicAnnotation) {
    // The move function must expose the memory_move intrinsic tag
    EXPECT_STREQ(move_intrinsic_annotation(), "memory_move");
}

// ===========================================================================
// Intrinsic Registry tests
// ===========================================================================

TEST(IntrinsicRegistryTest, RegisterAndLookup) {
    register_std_mem_intrinsics();

    // Verify the Storable trait is registered
    EXPECT_STREQ(Storable::intrinsic_annotation(), "memory_strategy");

    // Verify the move intrinsic is registered
    EXPECT_STREQ(move_intrinsic_annotation(), "memory_move");
}
