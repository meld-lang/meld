#include "../../include/meld/effects/effect_types.hpp"
#include <gtest/gtest.h>

using namespace meld::effects;

// Test Effect base class and built-in effects
TEST(EffectTypesTest, EffectPure) {
    const auto& pure1 = EffectPure::instance();
    const auto& pure2 = EffectPure::instance();
    
    EXPECT_EQ(pure1.name(), "Pure");
    EXPECT_TRUE(pure1.equals(pure2));
    EXPECT_EQ(pure1, pure2);
    
    auto cloned = pure1.clone();
    EXPECT_TRUE(pure1.equals(*cloned));
}

TEST(EffectTypesTest, EffectIO) {
    const auto& io1 = EffectIO::instance();
    const auto& io2 = EffectIO::instance();
    
    EXPECT_EQ(io1.name(), "IO");
    EXPECT_TRUE(io1.equals(io2));
    EXPECT_EQ(io1, io2);
    
    auto cloned = io1.clone();
    EXPECT_TRUE(io1.equals(*cloned));
}

TEST(EffectTypesTest, EffectNetwork) {
    const auto& net1 = EffectNetwork::instance();
    const auto& net2 = EffectNetwork::instance();
    
    EXPECT_EQ(net1.name(), "Network");
    EXPECT_TRUE(net1.equals(net2));
    EXPECT_EQ(net1, net2);
    
    auto cloned = net1.clone();
    EXPECT_TRUE(net1.equals(*cloned));
}

TEST(EffectTypesTest, EffectState) {
    const auto& state1 = EffectState::instance();
    const auto& state2 = EffectState::instance();
    
    EXPECT_EQ(state1.name(), "State");
    EXPECT_TRUE(state1.equals(state2));
    EXPECT_EQ(state1, state2);
    
    auto cloned = state1.clone();
    EXPECT_TRUE(state1.equals(*cloned));
}

TEST(EffectTypesTest, EffectTime) {
    const auto& time1 = EffectTime::instance();
    const auto& time2 = EffectTime::instance();
    
    EXPECT_EQ(time1.name(), "Time");
    EXPECT_TRUE(time1.equals(time2));
    EXPECT_EQ(time1, time2);
    
    auto cloned = time1.clone();
    EXPECT_TRUE(time1.equals(*cloned));
}

TEST(EffectTypesTest, CustomEffect) {
    CustomEffect custom1("Database");
    CustomEffect custom2("Database");
    CustomEffect custom3("Cache");
    
    EXPECT_EQ(custom1.name(), "Database");
    EXPECT_TRUE(custom1.equals(custom2));
    EXPECT_FALSE(custom1.equals(custom3));
    EXPECT_EQ(custom1, custom2);
    EXPECT_NE(custom1, custom3);
    
    auto cloned = custom1.clone();
    EXPECT_TRUE(custom1.equals(*cloned));
}

TEST(EffectTypesTest, EffectsDifferent) {
    const auto& pure = EffectPure::instance();
    const auto& io = EffectIO::instance();
    const auto& network = EffectNetwork::instance();
    
    EXPECT_FALSE(pure.equals(io));
    EXPECT_FALSE(io.equals(network));
    EXPECT_NE(pure, io);
    EXPECT_NE(io, network);
}

// Test EffectSet
TEST(EffectTypesTest, EmptyEffectSet) {
    EffectSet set;
    
    EXPECT_TRUE(set.is_pure());
    EXPECT_EQ(set.effects().size(), 0);
    EXPECT_EQ(set.to_string(), "Pure");
}

TEST(EffectTypesTest, SingleEffectSet) {
    EffectSet set(EffectIO::instance());
    
    EXPECT_FALSE(set.is_pure());
    EXPECT_EQ(set.effects().size(), 1);
    EXPECT_TRUE(set.contains(EffectIO::instance()));
    EXPECT_FALSE(set.contains(EffectNetwork::instance()));
}

TEST(EffectTypesTest, AddEffectToSet) {
    EffectSet set;
    
    set.add(EffectIO::instance());
    EXPECT_FALSE(set.is_pure());
    EXPECT_EQ(set.effects().size(), 1);
    EXPECT_TRUE(set.contains(EffectIO::instance()));
    
    set.add(EffectNetwork::instance());
    EXPECT_EQ(set.effects().size(), 2);
    EXPECT_TRUE(set.contains(EffectNetwork::instance()));
    
    // Adding same effect again should not increase size
    set.add(EffectIO::instance());
    EXPECT_EQ(set.effects().size(), 2);
}

TEST(EffectTypesTest, AddPureEffectToSet) {
    EffectSet set;
    
    set.add(EffectPure::instance());
    EXPECT_TRUE(set.is_pure());
    EXPECT_EQ(set.effects().size(), 0);
    
    set.add(EffectIO::instance());
    set.add(EffectPure::instance());
    EXPECT_FALSE(set.is_pure());
    EXPECT_EQ(set.effects().size(), 1);
}

