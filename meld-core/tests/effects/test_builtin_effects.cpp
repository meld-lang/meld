#include "../../include/meld/effects/builtin_effects.hpp"
#include "../../include/meld/effects/effect.hpp"
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>

using namespace meld::effects;
using namespace meld::kernel;

class BuiltinEffectsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear any existing handlers
        EffectRuntime::instance().clear_handlers();
        
        // Create a temporary directory for file tests
        test_dir_ = std::filesystem::temp_directory_path() / "meld_test";
        std::filesystem::create_directories(test_dir_);
    }
    
    void TearDown() override {
        // Clean up test directory
        if (std::filesystem::exists(test_dir_)) {
            std::filesystem::remove_all(test_dir_);
        }
        
        // Clear handlers
        EffectRuntime::instance().clear_handlers();
    }
    
    std::filesystem::path test_dir_;
};

// ============================================================================
// FILESYSTEM EFFECT TESTS
// ============================================================================

TEST_F(BuiltinEffectsTest, FileSystemEffectDefinition) {
    auto fs_effect = create_filesystem_effect();
    
    EXPECT_EQ(fs_effect->name(), "FileSystem");
    EXPECT_TRUE(fs_effect->has_operation("read"));
    EXPECT_TRUE(fs_effect->has_operation("write"));
    EXPECT_TRUE(fs_effect->has_operation("delete"));
    EXPECT_TRUE(fs_effect->has_operation("exists"));
    
    // Check operation signatures
    auto read_op = fs_effect->get_operation("read");
    ASSERT_NE(read_op, nullptr);
    EXPECT_EQ(read_op->parameter_types.size(), 1);
    EXPECT_EQ(read_op->parameter_types[0], "string");
    EXPECT_EQ(read_op->return_type, "string");
    
    auto write_op = fs_effect->get_operation("write");
    ASSERT_NE(write_op, nullptr);
    EXPECT_EQ(write_op->parameter_types.size(), 2);
    EXPECT_EQ(write_op->parameter_types[0], "string");
    EXPECT_EQ(write_op->parameter_types[1], "string");
    EXPECT_EQ(write_op->return_type, "void");
}

TEST_F(BuiltinEffectsTest, FileSystemHandler) {
    auto& runtime = EffectRuntime::instance();
    auto handler = create_filesystem_handler();
    
    EXPECT_TRUE(handler->has_handler("read"));
    EXPECT_TRUE(handler->has_handler("write"));
    EXPECT_TRUE(handler->has_handler("delete"));
    EXPECT_TRUE(handler->has_handler("exists"));
    EXPECT_TRUE(handler->is_complete());
    
    runtime.pushScope(handler);
    
    // Test file operations
    std::string test_file = (test_dir_ / "test.txt").string();
    std::string test_content = "Hello, World!";
    
    // Test exists (should be false initially)
    Value exists_result = runtime.perform_effect("FileSystem", "exists", {Value::from_string(test_file)});
    EXPECT_FALSE(exists_result.as_bool());
    
    // Test write
    Value write_result = runtime.perform_effect("FileSystem", "write", {
        Value::from_string(test_file),
        Value::from_string(test_content)
    });
    // write returns unit, so we just check it doesn't throw
    
    // Test exists (should be true now)
    exists_result = runtime.perform_effect("FileSystem", "exists", {Value::from_string(test_file)});
    EXPECT_TRUE(exists_result.as_bool());
    
    // Test read
    Value read_result = runtime.perform_effect("FileSystem", "read", {Value::from_string(test_file)});
    EXPECT_EQ(read_result.as_string(), test_content);
    
    // Test delete
    Value delete_result = runtime.perform_effect("FileSystem", "delete", {Value::from_string(test_file)});
    // delete returns unit, so we just check it doesn't throw
    
    // Test exists (should be false after delete)
    exists_result = runtime.perform_effect("FileSystem", "exists", {Value::from_string(test_file)});
    EXPECT_FALSE(exists_result.as_bool());
}

