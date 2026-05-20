/**
 * **Feature: meld-lsp-server, Property 14: Refinement constraint validation**
 *
 * For any refinement type constraint violation, the LSP server should validate
 * logical predicates and report violations.
 *
 * **Validates: Requirements 3.4**
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
            std::string s = "A";
            for (int i = 1; i < len; ++i) {
                s += static_cast<char>('a' + ((i * 5 + 2) % 26));
            }
            return s;
        }
    );
}

rc::Gen<std::string> genBaseType() {
    return rc::gen::element(
        std::string("Int"), std::string("Float"), std::string("String"));
}

rc::Gen<std::string> genValidConstraint() {
    return rc::gen::element(
        std::string("it > 0"),
        std::string("it >= 0"),
        std::string("it < 100"),
        std::string("it != 0"),
        std::string("it > 0 && it < 100"),
        std::string("it >= -10 && it <= 10"));
}

/// Well-formed refinement type
rc::Gen<std::string> genValidRefinement() {
    return rc::gen::map(
        rc::gen::tuple(genTypeName(), genBaseType(), genValidConstraint()),
        [](const std::tuple<std::string, std::string, std::string>& t) {
            auto [name, base, constraint] = t;
            return "type " + name + " = " + base + " where { " + constraint + " }";
        }
    );
}

/// Refinement with empty constraint
rc::Gen<std::string> genEmptyConstraint() {
    return rc::gen::map(
        rc::gen::tuple(genTypeName(), genBaseType()),
        [](const std::tuple<std::string, std::string>& t) {
            auto [name, base] = t;
            return "type " + name + " = " + base + " where {   }";
        }
    );
}

/// Refinement missing 'it' reference
rc::Gen<std::string> genMissingItConstraint() {
    return rc::gen::map(
        rc::gen::tuple(genTypeName(), genBaseType()),
        [](const std::tuple<std::string, std::string>& t) {
            auto [name, base] = t;
            return "type " + name + " = " + base + " where { x > 0 }";
        }
    );
}

/// Refinement missing comparison operator
rc::Gen<std::string> genNoOperatorConstraint() {
    return rc::gen::map(
        rc::gen::tuple(genTypeName(), genBaseType()),
        [](const std::tuple<std::string, std::string>& t) {
            auto [name, base] = t;
            return "type " + name + " = " + base + " where { it }";
        }
    );
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

TEST(RefinementConstraintPropertyTest, ValidRefinementsProduceNoErrors) {
    rc::check("Well-formed refinement types must produce no constraint errors",
        []() {
            auto source = *genValidRefinement();
            AnalysisEngine engine;
            auto result = engine.validate_refinement_constraints("file:///test.meld", source);
            RC_ASSERT(!result.has_errors());
        }
    );
}

TEST(RefinementConstraintPropertyTest, EmptyConstraintProducesError) {
    rc::check("Refinement with empty constraint must produce an error",
        []() {
            auto source = *genEmptyConstraint();
            AnalysisEngine engine;
            auto result = engine.validate_refinement_constraints("file:///test.meld", source);
            RC_ASSERT(result.has_errors());
            RC_ASSERT(result.errors[0].message.find("empty") != std::string::npos);
        }
    );
}

TEST(RefinementConstraintPropertyTest, MissingItReferenceProducesError) {
    rc::check("Refinement constraint not referencing 'it' must produce an error",
        []() {
            auto source = *genMissingItConstraint();
            AnalysisEngine engine;
            auto result = engine.validate_refinement_constraints("file:///test.meld", source);
            RC_ASSERT(result.has_errors());
            RC_ASSERT(result.errors[0].message.find("it") != std::string::npos);
        }
    );
}

TEST(RefinementConstraintPropertyTest, MissingOperatorProducesError) {
    rc::check("Refinement constraint without comparison operator must produce an error",
        []() {
            auto source = *genNoOperatorConstraint();
            AnalysisEngine engine;
            auto result = engine.validate_refinement_constraints("file:///test.meld", source);
            RC_ASSERT(result.has_errors());
            RC_ASSERT(result.errors[0].message.find("operator") != std::string::npos);
        }
    );
}

TEST(RefinementConstraintPropertyTest, ErrorPositionsAreValid) {
    rc::check("Refinement constraint error positions must be non-negative",
        []() {
            auto source = *rc::gen::oneOf(
                genEmptyConstraint(),
                genMissingItConstraint(),
                genNoOperatorConstraint()
            );
            AnalysisEngine engine;
            auto result = engine.validate_refinement_constraints("file:///test.meld", source);
            for (const auto& err : result.errors) {
                RC_ASSERT(err.line >= 0);
                RC_ASSERT(err.character >= 0);
            }
        }
    );
}
