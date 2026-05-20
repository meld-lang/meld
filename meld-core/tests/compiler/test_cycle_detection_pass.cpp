#include <gtest/gtest.h>
#include "meld/compiler/cycle_detection_pass.hpp"
#include "meld/compiler/intrinsic_resolution_pass.hpp"
#include "meld/std/mem.hpp"

using namespace meld::compiler;
using namespace meld::std_mem;

// ===========================================================================
// Helper: build AST nodes
// ===========================================================================

// Helper: build a type annotation (no generics)
static meld::parser::ast::type_annotation make_type(const std::string& name) {
    meld::parser::ast::type_annotation type;
    type.type_name.name = name;
    return type;
}

// Helper: build a generic type annotation like Hold[T]
static meld::parser::ast::type_annotation make_generic_type(
    const std::string& outer_name,
    std::vector<std::string> arg_names
) {
    meld::parser::ast::type_annotation type;
    type.type_name.name = outer_name;
    type.has_type_arguments = true;
    for (const auto& arg : arg_names) {
        type.type_arguments.push_back(
            boost::spirit::x3::forward_ast<meld::parser::ast::type_annotation>(
                make_type(arg)));
    }
    return type;
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

// ===========================================================================
// CycleDetectionPass — static helper tests
// ===========================================================================

TEST(ExtractOwnInnerTypeTest, RecognizesOwnWithTypeArg) {
    auto type = make_generic_type("Own", {"Node"});
    EXPECT_EQ(CycleDetectionPass::extract_own_inner_type(type), "Node");
}

TEST(ExtractOwnInnerTypeTest, RecognizesQualifiedOwn) {
    auto type = make_generic_type("std.mem.Own", {"Widget"});
    EXPECT_EQ(CycleDetectionPass::extract_own_inner_type(type), "Widget");
}

TEST(ExtractOwnInnerTypeTest, RecognizesMemOwn) {
    auto type = make_generic_type("mem.Own", {"Item"});
    EXPECT_EQ(CycleDetectionPass::extract_own_inner_type(type), "Item");
}

TEST(ExtractOwnInnerTypeTest, RejectsLinkType) {
    auto type = make_generic_type("Link", {"Node"});
    EXPECT_EQ(CycleDetectionPass::extract_own_inner_type(type), "");
}

TEST(ExtractOwnInnerTypeTest, RejectsOwnWithoutTypeArgs) {
    auto type = make_type("Own");
    EXPECT_EQ(CycleDetectionPass::extract_own_inner_type(type), "");
}

TEST(ExtractOwnInnerTypeTest, RejectsNonOwnGeneric) {
    auto type = make_generic_type("List", {"Node"});
    EXPECT_EQ(CycleDetectionPass::extract_own_inner_type(type), "");
}

TEST(ExtractOwnTargetsTest, ExtractsMultipleTargets) {
    std::vector<meld::parser::ast::field_declaration> fields = {
        make_field("child", make_generic_type("Own", {"Child"})),
        make_field("name", make_type("string")),
        make_field("sibling", make_generic_type("Own", {"Sibling"})),
        make_field("parent_ref", make_generic_type("Link", {"Parent"}))
    };

    auto targets = CycleDetectionPass::extract_own_targets(fields);
    EXPECT_EQ(targets.size(), 2u);
    EXPECT_TRUE(targets.count("Child"));
    EXPECT_TRUE(targets.count("Sibling"));
    EXPECT_FALSE(targets.count("Parent"));
}

TEST(ExtractOwnTargetsTest, EmptyFieldsYieldNoTargets) {
    std::vector<meld::parser::ast::field_declaration> fields;
    auto targets = CycleDetectionPass::extract_own_targets(fields);
    EXPECT_TRUE(targets.empty());
}

// ===========================================================================
// CycleDetectionPass — integration tests (Task 7.1)
// ===========================================================================

class CycleDetectionPassTest : public ::testing::Test {
protected:
    CycleDetectionPass pass;
};

TEST_F(CycleDetectionPassTest, EmptyProgramNoWarnings) {
    std::vector<meld::parser::ast::expression> exprs;
    auto result = pass.run(exprs, "test.meld");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.mutual_cycles_detected, 0u);
    EXPECT_EQ(result.classes_scanned, 0u);
}

