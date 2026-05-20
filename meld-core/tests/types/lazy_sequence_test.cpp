#include <gtest/gtest.h>
#include "meld/types/collection.hpp"
#include "meld/types/lazy_sequence.hpp"
#include <string>
#include <set>
#include <utility>

using namespace meld;

// Test that operations are deferred
TEST(LazySequenceTest, DeferredExecution) {
    std::vector<int> data = {1, 2, 3, 4, 5};
    auto collection = makeCollection(data);
    
    int executionCount = 0;
    
    // Create lazy sequence with map - should not execute yet
    auto lazy = collection->lazy()->map([&executionCount](const int& x) {
        executionCount++;
        return x * 2;
    });
    
    // No execution should have happened yet
    EXPECT_EQ(executionCount, 0);
    
    // Trigger execution with terminal operation
    auto result = lazy->toList();
    
    // Now execution should have happened
    EXPECT_EQ(executionCount, 5);
    EXPECT_EQ(result.size(), 5);
}

// Test lazy map operation
TEST(LazySequenceTest, LazyMap) {
    std::vector<int> data = {1, 2, 3, 4, 5};
    auto collection = makeCollection(data);
    
    auto result = collection->lazy()
        ->map([](const int& x) { return x * 2; })
        ->toList();
    
    EXPECT_EQ(result.size(), 5);
    EXPECT_EQ(result[0], 2);
    EXPECT_EQ(result[1], 4);
    EXPECT_EQ(result[2], 6);
    EXPECT_EQ(result[3], 8);
    EXPECT_EQ(result[4], 10);
}

// Test lazy filter operation
TEST(LazySequenceTest, LazyFilter) {
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto collection = makeCollection(data);
    
    auto result = collection->lazy()
        ->filter([](const int& x) { return x % 2 == 0; })
        ->toList();
    
    EXPECT_EQ(result.size(), 5);
    EXPECT_EQ(result[0], 2);
    EXPECT_EQ(result[1], 4);
    EXPECT_EQ(result[2], 6);
    EXPECT_EQ(result[3], 8);
    EXPECT_EQ(result[4], 10);
}

// Test lazy take operation
TEST(LazySequenceTest, LazyTake) {
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto collection = makeCollection(data);
    
    auto result = collection->lazy()
        ->take(3)
        ->toList();
    
    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[1], 2);
    EXPECT_EQ(result[2], 3);
}

// Test lazy sorted operation
TEST(LazySequenceTest, LazySorted) {
    std::vector<int> data = {5, 2, 8, 1, 9, 3};
    auto collection = makeCollection(data);
    
    auto result = collection->lazy()
        ->sorted()
        ->toList();
    
    EXPECT_EQ(result.size(), 6);
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[1], 2);
    EXPECT_EQ(result[2], 3);
    EXPECT_EQ(result[3], 5);
    EXPECT_EQ(result[4], 8);
    EXPECT_EQ(result[5], 9);
}

// Test lazy chaining
TEST(LazySequenceTest, LazyChaining) {
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto collection = makeCollection(data);
    
    int mapCount = 0;
    int filterCount = 0;
    
    auto lazy = collection->lazy()
        ->filter([&filterCount](const int& x) {
            filterCount++;
            return x % 2 == 0;
        })
        ->map([&mapCount](const int& x) {
            mapCount++;
            return x * 2;
        })
        ->take(3);
    
    // No execution yet
    EXPECT_EQ(mapCount, 0);
    EXPECT_EQ(filterCount, 0);
    
    // Trigger execution
    auto result = lazy->toList();
    
    // Execution happened
    EXPECT_GT(filterCount, 0);
    EXPECT_GT(mapCount, 0);
    
    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], 4);   // 2 * 2
    EXPECT_EQ(result[1], 8);   // 4 * 2
    EXPECT_EQ(result[2], 12);  // 6 * 2
}

