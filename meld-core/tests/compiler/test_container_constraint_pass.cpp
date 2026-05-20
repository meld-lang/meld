#include <gtest/gtest.h>
#include "meld/compiler/container_constraint_pass.hpp"
#include "meld/compiler/intrinsic_resolution_pass.hpp"
#include "meld/std/mem.hpp"

using namespace meld::compiler;
using namespace meld::std_mem;

// ===========================================================================
// Helper: build a seeded IntrinsicResolutionRegistry
// ===========================================================================

static IntrinsicResolutionRegistry make_seeded_registry() {
    IntrinsicResolutionPass ir_pass;
    std::vector<meld::parser::ast::expression> empty;
    auto result = ir_pass.run(empty);
    return std::move(result.registry);
}

// Helper: build a type annotation (no generics)
static meld::parser::ast::type_annotation make_type(const std::string& name) {
    meld::parser::ast::type_annotation type;
    type.type_name.name = name;
    return type;
}

// Helper: build a generic type annotation like vec[T]
static meld::parser::ast::type_annotation make_generic_type(
    const std::string& container_name,
    std::vector<std::string> arg_names
) {
    meld::parser::ast::type_annotation type;
    type.type_name.name = container_name;
    type.has_type_arguments = true;
    for (const auto& arg : arg_names) {
        type.type_arguments.push_back(
            boost::spirit::x3::forward_ast<meld::parser::ast::type_annotation>(
                make_type(arg)));
    }
    return type;
}

// Helper: build a function parameter with type annotation
static meld::parser::ast::function_parameter make_param(
    const std::string& name,
    meld::parser::ast::type_annotation type
) {
    meld::parser::ast::function_parameter param;
    param.name.name = name;
    param.type = std::move(type);
    return param;
}

// Helper: build a function definition with typed parameters and empty body
static meld::parser::ast::expression make_function_with_params(
    const std::string& fname,
    std::vector<meld::parser::ast::function_parameter> params
) {
    meld::parser::ast::function_definition func;
    func.name.name = fname;
    func.parameters = std::move(params);
    meld::parser::ast::block_expression body;
    func.body = boost::spirit::x3::forward_ast<meld::parser::ast::block_expression>(
        std::move(body));
    return meld::parser::ast::expression(
        boost::spirit::x3::forward_ast<meld::parser::ast::function_definition>(
            std::move(func)));
}

// Helper: build a class definition with fields
static meld::parser::ast::expression make_class_with_fields(
    const std::string& class_name,
    std::vector<meld::parser::ast::field_declaration> fields
) {
    meld::parser::ast::class_definition cls;
    cls.name.name = class_name;
    cls.fields = std::move(fields);
    meld::parser::ast::block_expression body;
    cls.body = boost::spirit::x3::forward_ast<meld::parser::ast::block_expression>(
        std::move(body));
    return meld::parser::ast::expression(
        boost::spirit::x3::forward_ast<meld::parser::ast::class_definition>(
            std::move(cls)));
}

// Helper: build a field declaration
static meld::parser::ast::field_declaration make_field(
    const std::string& name,
    meld::parser::ast::type_annotation type
) {
    meld::parser::ast::field_declaration field;
    field.name.name = name;
    field.type = std::move(type);
    field.is_mutable = false;
    return field;
}

// ===========================================================================
// ContainerConstraintPass — static helper tests
// ===========================================================================

TEST(IsManagedContainerTest, RecognizesList) {
    auto registry = make_seeded_registry();
    EXPECT_TRUE(ContainerConstraintPass::is_managed_container("List", registry));
}

TEST(IsManagedContainerTest, RecognizesMap) {
    auto registry = make_seeded_registry();
    EXPECT_TRUE(ContainerConstraintPass::is_managed_container("Map", registry));
}

TEST(IsManagedContainerTest, RecognizesSet) {
    auto registry = make_seeded_registry();
    EXPECT_TRUE(ContainerConstraintPass::is_managed_container("Set", registry));
}

TEST(IsManagedContainerTest, RecognizesQueue) {
    auto registry = make_seeded_registry();
    EXPECT_TRUE(ContainerConstraintPass::is_managed_container("Queue", registry));
}

TEST(IsManagedContainerTest, RejectsNonContainer) {
    auto registry = make_seeded_registry();
    EXPECT_FALSE(ContainerConstraintPass::is_managed_container("string", registry));
    EXPECT_FALSE(ContainerConstraintPass::is_managed_container("MyClass", registry));
}

TEST(IsStorableTypeTest, RecognizesOwn) {
    EXPECT_TRUE(ContainerConstraintPass::is_storable_type("Own"));
    EXPECT_TRUE(ContainerConstraintPass::is_storable_type("std.mem.Own"));
    EXPECT_TRUE(ContainerConstraintPass::is_storable_type("mem.Own"));
}

TEST(IsStorableTypeTest, RecognizesLink) {
    EXPECT_TRUE(ContainerConstraintPass::is_storable_type("Link"));
    EXPECT_TRUE(ContainerConstraintPass::is_storable_type("std.mem.Link"));
    EXPECT_TRUE(ContainerConstraintPass::is_storable_type("mem.Link"));
}

