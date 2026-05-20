#include <gtest/gtest.h>
#include "../src/code_analyzer.hpp"

namespace meld::mcp {

class CodeAnalyzerTest : public ::testing::Test {
protected:
    void SetUp() override {
        analyzer = std::make_unique<CodeAnalyzer>();
    }
    
    std::unique_ptr<CodeAnalyzer> analyzer;
};

TEST_F(CodeAnalyzerTest, AnalyzeFile) {
    // Test that files can be analyzed without errors
    EXPECT_NO_THROW(analyzer->analyzeFile("test.meld"));
}

TEST_F(CodeAnalyzerTest, GetSemanticInfo) {
    // Test that semantic info can be retrieved
    std::string code = "let x = 42";
    auto info = analyzer->getSemanticInfo(code, 1, 5);
    EXPECT_FALSE(info.empty());
}

TEST_F(CodeAnalyzerTest, GetCompletions) {
    // Test that completions can be generated
    std::string code = "let x = ";
    auto completions = analyzer->getCompletions(code, 1, 8);
    EXPECT_FALSE(completions.empty());
}

TEST_F(CodeAnalyzerTest, ExtractContext) {
    // Test that context can be extracted
    std::string code = "function add(a, b) { return a + b; }";
    auto context = analyzer->extractContext(code);
    EXPECT_FALSE(context.empty());
}

} // namespace meld::mcp