// Test forEach terminal operation
TEST(LazySequenceTest, ForEachTerminal) {
    std::vector<int> data = {1, 2, 3, 4, 5};
    auto collection = makeCollection(data);
    
    int sum = 0;
    collection->lazy()
        ->map([](const int& x) { return x * 2; })
        ->forEach([&sum](const int& x) {
            sum += x;
        });
    
    EXPECT_EQ(sum, 30);  // (1+2+3+4+5) * 2
}

// Test reduce terminal operation
TEST(LazySequenceTest, ReduceTerminal) {
    std::vector<int> data = {1, 2, 3, 4, 5};
    auto collection = makeCollection(data);
    
    int result = collection->lazy()
        ->map([](const int& x) { return x * 2; })
        ->reduce(0, [](const int& acc, const int& x) {
            return acc + x;
        });
    
    EXPECT_EQ(result, 30);
}

// Test count terminal operation
TEST(LazySequenceTest, CountTerminal) {
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto collection = makeCollection(data);
    
    size_t count = collection->lazy()
        ->filter([](const int& x) { return x % 2 == 0; })
        ->count();
    
    EXPECT_EQ(count, 5);
}

// Test isEmpty terminal operation
TEST(LazySequenceTest, IsEmptyTerminal) {
    std::vector<int> data = {1, 2, 3};
    auto collection = makeCollection(data);
    
    bool empty1 = collection->lazy()
        ->filter([](const int& x) { return x > 10; })
        ->isEmpty();
    
    bool empty2 = collection->lazy()
        ->filter([](const int& x) { return x > 0; })
        ->isEmpty();
    
    EXPECT_TRUE(empty1);
    EXPECT_FALSE(empty2);
}

// Test first terminal operation
TEST(LazySequenceTest, FirstTerminal) {
    std::vector<int> data = {1, 2, 3, 4, 5};
    auto collection = makeCollection(data);
    
    auto first = collection->lazy()
        ->filter([](const int& x) { return x % 2 == 0; })
        ->first();
    
    EXPECT_TRUE(first.has_value());
    EXPECT_EQ(first.value(), 2);
}

// Test last terminal operation
TEST(LazySequenceTest, LastTerminal) {
    std::vector<int> data = {1, 2, 3, 4, 5};
    auto collection = makeCollection(data);
    
    auto last = collection->lazy()
        ->filter([](const int& x) { return x % 2 == 0; })
        ->last();
    
    EXPECT_TRUE(last.has_value());
    EXPECT_EQ(last.value(), 4);
}

// Test toCollection terminal operation
TEST(LazySequenceTest, ToCollectionTerminal) {
    std::vector<int> data = {1, 2, 3, 4, 5};
    auto collection = makeCollection(data);
    
    auto newCollection = collection->lazy()
        ->map([](const int& x) { return x * 2; })
        ->toCollection();
    
    auto result = newCollection->toList();
    
    EXPECT_EQ(result.size(), 5);
    EXPECT_EQ(result[0], 2);
    EXPECT_EQ(result[4], 10);
}

// Test that lazy operations don't modify original collection
TEST(LazySequenceTest, ImmutabilityPreserved) {
    std::vector<int> data = {1, 2, 3, 4, 5};
    auto collection = makeCollection(data);
    
    // Create lazy sequence and evaluate
    collection->lazy()
        ->map([](const int& x) { return x * 100; })
        ->toList();
    
    // Original collection should be unchanged
    auto original = collection->toList();
    EXPECT_EQ(original[0], 1);
    EXPECT_EQ(original[4], 5);
}

// Test complex lazy pipeline
TEST(LazySequenceTest, ComplexPipeline) {
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto collection = makeCollection(data);
    
    auto result = collection->lazy()
        ->filter([](const int& x) { return x > 3; })      // 4,5,6,7,8,9,10
        ->map([](const int& x) { return x * 2; })         // 8,10,12,14,16,18,20
        ->filter([](const int& x) { return x < 17; })     // 8,10,12,14,16
        ->sorted([](const int& a, const int& b) { return a > b; })  // 16,14,12,10,8
        ->take(3)                                          // 16,14,12
        ->toList();
    
    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], 16);
    EXPECT_EQ(result[1], 14);
    EXPECT_EQ(result[2], 12);
}

