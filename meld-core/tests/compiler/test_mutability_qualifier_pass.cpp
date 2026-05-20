#include <gtest/gtest.h>
#include "meld/compiler/mutability_qualifier_pass.hpp"

using namespace meld::compiler;
using MQ = meld::parser::ast::type_annotation::MutabilityQualifier;

// ===========================================================================
// Helper: build AST nodes
// ===========================================================================

/// Create a simple type_annotation with no type arguments.
static meld::parser::ast::type_annotation make_type(const std::string& name) {
    meld::parser::ast::type_annotation type;
    type.type_name.name = name;
    return type;
}

/// Create a type_annotation with type arguments, e.g., Hold[User].
/// Each type argument gets the specified mutability qualifier.
static meld::parser::ast::type_annotation make_generic_type(
    const std::string& name,
    std::vector<std::pair<std::string, MQ>> args
) {
    meld::parser::ast::type_annotation type;
    type.type_name.name = name;
    type.has_type_arguments = true;
    for (const auto& [arg_name, mq] : args) {
        meld::parser::ast::type_annotation arg;
        arg.type_name.name = arg_name;
        arg.mutability_qualifier = mq;
        type.type_arguments.push_back(
            boost::spirit::x3::forward_ast<meld::parser::ast::type_annotation>(
                std::move(arg)));
    }
    return type;
}

/// Create a nested generic type, e.g., View[Map[K, V]].
/// outer_name: "View", inner is a full type_annotation for the inner generic.
static meld::parser::ast::type_annotation make_nested_generic(
    const std::string& outer_name,
    MQ outer_arg_mq,
    meld::parser::ast::type_annotation inner
) {
    inner.mutability_qualifier = outer_arg_mq;
    meld::parser::ast::type_annotation outer;
    outer.type_name.name = outer_name;
    outer.has_type_arguments = true;
    outer.type_arguments.push_back(
        boost::spirit::x3::forward_ast<meld::parser::ast::type_annotation>(
            std::move(inner)));
    return outer;
}

/// Wrap a val_declaration with a type annotation into an expression.
static meld::parser::ast::expression make_val_with_type(
    const std::string& val_name,
    meld::parser::ast::type_annotation type_ann
) {
    meld::parser::ast::val_declaration decl;
    decl.name.name = val_name;
    decl.has_type_annotation = true;
    decl.type_ann = boost::spirit::x3::forward_ast<meld::parser::ast::type_annotation>(
        std::move(type_ann));
    // Provide a dummy value (identifier)
    meld::parser::ast::identifier id;
    id.name = "dummy";
    decl.value = boost::spirit::x3::forward_ast<meld::parser::ast::expression>(
        meld::parser::ast::expression(id));
    return meld::parser::ast::expression(
        boost::spirit::x3::forward_ast<meld::parser::ast::val_declaration>(
            std::move(decl)));
}

/// Wrap a var_declaration with a type annotation into an expression.
static meld::parser::ast::expression make_var_with_type(
    const std::string& var_name,
    meld::parser::ast::type_annotation type_ann
) {
    meld::parser::ast::var_declaration decl;
    decl.name.name = var_name;
    decl.has_type_annotation = true;
    decl.type_ann = boost::spirit::x3::forward_ast<meld::parser::ast::type_annotation>(
        std::move(type_ann));
    meld::parser::ast::identifier id;
    id.name = "dummy";
    decl.value = boost::spirit::x3::forward_ast<meld::parser::ast::expression>(
        meld::parser::ast::expression(id));
    return meld::parser::ast::expression(
        boost::spirit::x3::forward_ast<meld::parser::ast::var_declaration>(
            std::move(decl)));
}

/// Create a function definition with parameters and body statements.
static meld::parser::ast::expression make_function_with_param_type(
    const std::string& func_name,
    const std::string& param_name,
    meld::parser::ast::type_annotation param_type
) {
    meld::parser::ast::function_definition func;
    func.name.name = func_name;
    meld::parser::ast::function_parameter param;
    param.name.name = param_name;
    param.type = std::move(param_type);
    func.parameters.push_back(std::move(param));
    meld::parser::ast::block_expression body;
    func.body = boost::spirit::x3::forward_ast<meld::parser::ast::block_expression>(
        std::move(body));
    return meld::parser::ast::expression(
        boost::spirit::x3::forward_ast<meld::parser::ast::function_definition>(
            std::move(func)));
}

/// Create a class definition with fields.
static meld::parser::ast::expression make_class_with_field_type(
    const std::string& class_name,
    const std::string& field_name,
    meld::parser::ast::type_annotation field_type
) {
    meld::parser::ast::class_definition cls;
    cls.name.name = class_name;
    meld::parser::ast::field_declaration field;
    field.name.name = field_name;
    field.type = std::move(field_type);
    field.is_mutable = false;
    cls.fields.push_back(std::move(field));
    return meld::parser::ast::expression(
        boost::spirit::x3::forward_ast<meld::parser::ast::class_definition>(
            std::move(cls)));
}