TEST_F(CycleDetectionPassTest, MutualOwnEmitsW4001) {
    // class Parent { val child: Hold[Child] }
    // class Child  { val parent: Hold[Parent] }
    auto parent_cls = make_class_with_fields("Parent", {
        make_field("child", make_generic_type("Own", {"Child"}))
    });
    auto child_cls = make_class_with_fields("Child", {
        make_field("parent", make_generic_type("Own", {"Parent"}))
    });

    std::vector<meld::parser::ast::expression> exprs = {parent_cls, child_cls};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_TRUE(result.success);  // Warnings don't block compilation
    EXPECT_EQ(result.mutual_cycles_detected, 1u);
    EXPECT_EQ(result.classes_scanned, 2u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "W4001");
    EXPECT_EQ(result.diagnostics[0].level, CycleDetectionDiagnostic::Level::Warning);
    EXPECT_NE(result.diagnostics[0].message.find("Child"), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("Parent"), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("View[T]"), std::string::npos);
}

TEST_F(CycleDetectionPassTest, OwnAndLinkNoCycle) {
    // class Parent { val child: Hold[Child] }
    // class Child  { val parent: View[Parent] }
    auto parent_cls = make_class_with_fields("Parent", {
        make_field("child", make_generic_type("Own", {"Child"}))
    });
    auto child_cls = make_class_with_fields("Child", {
        make_field("parent", make_generic_type("Link", {"Parent"}))
    });

    std::vector<meld::parser::ast::expression> exprs = {parent_cls, child_cls};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.mutual_cycles_detected, 0u);
    EXPECT_EQ(result.classes_scanned, 2u);
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST_F(CycleDetectionPassTest, UnidirectionalOwnNoCycle) {
    // class A { val b: Hold[B] }
    // class B { val name: string }
    auto a_cls = make_class_with_fields("A", {
        make_field("b", make_generic_type("Own", {"B"}))
    });
    auto b_cls = make_class_with_fields("B", {
        make_field("name", make_type("string"))
    });

    std::vector<meld::parser::ast::expression> exprs = {a_cls, b_cls};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.mutual_cycles_detected, 0u);
}

TEST_F(CycleDetectionPassTest, QualifiedOwnMutualCycleDetected) {
    // class X { val y: std.mem.Hold[Y] }
    // class Y { val x: mem.Hold[X] }
    auto x_cls = make_class_with_fields("X", {
        make_field("y", make_generic_type("std.mem.Own", {"Y"}))
    });
    auto y_cls = make_class_with_fields("Y", {
        make_field("x", make_generic_type("mem.Own", {"X"}))
    });

    std::vector<meld::parser::ast::expression> exprs = {x_cls, y_cls};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_EQ(result.mutual_cycles_detected, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "W4001");
}

TEST_F(CycleDetectionPassTest, SingleClassNoSelfCycleCheck) {
    // class Solo { val name: string }
    auto solo = make_class_with_fields("Solo", {
        make_field("name", make_type("string"))
    });

    std::vector<meld::parser::ast::expression> exprs = {solo};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.mutual_cycles_detected, 0u);
    EXPECT_EQ(result.classes_scanned, 1u);
}

TEST_F(CycleDetectionPassTest, ThreeClassesOneMutualCycle) {
    // class A { val b: Hold[B] }
    // class B { val a: Hold[A], val c: Hold[C] }
    // class C { val name: string }
    // Only A↔B is a mutual cycle
    auto a_cls = make_class_with_fields("A", {
        make_field("b", make_generic_type("Own", {"B"}))
    });
    auto b_cls = make_class_with_fields("B", {
        make_field("a", make_generic_type("Own", {"A"})),
        make_field("c", make_generic_type("Own", {"C"}))
    });
    auto c_cls = make_class_with_fields("C", {
        make_field("name", make_type("string"))
    });

    std::vector<meld::parser::ast::expression> exprs = {a_cls, b_cls, c_cls};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_EQ(result.mutual_cycles_detected, 1u);
    EXPECT_EQ(result.classes_scanned, 3u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "W4001");
}

TEST_F(CycleDetectionPassTest, MultipleMutualCyclesDetected) {
    // class A { val b: Hold[B] }
    // class B { val a: Hold[A], val c: Hold[C] }
    // class C { val b: Hold[B] }
    // Cycles: A↔B and B↔C
    auto a_cls = make_class_with_fields("A", {
        make_field("b", make_generic_type("Own", {"B"}))
    });
    auto b_cls = make_class_with_fields("B", {
        make_field("a", make_generic_type("Own", {"A"})),
        make_field("c", make_generic_type("Own", {"C"}))
    });
    auto c_cls = make_class_with_fields("C", {
        make_field("b", make_generic_type("Own", {"B"}))
    });

    std::vector<meld::parser::ast::expression> exprs = {a_cls, b_cls, c_cls};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_EQ(result.mutual_cycles_detected, 2u);
    EXPECT_EQ(result.diagnostics.size(), 2u);
    for (const auto& diag : result.diagnostics) {
        EXPECT_EQ(diag.code, "W4001");
    }
}

