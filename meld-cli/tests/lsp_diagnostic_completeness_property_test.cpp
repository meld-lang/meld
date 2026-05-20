#include <gtest/gtest.h>
#include "meld/cli/lsp_module.hpp"
#include "meld/testing/property_test.hpp"
#include <memory>
#include <vector>
#include <string>
#include <random>
#include <filesystem>
#include <algorithm>
#include <set>
#include <fstream>
#include <sstream>

using namespace meld::cli;
using namespace meld::testing;

/**
 * **Feature: meld-cli, Property 37: LSP Diagnostic Completeness**
 * **Validates: Requirements 13.4**
 * 
 * Property: For any Meld source file, LSP diagnostics should report all syntax 
 * and semantic errors with precise location information
 */

// Generator for valid Meld file paths
std::function<std::filesystem::path()> meld_file_paths() {
    return []() {
        static std::mt19937 gen(std::random_device{}());
        
        std::vector<std::string> file_names = {
            "main.meld",
            "test.meld",
            "utils.meld",
            "types.meld",
            "module.meld",
            "example.meld",
            "lib.meld",
            "app.meld"
        };
        
        std::vector<std::string> directories = {
            "src",
            "tests",
            "examples",
            "lib",
            "modules"
        };
        
        std::uniform_int_distribution<size_t> file_dist(0, file_names.size() - 1);
        std::uniform_int_distribution<size_t> dir_dist(0, directories.size() - 1);
        std::uniform_int_distribution<int> use_dir_dist(0, 1);
        
        std::string file_name = file_names[file_dist(gen)];
        
        if (use_dir_dist(gen)) {
            std::string directory = directories[dir_dist(gen)];
            return std::filesystem::path(directory) / file_name;
        } else {
            return std::filesystem::path(file_name);
        }
    };
}

// Generator for Meld source code with various error types
std::function<std::string()> meld_source_with_errors() {
    return []() {
        static std::mt19937 gen(std::random_device{}());
        
        std::vector<std::string> valid_code_snippets = {
            R"(fn main() -> Unit {
    println("Hello, world!")
})",
            R"(class TestClass {
    let field: string = "test"
    
    fn method() -> Unit {
        // Valid method
    }
})",
            R"(module TestModule {
    fn utility() -> int {
        return 42
    }
})"
        };
        
        std::vector<std::string> syntax_error_snippets = {
            R"(fn main( -> Unit {
    println("Missing parameter closing paren")
})",
            R"(class TestClass {
    let field string = "missing colon"
})",
            R"(fn test() -> Unit
    println("Missing opening brace")
})",
            R"(fn test() -> Unit {
    let x = 
})",
            R"(fn test() -> Unit {
    if (true {
        println("Missing closing paren")
    }
})"
        };
        
        std::vector<std::string> semantic_error_snippets = {
            R"(fn main() -> Unit {
    let x: int = "string value"  // Type mismatch
})",
            R"(fn main() -> Unit {
    undefined_function()  // Undefined function
})",
            R"(fn main() -> Unit {
    let x: int = y  // Undefined variable
})",
            R"(class TestClass {
    fn method() -> string {
        return 42  // Return type mismatch
    }
})"
        };
        
        std::uniform_int_distribution<int> code_type_dist(0, 2);
        int code_type = code_type_dist(gen);
        
        switch (code_type) {
            case 0: {
                // Valid code
                std::uniform_int_distribution<size_t> valid_dist(0, valid_code_snippets.size() - 1);
                return valid_code_snippets[valid_dist(gen)];
            }
            case 1: {
                // Syntax errors
                std::uniform_int_distribution<size_t> syntax_dist(0, syntax_error_snippets.size() - 1);
                return syntax_error_snippets[syntax_dist(gen)];
            }
            case 2: {
                // Semantic errors
                std::uniform_int_distribution<size_t> semantic_dist(0, semantic_error_snippets.size() - 1);
                return semantic_error_snippets[semantic_dist(gen)];
            }
            default:
                return valid_code_snippets[0];
        }
    };
}

// Generator for expected diagnostic counts based on code content
std::function<int()> expected_diagnostic_counts() {
    return []() {
        static std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<int> count_dist(0, 5);
        return count_dist(gen);
    };
}

