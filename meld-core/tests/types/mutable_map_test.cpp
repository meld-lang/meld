#include <gtest/gtest.h>
#include "meld/types/mutable_map.hpp"
#include <string>

using namespace meld;

// Test empty map creation
TEST(MutableMapTest, EmptyMap) {
    auto map = MutableMap<std::string, int>::empty();
    
    EXPECT_TRUE(map.isEmpty());
    EXPECT_EQ(map.size(), 0);
}

// Test map creation from std::map
TEST(MutableMapTest, CreateFromStdMap) {
    std::map<std::string, int> data = {{"a", 1}, {"b", 2}, {"c", 3}};
    auto map = MutableMap<std::string, int>::of(data);
    
    EXPECT_FALSE(map.isEmpty());
    EXPECT_EQ(map.size(), 3);
    EXPECT_EQ(map.get("a").value(), 1);
}

// Test put operation (mutability)
TEST(MutableMapTest, PutMutability) {
    auto map = MutableMap<std::string, int>::empty();
    
    map.put("a", 1);
    map.put("b", 2);
    
    EXPECT_EQ(map.size(), 2);
    EXPECT_EQ(map.get("a").value(), 1);
    EXPECT_EQ(map.get("b").value(), 2);
}

// Test put with existing key (override)
TEST(MutableMapTest, PutOverride) {
    auto map = MutableMap<std::string, int>::of({{"a", 1}, {"b", 2}});
    
    map.put("a", 99);
    
    EXPECT_EQ(map.get("a").value(), 99);
    EXPECT_EQ(map.size(), 2);
}

// Test putAll operation
TEST(MutableMapTest, PutAll) {
    auto map = MutableMap<std::string, int>::of({{"a", 1}});
    
    map.putAll({{"b", 2}, {"c", 3}});
    
    EXPECT_EQ(map.size(), 3);
    EXPECT_EQ(map.get("b").value(), 2);
}

// Test get operation
TEST(MutableMapTest, Get) {
    auto map = MutableMap<std::string, int>::of({{"a", 1}, {"b", 2}, {"c", 3}});
    
    EXPECT_TRUE(map.get("a").has_value());
    EXPECT_EQ(map.get("a").value(), 1);
    
    EXPECT_FALSE(map.get("z").has_value());
}

// Test getOrDefault operation
TEST(MutableMapTest, GetOrDefault) {
    auto map = MutableMap<std::string, int>::of({{"a", 1}, {"b", 2}});
    
    EXPECT_EQ(map.getOrDefault("a", 99), 1);
    EXPECT_EQ(map.getOrDefault("z", 99), 99);
}

// Test operator[]
TEST(MutableMapTest, OperatorBracket) {
    auto map = MutableMap<std::string, int>::empty();
    
    map["a"] = 1;
    map["b"] = 2;
    
    EXPECT_EQ(map["a"], 1);
    EXPECT_EQ(map["b"], 2);
    
    map["a"] = 99;
    EXPECT_EQ(map["a"], 99);
}

// Test remove operation
TEST(MutableMapTest, Remove) {
    auto map = MutableMap<std::string, int>::of({{"a", 1}, {"b", 2}, {"c", 3}});
    
    bool removed = map.remove("b");
    
    EXPECT_TRUE(removed);
    EXPECT_EQ(map.size(), 2);
    EXPECT_FALSE(map.get("b").has_value());
    
    bool notRemoved = map.remove("z");
    EXPECT_FALSE(notRemoved);
}

// Test clear operation
TEST(MutableMapTest, Clear) {
    auto map = MutableMap<std::string, int>::of({{"a", 1}, {"b", 2}, {"c", 3}});
    
    map.clear();
    
    EXPECT_TRUE(map.isEmpty());
    EXPECT_EQ(map.size(), 0);
}

// Test containsKey
TEST(MutableMapTest, ContainsKey) {
    auto map = MutableMap<std::string, int>::of({{"a", 1}, {"b", 2}});
    
    EXPECT_TRUE(map.containsKey("a"));
    EXPECT_TRUE(map.containsKey("b"));
    EXPECT_FALSE(map.containsKey("c"));
}

// Test containsValue
TEST(MutableMapTest, ContainsValue) {
    auto map = MutableMap<std::string, int>::of({{"a", 1}, {"b", 2}});
    
    EXPECT_TRUE(map.containsValue(1));
    EXPECT_TRUE(map.containsValue(2));
    EXPECT_FALSE(map.containsValue(3));
}

// Test keys operation
TEST(MutableMapTest, Keys) {
    auto map = MutableMap<std::string, int>::of({{"a", 1}, {"b", 2}, {"c", 3}});
    auto keys = map.keys();
    
    EXPECT_EQ(keys.size(), 3);
    EXPECT_TRUE(std::find(keys.begin(), keys.end(), "a") != keys.end());
    EXPECT_TRUE(std::find(keys.begin(), keys.end(), "b") != keys.end());
    EXPECT_TRUE(std::find(keys.begin(), keys.end(), "c") != keys.end());
}

