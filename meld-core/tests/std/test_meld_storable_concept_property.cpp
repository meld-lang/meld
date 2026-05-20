/// Property-based tests for C++20 MeldStorable concept enforcement (Property 17).
///
/// Feature: own-link-memory-model, Property 17: C++20 MeldStorable concept enforcement
/// **Validates: Requirements 9.3**
///
/// For any C++ type that does not provide get_ref_count() -> uint64_t and
/// is_owning() -> bool, using that type as an element of Vector<T> constrained
/// by MeldStorable shall produce a C++ compilation error. Conversely, Hold[T]
/// and View[T] always satisfy the concept and can be used with Vector.

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "meld/std/meld_storable_concept.hpp"
#include "meld/std/mem.hpp"
#include "meld/types/memory.hpp"
#include <memory>
#include <string>

using namespace meld::std_mem;
using namespace meld::types;

// ---------------------------------------------------------------------------
// Test types — ManagedObject-derived types for property testing
// ---------------------------------------------------------------------------

class PropTypeAlpha : public ManagedObject {
public:
    explicit PropTypeAlpha(int val) : val_(val) {}
    int val() const { return val_; }
private:
    int val_;
};

class PropTypeBeta : public ManagedObject {
public:
    explicit PropTypeBeta(std::string s) : s_(std::move(s)) {}
    const std::string& s() const { return s_; }
private:
    std::string s_;
};

class PropTypeGamma : public ManagedObject {
public:
    PropTypeGamma() = default;
};

// ---------------------------------------------------------------------------
// Compile-time assertions: raw types never satisfy MeldStorable
// These static_asserts verify Property 17 for a broad set of raw types.
// ---------------------------------------------------------------------------

static_assert(!MeldStorable<int>, "int must not satisfy MeldStorable");
static_assert(!MeldStorable<unsigned int>, "unsigned int must not satisfy MeldStorable");
static_assert(!MeldStorable<long>, "long must not satisfy MeldStorable");
static_assert(!MeldStorable<double>, "double must not satisfy MeldStorable");
static_assert(!MeldStorable<float>, "float must not satisfy MeldStorable");
static_assert(!MeldStorable<char>, "char must not satisfy MeldStorable");
static_assert(!MeldStorable<bool>, "bool must not satisfy MeldStorable");
static_assert(!MeldStorable<std::string>, "std::string must not satisfy MeldStorable");
static_assert(!MeldStorable<void*>, "void* must not satisfy MeldStorable");
static_assert(!MeldStorable<int*>, "int* must not satisfy MeldStorable");
static_assert(!MeldStorable<ManagedObject>, "Raw ManagedObject must not satisfy MeldStorable");
static_assert(!MeldStorable<PropTypeAlpha>, "Raw PropTypeAlpha must not satisfy MeldStorable");
static_assert(!MeldStorable<PropTypeBeta>, "Raw PropTypeBeta must not satisfy MeldStorable");
static_assert(!MeldStorable<PropTypeGamma>, "Raw PropTypeGamma must not satisfy MeldStorable");

// Compile-time assertions: Hold[T] and View[T] always satisfy MeldStorable
static_assert(MeldStorable<Own<PropTypeAlpha>>, "Own<PropTypeAlpha> must satisfy MeldStorable");
static_assert(MeldStorable<Own<PropTypeBeta>>, "Own<PropTypeBeta> must satisfy MeldStorable");
static_assert(MeldStorable<Own<PropTypeGamma>>, "Own<PropTypeGamma> must satisfy MeldStorable");
static_assert(MeldStorable<Link<PropTypeAlpha>>, "Link<PropTypeAlpha> must satisfy MeldStorable");
static_assert(MeldStorable<Link<PropTypeBeta>>, "Link<PropTypeBeta> must satisfy MeldStorable");
static_assert(MeldStorable<Link<PropTypeGamma>>, "Link<PropTypeGamma> must satisfy MeldStorable");

// ---------------------------------------------------------------------------
// Property 17: C++20 MeldStorable concept enforcement
// **Validates: Requirements 9.3**
//
// For randomly generated Hold[T] and View[T] instances, verify that:
//   1. They satisfy MeldStorable (get_ref_count() returns uint64_t, is_owning() returns bool)
//   2. They can be stored in Vector<T>
//   3. The concept methods return consistent values
// ---------------------------------------------------------------------------

TEST(MeldStorableConceptPropertyTest, OwnSatisfiesConceptForRandomTypes) {
    // Feature: own-link-memory-model, Property 17: C++20 MeldStorable concept enforcement
    rc::check("Hold[T] satisfies MeldStorable for any random integer-parameterized type",
        []() {
            auto val = *rc::gen::inRange(-10000, 10000);
            auto ptr = std::make_shared<PropTypeAlpha>(val);
            Own<PropTypeAlpha> own(ptr);

            // MeldStorable requires get_ref_count() -> uint64_t
            uint64_t count = own.get_ref_count();
            RC_ASSERT(count >= 1);

            // MeldStorable requires is_owning() -> bool
            bool owning = own.is_owning();
            RC_ASSERT(owning == true);

            // Can be stored in Vector
            Vector<Own<PropTypeAlpha>> vec;
            vec.push(Own<PropTypeAlpha>(ptr));
            RC_ASSERT(vec.size() == 1);
        }
    );
}

