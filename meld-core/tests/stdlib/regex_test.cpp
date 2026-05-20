#include <gtest/gtest.h>
#include "meld/stdlib/regex.hpp"
#include <string>

using namespace meld::stdlib;

// Test 1: Basic regex construction
TEST(RegexTest, BasicConstruction) {
    Regex regex("hello");
    EXPECT_TRUE(regex.valid());
    EXPECT_EQ(regex.pattern(), "hello");
}

// Test 2: Regex with flags
TEST(RegexTest, ConstructionWithFlags) {
    Regex regex("hello", RegexFlags::IGNORECASE);
    EXPECT_TRUE(regex.valid());
    EXPECT_EQ(regex.pattern(), "hello");
}

// Test 3: Regex with string flags
TEST(RegexTest, ConstructionWithStringFlags) {
    Regex regex("hello", "i");
    EXPECT_TRUE(regex.valid());
}

// Test 4: Static compile method
TEST(RegexTest, StaticCompile) {
    auto regex = Regex::compile("\\d+");
    EXPECT_TRUE(regex.valid());
    EXPECT_EQ(regex.pattern(), "\\d+");
}

// Test 5: Basic matching
TEST(RegexTest, BasicMatching) {
    Regex regex("hello");
    EXPECT_TRUE(regex.matches("hello"));
    EXPECT_FALSE(regex.matches("Hello"));
    EXPECT_FALSE(regex.matches("hello world"));
}

// Test 6: Case-insensitive matching
TEST(RegexTest, CaseInsensitiveMatching) {
    Regex regex("hello", RegexFlags::IGNORECASE);
    EXPECT_TRUE(regex.matches("hello"));
    EXPECT_TRUE(regex.matches("Hello"));
    EXPECT_TRUE(regex.matches("HELLO"));
}

// Test 7: Pattern with special characters
TEST(RegexTest, SpecialCharacters) {
    Regex regex("\\d+");
    EXPECT_TRUE(regex.matches("123"));
    EXPECT_TRUE(regex.matches("456"));
    EXPECT_FALSE(regex.matches("abc"));
}

// Test 8: Find first match
TEST(RegexTest, FindFirstMatch) {
    Regex regex("\\d+");
    auto match = regex.find("There are 123 apples");
    
    ASSERT_TRUE(match.has_value());
    EXPECT_EQ(match->text(), "123");
    EXPECT_EQ(match->start(), 10);
}

// Test 9: Find no match
TEST(RegexTest, FindNoMatch) {
    Regex regex("\\d+");
    auto match = regex.find("No numbers here");
    
    EXPECT_FALSE(match.has_value());
}

// Test 10: Find all matches
TEST(RegexTest, FindAllMatches) {
    Regex regex("\\d+");
    auto matches = regex.findAll("123 apples and 456 oranges");
    
    ASSERT_EQ(matches.size(), 2);
    EXPECT_EQ(matches[0].text(), "123");
    EXPECT_EQ(matches[1].text(), "456");
}

// Test 11: Find all with no matches
TEST(RegexTest, FindAllNoMatches) {
    Regex regex("\\d+");
    auto matches = regex.findAll("No numbers");
    
    EXPECT_EQ(matches.size(), 0);
}

// Test 12: Capture groups
TEST(RegexTest, CaptureGroups) {
    Regex regex("(\\d+)-(\\d+)");
    auto match = regex.find("123-456");
    
    ASSERT_TRUE(match.has_value());
    EXPECT_EQ(match->text(), "123-456");
    EXPECT_EQ(match->group(0).value(), "123-456");  // Full match
    EXPECT_EQ(match->group(1).value(), "123");      // First group
    EXPECT_EQ(match->group(2).value(), "456");      // Second group
}

// Test 13: Groups method
TEST(RegexTest, GroupsMethod) {
    Regex regex("(\\d+)-(\\d+)");
    auto match = regex.find("123-456");
    
    ASSERT_TRUE(match.has_value());
    auto groups = match->groups();
    
    ASSERT_EQ(groups.size(), 3);
    EXPECT_EQ(groups[0], "123-456");
    EXPECT_EQ(groups[1], "123");
    EXPECT_EQ(groups[2], "456");
}

// Test 14: Invalid group index
TEST(RegexTest, InvalidGroupIndex) {
    Regex regex("\\d+");
    auto match = regex.find("123");
    
    ASSERT_TRUE(match.has_value());
    EXPECT_FALSE(match->group(10).has_value());
}

