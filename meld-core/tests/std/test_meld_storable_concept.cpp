#include <gtest/gtest.h>
#include "meld/std/meld_storable_concept.hpp"
#include "meld/std/mem.hpp"
#include "meld/types/memory.hpp"
#include <memory>
#include <string>
#include <type_traits>

using namespace meld::std_mem;
using namespace meld::types;

// ---------------------------------------------------------------------------
// Test types — ManagedObject-derived types for concept satisfaction tests
// ---------------------------------------------------------------------------

class ConceptTestNode : public ManagedObject {
public:
    explicit ConceptTestNode(std::string name) : name_(std::move(name)) {}
    const std::string& name() const { return name_; }
private:
    std::string name_;
};

class ConceptTestWidget : public ManagedObject {
public:
    explicit ConceptTestWidget(int id) : id_(id) {}
    int id() const { return id_; }
private:
    int id_;
};

// ===========================================================================
// 11.1 — MeldStorable concept satisfaction
// Requirements: 9.1, 9.2, 9.4
// ===========================================================================

// ---------------------------------------------------------------------------
// Hold[T] satisfies MeldStorable
// ---------------------------------------------------------------------------

TEST(MeldStorableConceptTest, OwnSatisfiesConcept) {
    // Hold[T] must satisfy MeldStorable — it provides get_ref_count() and is_owning()
    static_assert(MeldStorable<Own<ConceptTestNode>>,
                  "Own<T> must satisfy MeldStorable concept");
    static_assert(MeldStorable<Own<ConceptTestWidget>>,
                  "Own<T> must satisfy MeldStorable concept for any ManagedObject-derived T");
}

TEST(MeldStorableConceptTest, OwnGetRefCountReturnsUint64) {
    auto ptr = std::make_shared<ConceptTestNode>("Alice");
    Own<ConceptTestNode> own(ptr);

    uint64_t count = own.get_ref_count();
    EXPECT_GE(count, 1u);
}

TEST(MeldStorableConceptTest, OwnIsOwningReturnsBool) {
    auto ptr = std::make_shared<ConceptTestNode>("Bob");
    Own<ConceptTestNode> own(ptr);

    bool owning = own.is_owning();
    EXPECT_TRUE(owning);
}

// ---------------------------------------------------------------------------
// View[T] satisfies MeldStorable
// ---------------------------------------------------------------------------

TEST(MeldStorableConceptTest, LinkSatisfiesConcept) {
    // View[T] must satisfy MeldStorable — it provides get_ref_count() and is_owning()
    static_assert(MeldStorable<Link<ConceptTestNode>>,
                  "Link<T> must satisfy MeldStorable concept");
    static_assert(MeldStorable<Link<ConceptTestWidget>>,
                  "Link<T> must satisfy MeldStorable concept for any ManagedObject-derived T");
}

TEST(MeldStorableConceptTest, LinkGetRefCountReturnsUint64) {
    auto ptr = std::make_shared<ConceptTestNode>("Carol");
    Own<ConceptTestNode> own(ptr);
    Link<ConceptTestNode> lnk = own.link();

    uint64_t count = lnk.get_ref_count();
    // The object is alive, so count should reflect the strong references
    EXPECT_GE(count, 1u);
}

TEST(MeldStorableConceptTest, LinkIsOwningReturnsBool) {
    auto ptr = std::make_shared<ConceptTestNode>("Dave");
    Own<ConceptTestNode> own(ptr);
    Link<ConceptTestNode> lnk = own.link();

    bool owning = lnk.is_owning();
    EXPECT_FALSE(owning);
}

TEST(MeldStorableConceptTest, LinkGetRefCountZeroWhenDead) {
    Link<ConceptTestNode> lnk;
    {
        auto ptr = std::make_shared<ConceptTestNode>("Eve");
        lnk = Link<ConceptTestNode>(ptr);
    }
    // Object is dead — get_ref_count() should return 0
    EXPECT_EQ(lnk.get_ref_count(), 0u);
}

// ---------------------------------------------------------------------------
// Raw types do NOT satisfy MeldStorable (Requirement 9.3)
// ---------------------------------------------------------------------------

TEST(MeldStorableConceptTest, RawTypesDoNotSatisfyConcept) {
    // Raw types must NOT satisfy MeldStorable
    static_assert(!MeldStorable<int>,
                  "int must not satisfy MeldStorable concept");
    static_assert(!MeldStorable<double>,
                  "double must not satisfy MeldStorable concept");
    static_assert(!MeldStorable<std::string>,
                  "std::string must not satisfy MeldStorable concept");
    static_assert(!MeldStorable<float>,
                  "float must not satisfy MeldStorable concept");
    static_assert(!MeldStorable<bool>,
                  "bool must not satisfy MeldStorable concept");
    static_assert(!MeldStorable<char>,
                  "char must not satisfy MeldStorable concept");
    static_assert(!MeldStorable<void*>,
                  "void* must not satisfy MeldStorable concept");
    static_assert(!MeldStorable<ConceptTestNode>,
                  "Raw ManagedObject-derived type must not satisfy MeldStorable");
    static_assert(!MeldStorable<ConceptTestWidget>,
                  "Raw ManagedObject-derived type must not satisfy MeldStorable");
}