TEST_F(BuiltinEffectsTest, FileSystemErrorHandling) {
    auto& runtime = EffectRuntime::instance();
    auto handler = create_filesystem_handler();
    runtime.pushScope(handler);
    
    std::string nonexistent_file = (test_dir_ / "nonexistent.txt").string();
    
    // Test reading nonexistent file
    EXPECT_THROW(
        runtime.perform_effect("FileSystem", "read", {Value::from_string(nonexistent_file)}),
        std::runtime_error
    );
    
    // Test deleting nonexistent file
    EXPECT_THROW(
        runtime.perform_effect("FileSystem", "delete", {Value::from_string(nonexistent_file)}),
        std::runtime_error
    );
}

// ============================================================================
// NETWORK EFFECT TESTS
// ============================================================================

TEST_F(BuiltinEffectsTest, NetworkEffectDefinition) {
    auto net_effect = create_network_effect();
    
    EXPECT_EQ(net_effect->name(), "Network");
    EXPECT_TRUE(net_effect->has_operation("get"));
    EXPECT_TRUE(net_effect->has_operation("post"));
    
    auto get_op = net_effect->get_operation("get");
    ASSERT_NE(get_op, nullptr);
    EXPECT_EQ(get_op->parameter_types.size(), 1);
    EXPECT_EQ(get_op->parameter_types[0], "string");
    EXPECT_EQ(get_op->return_type, "string");
    
    auto post_op = net_effect->get_operation("post");
    ASSERT_NE(post_op, nullptr);
    EXPECT_EQ(post_op->parameter_types.size(), 2);
    EXPECT_EQ(post_op->parameter_types[0], "string");
    EXPECT_EQ(post_op->parameter_types[1], "string");
    EXPECT_EQ(post_op->return_type, "string");
}

TEST_F(BuiltinEffectsTest, NetworkHandler) {
    auto& runtime = EffectRuntime::instance();
    auto handler = create_network_handler();
    
    EXPECT_TRUE(handler->has_handler("get"));
    EXPECT_TRUE(handler->has_handler("post"));
    EXPECT_TRUE(handler->is_complete());
    
    runtime.pushScope(handler);
    
    // Test GET request
    Value get_result = runtime.perform_effect("Network", "get", {Value::from_string("https://example.com")});
    std::string get_response = get_result.as_string();
    EXPECT_TRUE(get_response.find("HTTP response from https://example.com") != std::string::npos);
    
    // Test POST request
    Value post_result = runtime.perform_effect("Network", "post", {
        Value::from_string("https://api.example.com"),
        Value::from_string("{\"key\": \"value\"}")
    });
    std::string post_response = post_result.as_string();
    EXPECT_TRUE(post_response.find("POST response from https://api.example.com") != std::string::npos);
    EXPECT_TRUE(post_response.find("{\"key\": \"value\"}") != std::string::npos);
}

// ============================================================================
// CONSOLE EFFECT TESTS
// ============================================================================

TEST_F(BuiltinEffectsTest, ConsoleEffectDefinition) {
    auto console_effect = create_console_effect();
    
    EXPECT_EQ(console_effect->name(), "Console");
    EXPECT_TRUE(console_effect->has_operation("print"));
    EXPECT_TRUE(console_effect->has_operation("println"));
    EXPECT_TRUE(console_effect->has_operation("readLine"));
    
    auto print_op = console_effect->get_operation("print");
    ASSERT_NE(print_op, nullptr);
    EXPECT_EQ(print_op->parameter_types.size(), 1);
    EXPECT_EQ(print_op->parameter_types[0], "string");
    EXPECT_EQ(print_op->return_type, "void");
    
    auto readline_op = console_effect->get_operation("readLine");
    ASSERT_NE(readline_op, nullptr);
    EXPECT_EQ(readline_op->parameter_types.size(), 0);
    EXPECT_EQ(readline_op->return_type, "string");
}

