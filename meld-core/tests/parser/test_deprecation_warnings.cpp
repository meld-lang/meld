#include "meld/parser/parser.hpp"
#include <cassert>
#include <iostream>
#include <string>

using namespace meld::parser;

void test_effect_keyword_warning() {
    std::cout << "Testing 'effect' keyword deprecation warning..." << std::endl;
    
    std::string code = R"(
        effect FileSystem {
            fnc read(path: string) -> string
        }
    )";
    
    Lexer lexer(code);
    auto tokens = lexer.tokenize();
    auto warnings = lexer.warnings();
    
    // Should have at least one warning for 'effect' keyword
    assert(!warnings.empty() && "Expected deprecation warning for 'effect' keyword");
    
    // Check that the warning mentions 'effect' and suggests '@effect'
    bool found_effect_warning = false;
    for (const auto& warning : warnings) {
        if (warning.find("effect") != std::string::npos && 
            warning.find("@effect") != std::string::npos) {
            found_effect_warning = true;
            std::cout << "  Warning: " << warning << std::endl;
            break;
        }
    }
    
    assert(found_effect_warning && "Expected warning to mention 'effect' and '@effect'");
    std::cout << "  ✓ 'effect' keyword warning works correctly" << std::endl;
}

void test_imposes_keyword_warning() {
    std::cout << "Testing 'imposes' keyword deprecation warning..." << std::endl;
    
    std::string code = R"(
        fnc readFile(path: string) -> string
            imposes [FileSystem]
        {
            return ""
        }
    )";
    
    Lexer lexer(code);
    auto tokens = lexer.tokenize();
    auto warnings = lexer.warnings();
    
    // Should have at least one warning for 'imposes' keyword
    assert(!warnings.empty() && "Expected deprecation warning for 'imposes' keyword");
    
    // Check that the warning mentions 'imposes' and suggests '@imposes'
    bool found_imposes_warning = false;
    for (const auto& warning : warnings) {
        if (warning.find("imposes") != std::string::npos && 
            warning.find("@imposes") != std::string::npos) {
            found_imposes_warning = true;
            std::cout << "  Warning: " << warning << std::endl;
            break;
        }
    }
    
    assert(found_imposes_warning && "Expected warning to mention 'imposes' and '@imposes'");
    std::cout << "  ✓ 'imposes' keyword warning works correctly" << std::endl;
}

void test_perform_keyword_warning() {
    std::cout << "Testing 'perform' keyword deprecation warning..." << std::endl;
    
    std::string code = R"(
        fnc readConfig() {
            val content = perform FileSystem.read("config.txt")
        }
    )";
    
    Lexer lexer(code);
    auto tokens = lexer.tokenize();
    auto warnings = lexer.warnings();
    
    // Should have at least one warning for 'perform' keyword
    assert(!warnings.empty() && "Expected deprecation warning for 'perform' keyword");
    
    // Check that the warning mentions 'perform' and suggests the function syntax
    bool found_perform_warning = false;
    for (const auto& warning : warnings) {
        if (warning.find("perform") != std::string::npos && 
            warning.find("perform {") != std::string::npos) {
            found_perform_warning = true;
            std::cout << "  Warning: " << warning << std::endl;
            break;
        }
    }
    
    assert(found_perform_warning && "Expected warning to mention 'perform' and function syntax");
    std::cout << "  ✓ 'perform' keyword warning works correctly" << std::endl;
}

