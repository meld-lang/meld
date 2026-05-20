#include <gtest/gtest.h>
#include "meld/types/mutable_list.hpp"
#include <string>

using namespace meld;

// Test empty list creation
TEST(MutableListTest, EmptyList) {
    auto list = MutableList<int>::empty();
    
    EXPECT_TRUE(list.isEmpty());
    EXPECT_EQ(list.size(), 0);
}

// Test list creation from vector
TEST(MutableListTest, CreateFromVector) {
    std::vector<int> data = {1, 2, 3, 4, 5};
    auto list = MutableList<int>::of(data);
    
    EXPECT_FALSE(list.isEmpty());
    EXPECT_EQ(list.size(), 5);
    EXPECT_EQ(list.get(0), 1);
    EXPECT_EQ(list.get(4), 5);
}

// Test add operation (mutability)
TEST(MutableListTest, AddMutability) {
    auto list = MutableList<int>::of({1, 2, 3});
    
    list.add(4);
    
    EXPECT_EQ(list.size(), 4);
    EXPECT_EQ(list.get(3), 4);
}

// Test addAll operation
TEST(MutableListTest, AddAll) {
    auto list = MutableList<int>::of({1, 2});
    
    list.addAll({3, 4, 5});
    
    EXPECT_EQ(list.size(), 5);
    EXPECT_EQ(list.get(4), 5);
}

// Test insert operation
TEST(MutableListTest, Insert) {
    auto list = MutableList<int>::of({1, 2, 4, 5});
    
    list.insert(2, 3);
    
    EXPECT_EQ(list.size(), 5);
    EXPECT_EQ(list.get(2), 3);
    EXPECT_EQ(list.get(3), 4);
}

// Test get operation
TEST(MutableListTest, Get) {
    auto list = MutableList<int>::of({10, 20, 30, 40, 50});
    
    EXPECT_EQ(list.get(0), 10);
    EXPECT_EQ(list.get(2), 30);
    EXPECT_EQ(list.get(4), 50);
    
    EXPECT_THROW(list.get(5), std::out_of_range);
}

// Test operator[]
TEST(MutableListTest, OperatorBracket) {
    auto list = MutableList<int>::of({1, 2, 3});
    
    EXPECT_EQ(list[0], 1);
    EXPECT_EQ(list[2], 3);
    
    list[1] = 99;
    EXPECT_EQ(list[1], 99);
}

// Test set operation
TEST(MutableListTest, Set) {
    auto list = MutableList<int>::of({1, 2, 3});
    
    list.set(1, 99);
    
    EXPECT_EQ(list.get(1), 99);
    EXPECT_EQ(list.get(0), 1);
    EXPECT_EQ(list.get(2), 3);
}

// Test remove operation
TEST(MutableListTest, Remove) {
    auto list = MutableList<int>::of({1, 2, 3, 4, 5});
    
    list.remove(2);
    
    EXPECT_EQ(list.size(), 4);
    EXPECT_EQ(list.get(0), 1);
    EXPECT_EQ(list.get(1), 2);
    EXPECT_EQ(list.get(2), 4);  // 3 was removed
    EXPECT_EQ(list.get(3), 5);
}

// Test removeValue operation
TEST(MutableListTest, RemoveValue) {
    auto list = MutableList<int>::of({1, 2, 3, 4, 5});
    
    bool removed = list.removeValue(3);
    
    EXPECT_TRUE(removed);
    EXPECT_EQ(list.size(), 4);
    EXPECT_FALSE(list.contains(3));
    
    bool notRemoved = list.removeValue(99);
    EXPECT_FALSE(notRemoved);
}

// Test clear operation
TEST(MutableListTest, Clear) {
    auto list = MutableList<int>::of({1, 2, 3, 4, 5});
    
    list.clear();
    
    EXPECT_TRUE(list.isEmpty());
    EXPECT_EQ(list.size(), 0);
}