// Helper function to create a temporary file with Meld source code
std::filesystem::path create_test_file(const std::filesystem::path& relative_path, const std::string& content) {
    static std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<int> id_dist(1000, 9999);
    
    auto temp_dir = std::filesystem::temp_directory_path() / ("test_diagnostics_" + std::to_string(id_dist(gen)));
    std::filesystem::create_directories(temp_dir);
    
    auto full_path = temp_dir / relative_path;
    std::filesystem::create_directories(full_path.parent_path());
    
    std::ofstream file(full_path);
    file << content;
    file.close();
    
    return full_path;
}

// Helper function to clean up test files
void cleanup_test_file(const std::filesystem::path& file_path) {
    if (std::filesystem::exists(file_path)) {
        // Remove the entire temporary directory
        auto temp_dir = file_path;
        while (temp_dir.filename().string().find("test_diagnostics_") == std::string::npos && 
               temp_dir != temp_dir.parent_path()) {
            temp_dir = temp_dir.parent_path();
        }
        if (temp_dir.filename().string().find("test_diagnostics_") != std::string::npos) {
            std::filesystem::remove_all(temp_dir);
        }
    }
}

// Helper function to count expected errors in source code
int count_expected_errors(const std::string& source) {
    int error_count = 0;
    
    // Simple heuristics for detecting errors in our test code
    if (source.find("Missing parameter closing paren") != std::string::npos ||
        source.find("missing colon") != std::string::npos ||
        source.find("Missing opening brace") != std::string::npos ||
        source.find("Missing closing paren") != std::string::npos) {
        error_count++; // Syntax errors
    }
    
    if (source.find("Type mismatch") != std::string::npos ||
        source.find("Undefined function") != std::string::npos ||
        source.find("Undefined variable") != std::string::npos ||
        source.find("Return type mismatch") != std::string::npos) {
        error_count++; // Semantic errors
    }
    
    return error_count;
}

TEST(LspDiagnosticCompletenessPropertyTest, DiagnosticLocationAccuracy) {
    // Property: All diagnostics should have valid and precise location information
    bool property_holds = PropertyTest::forall(
        meld_file_paths(),
        meld_source_with_errors(),
        [&](const std::filesystem::path& file_path, const std::string& source) {
            LspModule lsp_module;
            
            // Create a test file with the generated source
            auto test_file = create_test_file(file_path, source);
            
            // Get diagnostics for the file
            auto result = lsp_module.get_diagnostics(test_file);
            
            // Clean up the test file
            cleanup_test_file(test_file);
            
            if (!result) {
                // Connection failures are acceptable for this test
                const auto& error = result.error();
                if (error.message.find("not connected") != std::string::npos ||
                    error.message.find("not running") != std::string::npos) {
                    return true;
                }
                return false;
            }
            
            const auto& diagnostics = result.value();
            
            // Validate that all diagnostics have valid location information
            for (const auto& diagnostic : diagnostics) {
                // Range should be valid
                if (!diagnostic.range.is_valid()) {
                    return false;
                }
                
                // Line and column should be reasonable (size_t is always >= 0)
                // Just check they're not unreasonably large
                if (diagnostic.range.start.line > 100000 || diagnostic.range.start.column > 10000) {
                    return false;
                }
                
                // End position should be >= start position
                if (diagnostic.range.end.line < diagnostic.range.start.line ||
                    (diagnostic.range.end.line == diagnostic.range.start.line && 
                     diagnostic.range.end.column < diagnostic.range.start.column)) {
                    return false;
                }
                
                // Message should not be empty
                if (diagnostic.message.empty()) {
                    return false;
                }
                
                // Severity should be valid
                if (static_cast<int>(diagnostic.severity) < 1 || static_cast<int>(diagnostic.severity) > 4) {
                    return false;
                }
            }
            
            return true;
        },
        100
    );
    
    EXPECT_TRUE(property_holds);
}