TEST_F(BuiltinEffectsTest, ConsoleHandler) {
    auto& runtime = EffectRuntime::instance();
    auto handler = create_console_handler();
    
    EXPECT_TRUE(handler->has_handler("print"));
    EXPECT_TRUE(handler->has_handler("println"));
    EXPECT_TRUE(handler->has_handler("readLine"));
    EXPECT_TRUE(handler->is_complete());
    
    runtime.pushScope(handler);
    
    // Test print (we can't easily test the output, but we can test it doesn't throw)
    Value print_result = runtime.perform_effect("Console", "print", {Value::from_string("Hello")});
    // print returns unit
    
    // Test println
    Value println_result = runtime.perform_effect("Console", "println", {Value::from_string("World")});
    // println returns unit
    
    // Note: We can't easily test readLine in automated tests since it requires stdin input
}

// ============================================================================
// RANDOM EFFECT TESTS
// ============================================================================

TEST_F(BuiltinEffectsTest, RandomEffectDefinition) {
    auto random_effect = create_random_effect();
    
    EXPECT_EQ(random_effect->name(), "Random");
    EXPECT_TRUE(random_effect->has_operation("nextInt"));
    EXPECT_TRUE(random_effect->has_operation("nextFloat"));
    
    auto nextint_op = random_effect->get_operation("nextInt");
    ASSERT_NE(nextint_op, nullptr);
    EXPECT_EQ(nextint_op->parameter_types.size(), 1);
    EXPECT_EQ(nextint_op->parameter_types[0], "int");
    EXPECT_EQ(nextint_op->return_type, "int");
    
    auto nextfloat_op = random_effect->get_operation("nextFloat");
    ASSERT_NE(nextfloat_op, nullptr);
    EXPECT_EQ(nextfloat_op->parameter_types.size(), 0);
    EXPECT_EQ(nextfloat_op->return_type, "float");
}

TEST_F(BuiltinEffectsTest, RandomHandler) {
    auto& runtime = EffectRuntime::instance();
    auto handler = create_random_handler();
    
    EXPECT_TRUE(handler->has_handler("nextInt"));
    EXPECT_TRUE(handler->has_handler("nextFloat"));
    EXPECT_TRUE(handler->is_complete());
    
    runtime.pushScope(handler);
    
    // Test nextInt
    Value int_result = runtime.perform_effect("Random", "nextInt", {Value::from_int(100)});
    int64_t random_int = int_result.as_int();
    EXPECT_GE(random_int, 0);
    EXPECT_LT(random_int, 100);
    
    // Test multiple calls produce different results (probabilistically)
    std::set<int64_t> results;
    for (int i = 0; i < 10; ++i) {
        Value result = runtime.perform_effect("Random", "nextInt", {Value::from_int(1000)});
        results.insert(result.as_int());
    }
    // We should get at least a few different values
    EXPECT_GT(results.size(), 1);
    
    // Test nextFloat
    Value float_result = runtime.perform_effect("Random", "nextFloat", {});
    std::string float_str = float_result.as_string();
    // Should be a valid float string representation
    EXPECT_FALSE(float_str.empty());
    
    // Test error handling
    EXPECT_THROW(
        runtime.perform_effect("Random", "nextInt", {Value::from_int(-1)}),
        std::runtime_error
    );
}

// ============================================================================
// TIME EFFECT TESTS
// ============================================================================

TEST_F(BuiltinEffectsTest, TimeEffectDefinition) {
    auto time_effect = create_time_effect();
    
    EXPECT_EQ(time_effect->name(), "Time");
    EXPECT_TRUE(time_effect->has_operation("now"));
    EXPECT_TRUE(time_effect->has_operation("sleep"));
    
    auto now_op = time_effect->get_operation("now");
    ASSERT_NE(now_op, nullptr);
    EXPECT_EQ(now_op->parameter_types.size(), 0);
    EXPECT_EQ(now_op->return_type, "int");
    
    auto sleep_op = time_effect->get_operation("sleep");
    ASSERT_NE(sleep_op, nullptr);
    EXPECT_EQ(sleep_op->parameter_types.size(), 1);
    EXPECT_EQ(sleep_op->parameter_types[0], "int");
    EXPECT_EQ(sleep_op->return_type, "void");
}

