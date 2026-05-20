#include <gtest/gtest.h>
#include "meld/types/persistent_map.hpp"
#include <string>

using namespace meld;

// Test empty map creation
TEST(PersistentMapTest, EmptyMap) {
    auto map = PersistentMap<std::string, int>::empty();
    
    EXPECT_TRUE(map.isEmpty());
    EXPECT_EQ(map.size(), 0);
}

// Test map creation from std::map
TEST(PersistentMapTest, CreateFromStdMap) {
    std::map<std::string, int> data = {{"a", 1}, {"b", 2}, {"c", 3}};
    auto map = PersistentMap<std::string, int>::of(data);
    
    EXPECT_FALSE(map.isEmpty());
    EXPECT_EQ(map.size(), 3);
    EXPECT_EQ(map.get("a").value(), 1);
    EXPECT_EQ(map.get("c").value(), 3);
}

// Test put operation (immutability)
TEST(PersistentMapTest, PutImmutability) {
    auto map1 = PersistentMap<std::string, int>::empty();
    auto map2 = map1.put("a", 1);
    auto map3 = map2.put("b", 2);
    
    // Original maps unchanged
    EXPECT_EQ(map1.size(), 0);
    EXPECT_EQ(map2.size(), 1);
    
    // New map has added entries
    EXPECT_EQ(map3.size(), 2);
    EXPECT_EQ(map3.get("a").value(), 1);
    EXPECT_EQ(map3.get("b").value(), 2);
}

// Test put with existing key (override)
TEST(PersistentMapTest, PutOverride) {
    auto map1 = PersistentMap<std::string, int>::of({{"a", 1}, {"b", 2}});
    auto map2 = map1.put("a", 99);
    
    // Original unchanged
    EXPECT_EQ(map1.get("a").value(), 1);
    
    // New map has updated value
    EXPECT_EQ(map2.get("a").value(), 99);
    EXPECT_EQ(map2.get("b").value(), 2);
    EXPECT_EQ(map2.size(), 2);
}

// Test putAll operation
TEST(PersistentMapTest, PutAll) {
    auto map1 = PersistentMap<std::string, int>::of({{"a", 1}});
    auto map2 = map1.putAll({{"b", 2}, {"c", 3}});
    
    EXPECT_EQ(map1.size(), 1);
    EXPECT_EQ(map2.size(), 3);
    EXPECT_EQ(map2.get("b").value(), 2);
}

// Test get operation
TEST(PersistentMapTest, Get) {
    auto map = PersistentMap<std::string, int>::of({{"a", 1}, {"b", 2}, {"c", 3}});
    
    EXPECT_TRUE(map.get("a").has_value());
    EXPECT_EQ(map.get("a").value(), 1);
    
    EXPECT_TRUE(map.get("c").has_value());
    EXPECT_EQ(map.get("c").value(), 3);
    
    EXPECT_FALSE(map.get("z").has_value());
}

// Test getOrDefault operation
TEST(PersistentMapTest, GetOrDefault) {
    auto map = PersistentMap<std::string, int>::of({{"a", 1}, {"b", 2}});
    
    EXPECT_EQ(map.getOrDefault("a", 99), 1);
    EXPECT_EQ(map.getOrDefault("z", 99), 99);
}

// Test remove operation (immutability)
TEST(PersistentMapTest, RemoveImmutability) {
    auto map1 = PersistentMap<std::string, int>::of({{"a", 1}, {"b", 2}, {"c", 3}});
    auto map2 = map1.remove("b");
    
    // Original unchanged
    EXPECT_EQ(map1.size(), 3);
    EXPECT_TRUE(map1.get("b").has_value());
    
    // New map has entry removed
    EXPECT_EQ(map2.size(), 2);
    EXPECT_FALSE(map2.get("b").has_value());
    EXPECT_TRUE(map2.get("a").has_value());
    EXPECT_TRUE(map2.get("c").has_value());
}

// Test containsKey
TEST(PersistentMapTest, ContainsKey) {
    auto map = PersistentMap<std::string, int>::of({{"a", 1}, {"b", 2}});
    
    EXPECT_TRUE(map.containsKey("a"));
    EXPECT_TRUE(map.containsKey("b"));
    EXPECT_FALSE(map.containsKey("c"));
}

// Test containsValue
TEST(PersistentMapTest, ContainsValue) {
    auto map = PersistentMap<std::string, int>::of({{"a", 1}, {"b", 2}});
    
    EXPECT_TRUE(map.containsValue(1));
    EXPECT_TRUE(map.containsValue(2));
    EXPECT_FALSE(map.containsValue(3));
}

// Test keys operation
TEST(PersistentMapTest, Keys) {
    auto map = PersistentMap<std::string, int>::of({{"a", 1}, {"b", 2}, {"c", 3}});
    auto keys = map.keys();
    
    EXPECT_EQ(keys.size(), 3);
    EXPECT_TRUE(std::find(keys.begin(), keys.end(), "a") != keys.end());
    EXPECT_TRUE(std::find(keys.begin(), keys.end(), "b") != keys.end());
    EXPECT_TRUE(std::find(keys.begin(), keys.end(), "c") != keys.end());
}

// Test values operation
TEST(PersistentMapTest, Values) {
    auto map = PersistentMap<std::string, int>::of({{"a", 1}, {"b", 2}, {"c", 3}});
    auto values = map.values();
    
    EXPECT_EQ(values.size(), 3);
    EXPECT_TRUE(std::find(values.begin(), values.end(), 1) != values.end());
    EXPECT_TRUE(std::find(values.begin(), values.end(), 2) != values.end());
    EXPECT_TRUE(std::find(values.begin(), values.end(), 3) != values.end());
}

