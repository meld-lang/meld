/// Property-based tests for collection iterator behavior (Task 12.3, 12.4).
///
/// Feature: own-link-memory-model, Property 10: vec[Hold[T]] solid iteration
/// Feature: own-link-memory-model, Property 11: vec[View[T]] filtering iteration
///
/// Uses rapidcheck for property-based testing with Google Test integration.

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "meld/std/collection_iterators.hpp"
#include "meld/std/mem.hpp"
#include "meld/types/memory.hpp"

#include <algorithm>
#include <memory>
#include <set>
#include <vector>

using namespace meld::std_mem;
using namespace meld::types;

// ---------------------------------------------------------------------------
// Test type
// ---------------------------------------------------------------------------

class PropIterNode : public ManagedObject {
public:
    explicit PropIterNode(int val) : val_(val) {}
    int val() const { return val_; }
private:
    int val_;
};

// ===========================================================================
// Property 10: vec[Hold[T]] solid iteration
// **Validates: Requirements 6.1**
//
// For any vec[Hold[T]] containing n elements, the default iterator shall
// yield exactly n elements, each of which is non-null and alive
// (strong_count > 0).
// ===========================================================================

TEST(CollectionIteratorPropertyTest, HoldVectorSolidIteration) {
    // Feature: own-link-memory-model, Property 10: vec[Hold[T]] solid iteration
    // **Validates: Requirements 6.1**
    rc::check("vec[Hold[T]] solid iterator yields exactly n non-null alive elements",
        []() {
            auto n = *rc::gen::inRange(0, 100);

            HoldVector<PropIterNode> vec;
            for (int i = 0; i < n; ++i) {
                auto val = *rc::gen::inRange(-10000, 10000);
                vec.push(Own<PropIterNode>(std::make_shared<PropIterNode>(val)));
            }

            RC_ASSERT(vec.size() == static_cast<size_t>(n));

            // Iterate and count — every element must be yielded
            size_t count = 0;
            for (auto& elem : vec) {
                // Each element is non-null and alive (T& is valid)
                // Accessing .val() would crash if null — this IS the check
                [[maybe_unused]] int v = elem.val();
                count++;
            }

            RC_ASSERT(count == static_cast<size_t>(n));
        }
    );
}

TEST(CollectionIteratorPropertyTest, HoldVectorSolidIterationPreservesOrder) {
    // Feature: own-link-memory-model, Property 10: vec[Hold[T]] solid iteration
    // **Validates: Requirements 6.1**
    rc::check("vec[Hold[T]] solid iterator preserves insertion order",
        []() {
            auto n = *rc::gen::inRange(1, 50);

            std::vector<int> expected;
            HoldVector<PropIterNode> vec;
            for (int i = 0; i < n; ++i) {
                auto val = *rc::gen::inRange(-10000, 10000);
                expected.push_back(val);
                vec.push(Own<PropIterNode>(std::make_shared<PropIterNode>(val)));
            }

            size_t idx = 0;
            for (auto& elem : vec) {
                RC_ASSERT(elem.val() == expected[idx]);
                idx++;
            }
            RC_ASSERT(idx == static_cast<size_t>(n));
        }
    );
}

// ===========================================================================
// Property 11: vec[View[T]] filtering iteration
// **Validates: Requirements 6.2, 6.3, 6.4, 6.5**
//
// For any vec[View[T]] containing n elements where k of the referenced
// objects have been deallocated, the default iterator shall yield exactly
// n - k elements, all alive. The .entries() iterator shall yield exactly
// n elements, with dead entries represented as none.
// ===========================================================================

TEST(CollectionIteratorPropertyTest, ViewVectorFilteringIteration) {
    // Feature: own-link-memory-model, Property 11: vec[View[T]] filtering iteration
    // **Validates: Requirements 6.2, 6.3, 6.4, 6.5**
    rc::check("vec[View[T]] default iterator yields n-k live elements, .entries() yields n",
        []() {
            auto n = *rc::gen::inRange(0, 50);

            // Create n objects, keep shared_ptrs alive
            std::vector<std::shared_ptr<PropIterNode>> owners;
            ViewVector<PropIterNode> vec;

            for (int i = 0; i < n; ++i) {
                auto ptr = std::make_shared<PropIterNode>(i);
                owners.push_back(ptr);
                vec.push(Link<PropIterNode>(ptr));
            }

            // Randomly select k elements to deallocate
            std::set<int> kill_set;
            for (int i = 0; i < n; ++i) {
                bool should_kill = *rc::gen::arbitrary<bool>();
                if (should_kill) {
                    kill_set.insert(i);
                }
            }

            // Deallocate selected elements
            for (int idx : kill_set) {
                owners[idx].reset();
            }

            size_t k = kill_set.size();
            size_t expected_live = static_cast<size_t>(n) - k;

            // Default iterator: yields exactly n - k live elements
            size_t live_count = 0;
            for (auto& elem : vec) {
                // Every yielded element must be alive and accessible
                [[maybe_unused]] int v = elem.val();
                live_count++;
            }
            RC_ASSERT(live_count == expected_live);

            // .entries() iterator: yields exactly n elements total
            size_t entries_total = 0;
            size_t entries_live = 0;
            size_t entries_dead = 0;

            for (auto entry : vec.entries()) {
                entries_total++;
                if (entry.has_value()) {
                    // Live entry — must be accessible
                    [[maybe_unused]] int v = entry->get().val();
                    entries_live++;
                } else {
                    entries_dead++;
                }
            }

            RC_ASSERT(entries_total == static_cast<size_t>(n));
            RC_ASSERT(entries_live == expected_live);
            RC_ASSERT(entries_dead == k);
        }
    );
}

TEST(CollectionIteratorPropertyTest, ViewVectorFilteringSkipsDeadWithoutError) {
    // Feature: own-link-memory-model, Property 11: vec[View[T]] filtering iteration
    // **Validates: Requirements 6.2, 6.4**
    rc::check("vec[View[T]] default iterator skips dead entries without error",
        []() {
            auto n = *rc::gen::inRange(1, 30);

            std::vector<std::shared_ptr<PropIterNode>> owners;
            ViewVector<PropIterNode> vec;

            for (int i = 0; i < n; ++i) {
                auto ptr = std::make_shared<PropIterNode>(i);
                owners.push_back(ptr);
                vec.push(Link<PropIterNode>(ptr));
            }

            // Kill all elements — iterator must yield 0 without error
            for (auto& o : owners) {
                o.reset();
            }

            size_t count = 0;
            for ([[maybe_unused]] auto& elem : vec) {
                count++;
            }
            RC_ASSERT(count == 0u);

            // .entries() must still yield n entries, all nullopt
            size_t entries_count = 0;
            for (auto entry : vec.entries()) {
                RC_ASSERT(!entry.has_value());
                entries_count++;
            }
            RC_ASSERT(entries_count == static_cast<size_t>(n));
        }
    );
}
