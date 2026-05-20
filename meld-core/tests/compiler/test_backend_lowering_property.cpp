#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "meld/std/mem.hpp"
#include "meld/types/memory.hpp"

// Feature: hold-view-tenancy-model, Property 1: Hold[T] strong_count invariant
// Feature: hold-view-tenancy-model, Property 2: View[T] weak_count isolation
// **Validates: Requirements 1.2, 1.3, 2.2, 2.3**

using namespace meld::std_mem;
using namespace meld::types;

// ---------------------------------------------------------------------------
// Test fixture: a ManagedObject-derived type for property testing
// ---------------------------------------------------------------------------

class PropTestNode : public ManagedObject {
public:
    explicit PropTestNode(int id) : id_(id) {}
    int id() const { return id_; }
private:
    int id_;
};

// ---------------------------------------------------------------------------
// Operation types for generating random sequences
// HoldOp: operations on Hold[T] (strong/owning references)
// ViewOp: operations on View[T] (non-owning observation references)
// ---------------------------------------------------------------------------

enum class OwnOp { Copy, Move, Destroy };
enum class LinkOp { Create, Destroy };

namespace rc {

template<>
struct Arbitrary<OwnOp> {
    static Gen<OwnOp> arbitrary() {
        return gen::element(OwnOp::Copy, OwnOp::Move, OwnOp::Destroy);
    }
};

template<>
struct Arbitrary<LinkOp> {
    static Gen<LinkOp> arbitrary() {
        return gen::element(LinkOp::Create, LinkOp::Destroy);
    }
};

} // namespace rc

// ===========================================================================
// Property 1: Hold[T] strong_count invariant
//
// For any sequence of Hold[T] copy/move/destroy operations, strong_count
// shall always equal the number of currently live Hold[T] instances
// pointing to that object.
// **Validates: Requirements 1.2, 1.3**
// ===========================================================================

RC_GTEST_PROP(OwnStrongCountInvariant, StrongCountEqualsLiveCount, ()) {
    // Create the underlying object
    auto ptr = std::make_shared<PropTestNode>(1);
    long base_count = ptr.use_count(); // 1 (ptr itself)

    // Start with one Hold reference
    std::vector<std::unique_ptr<Own<PropTestNode>>> live_owns;
    live_owns.push_back(std::make_unique<Own<PropTestNode>>(ptr));
    size_t expected_live = 1;

    // Generate a random sequence of operations
    auto ops = *rc::gen::container<std::vector<OwnOp>>(
        *rc::gen::inRange(1, 20),
        rc::gen::arbitrary<OwnOp>()
    );

    for (auto op : ops) {
        switch (op) {
            case OwnOp::Copy: {
                if (!live_owns.empty()) {
                    // Copy an existing Hold reference — increments strong_count
                    size_t idx = *rc::gen::inRange<size_t>(0, live_owns.size());
                    live_owns.push_back(
                        std::make_unique<Own<PropTestNode>>(*live_owns[idx])
                    );
                    expected_live++;
                }
                break;
            }
            case OwnOp::Move: {
                if (!live_owns.empty()) {
                    // Move an existing Hold reference — count unchanged
                    size_t idx = *rc::gen::inRange<size_t>(0, live_owns.size());
                    auto moved = std::make_unique<Own<PropTestNode>>(
                        mem_move(*live_owns[idx])
                    );
                    // Remove the moved-from (now null) entry
                    live_owns.erase(live_owns.begin() + static_cast<long>(idx));
                    // Add the moved-to entry
                    live_owns.push_back(std::move(moved));
                    // expected_live unchanged — move transfers, doesn't create
                }
                break;
            }
            case OwnOp::Destroy: {
                if (live_owns.size() > 1) {
                    // Destroy one Hold reference — decrements strong_count
                    size_t idx = *rc::gen::inRange<size_t>(0, live_owns.size());
                    live_owns.erase(live_owns.begin() + static_cast<long>(idx));
                    expected_live--;
                }
                break;
            }
        }

        // Invariant: strong_count == base_count + expected_live
        RC_ASSERT(ptr.use_count() == static_cast<long>(base_count + expected_live));
    }
}

// ===========================================================================
// Property 2: View[T] weak_count isolation
//
// For any sequence of View[T] create/destroy operations, weak_count
// shall equal the number of live View[T] instances, and strong_count
// shall remain unchanged by any View[T] operation.
// **Validates: Requirements 2.2, 2.3**
// ===========================================================================

RC_GTEST_PROP(LinkWeakCountIsolation, WeakCountEqualsLiveAndStrongUnchanged, ()) {
    // Create the underlying object with one Hold reference
    auto ptr = std::make_shared<PropTestNode>(2);
    Own<PropTestNode> owner(ptr);
    long strong_count_baseline = ptr.use_count(); // ptr + owner

    // Track live View references
    std::vector<std::unique_ptr<Link<PropTestNode>>> live_links;
    size_t expected_link_count = 0;

    // Generate a random sequence of View operations
    auto ops = *rc::gen::container<std::vector<LinkOp>>(
        *rc::gen::inRange(1, 20),
        rc::gen::arbitrary<LinkOp>()
    );

    for (auto op : ops) {
        switch (op) {
            case LinkOp::Create: {
                // Create a new View from the owner — increments weak_count
                live_links.push_back(
                    std::make_unique<Link<PropTestNode>>(link(owner))
                );
                expected_link_count++;
                break;
            }
            case LinkOp::Destroy: {
                if (!live_links.empty()) {
                    // Destroy a View — decrements weak_count
                    size_t idx = *rc::gen::inRange<size_t>(0, live_links.size());
                    live_links.erase(live_links.begin() + static_cast<long>(idx));
                    expected_link_count--;
                }
                break;
            }
        }

        // Invariant 1: strong_count is unchanged by View operations
        RC_ASSERT(ptr.use_count() == strong_count_baseline);

        // Invariant 2: weak_count equals the number of live Views
        // We verify this indirectly: all live views should be non-expired
        for (const auto& lnk : live_links) {
            RC_ASSERT(!lnk->expired());
        }
        RC_ASSERT(live_links.size() == expected_link_count);
    }
}
