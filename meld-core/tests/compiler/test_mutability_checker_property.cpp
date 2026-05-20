/// @file test_mutability_checker_property.cpp
/// @brief Property-based tests for mutating method enforcement (Task 44.6).
///
/// Feature: meld-lang, Property 53: Mutating Method Declaration Enforcement
///
/// For any method M that mutates `this`, the compiler accepts M if and only
/// if M is declared with `var fnc`. Specifically:
///   - mutates_this && is_mutating  → no diagnostic (correct var fnc usage)
///   - mutates_this && !is_mutating → E5001 error (missing var fnc)
///   - !mutates_this && is_mutating → W5001 warning (unnecessary var fnc)
///   - !mutates_this && !is_mutating → no diagnostic (correct non-mutating)
///
/// Uses rapidcheck for property-based testing with Google Test integration.
///
/// **Validates: Requirements 57.1, 57.2, 57.3**

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "meld/compiler/mutability_checker_pass.hpp"

#include <algorithm>
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

ast::expression make_id_expr(const std::string& name) {
    return ast::expression(make_id(name));
}

ast::expression make_binop(
    const std::string& op,
    ast::expression left,
    ast::expression right
) {
    ast::binary_operation binop;
    binop.op = op;
    binop.left = boost::spirit::x3::forward_ast<ast::expression>(std::move(left));
    binop.right = boost::spirit::x3::forward_ast<ast::expression>(std::move(right));
    return ast::expression(
        boost::spirit::x3::forward_ast<ast::binary_operation>(std::move(binop)));
}

ast::expression make_this_dot(const std::string& field_name) {
    return make_binop(".", make_id_expr("this"), make_id_expr(field_name));
}

ast::expression make_this_field_assign(
    const std::string& field_name,
    ast::expression value
) {
    return make_binop("=", make_this_dot(field_name), std::move(value));
}

ast::function_definition make_method(
    const std::string& name,
    bool is_mutating,
    std::vector<ast::expression> body_stmts
) {
    ast::function_definition method;
    method.name = make_id(name);
    method.is_mutating = is_mutating;
    ast::block_expression body;
    for (auto& stmt : body_stmts) {
        body.statements.push_back(
            boost::spirit::x3::forward_ast<ast::expression>(std::move(stmt)));
    }
    method.body = boost::spirit::x3::forward_ast<ast::block_expression>(std::move(body));
    return method;
}

ast::expression make_class_with_methods(
    const std::string& class_name,
    std::vector<ast::function_definition> methods
) {
    ast::class_definition cls;
    cls.name = make_id(class_name);
    cls.methods = std::move(methods);
    ast::block_expression body;
    cls.body = boost::spirit::x3::forward_ast<ast::block_expression>(std::move(body));
    return ast::expression(
        boost::spirit::x3::forward_ast<ast::class_definition>(std::move(cls)));
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

/// Descriptor for a single generated method.
struct MethodSpec {
    std::string name;
    bool is_mutating;    // declared with var fnc
    bool mutates_this;   // body actually mutates this
};

/// Generate a method spec with a unique name derived from an index.
rc::Gen<MethodSpec> genMethodSpec(int index) {
    return rc::gen::apply(
        [index](const std::string& base, bool is_mutating, bool mutates_this) {
            // Append index to ensure unique method names within a class
            MethodSpec spec;
            spec.name = base + std::to_string(index);
            spec.is_mutating = is_mutating;
            spec.mutates_this = mutates_this;
            return spec;
        },
        genIdentifier(),
        rc::gen::arbitrary<bool>(),
        rc::gen::arbitrary<bool>()
    );
}

/// Build an AST function_definition from a MethodSpec.
/// If mutates_this is true, the body contains `this.field = value`.
/// Otherwise, the body contains a simple non-mutating expression.
ast::function_definition buildMethod(
    const MethodSpec& spec,
    const std::vector<ast::function_definition>& existing_var_fnc_methods
) {
    std::vector<ast::expression> body_stmts;
    if (spec.mutates_this) {
        // this.x = 1  — a direct this-field assignment
        body_stmts.push_back(
            make_this_field_assign("x", make_id_expr("1")));
    } else {
        // Just a read: `count` — no mutation
        body_stmts.push_back(make_id_expr("count"));
    }
    return make_method(spec.name, spec.is_mutating, std::move(body_stmts));
}

/// Generate a class with 1-5 random methods and return both the AST
/// expression and the vector of MethodSpecs for verification.
rc::Gen<std::pair<ast::expression, std::vector<MethodSpec>>> genClassWithMethods() {
    return rc::gen::mapcat(
        rc::gen::pair(genIdentifier(), rc::gen::inRange(1, 6)),
        [](const std::pair<std::string, int>& p) {
            const auto& class_name = p.first;
            int num_methods = p.second;

            // Build generators for each method with unique index
            std::vector<rc::Gen<MethodSpec>> method_gens;
            for (int i = 0; i < num_methods; ++i) {
                method_gens.push_back(genMethodSpec(i));
            }

            return rc::gen::apply(
                [class_name](const std::vector<MethodSpec>& specs) {
                    // Build AST methods from specs
                    std::vector<ast::function_definition> methods;
                    for (const auto& spec : specs) {
                        methods.push_back(buildMethod(spec, {}));
                    }
                    auto cls_expr = make_class_with_methods(class_name, std::move(methods));
                    return std::make_pair(std::move(cls_expr), specs);
                },
                rc::gen::container<std::vector<MethodSpec>>(
                    num_methods, genMethodSpec(0)
                )
            );
        }
    );
}

} // anonymous namespace

