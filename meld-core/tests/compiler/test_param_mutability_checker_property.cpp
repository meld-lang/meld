/// @file test_param_mutability_checker_property.cpp
/// @brief Property-based tests for mutable parameter enforcement (Task 44.7).
///
/// Feature: meld-lang, Property 54: Mutable Parameter Enforcement
///
/// For any function parameter P, mutation of P is accepted if and only
/// if P is declared with `var`. Specifically:
///   - is_mutated && is_mutable  → no diagnostic (correct var usage)
///   - is_mutated && !is_mutable → E5002 error (missing var)
///   - !is_mutated && is_mutable → W5002 warning (unnecessary var)
///   - !is_mutated && !is_mutable → no diagnostic (correct immutable)
///
/// Uses rapidcheck for property-based testing with Google Test integration.
///
/// **Validates: Requirements 57.4, 57.5, 57.6**

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "meld/compiler/param_mutability_checker_pass.hpp"

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

ast::expression make_dot_access(const std::string& target, const std::string& field) {
    return make_binop(".", make_id_expr(target), make_id_expr(field));
}

ast::expression make_direct_assign(
    const std::string& target,
    ast::expression value
) {
    return make_binop("=", make_id_expr(target), std::move(value));
}

ast::expression make_field_assign(
    const std::string& target,
    const std::string& field,
    ast::expression value
) {
    return make_binop("=", make_dot_access(target, field), std::move(value));
}

ast::function_parameter make_param(
    const std::string& name,
    bool is_mutable = false
) {
    ast::function_parameter param;
    param.name = make_id(name);
    param.is_mutable = is_mutable;
    return param;
}

ast::function_definition make_function(
    const std::string& name,
    std::vector<ast::function_parameter> params,
    std::vector<ast::expression> body_stmts
) {
    ast::function_definition func;
    func.name = make_id(name);
    func.parameters = std::move(params);
    ast::block_expression body;
    for (auto& stmt : body_stmts) {
        body.statements.push_back(
            boost::spirit::x3::forward_ast<ast::expression>(std::move(stmt)));
    }
    func.body = boost::spirit::x3::forward_ast<ast::block_expression>(std::move(body));
    return func;
}

ast::expression make_func_expr(ast::function_definition func) {
    return ast::expression(
        boost::spirit::x3::forward_ast<ast::function_definition>(std::move(func)));
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

/// Descriptor for a single generated parameter.
struct ParamSpec {
    std::string name;
    bool is_mutable;        // declared with var
    bool is_mutated;        // body mutates this param
    bool is_field_mutation;  // true = param.field = val, false = param = val
};

/// Generate a ParamSpec with a unique name derived from an index.
rc::Gen<ParamSpec> genParamSpec(int index) {
    return rc::gen::apply(
        [index](const std::string& base, bool is_mutable,
                bool is_mutated, bool is_field_mutation) {
            ParamSpec spec;
            spec.name = base + std::to_string(index);
            spec.is_mutable = is_mutable;
            spec.is_mutated = is_mutated;
            spec.is_field_mutation = is_field_mutation;
            return spec;
        },
        genIdentifier(),
        rc::gen::arbitrary<bool>(),
        rc::gen::arbitrary<bool>(),
        rc::gen::arbitrary<bool>()
    );
}

/// Build a function AST from a vector of ParamSpecs.
/// For mutated params, the body contains an assignment statement
/// (direct or field-based depending on is_field_mutation).
/// For non-mutated params, the body contains a read expression.
ast::function_definition buildFunction(
    const std::string& func_name,
    const std::vector<ParamSpec>& specs
) {
    std::vector<ast::function_parameter> params;
    std::vector<ast::expression> body_stmts;

    for (const auto& spec : specs) {
        params.push_back(make_param(spec.name, spec.is_mutable));

        if (spec.is_mutated) {
            if (spec.is_field_mutation) {
                // param.field = value
                body_stmts.push_back(
                    make_field_assign(spec.name, "field", make_id_expr("value")));
            } else {
                // param = value
                body_stmts.push_back(
                    make_direct_assign(spec.name, make_id_expr("value")));
            }
        } else {
            // Read-only usage: param.field (dot access, not assignment)
            body_stmts.push_back(make_dot_access(spec.name, "field"));
        }
    }

    return make_function(func_name, std::move(params), std::move(body_stmts));
}

} // anonymous namespace

