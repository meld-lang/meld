#include <gtest/gtest.h>
#include "meld/stdlib/pattern_matcher.hpp"
#include <string>

using namespace meld::stdlib;

// Test 1: Basic literal pattern matching
TEST(PatternMatcherTest, LiteralMatching) {
    int value = 2;
    PatternMatcher<int, std::string> matcher(value);
    
    std::string result = matcher
        .on(0).then([]() { return "zero"; })
        .on(1).then([]() { return "one"; })
        .on(2).then([]() { return "two"; })
        .otherwise([]() { return "other"; });
    
    EXPECT_EQ(result, "two");
}

// Test 2: Otherwise clause
TEST(PatternMatcherTest, OtherwiseClause) {
    int value = 99;
    PatternMatcher<int, std::string> matcher(value);
    
    std::string result = matcher
        .on(0).then([]() { return "zero"; })
        .on(1).then([]() { return "one"; })
        .otherwise([]() { return "other"; });
    
    EXPECT_EQ(result, "other");
}

// Test 3: Multiple literal patterns
TEST(PatternMatcherTest, MultipleLiterals) {
    int value = 4;
    PatternMatcher<int, std::string> matcher(value);
    
    std::string result = matcher
        .on(0, 1, 2).then([]() { return "small"; })
        .on(3, 4, 5).then([]() { return "medium"; })
        .otherwise([]() { return "large"; });
    
    EXPECT_EQ(result, "medium");
}

// Test 4: Pattern guards
TEST(PatternMatcherTest, GuardedPatterns) {
    int value = 15;
    PatternMatcher<int, std::string> matcher(value);
    
    std::string result = matcher
        .on(value).when([](const int& n) { return n < 10; }).then([]() { return "small"; })
        .on(value).when([](const int& n) { return n < 20; }).then([]() { return "medium"; })
        .otherwise([]() { return "large"; });
    
    EXPECT_EQ(result, "medium");
}

// Test 5: Guard that fails
TEST(PatternMatcherTest, GuardFails) {
    int value = 25;
    PatternMatcher<int, std::string> matcher(value);
    
    std::string result = matcher
        .on(value).when([](const int& n) { return n < 10; }).then([]() { return "small"; })
        .on(value).when([](const int& n) { return n < 20; }).then([]() { return "medium"; })
        .otherwise([]() { return "large"; });
    
    EXPECT_EQ(result, "large");
}

// Test 6: Handler with value access
TEST(PatternMatcherTest, HandlerWithValue) {
    int value = 42;
    PatternMatcher<int, std::string> matcher(value);
    
    std::string result = matcher
        .on(value).then([](const int& n) { 
            return "value is " + std::to_string(n); 
        })
        .otherwise([]() { return "no match"; });
    
    EXPECT_EQ(result, "value is 42");
}

// Test 7: String matching
TEST(PatternMatcherTest, StringMatching) {
    std::string value = "hello";
    PatternMatcher<std::string, int> matcher(value);
    
    int result = matcher
        .on("hello").then([]() { return 1; })
        .on("world").then([]() { return 2; })
        .otherwise([]() { return 0; });
    
    EXPECT_EQ(result, 1);
}

// Test 8: Boolean matching (exhaustive)
TEST(PatternMatcherTest, BooleanMatching) {
    bool value = true;
    PatternMatcher<bool, std::string> matcher(value);
    
    std::string result = matcher
        .on(true).then([]() { return "yes"; })
        .on(false).then([]() { return "no"; })
        .otherwise([]() { return "unknown"; });
    
    EXPECT_EQ(result, "yes");
}

// Test 9: First match wins
TEST(PatternMatcherTest, FirstMatchWins) {
    int value = 5;
    PatternMatcher<int, std::string> matcher(value);
    
    std::string result = matcher
        .on(5).then([]() { return "first"; })
        .on(5).then([]() { return "second"; })
        .otherwise([]() { return "other"; });
    
    EXPECT_EQ(result, "first");
}

// Test 10: Guard with value access
TEST(PatternMatcherTest, GuardWithValueAccess) {
    int value = 15;
    PatternMatcher<int, std::string> matcher(value);
    
    std::string result = matcher
        .on(value).when([](const int& n) { return n % 2 == 0; }).then([]() { return "even"; })
        .on(value).when([](const int& n) { return n % 2 == 1; }).then([]() { return "odd"; })
        .otherwise([]() { return "unknown"; });
    
    EXPECT_EQ(result, "odd");
}

