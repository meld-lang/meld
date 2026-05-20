/**
 * Property-based tests for null safety (Tasks 48.5–48.8).
 *
 * Property 72: Non-Optional Nil Rejection (Req 14A-NIL.20, 14A-NIL.21)
 * Property 73: Optional Structural Distinction (Req 14A-NIL.22)
 * Property 74: Flow-Sensitive Narrowing Correctness (Req 14A-NIL.23, 14A-NIL.24)
 * Property 75: NPE Impossibility (Req 14A-NIL.26)
 */

#include <gtest/gtest.h>
#include "meld/compiler/type_checker.hpp"
#include "meld/parser/ast.hpp"
#include "meld/meta/metatype.hpp"

using namespace meld::compiler;
using namespace meld::parser::ast;
using namespace meld::meta;
namespace x3 = boost::spirit::x3;

class NilSafetyPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        checker = std::make_unique<TypeChecker>();
        env = std::make_shared<TypeEnvironment>();
    }

    std::unique_ptr<TypeChecker> checker;
    std::shared_ptr<TypeEnvironment> env;

    expression make_nil() {
        identifier id; id.name = "nil";
        return expression(id);
    }

    expression make_id(const std::string& name) {
        identifier id; id.name = name;
        return expression(id);
    }

    expression make_val(const std::string& name, const std::string& type_name,
                        bool nullable, const expression& value) {
        val_declaration decl;
        decl.name.name = name;
        decl.has_type_annotation = true;
        decl.type_ann.get().type_name.name = type_name;
        decl.type_ann.get().is_nullable = nullable;
        decl.value = value;
        expression e; e = x3::forward_ast<val_declaration>(decl);
        return e;
    }

    expression make_func_call(const std::string& func_name,
                              const std::vector<expression>& args) {
        function_call call;
        call.function_name.name = func_name;
        for (auto& a : args) call.arguments.push_back(a);
        expression e; e = x3::forward_ast<function_call>(call);
        return e;
    }

    expression make_tuple_indexing(const expression& obj, const std::string& field) {
        tuple_indexing ti;
        ti.tuple = obj;
        ti.index = field;
        ti.is_numeric = false;
        expression e; e = x3::forward_ast<tuple_indexing>(ti);
        return e;
    }

    expression make_unary(const std::string& op, const expression& operand) {
        unary_operation uo;
        uo.op = op;
        uo.operand = operand;
        expression e; e = x3::forward_ast<unary_operation>(uo);
        return e;
    }

    expression make_elvis(const expression& nullable, const expression& default_val) {
        elvis_expression ev;
        ev.nullable_expr = nullable;
        ev.default_value = default_val;
        expression e; e = x3::forward_ast<elvis_expression>(ev);
        return e;
    }

    expression make_safe_nav(const expression& nullable, const std::string& field) {
        safe_navigation_expression sn;
        sn.nullable_expr = nullable;
        sn.field_name = field;
        expression e; e = x3::forward_ast<safe_navigation_expression>(sn);
        return e;
    }

    void define_func(const std::string& name, const std::string& param_name,
                     const std::string& param_type, bool param_nullable) {
        function_definition fd;
        fd.name.name = name;
        fd.has_return_type = true;
        fd.return_type.type_name.name = "unit";
        function_parameter p;
        p.name.name = param_name;
        p.type.type_name.name = param_type;
        p.type.is_nullable = param_nullable;
        fd.parameters.push_back(p);
        expression e; e = x3::forward_ast<function_definition>(fd);
        checker->check_expression(e, env);
    }

    void bind_optional(const std::string& name, const std::string& inner) {
        auto& reg = TypeRegistry::instance();
        auto t = reg.get_type(inner);
        if (t) env->bind(name, reg.create_optional_type(*t));
    }

    void bind_non_optional(const std::string& name, const std::string& type_name) {
        auto& reg = TypeRegistry::instance();
        auto t = reg.get_type(type_name);
        if (t) env->bind(name, *t);
    }
};

// ============================================================================
// Property 72: Non-Optional Nil Rejection
// For any non-optional type T, assigning nil must be rejected.
// ============================================================================

TEST_F(NilSafetyPropertyTest, Property72_NilAssignmentToStringRejected) {
    auto result = checker->check_expression(
        make_val("x", "string", false, make_nil()), env);
    EXPECT_FALSE(result.has_value());
}

TEST_F(NilSafetyPropertyTest, Property72_NilAssignmentToIntRejected) {
    auto result = checker->check_expression(
        make_val("x", "int", false, make_nil()), env);
    EXPECT_FALSE(result.has_value());
}

TEST_F(NilSafetyPropertyTest, Property72_NilAssignmentToBoolRejected) {
    auto result = checker->check_expression(
        make_val("x", "bool", false, make_nil()), env);
    EXPECT_FALSE(result.has_value());
}

TEST_F(NilSafetyPropertyTest, Property72_NilAssignmentToFloatRejected) {
    auto result = checker->check_expression(
        make_val("x", "float", false, make_nil()), env);
    EXPECT_FALSE(result.has_value());
}

TEST_F(NilSafetyPropertyTest, Property72_NilToOptionalAccepted) {
    auto result = checker->check_expression(
        make_val("x", "string", true, make_nil()), env);
    EXPECT_TRUE(result.has_value());
}

TEST_F(NilSafetyPropertyTest, Property72_NilArgToNonOptionalParamRejected) {
    define_func("greet", "name", "string", false);
    auto result = checker->check_expression(
        make_func_call("greet", {make_nil()}), env);
    EXPECT_FALSE(result.has_value());
}

