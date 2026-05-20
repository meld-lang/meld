#include <gtest/gtest.h>
#include "meld/types/parallel_executor.hpp"
#include "meld/types/collection.hpp"
#include <vector>
#include <numeric>
#include <chrono>
#include <thread>

using namespace meld;

class ParallelExecutionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test data
        data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        largeData.resize(10000);
        std::iota(largeData.begin(), largeData.end(), 1);
    }
    
    std::vector<int> data;
    std::vector<int> largeData;
};

// Test parallel forEach produces same results as sequential
TEST_F(ParallelExecutionTest, ParallelForEachCorrectness) {
    std::atomic<int> parallelSum{0};
    int sequentialSum = 0;
    
    // Parallel execution
    ParallelExecutor::forEach<int>(data, [&parallelSum](const int& x) {
        parallelSum += x;
    });
    
    // Sequential execution
    for (const auto& x : data) {
        sequentialSum += x;
    }
    
    EXPECT_EQ(parallelSum.load(), sequentialSum);
    EXPECT_EQ(parallelSum.load(), 55); // 1+2+...+10 = 55
}

// Test parallel map produces same results as sequential
TEST_F(ParallelExecutionTest, ParallelMapCorrectness) {
    auto parallelResult = ParallelExecutor::map<int, int>(data, [](const int& x) {
        return x * 2;
    });
    
    std::vector<int> sequentialResult;
    for (const auto& x : data) {
        sequentialResult.push_back(x * 2);
    }
    
    EXPECT_EQ(parallelResult.size(), sequentialResult.size());
    for (size_t i = 0; i < parallelResult.size(); ++i) {
        EXPECT_EQ(parallelResult[i], sequentialResult[i]);
    }
}

// Test parallel filter produces same results as sequential
TEST_F(ParallelExecutionTest, ParallelFilterCorrectness) {
    auto parallelResult = ParallelExecutor::filter<int>(data, [](const int& x) {
        return x % 2 == 0;
    });
    
    std::vector<int> sequentialResult;
    for (const auto& x : data) {
        if (x % 2 == 0) {
            sequentialResult.push_back(x);
        }
    }
    
    // Sort both results since parallel filter may not preserve order
    std::sort(parallelResult.begin(), parallelResult.end());
    std::sort(sequentialResult.begin(), sequentialResult.end());
    
    EXPECT_EQ(parallelResult.size(), sequentialResult.size());
    for (size_t i = 0; i < parallelResult.size(); ++i) {
        EXPECT_EQ(parallelResult[i], sequentialResult[i]);
    }
}

// Test parallel reduce produces same results as sequential
TEST_F(ParallelExecutionTest, ParallelReduceCorrectness) {
    int parallelResult = ParallelExecutor::reduce<int, int>(data, 0, [](const int& acc, const int& x) {
        return acc + x;
    });
    
    int sequentialResult = std::accumulate(data.begin(), data.end(), 0);
    
    EXPECT_EQ(parallelResult, sequentialResult);
    EXPECT_EQ(parallelResult, 55);
}

// Test parallel sort produces same results as sequential
TEST_F(ParallelExecutionTest, ParallelSortCorrectness) {
    std::vector<int> unsorted = {5, 2, 8, 1, 9, 3, 7, 4, 6, 10};
    
    auto parallelResult = ParallelExecutor::sort(unsorted);
    
    std::vector<int> sequentialResult = unsorted;
    std::sort(sequentialResult.begin(), sequentialResult.end());
    
    EXPECT_EQ(parallelResult.size(), sequentialResult.size());
    for (size_t i = 0; i < parallelResult.size(); ++i) {
        EXPECT_EQ(parallelResult[i], sequentialResult[i]);
    }
}

// Test parallel sort with custom comparator
TEST_F(ParallelExecutionTest, ParallelSortWithComparatorCorrectness) {
    std::vector<int> unsorted = {5, 2, 8, 1, 9, 3, 7, 4, 6, 10};
    
    auto comparator = [](const int& a, const int& b) { return a > b; };
    auto parallelResult = ParallelExecutor::sort<int>(unsorted, comparator);
    
    std::vector<int> sequentialResult = unsorted;
    std::sort(sequentialResult.begin(), sequentialResult.end(), comparator);
    
    EXPECT_EQ(parallelResult.size(), sequentialResult.size());
    for (size_t i = 0; i < parallelResult.size(); ++i) {
        EXPECT_EQ(parallelResult[i], sequentialResult[i]);
    }
}

