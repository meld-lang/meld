#include "meld/manifest/bridge_rule.hpp"

#include <gtest/gtest.h>

namespace meld::manifest {
namespace {

TEST(BridgeRuleTest, TaggedFfiAccepted) {
    BridgeRuleChecker checker;
    EffectBitmask effects;
    effects.set(Effect::Network);

    auto diag = checker.check_ffi_function("c_send", true, effects);
    EXPECT_FALSE(diag.has_value());  // No diagnostic for properly tagged FFI
}

TEST(BridgeRuleTest, UntaggedFfiDefaultsToFullIO) {
    BridgeRuleChecker checker;
    EffectBitmask empty;

    auto diag = checker.check_ffi_function("c_unknown", false, empty);
    ASSERT_TRUE(diag.has_value());
    EXPECT_TRUE(diag->is_warning);
    EXPECT_EQ(diag->symbol_name, "c_unknown");
}

TEST(BridgeRuleTest, CheckAllProducesEntries) {
    BridgeRuleChecker checker;

    EffectBitmask net;
    net.set(Effect::Network);

    std::vector<BridgeRuleChecker::FfiDeclaration> decls = {
        {"tagged_fn", true, net, {}},
        {"untagged_fn", false, {}, {}},
    };

    auto result = checker.check_all(decls);

    // Both should produce manifest entries
    EXPECT_EQ(result.entries.size(), 2u);

    // Only the untagged one should produce a diagnostic
    EXPECT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].symbol_name, "untagged_fn");
    EXPECT_TRUE(result.diagnostics[0].is_warning);
}

TEST(BridgeRuleTest, TaggedEntryHasDeclaredEffects) {
    BridgeRuleChecker checker;

    EffectBitmask effects;
    effects.set(Effect::FileSystemRead);
    effects.set(Effect::FileSystemWrite);

    std::vector<BridgeRuleChecker::FfiDeclaration> decls = {
        {"c_file_op", true, effects, {{"example.com"}, {"/data"}, {"/out"}, {}}},
    };

    auto result = checker.check_all(decls);
    ASSERT_EQ(result.entries.size(), 1u);
    EXPECT_EQ(result.entries[0].symbol_name, "c_file_op");
    EXPECT_TRUE(result.entries[0].effects.has(Effect::FileSystemRead));
    EXPECT_TRUE(result.entries[0].effects.has(Effect::FileSystemWrite));
    EXPECT_TRUE(result.entries[0].is_ffi);
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST(BridgeRuleTest, UntaggedEntryHasFullIO) {
    BridgeRuleChecker checker;

    std::vector<BridgeRuleChecker::FfiDeclaration> decls = {
        {"c_mystery", false, {}, {}},
    };

    auto result = checker.check_all(decls);
    ASSERT_EQ(result.entries.size(), 1u);

    // Untagged FFI should have all effects set (Full IO)
    auto& entry = result.entries[0];
    for (uint8_t i = 0; i < kEffectCount; ++i) {
        EXPECT_TRUE(entry.effects.has(static_cast<Effect>(i)))
            << "Missing effect: " << effect_to_string(static_cast<Effect>(i));
    }
    EXPECT_TRUE(entry.is_ffi);
}

TEST(BridgeRuleTest, EmptyDeclarations) {
    BridgeRuleChecker checker;
    auto result = checker.check_all({});
    EXPECT_TRUE(result.entries.empty());
    EXPECT_TRUE(result.diagnostics.empty());
}

}  // namespace
}  // namespace meld::manifest