TEST(MeldStorableConceptPropertyTest, OwnSatisfiesConceptForStringTypes) {
    // Feature: own-link-memory-model, Property 17: C++20 MeldStorable concept enforcement
    rc::check("Hold[T] satisfies MeldStorable for any random string-parameterized type",
        []() {
            auto label = *rc::gen::string<std::string>();
            auto ptr = std::make_shared<PropTypeBeta>(label);
            Own<PropTypeBeta> own(ptr);

            uint64_t count = own.get_ref_count();
            RC_ASSERT(count >= 1);
            RC_ASSERT(own.is_owning() == true);

            Vector<Own<PropTypeBeta>> vec;
            vec.push(Own<PropTypeBeta>(ptr));
            RC_ASSERT(vec.size() == 1);
        }
    );
}

TEST(MeldStorableConceptPropertyTest, LinkSatisfiesConceptForRandomTypes) {
    // Feature: own-link-memory-model, Property 17: C++20 MeldStorable concept enforcement
    rc::check("View[T] satisfies MeldStorable for any random integer-parameterized type",
        []() {
            auto val = *rc::gen::inRange(-10000, 10000);
            auto ptr = std::make_shared<PropTypeAlpha>(val);
            Own<PropTypeAlpha> own(ptr);
            Link<PropTypeAlpha> lnk = own.link();

            // MeldStorable requires get_ref_count() -> uint64_t
            uint64_t count = lnk.get_ref_count();
            RC_ASSERT(count >= 1);

            // MeldStorable requires is_owning() -> bool
            bool owning = lnk.is_owning();
            RC_ASSERT(owning == false);

            // Can be stored in Vector
            Vector<Link<PropTypeAlpha>> vec;
            vec.push(own.link());
            RC_ASSERT(vec.size() == 1);
        }
    );
}

TEST(MeldStorableConceptPropertyTest, ConceptEnforcementAcrossRandomTypeSelection) {
    // Feature: own-link-memory-model, Property 17: C++20 MeldStorable concept enforcement
    rc::check("MeldStorable concept correctly distinguishes managed vs raw types across random selections",
        []() {
            // Randomly select a type variant (0=Alpha, 1=Beta, 2=Gamma)
            auto type_sel = *rc::gen::inRange(0, 3);
            // Randomly select Own vs Link (0=Own, 1=Link)
            auto wrapper_sel = *rc::gen::inRange(0, 2);

            if (type_sel == 0) {
                auto val = *rc::gen::inRange(-1000, 1000);
                auto ptr = std::make_shared<PropTypeAlpha>(val);
                if (wrapper_sel == 0) {
                    Own<PropTypeAlpha> own(ptr);
                    RC_ASSERT(own.is_owning() == true);
                    RC_ASSERT(own.get_ref_count() >= 1);
                } else {
                    Own<PropTypeAlpha> own(ptr);
                    Link<PropTypeAlpha> lnk = own.link();
                    RC_ASSERT(lnk.is_owning() == false);
                    RC_ASSERT(lnk.get_ref_count() >= 1);
                }
            } else if (type_sel == 1) {
                auto label = *rc::gen::string<std::string>();
                auto ptr = std::make_shared<PropTypeBeta>(label);
                if (wrapper_sel == 0) {
                    Own<PropTypeBeta> own(ptr);
                    RC_ASSERT(own.is_owning() == true);
                    RC_ASSERT(own.get_ref_count() >= 1);
                } else {
                    Own<PropTypeBeta> own(ptr);
                    Link<PropTypeBeta> lnk = own.link();
                    RC_ASSERT(lnk.is_owning() == false);
                    RC_ASSERT(lnk.get_ref_count() >= 1);
                }
            } else {
                auto ptr = std::make_shared<PropTypeGamma>();
                if (wrapper_sel == 0) {
                    Own<PropTypeGamma> own(ptr);
                    RC_ASSERT(own.is_owning() == true);
                    RC_ASSERT(own.get_ref_count() >= 1);
                } else {
                    Own<PropTypeGamma> own(ptr);
                    Link<PropTypeGamma> lnk = own.link();
                    RC_ASSERT(lnk.is_owning() == false);
                    RC_ASSERT(lnk.get_ref_count() >= 1);
                }
            }
        }
    );
}

TEST(MeldStorableConceptPropertyTest, VectorStoresRandomCountOfElements) {
    // Feature: own-link-memory-model, Property 17: C++20 MeldStorable concept enforcement
    rc::check("Vector<Own<T>> stores any random number of elements correctly",
        []() {
            auto n = *rc::gen::inRange(0, 50);

            Vector<Own<PropTypeAlpha>> vec;
            for (int i = 0; i < n; ++i) {
                vec.push(Own<PropTypeAlpha>(std::make_shared<PropTypeAlpha>(i)));
            }

            RC_ASSERT(vec.size() == static_cast<size_t>(n));

            // All elements satisfy MeldStorable
            size_t count = 0;
            for (auto& elem : vec) {
                RC_ASSERT(elem.is_owning() == true);
                RC_ASSERT(elem.get_ref_count() >= 1);
                count++;
            }
            RC_ASSERT(count == static_cast<size_t>(n));
        }
    );
}
