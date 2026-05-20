/**
 * **Feature: meld-daemon, Property 34: Refinement Constraint Validation**
 *
 * For any refinement type constraint violation, the LspChannel SHALL validate
 * logical predicates and report constraint violations.
 *
 * **Validates: Requirements 17.4**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "meld/daemon/diagnostics_provider.hpp"

#include <string>
#include <vector>

namespace meld::daemon {
namespace {

// ---------------------------------------------------------------------------
// Generators
// ---------------------------------------------------------------------------

/// Generate a positive integer value (satisfies "x > 0").
rc::Gen<int> genPositiveInt() {
    return rc::gen::inRange(1, 1000);
}

/// Generate a non-positive integer value (violates "x > 0").
rc::Gen<int> genNonPositiveInt() {
    return rc::gen::inRange(-1000, 1);
}

/// Generate a comparison operator.
rc::Gen<std::string> genOperator() {
    return rc::gen::elementOf(
        std::vector<std::string>{">", "<", ">=", "<=", "==", "!="});
}

/// Build a model with a refinement-typed variable.
void populate_with_refinement(SemanticModel& model,
                               const std::filesystem::path& file,
                               const std::string& predicate,
                               const std::string& value) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";

    auto decl = std::make_shared<ASTNode>();
    decl->kind = "val_declaration";
    decl->name = "x";
    decl->type_info = "Int{" + predicate + "}";
    decl->location = {file, 3, 0};

    auto init = std::make_shared<ASTNode>();
    init->kind = "initializer";
    init->name = value;
    decl->children.push_back(init);

    root->children.push_back(decl);

    FileSemantics sem;
    sem.path = file;
    sem.ast = root;
    model.update_file(file, std::move(sem));
}

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

/**
 * Property 34a: satisfies_predicate correctly evaluates "x > 0" for
 * positive values (true) and non-positive values (false).
 */
RC_GTEST_PROP(RefinementConstraintProperty,
              PositivePredicateCorrectness,
              ()) {
    auto pos_val = *genPositiveInt();
    auto non_pos_val = *genNonPositiveInt();

    RC_ASSERT(DiagnosticsProvider::satisfies_predicate(
        "x > 0", std::to_string(pos_val)));
    RC_ASSERT(!DiagnosticsProvider::satisfies_predicate(
        "x > 0", std::to_string(non_pos_val)));
}

/**
 * Property 34b: A value violating a refinement constraint is detected.
 */
RC_GTEST_PROP(RefinementConstraintProperty,
              ViolatingValueDetected,
              ()) {
    auto bad_val = *genNonPositiveInt();

    SemanticModel model;
    std::filesystem::path file = "test.meld";
    populate_with_refinement(model, file, "x > 0", std::to_string(bad_val));

    DiagnosticsProvider provider(model);
    auto violations = provider.check_refinement_constraints(file);

    RC_ASSERT(!violations.empty());
    RC_ASSERT(violations[0].predicate == "x > 0");
    RC_ASSERT(violations[0].violating_value == std::to_string(bad_val));
}

/**
 * Property 34c: A value satisfying a refinement constraint produces no violations.
 */
RC_GTEST_PROP(RefinementConstraintProperty,
              SatisfyingValueNoViolation,
              ()) {
    auto good_val = *genPositiveInt();

    SemanticModel model;
    std::filesystem::path file = "test.meld";
    populate_with_refinement(model, file, "x > 0", std::to_string(good_val));

    DiagnosticsProvider provider(model);
    auto violations = provider.check_refinement_constraints(file);

    RC_ASSERT(violations.empty());
}

/**
 * Property 34d: satisfies_predicate handles all comparison operators consistently.
 */
RC_GTEST_PROP(RefinementConstraintProperty,
              AllOperatorsConsistent,
              ()) {
    auto threshold = *rc::gen::inRange(-100, 100);
    auto value = *rc::gen::inRange(-100, 100);

    std::string pred_gt = "x > " + std::to_string(threshold);
    std::string pred_lt = "x < " + std::to_string(threshold);
    std::string pred_eq = "x == " + std::to_string(threshold);

    bool gt_result = DiagnosticsProvider::satisfies_predicate(
        pred_gt, std::to_string(value));
    bool lt_result = DiagnosticsProvider::satisfies_predicate(
        pred_lt, std::to_string(value));
    bool eq_result = DiagnosticsProvider::satisfies_predicate(
        pred_eq, std::to_string(value));

    RC_ASSERT(gt_result == (value > threshold));
    RC_ASSERT(lt_result == (value < threshold));
    RC_ASSERT(eq_result == (value == threshold));
}

}  // namespace
}  // namespace meld::daemon