void test_handle_with_keywords_warning() {
    std::cout << "Testing 'handle' and 'with' keywords deprecation warning..." << std::endl;
    
    std::string code = R"(
        val result = handle {
            readConfig()
        } with FileSystem {
            fnc read(path: string) -> string {
                return "mocked"
            }
        }
    )";
    
    Lexer lexer(code);
    auto tokens = lexer.tokenize();
    auto warnings = lexer.warnings();
    
    // Should have warnings for both 'handle' and 'with' keywords
    assert(warnings.size() >= 2 && "Expected deprecation warnings for 'handle' and 'with' keywords");
    
    // Check for handle warning
    bool found_handle_warning = false;
    bool found_with_warning = false;
    
    for (const auto& warning : warnings) {
        if (warning.find("handle") != std::string::npos && 
            warning.find("handle(computation:") != std::string::npos) {
            found_handle_warning = true;
            std::cout << "  Warning: " << warning << std::endl;
        }
        if (warning.find("with") != std::string::npos) {
            found_with_warning = true;
            std::cout << "  Warning: " << warning << std::endl;
        }
    }
    
    assert(found_handle_warning && "Expected warning for 'handle' keyword");
    assert(found_with_warning && "Expected warning for 'with' keyword");
    std::cout << "  ✓ 'handle' and 'with' keyword warnings work correctly" << std::endl;
}

void test_resume_keyword_warning() {
    std::cout << "Testing 'resume' keyword deprecation warning..." << std::endl;
    
    std::string code = R"(
        fnc read(path: string) -> string {
            return resume("mocked content")
        }
    )";
    
    Lexer lexer(code);
    auto tokens = lexer.tokenize();
    auto warnings = lexer.warnings();
    
    // Should have at least one warning for 'resume' keyword
    assert(!warnings.empty() && "Expected deprecation warning for 'resume' keyword");
    
    // Check that the warning mentions 'resume'
    bool found_resume_warning = false;
    for (const auto& warning : warnings) {
        if (warning.find("resume") != std::string::npos) {
            found_resume_warning = true;
            std::cout << "  Warning: " << warning << std::endl;
            break;
        }
    }
    
    assert(found_resume_warning && "Expected warning to mention 'resume'");
    std::cout << "  ✓ 'resume' keyword warning works correctly" << std::endl;
}

void test_multiple_deprecated_keywords() {
    std::cout << "Testing multiple deprecated keywords in one file..." << std::endl;
    
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
    
    Lexer lexer(code);
    auto tokens = lexer.tokenize();
    auto warnings = lexer.warnings();
    
    // Should have warnings for: effect, imposes, perform, handle, with, resume
    assert(warnings.size() >= 6 && "Expected at least 6 deprecation warnings");
    
    std::cout << "  Found " << warnings.size() << " deprecation warnings:" << std::endl;
    for (const auto& warning : warnings) {
        std::cout << "    - " << warning << std::endl;
    }
    
    std::cout << "  ✓ Multiple deprecated keywords detected correctly" << std::endl;
}

void test_no_warnings_for_new_syntax() {
    std::cout << "Testing that new syntax doesn't trigger warnings..." << std::endl;
    
    std::string code = R"(
        @effect
        trait FileSystem {
            fnc read(path: string) -> string
        }
        
        @imposes(FileSystem)
        fnc readConfig(path: string) -> string {
            val content = perform { FileSystem.read(path) }
            return content
        }
    )";
    
    Lexer lexer(code);
    auto tokens = lexer.tokenize();
    auto warnings = lexer.warnings();
    
    // Should have no warnings for the new annotation-based syntax
    assert(warnings.empty() && "Expected no warnings for new annotation-based syntax");
    
    std::cout << "  ✓ New annotation-based syntax produces no warnings" << std::endl;
}

int main() {
    std::cout << "=== Deprecation Warnings Test Suite ===" << std::endl << std::endl;
    
    try {
        test_effect_keyword_warning();
        std::cout << std::endl;
        
        test_imposes_keyword_warning();
        std::cout << std::endl;
        
        test_perform_keyword_warning();
        std::cout << std::endl;
        
        test_handle_with_keywords_warning();
        std::cout << std::endl;
        
        test_resume_keyword_warning();
        std::cout << std::endl;
        
        test_multiple_deprecated_keywords();
        std::cout << std::endl;
        
        test_no_warnings_for_new_syntax();
        std::cout << std::endl;
        
        std::cout << "=== All Deprecation Warning Tests Passed! ===" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}
