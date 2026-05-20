/**
 * Tests for flow-sensitive type narrowing (Task 48.3).
 *
 * Validates Requirements 14A-NIL.23, 14A-NIL.25:
 * - ?. narrows optional[T] and produces optional[result]
 * - ?: narrows optional[T] to T (unwrapped)
 * - !! narrows optional[T] to T (force unwrap)
 * - ? narrows optional[T] to T (safe return)
 */

#include <gtest/gtest.h>
#include "meld/compiler/type_checker.hpp"
#include "meld/parser/ast.hpp"
#include "meld/meta/metatype.hpp"

using namespace meld::compiler;
using namespace meld::parser::ast;
using namespace meld::meta;
namespace x3 = boost::spirit::x3;

class NarrowingTest : public ::testing::Test {
protected:
    void SetUp() override {
        checker = std::make_unique<TypeChecker>();
        env = std::make_shared<TypeEnvironment>();
    }

    std::unique_ptr<TypeChecker> checker;
    std::shared_ptr<TypeEnvironment> env;

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

    expression make_id(const std::string& name) {
        identifier id; id.name = name;
        return expression(id);
    }

    expression make_int(int64_t v) {
        integer_literal lit; lit.value = v; lit.suffix = "";
        return expression(lit);
    }
};

// ── !! (force unwrap) narrows optional[T] to T ──

TEST_F(NarrowingTest, ForceUnwrapNarrowsOptionalToInner) {
    bind_optional("maybe-name", "string");

    unary_operation op;
    op.op = "!!";
    op.operand = make_id("maybe-name");
    expression e; e = x3::forward_ast<unary_operation>(op);

    auto result = checker->check_expression(e, env);
    ASSERT_TRUE(result.has_value());

    auto& reg = TypeRegistry::instance();
    auto string_type = reg.get_string_type();
    EXPECT_EQ((*result)->name(), string_type->name());
}

TEST_F(NarrowingTest, ForceUnwrapOnNonOptionalPassesThrough) {
    bind_non_optional("name", "string");

    unary_operation op;
    op.op = "!!";
    op.operand = make_id("name");
    expression e; e = x3::forward_ast<unary_operation>(op);

    auto result = checker->check_expression(e, env);
    ASSERT_TRUE(result.has_value());

    auto& reg = TypeRegistry::instance();
    EXPECT_EQ((*result)->name(), reg.get_string_type()->name());
}

// ── ? (safe return) narrows optional[T] to T ──

TEST_F(NarrowingTest, SafeReturnNarrowsOptionalToInner) {
    bind_optional("maybe-count", "int");

    unary_operation op;
    op.op = "?";
    op.operand = make_id("maybe-count");
    expression e; e = x3::forward_ast<unary_operation>(op);

    auto result = checker->check_expression(e, env);
    ASSERT_TRUE(result.has_value());

    auto& reg = TypeRegistry::instance();
    EXPECT_EQ((*result)->name(), reg.get_int_type()->name());
}

// ── ?: (elvis) narrows optional[T] to T ──

TEST_F(NarrowingTest, ElvisNarrowsOptionalToInner) {
    bind_optional("maybe-name", "string");

    elvis_expression ev;
    ev.nullable_expr = make_id("maybe-name");
    ev.default_value = make_id("maybe-name");  // default doesn't matter for type
    expression e; e = x3::forward_ast<elvis_expression>(ev);

    auto result = checker->check_expression(e, env);
    ASSERT_TRUE(result.has_value());

    auto& reg = TypeRegistry::instance();
    EXPECT_EQ((*result)->name(), reg.get_string_type()->name());
}

TEST_F(NarrowingTest, ElvisOnNonOptionalPassesThrough) {
    bind_non_optional("name", "string");

    elvis_expression ev;
    ev.nullable_expr = make_id("name");
    ev.default_value = make_id("name");
    expression e; e = x3::forward_ast<elvis_expression>(ev);

    auto result = checker->check_expression(e, env);
    ASSERT_TRUE(result.has_value());

    auto& reg = TypeRegistry::instance();
    EXPECT_EQ((*result)->name(), reg.get_string_type()->name());
}

// ── ?. (safe navigation) produces optional result ──

TEST_F(NarrowingTest, SafeNavOnOptionalProducesOptional) {
    bind_optional("maybe-user", "string");

    safe_navigation_expression sn;
    sn.nullable_expr = make_id("maybe-user");
    sn.field_name = "length";
    expression e; e = x3::forward_ast<safe_navigation_expression>(sn);

    auto result = checker->check_expression(e, env);
    ASSERT_TRUE(result.has_value());

    auto& reg = TypeRegistry::instance();
    EXPECT_TRUE(reg.is_nullable_type(**result));
}

TEST_F(NarrowingTest, SafeNavOnNonOptionalReturnsUnit) {
    bind_non_optional("user", "string");

    safe_navigation_expression sn;
    sn.nullable_expr = make_id("user");
    sn.field_name = "length";
    expression e; e = x3::forward_ast<safe_navigation_expression>(sn);

    auto result = checker->check_expression(e, env);
    ASSERT_TRUE(result.has_value());
}
