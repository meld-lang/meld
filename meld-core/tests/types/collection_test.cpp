#include <gtest/gtest.h>
#include "meld/types/collection.hpp"
#include <string>

using namespace meld;

// Test basic collection creation and iteration
TEST(CollectionTest, ForEach) {
    std::vector<int> data = {1, 2, 3, 4, 5};
    auto collection = makeCollection(data);
    
    int sum = 0;
    collection->forEach([&sum](const int& item) {
        sum += item;
    });
    
    EXPECT_EQ(sum, 15);
}

// Test map operation
TEST(CollectionTest, Map) {
    std::vector<int> data = {1, 2, 3, 4, 5};
    auto collection = makeCollection(data);
    
    auto doubled = collection->map([](const int& x) { return x * 2; });
    auto result = doubled->toList();
    
    EXPECT_EQ(result.size(), 5);
    EXPECT_EQ(result[0], 2);
    EXPECT_EQ(result[1], 4);
    EXPECT_EQ(result[2], 6);
    EXPECT_EQ(result[3], 8);
    EXPECT_EQ(result[4], 10);
}

// Test map with type transformation
TEST(CollectionTest, MapTypeTransform) {
    std::vector<int> data = {1, 2, 3};
    auto collection = makeCollection(data);
    
    auto strings = collection->map<std::string>([](const int& x) {
        return std::to_string(x);
    });
    
    auto result = strings->toList();
    
    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], "1");
    EXPECT_EQ(result[1], "2");
    EXPECT_EQ(result[2], "3");
}

// Test filter operation
TEST(CollectionTest, Filter) {
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto collection = makeCollection(data);
    
    auto evens = collection->filter([](const int& x) { return x % 2 == 0; });
    auto result = evens->toList();
    
    EXPECT_EQ(result.size(), 5);
    EXPECT_EQ(result[0], 2);
    EXPECT_EQ(result[1], 4);
    EXPECT_EQ(result[2], 6);
    EXPECT_EQ(result[3], 8);
    EXPECT_EQ(result[4], 10);
}

// Test reduce operation
TEST(CollectionTest, Reduce) {
    std::vector<int> data = {1, 2, 3, 4, 5};
    auto collection = makeCollection(data);
    
    int sum = collection->reduce(0, [](const int& acc, const int& x) {
        return acc + x;
    });
    
    EXPECT_EQ(sum, 15);
}

// Test reduce with different type
TEST(CollectionTest, ReduceTypeTransform) {
    std::vector<int> data = {1, 2, 3};
    auto collection = makeCollection(data);
    
    std::string result = collection->reduce<std::string>("", [](const std::string& acc, const int& x) {
        return acc + std::to_string(x);
    });
    
    EXPECT_EQ(result, "123");
}

// Test groupBy operation
TEST(CollectionTest, GroupBy) {
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto collection = makeCollection(data);
    
    auto groups = collection->groupBy<std::string>([](const int& x) {
        return (x % 2 == 0) ? "even" : "odd";
    });
    
    EXPECT_EQ(groups.size(), 2);
    EXPECT_EQ(groups["even"].size(), 5);
    EXPECT_EQ(groups["odd"].size(), 5);
    EXPECT_EQ(groups["even"][0], 2);
    EXPECT_EQ(groups["odd"][0], 1);
}

// Test join operation
TEST(CollectionTest, Join) {
    std::vector<int> left = {1, 2, 3};
    std::vector<int> right = {2, 3, 4};
    
    auto leftCollection = makeCollection(left);
    auto rightCollection = makeCollection(right);
    
    auto joined = leftCollection->template join<int>(*rightCollection, [](const int& l, const int& r) {
        return l == r;
    });
    
    auto result = joined->toList();
    
    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result[0].first, 2);
    EXPECT_EQ(result[0].second, 2);
    EXPECT_EQ(result[1].first, 3);
    EXPECT_EQ(result[1].second, 3);
}

// Test sorted operation
TEST(CollectionTest, Sorted) {
    std::vector<int> data = {5, 2, 8, 1, 9, 3};
    auto collection = makeCollection(data);
    
    auto sorted = collection->sorted();
    auto result = sorted->toList();
    
    EXPECT_EQ(result.size(), 6);
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[1], 2);
    EXPECT_EQ(result[2], 3);
    EXPECT_EQ(result[3], 5);
    EXPECT_EQ(result[4], 8);
    EXPECT_EQ(result[5], 9);
}