TEST_F(BuiltinEffectsTest, TimeHandler) {
    auto& runtime = EffectRuntime::instance();
    auto handler = create_time_handler();
    
    EXPECT_TRUE(handler->has_handler("now"));
    EXPECT_TRUE(handler->has_handler("sleep"));
    EXPECT_TRUE(handler->is_complete());
    
    runtime.pushScope(handler);
    
    // Test now
    Value now_result1 = runtime.perform_effect("Time", "now", {});
    int64_t timestamp1 = now_result1.as_int();
    EXPECT_GT(timestamp1, 0);
    
    // Sleep for a short time
    Value sleep_result = runtime.perform_effect("Time", "sleep", {Value::from_int(10)});
    // sleep returns unit
    
    // Get time again
    Value now_result2 = runtime.perform_effect("Time", "now", {});
    int64_t timestamp2 = now_result2.as_int();
    
    // Second timestamp should be later
    EXPECT_GE(timestamp2, timestamp1);
    
    // Test error handling
    EXPECT_THROW(
        runtime.perform_effect("Time", "sleep", {Value::from_int(-1)}),
        std::runtime_error
    );
}

// ============================================================================
// EXCEPTION EFFECT TESTS
// ============================================================================

TEST_F(BuiltinEffectsTest, ExceptionEffectDefinition) {
    auto exception_effect = create_exception_effect();
    
    EXPECT_EQ(exception_effect->name(), "Exception");
    EXPECT_TRUE(exception_effect->has_operation("raise"));
    
    auto raise_op = exception_effect->get_operation("raise");
    ASSERT_NE(raise_op, nullptr);
    EXPECT_EQ(raise_op->parameter_types.size(), 1);
    EXPECT_EQ(raise_op->parameter_types[0], "string");
    EXPECT_EQ(raise_op->return_type, "void");
}

TEST_F(BuiltinEffectsTest, ExceptionHandler) {
    auto& runtime = EffectRuntime::instance();
    auto handler = create_exception_handler();
    
    EXPECT_TRUE(handler->has_handler("raise"));
    EXPECT_TRUE(handler->is_complete());
    
    runtime.pushScope(handler);
    
    // Test exception raising
    EXPECT_THROW(
        runtime.perform_effect("Exception", "raise", {Value::from_string("Test exception")}),
        std::runtime_error
    );
}

// ============================================================================
// ASYNC EFFECT TESTS
// ============================================================================

TEST_F(BuiltinEffectsTest, AsyncEffectDefinition) {
    auto async_effect = create_async_effect();
    
    EXPECT_EQ(async_effect->name(), "Async");
    EXPECT_TRUE(async_effect->has_operation("wait"));
    
    auto wait_op = async_effect->get_operation("wait");
    ASSERT_NE(wait_op, nullptr);
    EXPECT_EQ(wait_op->parameter_types.size(), 1);
    EXPECT_EQ(wait_op->parameter_types[0], "int");
    EXPECT_EQ(wait_op->return_type, "string");
}

TEST_F(BuiltinEffectsTest, AsyncHandler) {
    auto& runtime = EffectRuntime::instance();
    auto handler = create_async_handler();
    
    EXPECT_TRUE(handler->has_handler("wait"));
    EXPECT_TRUE(handler->is_complete());
    
    runtime.pushScope(handler);
    
    // Test async wait
    auto start_time = std::chrono::steady_clock::now();
    Value wait_result = runtime.perform_effect("Async", "wait", {Value::from_int(50)});
    auto end_time = std::chrono::steady_clock::now();
    
    std::string result_str = wait_result.as_string();
    EXPECT_TRUE(result_str.find("Async operation completed after 50ms") != std::string::npos);
    
    // Check that it actually waited (approximately)
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    EXPECT_GE(duration.count(), 40); // Allow some tolerance
    
    // Test error handling
    EXPECT_THROW(
        runtime.perform_effect("Async", "wait", {Value::from_int(-1)}),
        std::runtime_error
    );
}

