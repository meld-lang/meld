#include <gtest/gtest.h>
#include "meld/types/anonymous_types.hpp"
#include "meld/meta/metatype.hpp"

using namespace meld::types;
using namespace meld::meta;

class ArrayOperationsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create basic types for testing
        intType = std::make_shared<PrimitiveMetaType>("int", 8, true);
        stringType = std::make_shared<PrimitiveMetaType>("string", 32, false);
    }
    
    std::shared_ptr<MetaType> intType;
    std::shared_ptr<MetaType> stringType;
};

TEST_F(ArrayOperationsTest, BasicArrayOperations) {
    auto arrayType = std::make_shared<AnonymousArrayType>(intType);
    auto arrayValue = std::make_shared<AnonymousArrayValue>(arrayType);
    
    // Test empty array
    EXPECT_TRUE(arrayValue->isEmpty());
    EXPECT_EQ(arrayValue->size(), 0);
    
    // Test push operations
    arrayValue->push(1);
    arrayValue->push(2);
    arrayValue->push(3);
    
    EXPECT_FALSE(arrayValue->isEmpty());
    EXPECT_EQ(arrayValue->size(), 3);
    
    // Test get operations
    auto elem0 = arrayValue->get(0);
    ASSERT_TRUE(elem0.has_value());
    EXPECT_EQ(std::get<int>(*elem0), 1);
    
    auto elem1 = arrayValue->get(1);
    ASSERT_TRUE(elem1.has_value());
    EXPECT_EQ(std::get<int>(*elem1), 2);
    
    auto elem2 = arrayValue->get(2);
    ASSERT_TRUE(elem2.has_value());
    EXPECT_EQ(std::get<int>(*elem2), 3);
    
    // Test out of bounds
    auto elem3 = arrayValue->get(3);
    EXPECT_FALSE(elem3.has_value());
}

TEST_F(ArrayOperationsTest, ArraySetOperation) {
    auto arrayType = std::make_shared<AnonymousArrayType>(intType);
    auto arrayValue = std::make_shared<AnonymousArrayValue>(arrayType);
    
    // Add some elements
    arrayValue->push(10);
    arrayValue->push(20);
    arrayValue->push(30);
    
    // Test set operation
    arrayValue->set(1, 25);
    
    auto elem1 = arrayValue->get(1);
    ASSERT_TRUE(elem1.has_value());
    EXPECT_EQ(std::get<int>(*elem1), 25);
    
    // Test set out of bounds (should not crash)
    arrayValue->set(10, 100);  // Should be ignored
    EXPECT_EQ(arrayValue->size(), 3);
}

TEST_F(ArrayOperationsTest, ArrayPopOperation) {
    auto arrayType = std::make_shared<AnonymousArrayType>(intType);
    auto arrayValue = std::make_shared<AnonymousArrayValue>(arrayType);
    
    // Add some elements
    arrayValue->push(1);
    arrayValue->push(2);
    arrayValue->push(3);
    
    // Test pop operation
    auto popped = arrayValue->pop();
    ASSERT_TRUE(popped.has_value());
    EXPECT_EQ(std::get<int>(*popped), 3);
    EXPECT_EQ(arrayValue->size(), 2);
    
    // Pop again
    popped = arrayValue->pop();
    ASSERT_TRUE(popped.has_value());
    EXPECT_EQ(std::get<int>(*popped), 2);
    EXPECT_EQ(arrayValue->size(), 1);
    
    // Pop last element
    popped = arrayValue->pop();
    ASSERT_TRUE(popped.has_value());
    EXPECT_EQ(std::get<int>(*popped), 1);
    EXPECT_EQ(arrayValue->size(), 0);
    EXPECT_TRUE(arrayValue->isEmpty());
    
    // Pop from empty array
    popped = arrayValue->pop();
    EXPECT_FALSE(popped.has_value());
}

TEST_F(ArrayOperationsTest, ArraySliceOperation) {
    auto arrayType = std::make_shared<AnonymousArrayType>(intType);
    auto arrayValue = std::make_shared<AnonymousArrayValue>(arrayType);
    
    // Add elements [1, 2, 3, 4, 5]
    for (int i = 1; i <= 5; ++i) {
        arrayValue->push(i);
    }
    
    // Test slice [1, 3) -> [2, 3]
    auto sliced = arrayValue->slice(1, 3);
    EXPECT_EQ(sliced->size(), 2);
    
    auto elem0 = sliced->get(0);
    ASSERT_TRUE(elem0.has_value());
    EXPECT_EQ(std::get<int>(*elem0), 2);
    
    auto elem1 = sliced->get(1);
    ASSERT_TRUE(elem1.has_value());
    EXPECT_EQ(std::get<int>(*elem1), 3);
    
    // Test slice entire array
    auto full_slice = arrayValue->slice(0, 5);
    EXPECT_EQ(full_slice->size(), 5);
    
    // Test invalid slice
    auto invalid_slice = arrayValue->slice(3, 2);  // start > end
    EXPECT_EQ(invalid_slice->size(), 0);
}