// ===========================================================================
// Property 54: Mutable Parameter Enforcement
//
// For any randomly generated function with parameters that may or may not
// be declared `var`, and whose body may or may not mutate each parameter:
//   - is_mutated && is_mutable  → no diagnostic for that parameter
//   - is_mutated && !is_mutable → E5002 error
//   - !is_mutated && is_mutable → W5002 warning
//   - !is_mutated && !is_mutable → no diagnostic
//
// **Validates: Requirements 57.4, 57.5, 57.6**
// ===========================================================================

RC_GTEST_PROP(ParamMutabilityCheckerProperty, MutableParameterEnforcement, ()) {
    // Generate 1-4 parameter specs with unique names
    int num_params = *rc::gen::inRange(1, 5);
    std::vector<ParamSpec> specs;
    for (int i = 0; i < num_params; ++i) {
        auto spec = *genParamSpec(i);
        specs.push_back(spec);
    }

    // Generate a function name
    auto func_name = *genIdentifier();

    // Build the function AST and wrap as top-level expression
    auto func = buildFunction(func_name, specs);
    std::vector<ast::expression> exprs = { make_func_expr(std::move(func)) };

    // Run the parameter mutability checker pass
    ParamMutabilityCheckerPass pass;
    auto result = pass.run(exprs, "property_test.meld");

    // Compute expected diagnostics from specs
    size_t expected_errors = 0;
    size_t expected_warnings = 0;
    size_t expected_mutations = 0;
    for (const auto& spec : specs) {
        if (spec.is_mutated) {
            expected_mutations++;
            if (!spec.is_mutable) {
                expected_errors++;   // E5002: mutated without var
            }
        } else {
            if (spec.is_mutable) {
                expected_warnings++; // W5002: var but never mutated
            }
        }
    }

    // Verify aggregate counts
    RC_ASSERT(result.functions_checked == 1u);
    RC_ASSERT(result.param_mutations_detected == expected_mutations);
    RC_ASSERT(result.errors_emitted == expected_errors);
    RC_ASSERT(result.warnings_emitted == expected_warnings);

    // Verify success flag: false iff any E5002 errors
    RC_ASSERT(result.success == (expected_errors == 0));

    // Verify total diagnostic count
    RC_ASSERT(result.diagnostics.size() == expected_errors + expected_warnings);

    // Verify per-parameter diagnostics match the four-quadrant property
    for (const auto& spec : specs) {
        bool has_e5002 = std::any_of(
            result.diagnostics.begin(), result.diagnostics.end(),
            [&](const ParamMutabilityDiagnostic& d) {
                return d.code == "E5002" &&
                       d.message.find(spec.name) != std::string::npos;
            });

        bool has_w5002 = std::any_of(
            result.diagnostics.begin(), result.diagnostics.end(),
            [&](const ParamMutabilityDiagnostic& d) {
                return d.code == "W5002" &&
                       d.message.find(spec.name) != std::string::npos;
            });

        if (spec.is_mutated && !spec.is_mutable) {
            // E5002 expected: mutated without var
            RC_ASSERT(has_e5002);
            RC_ASSERT(!has_w5002);
        } else if (!spec.is_mutated && spec.is_mutable) {
            // W5002 expected: var but no mutation
            RC_ASSERT(!has_e5002);
            RC_ASSERT(has_w5002);
        } else {
            // No diagnostic expected for this parameter
            RC_ASSERT(!has_e5002);
            RC_ASSERT(!has_w5002);
        }
    }

    // Verify diagnostic source file propagation
    for (const auto& diag : result.diagnostics) {
        RC_ASSERT(diag.source_file == "property_test.meld");
    }

    // Verify function name appears in all diagnostic messages
    for (const auto& diag : result.diagnostics) {
        RC_ASSERT(diag.message.find(func_name) != std::string::npos);
    }
}
