#include <gtest/gtest.h>
#include "meld/kernel/control_flow.hpp"
#include "meld/kernel/primitives.hpp"
#include "meld/kernel/extension_registry.hpp"

using namespace meld::kernel;

class ControlFlowTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear and register extensions before each test
        ExtensionRegistry::instance().clear();
        register_control_flow_extensions();
    }
};

// Test 12.1: Boolean control flow methods

TEST_F(ControlFlowTest, BooleanIfTrueIfFalse_TrueCondition) {
    // Test: true.ifTrueIfFalse({ "yes" }, { "no" }) should return "yes"
    
    auto true_val = Value(Boolean::from(true));
    
    bool true_block_called = false;
    bool false_block_called = false;
    
    auto true_block = std::make_shared<Function>(
        std::vector<std::shared_ptr<Symbol>>{},
        Value(),
        [&true_block_called](const std::vector<Value>&) -> Value {
            true_block_called = true;
            return Value(std::make_shared<String>("yes"));
        }
    );
    
    auto false_block = std::make_shared<Function>(
        std::vector<std::shared_ptr<Symbol>>{},
        Value(),
        [&false_block_called](const std::vector<Value>&) -> Value {
            false_block_called = true;
            return Value(std::make_shared<String>("no"));
        }
    );
    
    // Look up the extension
    const ExtensionMethod* method = ExtensionRegistry::instance().lookup_extension("Boolean", "ifTrueIfFalse");
    ASSERT_NE(method, nullptr);
    
    // Call the extension
    Value result = method->implementation({true_val, Value(true_block), Value(false_block)});
    
    EXPECT_TRUE(true_block_called);
    EXPECT_FALSE(false_block_called);
    ASSERT_TRUE(result.is<String>());
    EXPECT_EQ(result.as<String>()->value(), "yes");
}

TEST_F(ControlFlowTest, BooleanIfTrueIfFalse_FalseCondition) {
    // Test: false.ifTrueIfFalse({ "yes" }, { "no" }) should return "no"
    
    auto false_val = Value(Boolean::from(false));
    
    bool true_block_called = false;
    bool false_block_called = false;
    
    auto true_block = std::make_shared<Function>(
        std::vector<std::shared_ptr<Symbol>>{},
        Value(),
        [&true_block_called](const std::vector<Value>&) -> Value {
            true_block_called = true;
            return Value(std::make_shared<String>("yes"));
        }
    );
    
    auto false_block = std::make_shared<Function>(
        std::vector<std::shared_ptr<Symbol>>{},
        Value(),
        [&false_block_called](const std::vector<Value>&) -> Value {
            false_block_called = true;
            return Value(std::make_shared<String>("no"));
        }
    );
    
    // Look up the extension
    const ExtensionMethod* method = ExtensionRegistry::instance().lookup_extension("Boolean", "ifTrueIfFalse");
    ASSERT_NE(method, nullptr);
    
    // Call the extension
    Value result = method->implementation({false_val, Value(true_block), Value(false_block)});
    
    EXPECT_FALSE(true_block_called);
    EXPECT_TRUE(false_block_called);
    ASSERT_TRUE(result.is<String>());
    EXPECT_EQ(result.as<String>()->value(), "no");
}

TEST_F(ControlFlowTest, BooleanIfTrue_ReturnsBuilder) {
    // Test: true.ifTrue({ "yes" }) should return an IfTrueBuilder
    
    auto true_val = Value(Boolean::from(true));
    
    auto true_block = std::make_shared<Function>(
        std::vector<std::shared_ptr<Symbol>>{},
        Value(),
        [](const std::vector<Value>&) -> Value {
            return Value(std::make_shared<String>("yes"));
        }
    );
    
    // Look up the extension
    const ExtensionMethod* method = ExtensionRegistry::instance().lookup_extension("Boolean", "ifTrue");
    ASSERT_NE(method, nullptr);
    
    // Call the extension
    Value result = method->implementation({true_val, Value(true_block)});
    
    // Result should be a function (representing the builder with ifFalse method)
    ASSERT_TRUE(result.is<Function>());
    EXPECT_EQ(result.as<Function>()->name().value_or(""), "ifFalse");
}

// Test 12.2: Int loop methods

TEST_F(ControlFlowTest, IntTimes_ExecutesBlockNTimes) {
    // Test: 5.times({ println("Hello") }) should execute block 5 times
    
    auto count_val = Value(std::make_shared<Integer>(5));
    
    int execution_count = 0;
    auto block = std::make_shared<Function>(
        std::vector<std::shared_ptr<Symbol>>{},
        Value(),
        [&execution_count](const std::vector<Value>&) -> Value {
            execution_count++;
            return Value(Boolean::from(false)); // Unit
        }
    );
    
    // Look up the extension
    const ExtensionMethod* method = ExtensionRegistry::instance().lookup_extension("Int", "times");
    ASSERT_NE(method, nullptr);
    
    // Call the extension
    method->implementation({count_val, Value(block)});
    
    EXPECT_EQ(execution_count, 5);
}

TEST_F(ControlFlowTest, IntTimes_ZeroTimes) {
    // Test: 0.times({ ... }) should not execute block
    
    auto count_val = Value(std::make_shared<Integer>(0));
    
    int execution_count = 0;
    auto block = std::make_shared<Function>(
        std::vector<std::shared_ptr<Symbol>>{},
        Value(),
        [&execution_count](const std::vector<Value>&) -> Value {
            execution_count++;
            return Value(Boolean::from(false));
        }
    );
    
    const ExtensionMethod* method = ExtensionRegistry::instance().lookup_extension("Int", "times");
    ASSERT_NE(method, nullptr);
    
    method->implementation({count_val, Value(block)});
    
    EXPECT_EQ(execution_count, 0);
}

