#include <gtest/gtest.h>
#include "meld/types/mutable_set.hpp"
#include <string>

using namespace meld;

// Test empty set creation
TEST(MutableSetTest, EmptySet) {
    auto set = MutableSet<int>::empty();
    
    EXPECT_TRUE(set.isEmpty());
    EXPECT_EQ(set.size(), 0);
}

// Test set creation from std::set
TEST(MutableSetTest, CreateFromStdSet) {
    std::set<int> data = {1, 2, 3, 4, 5};
    auto set = MutableSet<int>::of(data);
    
    EXPECT_FALSE(set.isEmpty());
    EXPECT_EQ(set.size(), 5);
    EXPECT_TRUE(set.contains(1));
}

// Test set creation from vector
TEST(MutableSetTest, CreateFromVector) {
    std::vector<int> data = {1, 2, 3, 2, 1};  // Duplicates
    auto set = MutableSet<int>::of(data);
    
    EXPECT_EQ(set.size(), 3);  // Duplicates removed
    EXPECT_TRUE(set.contains(1));
    EXPECT_TRUE(set.contains(2));
    EXPECT_TRUE(set.contains(3));
}

// Test add operation (mutability)
TEST(MutableSetTest, AddMutability) {
    auto set = MutableSet<int>::empty();
    
    bool added1 = set.add(1);
    bool added2 = set.add(2);
    
    EXPECT_TRUE(added1);
    EXPECT_TRUE(added2);
    EXPECT_EQ(set.size(), 2);
    EXPECT_TRUE(set.contains(1));
    EXPECT_TRUE(set.contains(2));
}

// Test add with duplicate
TEST(MutableSetTest, AddDuplicate) {
    auto set = MutableSet<int>::of({1, 2, 3});
    
    bool added = set.add(2);
    
    EXPECT_FALSE(added);  // Duplicate not added
    EXPECT_EQ(set.size(), 3);
}

// Test addAll operation
TEST(MutableSetTest, AddAll) {
    auto set = MutableSet<int>::of({1, 2});
    
    set.addAll({3, 4, 5});
    
    EXPECT_EQ(set.size(), 5);
    EXPECT_TRUE(set.contains(3));
    EXPECT_TRUE(set.contains(5));
}

// Test addAll with duplicates
TEST(MutableSetTest, AddAllWithDuplicates) {
    auto set = MutableSet<int>::of({1, 2, 3});
    
    set.addAll({2, 3, 4});
    
    EXPECT_EQ(set.size(), 4);  // Only 4 added
    EXPECT_TRUE(set.contains(4));
}

// Test remove operation
TEST(MutableSetTest, Remove) {
    auto set = MutableSet<int>::of({1, 2, 3, 4, 5});
    
    bool removed = set.remove(3);
    
    EXPECT_TRUE(removed);
    EXPECT_EQ(set.size(), 4);
    EXPECT_FALSE(set.contains(3));
    
    bool notRemoved = set.remove(99);
    EXPECT_FALSE(notRemoved);
}

// Test clear operation
TEST(MutableSetTest, Clear) {
    auto set = MutableSet<int>::of({1, 2, 3, 4, 5});
    
    set.clear();
    
    EXPECT_TRUE(set.isEmpty());
    EXPECT_EQ(set.size(), 0);
}

// Test contains
TEST(MutableSetTest, Contains) {
    auto set = MutableSet<int>::of({1, 2, 3, 4, 5});
    
    EXPECT_TRUE(set.contains(1));
    EXPECT_TRUE(set.contains(3));
    EXPECT_TRUE(set.contains(5));
    EXPECT_FALSE(set.contains(10));
}

// Test map operation (returns new set)
TEST(MutableSetTest, Map) {
    auto set1 = MutableSet<int>::of({1, 2, 3});
    auto set2 = set1.map<int>([](const int& x) { return x * 2; });
    
    // Original unchanged
    EXPECT_TRUE(set1.contains(1));
    
    // New set has mapped values
    EXPECT_EQ(set2.size(), 3);
    EXPECT_TRUE(set2.contains(2));
    EXPECT_TRUE(set2.contains(4));
    EXPECT_TRUE(set2.contains(6));
}

// Test filter operation (returns new set)
TEST(MutableSetTest, Filter) {
    auto set1 = MutableSet<int>::of({1, 2, 3, 4, 5, 6});
    auto set2 = set1.filter([](const int& x) { return x % 2 == 0; });
    
    // Original unchanged
    EXPECT_EQ(set1.size(), 6);
    
    // New set has filtered values
    EXPECT_EQ(set2.size(), 3);
    EXPECT_TRUE(set2.contains(2));
    EXPECT_TRUE(set2.contains(4));
    EXPECT_TRUE(set2.contains(6));
}

// Test forEach operation
TEST(MutableSetTest, ForEach) {
    auto set = MutableSet<int>::of({1, 2, 3, 4, 5});
    
    int sum = 0;
    set.forEach([&sum](const int& x) {
        sum += x;
    });
    
    EXPECT_EQ(sum, 15);
}