// ============================================================================
// GENERATOR EFFECT TESTS
// ============================================================================

TEST_F(BuiltinEffectsTest, GeneratorEffectDefinition) {
    auto generator_effect = create_generator_effect();
    
    EXPECT_EQ(generator_effect->name(), "Generator");
    EXPECT_TRUE(generator_effect->has_operation("yield"));
    
    auto yield_op = generator_effect->get_operation("yield");
    ASSERT_NE(yield_op, nullptr);
    EXPECT_EQ(yield_op->parameter_types.size(), 1);
    EXPECT_EQ(yield_op->parameter_types[0], "string");
    EXPECT_EQ(yield_op->return_type, "string");
}

TEST_F(BuiltinEffectsTest, GeneratorHandler) {
    auto& runtime = EffectRuntime::instance();
    auto handler = create_generator_handler();
    
    EXPECT_TRUE(handler->has_handler("yield"));
    EXPECT_TRUE(handler->is_complete());
    
    runtime.pushScope(handler);
    
    // Test generator yield
    Value yield_result = runtime.perform_effect("Generator", "yield", {Value::from_string("test_value")});
    std::string result_str = yield_result.as_string();
    EXPECT_EQ(result_str, "Yielded: test_value");
}

// ============================================================================
// INTEGRATION TESTS
// ============================================================================

TEST_F(BuiltinEffectsTest, AllBuiltinHandlers) {
    auto handlers = create_all_builtin_handlers();
    
    // Should have all 8 built-in effects
    EXPECT_EQ(handlers.size(), 8);
    
    // Check that we have handlers for all expected effects
    std::set<std::string> effect_names;
    for (const auto& handler : handlers) {
        effect_names.insert(handler->effect().name());
    }
    
    EXPECT_TRUE(effect_names.count("FileSystem"));
    EXPECT_TRUE(effect_names.count("Network"));
    EXPECT_TRUE(effect_names.count("Console"));
    EXPECT_TRUE(effect_names.count("Random"));
    EXPECT_TRUE(effect_names.count("Time"));
    EXPECT_TRUE(effect_names.count("Exception"));
    EXPECT_TRUE(effect_names.count("Async"));
    EXPECT_TRUE(effect_names.count("Generator"));
}

TEST_F(BuiltinEffectsTest, BuiltinEffectsScope) {
    auto& runtime = EffectRuntime::instance();
    
    EXPECT_EQ(runtime.scope_depth(), 0);
    EXPECT_FALSE(runtime.has_handler("FileSystem"));
    
    {
        BuiltinEffectsScope scope;
        
        EXPECT_EQ(runtime.scope_depth(), 1);
        EXPECT_TRUE(runtime.has_handler("FileSystem"));
        EXPECT_TRUE(runtime.has_handler("Network"));
        EXPECT_TRUE(runtime.has_handler("Console"));
        EXPECT_TRUE(runtime.has_handler("Random"));
        EXPECT_TRUE(runtime.has_handler("Time"));
        EXPECT_TRUE(runtime.has_handler("Exception"));
        EXPECT_TRUE(runtime.has_handler("Async"));
        EXPECT_TRUE(runtime.has_handler("Generator"));
    }
    
    // Scope should be cleaned up
    EXPECT_EQ(runtime.scope_depth(), 0);
    EXPECT_FALSE(runtime.has_handler("FileSystem"));
}

TEST_F(BuiltinEffectsTest, InstallUninstallBuiltinEffects) {
    auto& runtime = EffectRuntime::instance();
    
    EXPECT_EQ(runtime.scope_depth(), 0);
    EXPECT_FALSE(runtime.has_handler("FileSystem"));
    
    install_builtin_effects();
    
    EXPECT_EQ(runtime.scope_depth(), 1);
    EXPECT_TRUE(runtime.has_handler("FileSystem"));
    EXPECT_TRUE(runtime.has_handler("Network"));
    
    uninstall_builtin_effects();
    
    EXPECT_EQ(runtime.scope_depth(), 0);
    EXPECT_FALSE(runtime.has_handler("FileSystem"));
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}