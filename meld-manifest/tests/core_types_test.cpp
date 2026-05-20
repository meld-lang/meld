#include "meld/manifest/core_types.hpp"

#include <gtest/gtest.h>

namespace meld::manifest {
namespace {

TEST(EffectTest, EnumValues) {
    EXPECT_EQ(static_cast<uint8_t>(Effect::Network), 0);
    EXPECT_EQ(static_cast<uint8_t>(Effect::FileSystemRead), 1);
    EXPECT_EQ(static_cast<uint8_t>(Effect::FileSystemWrite), 2);
    EXPECT_EQ(static_cast<uint8_t>(Effect::ProcessExec), 3);
    EXPECT_EQ(static_cast<uint8_t>(Effect::SystemTime), 4);
    EXPECT_EQ(static_cast<uint8_t>(Effect::State), 5);
}

TEST(EffectTest, EffectCount) {
    EXPECT_EQ(kEffectCount, 6u);
}

TEST(EffectTest, EffectToString) {
    EXPECT_EQ(effect_to_string(Effect::Network), "Network");
    EXPECT_EQ(effect_to_string(Effect::FileSystemRead), "FileSystemRead");
    EXPECT_EQ(effect_to_string(Effect::FileSystemWrite), "FileSystemWrite");
    EXPECT_EQ(effect_to_string(Effect::ProcessExec), "ProcessExec");
    EXPECT_EQ(effect_to_string(Effect::SystemTime), "SystemTime");
    EXPECT_EQ(effect_to_string(Effect::State), "State");
}

TEST(EffectBitmaskTest, DefaultEmpty) {
    EffectBitmask mask;
    EXPECT_TRUE(mask.empty());
    EXPECT_EQ(mask.raw(), 0);
}

TEST(EffectBitmaskTest, SetAndHas) {
    EffectBitmask mask;
    mask.set(Effect::Network);
    EXPECT_TRUE(mask.has(Effect::Network));
    EXPECT_FALSE(mask.has(Effect::FileSystemRead));
    EXPECT_FALSE(mask.empty());
}

TEST(EffectBitmaskTest, Clear) {
    EffectBitmask mask;
    mask.set(Effect::Network);
    mask.set(Effect::State);
    mask.clear(Effect::Network);
    EXPECT_FALSE(mask.has(Effect::Network));
    EXPECT_TRUE(mask.has(Effect::State));
}

TEST(EffectBitmaskTest, SetAll) {
    EffectBitmask mask;
    mask.set_all();
    for (uint8_t i = 0; i < kEffectCount; ++i) {
        EXPECT_TRUE(mask.has(static_cast<Effect>(i)));
    }
}

TEST(EffectBitmaskTest, Intersection) {
    EffectBitmask a;
    a.set(Effect::Network);
    a.set(Effect::FileSystemRead);

    EffectBitmask b;
    b.set(Effect::Network);
    b.set(Effect::State);

    auto result = a & b;
    EXPECT_TRUE(result.has(Effect::Network));
    EXPECT_FALSE(result.has(Effect::FileSystemRead));
    EXPECT_FALSE(result.has(Effect::State));
}

TEST(EffectBitmaskTest, Union) {
    EffectBitmask a;
    a.set(Effect::Network);

    EffectBitmask b;
    b.set(Effect::State);

    auto result = a | b;
    EXPECT_TRUE(result.has(Effect::Network));
    EXPECT_TRUE(result.has(Effect::State));
}

TEST(EffectBitmaskTest, ToVector) {
    EffectBitmask mask;
    mask.set(Effect::Network);
    mask.set(Effect::ProcessExec);
    auto vec = mask.to_vector();
    EXPECT_EQ(vec.size(), 2u);
    EXPECT_EQ(vec[0], Effect::Network);
    EXPECT_EQ(vec[1], Effect::ProcessExec);
}

TEST(EffectBitmaskTest, FromVector) {
    std::vector<Effect> effects{Effect::FileSystemWrite, Effect::SystemTime};
    auto mask = EffectBitmask::from_vector(effects);
    EXPECT_TRUE(mask.has(Effect::FileSystemWrite));
    EXPECT_TRUE(mask.has(Effect::SystemTime));
    EXPECT_FALSE(mask.has(Effect::Network));
}

TEST(EffectBitmaskTest, Equality) {
    EffectBitmask a;
    a.set(Effect::Network);
    EffectBitmask b;
    b.set(Effect::Network);
    EffectBitmask c;
    c.set(Effect::State);

    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

TEST(ResourceBoundsTest, DefaultConstruction) {
    ResourceBounds rb;
    EXPECT_TRUE(rb.allowed_domains.empty());
    EXPECT_TRUE(rb.read_paths.empty());
    EXPECT_TRUE(rb.write_paths.empty());
    EXPECT_TRUE(rb.exec_paths.empty());
}

TEST(ResourceBoundsTest, Equality) {
    ResourceBounds a{{"example.com"}, {"/tmp"}, {"/out"}, {"/bin/ls"}};
    ResourceBounds b{{"example.com"}, {"/tmp"}, {"/out"}, {"/bin/ls"}};
    ResourceBounds c{{"other.com"}, {}, {}, {}};
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

TEST(SymbolEffectEntryTest, Equality) {
    EffectBitmask mask;
    mask.set(Effect::Network);
    SymbolEffectEntry a{"foo", mask, {}, false};
    SymbolEffectEntry b{"foo", mask, {}, false};
    SymbolEffectEntry c{"bar", mask, {}, false};
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

TEST(SymbolEffectEntryTest, FfiFlag) {
    SymbolEffectEntry entry;
    entry.symbol_name = "c_func";
    entry.is_ffi = true;
    EXPECT_TRUE(entry.is_ffi);
}

}  // namespace
}  // namespace meld::manifest
