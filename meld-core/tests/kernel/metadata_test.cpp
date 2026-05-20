#include <gtest/gtest.h>
#include "meld/kernel/primitives.hpp"
#include "meld/kernel/operations.hpp"
#include "meld/kernel/symbol_table.hpp"

using namespace meld::kernel;

class MetadataTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear metadata store before each test
        MetadataStore::instance().clear_all();
    }
    
    void TearDown() override {
        // Clear metadata store after each test
        MetadataStore::instance().clear_all();
    }
};

// Test basic meta_set and meta_get functionality
TEST_F(MetadataTest, BasicSetAndGet) {
    // Create a value
    auto num = Value(std::make_shared<Integer>(42));
    
    // Attach metadata
    auto metadata = Value(std::make_shared<String>("test_metadata"));
    auto result = meta_set(num, "key1", metadata);
    
    // Result should be the same object
    EXPECT_TRUE(result.is<Integer>());
    EXPECT_EQ(result.as<Integer>()->value(), 42);
    
    // Retrieve metadata
    auto retrieved = meta_get(num, "key1");
    EXPECT_TRUE(retrieved.is<String>());
    EXPECT_EQ(retrieved.as<String>()->value(), "test_metadata");
}

// Test that metadata doesn't affect object value
TEST_F(MetadataTest, MetadataDoesNotAffectValue) {
    auto num = Value(std::make_shared<Integer>(42));
    auto metadata = Value(std::make_shared<String>("metadata"));
    
    // Attach metadata
    meta_set(num, "key", metadata);
    
    // Value should be unchanged
    EXPECT_TRUE(num.is<Integer>());
    EXPECT_EQ(num.as<Integer>()->value(), 42);
    EXPECT_EQ(num.to_string(), "42");
}

// Test retrieving non-existent metadata returns nil
TEST_F(MetadataTest, NonExistentMetadataReturnsNil) {
    auto num = Value(std::make_shared<Integer>(42));
    
    // Try to get metadata that doesn't exist
    auto result = meta_get(num, "nonexistent");
    
    EXPECT_TRUE(result.is<Empty>());
}

// Test multiple metadata keys on same object
TEST_F(MetadataTest, MultipleMetadataKeys) {
    auto num = Value(std::make_shared<Integer>(42));
    
    // Attach multiple metadata values
    auto meta1 = Value(std::make_shared<String>("value1"));
    auto meta2 = Value(std::make_shared<String>("value2"));
    auto meta3 = Value(std::make_shared<Integer>(99));
    
    meta_set(num, "key1", meta1);
    meta_set(num, "key2", meta2);
    meta_set(num, "key3", meta3);
    
    // Retrieve all metadata
    auto retrieved1 = meta_get(num, "key1");
    auto retrieved2 = meta_get(num, "key2");
    auto retrieved3 = meta_get(num, "key3");
    
    EXPECT_TRUE(retrieved1.is<String>());
    EXPECT_EQ(retrieved1.as<String>()->value(), "value1");
    
    EXPECT_TRUE(retrieved2.is<String>());
    EXPECT_EQ(retrieved2.as<String>()->value(), "value2");
    
    EXPECT_TRUE(retrieved3.is<Integer>());
    EXPECT_EQ(retrieved3.as<Integer>()->value(), 99);
}

// Test metadata on different value types
TEST_F(MetadataTest, MetadataOnDifferentTypes) {
    // Integer
    auto num = Value(std::make_shared<Integer>(42));
    meta_set(num, "type", Value(std::make_shared<String>("integer")));
    
    // String
    auto str = Value(std::make_shared<String>("hello"));
    meta_set(str, "type", Value(std::make_shared<String>("string")));
    
    // Symbol
    auto sym = Value(SymbolTable::instance().intern("test"));
    meta_set(sym, "type", Value(std::make_shared<String>("symbol")));
    
    // Boolean
    auto bool_val = Value(Boolean::true_value());
    meta_set(bool_val, "type", Value(std::make_shared<String>("boolean")));
    
    // Retrieve and verify
    EXPECT_EQ(meta_get(num, "type").as<String>()->value(), "integer");
    EXPECT_EQ(meta_get(str, "type").as<String>()->value(), "string");
    EXPECT_EQ(meta_get(sym, "type").as<String>()->value(), "symbol");
    EXPECT_EQ(meta_get(bool_val, "type").as<String>()->value(), "boolean");
}

