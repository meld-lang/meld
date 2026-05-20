/**
 * **Feature: meld-daemon, Property 53: Refinement Type Evaluation**
 *
 * For any refinement type, logical predicates and constraint expressions
 * SHALL be evaluated correctly.
 *
 * **Validates: Requirements 21.3**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "meld/daemon/meld_feature_provider.hpp"

#include <string>
#include <vector>

namespace meld::daemon {
namespace {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void add_refinement_type(SemanticModel& model,
                         const std::filesystem::path& file,
                         const std::string& type_name,
                         const std::string& base_type,
                         const std::string& predicate) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    root->name = file.stem().string();
    root->location = SourceLocation{file, 1, 0};

    auto type_node = std::make_shared<ASTNode>();
    type_node->kind = "refinement_type";
    type_node->name = type_name;
    type_node->type_info = base_type;
    type_node->location = SourceLocation{file, 3, 0};
    if (!predicate.empty()) {
        type_node->effects.push_back(predicate);
    }

    root->children.push_back(type_node);

    FileSemantics sem;
    sem.path = file;
    sem.ast = root;
    model.update_file(file, std::move(sem));
}

// ---------------------------------------------------------------------------
// Generators
// ---------------------------------------------------------------------------

rc::Gen<std::string> genValidPredicate() {
    return rc::gen::elementOf(std::vector<std::string>{
        "x > 0", "x >= 0 && x < 100", "len != 0",
        "value <= max", "a == b", "!empty"});
}

rc::Gen<std::string> genInvalidPredicate() {
    return rc::gen::elementOf(std::vector<std::string>{
        "", "hello", "42", "just_a_name"});
}

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

/**
 * Property 53a: Valid predicates are recognized as well-formed.
 */
RC_GTEST_PROP(RefinementTypeEvaluation,
              ValidPredicatesRecognized,
              ()) {
    auto predicate = *genValidPredicate();

    SemanticModel model;
    std::filesystem::path file = "types.meld";
    add_refinement_type(model, file, "PositiveInt", "Int", predicate);

    MeldFeatureProvider provider(model);
    auto result = provider.evaluate_refinement(file, "PositiveInt");

    RC_ASSERT(result.predicate_valid == true);
    RC_ASSERT(result.constraint_satisfied == true);
    RC_ASSERT(result.predicate == predicate);
    RC_ASSERT(result.base_type == "Int");
}

/**
 * Property 53b: Invalid predicates are flagged with a violation message.
 */
RC_GTEST_PROP(RefinementTypeEvaluation,
              InvalidPredicatesFlagged,
              ()) {
    auto predicate = *genInvalidPredicate();

    SemanticModel model;
    std::filesystem::path file = "types.meld";
    add_refinement_type(model, file, "BadType", "Int", predicate);

    MeldFeatureProvider provider(model);
    auto result = provider.evaluate_refinement(file, "BadType");

    RC_ASSERT(result.predicate_valid == false);
    RC_ASSERT(!result.violation_message.empty());
}

/**
 * Property 53c: A type with no predicate is not a valid refinement.
 */
RC_GTEST_PROP(RefinementTypeEvaluation,
              NoPredMeansNoRefinement,
              ()) {
    SemanticModel model;
    std::filesystem::path file = "types.meld";
    add_refinement_type(model, file, "PlainInt", "Int", "");

    MeldFeatureProvider provider(model);
    auto result = provider.evaluate_refinement(file, "PlainInt");

    RC_ASSERT(result.predicate_valid == false);
    RC_ASSERT(!result.violation_message.empty());
}

}  // namespace
}  // namespace meld::daemon
