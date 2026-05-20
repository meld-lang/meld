/// @file test_mutability_qualifier_property.cpp
/// @brief Property-based tests for generic mutability qualifiers
///
/// Property: val Type Parameter Immutability (Req 165.3)
/// Property: var-to-val Downgrade Compatibility (Req 165.5)
///
/// Requirements: 165.1-165.6

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "meld/compiler/mutability_enforcement_pass.hpp"
#include "meld/compiler/mutability_qualifier_pass.hpp"

using namespace meld::compiler;
using MQ = meld::parser::ast::type_annotation::MutabilityQualifier;

// ===========================================================================
// Helpers
// ===========================================================================

static meld::parser::ast::type_annotation make_generic_type(
    const std::string& name, const std::string& arg_name, MQ mq
) {
    meld::parser::ast::type_annotation type;
    type.type_name.name = name;
    type.has_type_arguments = true;
    meld::parser::ast::type_annotation arg;
    arg.type_name.name = arg_name;
    arg.mutability_qualifier = mq;
    type.type_arguments.push_back(
        boost::spirit::x3::forward_ast<meld::parser::ast::type_annotation>(std::move(arg)));
    return type;
}

static meld::parser::ast::expression make_id_expr(const std::string& name) {
    meld::parser::ast::identifier id;
    id.name = name;
    return meld::parser::ast::expression(id);
}

static meld::parser::ast::expression make_call_expr(const std::string& func_name) {
    meld::parser::ast::function_call call;
    call.function_name.name = func_name;
    return meld::parser::ast::expression(
        boost::spirit::x3::forward_ast<meld::parser::ast::function_call>(std::move(call)));
}

static meld::parser::ast::expression make_method_call(
    const std::string& receiver, const std::string& method
) {
    meld::parser::ast::binary_operation binop;
    binop.op = ".";
    binop.left = boost::spirit::x3::forward_ast<meld::parser::ast::expression>(make_id_expr(receiver));
    binop.right = boost::spirit::x3::forward_ast<meld::parser::ast::expression>(make_call_expr(method));
    return meld::parser::ast::expression(
        boost::spirit::x3::forward_ast<meld::parser::ast::binary_operation>(std::move(binop)));
}

static meld::parser::ast::expression make_val_with_type(
    const std::string& name, meld::parser::ast::type_annotation type_ann
) {
    meld::parser::ast::val_declaration decl;
    decl.name.name = name;
    decl.has_type_annotation = true;
    decl.type_ann = boost::spirit::x3::forward_ast<meld::parser::ast::type_annotation>(std::move(type_ann));
    decl.value = boost::spirit::x3::forward_ast<meld::parser::ast::expression>(make_id_expr("dummy"));
    return meld::parser::ast::expression(
        boost::spirit::x3::forward_ast<meld::parser::ast::val_declaration>(std::move(decl)));
}

static meld::parser::ast::expression make_function_with_body(
    const std::string& func_name,
    std::vector<meld::parser::ast::expression> body_stmts
) {
    meld::parser::ast::function_definition func;
    func.name.name = func_name;
    meld::parser::ast::block_expression body;
    for (auto& stmt : body_stmts) {
        body.statements.push_back(
            boost::spirit::x3::forward_ast<meld::parser::ast::expression>(std::move(stmt)));
    }
    func.body = boost::spirit::x3::forward_ast<meld::parser::ast::block_expression>(std::move(body));
    return meld::parser::ast::expression(
        boost::spirit::x3::forward_ast<meld::parser::ast::function_definition>(std::move(func)));
}

// ===========================================================================
// Property: val Type Parameter Immutability
// Feature: hold-view-tenancy-model, Req 165.3
//
// For any generic type Foo[val T], calling a mutating method on the
// contained value should be rejected at compile time (E4010).
// ===========================================================================

TEST(MutabilityQualifierPropertyTest, ValTypeParamImmutability) {
    rc::check("Any mutating method call on val-qualified type param emits E4010",
        []() {
            // Generate a valid class name (uppercase first, alphanumeric)
            auto class_name = *rc::gen::suchThat(
                rc::gen::string<std::string>(),
                [](const std::string& s) {
                    return s.size() >= 1 && s.size() <= 15 &&
                           std::isupper(s[0]) &&
                           std::all_of(s.begin(), s.end(),
                               [](char c) { return std::isalnum(c); });
                });

            // Generate a valid binding name (lowercase first, alphanumeric)
            auto binding = *rc::gen::suchThat(
                rc::gen::string<std::string>(),
                [&class_name](const std::string& s) {
                    return s.size() >= 1 && s.size() <= 15 && s != class_name &&
                           std::islower(s[0]) &&
                           std::all_of(s.begin(), s.end(),
                               [](char c) { return std::isalnum(c) || c == '_'; });
                });

            // Pick a random mutating method
            std::vector<std::string> methods = {
                "add", "remove", "clear", "push", "pop",
                "set", "insert", "delete", "update", "append"
            };
            auto idx = *rc::gen::inRange(0, static_cast<int>(methods.size()));
            const auto& method = methods[idx];

            // Build: fnc test() { val x: Hold[val ClassName] = dummy; x.method() }
            auto val_decl = make_val_with_type(binding,
                make_generic_type("Hold", class_name, MQ::VAL));
            auto call = make_method_call(binding, method);
            auto func = make_function_with_body("test", {std::move(val_decl), std::move(call)});

            std::vector<meld::parser::ast::expression> exprs = {std::move(func)};
            MutabilityEnforcementPass pass;
            auto result = pass.run(exprs, "test.meld");

            RC_ASSERT(!result.success);
            RC_ASSERT(result.val_violations >= 1u);
            RC_ASSERT(!result.diagnostics.empty());
            RC_ASSERT(result.diagnostics[0].code == "E4010");
        }
    );
}

// ===========================================================================
// Property: var-to-val Downgrade Compatibility
// Feature: hold-view-tenancy-model, Req 165.5
//
// For any pair of MutabilityQualifier values, is_qualifier_compatible
// returns true iff source==target or (source==VAR and target==VAL).
// ===========================================================================

TEST(MutabilityQualifierPropertyTest, VarToValDowngradeCompatibility) {
    rc::check("Qualifier compatibility follows downgrade-only rule",
        []() {
            // Generate random source and target qualifiers (VAL or VAR)
            auto src_idx = *rc::gen::inRange(0, 2);
            auto tgt_idx = *rc::gen::inRange(0, 2);
            MQ source = (src_idx == 0) ? MQ::VAL : MQ::VAR;
            MQ target = (tgt_idx == 0) ? MQ::VAL : MQ::VAR;

            bool result = MutabilityEnforcementPass::is_qualifier_compatible(source, target);

            // Expected: compatible iff same or downgrade (VAR→VAL)
            bool expected = (source == target) || (source == MQ::VAR && target == MQ::VAL);
            RC_ASSERT(result == expected);
        }
    );
}