// Test 15: Replace first occurrence
TEST(RegexTest, ReplaceFirst) {
    Regex regex("\\d+");
    std::string result = regex.replace("123 and 456", "NUM");
    
    EXPECT_EQ(result, "NUM and 456");
}

// Test 16: Replace all occurrences
TEST(RegexTest, ReplaceAll) {
    Regex regex("\\d+");
    std::string result = regex.replaceAll("123 and 456", "NUM");
    
    EXPECT_EQ(result, "NUM and NUM");
}

// Test 17: Replace with capture groups
TEST(RegexTest, ReplaceWithCaptureGroups) {
    Regex regex("(\\d+)-(\\d+)");
    std::string result = regex.replace("123-456", "$2-$1");
    
    EXPECT_EQ(result, "456-123");
}

// Test 18: Split string
TEST(RegexTest, SplitString) {
    Regex regex("\\s+");
    auto parts = regex.split("hello   world   test");
    
    ASSERT_EQ(parts.size(), 3);
    EXPECT_EQ(parts[0], "hello");
    EXPECT_EQ(parts[1], "world");
    EXPECT_EQ(parts[2], "test");
}

// Test 19: Split with limit
TEST(RegexTest, SplitWithLimit) {
    Regex regex("\\s+");
    auto parts = regex.split("one two three four", 2);
    
    ASSERT_EQ(parts.size(), 2);
    EXPECT_EQ(parts[0], "one");
    EXPECT_EQ(parts[1], "two");
}

// Test 20: Split by comma
TEST(RegexTest, SplitByComma) {
    Regex regex(",");
    auto parts = regex.split("a,b,c,d");
    
    ASSERT_EQ(parts.size(), 4);
    EXPECT_EQ(parts[0], "a");
    EXPECT_EQ(parts[1], "b");
    EXPECT_EQ(parts[2], "c");
    EXPECT_EQ(parts[3], "d");
}

// Test 21: Email validation
TEST(RegexTest, EmailValidation) {
    Regex regex("[a-z0-9._%+-]+@[a-z0-9.-]+\\.[a-z]{2,}");
    
    EXPECT_TRUE(regex.matches("user@example.com"));
    EXPECT_TRUE(regex.matches("test.user@domain.co.uk"));
    EXPECT_FALSE(regex.matches("invalid.email"));
    EXPECT_FALSE(regex.matches("@example.com"));
}

// Test 22: URL matching
TEST(RegexTest, URLMatching) {
    Regex regex("https?://[^\\s]+");
    auto match = regex.find("Visit https://example.com for more");
    
    ASSERT_TRUE(match.has_value());
    EXPECT_EQ(match->text(), "https://example.com");
}

// Test 23: Match position
TEST(RegexTest, MatchPosition) {
    Regex regex("world");
    auto match = regex.find("hello world");
    
    ASSERT_TRUE(match.has_value());
    EXPECT_EQ(match->start(), 6);
    EXPECT_EQ(match->end(), 11);
    EXPECT_EQ(match->length(), 5);
}

// Test 24: Match size
TEST(RegexTest, MatchSize) {
    Regex regex("(\\d+)-(\\d+)-(\\d+)");
    auto match = regex.find("2024-12-02");
    
    ASSERT_TRUE(match.has_value());
    EXPECT_EQ(match->size(), 4);  // Full match + 3 groups
}

// Test 25: RegexBuilder basic
TEST(RegexBuilderTest, BasicBuilder) {
    auto regex = RegexBuilder()
        .pattern("hello")
        .build();
    
    EXPECT_TRUE(regex.matches("hello"));
}

// Test 26: RegexBuilder with ignoreCase
TEST(RegexBuilderTest, BuilderIgnoreCase) {
    auto regex = RegexBuilder()
        .pattern("hello")
        .ignoreCase()
        .build();
    
    EXPECT_TRUE(regex.matches("hello"));
    EXPECT_TRUE(regex.matches("HELLO"));
    EXPECT_TRUE(regex.matches("Hello"));
}

// Test 27: RegexBuilder with multiple flags
TEST(RegexBuilderTest, BuilderMultipleFlags) {
    auto regex = RegexBuilder()
        .pattern("test")
        .ignoreCase()
        .multiline()
        .build();
    
    EXPECT_TRUE(regex.matches("test"));
    EXPECT_TRUE(regex.matches("TEST"));
}