// ===========================================================================
// 11.1 — Vector<T> constrained by MeldStorable
// Requirements: 9.2
// ===========================================================================

// ---------------------------------------------------------------------------
// Vector<Own<T>> compiles and works
// ---------------------------------------------------------------------------

TEST(MeldStorableVectorTest, VectorOfOwnCompiles) {
    Vector<Own<ConceptTestNode>> vec;
    EXPECT_EQ(vec.size(), 0u);
    EXPECT_TRUE(vec.empty());
}

TEST(MeldStorableVectorTest, VectorOfOwnPushAndGet) {
    Vector<Own<ConceptTestNode>> vec;

    auto ptr1 = std::make_shared<ConceptTestNode>("Alice");
    auto ptr2 = std::make_shared<ConceptTestNode>("Bob");

    vec.push(Own<ConceptTestNode>(ptr1));
    vec.push(Own<ConceptTestNode>(ptr2));

    EXPECT_EQ(vec.size(), 2u);

    auto elem0 = vec.get(0);
    ASSERT_TRUE(elem0.has_value());
    EXPECT_EQ(elem0->get().get().name(), "Alice");

    auto elem1 = vec.get(1);
    ASSERT_TRUE(elem1.has_value());
    EXPECT_EQ(elem1->get().get().name(), "Bob");
}

TEST(MeldStorableVectorTest, VectorOfOwnGetOutOfBounds) {
    Vector<Own<ConceptTestNode>> vec;
    auto result = vec.get(0);
    EXPECT_FALSE(result.has_value());
}

TEST(MeldStorableVectorTest, VectorOfOwnIteration) {
    Vector<Own<ConceptTestNode>> vec;

    vec.push(Own<ConceptTestNode>(std::make_shared<ConceptTestNode>("A")));
    vec.push(Own<ConceptTestNode>(std::make_shared<ConceptTestNode>("B")));
    vec.push(Own<ConceptTestNode>(std::make_shared<ConceptTestNode>("C")));

    size_t count = 0;
    for (auto& elem : vec) {
        EXPECT_TRUE(elem.is_owning());
        count++;
    }
    EXPECT_EQ(count, 3u);
}

// ---------------------------------------------------------------------------
// Vector<Link<T>> compiles and works
// ---------------------------------------------------------------------------

TEST(MeldStorableVectorTest, VectorOfLinkCompiles) {
    Vector<Link<ConceptTestNode>> vec;
    EXPECT_EQ(vec.size(), 0u);
    EXPECT_TRUE(vec.empty());
}

TEST(MeldStorableVectorTest, VectorOfLinkPushAndGet) {
    auto ptr = std::make_shared<ConceptTestNode>("Carol");
    Own<ConceptTestNode> own(ptr);

    Vector<Link<ConceptTestNode>> vec;
    vec.push(own.link());

    EXPECT_EQ(vec.size(), 1u);

    auto elem = vec.get(0);
    ASSERT_TRUE(elem.has_value());
    EXPECT_FALSE(elem->get().is_owning());
}

TEST(MeldStorableVectorTest, VectorOfLinkIteration) {
    auto ptr1 = std::make_shared<ConceptTestNode>("X");
    auto ptr2 = std::make_shared<ConceptTestNode>("Y");
    Own<ConceptTestNode> own1(ptr1);
    Own<ConceptTestNode> own2(ptr2);

    Vector<Link<ConceptTestNode>> vec;
    vec.push(own1.link());
    vec.push(own2.link());

    size_t count = 0;
    for (const auto& elem : vec) {
        EXPECT_FALSE(elem.is_owning());
        count++;
    }
    EXPECT_EQ(count, 2u);
}

// ---------------------------------------------------------------------------
// Concept mirrors Storable trait method signatures (Requirement 9.4)
// ---------------------------------------------------------------------------

TEST(MeldStorableConceptTest, ConceptMirrorsStorableTrait) {
    // Verify that types satisfying MeldStorable also derive from Storable
    // This ensures the concept mirrors the trait's method signatures
    auto ptr = std::make_shared<ConceptTestNode>("Mirror");
    Own<ConceptTestNode> own(ptr);
    Link<ConceptTestNode> lnk = own.link();

    // Both satisfy MeldStorable
    static_assert(MeldStorable<Own<ConceptTestNode>>);
    static_assert(MeldStorable<Link<ConceptTestNode>>);

    // Both are Storable (trait implementation)
    Storable* own_storable = &own;
    Storable* link_storable = &lnk;

    // The concept's is_owning() matches the trait's is_owning()
    EXPECT_EQ(own.is_owning(), own_storable->is_owning());
    EXPECT_EQ(lnk.is_owning(), link_storable->is_owning());
}
