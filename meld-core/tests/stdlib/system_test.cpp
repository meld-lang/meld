#include <gtest/gtest.h>
#include "../../include/meld/stdlib/system.hpp"
#include "../../include/meld/stdlib/effects.hpp"
#include "../../include/meld/effects/builtin_effects.hpp"
#include <sstream>
#include <iostream>

namespace meld::stdlib::test {

// ============================================================================
// SYSTEM.OUT NAMESPACE TESTS
// Task 35.13: Test System.out namespace implementation
// Requirements: 41A.1, 41A.2, 41A.3, 41A.4, 41A.5, 41A.6, 41A.7
// ============================================================================

class SystemOutTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Install Console handler for testing
        console_handler_ = effects::create_console_handler();
        effects::EffectRuntime::instance().pushScope(console_handler_);
        
        // Redirect cout to capture output
        original_cout_ = std::cout.rdbuf();
        std::cout.rdbuf(captured_output_.rdbuf());
    }
    
    void TearDown() override {
        // Restore cout
        std::cout.rdbuf(original_cout_);
        
        // Clean up effect runtime
        effects::EffectRuntime::instance().popScope();
    }
    
    std::string getCapturedOutput() {
        std::string output = captured_output_.str();
        captured_output_.str("");  // Clear for next test
        captured_output_.clear();
        return output;
    }
    
protected:
    std::shared_ptr<effects::EffectHandler> console_handler_;

private:
    std::ostringstream captured_output_;
    std::streambuf* original_cout_;
};

// Test System::out::println function
// Requirements: 41A.1, 41A.2, 41A.3
TEST_F(SystemOutTest, PrintlnBasicFunctionality) {
    // Test basic println functionality
    System::out::println("Hello, World!");
    
    std::string output = getCapturedOutput();
    EXPECT_EQ(output, "Hello, World!\n");
}

// Test System::out::println with empty string
// Requirements: 41A.1, 41A.2
TEST_F(SystemOutTest, PrintlnEmptyString) {
    // Test println with empty string
    System::out::println("");
    
    std::string output = getCapturedOutput();
    EXPECT_EQ(output, "\n");
}

// Test System::out::println with special characters
// Requirements: 41A.1, 41A.2
TEST_F(SystemOutTest, PrintlnSpecialCharacters) {
    // Test println with special characters
    System::out::println("Line 1\nLine 2\tTabbed");
    
    std::string output = getCapturedOutput();
    EXPECT_EQ(output, "Line 1\nLine 2\tTabbed\n");
}

// Test System::out::print function
// Requirements: 41A.4, 41A.5, 41A.6
TEST_F(SystemOutTest, PrintBasicFunctionality) {
    // Test basic print functionality (no newline)
    System::out::print("Hello, ");
    System::out::print("World!");
    
    std::string output = getCapturedOutput();
    EXPECT_EQ(output, "Hello, World!");
}

// Test System::out::print with empty string
// Requirements: 41A.4, 41A.5
TEST_F(SystemOutTest, PrintEmptyString) {
    // Test print with empty string
    System::out::print("");
    
    std::string output = getCapturedOutput();
    EXPECT_EQ(output, "");
}

// Test mixed usage of print and println
// Requirements: 41A.1, 41A.2, 41A.4, 41A.5
TEST_F(SystemOutTest, MixedPrintAndPrintln) {
    // Test mixed usage
    System::out::print("Start: ");
    System::out::println("First line");
    System::out::print("Middle: ");
    System::out::println("Second line");
    System::out::print("End");
    
    std::string output = getCapturedOutput();
    EXPECT_EQ(output, "Start: First line\nMiddle: Second line\nEnd");
}

// Test that System::out functions perform Console effects
// Requirements: 41A.2, 41A.5, 41A.7
TEST_F(SystemOutTest, PerformsConsoleEffects) {
    // This test verifies that the functions actually call perform("Console", ...)
    // by checking that they work with the Console handler installed
    
    // If the functions didn't perform Console effects, they would throw
    // "Unhandled effect" exceptions when no Console handler is installed
    
    // First, remove the Console handler
    effects::EffectRuntime::instance().popScope();
    
    // Now calling System::out functions should throw
    EXPECT_THROW({
        System::out::println("This should fail");
    }, std::runtime_error);
    
    EXPECT_THROW({
        System::out::print("This should also fail");
    }, std::runtime_error);
    
    // Restore handler for cleanup
    effects::EffectRuntime::instance().pushScope(console_handler_);
}

// Test multiple consecutive calls
// Requirements: 41A.1, 41A.2, 41A.4, 41A.5
TEST_F(SystemOutTest, MultipleConsecutiveCalls) {
    // Test multiple consecutive println calls
    System::out::println("Line 1");
    System::out::println("Line 2");
    System::out::println("Line 3");
    
    std::string output = getCapturedOutput();
    EXPECT_EQ(output, "Line 1\nLine 2\nLine 3\n");
    
    // Test multiple consecutive print calls
    System::out::print("A");
    System::out::print("B");
    System::out::print("C");
    
    output = getCapturedOutput();
    EXPECT_EQ(output, "ABC");
}

// Test long strings
// Requirements: 41A.1, 41A.2, 41A.4, 41A.5
TEST_F(SystemOutTest, LongStrings) {
    // Test with a long string
    std::string long_string(1000, 'X');
    System::out::println(long_string);
    
    std::string output = getCapturedOutput();
    EXPECT_EQ(output, long_string + "\n");
    
    // Test print with long string
    System::out::print(long_string);
    
    output = getCapturedOutput();
    EXPECT_EQ(output, long_string);
}

// Test Unicode strings
// Requirements: 41A.1, 41A.2, 41A.4, 41A.5
TEST_F(SystemOutTest, UnicodeStrings) {
    // Test with Unicode characters
    std::string unicode_string = "Hello, 世界! 🌍 Здравствуй мир!";
    System::out::println(unicode_string);
    
    std::string output = getCapturedOutput();
    EXPECT_EQ(output, unicode_string + "\n");
    
    // Test print with Unicode
    System::out::print(unicode_string);
    
    output = getCapturedOutput();
    EXPECT_EQ(output, unicode_string);
}

} // namespace meld::stdlib::test