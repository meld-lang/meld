/// @file test_effect_handler_arc_property.cpp
/// @brief Property-based tests for effect handler ARC interaction
///
/// Property 14: Effect handler Hold[T] capture round trip
///   **Validates: Requirements 8.1, 8.2**
///
/// Property 15: Effect handler View[T] upgrade enforcement
///   **Validates: Requirements 8.3**
///
/// Property 16: Multi-shot continuation clone ARC correctness
///   **Validates: Requirements 8.4, 8.5**

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "meld/effects/effect_handler_arc.hpp"
#include "meld/compiler/view_access_pass.hpp"
#include "meld/std/mem.hpp"
#include "meld/types/memory.hpp"
#include <memory>
#include <string>
#include <vector>

using namespace meld::effects;
using namespace meld::std_mem;
using namespace meld::types;
using namespace meld::compiler;

// ---------------------------------------------------------------------------
// Test types
// ---------------------------------------------------------------------------

class PropArcNode : public ManagedObject {
public:
    explicit PropArcNode(int val) : val_(val) {}
    int val() const { return val_; }
private:
    int val_;
};

class PropArcWidget : public ManagedObject {
public:
    explicit PropArcWidget(std::string name) : name_(std::move(name)) {}
    const std::string& name() const { return name_; }
private:
    std::string name_;
};

// ===========================================================================
// Property 14: Effect handler Hold[T] capture round trip
// Feature: hold-view-tenancy-model, Property 14: Effect handler Hold[T] capture round trip
// **Validates: Requirements 8.1, 8.2**
//
// For any effect handler that captures Hold[T] references from its enclosing
// scope, installing the handler shall increment strong_count for each
// captured reference, and exiting the handler scope shall decrement
// strong_count back to its pre-installation value.
// ===========================================================================

TEST(EffectHandlerArcPropertyTest, OwnCaptureRoundTrip) {
    // Feature: hold-view-tenancy-model, Property 14: Effect handler Hold[T] capture round trip
    rc::check("Hold[T] capture: strong_count increments on install and decrements on exit",
        []() {
            // Generate a random number of Hold[T] references to capture (1..10)
            auto num_captures = *rc::gen::inRange(1, 11);

            // Create the shared_ptrs and Hold[T] wrappers
            std::vector<std::shared_ptr<PropArcNode>> ptrs;
            std::vector<Own<PropArcNode>> owns;
            ptrs.reserve(num_captures);
            owns.reserve(num_captures);

            for (int i = 0; i < num_captures; ++i) {
                auto val = *rc::gen::inRange(-1000, 1000);
                auto ptr = std::make_shared<PropArcNode>(val);
                ptrs.push_back(ptr);
                owns.emplace_back(ptr);
            }

            // Record baseline strong_counts
            std::vector<long> baselines;
            baselines.reserve(num_captures);
            for (int i = 0; i < num_captures; ++i) {
                baselines.push_back(ptrs[i].use_count());
            }

            {
                EffectHandlerFrame frame;
                for (int i = 0; i < num_captures; ++i) {
                    frame.capture_own(owns[i], "cap_" + std::to_string(i));
                }
                EffectHandlerArcScope scope(frame);

                // Req 8.1: strong_count incremented by 1 for each capture
                for (int i = 0; i < num_captures; ++i) {
                    RC_ASSERT(ptrs[i].use_count() == baselines[i] + 1);
                }
            }

            // Req 8.2: strong_count back to baseline after scope exit
            for (int i = 0; i < num_captures; ++i) {
                RC_ASSERT(ptrs[i].use_count() == baselines[i]);
            }
        }
    );
}

TEST(EffectHandlerArcPropertyTest, OwnCaptureRoundTripWithStringType) {
    // Feature: hold-view-tenancy-model, Property 14: Effect handler Hold[T] capture round trip
    rc::check("Hold[T] capture round trip holds for string-parameterized types",
        []() {
            auto label = *rc::gen::string<std::string>();
            auto ptr = std::make_shared<PropArcWidget>(label);
            Own<PropArcWidget> own(ptr);

            long baseline = ptr.use_count();

            {
                EffectHandlerFrame frame;
                frame.capture_own(own, "widget");
                EffectHandlerArcScope scope(frame);

                RC_ASSERT(ptr.use_count() == baseline + 1);
            }
            RC_ASSERT(ptr.use_count() == baseline);
        }
    );
}

// ===========================================================================
// Property 15: Effect handler View[T] upgrade enforcement
// Feature: hold-view-tenancy-model, Property 15: Effect handler View[T] upgrade enforcement
// **Validates: Requirements 8.3**
//
// For any effect handler that captures a View[T] reference from its
// enclosing scope, direct access to the captured View[T] inside the
// handler body without upgrading via `if val` or `match` shall be
// rejected by the Semantic Analyzer (E4001).
//
// This is tested via the ViewAccessPass: we construct AST fragments
// that simulate a handler body accessing a View[T] binding directly,
// and verify E4001 is emitted.
// ===========================================================================

