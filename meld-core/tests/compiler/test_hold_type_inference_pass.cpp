#include <gtest/gtest.h>
#include "meld/compiler/hold_type_inference_pass.hpp"
#include "meld/compiler/intrinsic_resolution_pass.hpp"
#include "meld/std/mem.hpp"
#include <rapidcheck.h>

using namespace meld::compiler;
using namespace meld::std_mem;

// ===========================================================================
// Helper: build AST nodes
// ===========================================================================

static meld::parser::ast::type_annotation make_type(const std::string& name) {
    meld::parser::ast::type_annotation type;
    type.type_name.name = name;
    return type;
}

static meld::parser::ast::field_declaration make_field(
    const std::string& name, meld::parser::ast::type_annotation type) {
    meld::parser::ast::field_declaration field;
    field.name.name = name;
    field.type = std::move(type);
    field.is_mutable = false;
    return field;
}

static meld::parser::ast::expression make_class_with_fields(
    const std::string& class_name,
    std::vector<meld::parser::ast::field_declaration> fields) {
    meld::parser::ast::class_definition cls;
    cls.name.name = class_name;
    cls.fields = std::move(fields);
    return meld::parser::ast::expression(
        boost::spirit::x3::forward_ast<meld::parser::ast::class_definition>(
            std::move(cls)));
}

static meld::parser::ast::expression make_constructor_call(
    const std::string& class_name) {
    meld::parser::ast::function_call call;
    call.function_name.name = class_name;
    return meld::parser::ast::expression(
        boost::spirit::x3::forward_ast<meld::parser::ast::function_call>(
            std::move(call)));
}


static meld::parser::ast::expression make_val_with_constructor(
    const std::string& val_name, const std::string& class_name) {
    meld::parser::ast::val_declaration decl;
    decl.name.name = val_name;
    decl.value = boost::spirit::x3::forward_ast<meld::parser::ast::expression>(
        make_constructor_call(class_name));
    return meld::parser::ast::expression(
        boost::spirit::x3::forward_ast<meld::parser::ast::val_declaration>(
            std::move(decl)));
}

static meld::parser::ast::expression make_var_with_constructor(
    const std::string& var_name, const std::string& class_name) {
    meld::parser::ast::var_declaration decl;
    decl.name.name = var_name;
    decl.value = boost::spirit::x3::forward_ast<meld::parser::ast::expression>(
        make_constructor_call(class_name));
    return meld::parser::ast::expression(
        boost::spirit::x3::forward_ast<meld::parser::ast::var_declaration>(
            std::move(decl)));
}

static meld::parser::ast::expression make_val_with_id(
    const std::string& val_name, const std::string& id_name) {
    meld::parser::ast::val_declaration decl;
    decl.name.name = val_name;
    meld::parser::ast::identifier id;
    id.name = id_name;
    decl.value = boost::spirit::x3::forward_ast<meld::parser::ast::expression>(
        meld::parser::ast::expression(id));
    return meld::parser::ast::expression(
        boost::spirit::x3::forward_ast<meld::parser::ast::val_declaration>(
            std::move(decl)));
}

static meld::parser::ast::expression make_function_with_body(
    const std::string& func_name,
    std::vector<meld::parser::ast::expression> body_stmts) {
    meld::parser::ast::function_definition func;
    func.name.name = func_name;
    meld::parser::ast::block_expression body;
    for (auto& stmt : body_stmts) {
        body.statements.push_back(
            boost::spirit::x3::forward_ast<meld::parser::ast::expression>(
                std::move(stmt)));
    }
    func.body = boost::spirit::x3::forward_ast<meld::parser::ast::block_expression>(
        std::move(body));
    return meld::parser::ast::expression(
        boost::spirit::x3::forward_ast<meld::parser::ast::function_definition>(
            std::move(func)));
}


static meld::parser::ast::function_parameter make_param(
    const std::string& param_name, const std::string& type_name) {
    meld::parser::ast::function_parameter param;
    param.name.name = param_name;
    param.type.type_name.name = type_name;
    return param;
}

static meld::parser::ast::expression make_function_with_params(
    const std::string& func_name,
    std::vector<meld::parser::ast::function_parameter> params,
    std::vector<meld::parser::ast::expression> body_stmts = {}) {
    meld::parser::ast::function_definition func;
    func.name.name = func_name;
    func.parameters = std::move(params);
    meld::parser::ast::block_expression body;
    for (auto& stmt : body_stmts) {
        body.statements.push_back(
            boost::spirit::x3::forward_ast<meld::parser::ast::expression>(
                std::move(stmt)));
    }
    func.body = boost::spirit::x3::forward_ast<meld::parser::ast::block_expression>(
        std::move(body));
    return meld::parser::ast::expression(
        boost::spirit::x3::forward_ast<meld::parser::ast::function_definition>(
            std::move(func)));
}