TEST(EffectTypesTest, RemoveEffectFromSet) {
    EffectSet set;
    set.add(EffectIO::instance());
    set.add(EffectNetwork::instance());
    
    EXPECT_EQ(set.effects().size(), 2);
    
    set.remove(EffectIO::instance());
    EXPECT_EQ(set.effects().size(), 1);
    EXPECT_FALSE(set.contains(EffectIO::instance()));
    EXPECT_TRUE(set.contains(EffectNetwork::instance()));
    
    set.remove(EffectNetwork::instance());
    EXPECT_TRUE(set.is_pure());
}

TEST(EffectTypesTest, EffectSetUnion) {
    EffectSet set1;
    set1.add(EffectIO::instance());
    
    EffectSet set2;
    set2.add(EffectNetwork::instance());
    
    EffectSet union_set = set1.union_with(set2);
    
    EXPECT_EQ(union_set.effects().size(), 2);
    EXPECT_TRUE(union_set.contains(EffectIO::instance()));
    EXPECT_TRUE(union_set.contains(EffectNetwork::instance()));
}

TEST(EffectTypesTest, EffectSetSubset) {
    EffectSet subset;
    subset.add(EffectIO::instance());
    
    EffectSet superset;
    superset.add(EffectIO::instance());
    superset.add(EffectNetwork::instance());
    
    EXPECT_TRUE(subset.is_subset_of(superset));
    EXPECT_FALSE(superset.is_subset_of(subset));
    
    EffectSet empty_set;
    EXPECT_TRUE(empty_set.is_subset_of(subset));
    EXPECT_TRUE(empty_set.is_subset_of(superset));
}

TEST(EffectTypesTest, EffectSetEquality) {
    EffectSet set1;
    set1.add(EffectIO::instance());
    set1.add(EffectNetwork::instance());
    
    EffectSet set2;
    set2.add(EffectNetwork::instance());
    set2.add(EffectIO::instance());
    
    EXPECT_TRUE(set1.equals(set2));
    EXPECT_EQ(set1, set2);
    
    EffectSet set3;
    set3.add(EffectIO::instance());
    
    EXPECT_FALSE(set1.equals(set3));
    EXPECT_NE(set1, set3);
}

TEST(EffectTypesTest, EffectSetCopy) {
    EffectSet original;
    original.add(EffectIO::instance());
    original.add(EffectNetwork::instance());
    
    EffectSet copied = original;
    
    EXPECT_EQ(original, copied);
    EXPECT_EQ(copied.effects().size(), 2);
    
    // Modify original
    original.add(EffectState::instance());
    
    // Copy should be unchanged
    EXPECT_EQ(copied.effects().size(), 2);
    EXPECT_NE(original, copied);
}

TEST(EffectTypesTest, EffectSetInitializerList) {
    EffectSet set{EffectIO::instance(), EffectNetwork::instance(), EffectState::instance()};
    
    EXPECT_EQ(set.effects().size(), 3);
    EXPECT_TRUE(set.contains(EffectIO::instance()));
    EXPECT_TRUE(set.contains(EffectNetwork::instance()));
    EXPECT_TRUE(set.contains(EffectState::instance()));
}

// Test helper functions
TEST(EffectTypesTest, HelperFunctions) {
    auto pure = pure_effect();
    EXPECT_TRUE(pure.is_pure());
    
    auto io = io_effect();
    EXPECT_TRUE(io.contains(EffectIO::instance()));
    
    auto network = network_effect();
    EXPECT_TRUE(network.contains(EffectNetwork::instance()));
    
    auto state = state_effect();
    EXPECT_TRUE(state.contains(EffectState::instance()));
    
    auto time = time_effect();
    EXPECT_TRUE(time.contains(EffectTime::instance()));
    
    auto custom = custom_effect("Database");
    EXPECT_TRUE(custom.contains(CustomEffect("Database")));
}

TEST(EffectTypesTest, CombineEffects) {
    auto combined = combine_effects(EffectIO::instance(), EffectNetwork::instance(), EffectState::instance());
    
    EXPECT_EQ(combined.effects().size(), 3);
    EXPECT_TRUE(combined.contains(EffectIO::instance()));
    EXPECT_TRUE(combined.contains(EffectNetwork::instance()));
    EXPECT_TRUE(combined.contains(EffectState::instance()));
}

TEST(EffectTypesTest, EffectSetToString) {
    EffectSet empty;
    EXPECT_EQ(empty.to_string(), "Pure");
    
    EffectSet single;
    single.add(EffectIO::instance());
    EXPECT_EQ(single.to_string(), "{IO}");
    
    EffectSet multiple;
    multiple.add(EffectIO::instance());
    multiple.add(EffectNetwork::instance());
    // Note: Order may vary, so we check that it contains both
    std::string str = multiple.to_string();
    EXPECT_TRUE(str.find("IO") != std::string::npos);
    EXPECT_TRUE(str.find("Network") != std::string::npos);
    EXPECT_TRUE(str.find("{") != std::string::npos);
    EXPECT_TRUE(str.find("}") != std::string::npos);
}

// int main(int argc, char** argv) {
//     testing::InitGoogleTest(&argc, argv);
//     return RUN_ALL_TESTS();
// }