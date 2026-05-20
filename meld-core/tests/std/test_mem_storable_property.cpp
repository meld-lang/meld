/// Property-based tests for Storable trait consistency (Property 3).
///
/// Feature: own-link-memory-model, Property 3: Hold[T] and View[T] Storable trait consistency
/// Validates: Requirements 1.6, 2.5
///
/// For any type T (derived from ManagedObject), Hold[T].is_owning() == true
/// and View[T].is_owning() == false. Both types implement the Storable trait.

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "meld/std/mem.hpp"
#include "meld/types/memory.hpp"
#include <memory>
#include <string>

using namespace meld::std_mem;
using namespace meld::types;

// ---------------------------------------------------------------------------
// Test types — various ManagedObject-derived types to exercise generics
// ---------------------------------------------------------------------------

class TypeA : public ManagedObject {
public:
    explicit TypeA(int id) : id_(id) {}
    int id() const { return id_; }
private:
    int id_;
};

class TypeB : public ManagedObject {
public:
    explicit TypeB(std::string label) : label_(std::move(label)) {}
    const std::string& label() const { return label_; }
private:
    std::string label_;
};

class TypeC : public ManagedObject {
public:
    TypeC() = default;
};

// ---------------------------------------------------------------------------
// Property 3: Hold[T] and View[T] Storable trait consistency
// **Validates: Requirements 1.6, 2.5**
// ---------------------------------------------------------------------------

TEST(StorablePropertyTest, OwnIsAlwaysOwning) {
    // Feature: own-link-memory-model, Property 3: Hold[T] and View[T] Storable trait consistency
    rc::check("Hold[T].is_owning() == true for any T and any integer id",
        []() {
            auto id = *rc::gen::inRange(-1000, 1000);

            auto ptr = std::make_shared<TypeA>(id);
            Own<TypeA> own(ptr);

            RC_ASSERT(own.is_owning() == true);
            RC_ASSERT(own.access().has_value());
        }
    );
}

TEST(StorablePropertyTest, LinkIsNeverOwning) {
    // Feature: own-link-memory-model, Property 3: Hold[T] and View[T] Storable trait consistency
    rc::check("View[T].is_owning() == false for any T and any integer id",
        []() {
            auto id = *rc::gen::inRange(-1000, 1000);

            auto ptr = std::make_shared<TypeA>(id);
            Own<TypeA> own(ptr);
            Link<TypeA> lnk = own.link();

            RC_ASSERT(lnk.is_owning() == false);
        }
    );
}

TEST(StorablePropertyTest, OwnIsOwningWithStringType) {
    // Feature: own-link-memory-model, Property 3: Hold[T] and View[T] Storable trait consistency
    rc::check("Hold[TypeB].is_owning() == true for any string label",
        []() {
            auto label = *rc::gen::string<std::string>();

            auto ptr = std::make_shared<TypeB>(label);
            Own<TypeB> own(ptr);

            RC_ASSERT(own.is_owning() == true);
            RC_ASSERT(own.access().has_value());
        }
    );
}

TEST(StorablePropertyTest, LinkIsNeverOwningWithStringType) {
    // Feature: own-link-memory-model, Property 3: Hold[T] and View[T] Storable trait consistency
    rc::check("View[TypeB].is_owning() == false for any string label",
        []() {
            auto label = *rc::gen::string<std::string>();

            auto ptr = std::make_shared<TypeB>(label);
            Own<TypeB> own(ptr);
            Link<TypeB> lnk = own.link();

            RC_ASSERT(lnk.is_owning() == false);
        }
    );
}

TEST(StorablePropertyTest, StorableConsistencyAcrossTypes) {
    // Feature: own-link-memory-model, Property 3: Hold[T] and View[T] Storable trait consistency
    rc::check("For any random type selection, Own is owning and Link is not",
        []() {
            // Randomly select which type to test (0=TypeA, 1=TypeB, 2=TypeC)
            auto type_selector = *rc::gen::inRange(0, 3);

            // We test the Storable interface through base pointer
            std::unique_ptr<Storable> own_storable;
            std::unique_ptr<Storable> link_storable;

            if (type_selector == 0) {
                auto id = *rc::gen::inRange(-100, 100);
                auto ptr = std::make_shared<TypeA>(id);
                own_storable = std::make_unique<Own<TypeA>>(ptr);
                auto own_tmp = Own<TypeA>(ptr);
                link_storable = std::make_unique<Link<TypeA>>(own_tmp.link());
            } else if (type_selector == 1) {
                auto label = *rc::gen::string<std::string>();
                auto ptr = std::make_shared<TypeB>(label);
                own_storable = std::make_unique<Own<TypeB>>(ptr);
                auto own_tmp = Own<TypeB>(ptr);
                link_storable = std::make_unique<Link<TypeB>>(own_tmp.link());
            } else {
                auto ptr = std::make_shared<TypeC>();
                own_storable = std::make_unique<Own<TypeC>>(ptr);
                auto own_tmp = Own<TypeC>(ptr);
                link_storable = std::make_unique<Link<TypeC>>(own_tmp.link());
            }

            // The core property: Own is owning, Link is not
            RC_ASSERT(own_storable->is_owning() == true);
            RC_ASSERT(link_storable->is_owning() == false);

            // Both must provide valid access while the object is alive
            RC_ASSERT(own_storable->access().has_value());
            RC_ASSERT(link_storable->access().has_value());
        }
    );
}
