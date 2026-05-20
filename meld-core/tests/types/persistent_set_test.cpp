#include <gtest/gtest.h>
#include "meld/types/persistent_set.hpp"
#include <string>

using namespace meld;

// Test empty set creation
TEST(PersistentSetTest, EmptySet) {
    auto set = PersistentSet<int>::empty();
    
    EXPECT_TRUE(set.isEmpty());
    EXPECT_EQ(set.size(), 0);
}

// Test set creation from std::set
TEST(PersistentSetTest, CreateFromStdSet) {
    std::set<int> data = {1, 2, 3, 4, 5};
    auto set = PersistentSet<int>::of(data);
    
    EXPECT_FALSE(set.isEmpty());
    EXPECT_EQ(set.size(), 5);
    EXPECT_TRUE(set.contains(1));
    EXPECT_TRUE(set.contains(5));
}

// Test set creation from vector
TEST(PersistentSetTest, CreateFromVector) {
    std::vector<int> data = {1, 2, 3, 2, 1};  // Duplicates
    auto set = PersistentSet<int>::of(data);
    
    EXPECT_EQ(set.size(), 3);  // Duplicates removed
    EXPECT_TRUE(set.contains(1));
    EXPECT_TRUE(set.contains(2));
    EXPECT_TRUE(set.contains(3));
}

// Test add operation (immutability)
TEST(PersistentSetTest, AddImmutability) {
    auto set1 = PersistentSet<int>::empty();
    auto set2 = set1.add(1);
    auto set3 = set2.add(2);
    
    // Original sets unchanged
    EXPECT_EQ(set1.size(), 0);
    EXPECT_EQ(set2.size(), 1);
    
    // New set has added elements
    EXPECT_EQ(set3.size(), 2);
    EXPECT_TRUE(set3.contains(1));
    EXPECT_TRUE(set3.contains(2));
}

// Test add with duplicate (no change)
TEST(PersistentSetTest, AddDuplicate) {
    auto set1 = PersistentSet<int>::of({1, 2, 3});
    auto set2 = set1.add(2);
    
    // Size unchanged (duplicate not added)
    EXPECT_EQ(set1.size(), 3);
    EXPECT_EQ(set2.size(), 3);
}

// Test addAll operation
TEST(PersistentSetTest, AddAll) {
    auto set1 = PersistentSet<int>::of({1, 2});
    auto set2 = set1.addAll({3, 4, 5});
    
    EXPECT_EQ(set1.size(), 2);
    EXPECT_EQ(set2.size(), 5);
    EXPECT_TRUE(set2.contains(3));
    EXPECT_TRUE(set2.contains(5));
}

// Test addAll with duplicates
TEST(PersistentSetTest, AddAllWithDuplicates) {
    auto set1 = PersistentSet<int>::of({1, 2, 3});
    auto set2 = set1.addAll({2, 3, 4});
    
    EXPECT_EQ(set2.size(), 4);  // Only 4 added
    EXPECT_TRUE(set2.contains(4));
}

// Test remove operation (immutability)
TEST(PersistentSetTest, RemoveImmutability) {
    auto set1 = PersistentSet<int>::of({1, 2, 3, 4, 5});
    auto set2 = set1.remove(3);
    
    // Original unchanged
    EXPECT_EQ(set1.size(), 5);
    EXPECT_TRUE(set1.contains(3));
    
    // New set has element removed
    EXPECT_EQ(set2.size(), 4);
    EXPECT_FALSE(set2.contains(3));
    EXPECT_TRUE(set2.contains(1));
    EXPECT_TRUE(set2.contains(5));
}

// Test remove non-existent element
TEST(PersistentSetTest, RemoveNonExistent) {
    auto set1 = PersistentSet<int>::of({1, 2, 3});
    auto set2 = set1.remove(99);
    
    // No change
    EXPECT_EQ(set1.size(), 3);
    EXPECT_EQ(set2.size(), 3);
}

// Test contains
TEST(PersistentSetTest, Contains) {
    auto set = PersistentSet<int>::of({1, 2, 3, 4, 5});
    
    EXPECT_TRUE(set.contains(1));
    EXPECT_TRUE(set.contains(3));
    EXPECT_TRUE(set.contains(5));
    EXPECT_FALSE(set.contains(10));
    EXPECT_FALSE(set.contains(0));
}

// Test map operation
TEST(PersistentSetTest, Map) {
    auto set1 = PersistentSet<int>::of({1, 2, 3});
    auto set2 = set1.map<int>([](const int& x) { return x * 2; });
    
    // Original unchanged
    EXPECT_TRUE(set1.contains(1));
    
    // New set has mapped values
    EXPECT_EQ(set2.size(), 3);
    EXPECT_TRUE(set2.contains(2));
    EXPECT_TRUE(set2.contains(4));
    EXPECT_TRUE(set2.contains(6));
}

// Test map with type transformation
TEST(PersistentSetTest, MapTypeTransform) {
    auto set1 = PersistentSet<int>::of({1, 2, 3});
    auto set2 = set1.map<std::string>([](const int& x) {
        return std::to_string(x);
    });
    
    EXPECT_EQ(set2.size(), 3);
    EXPECT_TRUE(set2.contains("1"));
    EXPECT_TRUE(set2.contains("2"));
    EXPECT_TRUE(set2.contains("3"));
}

// Test filter operation
TEST(PersistentSetTest, Filter) {
    auto set1 = PersistentSet<int>::of({1, 2, 3, 4, 5, 6});
    auto set2 = set1.filter([](const int& x) { return x % 2 == 0; });
    
    // Original unchanged
    EXPECT_EQ(set1.size(), 6);
    
    // New set has filtered values
    EXPECT_EQ(set2.size(), 3);
    EXPECT_TRUE(set2.contains(2));
    EXPECT_TRUE(set2.contains(4));
    EXPECT_TRUE(set2.contains(6));
    EXPECT_FALSE(set2.contains(1));
}