TEST(EffectHandlerArcPropertyTest, LinkUpgradeEnforcementInHandlerBody) {
    // Feature: hold-view-tenancy-model, Property 15: Effect handler View[T] upgrade enforcement
    rc::check("View[T] direct access inside handler body emits E4001",
        []() {
            // Generate random binding and field names
            auto binding_name = *rc::gen::nonEmpty(
                rc::gen::container<std::string>(rc::gen::inRange('a', 'z' + 1)));
            auto field_name = *rc::gen::nonEmpty(
                rc::gen::container<std::string>(rc::gen::inRange('a', 'z' + 1)));

            // Ensure names are different to avoid ambiguity
            RC_PRE(binding_name != field_name);

            // Simulate: a function parameter with View[T] type, and a
            // member access on that parameter without upgrade.
            // The ViewAccessPass should emit E4001.

            // Build a minimal function with a View[T] parameter and
            // direct member access: `fnc handler(obs: View[Node]) { obs.field }`
            parser::ast::function_definition func;
            func.name.name = "test_handler";

            // Parameter with Link type annotation
            parser::ast::parameter param;
            param.name.name = binding_name;
            param.type.type_name.name = "Link";
            func.parameters.push_back(param);

            // Body: a single member access expression `binding.field`
            parser::ast::binary_operation member_access;
            member_access.op = ".";

            parser::ast::identifier lhs_id;
            lhs_id.name = binding_name;
            member_access.left = parser::ast::expression(lhs_id);

            parser::ast::identifier rhs_id;
            rhs_id.name = field_name;
            member_access.right = parser::ast::expression(rhs_id);

            parser::ast::block_expression body;
            body.statements.push_back(
                boost::spirit::x3::forward_ast<parser::ast::expression>(
                    parser::ast::expression(
                        boost::spirit::x3::forward_ast<parser::ast::binary_operation>(
                            member_access))));
            func.body = body;

            // Wrap in module-level expressions
            std::vector<parser::ast::expression> exprs;
            exprs.push_back(parser::ast::expression(
                boost::spirit::x3::forward_ast<parser::ast::function_definition>(func)));

            IntrinsicResolutionRegistry registry;
            ViewAccessPass pass;
            auto result = pass.run(exprs, registry, "test_handler_body.meld");

            // E4001 must be emitted for direct View[T] access
            RC_ASSERT(!result.success);
            RC_ASSERT(result.direct_access_errors >= 1);

            bool found_e4001 = false;
            for (const auto& diag : result.diagnostics) {
                if (diag.code == "E4001") {
                    found_e4001 = true;
                    break;
                }
            }
            RC_ASSERT(found_e4001);
        }
    );
}

// ===========================================================================
// Property 16: Multi-shot continuation clone ARC correctness
// Feature: hold-view-tenancy-model, Property 16: Multi-shot continuation clone ARC correctness
// **Validates: Requirements 8.4, 8.5**
//
// For any multi-shot continuation that captures Hold[T] references,
// cloning the continuation shall increment strong_count for each
// captured reference by 1 per clone. Discarding a clone shall
// decrement strong_count for each captured reference by 1.
// ===========================================================================

TEST(EffectHandlerArcPropertyTest, MultiShotCloneArcCorrectness) {
    // Feature: hold-view-tenancy-model, Property 16: Multi-shot continuation clone ARC correctness
    rc::check("Cloning N times increments strong_count by N; discarding returns count",
        []() {
            // Random number of captures (1..5) and clones (1..10)
            auto num_captures = *rc::gen::inRange(1, 6);
            auto num_clones = *rc::gen::inRange(1, 11);

            std::vector<std::shared_ptr<PropArcNode>> ptrs;
            std::vector<Own<PropArcNode>> owns;
            ptrs.reserve(num_captures);
            owns.reserve(num_captures);

            for (int i = 0; i < num_captures; ++i) {
                auto val = *rc::gen::inRange(-500, 500);
                auto ptr = std::make_shared<PropArcNode>(val);
                ptrs.push_back(ptr);
                owns.emplace_back(ptr);
            }

            // Record baselines
            std::vector<long> baselines;
            baselines.reserve(num_captures);
            for (int i = 0; i < num_captures; ++i) {
                baselines.push_back(ptrs[i].use_count());
            }

            EffectHandlerFrame frame;
            for (int i = 0; i < num_captures; ++i) {
                frame.capture_own(owns[i], "cap_" + std::to_string(i));
            }
            frame.install();

            // After install: each count is baseline + 1
            for (int i = 0; i < num_captures; ++i) {
                RC_ASSERT(ptrs[i].use_count() == baselines[i] + 1);
            }

            // Clone N times
            std::vector<std::unique_ptr<EffectHandlerFrame>> clones;
            for (int c = 0; c < num_clones; ++c) {
                clones.push_back(frame.clone());

                // Req 8.4: each clone increments strong_count by 1
                for (int i = 0; i < num_captures; ++i) {
                    RC_ASSERT(ptrs[i].use_count() == baselines[i] + 1 + (c + 1));
                }
            }

            // All N clones alive: count = baseline + 1 + N
            for (int i = 0; i < num_captures; ++i) {
                RC_ASSERT(ptrs[i].use_count() == baselines[i] + 1 + num_clones);
            }

            // Discard clones one by one
            for (int c = num_clones - 1; c >= 0; --c) {
                clones.pop_back();

                // Req 8.5: discarding decrements strong_count by 1
                for (int i = 0; i < num_captures; ++i) {
                    RC_ASSERT(ptrs[i].use_count() == baselines[i] + 1 + c);
                }
            }

            // All clones discarded: count = baseline + 1 (frame still installed)
            for (int i = 0; i < num_captures; ++i) {
                RC_ASSERT(ptrs[i].use_count() == baselines[i] + 1);
            }

            frame.uninstall();

            // After uninstall: count = baseline
            for (int i = 0; i < num_captures; ++i) {
                RC_ASSERT(ptrs[i].use_count() == baselines[i]);
            }
        }
    );
}