// ===========================================================================
// Test fixture
// ===========================================================================

class HoldTypeInferencePassTest : public ::testing::Test {
protected:
    HoldTypeInferencePass pass;
    IntrinsicResolutionRegistry registry;
};

// ===========================================================================
// Creator Rule unit tests -- Hold[T] inference
// ===========================================================================

TEST_F(HoldTypeInferencePassTest, EmptyProgramNoInferences) {
    std::vector<meld::parser::ast::expression> exprs;
    auto result = pass.run(exprs, registry, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.own_types_inferred, 0u);
    EXPECT_TRUE(result.inferences.empty());
    EXPECT_EQ(result.guest_types_inferred, 0u);
    EXPECT_TRUE(result.guest_rule_inferences.empty());
}


TEST_F(HoldTypeInferencePassTest, ValWithConstructorInfersHoldT) {
    auto user_cls = make_class_with_fields("User", {make_field("name", make_type("string"))});
    auto main_fn = make_function_with_body("main", {make_val_with_constructor("x", "User")});
    std::vector<meld::parser::ast::expression> exprs = {user_cls, main_fn};
    auto result = pass.run(exprs, registry, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.own_types_inferred, 1u);
    ASSERT_EQ(result.inferences.size(), 1u);
    EXPECT_EQ(result.inferences[0].binding_name, "x");
    EXPECT_EQ(result.inferences[0].class_name, "User");
    EXPECT_EQ(result.inferences[0].inferred_type, "Hold[User]");
}

TEST_F(HoldTypeInferencePassTest, VarWithConstructorInfersHoldT) {
    auto widget_cls = make_class_with_fields("Widget", {make_field("id", make_type("int"))});
    auto main_fn = make_function_with_body("main", {make_var_with_constructor("w", "Widget")});
    std::vector<meld::parser::ast::expression> exprs = {widget_cls, main_fn};
    auto result = pass.run(exprs, registry, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.own_types_inferred, 1u);
    ASSERT_EQ(result.inferences.size(), 1u);
    EXPECT_EQ(result.inferences[0].binding_name, "w");
    EXPECT_EQ(result.inferences[0].inferred_type, "Hold[Widget]");
}

TEST_F(HoldTypeInferencePassTest, ValWithIdentifierNoInference) {
    auto user_cls = make_class_with_fields("User", {make_field("name", make_type("string"))});
    auto main_fn = make_function_with_body("main", {make_val_with_id("y", "existing_ref")});
    std::vector<meld::parser::ast::expression> exprs = {user_cls, main_fn};
    auto result = pass.run(exprs, registry, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.own_types_inferred, 0u);
}

TEST_F(HoldTypeInferencePassTest, NonClassFunctionCallNoInference) {
    auto user_cls = make_class_with_fields("User", {make_field("name", make_type("string"))});
    meld::parser::ast::function_call call;
    call.function_name.name = "create_user";
    meld::parser::ast::val_declaration decl;
    decl.name.name = "x";
    decl.value = boost::spirit::x3::forward_ast<meld::parser::ast::expression>(
        meld::parser::ast::expression(
            boost::spirit::x3::forward_ast<meld::parser::ast::function_call>(std::move(call))));
    auto val_expr = meld::parser::ast::expression(
        boost::spirit::x3::forward_ast<meld::parser::ast::val_declaration>(std::move(decl)));
    auto main_fn = make_function_with_body("main", {std::move(val_expr)});
    std::vector<meld::parser::ast::expression> exprs = {user_cls, main_fn};
    auto result = pass.run(exprs, registry, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.own_types_inferred, 0u);
}


TEST_F(HoldTypeInferencePassTest, MultipleConstructorCallsInferMultiple) {
    auto a_cls = make_class_with_fields("A", {make_field("x", make_type("int"))});
    auto b_cls = make_class_with_fields("B", {make_field("y", make_type("string"))});
    auto main_fn = make_function_with_body("main", {
        make_val_with_constructor("a", "A"), make_val_with_constructor("b", "B")});
    std::vector<meld::parser::ast::expression> exprs = {a_cls, b_cls, main_fn};
    auto result = pass.run(exprs, registry, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.own_types_inferred, 2u);
    EXPECT_EQ(result.inferences.size(), 2u);
    bool found_a = false, found_b = false;
    for (const auto& inf : result.inferences) {
        if (inf.binding_name == "a" && inf.inferred_type == "Hold[A]") found_a = true;
        if (inf.binding_name == "b" && inf.inferred_type == "Hold[B]") found_b = true;
    }
    EXPECT_TRUE(found_a);
    EXPECT_TRUE(found_b);
}