// Test 28: RegexBuilder chaining
TEST(RegexBuilderTest, BuilderChaining) {
    auto regex = RegexBuilder()
        .pattern("\\d+")
        .ignoreCase()
        .multiline()
        .dotAll()
        .build();
    
    EXPECT_TRUE(regex.matches("123"));
}

// Test 29: RegexBuilder empty pattern error
TEST(RegexBuilderTest, EmptyPatternError) {
    RegexBuilder builder;
    
    EXPECT_THROW(builder.build(), std::runtime_error);
}

// Test 30: Invalid regex pattern
TEST(RegexTest, InvalidPattern) {
    EXPECT_THROW(Regex("[invalid"), std::runtime_error);
}

// Test 31: Whitespace matching
TEST(RegexTest, WhitespaceMatching) {
    Regex regex("\\s+");
    auto matches = regex.findAll("a b  c   d");
    
    EXPECT_EQ(matches.size(), 3);
}

// Test 32: Word boundary
TEST(RegexTest, WordBoundary) {
    Regex regex("\\btest\\b");
    
    EXPECT_TRUE(regex.find("test").has_value());
    EXPECT_TRUE(regex.find("a test b").has_value());
    EXPECT_FALSE(regex.find("testing").has_value());
}

// Test 33: Anchors
TEST(RegexTest, Anchors) {
    Regex regex("^hello$");
    
    EXPECT_TRUE(regex.matches("hello"));
    EXPECT_FALSE(regex.matches("hello world"));
    EXPECT_FALSE(regex.matches("say hello"));
}

// Test 34: Quantifiers
TEST(RegexTest, Quantifiers) {
    Regex regex("a{2,4}");
    
    EXPECT_TRUE(regex.matches("aa"));
    EXPECT_TRUE(regex.matches("aaa"));
    EXPECT_TRUE(regex.matches("aaaa"));
    EXPECT_FALSE(regex.matches("a"));
    EXPECT_FALSE(regex.matches("aaaaa"));
}

// Test 35: Alternation
TEST(RegexTest, Alternation) {
    Regex regex("cat|dog");
    
    EXPECT_TRUE(regex.find("I have a cat").has_value());
    EXPECT_TRUE(regex.find("I have a dog").has_value());
    EXPECT_FALSE(regex.find("I have a bird").has_value());
}

// Test 36: Character classes
TEST(RegexTest, CharacterClasses) {
    Regex regex("[aeiou]");
    auto matches = regex.findAll("hello");
    
    EXPECT_EQ(matches.size(), 2);  // 'e' and 'o'
}

// Test 37: Negated character class
TEST(RegexTest, NegatedCharacterClass) {
    Regex regex("[^0-9]+");
    
    EXPECT_TRUE(regex.matches("abc"));
    EXPECT_FALSE(regex.matches("123"));
}

// Test 38: Greedy vs non-greedy
TEST(RegexTest, GreedyMatching) {
    Regex greedy("<.*>");
    Regex nonGreedy("<.*?>");
    
    std::string input = "<tag>content</tag>";
    
    auto greedyMatch = greedy.find(input);
    auto nonGreedyMatch = nonGreedy.find(input);
    
    ASSERT_TRUE(greedyMatch.has_value());
    ASSERT_TRUE(nonGreedyMatch.has_value());
    
    EXPECT_EQ(greedyMatch->text(), "<tag>content</tag>");
    EXPECT_EQ(nonGreedyMatch->text(), "<tag>");
}

// Test 39: Multiple matches with positions
TEST(RegexTest, MultipleMatchesWithPositions) {
    Regex regex("\\d+");
    auto matches = regex.findAll("10 apples, 20 oranges, 30 bananas");
    
    ASSERT_EQ(matches.size(), 3);
    EXPECT_EQ(matches[0].text(), "10");
    EXPECT_EQ(matches[1].text(), "20");
    EXPECT_EQ(matches[2].text(), "30");
}

// Test 40: Complex pattern
TEST(RegexTest, ComplexPattern) {
    // Date pattern: YYYY-MM-DD
    Regex regex("(\\d{4})-(\\d{2})-(\\d{2})");
    auto match = regex.find("Today is 2024-12-02");
    
    ASSERT_TRUE(match.has_value());
    EXPECT_EQ(match->group(1).value(), "2024");
    EXPECT_EQ(match->group(2).value(), "12");
    EXPECT_EQ(match->group(3).value(), "02");
}