// Test collection parallel modifier integration
TEST_F(ParallelExecutionTest, CollectionParallelModifier) {
    auto collection = makeCollection(data);
    auto parallelCollection = collection->parallel();
    
    // Verify parallel collection is marked as parallel
    auto vectorColl = std::dynamic_pointer_cast<VectorCollection<int>>(parallelCollection);
    ASSERT_TRUE(vectorColl != nullptr);
    EXPECT_TRUE(vectorColl->isParallel());
    
    // Test parallel operations produce correct results
    std::atomic<int> sum{0};
    parallelCollection->forEach([&sum](const int& x) {
        sum += x;
    });
    EXPECT_EQ(sum.load(), 55);
    
    auto doubled = parallelCollection->map([](const int& x) { return x * 2; });
    auto result = doubled->toList();
    EXPECT_EQ(result.size(), 10);
    
    // Verify all elements are doubled correctly
    for (size_t i = 0; i < result.size(); ++i) {
        bool found = false;
        for (int original : data) {
            if (result[i] == original * 2) {
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found) << "Element " << result[i] << " not found in expected doubled values";
    }
}

// Test parallel execution with larger dataset for performance
TEST_F(ParallelExecutionTest, LargeDatasetPerformance) {
    // This test verifies parallel execution works with larger datasets
    // We don't test for performance improvement as that depends on hardware
    
    std::atomic<long long> sum{0};
    ParallelExecutor::forEach<int>(largeData, [&sum](const int& x) {
        sum += x;
    });
    
    // Expected sum: 1+2+...+10000 = 10000*10001/2 = 50005000
    long long expected = static_cast<long long>(largeData.size()) * (largeData.size() + 1) / 2;
    EXPECT_EQ(sum.load(), expected);
}

// Test parallel map with type transformation
TEST_F(ParallelExecutionTest, ParallelMapTypeTransformation) {
    auto stringResult = ParallelExecutor::map<int, std::string>(data, [](const int& x) {
        return "num_" + std::to_string(x);
    });
    
    EXPECT_EQ(stringResult.size(), data.size());
    for (size_t i = 0; i < stringResult.size(); ++i) {
        // Find the corresponding original value
        bool found = false;
        for (int original : data) {
            if (stringResult[i] == "num_" + std::to_string(original)) {
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found) << "String " << stringResult[i] << " not found in expected values";
    }
}

// Test parallel reduce with different accumulator type
TEST_F(ParallelExecutionTest, ParallelReduceTypeTransformation) {
    std::string result = ParallelExecutor::reduce<int, std::string>(
        std::vector<int>{1, 2, 3}, 
        std::string(""), 
        [](const std::string& acc, const int& x) {
            return acc + std::to_string(x);
        }
    );
    
    // Result should contain all digits, but order may vary due to parallel execution
    EXPECT_EQ(result.length(), 3);
    EXPECT_TRUE(result.find('1') != std::string::npos);
    EXPECT_TRUE(result.find('2') != std::string::npos);
    EXPECT_TRUE(result.find('3') != std::string::npos);
}

// Test empty collection handling
TEST_F(ParallelExecutionTest, EmptyCollectionHandling) {
    std::vector<int> empty;
    
    // forEach with empty collection should not crash
    ParallelExecutor::forEach<int>(empty, [](const int& x) {
        // Should never be called
        FAIL() << "forEach called on empty collection";
    });
    
    // map with empty collection should return empty result
    auto mapResult = ParallelExecutor::map<int, int>(empty, [](const int& x) {
        return x * 2;
    });
    EXPECT_TRUE(mapResult.empty());
    
    // filter with empty collection should return empty result
    auto filterResult = ParallelExecutor::filter<int>(empty, [](const int& x) {
        return true;
    });
    EXPECT_TRUE(filterResult.empty());
    
    // reduce with empty collection should return initial value
    int reduceResult = ParallelExecutor::reduce<int, int>(empty, 42, [](const int& acc, const int& x) {
        return acc + x;
    });
    EXPECT_EQ(reduceResult, 42);
    
    // sort with empty collection should return empty result
    auto sortResult = ParallelExecutor::sort(empty);
    EXPECT_TRUE(sortResult.empty());
}

// Test thread safety with concurrent access
TEST_F(ParallelExecutionTest, ThreadSafety) {
    const int numThreads = 4;
    const int numOperations = 100;
    std::vector<std::thread> threads;
    std::vector<int> results(numThreads * numOperations);
    
    // Launch multiple threads performing parallel operations
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < numOperations; ++i) {
                int sum = ParallelExecutor::reduce<int, int>(data, 0, [](const int& acc, const int& x) {
                    return acc + x;
                });
                results[t * numOperations + i] = sum;
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify all results are correct
    for (int result : results) {
        EXPECT_EQ(result, 55);
    }
}