// Test first and last
TEST(MutableListTest, FirstAndLast) {
    auto list = MutableList<int>::of({10, 20, 30});
    
    EXPECT_TRUE(list.first().has_value());
    EXPECT_EQ(list.first().value(), 10);
    
    EXPECT_TRUE(list.last().has_value());
    EXPECT_EQ(list.last().value(), 30);
}

// Test contains
TEST(MutableListTest, Contains) {
    auto list = MutableList<int>::of({1, 2, 3, 4, 5});
    
    EXPECT_TRUE(list.contains(3));
    EXPECT_TRUE(list.contains(1));
    EXPECT_TRUE(list.contains(5));
    EXPECT_FALSE(list.contains(10));
}

// Test map operation (returns new list)
TEST(MutableListTest, Map) {
    auto list1 = MutableList<int>::of({1, 2, 3});
    auto list2 = list1.map<int>([](const int& x) { return x * 2; });
    
    // Original unchanged
    EXPECT_EQ(list1.get(0), 1);
    
    // New list has mapped values
    EXPECT_EQ(list2.size(), 3);
    EXPECT_EQ(list2.get(0), 2);
    EXPECT_EQ(list2.get(1), 4);
    EXPECT_EQ(list2.get(2), 6);
}

// Test filter operation (returns new list)
TEST(MutableListTest, Filter) {
    auto list1 = MutableList<int>::of({1, 2, 3, 4, 5, 6});
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
TEST(MutableListTest, ForEach) {
    auto list = MutableList<int>::of({1, 2, 3, 4, 5});
    
    int sum = 0;
    list.forEach([&sum](const int& x) {
        sum += x;
    });
    
    EXPECT_EQ(sum, 15);
}

// Test forEachMut operation
TEST(MutableListTest, ForEachMut) {
    auto list = MutableList<int>::of({1, 2, 3, 4, 5});
    
    list.forEachMut([](int& x) {
        x *= 2;
    });
    
    EXPECT_EQ(list.get(0), 2);
    EXPECT_EQ(list.get(1), 4);
    EXPECT_EQ(list.get(2), 6);
}

// Test reduce operation
TEST(MutableListTest, Reduce) {
    auto list = MutableList<int>::of({1, 2, 3, 4, 5});
    
    int sum = list.reduce<int>(0, [](const int& acc, const int& x) {
        return acc + x;
    });
    
    EXPECT_EQ(sum, 15);
}

// Test sort operation
TEST(MutableListTest, Sort) {
    auto list = MutableList<int>::of({5, 2, 8, 1, 9, 3});
    
    list.sort();
    
    EXPECT_EQ(list.get(0), 1);
    EXPECT_EQ(list.get(1), 2);
    EXPECT_EQ(list.get(5), 9);
}

// Test sort with comparator
TEST(MutableListTest, SortWithComparator) {
    auto list = MutableList<int>::of({5, 2, 8, 1, 9, 3});
    
    list.sort([](const int& a, const int& b) {
        return a > b;  // Descending
    });
    
    EXPECT_EQ(list.get(0), 9);
    EXPECT_EQ(list.get(5), 1);
}

// Test reverse operation
TEST(MutableListTest, Reverse) {
    auto list = MutableList<int>::of({1, 2, 3, 4, 5});
    
    list.reverse();
    
    EXPECT_EQ(list.get(0), 5);
    EXPECT_EQ(list.get(4), 1);
}

// Test range-based for loop
TEST(MutableListTest, RangeBasedFor) {
    auto list = MutableList<int>::of({1, 2, 3, 4, 5});
    
    int sum = 0;
    for (const auto& x : list) {
        sum += x;
    }
    
    EXPECT_EQ(sum, 15);
}

// Test mutable range-based for loop
TEST(MutableListTest, MutableRangeBasedFor) {
    auto list = MutableList<int>::of({1, 2, 3});
    
    for (auto& x : list) {
        x *= 2;
    }
    
    EXPECT_EQ(list.get(0), 2);
    EXPECT_EQ(list.get(1), 4);
    EXPECT_EQ(list.get(2), 6);
}
