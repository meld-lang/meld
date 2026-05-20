#include "meld/manifest/effect_elision.hpp"

#include <gtest/gtest.h>

namespace meld::manifest {
namespace {

Manifest make_manifest_with_symbols() {
    Manifest m;
    m.format_version = 1;
    m.project_name = "elision-test";
    m.project_version = "1.0.0";
    m.code_hash = "deadbeef";

    SymbolEffectEntry main_entry;
    main_entry.symbol_name = "main";
    main_entry.effects.set(Effect::FileSystemRead);

    SymbolEffectEntry helper;
    helper.symbol_name = "helper";
    helper.effects.set(Effect::Network);

    SymbolEffectEntry unreachable;
    unreachable.symbol_name = "dead_code";
    unreachable.effects.set(Effect::ProcessExec);

    m.symbols = {main_entry, helper, unreachable};
    return m;
}

TEST(EffectElisionTest, UnreachableEffectsRemoved) {
    EffectElision elision;
    elision.set_call_graph({{"main", "helper"}});
    elision.set_entry_points({"main"});

    auto m = make_manifest_with_symbols();
    auto elided = elision.elide(m, /*is_release_build=*/true);

    // main and helper are reachable; dead_code is not
    EXPECT_EQ(elided.symbols.size(), 2u);
    bool found_main = false, found_helper = false, found_dead = false;
    for (const auto& s : elided.symbols) {
        if (s.symbol_name == "main") found_main = true;
        if (s.symbol_name == "helper") found_helper = true;
        if (s.symbol_name == "dead_code") found_dead = true;
    }
    EXPECT_TRUE(found_main);
    EXPECT_TRUE(found_helper);
    EXPECT_FALSE(found_dead);
}

TEST(EffectElisionTest, DebugBuildPreservesAll) {
    EffectElision elision;
    elision.set_call_graph({{"main", "helper"}});
    elision.set_entry_points({"main"});

    auto m = make_manifest_with_symbols();
    auto result = elision.elide(m, /*is_release_build=*/false);

    // Debug build: all symbols preserved
    EXPECT_EQ(result.symbols.size(), 3u);
}

TEST(EffectElisionTest, TransitiveReachability) {
    EffectElision elision;
    elision.set_call_graph({{"main", "a"}, {"a", "b"}, {"b", "c"}});
    elision.set_entry_points({"main"});

    Manifest m;
    m.format_version = 1;
    m.project_name = "chain";
    m.project_version = "1.0.0";
    m.code_hash = "aabb";

    for (const auto& name : {"main", "a", "b", "c", "orphan"}) {
        SymbolEffectEntry e;
        e.symbol_name = name;
        e.effects.set(Effect::State);
        m.symbols.push_back(e);
    }

    auto elided = elision.elide(m, true);
    EXPECT_EQ(elided.symbols.size(), 4u);  // main, a, b, c — not orphan
}

TEST(EffectElisionTest, NoEntryPointsRemovesAll) {
    EffectElision elision;
    elision.set_call_graph({});
    elision.set_entry_points({});

    auto m = make_manifest_with_symbols();
    auto elided = elision.elide(m, true);
    EXPECT_TRUE(elided.symbols.empty());
}

TEST(EffectElisionTest, ManifestMetadataPreserved) {
    EffectElision elision;
    elision.set_call_graph({});
    elision.set_entry_points({"main"});

    auto m = make_manifest_with_symbols();
    auto elided = elision.elide(m, true);

    EXPECT_EQ(elided.format_version, m.format_version);
    EXPECT_EQ(elided.project_name, m.project_name);
    EXPECT_EQ(elided.project_version, m.project_version);
    // code_hash may be re-computed after elision
}

}  // namespace
}  // namespace meld::manifest
