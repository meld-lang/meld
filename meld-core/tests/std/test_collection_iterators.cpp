/// Unit tests for collection iterator behavior (Task 12.1, 12.2).
///
/// Tests HoldVector<T> solid iterator and ViewVector<T> filtering iterator
/// plus the .entries() method.
///
/// Requirements: 6.1, 6.2, 6.3, 6.4, 6.5

#include <gtest/gtest.h>
#include "meld/std/collection_iterators.hpp"
#include "meld/std/mem.hpp"
#include "meld/types/memory.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

using namespace meld::std_mem;
using namespace meld::types;

// ---------------------------------------------------------------------------
// Test fixture: ManagedObject-derived type
// ---------------------------------------------------------------------------

class IterTestNode : public ManagedObject {
public:
    explicit IterTestNode(int id) : id_(id) {}
    int id() const { return id_; }
private:
    int id_;
};

// ===========================================================================
// 12.1 — HoldVector<T> solid iterator
// Requirement 6.1: default iterator yields T directly, non-null, alive
// ===========================================================================

TEST(HoldVectorTest, EmptyVectorIteratesZeroElements) {
    HoldVector<IterTestNode> vec;
    size_t count = 0;
    for ([[maybe_unused]] auto& elem : vec) {
        count++;
    }
    EXPECT_EQ(count, 0u);
    EXPECT_TRUE(vec.empty());
}

TEST(HoldVectorTest, SingleElementIteration) {
    HoldVector<IterTestNode> vec;
    vec.push(Own<IterTestNode>(std::make_shared<IterTestNode>(42)));

    size_t count = 0;
    for (auto& elem : vec) {
        EXPECT_EQ(elem.id(), 42);
        count++;
    }
    EXPECT_EQ(count, 1u);
}

TEST(HoldVectorTest, MultipleElementsAllYielded) {
    HoldVector<IterTestNode> vec;
    for (int i = 0; i < 5; ++i) {
        vec.push(Own<IterTestNode>(std::make_shared<IterTestNode>(i * 10)));
    }

    EXPECT_EQ(vec.size(), 5u);

    std::vector<int> ids;
    for (auto& elem : vec) {
        ids.push_back(elem.id());
    }

    ASSERT_EQ(ids.size(), 5u);
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(ids[i], i * 10);
    }
}

TEST(HoldVectorTest, IteratorYieldsDirectReference) {
    HoldVector<IterTestNode> vec;
    auto ptr = std::make_shared<IterTestNode>(99);
    vec.push(Own<IterTestNode>(ptr));

    auto it = vec.begin();
    // operator* yields T& directly — no null check needed
    IterTestNode& ref = *it;
    EXPECT_EQ(ref.id(), 99);
    // operator-> also works
    EXPECT_EQ(it->id(), 99);
}

TEST(HoldVectorTest, ConstIteration) {
    HoldVector<IterTestNode> vec;
    vec.push(Own<IterTestNode>(std::make_shared<IterTestNode>(1)));
    vec.push(Own<IterTestNode>(std::make_shared<IterTestNode>(2)));

    const auto& cvec = vec;
    size_t count = 0;
    for (const auto& elem : cvec) {
        EXPECT_GT(elem.id(), 0);
        count++;
    }
    EXPECT_EQ(count, 2u);
}

TEST(HoldVectorTest, PostIncrementIterator) {
    HoldVector<IterTestNode> vec;
    vec.push(Own<IterTestNode>(std::make_shared<IterTestNode>(10)));
    vec.push(Own<IterTestNode>(std::make_shared<IterTestNode>(20)));

    auto it = vec.begin();
    auto prev = it++;
    EXPECT_EQ(prev->id(), 10);
    EXPECT_EQ(it->id(), 20);
}

// ===========================================================================
// 12.2 — ViewVector<T> filtering iterator
// Requirement 6.2: default iterator skips dead entries
// Requirement 6.4: skips dead entries without error
// ===========================================================================

TEST(ViewVectorTest, EmptyVectorIteratesZeroElements) {
    ViewVector<IterTestNode> vec;
    size_t count = 0;
    for ([[maybe_unused]] auto& elem : vec) {
        count++;
    }
    EXPECT_EQ(count, 0u);
}

