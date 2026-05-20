#include <gtest/gtest.h>
#include "meld/types/persistent_list.hpp"
#include <string>

using namespace meld;

// Test empty list creation
TEST(PersistentListTest, EmptyList) {
    auto list = PersistentList<int>::empty();
    
    EXPECT_TRUE(list.isEmpty());
    EXPECT_EQ(list.size(), 0);
    EXPECT_FALSE(list.first().has_value());
    EXPECT_FALSE(list.last().has_value());
}

// Test list creation from vector
TEST(PersistentListTest, CreateFromVector) {
    std::vector<int> data = {1, 2, 3, 4, 5};
    auto list = PersistentList<int>::of(data);
    
    EXPECT_FALSE(list.isEmpty());
    EXPECT_EQ(list.size(), 5);
    EXPECT_EQ(list.get(0), 1);
    EXPECT_EQ(list.get(4), 5);
}

// Test add operation (immutability)
TEST(PersistentListTest, AddImmutability) {
    auto list1 = PersistentList<int>::of({1, 2, 3});
    auto list2 = list1.add(4);
    
    // Original list unchanged
    EXPECT_EQ(list1.size(), 3);
    EXPECT_EQ(list1.get(2), 3);
    
    // New list has added element
    EXPECT_EQ(list2.size(), 4);
    EXPECT_EQ(list2.get(0), 4);  // Added at front for structural sharing
    EXPECT_EQ(list2.get(3), 3);
}

// Test addAll operation
TEST(PersistentListTest, AddAll) {
    auto list1 = PersistentList<int>::of({1, 2});
    auto list2 = list1.addAll({3, 4, 5});
    
    EXPECT_EQ(list1.size(), 2);
    EXPECT_EQ(list2.size(), 5);
}

// Test get operation
TEST(PersistentListTest, Get) {
    auto list = PersistentList<int>::of({10, 20, 30, 40, 50});
    
    EXPECT_EQ(list.get(0), 10);
    EXPECT_EQ(list.get(2), 30);
    EXPECT_EQ(list.get(4), 50);
    
    EXPECT_THROW(list.get(5), std::out_of_range);
}

// Test set operation (immutability)
TEST(PersistentListTest, SetImmutability) {
    auto list1 = PersistentList<int>::of({1, 2, 3});
    auto list2 = list1.set(1, 99);
    
    // Original unchanged
    EXPECT_EQ(list1.get(1), 2);
    
    // New list has updated value
    EXPECT_EQ(list2.get(1), 99);
    EXPECT_EQ(list2.get(0), 1);
    EXPECT_EQ(list2.get(2), 3);
}

// Test remove operation (immutability)
TEST(PersistentListTest, RemoveImmutability) {
    auto list1 = PersistentList<int>::of({1, 2, 3, 4, 5});
    auto list2 = list1.remove(2);
    
    // Original unchanged
    EXPECT_EQ(list1.size(), 5);
    EXPECT_EQ(list1.get(2), 3);
    
    // New list has element removed
    EXPECT_EQ(list2.size(), 4);
    EXPECT_EQ(list2.get(0), 1);
    EXPECT_EQ(list2.get(1), 2);
    EXPECT_EQ(list2.get(2), 4);  // 3 was removed
    EXPECT_EQ(list2.get(3), 5);
}

// Test first and last
TEST(PersistentListTest, FirstAndLast) {
    auto list = PersistentList<int>::of({10, 20, 30});
    
    EXPECT_TRUE(list.first().has_value());
    EXPECT_EQ(list.first().value(), 10);
    
    EXPECT_TRUE(list.last().has_value());
    EXPECT_EQ(list.last().value(), 30);
}

// Test contains
TEST(PersistentListTest, Contains) {
    auto list = PersistentList<int>::of({1, 2, 3, 4, 5});
    
    EXPECT_TRUE(list.contains(3));
    EXPECT_TRUE(list.contains(1));
    EXPECT_TRUE(list.contains(5));
    EXPECT_FALSE(list.contains(10));
}

// Test map operation
TEST(PersistentListTest, Map) {
    auto list1 = PersistentList<int>::of({1, 2, 3});
    auto list2 = list1.map<int>([](const int& x) { return x * 2; });
    
    // Original unchanged
    EXPECT_EQ(list1.get(0), 1);
    
    // New list has mapped values
    EXPECT_EQ(list2.size(), 3);
    EXPECT_EQ(list2.get(0), 2);
    EXPECT_EQ(list2.get(1), 4);
    EXPECT_EQ(list2.get(2), 6);
}