// Test reduce operation
TEST(MutableSetTest, Reduce) {
    auto set = MutableSet<int>::of({1, 2, 3, 4, 5});
    
    int sum = set.reduce<int>(0, [](const int& acc, const int& x) {
        return acc + x;
    });
    
    EXPECT_EQ(sum, 15);
}

// Test unionWith operation (mutates in place)
TEST(MutableSetTest, UnionWith) {
    auto set1 = MutableSet<int>::of({1, 2, 3});
    auto set2 = MutableSet<int>::of({3, 4, 5});
    
    set1.unionWith(set2);
    
    EXPECT_EQ(set1.size(), 5);
    EXPECT_TRUE(set1.contains(1));
    EXPECT_TRUE(set1.contains(3));
    EXPECT_TRUE(set1.contains(5));
}

// Test unionCopy operation (returns new set)
TEST(MutableSetTest, UnionCopy) {
    auto set1 = MutableSet<int>::of({1, 2, 3});
    auto set2 = MutableSet<int>::of({3, 4, 5});
    auto set3 = set1.unionCopy(set2);
    
    // Originals unchanged
    EXPECT_EQ(set1.size(), 3);
    EXPECT_EQ(set2.size(), 3);
    
    // Union contains all elements
    EXPECT_EQ(set3.size(), 5);
    EXPECT_TRUE(set3.contains(1));
    EXPECT_TRUE(set3.contains(5));
}

// Test intersectWith operation (mutates in place)
TEST(MutableSetTest, IntersectWith) {
    auto set1 = MutableSet<int>::of({1, 2, 3, 4});
    auto set2 = MutableSet<int>::of({3, 4, 5, 6});
    
    set1.intersectWith(set2);
    
    EXPECT_EQ(set1.size(), 2);
    EXPECT_TRUE(set1.contains(3));
    EXPECT_TRUE(set1.contains(4));
    EXPECT_FALSE(set1.contains(1));
}

// Test intersect operation (returns new set)
TEST(MutableSetTest, Intersect) {
    auto set1 = MutableSet<int>::of({1, 2, 3, 4});
    auto set2 = MutableSet<int>::of({3, 4, 5, 6});
    auto set3 = set1.intersect(set2);
    
    // Originals unchanged
    EXPECT_EQ(set1.size(), 4);
    EXPECT_EQ(set2.size(), 4);
    
    // Intersection contains common elements
    EXPECT_EQ(set3.size(), 2);
    EXPECT_TRUE(set3.contains(3));
    EXPECT_TRUE(set3.contains(4));
}

// Test differenceWith operation (mutates in place)
TEST(MutableSetTest, DifferenceWith) {
    auto set1 = MutableSet<int>::of({1, 2, 3, 4});
    auto set2 = MutableSet<int>::of({3, 4, 5, 6});
    
    set1.differenceWith(set2);
    
    EXPECT_EQ(set1.size(), 2);
    EXPECT_TRUE(set1.contains(1));
    EXPECT_TRUE(set1.contains(2));
    EXPECT_FALSE(set1.contains(3));
}

// Test difference operation (returns new set)
TEST(MutableSetTest, Difference) {
    auto set1 = MutableSet<int>::of({1, 2, 3, 4});
    auto set2 = MutableSet<int>::of({3, 4, 5, 6});
    auto set3 = set1.difference(set2);
    
    // Originals unchanged
    EXPECT_EQ(set1.size(), 4);
    EXPECT_EQ(set2.size(), 4);
    
    // Difference contains elements in set1 but not in set2
    EXPECT_EQ(set3.size(), 2);
    EXPECT_TRUE(set3.contains(1));
    EXPECT_TRUE(set3.contains(2));
}

// Test isSubsetOf operation
TEST(MutableSetTest, IsSubsetOf) {
    auto set1 = MutableSet<int>::of({1, 2});
    auto set2 = MutableSet<int>::of({1, 2, 3, 4});
    auto set3 = MutableSet<int>::of({5, 6});
    
    EXPECT_TRUE(set1.isSubsetOf(set2));
    EXPECT_FALSE(set2.isSubsetOf(set1));
    EXPECT_FALSE(set3.isSubsetOf(set2));
}

// Test isSupersetOf operation
TEST(MutableSetTest, IsSupersetOf) {
    auto set1 = MutableSet<int>::of({1, 2, 3, 4});
    auto set2 = MutableSet<int>::of({1, 2});
    auto set3 = MutableSet<int>::of({5, 6});
    
    EXPECT_TRUE(set1.isSupersetOf(set2));
    EXPECT_FALSE(set2.isSupersetOf(set1));
    EXPECT_FALSE(set1.isSupersetOf(set3));
}

// Test toVector operation
TEST(MutableSetTest, ToVector) {
    auto set = MutableSet<int>::of({3, 1, 4, 2, 5});
    auto vec = set.toVector();
    
    EXPECT_EQ(vec.size(), 5);
    // Elements should be sorted (std::set property)
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[4], 5);
}

// Test range-based for loop
TEST(MutableSetTest, RangeBasedFor) {
    auto set = MutableSet<int>::of({1, 2, 3, 4, 5});
    
    int sum = 0;
    for (const auto& x : set) {
        sum += x;
    }
    
    EXPECT_EQ(sum, 15);
}