TEST(IsStorableTypeTest, RecognizesHold) {
    EXPECT_TRUE(ContainerConstraintPass::is_storable_type("Hold"));
    EXPECT_TRUE(ContainerConstraintPass::is_storable_type("std.mem.Hold"));
    EXPECT_TRUE(ContainerConstraintPass::is_storable_type("mem.Hold"));
}

TEST(IsStorableTypeTest, RecognizesView) {
    EXPECT_TRUE(ContainerConstraintPass::is_storable_type("View"));
    EXPECT_TRUE(ContainerConstraintPass::is_storable_type("std.mem.View"));
    EXPECT_TRUE(ContainerConstraintPass::is_storable_type("mem.View"));
}

TEST(IsStorableTypeTest, RejectsRawTypes) {
    EXPECT_FALSE(ContainerConstraintPass::is_storable_type("User"));
    EXPECT_FALSE(ContainerConstraintPass::is_storable_type("Node"));
    EXPECT_FALSE(ContainerConstraintPass::is_storable_type("string"));
}

// ===========================================================================
// ContainerConstraintPass — integration tests (Task 6.1)
// ===========================================================================

class ContainerConstraintPassTest : public ::testing::Test {
protected:
    ContainerConstraintPass pass;
    IntrinsicResolutionRegistry registry;

    void SetUp() override {
        registry = make_seeded_registry();
    }
};

TEST_F(ContainerConstraintPassTest, EmptyProgramNoErrors) {
    std::vector<meld::parser::ast::expression> exprs;
    auto result = pass.run(exprs, registry, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.raw_type_errors, 0u);
}

TEST_F(ContainerConstraintPassTest, RawTypeInListEmitsE4003) {
    // fnc test(items: List[User]) { }
    auto func = make_function_with_params("test", {
        make_param("items", make_generic_type("List", {"User"}))
    });

    std::vector<meld::parser::ast::expression> exprs = {func};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.raw_type_errors, 1u);
    EXPECT_EQ(result.container_types_checked, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4003");
    EXPECT_NE(result.diagnostics[0].message.find("User"), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("Hold[User]"), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("View[User]"), std::string::npos);
}

TEST_F(ContainerConstraintPassTest, OwnTypeInListIsOk) {
    // fnc test(items: List[Own]) { }
    auto func = make_function_with_params("test", {
        make_param("items", make_generic_type("List", {"Own"}))
    });

    std::vector<meld::parser::ast::expression> exprs = {func};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.raw_type_errors, 0u);
    EXPECT_EQ(result.container_types_checked, 1u);
}

TEST_F(ContainerConstraintPassTest, LinkTypeInListIsOk) {
    // fnc test(items: List[Link]) { }
    auto func = make_function_with_params("test", {
        make_param("items", make_generic_type("List", {"Link"}))
    });

    std::vector<meld::parser::ast::expression> exprs = {func};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.raw_type_errors, 0u);
}

TEST_F(ContainerConstraintPassTest, QualifiedOwnInListIsOk) {
    // fnc test(items: List[std.mem.Own]) { }
    auto func = make_function_with_params("test", {
        make_param("items", make_generic_type("List", {"std.mem.Own"}))
    });

    std::vector<meld::parser::ast::expression> exprs = {func};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.raw_type_errors, 0u);
}

TEST_F(ContainerConstraintPassTest, RawTypeInMapEmitsE4003) {
    // fnc test(m: Map[string, User]) { }
    // Both type args are checked — string is raw, User is raw
    auto func = make_function_with_params("test", {
        make_param("m", make_generic_type("Map", {"string", "User"}))
    });

    std::vector<meld::parser::ast::expression> exprs = {func};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_FALSE(result.success);
    EXPECT_GE(result.raw_type_errors, 1u);
    EXPECT_EQ(result.container_types_checked, 1u);

    // At least one diagnostic should mention "User"
    bool found_user = false;
    for (const auto& diag : result.diagnostics) {
        if (diag.message.find("User") != std::string::npos) {
            found_user = true;
            EXPECT_EQ(diag.code, "E4003");
        }
    }
    EXPECT_TRUE(found_user);
}

TEST_F(ContainerConstraintPassTest, RawTypeInSetEmitsE4003) {
    // fnc test(s: Set[Node]) { }
    auto func = make_function_with_params("test", {
        make_param("s", make_generic_type("Set", {"Node"}))
    });

    std::vector<meld::parser::ast::expression> exprs = {func};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.raw_type_errors, 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4003");
    EXPECT_NE(result.diagnostics[0].message.find("Node"), std::string::npos);
}

TEST_F(ContainerConstraintPassTest, NonManagedContainerIsIgnored) {
    // fnc test(items: Array[User]) { }
    // "Array" is not a managed container — no error
    auto func = make_function_with_params("test", {
        make_param("items", make_generic_type("Array", {"User"}))
    });

    std::vector<meld::parser::ast::expression> exprs = {func};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.raw_type_errors, 0u);
    EXPECT_EQ(result.container_types_checked, 0u);
}