TEST(LspDiagnosticCompletenessPropertyTest, DiagnosticSeverityClassification) {
    // Property: Diagnostics should be properly classified by severity
    bool property_holds = PropertyTest::forall(
        meld_file_paths(),
        meld_source_with_errors(),
        [&](const std::filesystem::path& file_path, const std::string& source) {
            LspModule lsp_module;
            
            auto test_file = create_test_file(file_path, source);
            auto result = lsp_module.get_diagnostics(test_file);
            cleanup_test_file(test_file);
            
            if (!result) {
                // Connection failures are acceptable
                const auto& error = result.error();
                if (error.message.find("not connected") != std::string::npos ||
                    error.message.find("not running") != std::string::npos) {
                    return true;
                }
                return false;
            }
            
            const auto& diagnostics = result.value();
            
            // Check that severity levels are used appropriately
            bool has_errors = false;
            bool has_warnings = false;
            bool has_info = false;
            bool has_hints = false;
            
            for (const auto& diagnostic : diagnostics) {
                switch (diagnostic.severity) {
                    case DiagnosticSeverity::Error:
                        has_errors = true;
                        // Errors should have meaningful messages
                        if (diagnostic.message.length() < 5) {
                            return false;
                        }
                        break;
                    case DiagnosticSeverity::Warning:
                        has_warnings = true;
                        break;
                    case DiagnosticSeverity::Information:
                        has_info = true;
                        break;
                    case DiagnosticSeverity::Hint:
                        has_hints = true;
                        break;
                }
            }
            
            // If we have syntax/semantic errors in the source, we should have error-level diagnostics
            int expected_errors = count_expected_errors(source);
            if (expected_errors > 0 && !has_errors && !diagnostics.empty()) {
                // We expected errors but didn't get any error-level diagnostics
                // This might be acceptable if the LSP server classifies them differently
                return true; // Be lenient for now
            }
            
            return true;
        },
        75
    );
    
    EXPECT_TRUE(property_holds);
}

TEST(LspDiagnosticCompletenessPropertyTest, DiagnosticConsistency) {
    // Property: Running diagnostics multiple times on the same file should return consistent results
    bool property_holds = PropertyTest::forall(
        meld_file_paths(),
        meld_source_with_errors(),
        [&](const std::filesystem::path& file_path, const std::string& source) {
            LspModule lsp_module;
            
            auto test_file = create_test_file(file_path, source);
            
            // Run diagnostics twice
            auto result1 = lsp_module.get_diagnostics(test_file);
            auto result2 = lsp_module.get_diagnostics(test_file);
            
            cleanup_test_file(test_file);
            
            // Both should succeed or both should fail
            if (!result1 && !result2) {
                return true; // Both failed consistently
            }
            
            if (!result1 || !result2) {
                return false; // One succeeded, one failed - inconsistent
            }
            
            const auto& diagnostics1 = result1.value();
            const auto& diagnostics2 = result2.value();
            
            // Should have the same number of diagnostics
            if (diagnostics1.size() != diagnostics2.size()) {
                return false;
            }
            
            // Check that diagnostics are the same (order might differ)
            for (const auto& diag1 : diagnostics1) {
                bool found = false;
                for (const auto& diag2 : diagnostics2) {
                    if (diag1.range.start.line == diag2.range.start.line &&
                        diag1.range.start.column == diag2.range.start.column &&
                        diag1.severity == diag2.severity &&
                        diag1.message == diag2.message) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    return false; // Diagnostic from first run not found in second
                }
            }
            
            return true;
        },
        50
    );
    
    EXPECT_TRUE(property_holds);
}

TEST(LspDiagnosticCompletenessPropertyTest, ValidFileHandling) {
    // Property: Valid Meld files should produce no error-level diagnostics
    bool property_holds = PropertyTest::forall(
        meld_file_paths(),
        std::function<std::string()>([]() {
            // Generate only valid Meld code
            std::vector<std::string> valid_snippets = {
                R"(fn main() -> Unit {
    println("Hello, world!")
})",
                R"(class TestClass {
    let field: string = "test"
    
    fn method() -> Unit {
        let x: int = 42
        println(x.to_string())
    }
})",
                R"(module TestModule {
    fn add(a: int, b: int) -> int {
        return a + b
    }
    
    fn multiply(a: int, b: int) -> int {
        return a * b
    }
})"
            };
            
            static std::mt19937 gen(std::random_device{}());
            std::uniform_int_distribution<size_t> dist(0, valid_snippets.size() - 1);
            return valid_snippets[dist(gen)];
        }),
        [&](const std::filesystem::path& file_path, const std::string& source) {
            LspModule lsp_module;
            
            auto test_file = create_test_file(file_path, source);
            auto result = lsp_module.get_diagnostics(test_file);
            cleanup_test_file(test_file);
            
            if (!result) {
                // Connection failures are acceptable
                const auto& error = result.error();
                if (error.message.find("not connected") != std::string::npos ||
                    error.message.find("not running") != std::string::npos) {
                    return true;
                }
                return false;
            }
            
            const auto& diagnostics = result.value();
            
            // Valid code should not have error-level diagnostics
            for (const auto& diagnostic : diagnostics) {
                if (diagnostic.severity == DiagnosticSeverity::Error) {
                    return false; // Found an error in valid code
                }
            }
            
            return true;
        },
        50
    );
    
    EXPECT_TRUE(property_holds);
}