TEST_F(NilSafetyPropertyTest, Property72_NilArgToOptionalParamAccepted) {
    define_func("maybe-greet", "name", "string", true);
    auto result = checker->check_expression(
        make_func_call("maybe-greet", {make_nil()}), env);
    EXPECT_TRUE(result.has_value());
}

// ============================================================================
// Property 73: Optional Structural Distinction
// For any type T, calling a method of T on optional[T] must be rejected.
// ============================================================================

TEST_F(NilSafetyPropertyTest, Property73_MemberAccessOnOptionalStringRejected) {
    bind_optional("name", "string");
    auto result = checker->check_expression(
        make_tuple_indexing(make_id("name"), "length"), env);
    EXPECT_FALSE(result.has_value());
    EXPECT_NE(result.error().message.find("optional"), std::string::npos);
}

TEST_F(NilSafetyPropertyTest, Property73_MemberAccessOnOptionalIntRejected) {
    bind_optional("count", "int");
    auto result = checker->check_expression(
        make_tuple_indexing(make_id("count"), "toString"), env);
    EXPECT_FALSE(result.has_value());
}

TEST_F(NilSafetyPropertyTest, Property73_MemberAccessOnNonOptionalAllowed) {
    bind_non_optional("name", "string");
    auto result = checker->check_expression(
        make_tuple_indexing(make_id("name"), "length"), env);
    // Should not fail with optional-related error
    if (!result.has_value()) {
        EXPECT_EQ(result.error().message.find("optional"), std::string::npos);
    }
}

TEST_F(NilSafetyPropertyTest, Property73_OptionalArgToNonOptionalParamRejected) {
    define_func("print-name", "name", "string", false);
    bind_optional("maybe-name", "string");
    auto result = checker->check_expression(
        make_func_call("print-name", {make_id("maybe-name")}), env);
    EXPECT_FALSE(result.has_value());
    EXPECT_NE(result.error().message.find("optional"), std::string::npos);
}

// ============================================================================
// Property 74: Flow-Sensitive Narrowing Correctness
// Safe operators narrow optional[T] to T within their scope.
// ============================================================================

TEST_F(NilSafetyPropertyTest, Property74_ForceUnwrapNarrowsToInner) {
    bind_optional("x", "string");
    auto result = checker->check_expression(
        make_unary("!!", make_id("x")), env);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)->name(), TypeRegistry::instance().get_string_type()->name());
}

TEST_F(NilSafetyPropertyTest, Property74_ForceUnwrapNarrowsIntToInner) {
    bind_optional("n", "int");
    auto result = checker->check_expression(
        make_unary("!!", make_id("n")), env);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)->name(), TypeRegistry::instance().get_int_type()->name());
}

TEST_F(NilSafetyPropertyTest, Property74_SafeReturnNarrowsToInner) {
    bind_optional("x", "string");
    auto result = checker->check_expression(
        make_unary("?", make_id("x")), env);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)->name(), TypeRegistry::instance().get_string_type()->name());
}

TEST_F(NilSafetyPropertyTest, Property74_ElvisNarrowsToInner) {
    bind_optional("x", "string");
    bind_non_optional("default-val", "string");
    auto result = checker->check_expression(
        make_elvis(make_id("x"), make_id("default-val")), env);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)->name(), TypeRegistry::instance().get_string_type()->name());
}

TEST_F(NilSafetyPropertyTest, Property74_SafeNavProducesOptional) {
    bind_optional("user", "string");
    auto result = checker->check_expression(
        make_safe_nav(make_id("user"), "length"), env);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(TypeRegistry::instance().is_nullable_type(**result));
}

TEST_F(NilSafetyPropertyTest, Property74_NarrowingDoesNotPersistOutsideOperator) {
    bind_optional("x", "string");
    // After !!, the variable itself is still optional in the environment
    checker->check_expression(make_unary("!!", make_id("x")), env);
    // Direct member access on x should still be rejected
    auto result = checker->check_expression(
        make_tuple_indexing(make_id("x"), "length"), env);
    EXPECT_FALSE(result.has_value());
}

// ============================================================================
// Property 75: NPE Impossibility
// Without !!, no execution path can produce a null dereference.
// The type system guarantees this by:
// 1. Rejecting nil assignment to non-optional types (Property 72)
// 2. Rejecting method calls on optional types (Property 73)
// 3. Safe operators always check for nil before accessing (Property 74)
// ============================================================================

TEST_F(NilSafetyPropertyTest, Property75_SafeNavNeverDereferencesNil) {
    bind_optional("user", "string");
    // ?. on optional should succeed (produces optional result, never NPE)
    auto result = checker->check_expression(
        make_safe_nav(make_id("user"), "length"), env);
    EXPECT_TRUE(result.has_value());
}

TEST_F(NilSafetyPropertyTest, Property75_ElvisNeverDereferencesNil) {
    bind_optional("x", "int");
    bind_non_optional("default-val", "int");
    // ?: on optional should succeed (provides default, never NPE)
    auto result = checker->check_expression(
        make_elvis(make_id("x"), make_id("default-val")), env);
    EXPECT_TRUE(result.has_value());
}

TEST_F(NilSafetyPropertyTest, Property75_DirectAccessBlockedOnOptional) {
    bind_optional("x", "string");
    // Direct .field on optional is blocked — prevents NPE
    auto result = checker->check_expression(
        make_tuple_indexing(make_id("x"), "length"), env);
    EXPECT_FALSE(result.has_value());
}

TEST_F(NilSafetyPropertyTest, Property75_NilCannotReachNonOptionalBinding) {
    // nil to non-optional is rejected — prevents NPE at source
    auto result = checker->check_expression(
        make_val("x", "string", false, make_nil()), env);
    EXPECT_FALSE(result.has_value());
}