// Test values operation
TEST(MutableMapTest, Values) {
    auto map = MutableMap<std::string, int>::of({{"a", 1}, {"b", 2}, {"c", 3}});
    auto values = map.values();
    
    EXPECT_EQ(values.size(), 3);
    EXPECT_TRUE(std::find(values.begin(), values.end(), 1) != values.end());
    EXPECT_TRUE(std::find(values.begin(), values.end(), 2) != values.end());
    EXPECT_TRUE(std::find(values.begin(), values.end(), 3) != values.end());
}

// Test entries operation
TEST(MutableMapTest, Entries) {
    auto map = MutableMap<std::string, int>::of({{"a", 1}, {"b", 2}});
    auto entries = map.entries();
    
    EXPECT_EQ(entries.size(), 2);
}

// Test mapValues operation (returns new map)
TEST(MutableMapTest, MapValues) {
    auto map1 = MutableMap<std::string, int>::of({{"a", 1}, {"b", 2}, {"c", 3}});
    auto map2 = map1.mapValues<int>([](const int& v) { return v * 2; });
    
    // Original unchanged
    EXPECT_EQ(map1.get("a").value(), 1);
    
    // New map has mapped values
    EXPECT_EQ(map2.size(), 3);
    EXPECT_EQ(map2.get("a").value(), 2);
    EXPECT_EQ(map2.get("b").value(), 4);
    EXPECT_EQ(map2.get("c").value(), 6);
}

// Test filter operation (returns new map)
TEST(MutableMapTest, Filter) {
    auto map1 = MutableMap<std::string, int>::of({{"a", 1}, {"b", 2}, {"c", 3}, {"d", 4}});
    auto map2 = map1.filter([](const std::string& k, const int& v) {
        return v % 2 == 0;
    });
    
    // Original unchanged
    EXPECT_EQ(map1.size(), 4);
    
    // New map has filtered entries
    EXPECT_EQ(map2.size(), 2);
    EXPECT_TRUE(map2.containsKey("b"));
    EXPECT_TRUE(map2.containsKey("d"));
}

// Test forEach operation
TEST(MutableMapTest, ForEach) {
    auto map = MutableMap<std::string, int>::of({{"a", 1}, {"b", 2}, {"c", 3}});
    
    int sum = 0;
    map.forEach([&sum](const std::string& k, const int& v) {
        sum += v;
    });
    
    EXPECT_EQ(sum, 6);
}

// Test forEachMut operation
TEST(MutableMapTest, ForEachMut) {
    auto map = MutableMap<std::string, int>::of({{"a", 1}, {"b", 2}, {"c", 3}});
    
    map.forEachMut([](const std::string& k, int& v) {
        v *= 2;
    });
    
    EXPECT_EQ(map.get("a").value(), 2);
    EXPECT_EQ(map.get("b").value(), 4);
    EXPECT_EQ(map.get("c").value(), 6);
}

// Test reduce operation
TEST(MutableMapTest, Reduce) {
    auto map = MutableMap<std::string, int>::of({{"a", 1}, {"b", 2}, {"c", 3}});
    
    int sum = map.reduce<int>(0, [](const int& acc, const std::string& k, const int& v) {
        return acc + v;
    });
    
    EXPECT_EQ(sum, 6);
}

// Test merge operation
TEST(MutableMapTest, Merge) {
    auto map1 = MutableMap<std::string, int>::of({{"a", 1}, {"b", 2}});
    auto map2 = MutableMap<std::string, int>::of({{"b", 99}, {"c", 3}});
    
    map1.merge(map2);
    
    EXPECT_EQ(map1.size(), 3);
    EXPECT_EQ(map1.get("a").value(), 1);
    EXPECT_EQ(map1.get("b").value(), 99);  // Overridden
    EXPECT_EQ(map1.get("c").value(), 3);
}

// Test range-based for loop
TEST(MutableMapTest, RangeBasedFor) {
    auto map = MutableMap<std::string, int>::of({{"a", 1}, {"b", 2}, {"c", 3}});
    
    int sum = 0;
    for (const auto& [key, value] : map) {
        sum += value;
    }
    
    EXPECT_EQ(sum, 6);
}

// Test mutable range-based for loop
TEST(MutableMapTest, MutableRangeBasedFor) {
    auto map = MutableMap<std::string, int>::of({{"a", 1}, {"b", 2}, {"c", 3}});
    
    for (auto& [key, value] : map) {
        value *= 2;
    }
    
    EXPECT_EQ(map.get("a").value(), 2);
    EXPECT_EQ(map.get("b").value(), 4);
    EXPECT_EQ(map.get("c").value(), 6);
}