// Test map with type transformation
TEST(PersistentListTest, MapTypeTransform) {
    auto list1 = PersistentList<int>::of({1, 2, 3});
    auto list2 = list1.map<std::string>([](const int& x) {
        return std::to_string(x);
    });
    
    EXPECT_EQ(list2.size(), 3);
    EXPECT_EQ(list2.get(0), "1");
    EXPECT_EQ(list2.get(1), "2");
    EXPECT_EQ(list2.get(2), "3");
}

// Test filter operation
TEST(PersistentListTest, Filter) {
    auto list1 = PersistentList<int>::of({1, 2, 3, 4, 5, 6});
    auto list2 = list1.filter([](const int& x) { return x % 2 == 0; });
    
    // Original unchanged
    EXPECT_EQ(list1.size(), 6);
    
    // New list has filtered values
    EXPECT_EQ(list2.size(), 3);
    EXPECT_EQ(list2.get(0), 2);
    EXPECT_EQ(list2.get(1), 4);
    EXPECT_EQ(list2.get(2), 6);
}

// Test forEach operation
TEST(PersistentListTest, ForEach) {
    auto list = PersistentList<int>::of({1, 2, 3, 4, 5});
    
    int sum = 0;
    list.forEach([&sum](const int& x) {
        sum += x;
    });
    
    EXPECT_EQ(sum, 15);
}

// Test reduce operation
TEST(PersistentListTest, Reduce) {
    auto list = PersistentList<int>::of({1, 2, 3, 4, 5});
    
    int sum = list.reduce<int>(0, [](const int& acc, const int& x) {
        return acc + x;
    });
    
    EXPECT_EQ(sum, 15);
}

// Test concat operation
TEST(PersistentListTest, Concat) {
    auto list1 = PersistentList<int>::of({1, 2, 3});
    auto list2 = PersistentList<int>::of({4, 5, 6});
    auto list3 = list1.concat(list2);
    
    // Originals unchanged
    EXPECT_EQ(list1.size(), 3);
    EXPECT_EQ(list2.size(), 3);
    
    // Concatenated list
    EXPECT_EQ(list3.size(), 6);
    EXPECT_EQ(list3.get(0), 1);
    EXPECT_EQ(list3.get(2), 3);
    EXPECT_EQ(list3.get(3), 4);
    EXPECT_EQ(list3.get(5), 6);
}

// Test take operation
TEST(PersistentListTest, Take) {
    auto list1 = PersistentList<int>::of({1, 2, 3, 4, 5});
    auto list2 = list1.take(3);
    
    EXPECT_EQ(list1.size(), 5);
    EXPECT_EQ(list2.size(), 3);
    EXPECT_EQ(list2.get(0), 1);
    EXPECT_EQ(list2.get(2), 3);
}

// Test drop operation
TEST(PersistentListTest, Drop) {
    auto list1 = PersistentList<int>::of({1, 2, 3, 4, 5});
    auto list2 = list1.drop(2);
    
    EXPECT_EQ(list1.size(), 5);
    EXPECT_EQ(list2.size(), 3);
    EXPECT_EQ(list2.get(0), 3);
    EXPECT_EQ(list2.get(2), 5);
}

// Test toVector operation
TEST(PersistentListTest, ToVector) {
    auto list = PersistentList<int>::of({1, 2, 3, 4, 5});
    auto vec = list.toVector();
    
    EXPECT_EQ(vec.size(), 5);
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[4], 5);
}

// Test structural sharing (multiple operations)
TEST(PersistentListTest, StructuralSharing) {
    auto list1 = PersistentList<int>::of({1, 2, 3});
    auto list2 = list1.add(4);
    auto list3 = list2.add(5);
    auto list4 = list1.add(10);
    
    // All lists maintain their own state
    EXPECT_EQ(list1.size(), 3);
    EXPECT_EQ(list2.size(), 4);
    EXPECT_EQ(list3.size(), 5);
    EXPECT_EQ(list4.size(), 4);
    
    // Verify values
    EXPECT_EQ(list1.get(2), 3);
    EXPECT_EQ(list4.get(0), 10);
}