// Test metadata on cons cells (AST nodes)
TEST_F(MetadataTest, MetadataOnConsCells) {
    // Create a cons cell (simulating an AST node)
    auto car = Value(SymbolTable::instance().intern("define"));
    auto cdr = Value(std::make_shared<Integer>(42));
    auto cons = Value(std::make_shared<Cons>(car, cdr));
    
    // Attach provenance metadata
    auto provenance = Value(std::make_shared<String>("Origin.Human"));
    meta_set(cons, "provenance", provenance);
    
    // Attach line number metadata
    auto line_num = Value(std::make_shared<Integer>(10));
    meta_set(cons, "line", line_num);
    
    // Retrieve metadata
    auto retrieved_provenance = meta_get(cons, "provenance");
    auto retrieved_line = meta_get(cons, "line");
    
    EXPECT_TRUE(retrieved_provenance.is<String>());
    EXPECT_EQ(retrieved_provenance.as<String>()->value(), "Origin.Human");
    
    EXPECT_TRUE(retrieved_line.is<Integer>());
    EXPECT_EQ(retrieved_line.as<Integer>()->value(), 10);
}

// Test overwriting metadata
TEST_F(MetadataTest, OverwriteMetadata) {
    auto num = Value(std::make_shared<Integer>(42));
    
    // Set initial metadata
    auto meta1 = Value(std::make_shared<String>("initial"));
    meta_set(num, "key", meta1);
    
    // Verify initial value
    auto retrieved1 = meta_get(num, "key");
    EXPECT_EQ(retrieved1.as<String>()->value(), "initial");
    
    // Overwrite metadata
    auto meta2 = Value(std::make_shared<String>("updated"));
    meta_set(num, "key", meta2);
    
    // Verify updated value
    auto retrieved2 = meta_get(num, "key");
    EXPECT_EQ(retrieved2.as<String>()->value(), "updated");
}

// Test meta_has helper function
TEST_F(MetadataTest, MetaHasHelper) {
    auto num = Value(std::make_shared<Integer>(42));
    
    // Initially no metadata
    EXPECT_FALSE(meta_has(num, "key"));
    
    // Add metadata
    meta_set(num, "key", Value(std::make_shared<String>("value")));
    
    // Now metadata exists
    EXPECT_TRUE(meta_has(num, "key"));
    
    // Different key doesn't exist
    EXPECT_FALSE(meta_has(num, "other_key"));
}

// Test code provenance use case
TEST_F(MetadataTest, CodeProvenanceUseCase) {
    // Simulate an AST node for a function definition
    auto func_name = Value(SymbolTable::instance().intern("calculate"));
    auto func_body = Value(std::make_shared<Integer>(42));
    auto ast_node = Value(std::make_shared<Cons>(func_name, func_body));
    
    // Attach provenance metadata
    auto origin = Value(std::make_shared<String>("Origin.Human"));
    auto timestamp = Value(std::make_shared<Integer>(1234567890));
    auto author = Value(std::make_shared<String>("developer@example.com"));
    
    meta_set(ast_node, "origin", origin);
    meta_set(ast_node, "timestamp", timestamp);
    meta_set(ast_node, "author", author);
    
    // Query provenance
    auto retrieved_origin = meta_get(ast_node, "origin");
    auto retrieved_timestamp = meta_get(ast_node, "timestamp");
    auto retrieved_author = meta_get(ast_node, "author");
    
    EXPECT_EQ(retrieved_origin.as<String>()->value(), "Origin.Human");
    EXPECT_EQ(retrieved_timestamp.as<Integer>()->value(), 1234567890);
    EXPECT_EQ(retrieved_author.as<String>()->value(), "developer@example.com");
}

