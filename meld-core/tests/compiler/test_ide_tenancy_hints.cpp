#include <gtest/gtest.h>
#include "../../include/meld/compiler/ide_integration.hpp"
#include "../../include/meld/compiler/hold_type_inference_pass.hpp"

using namespace meld::compiler;

class IDETenancyHintsTest : public ::testing::Test {
protected:
    void SetUp() override {
        ide = std::make_unique<IDEIntegration>();
    }

    std::unique_ptr<IDEIntegration> ide;
};

// Task 65.2: Creator Rule inference produces a type hint with "Hold[User]"
// Requirement 175.2
TEST_F(IDETenancyHintsTest, CreatorRuleProducesHoldHint) {
    HoldTypeInferenceResult result;
    HoldTypeInference inf;
    inf.binding_name = "user";
    inf.class_name = "User";
    inf.inferred_type = "Hold[User]";
    inf.source_file = "test.meld";
    inf.line = 5;
    inf.column = 4;
    result.inferences.push_back(inf);

    auto hints = ide->generate_tenancy_inlay_hints(result);

    ASSERT_EQ(hints.size(), 1);
    EXPECT_EQ(hints[0].text, " : Hold[User]");
    EXPECT_EQ(hints[0].kind, InlayHintKind::Type);
    EXPECT_TRUE(hints[0].is_ghost_annotation);
    EXPECT_EQ(hints[0].position.line, 5);
    EXPECT_EQ(hints[0].position.column, 4);
}

// Task 65.3: Guest Rule inference produces a type hint with "View[User]"
// Requirement 175.3
TEST_F(IDETenancyHintsTest, GuestRuleProducesViewHint) {
    HoldTypeInferenceResult result;
    GuestRuleInference inf;
    inf.parameter_name = "user";
    inf.class_name = "User";
    inf.inferred_type = "View[User]";
    inf.function_name = "process";
    inf.source_file = "test.meld";
    inf.line = 10;
    inf.column = 12;
    result.guest_rule_inferences.push_back(inf);

    auto hints = ide->generate_tenancy_inlay_hints(result);

    ASSERT_EQ(hints.size(), 1);
    EXPECT_EQ(hints[0].text, " as View[User]");
    EXPECT_EQ(hints[0].kind, InlayHintKind::Type);
    EXPECT_TRUE(hints[0].is_ghost_annotation);
    EXPECT_EQ(hints[0].position.line, 10);
    EXPECT_EQ(hints[0].position.column, 12);
}

// Task 65.1-65.4: Both Creator and Guest Rule hints together
// Requirements 175.1-175.4
TEST_F(IDETenancyHintsTest, BothRulesProduceGhostHints) {
    HoldTypeInferenceResult result;

    HoldTypeInference creator;
    creator.binding_name = "user";
    creator.class_name = "User";
    creator.inferred_type = "Hold[User]";
    creator.source_file = "test.meld";
    creator.line = 1;
    creator.column = 4;
    result.inferences.push_back(creator);

    GuestRuleInference guest;
    guest.parameter_name = "u";
    guest.class_name = "User";
    guest.inferred_type = "View[User]";
    guest.function_name = "process";
    guest.source_file = "test.meld";
    guest.line = 5;
    guest.column = 14;
    result.guest_rule_inferences.push_back(guest);

    auto hints = ide->generate_tenancy_inlay_hints(result);

    ASSERT_EQ(hints.size(), 2);

    // Creator Rule hint
    EXPECT_EQ(hints[0].text, " : Hold[User]");
    EXPECT_EQ(hints[0].kind, InlayHintKind::Type);
    EXPECT_TRUE(hints[0].is_ghost_annotation);

    // Guest Rule hint
    EXPECT_EQ(hints[1].text, " as View[User]");
    EXPECT_EQ(hints[1].kind, InlayHintKind::Type);
    EXPECT_TRUE(hints[1].is_ghost_annotation);
}

// Empty inference result produces no hints
TEST_F(IDETenancyHintsTest, EmptyResultProducesNoHints) {
    HoldTypeInferenceResult result;
    auto hints = ide->generate_tenancy_inlay_hints(result);
    EXPECT_TRUE(hints.empty());
}