TEST(ViewVectorTest, AllAliveElementsYielded) {
    auto p1 = std::make_shared<IterTestNode>(1);
    auto p2 = std::make_shared<IterTestNode>(2);
    auto p3 = std::make_shared<IterTestNode>(3);

    ViewVector<IterTestNode> vec;
    vec.push(Link<IterTestNode>(p1));
    vec.push(Link<IterTestNode>(p2));
    vec.push(Link<IterTestNode>(p3));

    std::vector<int> ids;
    for (auto& elem : vec) {
        ids.push_back(elem.id());
    }

    ASSERT_EQ(ids.size(), 3u);
    EXPECT_EQ(ids[0], 1);
    EXPECT_EQ(ids[1], 2);
    EXPECT_EQ(ids[2], 3);
}

TEST(ViewVectorTest, DeadEntriesSkipped) {
    auto p1 = std::make_shared<IterTestNode>(10);
    auto p2 = std::make_shared<IterTestNode>(20);
    auto p3 = std::make_shared<IterTestNode>(30);

    ViewVector<IterTestNode> vec;
    vec.push(Link<IterTestNode>(p1));
    vec.push(Link<IterTestNode>(p2));
    vec.push(Link<IterTestNode>(p3));

    // Kill the middle element
    p2.reset();

    std::vector<int> ids;
    for (auto& elem : vec) {
        ids.push_back(elem.id());
    }

    // Only 2 live elements should be yielded
    ASSERT_EQ(ids.size(), 2u);
    EXPECT_EQ(ids[0], 10);
    EXPECT_EQ(ids[1], 30);
}

TEST(ViewVectorTest, AllDeadEntriesYieldZeroElements) {
    ViewVector<IterTestNode> vec;
    {
        auto p1 = std::make_shared<IterTestNode>(1);
        auto p2 = std::make_shared<IterTestNode>(2);
        vec.push(Link<IterTestNode>(p1));
        vec.push(Link<IterTestNode>(p2));
    }
    // Both objects destroyed

    EXPECT_EQ(vec.size(), 2u); // slots still exist
    size_t count = 0;
    for ([[maybe_unused]] auto& elem : vec) {
        count++;
    }
    EXPECT_EQ(count, 0u); // but default iterator yields nothing
}

TEST(ViewVectorTest, FirstElementDead) {
    auto p1 = std::make_shared<IterTestNode>(100);
    auto p2 = std::make_shared<IterTestNode>(200);

    ViewVector<IterTestNode> vec;
    vec.push(Link<IterTestNode>(p1));
    vec.push(Link<IterTestNode>(p2));

    p1.reset(); // kill first

    std::vector<int> ids;
    for (auto& elem : vec) {
        ids.push_back(elem.id());
    }
    ASSERT_EQ(ids.size(), 1u);
    EXPECT_EQ(ids[0], 200);
}

TEST(ViewVectorTest, LastElementDead) {
    auto p1 = std::make_shared<IterTestNode>(100);
    auto p2 = std::make_shared<IterTestNode>(200);

    ViewVector<IterTestNode> vec;
    vec.push(Link<IterTestNode>(p1));
    vec.push(Link<IterTestNode>(p2));

    p2.reset(); // kill last

    std::vector<int> ids;
    for (auto& elem : vec) {
        ids.push_back(elem.id());
    }
    ASSERT_EQ(ids.size(), 1u);
    EXPECT_EQ(ids[0], 100);
}

// ===========================================================================
// 12.2 — ViewVector<T> .entries() iterator
// Requirement 6.3: .entries() yields optional[T] for every slot
// Requirement 6.5: dead entries yield none
// ===========================================================================

TEST(ViewVectorEntriesTest, AllAliveEntriesYieldValues) {
    auto p1 = std::make_shared<IterTestNode>(1);
    auto p2 = std::make_shared<IterTestNode>(2);

    ViewVector<IterTestNode> vec;
    vec.push(Link<IterTestNode>(p1));
    vec.push(Link<IterTestNode>(p2));

    size_t total = 0;
    size_t live = 0;
    for (auto entry : vec.entries()) {
        total++;
        if (entry.has_value()) {
            live++;
        }
    }
    EXPECT_EQ(total, 2u);
    EXPECT_EQ(live, 2u);
}