// Test AI agent metadata use case
TEST_F(MetadataTest, AIAgentMetadataUseCase) {
    // Simulate AI-generated code
    auto ai_code = Value(std::make_shared<String>("fn calculate(x) { x * 2 }"));
    
    // Attach AI metadata
    auto origin = Value(std::make_shared<String>("Origin.Agent"));
    auto model = Value(std::make_shared<String>("gpt-4"));
    auto confidence = Value(std::make_shared<Integer>(95));
    
    meta_set(ai_code, "origin", origin);
    meta_set(ai_code, "model", model);
    meta_set(ai_code, "confidence", confidence);
    
    // Query AI metadata
    EXPECT_EQ(meta_get(ai_code, "origin").as<String>()->value(), "Origin.Agent");
    EXPECT_EQ(meta_get(ai_code, "model").as<String>()->value(), "gpt-4");
    EXPECT_EQ(meta_get(ai_code, "confidence").as<Integer>()->value(), 95);
}

// Test documentation metadata use case
TEST_F(MetadataTest, DocumentationMetadataUseCase) {
    // Simulate a function AST node
    auto func = Value(SymbolTable::instance().intern("add"));
    
    // Attach documentation metadata
    auto doc = Value(std::make_shared<String>("Adds two numbers together"));
    auto params = Value(std::make_shared<String>("a: int, b: int"));
    auto returns = Value(std::make_shared<String>("int"));
    
    meta_set(func, "doc", doc);
    meta_set(func, "params", params);
    meta_set(func, "returns", returns);
    
    // Query documentation
    EXPECT_EQ(meta_get(func, "doc").as<String>()->value(), "Adds two numbers together");
    EXPECT_EQ(meta_get(func, "params").as<String>()->value(), "a: int, b: int");
    EXPECT_EQ(meta_get(func, "returns").as<String>()->value(), "int");
}

// Test that metadata is independent per object
TEST_F(MetadataTest, MetadataIndependentPerObject) {
    auto num1 = Value(std::make_shared<Integer>(42));
    auto num2 = Value(std::make_shared<Integer>(42));
    
    // Attach metadata to first object only
    meta_set(num1, "key", Value(std::make_shared<String>("value1")));
    
    // Second object should not have metadata
    auto retrieved1 = meta_get(num1, "key");
    auto retrieved2 = meta_get(num2, "key");
    
    EXPECT_TRUE(retrieved1.is<String>());
    EXPECT_EQ(retrieved1.as<String>()->value(), "value1");
    
    EXPECT_TRUE(retrieved2.is<Empty>());
}

// Test metadata with complex nested structures
TEST_F(MetadataTest, MetadataOnNestedStructures) {
    // Create nested cons cells (list structure)
    auto elem1 = Value(std::make_shared<Integer>(1));
    auto elem2 = Value(std::make_shared<Integer>(2));
    auto elem3 = Value(std::make_shared<Integer>(3));
    
    auto cons3 = Value(std::make_shared<Cons>(elem3, Value(Empty::instance())));
    auto cons2 = Value(std::make_shared<Cons>(elem2, cons3));
    auto cons1 = Value(std::make_shared<Cons>(elem1, cons2));
    
    // Attach metadata to each level
    meta_set(cons1, "level", Value(std::make_shared<Integer>(1)));
    meta_set(cons2, "level", Value(std::make_shared<Integer>(2)));
    meta_set(cons3, "level", Value(std::make_shared<Integer>(3)));
    
    // Verify each level has independent metadata
    EXPECT_EQ(meta_get(cons1, "level").as<Integer>()->value(), 1);
    EXPECT_EQ(meta_get(cons2, "level").as<Integer>()->value(), 2);
    EXPECT_EQ(meta_get(cons3, "level").as<Integer>()->value(), 3);
}