TEST_F(CycleDetectionPassTest, ClassWithNoFieldsNoCycle) {
    // class Empty { }
    // class Other { val e: Hold[Empty] }
    auto empty_cls = make_class_with_fields("Empty", {});
    auto other_cls = make_class_with_fields("Other", {
        make_field("e", make_generic_type("Own", {"Empty"}))
    });

    std::vector<meld::parser::ast::expression> exprs = {empty_cls, other_cls};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_EQ(result.mutual_cycles_detected, 0u);
}

TEST_F(CycleDetectionPassTest, NoCycleDuplicateReporting) {
    // Ensure A↔B is reported exactly once, not twice
    auto a_cls = make_class_with_fields("A", {
        make_field("b", make_generic_type("Own", {"B"}))
    });
    auto b_cls = make_class_with_fields("B", {
        make_field("a", make_generic_type("Own", {"A"}))
    });

    std::vector<meld::parser::ast::expression> exprs = {a_cls, b_cls};
    auto result = pass.run(exprs, "test.meld");

    EXPECT_EQ(result.mutual_cycles_detected, 1u);
    EXPECT_EQ(result.diagnostics.size(), 1u);
}

// ===========================================================================
// Property-based tests (Task 7.2)
// ===========================================================================

#include <rapidcheck.h>

// ---------------------------------------------------------------------------
// Property 13: Mutual Hold[T] cycle detection
// Feature: hold-view-tenancy-model, Property 13: Mutual Hold[T] cycle detection
// **Validates: Requirements 7.4, 10.4**
//
// For any pair of class declarations where class A has a field of type
// Hold[B] and class B has a field of type Hold[A], the Semantic Analyzer
// shall emit warning diagnostic W4001.
// ---------------------------------------------------------------------------

TEST_F(CycleDetectionPassTest, Property13_MutualOwnCycleDetection) {
    // Feature: hold-view-tenancy-model, Property 13: Mutual Hold[T] cycle detection
    rc::check("Any pair of classes with mutual Hold[T] fields emits W4001",
        [this]() {
            // Generate two distinct class names
            auto name_a = *rc::gen::suchThat(
                rc::gen::string<std::string>(),
                [](const std::string& s) {
                    if (s.empty() || s.size() > 20) return false;
                    if (!std::isalpha(s[0]) || !std::isupper(s[0])) return false;
                    return std::all_of(s.begin(), s.end(),
                        [](char c) { return std::isalnum(c) || c == '_'; });
                });

            auto name_b = *rc::gen::suchThat(
                rc::gen::string<std::string>(),
                [&name_a](const std::string& s) {
                    if (s.empty() || s.size() > 20) return false;
                    if (!std::isalpha(s[0]) || !std::isupper(s[0])) return false;
                    if (s == name_a) return false;  // Must be distinct
                    return std::all_of(s.begin(), s.end(),
                        [](char c) { return std::isalnum(c) || c == '_'; });
                });

            // Pick a random Own variant for each direction
            auto own_variants = std::vector<std::string>{"Own", "std.mem.Own", "mem.Own"};
            auto idx_a = *rc::gen::inRange(0, static_cast<int>(own_variants.size()));
            auto idx_b = *rc::gen::inRange(0, static_cast<int>(own_variants.size()));

            // Build class A with Hold[B] field, class B with Hold[A] field
            auto cls_a = make_class_with_fields(name_a, {
                make_field("ref_b", make_generic_type(own_variants[idx_a], {name_b}))
            });
            auto cls_b = make_class_with_fields(name_b, {
                make_field("ref_a", make_generic_type(own_variants[idx_b], {name_a}))
            });

            std::vector<meld::parser::ast::expression> exprs = {cls_a, cls_b};
            auto result = pass.run(exprs, "test.meld");

            // Warnings don't block compilation
            RC_ASSERT(result.success);
            RC_ASSERT(result.mutual_cycles_detected >= 1u);
            RC_ASSERT(result.classes_scanned == 2u);
            RC_ASSERT(!result.diagnostics.empty());

            // Find the W4001 diagnostic mentioning both class names
            bool found_w4001 = false;
            for (const auto& diag : result.diagnostics) {
                if (diag.code == "W4001" &&
                    diag.message.find(name_a) != std::string::npos &&
                    diag.message.find(name_b) != std::string::npos) {
                    found_w4001 = true;
                    RC_ASSERT(diag.level == CycleDetectionDiagnostic::Level::Warning);
                    RC_ASSERT(diag.message.find("View[T]") != std::string::npos);
                }
            }
            RC_ASSERT(found_w4001);
        }
    );
}