TEST(ViewVectorEntriesTest, DeadEntriesYieldNullopt) {
    auto p1 = std::make_shared<IterTestNode>(10);
    auto p2 = std::make_shared<IterTestNode>(20);
    auto p3 = std::make_shared<IterTestNode>(30);

    ViewVector<IterTestNode> vec;
    vec.push(Link<IterTestNode>(p1));
    vec.push(Link<IterTestNode>(p2));
    vec.push(Link<IterTestNode>(p3));

    // Kill the middle element
    p2.reset();

    size_t total = 0;
    size_t live = 0;
    size_t dead = 0;
    std::vector<int> live_ids;

    for (auto entry : vec.entries()) {
        total++;
        if (entry.has_value()) {
            live++;
            live_ids.push_back(entry->get().id());
        } else {
            dead++;
        }
    }

    EXPECT_EQ(total, 3u);  // all slots visited
    EXPECT_EQ(live, 2u);
    EXPECT_EQ(dead, 1u);
    ASSERT_EQ(live_ids.size(), 2u);
    EXPECT_EQ(live_ids[0], 10);
    EXPECT_EQ(live_ids[1], 30);
}

TEST(ViewVectorEntriesTest, AllDeadEntriesYieldNullopt) {
    ViewVector<IterTestNode> vec;
    {
        auto p1 = std::make_shared<IterTestNode>(1);
        auto p2 = std::make_shared<IterTestNode>(2);
        auto p3 = std::make_shared<IterTestNode>(3);
        vec.push(Link<IterTestNode>(p1));
        vec.push(Link<IterTestNode>(p2));
        vec.push(Link<IterTestNode>(p3));
    }

    size_t total = 0;
    size_t dead = 0;
    for (auto entry : vec.entries()) {
        total++;
        if (!entry.has_value()) dead++;
    }
    EXPECT_EQ(total, 3u);
    EXPECT_EQ(dead, 3u);
}

TEST(ViewVectorEntriesTest, EmptyVectorEntriesYieldNothing) {
    ViewVector<IterTestNode> vec;
    size_t total = 0;
    for ([[maybe_unused]] auto entry : vec.entries()) {
        total++;
    }
    EXPECT_EQ(total, 0u);
}

TEST(ViewVectorEntriesTest, EntriesPreservesSlotCount) {
    auto p1 = std::make_shared<IterTestNode>(1);
    auto p2 = std::make_shared<IterTestNode>(2);
    auto p3 = std::make_shared<IterTestNode>(3);
    auto p4 = std::make_shared<IterTestNode>(4);

    ViewVector<IterTestNode> vec;
    vec.push(Link<IterTestNode>(p1));
    vec.push(Link<IterTestNode>(p2));
    vec.push(Link<IterTestNode>(p3));
    vec.push(Link<IterTestNode>(p4));

    // Kill 2 of 4
    p1.reset();
    p3.reset();

    size_t entries_count = 0;
    for ([[maybe_unused]] auto entry : vec.entries()) {
        entries_count++;
    }
    // .entries() must yield exactly n elements (all slots)
    EXPECT_EQ(entries_count, 4u);

    size_t filter_count = 0;
    for ([[maybe_unused]] auto& elem : vec) {
        filter_count++;
    }
    // Default iterator yields only live elements
    EXPECT_EQ(filter_count, 2u);
}

TEST(ViewVectorTest, LiveCountReflectsDeadEntries) {
    auto p1 = std::make_shared<IterTestNode>(1);
    auto p2 = std::make_shared<IterTestNode>(2);

    ViewVector<IterTestNode> vec;
    vec.push(Link<IterTestNode>(p1));
    vec.push(Link<IterTestNode>(p2));

    EXPECT_EQ(vec.live_count(), 2u);
    p1.reset();
    EXPECT_EQ(vec.live_count(), 1u);
    p2.reset();
    EXPECT_EQ(vec.live_count(), 0u);
}