TEST_F(ControlFlowTest, IntTo_ReturnsRange) {
    // Test: 1.to(10) should return a Range
    
    auto start_val = Value(std::make_shared<Integer>(1));
    auto end_val = Value(std::make_shared<Integer>(10));
    
    // Look up the extension
    const ExtensionMethod* method = ExtensionRegistry::instance().lookup_extension("Int", "to");
    ASSERT_NE(method, nullptr);
    
    // Call the extension
    Value result = method->implementation({start_val, end_val});
    
    // Result should be a function (representing the Range with do: method)
    ASSERT_TRUE(result.is<Function>());
    EXPECT_EQ(result.as<Function>()->name().value_or(""), "do");
}

TEST_F(ControlFlowTest, RangeDo_IteratesOverRange) {
    // Test: 1.to(5).do({ i => println(i) }) should iterate from 1 to 5
    
    auto start_val = Value(std::make_shared<Integer>(1));
    auto end_val = Value(std::make_shared<Integer>(5));
    
    // Get the range
    const ExtensionMethod* to_method = ExtensionRegistry::instance().lookup_extension("Int", "to");
    ASSERT_NE(to_method, nullptr);
    Value range_fn = to_method->implementation({start_val, end_val});
    
    // Create a block that collects values
    std::vector<int64_t> collected_values;
    auto block = std::make_shared<Function>(
        std::vector<std::shared_ptr<Symbol>>{std::make_shared<Symbol>("i")},
        Value(),
        [&collected_values](const std::vector<Value>& args) -> Value {
            if (!args.empty() && args[0].is<Integer>()) {
                collected_values.push_back(args[0].as<Integer>()->value());
            }
            return Value(Boolean::from(false));
        }
    );
    
    // Call the do: method on the range
    ASSERT_TRUE(range_fn.is<Function>());
    auto range_do_fn = range_fn.as<Function>();
    if (range_do_fn->impl()) {
        (*range_do_fn->impl())({Value(block)});
    }
    
    // Check that we collected values 1 through 5
    ASSERT_EQ(collected_values.size(), 5);
    EXPECT_EQ(collected_values[0], 1);
    EXPECT_EQ(collected_values[1], 2);
    EXPECT_EQ(collected_values[2], 3);
    EXPECT_EQ(collected_values[3], 4);
    EXPECT_EQ(collected_values[4], 5);
}

// Test 12.3: Block.whileTrue: method

TEST_F(ControlFlowTest, BlockWhileTrue_LoopsWhileConditionTrue) {
    // Test: { x < 5 }.whileTrue({ x = x + 1 })
    
    int counter = 0;
    
    auto condition_block = std::make_shared<Function>(
        std::vector<std::shared_ptr<Symbol>>{},
        Value(),
        [&counter](const std::vector<Value>&) -> Value {
            return Value(Boolean::from(counter < 5));
        }
    );
    
    auto body_block = std::make_shared<Function>(
        std::vector<std::shared_ptr<Symbol>>{},
        Value(),
        [&counter](const std::vector<Value>&) -> Value {
            counter++;
            return Value(Boolean::from(false));
        }
    );
    
    // Look up the extension
    const ExtensionMethod* method = ExtensionRegistry::instance().lookup_extension("Block", "whileTrue");
    ASSERT_NE(method, nullptr);
    
    // Call the extension
    method->implementation({Value(condition_block), Value(body_block)});
    
    EXPECT_EQ(counter, 5);
}

TEST_F(ControlFlowTest, BlockWhileTrue_StopsWhenConditionFalse) {
    // Test: { false }.whileTrue({ ... }) should not execute body
    
    int execution_count = 0;
    
    auto condition_block = std::make_shared<Function>(
        std::vector<std::shared_ptr<Symbol>>{},
        Value(),
        [](const std::vector<Value>&) -> Value {
            return Value(Boolean::from(false));
        }
    );
    
    auto body_block = std::make_shared<Function>(
        std::vector<std::shared_ptr<Symbol>>{},
        Value(),
        [&execution_count](const std::vector<Value>&) -> Value {
            execution_count++;
            return Value(Boolean::from(false));
        }
    );
    
    const ExtensionMethod* method = ExtensionRegistry::instance().lookup_extension("Block", "whileTrue");
    ASSERT_NE(method, nullptr);
    
    method->implementation({Value(condition_block), Value(body_block)});
    
    EXPECT_EQ(execution_count, 0);
}

// Test 12.4: Collection.forEach: method

TEST_F(ControlFlowTest, CollectionForEach_IsRegistered) {
    // Test: Collection.forEach: extension is registered
    
    EXPECT_TRUE(ExtensionRegistry::instance().has_extension("Collection", "forEach"));
    
    const ExtensionMethod* method = ExtensionRegistry::instance().lookup_extension("Collection", "forEach");
    ASSERT_NE(method, nullptr);
    EXPECT_EQ(method->method_name, "forEach");
    EXPECT_EQ(method->target_type_name, "Collection");
}

// Test extension registration

TEST_F(ControlFlowTest, ExtensionsAreRegistered) {
    // Verify all control flow extensions are registered
    
    EXPECT_TRUE(ExtensionRegistry::instance().has_extension("Boolean", "ifTrue"));
    EXPECT_TRUE(ExtensionRegistry::instance().has_extension("Boolean", "ifTrueIfFalse"));
    EXPECT_TRUE(ExtensionRegistry::instance().has_extension("Int", "times"));
    EXPECT_TRUE(ExtensionRegistry::instance().has_extension("Int", "to"));
    EXPECT_TRUE(ExtensionRegistry::instance().has_extension("Block", "whileTrue"));
    EXPECT_TRUE(ExtensionRegistry::instance().has_extension("Collection", "forEach"));
}
