#include <gtest/gtest.h>
#include "meld/compiler/compiler.hpp"
#include "meld/provenance/provenance.hpp"
#include "meld/kernel/primitives.hpp"

using namespace meld;
using namespace meld::compiler;
using namespace meld::provenance;

class TrustLevelEnforcementTest : public ::testing::Test {
protected:
    void SetUp() override {
        compiler_ = std::make_unique<Compiler>();
    }
    
    std::unique_ptr<Compiler> compiler_;
};

TEST_F(TrustLevelEnforcementTest, NoTrustLevelEnforcement) {
    // Test that compilation succeeds when no trust level is specified
    std::string source = R"(
        fnc test_function() -> int {
            rtn 42
        }
    )";
    
    CompilationOptions options;
    // No minimum_trust_level set
    
    auto result = compiler_->compile_source(source, "test.meld", options);
    
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.diagnostics.size(), 0);
}

TEST_F(TrustLevelEnforcementTest, TrustLevelZeroAllowsAllCode) {
    // Test that trust level 0.0 allows all code (no enforcement)
    std::string source = R"(
        fnc test_function() -> int {
            rtn 42
        }
    )";
    
    CompilationOptions options;
    options.minimum_trust_level = 0.0;  // Allow all code
    
    auto result = compiler_->compile_source(source, "test.meld", options);
    
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.diagnostics.size(), 0);
}

TEST_F(TrustLevelEnforcementTest, TrustLevelEnforcementFailsForUntrustedCode) {
    // Test that trust level enforcement fails for code without provenance metadata
    std::string source = R"(
        fnc test_function() -> int {
            rtn 42
        }
    )";
    
    CompilationOptions options;
    options.minimum_trust_level = 0.5;  // Require at least low confidence
    
    auto result = compiler_->compile_source(source, "test.meld", options);
    
    EXPECT_FALSE(result.success);
    EXPECT_GT(result.diagnostics.size(), 0);
    
    // Check that we have trust-related error messages
    bool found_trust_error = false;
    for (const auto& diagnostic : result.diagnostics) {
        if (diagnostic.level == DiagnosticLevel::ERROR &&
            diagnostic.message.find("trust level") != std::string::npos) {
            found_trust_error = true;
            break;
        }
    }
    EXPECT_TRUE(found_trust_error);
}

TEST_F(TrustLevelEnforcementTest, HighTrustLevelRequiresVerifiedCode) {
    // Test that high trust level (1.0) requires verified code
    std::string source = R"(
        fnc test_function() -> int {
            rtn 42
        }
    )";
    
    CompilationOptions options;
    options.minimum_trust_level = 1.0;  // Require verified code only
    
    auto result = compiler_->compile_source(source, "test.meld", options);
    
    EXPECT_FALSE(result.success);
    EXPECT_GT(result.diagnostics.size(), 0);
    
    // Check for specific trust level error
    bool found_verified_error = false;
    for (const auto& diagnostic : result.diagnostics) {
        if (diagnostic.level == DiagnosticLevel::ERROR &&
            (diagnostic.message.find("VERIFIED") != std::string::npos ||
             diagnostic.message.find("1.0") != std::string::npos)) {
            found_verified_error = true;
            break;
        }
    }
    EXPECT_TRUE(found_verified_error);
}

TEST_F(TrustLevelEnforcementTest, InvalidTrustLevelValues) {
    // Test validation of trust level values in CLI
    // This would be tested at the CLI level, but we can test the compiler's handling
    
    CompilationOptions options;
    
    // Test that negative values are handled (should not be set by CLI, but test robustness)
    options.minimum_trust_level = -0.1;
    
    std::string source = R"(
        fnc test_function() -> int {
            rtn 42
        }
    )";
    
    // The compiler should handle invalid trust levels gracefully
    auto result = compiler_->compile_source(source, "test.meld", options);
    
    // With negative trust level, all code should pass (treated as 0.0)
    EXPECT_TRUE(result.success);
}

TEST_F(TrustLevelEnforcementTest, TrustLevelErrorMessages) {
    // Test that error messages are informative and include trust level information
    std::string source = R"(
        fnc my_function() -> int {
            rtn 42
        }
        
        class MyClass {
            var value: int
        }
    )";
    
    CompilationOptions options;
    options.minimum_trust_level = 0.9;  // High confidence required
    
    auto result = compiler_->compile_source(source, "test.meld", options);
    
    EXPECT_FALSE(result.success);
    EXPECT_GT(result.diagnostics.size(), 0);
    
    // Check that error messages contain useful information
    for (const auto& diagnostic : result.diagnostics) {
        if (diagnostic.level == DiagnosticLevel::ERROR) {
            // Should contain trust level numbers
            EXPECT_TRUE(diagnostic.message.find("0.9") != std::string::npos ||
                       diagnostic.message.find("trust level") != std::string::npos);
        }
    }
}