TEST_F(HoldTypeInferencePassTest, InferredTypesMapPopulated) {
    auto cls = make_class_with_fields("Node", {make_field("id", make_type("int"))});
    auto main_fn = make_function_with_body("main", {make_val_with_constructor("n", "Node")});
    std::vector<meld::parser::ast::expression> exprs = {cls, main_fn};
    pass.run(exprs, registry, "test.meld");
    const auto& types = pass.inferred_types();
    ASSERT_EQ(types.size(), 1u);
    EXPECT_EQ(types.at("n"), "Hold[Node]");
}

TEST_F(HoldTypeInferencePassTest, IsConstructorCallStaticHelper) {
    std::unordered_set<std::string> classes = {"User", "Widget", "Node"};
    EXPECT_TRUE(HoldTypeInferencePass::is_constructor_call("User", classes));
    EXPECT_TRUE(HoldTypeInferencePass::is_constructor_call("Widget", classes));
    EXPECT_FALSE(HoldTypeInferencePass::is_constructor_call("create_user", classes));
    EXPECT_FALSE(HoldTypeInferencePass::is_constructor_call("println", classes));
}

// ===========================================================================
// Guest Rule unit tests -- View[T] inference for bare class parameters
// Requirements: 120.7
// ===========================================================================

TEST_F(HoldTypeInferencePassTest, GuestRule_BareClassParamInfersViewT) {
    auto user_cls = make_class_with_fields("User", {make_field("name", make_type("string"))});
    auto fn = make_function_with_params("rename", {make_param("user", "User")});
    std::vector<meld::parser::ast::expression> exprs = {user_cls, fn};
    auto result = pass.run(exprs, registry, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.guest_types_inferred, 1u);
    ASSERT_EQ(result.guest_rule_inferences.size(), 1u);
    EXPECT_EQ(result.guest_rule_inferences[0].parameter_name, "user");
    EXPECT_EQ(result.guest_rule_inferences[0].class_name, "User");
    EXPECT_EQ(result.guest_rule_inferences[0].inferred_type, "View[User]");
    EXPECT_EQ(result.guest_rule_inferences[0].function_name, "rename");
}


TEST_F(HoldTypeInferencePassTest, GuestRule_HoldParamNotReInferred) {
    auto user_cls = make_class_with_fields("User", {make_field("name", make_type("string"))});
    auto fn = make_function_with_params("take", {make_param("user", "Hold")});
    std::vector<meld::parser::ast::expression> exprs = {user_cls, fn};
    auto result = pass.run(exprs, registry, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.guest_types_inferred, 0u);
    EXPECT_TRUE(result.guest_rule_inferences.empty());
}

TEST_F(HoldTypeInferencePassTest, GuestRule_ViewParamNotReInferred) {
    auto user_cls = make_class_with_fields("User", {make_field("name", make_type("string"))});
    auto fn = make_function_with_params("observe", {make_param("user", "View")});
    std::vector<meld::parser::ast::expression> exprs = {user_cls, fn};
    auto result = pass.run(exprs, registry, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.guest_types_inferred, 0u);
    EXPECT_TRUE(result.guest_rule_inferences.empty());
}

TEST_F(HoldTypeInferencePassTest, GuestRule_IntParamNotInferred) {
    auto user_cls = make_class_with_fields("User", {make_field("name", make_type("string"))});
    auto fn = make_function_with_params("count", {make_param("x", "int")});
    std::vector<meld::parser::ast::expression> exprs = {user_cls, fn};
    auto result = pass.run(exprs, registry, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.guest_types_inferred, 0u);
    EXPECT_TRUE(result.guest_rule_inferences.empty());
}

TEST_F(HoldTypeInferencePassTest, GuestRule_StringParamNotInferred) {
    auto user_cls = make_class_with_fields("User", {make_field("name", make_type("string"))});
    auto fn = make_function_with_params("greet", {make_param("x", "string")});
    std::vector<meld::parser::ast::expression> exprs = {user_cls, fn};
    auto result = pass.run(exprs, registry, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.guest_types_inferred, 0u);
    EXPECT_TRUE(result.guest_rule_inferences.empty());
}

TEST_F(HoldTypeInferencePassTest, GuestRule_QualifiedHoldNotInferred) {
    auto user_cls = make_class_with_fields("User", {make_field("name", make_type("string"))});
    auto fn = make_function_with_params("take", {make_param("user", "std.mem.Hold")});
    std::vector<meld::parser::ast::expression> exprs = {user_cls, fn};
    auto result = pass.run(exprs, registry, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.guest_types_inferred, 0u);
}