// Test sorted with custom comparator
TEST(CollectionTest, SortedWithComparator) {
    std::vector<int> data = {5, 2, 8, 1, 9, 3};
    auto collection = makeCollection(data);
    
    auto sorted = collection->sorted([](const int& a, const int& b) {
        return a > b;  // Descending order
    });
    auto result = sorted->toList();
    
    EXPECT_EQ(result.size(), 6);
    EXPECT_EQ(result[0], 9);
    EXPECT_EQ(result[1], 8);
    EXPECT_EQ(result[2], 5);
    EXPECT_EQ(result[3], 3);
    EXPECT_EQ(result[4], 2);
    EXPECT_EQ(result[5], 1);
}

// Test take operation
TEST(CollectionTest, Take) {
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto collection = makeCollection(data);
    
    auto taken = collection->take(5);
    auto result = taken->toList();
    
    EXPECT_EQ(result.size(), 5);
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[4], 5);
}

// Test take with count larger than collection size
TEST(CollectionTest, TakeMoreThanSize) {
    std::vector<int> data = {1, 2, 3};
    auto collection = makeCollection(data);
    
    auto taken = collection->take(10);
    auto result = taken->toList();
    
    EXPECT_EQ(result.size(), 3);
}

// Test method chaining
TEST(CollectionTest, MethodChaining) {
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto collection = makeCollection(data);
    
    auto result = collection
        ->filter([](const int& x) { return x % 2 == 0; })  // Get evens: 2,4,6,8,10
        ->map([](const int& x) { return x * 2; })          // Double: 4,8,12,16,20
        ->sorted([](const int& a, const int& b) { return a > b; })  // Descending
        ->take(3)                                           // Take first 3: 20,16,12
        ->toList();
    
    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], 20);
    EXPECT_EQ(result[1], 16);
    EXPECT_EQ(result[2], 12);
}

// Test count operation
TEST(CollectionTest, Count) {
    std::vector<int> data = {1, 2, 3, 4, 5};
    auto collection = makeCollection(data);
    
    EXPECT_EQ(collection->count(), 5);
}

// Test isEmpty operation
TEST(CollectionTest, IsEmpty) {
    std::vector<int> empty;
    auto emptyCollection = makeCollection(empty);
    
    std::vector<int> notEmpty = {1, 2, 3};
    auto notEmptyCollection = makeCollection(notEmpty);
    
    EXPECT_TRUE(emptyCollection->isEmpty());
    EXPECT_FALSE(notEmptyCollection->isEmpty());
}

// Test first operation
TEST(CollectionTest, First) {
    std::vector<int> data = {1, 2, 3};
    auto collection = makeCollection(data);
    
    auto first = collection->first();
    EXPECT_TRUE(first.has_value());
    EXPECT_EQ(first.value(), 1);
    
    std::vector<int> empty;
    auto emptyCollection = makeCollection(empty);
    auto emptyFirst = emptyCollection->first();
    EXPECT_FALSE(emptyFirst.has_value());
}

// Test last operation
TEST(CollectionTest, Last) {
    std::vector<int> data = {1, 2, 3};
    auto collection = makeCollection(data);
    
    auto last = collection->last();
    EXPECT_TRUE(last.has_value());
    EXPECT_EQ(last.value(), 3);
    
    std::vector<int> empty;
    auto emptyCollection = makeCollection(empty);
    auto emptyLast = emptyCollection->last();
    EXPECT_FALSE(emptyLast.has_value());
}

// Test parallel modifier
TEST(CollectionTest, Parallel) {
    std::vector<int> data = {1, 2, 3, 4, 5};
    auto collection = makeCollection(data);
    
    auto parallelCollection = collection->parallel();
    
    // Verify it's marked as parallel (implementation-specific)
    auto vectorColl = std::dynamic_pointer_cast<VectorCollection<int>>(parallelCollection);
    EXPECT_TRUE(vectorColl != nullptr);
    EXPECT_TRUE(vectorColl->isParallel());
}