TEST_F(ArrayOperationsTest, ArrayInsertOperation) {
    auto arrayType = std::make_shared<AnonymousArrayType>(intType);
    auto arrayValue = std::make_shared<AnonymousArrayValue>(arrayType);
    
    // Add elements [1, 2, 3]
    arrayValue->push(1);
    arrayValue->push(2);
    arrayValue->push(3);
    
    // Insert at beginning
    arrayValue->insert(0, 0);
    EXPECT_EQ(arrayValue->size(), 4);
    
    auto elem0 = arrayValue->get(0);
    ASSERT_TRUE(elem0.has_value());
    EXPECT_EQ(std::get<int>(*elem0), 0);
    
    // Insert in middle
    arrayValue->insert(2, 15);  // [0, 1, 15, 2, 3]
    EXPECT_EQ(arrayValue->size(), 5);
    
    auto elem2 = arrayValue->get(2);
    ASSERT_TRUE(elem2.has_value());
    EXPECT_EQ(std::get<int>(*elem2), 15);
    
    // Insert at end
    arrayValue->insert(5, 99);
    EXPECT_EQ(arrayValue->size(), 6);
    
    auto elem5 = arrayValue->get(5);
    ASSERT_TRUE(elem5.has_value());
    EXPECT_EQ(std::get<int>(*elem5), 99);
}

TEST_F(ArrayOperationsTest, ArrayRemoveOperation) {
    auto arrayType = std::make_shared<AnonymousArrayType>(intType);
    auto arrayValue = std::make_shared<AnonymousArrayValue>(arrayType);
    
    // Add elements [1, 2, 3, 4, 5]
    for (int i = 1; i <= 5; ++i) {
        arrayValue->push(i);
    }
    
    // Remove from middle
    arrayValue->remove(2);  // Remove 3, array becomes [1, 2, 4, 5]
    EXPECT_EQ(arrayValue->size(), 4);
    
    auto elem2 = arrayValue->get(2);
    ASSERT_TRUE(elem2.has_value());
    EXPECT_EQ(std::get<int>(*elem2), 4);  // 4 moved to index 2
    
    // Remove from beginning
    arrayValue->remove(0);  // Remove 1, array becomes [2, 4, 5]
    EXPECT_EQ(arrayValue->size(), 3);
    
    auto elem0 = arrayValue->get(0);
    ASSERT_TRUE(elem0.has_value());
    EXPECT_EQ(std::get<int>(*elem0), 2);
    
    // Remove from end
    arrayValue->remove(2);  // Remove 5, array becomes [2, 4]
    EXPECT_EQ(arrayValue->size(), 2);
    
    auto elem1 = arrayValue->get(1);
    ASSERT_TRUE(elem1.has_value());
    EXPECT_EQ(std::get<int>(*elem1), 4);
}

TEST_F(ArrayOperationsTest, ArrayClearOperation) {
    auto arrayType = std::make_shared<AnonymousArrayType>(intType);
    auto arrayValue = std::make_shared<AnonymousArrayValue>(arrayType);
    
    // Add some elements
    for (int i = 1; i <= 10; ++i) {
        arrayValue->push(i);
    }
    
    EXPECT_EQ(arrayValue->size(), 10);
    EXPECT_FALSE(arrayValue->isEmpty());
    
    // Clear the array
    arrayValue->clear();
    
    EXPECT_EQ(arrayValue->size(), 0);
    EXPECT_TRUE(arrayValue->isEmpty());
    
    // Verify no elements can be retrieved
    auto elem = arrayValue->get(0);
    EXPECT_FALSE(elem.has_value());
}

TEST_F(ArrayOperationsTest, EmptyArrayFactory) {
    auto emptyArray = AnonymousArrayValue::empty(intType);
    
    EXPECT_TRUE(emptyArray->isEmpty());
    EXPECT_EQ(emptyArray->size(), 0);
    EXPECT_EQ(emptyArray->type()->elementType(), intType);
}

TEST_F(ArrayOperationsTest, StringArrayOperations) {
    auto arrayType = std::make_shared<AnonymousArrayType>(stringType);
    auto arrayValue = std::make_shared<AnonymousArrayValue>(arrayType);
    
    // Test with string elements
    arrayValue->push(std::string("hello"));
    arrayValue->push(std::string("world"));
    arrayValue->push(std::string("test"));
    
    EXPECT_EQ(arrayValue->size(), 3);
    
    auto elem0 = arrayValue->get(0);
    ASSERT_TRUE(elem0.has_value());
    EXPECT_EQ(std::get<std::string>(*elem0), "hello");
    
    auto elem1 = arrayValue->get(1);
    ASSERT_TRUE(elem1.has_value());
    EXPECT_EQ(std::get<std::string>(*elem1), "world");
    
    // Test string array slice
    auto sliced = arrayValue->slice(0, 2);
    EXPECT_EQ(sliced->size(), 2);
    
    auto sliced_elem0 = sliced->get(0);
    ASSERT_TRUE(sliced_elem0.has_value());
    EXPECT_EQ(std::get<std::string>(*sliced_elem0), "hello");
}