TEST_F(ContainerConstraintPassTest, ClassFieldWithRawTypeInListEmitsE4003) {
    // class MyClass {
    //   val items: List[User]
    // }
    auto cls = make_class_with_fields("MyClass", {
        make_field("items", make_generic_type("List", {"User"}))
    });

    std::vector<meld::parser::ast::expression> exprs = {cls};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.raw_type_errors, 1u);
    EXPECT_EQ(result.diagnostics[0].code, "E4003");
}

TEST_F(ContainerConstraintPassTest, ClassFieldWithOwnInListIsOk) {
    // class MyClass {
    //   val items: List[Own]
    // }
    auto cls = make_class_with_fields("MyClass", {
        make_field("items", make_generic_type("List", {"Own"}))
    });

    std::vector<meld::parser::ast::expression> exprs = {cls};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.raw_type_errors, 0u);
}

TEST_F(ContainerConstraintPassTest, MultipleFunctionsCheckedIndependently) {
    // fnc f1(a: List[User]) { }   // E4003
    // fnc f2(b: List[Own]) { }    // OK
    auto f1 = make_function_with_params("f1", {
        make_param("a", make_generic_type("List", {"User"}))
    });
    auto f2 = make_function_with_params("f2", {
        make_param("b", make_generic_type("List", {"Own"}))
    });

    std::vector<meld::parser::ast::expression> exprs = {f1, f2};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.raw_type_errors, 1u);
    EXPECT_EQ(result.container_types_checked, 2u);
}

TEST_F(ContainerConstraintPassTest, MultipleRawTypesEmitMultipleE4003) {
    // fnc test(a: List[User], b: Set[Node]) { }
    auto func = make_function_with_params("test", {
        make_param("a", make_generic_type("List", {"User"})),
        make_param("b", make_generic_type("Set", {"Node"}))
    });

    std::vector<meld::parser::ast::expression> exprs = {func};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.raw_type_errors, 2u);
    EXPECT_EQ(result.diagnostics.size(), 2u);
}

TEST_F(ContainerConstraintPassTest, NonGenericContainerTypeIsIgnored) {
    // fnc test(v: List) { }
    // List without type arguments — no check
    auto func = make_function_with_params("test", {
        make_param("v", make_type("List"))
    });

    std::vector<meld::parser::ast::expression> exprs = {func};
    auto result = pass.run(exprs, registry, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.container_types_checked, 0u);
}

// ===========================================================================
// Property-based tests (Task 6.2)
// ===========================================================================

#include <rapidcheck.h>

// ---------------------------------------------------------------------------
// Property 9: Raw type in managed container rejection
// Feature: own-link-memory-model, Property 9: Raw type in managed container rejection
// **Validates: Requirements 5.3, 5.4, 7.3**
//
// For any raw class type T that does not implement the Storable trait,
// and for any collection type annotated with @intrinsic(managed_container),
// using T as the element type shall cause the Semantic Analyzer to emit
// error diagnostic E4003.
// ---------------------------------------------------------------------------

TEST_F(ContainerConstraintPassTest, Property9_RawTypeInManagedContainerRejection) {
    // Feature: own-link-memory-model, Property 9: Raw type in managed container rejection
    rc::check("Any raw class type in a managed container emits E4003",
        [this]() {
            // Generate a random raw class type name (not Own/Link)
            auto raw_type = *rc::gen::suchThat(
                rc::gen::string<std::string>(),
                [](const std::string& s) {
                    if (s.empty() || s.size() > 20) return false;
                    if (!std::isalpha(s[0]) || !std::isupper(s[0])) return false;
                    if (!std::all_of(s.begin(), s.end(),
                            [](char c) { return std::isalnum(c) || c == '_'; }))
                        return false;
                    // Exclude Storable type names
                    return !ContainerConstraintPass::is_storable_type(s);
                });

            // Pick a random managed container
            auto container_idx = *rc::gen::inRange(0, 4);
            std::string container_name;
            switch (container_idx) {
                case 0: container_name = "List"; break;
                case 1: container_name = "Map"; break;
                case 2: container_name = "Set"; break;
                case 3: container_name = "Queue"; break;
            }

            // Build: fnc test(param: <container>[<raw_type>]) { }
            auto func = make_function_with_params("test", {
                make_param("param", make_generic_type(container_name, {raw_type}))
            });

            std::vector<meld::parser::ast::expression> exprs = {func};
            auto result = pass.run(exprs, registry, "test.meld");

            RC_ASSERT(!result.success);
            RC_ASSERT(result.raw_type_errors >= 1u);
            RC_ASSERT(result.container_types_checked >= 1u);
            RC_ASSERT(!result.diagnostics.empty());
            RC_ASSERT(result.diagnostics[0].code == "E4003");
            RC_ASSERT(result.diagnostics[0].message.find(raw_type) != std::string::npos);
            RC_ASSERT(result.diagnostics[0].message.find("Hold[" + raw_type) != std::string::npos);
            RC_ASSERT(result.diagnostics[0].message.find("View[" + raw_type + "]") != std::string::npos);
        }
    );
}
