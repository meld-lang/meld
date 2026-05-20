/// @file test_trait_var_fnc_checker_property.cpp
/// @brief Property-based tests for trait `var fnc` compatibility (Task 44.8).
///
/// Feature: meld-lang, Property 55: Trait var fnc Compatibility
///
/// For any trait T with method M, an implementation of M must use `var fnc`
/// if and only if T declares M with `var fnc`. Specifically:
///   - trait_is_mutating && impl_is_mutating  → no diagnostic (match)
///   - trait_is_mutating && !impl_is_mutating → E5004 error (impl missing var fnc)
///   - !trait_is_mutating && impl_is_mutating → E5003 error (impl adds var fnc)
///   - !trait_is_mutating && !impl_is_mutating → no diagnostic (match)
///
/// The pass accepts if and only if `trait_is_mutating == impl_is_mutating`.
///
/// Uses rapidcheck for property-based testing with Google Test integration.
///
/// **Validates: Requirements 57.8**

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "meld/compiler/trait_var_fnc_checker_pass.hpp"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

using namespace meld::compiler;
namespace ast = meld::parser::ast;

// ===========================================================================
// AST construction helpers (mirrors unit test helpers)
// ===========================================================================

namespace {

ast::identifier make_id(const std::string& name) {
    ast::identifier id;
    id.name = name;
    return id;
}

ast::function_definition make_impl_method(
    const std::string& name,
    bool is_mutating
) {
    ast::function_definition func;
    func.name = make_id(name);
    func.is_mutating = is_mutating;
    ast::block_expression body;
    func.body = boost::spirit::x3::forward_ast<ast::block_expression>(std::move(body));
    return func;
}

TraitMethodSignature make_trait_sig(
    const std::string& name,
    bool is_mutating
) {
    return TraitMethodSignature{name, is_mutating};
}

} // anonymous namespace

// ===========================================================================
// Generators
// ===========================================================================

namespace {

/// Generate a valid identifier string (lowercase alpha, 3-8 chars).
rc::Gen<std::string> genIdentifier() {
    return rc::gen::mapcat(
        rc::gen::inRange(3, 9),
        [](int len) {
            return rc::gen::container<std::string>(
                len,
                rc::gen::inRange('a', static_cast<char>('z' + 1))
            );
        }
    );
}

/// Descriptor for a single generated method pair (trait side + impl side).
struct MethodPairSpec {
    std::string name;
    bool trait_is_mutating;
    bool impl_is_mutating;
};

/// Generate a method pair spec with a unique name derived from an index.
rc::Gen<MethodPairSpec> genMethodPairSpec(int index) {
    return rc::gen::apply(
        [index](const std::string& base, bool trait_mut, bool impl_mut) {
            MethodPairSpec spec;
            spec.name = base + std::to_string(index);
            spec.trait_is_mutating = trait_mut;
            spec.impl_is_mutating = impl_mut;
            return spec;
        },
        genIdentifier(),
        rc::gen::arbitrary<bool>(),
        rc::gen::arbitrary<bool>()
    );
}

} // anonymous namespace

// ===========================================================================
// Property 55: Trait var fnc Compatibility
//
// For any randomly generated trait with methods that may or may not be
// declared `var fnc`, and an implementation that may or may not use `var fnc`
// for each method:
//   - trait_is_mutating && impl_is_mutating  → no diagnostic (match)
//   - trait_is_mutating && !impl_is_mutating → E5004 error
//   - !trait_is_mutating && impl_is_mutating → E5003 error
//   - !trait_is_mutating && !impl_is_mutating → no diagnostic (match)
//
// The pass accepts if and only if trait_is_mutating == impl_is_mutating.
//
// **Validates: Requirements 57.8**
// ===========================================================================

RC_GTEST_PROP(TraitVarFncCheckerProperty, TraitVarFncCompatibility, ()) {
    // Generate 1-5 method pair specs with unique names
    int num_methods = *rc::gen::inRange(1, 6);
    std::vector<MethodPairSpec> specs;
    for (int i = 0; i < num_methods; ++i) {
        auto spec = *genMethodPairSpec(i);
        specs.push_back(spec);
    }

    // Generate trait and impl type names
    auto trait_name = *genIdentifier();
    auto impl_type_name = *genIdentifier();

    // Build the TraitImplPair from specs
    std::vector<TraitMethodSignature> trait_methods;
    std::vector<ast::function_definition> impl_methods;
    for (const auto& spec : specs) {
        trait_methods.push_back(make_trait_sig(spec.name, spec.trait_is_mutating));
        impl_methods.push_back(make_impl_method(spec.name, spec.impl_is_mutating));
    }

    TraitImplPair pair{
        .trait_name = trait_name,
        .impl_type_name = impl_type_name,
        .trait_methods = std::move(trait_methods),
        .impl_methods = std::move(impl_methods)
    };

    // Run the pass
    TraitVarFncCheckerPass pass;
    auto result = pass.run({pair}, "property_test.meld");

    // Compute expected diagnostics from specs
    size_t expected_e5003 = 0;  // impl adds var fnc
    size_t expected_e5004 = 0;  // impl missing var fnc
    for (const auto& spec : specs) {
        if (!spec.trait_is_mutating && spec.impl_is_mutating) {
            expected_e5003++;
        } else if (spec.trait_is_mutating && !spec.impl_is_mutating) {
            expected_e5004++;
        }
    }
    size_t expected_errors = expected_e5003 + expected_e5004;

    // Verify aggregate counts
    RC_ASSERT(result.methods_checked == static_cast<size_t>(num_methods));
    RC_ASSERT(result.errors_emitted == expected_errors);
    RC_ASSERT(result.diagnostics.size() == expected_errors);

    // Verify success flag: true iff no mismatches
    RC_ASSERT(result.success == (expected_errors == 0));

    // Verify per-method diagnostics match the four-quadrant property
    for (const auto& spec : specs) {
        bool has_e5003 = std::any_of(
            result.diagnostics.begin(), result.diagnostics.end(),
            [&](const TraitVarFncDiagnostic& d) {
                return d.code == "E5003" &&
                       d.message.find(spec.name) != std::string::npos;
            });

        bool has_e5004 = std::any_of(
            result.diagnostics.begin(), result.diagnostics.end(),
            [&](const TraitVarFncDiagnostic& d) {
                return d.code == "E5004" &&
                       d.message.find(spec.name) != std::string::npos;
            });

        if (!spec.trait_is_mutating && spec.impl_is_mutating) {
            // E5003: impl adds var fnc that trait doesn't declare
            RC_ASSERT(has_e5003);
            RC_ASSERT(!has_e5004);
        } else if (spec.trait_is_mutating && !spec.impl_is_mutating) {
            // E5004: impl omits var fnc that trait requires
            RC_ASSERT(!has_e5003);
            RC_ASSERT(has_e5004);
        } else {
            // Match — no diagnostic expected for this method
            RC_ASSERT(!has_e5003);
            RC_ASSERT(!has_e5004);
        }
    }

    // Verify diagnostic metadata propagation
    for (const auto& diag : result.diagnostics) {
        RC_ASSERT(diag.source_file == "property_test.meld");
        RC_ASSERT(diag.level == TraitVarFncDiagnostic::Level::Error);
        // Diagnostics should reference the impl type and trait names
        RC_ASSERT(diag.message.find(impl_type_name) != std::string::npos);
        RC_ASSERT(diag.message.find(trait_name) != std::string::npos);
    }
}
