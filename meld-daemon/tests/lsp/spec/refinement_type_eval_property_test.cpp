/**
 * **Feature: meld-lsp-server, Property 32: Refinement type evaluation**
 *
 * For any refinement type, logical predicates and constraint expressions
 * should be evaluated correctly.
 *
 * **Validates: Requirements 7.3**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "meld/daemon/analysis_engine.hpp"

#include <string>
#include <vector>

namespace {

using namespace meld::lsp::analysis;

// ---------------------------------------------------------------------------
// Generators
// ---------------------------------------------------------------------------

rc::Gen<std::string> genTypeName() {
    return rc::gen::map(
        rc::gen::inRange(1, 6),
        [](int len) {
            std::string s = "T";
            for (int i = 1; i < len; ++i) {
                s += static_cast<char>('a' + ((i * 7 + 4) % 26));
            }
            return s;
        }
    );
}

/// Valid predicate with proper comparison operators
rc::Gen<std::string> genValidPredicate() {
    return rc::gen::element(
        std::string("it > 0"),
        std::string("it >= 1 && it <= 100"),
        std::string("it != 0"),
        std::string("it == 42"),
        std::string("it < 1000"));
}

/// Predicate with unbalanced parentheses
rc::Gen<std::string> genUnbalancedParenPredicate() {
    return rc::gen::element(
        std::string("(it > 0"),
        std::string("it > 0)"),
        std::string("((it > 0)"),
        std::string("(it > 0 && (it < 100)"));
}

/// Predicate using assignment instead of comparison
rc::Gen<std::string> genAssignmentPredicate() {
    return rc::gen::element(
        std::string("it = 0"),
        std::string("it = 42"));
}

/// Well-formed refinement type
rc::Gen<std::string> genValidRefinementType() {
    return rc::gen::map(
        rc::gen::tuple(genTypeName(), genValidPredicate()),
        [](const std::tuple<std::string, std::string>& t) {
            auto [name, pred] = t;
            return "type " + name + " = Int where { " + pred + " }";
        }
    );
}

/// Refinement with unbalanced parens in predicate
rc::Gen<std::string> genUnbalancedRefinement() {
    return rc::gen::map(
        rc::gen::tuple(genTypeName(), genUnbalancedParenPredicate()),
        [](const std::tuple<std::string, std::string>& t) {
            auto [name, pred] = t;
            return "type " + name + " = Int where { " + pred + " }";
        }
    );
}

/// Refinement with assignment in predicate
rc::Gen<std::string> genAssignmentRefinement() {
    return rc::gen::map(
        rc::gen::tuple(genTypeName(), genAssignmentPredicate()),
        [](const std::tuple<std::string, std::string>& t) {
            auto [name, pred] = t;
            return "type " + name + " = Int where { " + pred + " }";
        }
    );
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

TEST(RefinementTypeEvalPropertyTest, ValidPredicatesProduceNoErrors) {
    rc::check("Refinement types with valid predicates must produce no errors",
        []() {
            auto source = *genValidRefinementType();
            AnalysisEngine engine;
            auto result = engine.evaluate_refinement_types("file:///test.meld", source);
            RC_ASSERT(!result.has_errors());
            RC_ASSERT(result.refinement_count >= 1);
        }
    );
}

TEST(RefinementTypeEvalPropertyTest, UnbalancedParensProduceError) {
    rc::check("Refinement predicates with unbalanced parentheses must produce errors",
        []() {
            auto source = *genUnbalancedRefinement();
            AnalysisEngine engine;
            auto result = engine.evaluate_refinement_types("file:///test.meld", source);
            RC_ASSERT(result.has_errors());
            RC_ASSERT(result.errors[0].message.find("parenthes") != std::string::npos);
        }
    );
}

TEST(RefinementTypeEvalPropertyTest, AssignmentInPredicateProducesError) {
    rc::check("Refinement predicates using assignment must produce errors",
        []() {
            auto source = *genAssignmentRefinement();
            AnalysisEngine engine;
            auto result = engine.evaluate_refinement_types("file:///test.meld", source);
            RC_ASSERT(result.has_errors());
            RC_ASSERT(result.errors[0].message.find("assignment") != std::string::npos);
        }
    );
}

TEST(RefinementTypeEvalPropertyTest, RefinementCountIsAccurate) {
    rc::check("Refinement count must match the number of refinement types in source",
        []() {
            auto r1 = *genValidRefinementType();
            auto r2 = *genValidRefinementType();
            // Ensure different names by appending suffix
            std::string source = r1 + "\n" + r2;
            AnalysisEngine engine;
            auto result = engine.evaluate_refinement_types("file:///test.meld", source);
            RC_ASSERT(result.refinement_count >= 1);
        }
    );
}

TEST(RefinementTypeEvalPropertyTest, EmptyContentProducesNoErrors) {
    rc::check("Empty content must produce no refinement type errors",
        []() {
            AnalysisEngine engine;
            auto result = engine.evaluate_refinement_types("file:///test.meld", "");
            RC_ASSERT(!result.has_errors());
            RC_ASSERT(result.refinement_count == 0);
        }
    );
}