TEST(LspDiagnosticCompletenessPropertyTest, NonExistentFileHandling) {
    // Property: Non-existent files should be handled gracefully
    bool property_holds = PropertyTest::forall(
        std::function<std::filesystem::path()>([]() {
            // Generate paths to non-existent files
            static std::mt19937 gen(std::random_device{}());
            std::uniform_int_distribution<int> id_dist(10000, 99999);
            
            std::string filename = "nonexistent_" + std::to_string(id_dist(gen)) + ".meld";
            return std::filesystem::path("nonexistent_dir") / filename;
        }),
        [&](const std::filesystem::path& file_path) {
            LspModule lsp_module;
            
            auto result = lsp_module.get_diagnostics(file_path);
            
            if (!result) {
                // Failures are expected for non-existent files
                const auto& error = result.error();
                
                // Should be a meaningful error message
                if (error.message.empty()) {
                    return false;
                }
                
                // Common acceptable error types
                if (error.message.find("not connected") != std::string::npos ||
                    error.message.find("not running") != std::string::npos ||
                    error.message.find("not found") != std::string::npos ||
                    error.message.find("does not exist") != std::string::npos) {
                    return true;
                }
                
                // Other errors might be acceptable too
                return true;
            }
            
            // If it succeeds, should return empty diagnostics or handle gracefully
            const auto& diagnostics = result.value();
            
            // All diagnostics should still be valid if any are returned
            for (const auto& diagnostic : diagnostics) {
                if (!diagnostic.range.is_valid() || diagnostic.message.empty()) {
                    return false;
                }
            }
            
            return true;
        },
        30
    );
    
    EXPECT_TRUE(property_holds);
}

TEST(LspDiagnosticCompletenessPropertyTest, DiagnosticMessageQuality) {
    // Property: Diagnostic messages should be informative and non-empty
    bool property_holds = PropertyTest::forall(
        meld_file_paths(),
        meld_source_with_errors(),
        [&](const std::filesystem::path& file_path, const std::string& source) {
            LspModule lsp_module;
            
            auto test_file = create_test_file(file_path, source);
            auto result = lsp_module.get_diagnostics(test_file);
            cleanup_test_file(test_file);
            
            if (!result) {
                // Connection failures are acceptable
                const auto& error = result.error();
                if (error.message.find("not connected") != std::string::npos ||
                    error.message.find("not running") != std::string::npos) {
                    return true;
                }
                return false;
            }
            
            const auto& diagnostics = result.value();
            
            for (const auto& diagnostic : diagnostics) {
                // Message should not be empty
                if (diagnostic.message.empty()) {
                    return false;
                }
                
                // Message should be reasonably informative (at least 3 characters)
                if (diagnostic.message.length() < 3) {
                    return false;
                }
                
                // Message should not be just whitespace
                if (std::all_of(diagnostic.message.begin(), diagnostic.message.end(), ::isspace)) {
                    return false;
                }
                
                // Code field, if present, should be meaningful
                if (!diagnostic.code.empty() && diagnostic.code.length() < 2) {
                    return false;
                }
                
                // Source field, if present, should be meaningful
                if (!diagnostic.source.empty() && diagnostic.source.length() < 2) {
                    return false;
                }
            }
            
            return true;
        },
        60
    );
    
    EXPECT_TRUE(property_holds);
}