// ===========================================================================
// Test fixture
// ===========================================================================

class MutabilityQualifierPassTest : public ::testing::Test {
protected:
    MutabilityQualifierPass pass;
};

// ===========================================================================
// Tests
// ===========================================================================

// Hold[User] → type arg gets resolved to VAL
TEST_F(MutabilityQualifierPassTest, NoneDefaultsToVal) {
    auto type = make_generic_type("Hold", {{"User", MQ::NONE}});
    std::vector<meld::parser::ast::expression> exprs;
    exprs.push_back(make_val_with_type("x", std::move(type)));

    auto result = pass.run(exprs, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.defaults_applied, 1u);
    EXPECT_EQ(result.type_args_scanned, 1u);

    // Verify the AST was mutated in place
    auto& val_decl = boost::get<
        boost::spirit::x3::forward_ast<meld::parser::ast::val_declaration>>(exprs[0]);
    auto& resolved_type = val_decl.get().type_ann.get();
    ASSERT_TRUE(resolved_type.has_type_arguments);
    EXPECT_EQ(resolved_type.type_arguments[0].get().mutability_qualifier, MQ::VAL);
}

// Hold[val User] → stays VAL (no change)
TEST_F(MutabilityQualifierPassTest, ExplicitValStaysVal) {
    auto type = make_generic_type("Hold", {{"User", MQ::VAL}});
    std::vector<meld::parser::ast::expression> exprs;
    exprs.push_back(make_val_with_type("x", std::move(type)));

    auto result = pass.run(exprs, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.defaults_applied, 0u);
    EXPECT_EQ(result.type_args_scanned, 1u);

    auto& val_decl = boost::get<
        boost::spirit::x3::forward_ast<meld::parser::ast::val_declaration>>(exprs[0]);
    auto& resolved_type = val_decl.get().type_ann.get();
    EXPECT_EQ(resolved_type.type_arguments[0].get().mutability_qualifier, MQ::VAL);
}

// Hold[var User] → stays VAR (no change)
TEST_F(MutabilityQualifierPassTest, ExplicitVarStaysVar) {
    auto type = make_generic_type("Hold", {{"User", MQ::VAR}});
    std::vector<meld::parser::ast::expression> exprs;
    exprs.push_back(make_val_with_type("x", std::move(type)));

    auto result = pass.run(exprs, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.defaults_applied, 0u);

    auto& val_decl = boost::get<
        boost::spirit::x3::forward_ast<meld::parser::ast::val_declaration>>(exprs[0]);
    auto& resolved_type = val_decl.get().type_ann.get();
    EXPECT_EQ(resolved_type.type_arguments[0].get().mutability_qualifier, MQ::VAR);
}

// Map[K, V] → both args get resolved to VAL
TEST_F(MutabilityQualifierPassTest, MultipleArgsAllDefaultToVal) {
    auto type = make_generic_type("Map", {{"K", MQ::NONE}, {"V", MQ::NONE}});
    std::vector<meld::parser::ast::expression> exprs;
    exprs.push_back(make_val_with_type("m", std::move(type)));

    auto result = pass.run(exprs, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.defaults_applied, 2u);
    EXPECT_EQ(result.type_args_scanned, 2u);

    auto& val_decl = boost::get<
        boost::spirit::x3::forward_ast<meld::parser::ast::val_declaration>>(exprs[0]);
    auto& resolved_type = val_decl.get().type_ann.get();
    EXPECT_EQ(resolved_type.type_arguments[0].get().mutability_qualifier, MQ::VAL);
    EXPECT_EQ(resolved_type.type_arguments[1].get().mutability_qualifier, MQ::VAL);
}

// Map[val K, var V] → K stays VAL, V stays VAR
TEST_F(MutabilityQualifierPassTest, MixedExplicitQualifiersUnchanged) {
    auto type = make_generic_type("Map", {{"K", MQ::VAL}, {"V", MQ::VAR}});
    std::vector<meld::parser::ast::expression> exprs;
    exprs.push_back(make_val_with_type("m", std::move(type)));

    auto result = pass.run(exprs, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.defaults_applied, 0u);

    auto& val_decl = boost::get<
        boost::spirit::x3::forward_ast<meld::parser::ast::val_declaration>>(exprs[0]);
    auto& resolved_type = val_decl.get().type_ann.get();
    EXPECT_EQ(resolved_type.type_arguments[0].get().mutability_qualifier, MQ::VAL);
    EXPECT_EQ(resolved_type.type_arguments[1].get().mutability_qualifier, MQ::VAR);
}