// Test entries operation
TEST(PersistentMapTest, Entries) {
    auto map = PersistentMap<std::string, int>::of({{"a", 1}, {"b", 2}});
    auto entries = map.entries();
    
    EXPECT_EQ(entries.size(), 2);
    
    bool foundA = false, foundB = false;
    for (const auto& [key, value] : entries) {
        if (key == "a" && value == 1) foundA = true;
        if (key == "b" && value == 2) foundB = true;
    }
    EXPECT_TRUE(foundA);
    EXPECT_TRUE(foundB);
}

// Test mapValues operation
TEST(PersistentMapTest, MapValues) {
    auto map1 = PersistentMap<std::string, int>::of({{"a", 1}, {"b", 2}, {"c", 3}});
    auto map2 = map1.mapValues<int>([](const int& v) { return v * 2; });
    
    // Original unchanged
    EXPECT_EQ(map1.get("a").value(), 1);
    
    // New map has mapped values
    EXPECT_EQ(map2.size(), 3);
    EXPECT_EQ(map2.get("a").value(), 2);
    EXPECT_EQ(map2.get("b").value(), 4);
    EXPECT_EQ(map2.get("c").value(), 6);
}

// Test mapValues with type transformation
TEST(PersistentMapTest, MapValuesTypeTransform) {
    auto map1 = PersistentMap<std::string, int>::of({{"a", 1}, {"b", 2}});
    auto map2 = map1.mapValues<std::string>([](const int& v) {
        return std::to_string(v);
    });
    
    EXPECT_EQ(map2.size(), 2);
    EXPECT_EQ(map2.get("a").value(), "1");
    EXPECT_EQ(map2.get("b").value(), "2");
}

// Test filter operation
TEST(PersistentMapTest, Filter) {
    auto map1 = PersistentMap<std::string, int>::of({{"a", 1}, {"b", 2}, {"c", 3}, {"d", 4}});
    auto map2 = map1.filter([](const std::string& k, const int& v) {
        return v % 2 == 0;
    });
    
    // Original unchanged
    EXPECT_EQ(map1.size(), 4);
    
    // New map has filtered entries
    EXPECT_EQ(map2.size(), 2);
    EXPECT_TRUE(map2.containsKey("b"));
    EXPECT_TRUE(map2.containsKey("d"));
    EXPECT_FALSE(map2.containsKey("a"));
    EXPECT_FALSE(map2.containsKey("c"));
}

// Test forEach operation
TEST(PersistentMapTest, ForEach) {
    auto map = PersistentMap<std::string, int>::of({{"a", 1}, {"b", 2}, {"c", 3}});
    
    int sum = 0;
    map.forEach([&sum](const std::string& k, const int& v) {
        sum += v;
    });
    
    EXPECT_EQ(sum, 6);
}

// Test reduce operation
TEST(PersistentMapTest, Reduce) {
    auto map = PersistentMap<std::string, int>::of({{"a", 1}, {"b", 2}, {"c", 3}});
    
    int sum = map.reduce<int>(0, [](const int& acc, const std::string& k, const int& v) {
        return acc + v;
    });
    
    EXPECT_EQ(sum, 6);
}

// Test merge operation
TEST(PersistentMapTest, Merge) {
    auto map1 = PersistentMap<std::string, int>::of({{"a", 1}, {"b", 2}});
    auto map2 = PersistentMap<std::string, int>::of({{"b", 99}, {"c", 3}});
    auto map3 = map1.merge(map2);
    
    // Originals unchanged
    EXPECT_EQ(map1.size(), 2);
    EXPECT_EQ(map2.size(), 2);
    
    // Merged map (map2 values override map1)
    EXPECT_EQ(map3.size(), 3);
    EXPECT_EQ(map3.get("a").value(), 1);
    EXPECT_EQ(map3.get("b").value(), 99);  // Overridden
    EXPECT_EQ(map3.get("c").value(), 3);
}

// Test toMap operation
TEST(PersistentMapTest, ToMap) {
    auto pmap = PersistentMap<std::string, int>::of({{"a", 1}, {"b", 2}, {"c", 3}});
    auto stdmap = pmap.toMap();
    
    EXPECT_EQ(stdmap.size(), 3);
    EXPECT_EQ(stdmap["a"], 1);
    EXPECT_EQ(stdmap["b"], 2);
    EXPECT_EQ(stdmap["c"], 3);
}

// Test structural sharing (multiple operations)
TEST(PersistentMapTest, StructuralSharing) {
    auto map1 = PersistentMap<std::string, int>::of({{"a", 1}});
    auto map2 = map1.put("b", 2);
    auto map3 = map2.put("c", 3);
    auto map4 = map1.put("x", 99);
    
    // All maps maintain their own state
    EXPECT_EQ(map1.size(), 1);
    EXPECT_EQ(map2.size(), 2);
    EXPECT_EQ(map3.size(), 3);
    EXPECT_EQ(map4.size(), 2);
    
    // Verify values
    EXPECT_EQ(map1.get("a").value(), 1);
    EXPECT_FALSE(map1.containsKey("b"));
    EXPECT_TRUE(map2.containsKey("b"));
    EXPECT_TRUE(map4.containsKey("x"));
    EXPECT_FALSE(map4.containsKey("b"));
}