TEST_F(HoldTypeInferencePassTest, GuestRule_QualifiedViewNotInferred) {
    auto user_cls = make_class_with_fields("User", {make_field("name", make_type("string"))});
    auto fn = make_function_with_params("observe", {make_param("user", "std.mem.View")});
    std::vector<meld::parser::ast::expression> exprs = {user_cls, fn};
    auto result = pass.run(exprs, registry, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.guest_types_inferred, 0u);
}


TEST_F(HoldTypeInferencePassTest, IsPrimitiveTypeStaticHelper) {
    EXPECT_TRUE(HoldTypeInferencePass::is_primitive_type("int"));
    EXPECT_TRUE(HoldTypeInferencePass::is_primitive_type("float"));
    EXPECT_TRUE(HoldTypeInferencePass::is_primitive_type("bool"));
    EXPECT_TRUE(HoldTypeInferencePass::is_primitive_type("string"));
    EXPECT_TRUE(HoldTypeInferencePass::is_primitive_type("nil"));
    EXPECT_FALSE(HoldTypeInferencePass::is_primitive_type("User"));
    EXPECT_FALSE(HoldTypeInferencePass::is_primitive_type("Widget"));
    EXPECT_FALSE(HoldTypeInferencePass::is_primitive_type("Hold"));
    EXPECT_FALSE(HoldTypeInferencePass::is_primitive_type("View"));
}

TEST_F(HoldTypeInferencePassTest, GuestRule_InferredTypesMapPopulated) {
    auto cls = make_class_with_fields("Node", {make_field("id", make_type("int"))});
    auto fn = make_function_with_params("process", {make_param("n", "Node")});
    std::vector<meld::parser::ast::expression> exprs = {cls, fn};
    pass.run(exprs, registry, "test.meld");
    const auto& types = pass.inferred_types();
    ASSERT_EQ(types.size(), 1u);
    EXPECT_EQ(types.at("n"), "View[Node]");
}

// ===========================================================================
// Property-based tests -- Creator Rule (Hold[T])
// **Validates: Requirements 119.5**
// =====================s======================================================

TEST_F(HoldTypeInferencePassTest, Property4_HoldTypeInference) {
    rc::check("Any class constructor assigned to val without wrapper infers Hold[T]",
        []() {
            auto class_name = *rc::gen::suchThat(
                rc::gen::string<std::string>(),
                [](const std::string& s) {
                    if (s.empty() || s.size() > 20) return false;
                    if (!std::isalpha(s[0]) || !std::isupper(s[0])) return false;
                    return std::all_of(s.begin(), s.end(),
                        [](char c) { return std::isalnum(c) || c == '_'; });
                });
            auto binding_name = *rc::gen::suchThat(
                rc::gen::string<std::string>(),
                [&class_name](const std::string& s) {
                    if (s.empty() || s.size() > 20) return false;
                    if (!std::isalpha(s[0]) || !std::islower(s[0])) return false;
                    if (s == class_name) return false;
                    return std::all_of(s.begin(), s.end(),
                        [](char c) { return std::isalnum(c) || c == '_'; });
                });
            auto num_fields = *rc::gen::inRange(0, 6);
            std::vector<meld::parser::ast::field_declaration> fields;
            for (int i = 0; i < num_fields; ++i) {
                fields.push_back(make_field(
                    "field_" + std::to_string(i), make_type("string")));
            }
            auto cls = make_class_with_fields(class_name, std::move(fields));
            auto use_var = *rc::gen::arbitrary<bool>();
            meld::parser::ast::expression decl_expr;
            if (use_var) {
                decl_expr = make_var_with_constructor(binding_name, class_name);
            } else {
                decl_expr = make_val_with_constructor(binding_name, class_name);
            }
            auto main_fn = make_function_with_body("main", {std::move(decl_expr)});
            HoldTypeInferencePass test_pass;
            IntrinsicResolutionRegistry test_registry;
            std::vector<meld::parser::ast::expression> exprs = {cls, main_fn};
            auto result = test_pass.run(exprs, test_registry, "test.meld");
            RC_ASSERT(result.success);
            RC_ASSERT(result.own_types_inferred == 1u);
            RC_ASSERT(result.inferences.size() == 1u);
            const auto& inference = result.inferences[0];
            RC_ASSERT(inference.binding_name == binding_name);
            RC_ASSERT(inference.class_name == class_name);
            RC_ASSERT(inference.inferred_type == "Hold[" + class_name + "]");
            const auto& types = test_pass.inferred_types();
            RC_ASSERT(types.count(binding_name) == 1u);
            RC_ASSERT(types.at(binding_name) == "Hold[" + class_name + "]");
        });
}