// Test type transformation with lazy map
TEST(LazySequenceTest, LazyMapTypeTransform) {
    std::vector<int> data = {1, 2, 3};
    auto collection = makeCollection(data);
    
    auto result = collection->lazy()
        ->map<std::string>([](const int& x) {
            return std::to_string(x);
        })
        ->toList();
    
    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], "1");
    EXPECT_EQ(result[1], "2");
    EXPECT_EQ(result[2], "3");
}

// Test toSet terminal operation
TEST(LazySequenceTest, ToSetTerminal) {
    std::vector<int> data = {1, 2, 2, 3, 3, 3, 4, 5};
    auto collection = makeCollection(data);
    
    auto result = collection->lazy()
        ->filter([](const int& x) { return x > 2; })
        ->toSet();
    
    EXPECT_EQ(result.size(), 3);  // 3, 4, 5 (duplicates removed)
    EXPECT_TRUE(result.count(3) == 1);
    EXPECT_TRUE(result.count(4) == 1);
    EXPECT_TRUE(result.count(5) == 1);
    EXPECT_FALSE(result.count(1) == 1);
    EXPECT_FALSE(result.count(2) == 1);
}

// Test lazy groupBy operation
TEST(LazySequenceTest, LazyGroupBy) {
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto collection = makeCollection(data);
    
    auto result = collection->lazy()
        ->groupBy<bool>([](const int& x) { return x % 2 == 0; })
        ->toList();
    
    EXPECT_EQ(result.size(), 2);  // Two groups: even and odd
    
    // Find even and odd groups
    bool foundEven = false, foundOdd = false;
    for (const auto& group : result) {
        if (group.first) {  // Even group
            foundEven = true;
            EXPECT_EQ(group.second.size(), 5);  // 2, 4, 6, 8, 10
            for (const auto& value : group.second) {
                EXPECT_EQ(value % 2, 0);
            }
        } else {  // Odd group
            foundOdd = true;
            EXPECT_EQ(group.second.size(), 5);  // 1, 3, 5, 7, 9
            for (const auto& value : group.second) {
                EXPECT_EQ(value % 2, 1);
            }
        }
    }
    
    EXPECT_TRUE(foundEven);
    EXPECT_TRUE(foundOdd);
}

// Test lazy join operation
TEST(LazySequenceTest, LazyJoin) {
    std::vector<int> left = {1, 2, 3, 4};
    std::vector<int> right = {2, 3, 4, 5};
    auto leftCollection = makeCollection(left);
    auto rightCollection = makeCollection(right);
    
    auto result = leftCollection->lazy()
        ->join<int>(rightCollection, [](const int& l, const int& r) { return l == r; })
        ->toList();
    
    EXPECT_EQ(result.size(), 3);  // (2,2), (3,3), (4,4)
    
    // Verify the joined pairs
    std::set<std::pair<int, int>> expectedPairs = {{2, 2}, {3, 3}, {4, 4}};
    std::set<std::pair<int, int>> actualPairs(result.begin(), result.end());
    
    EXPECT_EQ(actualPairs, expectedPairs);
}

// Test lazy chaining with groupBy
TEST(LazySequenceTest, LazyGroupByChaining) {
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto collection = makeCollection(data);
    
    int filterCount = 0;
    
    auto lazy = collection->lazy()
        ->filter([&filterCount](const int& x) {
            filterCount++;
            return x > 5;  // 6, 7, 8, 9, 10
        })
        ->groupBy<bool>([](const int& x) { return x % 2 == 0; });
    
    // No execution yet
    EXPECT_EQ(filterCount, 0);
    
    // Trigger execution
    auto result = lazy->toList();
    
    // Execution happened
    EXPECT_GT(filterCount, 0);
    
    EXPECT_EQ(result.size(), 2);  // Two groups: even and odd
    
    // Verify groups contain only filtered values (> 5)
    for (const auto& group : result) {
        for (const auto& value : group.second) {
            EXPECT_GT(value, 5);
        }
    }
}