// Nested: View[Map[K, V]] → outer arg resolved to VAL, inner args resolved to VAL
TEST_F(MutabilityQualifierPassTest, NestedGenericArgsResolved) {
    // Build Map[K, V] with NONE qualifiers
    auto inner = make_generic_type("Map", {{"K", MQ::NONE}, {"V", MQ::NONE}});
    // Wrap in View[Map[K, V]] with NONE qualifier on the outer arg
    auto outer = make_nested_generic("View", MQ::NONE, std::move(inner));

    std::vector<meld::parser::ast::expression> exprs;
    exprs.push_back(make_val_with_type("v", std::move(outer)));

    auto result = pass.run(exprs, "test.meld");

    EXPECT_TRUE(result.success);
    // 3 defaults: outer Map arg (NONE→VAL) + inner K (NONE→VAL) + inner V (NONE→VAL)
    EXPECT_EQ(result.defaults_applied, 3u);

    auto& val_decl = boost::get<
        boost::spirit::x3::forward_ast<meld::parser::ast::val_declaration>>(exprs[0]);
    auto& resolved_outer = val_decl.get().type_ann.get();

    // Outer type arg (Map[K, V]) should be VAL
    ASSERT_EQ(resolved_outer.type_arguments.size(), 1u);
    auto& map_arg = resolved_outer.type_arguments[0].get();
    EXPECT_EQ(map_arg.mutability_qualifier, MQ::VAL);

    // Inner type args (K, V) should both be VAL
    ASSERT_TRUE(map_arg.has_type_arguments);
    ASSERT_EQ(map_arg.type_arguments.size(), 2u);
    EXPECT_EQ(map_arg.type_arguments[0].get().mutability_qualifier, MQ::VAL);
    EXPECT_EQ(map_arg.type_arguments[1].get().mutability_qualifier, MQ::VAL);
}

// resolve_defaults static method works directly on a type_annotation
TEST_F(MutabilityQualifierPassTest, ResolveDefaultsStaticMethod) {
    auto type = make_generic_type("Hold", {{"User", MQ::NONE}});
    size_t applied = MutabilityQualifierPass::resolve_defaults(type);

    EXPECT_EQ(applied, 1u);
    EXPECT_EQ(type.type_arguments[0].get().mutability_qualifier, MQ::VAL);
}

// No type arguments → no changes
TEST_F(MutabilityQualifierPassTest, NoTypeArgsNoChanges) {
    auto type = make_type("User");
    size_t applied = MutabilityQualifierPass::resolve_defaults(type);

    EXPECT_EQ(applied, 0u);
}

// Empty program → success with zero counts
TEST_F(MutabilityQualifierPassTest, EmptyProgramNoChanges) {
    std::vector<meld::parser::ast::expression> exprs;
    auto result = pass.run(exprs, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.type_args_scanned, 0u);
    EXPECT_EQ(result.defaults_applied, 0u);
}

// var declaration type annotations are also resolved
TEST_F(MutabilityQualifierPassTest, VarDeclarationResolved) {
    auto type = make_generic_type("Hold", {{"User", MQ::NONE}});
    std::vector<meld::parser::ast::expression> exprs;
    exprs.push_back(make_var_with_type("x", std::move(type)));

    auto result = pass.run(exprs, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.defaults_applied, 1u);

    auto& var_decl = boost::get<
        boost::spirit::x3::forward_ast<meld::parser::ast::var_declaration>>(exprs[0]);
    auto& resolved_type = var_decl.get().type_ann.get();
    EXPECT_EQ(resolved_type.type_arguments[0].get().mutability_qualifier, MQ::VAL);
}

// Function parameter types are resolved
TEST_F(MutabilityQualifierPassTest, FunctionParameterTypeResolved) {
    auto param_type = make_generic_type("Hold", {{"User", MQ::NONE}});
    std::vector<meld::parser::ast::expression> exprs;
    exprs.push_back(make_function_with_param_type("process", "item", std::move(param_type)));

    auto result = pass.run(exprs, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.defaults_applied, 1u);

    auto& func = boost::get<
        boost::spirit::x3::forward_ast<meld::parser::ast::function_definition>>(exprs[0]);
    auto& resolved_param_type = func.get().parameters[0].type;
    EXPECT_EQ(resolved_param_type.type_arguments[0].get().mutability_qualifier, MQ::VAL);
}

// Class field types are resolved
TEST_F(MutabilityQualifierPassTest, ClassFieldTypeResolved) {
    auto field_type = make_generic_type("Hold", {{"User", MQ::NONE}});
    std::vector<meld::parser::ast::expression> exprs;
    exprs.push_back(make_class_with_field_type("Container", "item", std::move(field_type)));

    auto result = pass.run(exprs, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.defaults_applied, 1u);

    auto& cls = boost::get<
        boost::spirit::x3::forward_ast<meld::parser::ast::class_definition>>(exprs[0]);
    auto& resolved_field_type = cls.get().fields[0].type;
    EXPECT_EQ(resolved_field_type.type_arguments[0].get().mutability_qualifier, MQ::VAL);
}
