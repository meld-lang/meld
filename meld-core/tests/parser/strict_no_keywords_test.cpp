#include <gtest/gtest.h>
#include "meld/parser/parser.hpp"

using namespace meld::parser;

// Test that strict mode generates errors for each deprecated keyword
TEST(StrictNoKeywordsTest, RejectsEffectKeyword) {
    std::string code = R"(
        effect FileSystem {
            fnc read(path: string) -> string
        }
    )";
    
    Lexer lexer(code, true);
    lexer.tokenize();
    
    EXPECT_FALSE(lexer.errors().empty()) << "Expected error for 'effect' keyword in strict mode";
    EXPECT_TRUE(lexer.warnings().empty()) << "Expected no warnings in strict mode (errors instead)";
    
    bool found = false;
    for (const auto& err : lexer.errors()) {
        if (err.find("effect") != std::string::npos && err.find("strict mode") != std::string::npos) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "Error should mention 'effect' and 'strict mode'";
}

TEST(StrictNoKeywordsTest, RejectsImposesKeyword) {
    std::string code = R"(
        fnc readFile(path: string) -> string
            imposes [FileSystem]
        {
            return ""
        }
    )";
    
    Lexer lexer(code, true);
    lexer.tokenize();
    
    EXPECT_FALSE(lexer.errors().empty()) << "Expected error for 'imposes' keyword in strict mode";
    
    bool found = false;
    for (const auto& err : lexer.errors()) {
        if (err.find("imposes") != std::string::npos && err.find("strict mode") != std::string::npos) {
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST(StrictNoKeywordsTest, RejectsPerformKeyword) {
    std::string code = R"(
        fnc readConfig() {
            val content = perform FileSystem.read("config.txt")
        }
    )";
    
    Lexer lexer(code, true);
    lexer.tokenize();
    
    EXPECT_FALSE(lexer.errors().empty()) << "Expected error for 'perform' keyword in strict mode";
    
    bool found = false;
    for (const auto& err : lexer.errors()) {
        if (err.find("perform") != std::string::npos && err.find("strict mode") != std::string::npos) {
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST(StrictNoKeywordsTest, RejectsHandleAndWithKeywords) {
    std::string code = R"(
        val result = handle {
            readConfig()
        } with FileSystem {
            fnc read(path: string) -> string {
                return "mocked"
            }
        }
    )";
    
    Lexer lexer(code, true);
    lexer.tokenize();
    
    const auto& errors = lexer.errors();
    EXPECT_GE(errors.size(), 2u) << "Expected errors for both 'handle' and 'with' keywords";
    
    bool found_handle = false, found_with = false;
    for (const auto& err : errors) {
        if (err.find("handle") != std::string::npos && err.find("strict mode") != std::string::npos) {
            found_handle = true;
        }
        if (err.find("'with'") != std::string::npos && err.find("strict mode") != std::string::npos) {
            found_with = true;
        }
    }
    EXPECT_TRUE(found_handle) << "Expected error for 'handle' keyword";
    EXPECT_TRUE(found_with) << "Expected error for 'with' keyword";
}

TEST(StrictNoKeywordsTest, RejectsResumeKeyword) {
    std::string code = R"(
        fnc read(path: string) -> string {
            return resume("mocked content")
        }
    )";
    
    Lexer lexer(code, true);
    lexer.tokenize();
    
    EXPECT_FALSE(lexer.errors().empty()) << "Expected error for 'resume' keyword in strict mode";
    
    bool found = false;
    for (const auto& err : lexer.errors()) {
        if (err.find("resume") != std::string::npos && err.find("strict mode") != std::string::npos) {
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST(StrictNoKeywordsTest, RejectsAllDeprecatedKeywords) {
    std::string code = R"(
        effect FileSystem {
            fnc read(path: string) -> string
        }
        
        fnc readConfig(path: string) -> string
            imposes [FileSystem]
        {
            val content = perform FileSystem.read(path)
            return content
        }
        
        val result = handle {
            readConfig("config.txt")
        } with FileSystem {
            fnc read(path: string) -> string {
                return resume("mocked")
            }
        }
    )";
    
    Lexer lexer(code, true);
    lexer.tokenize();
    
    // Should have errors for: effect, imposes, perform, handle, with, resume
    EXPECT_GE(lexer.errors().size(), 6u) << "Expected at least 6 errors for all deprecated keywords";
    EXPECT_TRUE(lexer.warnings().empty()) << "Strict mode should produce errors, not warnings";
}

TEST(StrictNoKeywordsTest, AcceptsAnnotationSyntax) {
    std::string code = R"(
        @effect
        trait FileSystem {
            fnc read(path: string) -> string
        }
        
        @uses(FileSystem)
        fnc readConfig(path: string) -> string {
            val content = perform { FileSystem.read(path) }
            return content
        }
    )";
    
    Lexer lexer(code, true);
    lexer.tokenize();
    
    EXPECT_TRUE(lexer.errors().empty()) << "Annotation-based syntax should produce no errors in strict mode";
    EXPECT_TRUE(lexer.warnings().empty()) << "Annotation-based syntax should produce no warnings";
}

TEST(StrictNoKeywordsTest, NonStrictModeProducesWarningsNotErrors) {
    std::string code = R"(
        effect FileSystem {
            fnc read(path: string) -> string
        }
    )";
    
    Lexer lexer(code, false);
    lexer.tokenize();
    
    EXPECT_TRUE(lexer.errors().empty()) << "Non-strict mode should not produce errors for deprecated keywords";
    EXPECT_FALSE(lexer.warnings().empty()) << "Non-strict mode should produce warnings for deprecated keywords";
}

TEST(StrictNoKeywordsTest, DefaultModeIsNonStrict) {
    std::string code = R"(
        effect FileSystem {
            fnc read(path: string) -> string
        }
    )";
    
    // Default constructor (no strict flag)
    Lexer lexer(code);
    lexer.tokenize();
    
    EXPECT_TRUE(lexer.errors().empty()) << "Default mode should not produce errors";
    EXPECT_FALSE(lexer.warnings().empty()) << "Default mode should produce warnings";
}

TEST(StrictNoKeywordsTest, ErrorMessageSuggestsReplacement) {
    std::string code = "effect FileSystem { }";
    
    Lexer lexer(code, true);
    lexer.tokenize();
    
    ASSERT_FALSE(lexer.errors().empty());
    const auto& err = lexer.errors()[0];
    EXPECT_NE(err.find("@effect"), std::string::npos) << "Error should suggest @effect replacement";
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