// Test forEach operation
TEST(PersistentSetTest, ForEach) {
    auto set = PersistentSet<int>::of({1, 2, 3, 4, 5});
    
    int sum = 0;
    set.forEach([&sum](const int& x) {
        sum += x;
    });
    
    EXPECT_EQ(sum, 15);
}

// Test reduce operation
TEST(PersistentSetTest, Reduce) {
    auto set = PersistentSet<int>::of({1, 2, 3, 4, 5});
    
    int sum = set.reduce<int>(0, [](const int& acc, const int& x) {
        return acc + x;
    });
    
    EXPECT_EQ(sum, 15);
}

// Test unionWith operation
TEST(PersistentSetTest, UnionWith) {
    auto set1 = PersistentSet<int>::of({1, 2, 3});
    auto set2 = PersistentSet<int>::of({3, 4, 5});
    auto set3 = set1.unionWith(set2);
    
    // Originals unchanged
    EXPECT_EQ(set1.size(), 3);
    EXPECT_EQ(set2.size(), 3);
    
    // Union contains all elements
    EXPECT_EQ(set3.size(), 5);
    EXPECT_TRUE(set3.contains(1));
    EXPECT_TRUE(set3.contains(3));
    EXPECT_TRUE(set3.contains(5));
}

// Test intersect operation
TEST(PersistentSetTest, Intersect) {
    auto set1 = PersistentSet<int>::of({1, 2, 3, 4});
    auto set2 = PersistentSet<int>::of({3, 4, 5, 6});
    auto set3 = set1.intersect(set2);
    
    // Originals unchanged
    EXPECT_EQ(set1.size(), 4);
    EXPECT_EQ(set2.size(), 4);
    
    // Intersection contains common elements
    EXPECT_EQ(set3.size(), 2);
    EXPECT_TRUE(set3.contains(3));
    EXPECT_TRUE(set3.contains(4));
    EXPECT_FALSE(set3.contains(1));
    EXPECT_FALSE(set3.contains(5));
}

// Test difference operation
TEST(PersistentSetTest, Difference) {
    auto set1 = PersistentSet<int>::of({1, 2, 3, 4});
    auto set2 = PersistentSet<int>::of({3, 4, 5, 6});
    auto set3 = set1.difference(set2);
    
    // Originals unchanged
    EXPECT_EQ(set1.size(), 4);
    EXPECT_EQ(set2.size(), 4);
    
    // Difference contains elements in set1 but not in set2
    EXPECT_EQ(set3.size(), 2);
    EXPECT_TRUE(set3.contains(1));
    EXPECT_TRUE(set3.contains(2));
    EXPECT_FALSE(set3.contains(3));
    EXPECT_FALSE(set3.contains(4));
}

// Test isSubsetOf operation
TEST(PersistentSetTest, IsSubsetOf) {
    auto set1 = PersistentSet<int>::of({1, 2});
    auto set2 = PersistentSet<int>::of({1, 2, 3, 4});
    auto set3 = PersistentSet<int>::of({5, 6});
    
    EXPECT_TRUE(set1.isSubsetOf(set2));
    EXPECT_FALSE(set2.isSubsetOf(set1));
    EXPECT_FALSE(set3.isSubsetOf(set2));
}

// Test isSupersetOf operation
TEST(PersistentSetTest, IsSupersetOf) {
    auto set1 = PersistentSet<int>::of({1, 2, 3, 4});
    auto set2 = PersistentSet<int>::of({1, 2});
    auto set3 = PersistentSet<int>::of({5, 6});
    
    EXPECT_TRUE(set1.isSupersetOf(set2));
    EXPECT_FALSE(set2.isSupersetOf(set1));
    EXPECT_FALSE(set1.isSupersetOf(set3));
}

// Test toVector operation
TEST(PersistentSetTest, ToVector) {
    auto set = PersistentSet<int>::of({3, 1, 4, 2, 5});
    auto vec = set.toVector();
    
    EXPECT_EQ(vec.size(), 5);
    // Elements should be sorted (std::set property)
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[1], 2);
    EXPECT_EQ(vec[2], 3);
    EXPECT_EQ(vec[3], 4);
    EXPECT_EQ(vec[4], 5);
}

// Test toSet operation
TEST(PersistentSetTest, ToSet) {
    auto pset = PersistentSet<int>::of({1, 2, 3, 4, 5});
    auto stdset = pset.toSet();
    
    EXPECT_EQ(stdset.size(), 5);
    EXPECT_TRUE(stdset.find(1) != stdset.end());
    EXPECT_TRUE(stdset.find(5) != stdset.end());
}

// Test structural sharing (multiple operations)
TEST(PersistentSetTest, StructuralSharing) {
    auto set1 = PersistentSet<int>::of({1, 2, 3});
    auto set2 = set1.add(4);
    auto set3 = set2.add(5);
    auto set4 = set1.add(10);
    
    // All sets maintain their own state
    EXPECT_EQ(set1.size(), 3);
    EXPECT_EQ(set2.size(), 4);
    EXPECT_EQ(set3.size(), 5);
    EXPECT_EQ(set4.size(), 4);
    
    // Verify values
    EXPECT_FALSE(set1.contains(4));
    EXPECT_TRUE(set2.contains(4));
    EXPECT_TRUE(set3.contains(5));
    EXPECT_TRUE(set4.contains(10));
    EXPECT_FALSE(set4.contains(4));
}