// Test 11: Multiple guards on same pattern
TEST(PatternMatcherTest, MultipleGuards) {
    int value = 10;
    PatternMatcher<int, std::string> matcher(value);
    
    std::string result = matcher
        .on(value).when([](const int& n) { return n < 5; }).then([]() { return "very small"; })
        .on(value).when([](const int& n) { return n < 15; }).then([]() { return "small"; })
        .on(value).when([](const int& n) { return n < 25; }).then([]() { return "medium"; })
        .otherwise([]() { return "large"; });
    
    EXPECT_EQ(result, "small");
}

// Test 12: Empty pattern list uses otherwise
TEST(PatternMatcherTest, EmptyPatternList) {
    int value = 42;
    PatternMatcher<int, std::string> matcher(value);
    
    std::string result = matcher
        .otherwise([]() { return "default"; });
    
    EXPECT_EQ(result, "default");
}

// Test 13: Complex guard condition
TEST(PatternMatcherTest, ComplexGuard) {
    int value = 12;
    PatternMatcher<int, std::string> matcher(value);
    
    std::string result = matcher
        .on(value).when([](const int& n) { 
            return n > 10 && n < 20 && n % 2 == 0; 
        }).then([]() { return "match"; })
        .otherwise([]() { return "no match"; });
    
    EXPECT_EQ(result, "match");
}

// Test 14: Returning different types
TEST(PatternMatcherTest, NumericResult) {
    std::string value = "multiply";
    PatternMatcher<std::string, int> matcher(value);
    
    int result = matcher
        .on("add").then([]() { return 10 + 5; })
        .on("multiply").then([]() { return 10 * 5; })
        .otherwise([]() { return 0; });
    
    EXPECT_EQ(result, 50);
}

// Test 15: Pattern case count
TEST(PatternMatcherTest, PatternCaseCount) {
    int value = 1;
    PatternMatcher<int, std::string> matcher(value);
    
    matcher
        .on(0).then([]() { return "zero"; })
        .on(1).then([]() { return "one"; })
        .on(2).then([]() { return "two"; });
    
    EXPECT_EQ(matcher.cases().size(), 3);
}

// Test 16: Guard detection
TEST(PatternMatcherTest, GuardDetection) {
    int value = 5;
    PatternMatcher<int, std::string> matcher(value);
    
    matcher
        .on(value).when([](const int& n) { return n > 0; }).then([]() { return "positive"; });
    
    EXPECT_TRUE(matcher.cases()[0].has_guard());
}

// Test 17: No guard detection
TEST(PatternMatcherTest, NoGuardDetection) {
    int value = 5;
    PatternMatcher<int, std::string> matcher(value);
    
    matcher
        .on(value).then([]() { return "match"; });
    
    EXPECT_FALSE(matcher.cases()[0].has_guard());
}

// Test 18: Exhaustiveness checking - boolean exhaustive
TEST(PatternMatcherTest, BooleanExhaustive) {
    bool value = true;
    PatternMatcher<bool, std::string> matcher(value);
    
    matcher
        .on(true).then([]() { return "yes"; })
        .on(false).then([]() { return "no"; });
    
    EXPECT_TRUE(matcher.is_exhaustive());
}

// Test 19: Exhaustiveness checking - boolean non-exhaustive
TEST(PatternMatcherTest, BooleanNonExhaustive) {
    bool value = true;
    PatternMatcher<bool, std::string> matcher(value);
    
    matcher
        .on(true).then([]() { return "yes"; });
    
    EXPECT_FALSE(matcher.is_exhaustive());
}

// Test 20: Exhaustiveness checking - with otherwise clause
TEST(PatternMatcherTest, ExhaustiveWithOtherwise) {
    int value = 42;
    PatternMatcher<int, std::string> matcher(value);
    
    matcher
        .on(1, 2, 3).then([]() { return "small"; });
    
    auto result = matcher.otherwise([]() { return "other"; });
    
    EXPECT_TRUE(matcher.is_exhaustive());
    EXPECT_EQ(result, "other");
}

// Test 21: Universal match function
TEST(PatternMatcherTest, UniversalMatchFunction) {
    int value = 3;
    
    auto result = match<int, std::string>(value, [](auto& matcher) {
        return matcher
            .on(1).then([]() { return "one"; })
            .on(2).then([]() { return "two"; })
            .on(3).then([]() { return "three"; })
            .otherwise([]() { return "other"; });
    });
    
    EXPECT_EQ(result, "three");
}

// Test 22: Match proxy
TEST(PatternMatcherTest, MatchProxy) {
    std::string value = "test";
    
    auto result = make_match(value)([](auto& matcher) {
        return matcher
            .on("hello").then([]() { return 1; })
            .on("test").then([]() { return 2; })
            .otherwise([]() { return 0; });
    });
    
    EXPECT_EQ(result, 2);
}