// ===========================================================================
// Property 53: Mutating Method Declaration Enforcement
//
// For any randomly generated class with methods that may or may not mutate
// `this`, and may or may not be declared `var fnc`:
//   - mutates_this && is_mutating  → no diagnostic for that method
//   - mutates_this && !is_mutating → E5001 error
//   - !mutates_this && is_mutating → W5001 warning
//   - !mutates_this && !is_mutating → no diagnostic
//
// **Validates: Requirements 57.1, 57.2, 57.3**
// ===========================================================================

RC_GTEST_PROP(MutabilityCheckerProperty, MutatingMethodDeclarationEnforcement, ()) {
    // Generate method specs with unique names
    int num_methods = *rc::gen::inRange(1, 6);
    std::vector<MethodSpec> specs;
    for (int i = 0; i < num_methods; ++i) {
        auto spec = *genMethodSpec(i);
        specs.push_back(spec);
    }

    // Generate a class name
    auto class_name = *genIdentifier();

    // Build AST methods from specs
    std::vector<ast::function_definition> methods;
    for (const auto& spec : specs) {
        methods.push_back(buildMethod(spec, {}));
    }
    auto cls_expr = make_class_with_methods(class_name, std::move(methods));

    // Run the mutability checker pass
    MutabilityCheckerPass pass;
    std::vector<ast::expression> exprs = { std::move(cls_expr) };
    auto result = pass.run(exprs, "property_test.meld");

    // Compute expected diagnostics from specs
    size_t expected_errors = 0;
    size_t expected_warnings = 0;
    for (const auto& spec : specs) {
        if (spec.mutates_this && !spec.is_mutating) {
            expected_errors++;
        } else if (!spec.mutates_this && spec.is_mutating) {
            expected_warnings++;
        }
    }

    // Verify aggregate counts
    RC_ASSERT(result.errors_emitted == expected_errors);
    RC_ASSERT(result.warnings_emitted == expected_warnings);
    RC_ASSERT(result.methods_checked == static_cast<size_t>(num_methods));

    // Verify success flag: false iff any E5001 errors
    RC_ASSERT(result.success == (expected_errors == 0));

    // Verify per-method diagnostics
    for (const auto& spec : specs) {
        bool has_e5001 = std::any_of(
            result.diagnostics.begin(), result.diagnostics.end(),
            [&](const MutabilityDiagnostic& d) {
                return d.code == "E5001" &&
                       d.message.find(spec.name) != std::string::npos;
            });

        bool has_w5001 = std::any_of(
            result.diagnostics.begin(), result.diagnostics.end(),
            [&](const MutabilityDiagnostic& d) {
                return d.code == "W5001" &&
                       d.message.find(spec.name) != std::string::npos;
            });

        if (spec.mutates_this && !spec.is_mutating) {
            // E5001 expected: mutates this without var fnc
            RC_ASSERT(has_e5001);
            RC_ASSERT(!has_w5001);
        } else if (!spec.mutates_this && spec.is_mutating) {
            // W5001 expected: var fnc but no mutation
            RC_ASSERT(!has_e5001);
            RC_ASSERT(has_w5001);
        } else {
            // No diagnostic expected for this method
            RC_ASSERT(!has_e5001);
            RC_ASSERT(!has_w5001);
        }
    }

    // Verify diagnostic source file propagation
    for (const auto& diag : result.diagnostics) {
        RC_ASSERT(diag.source_file == "property_test.meld");
    }
}
