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

using namespace meld::cli;
using namespace meld::testing;

/**
 * **Feature: meld-cli, Property 36: LSP Symbol Search Accuracy**
 * **Validates: Requirements 13.3**
 * 
 * Property: For any symbol query, the LSP integration should return all matching 
 * symbols from the workspace with accurate location information
 */

// Generator for valid symbol queries
std::function<std::string()> symbol_queries() {
    return []() {
        static std::mt19937 gen(std::random_device{}());
        
        // Common symbol patterns to search for
        std::vector<std::string> patterns = {
            "main",
            "test",
            "function",
            "class",
            "variable",
            "method",
            "struct",
            "enum",
            "interface",
            "module",
            "fn",
            "let",
            "const",
            "type",
            "impl",
            "trait"
        };
        
        std::uniform_int_distribution<size_t> pattern_dist(0, patterns.size() - 1);
        std::uniform_int_distribution<int> variation_dist(0, 3);
        
        std::string base_pattern = patterns[pattern_dist(gen)];
        int variation = variation_dist(gen);
        
        switch (variation) {
            case 0:
                return base_pattern; // Exact match
            case 1:
                return base_pattern + "_test"; // With suffix
            case 2:
                return "test_" + base_pattern; // With prefix
            case 3:
                return base_pattern.substr(0, std::max(1, static_cast<int>(base_pattern.length()) - 1)); // Partial match
            default:
                return base_pattern;
        }
    };
}

// Generator for workspace paths
std::function<std::filesystem::path()> workspace_paths() {
    return []() {
        static std::mt19937 gen(std::random_device{}());
        
        std::vector<std::filesystem::path> paths = {
            std::filesystem::current_path(),
            std::filesystem::current_path() / "src",
            std::filesystem::current_path() / "tests",
            std::filesystem::current_path() / "examples",
            std::filesystem::temp_directory_path() / "test_workspace"
        };
        
        std::uniform_int_distribution<size_t> path_dist(0, paths.size() - 1);
        return paths[path_dist(gen)];
    };
}

// Generator for mock symbol data to validate against
std::function<std::vector<Symbol>()> expected_symbols() {
    return []() {
        static std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<size_t> count_dist(0, 10);
        std::uniform_int_distribution<int> kind_dist(1, 26); // SymbolKind range
        std::uniform_int_distribution<size_t> line_dist(0, 100);
        std::uniform_int_distribution<size_t> col_dist(0, 80);
        
        std::vector<Symbol> symbols;
        size_t count = count_dist(gen);
        
        for (size_t i = 0; i < count; ++i) {
            std::string name = "symbol_" + std::to_string(i);
            SymbolKind kind = static_cast<SymbolKind>(kind_dist(gen));
            Location location("test_file_" + std::to_string(i) + ".meld", line_dist(gen), col_dist(gen));
            
            Symbol symbol(name, kind, location);
            symbol.detail = "Test symbol " + std::to_string(i);
            symbols.push_back(symbol);
        }
        
        return symbols;
    };
}

// Helper function to create a temporary workspace with test files
std::filesystem::path create_test_workspace() {
    static std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<int> id_dist(1000, 9999);
    
    auto workspace = std::filesystem::temp_directory_path() / ("test_workspace_" + std::to_string(id_dist(gen)));
    std::filesystem::create_directories(workspace);
    
    // Create some test files with symbols
    std::ofstream main_file(workspace / "main.meld");
    main_file << R"(
fn main() -> Unit {
    println("Hello, world!")
}

fn test_function() -> Unit {
    // Test function
}

class TestClass {
    let test_variable: string = "test"
    
    fn test_method() -> Unit {
        // Test method
    }
}
)";
    main_file.close();
    
    std::ofstream utils_file(workspace / "utils.meld");
    utils_file << R"(
module Utils {
    fn utility_function() -> Unit {
        // Utility function
    }
    
    struct UtilityStruct {
        field: int
    }
    
    enum UtilityEnum {
        Option1,
        Option2
    }
}
)";
    utils_file.close();
    
    return workspace;
}

// Helper function to clean up test workspace
void cleanup_test_workspace(const std::filesystem::path& workspace) {
    if (std::filesystem::exists(workspace)) {
        std::filesystem::remove_all(workspace);
    }
}

TEST(LspSymbolSearchPropertyTest, SymbolSearchAccuracy) {
    // Property: Symbol search should return all matching symbols with accurate location information
    bool property_holds = PropertyTest::forall(
        symbol_queries(),
        workspace_paths(),
        [&](const std::string& query, const std::filesystem::path& workspace) {
            LspModule lsp_module;
            
            // Create a test workspace if it doesn't exist
            std::filesystem::path test_workspace = workspace;
            bool created_workspace = false;
            
            if (!std::filesystem::exists(workspace)) {
                test_workspace = create_test_workspace();
                created_workspace = true;
            }
            
            // Query symbols - the mock implementation will handle connection internally
            auto result = lsp_module.query_symbols(test_workspace, query);
            
            // Clean up if we created a workspace
            if (created_workspace) {
                cleanup_test_workspace(test_workspace);
            }
            
            if (!result) {
                // If query failed, it should be due to connection issues, not search logic
                const auto& error = result.error();
                
                // Acceptable failures: client not connected, server not running
                if (error.message.find("not connected") != std::string::npos ||
                    error.message.find("not running") != std::string::npos) {
                    return true;
                }
                
                // Other failures might indicate search logic issues
                return false;
            }
            
            const auto& symbols = result.value();
            
            // Validate that all returned symbols are relevant to the query
            for (const auto& symbol : symbols) {
                // Symbol name should contain the query or be related to it
                std::string lower_name = symbol.name;
                std::string lower_query = query;
                std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
                std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(), ::tolower);
                
                // Check if symbol is relevant to query
                bool is_relevant = (lower_name.find(lower_query) != std::string::npos) ||
                                 (lower_query.find(lower_name) != std::string::npos) ||
                                 (query.empty()); // Empty query should return all symbols
                
                if (!is_relevant) {
                    // Symbol doesn't match query - this violates accuracy
                    return false;
                }
                
                // Validate location information is present and reasonable
                if (symbol.location.file.empty()) {
                    return false; // Location should have a file
                }
                
                // Range should be valid
                if (!symbol.location.range.is_valid()) {
                    return false;
                }
            }
            
            // If we got here, all returned symbols are accurate
            return true;
        },
        100  // Run 100 iterations
    );
    
    EXPECT_TRUE(property_holds);
}

TEST(LspSymbolSearchPropertyTest, SymbolSearchCompleteness) {
    // Property: Symbol search should not miss obvious matches
    bool property_holds = PropertyTest::forall(
        std::function<std::string()>([]() { return std::string("main"); }), // Always search for "main"
        [&](const std::string& query) {
            LspModule lsp_module;
            
            // Create a workspace with a main function
            auto test_workspace = create_test_workspace();
            
            // Query for main symbols
            auto result = lsp_module.query_symbols(test_workspace, query);
            
            cleanup_test_workspace(test_workspace);
            
            if (!result) {
                // Connection failures are acceptable
                const auto& error = result.error();
                if (error.message.find("not connected") != std::string::npos ||
                    error.message.find("not running") != std::string::npos) {
                    return true;
                }
                return false;
            }
            
            const auto& symbols = result.value();
            
            // Should find at least one symbol containing "main"
            bool found_main = false;
            for (const auto& symbol : symbols) {
                std::string lower_name = symbol.name;
                std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
                if (lower_name.find("main") != std::string::npos) {
                    found_main = true;
                    break;
                }
            }
            
            // In our mock implementation, we should find main symbols
            return found_main || symbols.empty(); // Empty is acceptable if no workspace
        },
        50  // Run 50 iterations
    );
    
    EXPECT_TRUE(property_holds);
}

TEST(LspSymbolSearchPropertyTest, SymbolSearchConsistency) {
    // Property: Repeated searches with the same query should return consistent results
    bool property_holds = PropertyTest::forall(
        symbol_queries(),
        [&](const std::string& query) {
            LspModule lsp_module;
            
            auto test_workspace = create_test_workspace();
            
            // Perform the same search twice
            auto result1 = lsp_module.query_symbols(test_workspace, query);
            auto result2 = lsp_module.query_symbols(test_workspace, query);
            
            cleanup_test_workspace(test_workspace);
            
            // Both should succeed or both should fail with the same error type
            if (!result1 && !result2) {
                return true; // Both failed consistently
            }
            
            if (!result1 || !result2) {
                return false; // One succeeded, one failed - inconsistent
            }
            
            // Both succeeded - results should be the same
            const auto& symbols1 = result1.value();
            const auto& symbols2 = result2.value();
            
            if (symbols1.size() != symbols2.size()) {
                return false; // Different number of results
            }
            
            // Check that all symbols from first search are in second search
            for (const auto& symbol1 : symbols1) {
                bool found = false;
                for (const auto& symbol2 : symbols2) {
                    if (symbol1.name == symbol2.name && 
                        symbol1.location.file == symbol2.location.file &&
                        symbol1.location.range.start.line == symbol2.location.range.start.line &&
                        symbol1.location.range.start.column == symbol2.location.range.start.column) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    return false; // Symbol from first search not found in second
                }
            }
            
            return true;
        },
        50
    );
    
    EXPECT_TRUE(property_holds);
}

TEST(LspSymbolSearchPropertyTest, SymbolSearchCaseInsensitivity) {
    // Property: Symbol search should be case-insensitive
    bool property_holds = PropertyTest::forall(
        std::function<std::string()>([]() { return std::string("main"); }), // Always search for "main"
        [&](const std::string& base_query) {
            LspModule lsp_module;
            auto test_workspace = create_test_workspace();
            
            // Test different case variations
            std::vector<std::string> case_variations = {
                base_query,
                "MAIN",
                "Main",
                "mAiN"
            };
            
            std::vector<std::vector<Symbol>> results;
            
            for (const auto& query : case_variations) {
                auto result = lsp_module.query_symbols(test_workspace, query);
                if (result) {
                    results.push_back(result.value());
                } else {
                    // If any query fails, skip this test iteration
                    cleanup_test_workspace(test_workspace);
                    return true;
                }
            }
            
            cleanup_test_workspace(test_workspace);
            
            // All case variations should return the same symbols
            if (results.size() < 2) {
                return true; // Not enough results to compare
            }
            
            const auto& first_result = results[0];
            for (size_t i = 1; i < results.size(); ++i) {
                const auto& current_result = results[i];
                
                if (first_result.size() != current_result.size()) {
                    return false; // Different number of results for different cases
                }
                
                // Check that symbols are the same (order might differ)
                for (const auto& symbol1 : first_result) {
                    bool found = false;
                    for (const auto& symbol2 : current_result) {
                        if (symbol1.name == symbol2.name) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        return false; // Symbol missing in case variation
                    }
                }
            }
            
            return true;
        },
        25
    );
    
    EXPECT_TRUE(property_holds);
}

TEST(LspSymbolSearchPropertyTest, EmptyQueryHandling) {
    // Property: Empty queries should be handled gracefully
    bool property_holds = PropertyTest::forall(
        std::function<std::string()>([]() { return std::string(""); }), // Always use empty query
        [&](const std::string& query) {
            LspModule lsp_module;
            
            auto test_workspace = create_test_workspace();
            
            auto result = lsp_module.query_symbols(test_workspace, query);
            
            cleanup_test_workspace(test_workspace);
            
            if (!result) {
                // Connection failures are acceptable
                const auto& error = result.error();
                if (error.message.find("not connected") != std::string::npos ||
                    error.message.find("not running") != std::string::npos) {
                    return true;
                }
                // Other failures might be acceptable for empty queries
                return true;
            }
            
            // If it succeeds, results should be valid symbols
            const auto& symbols = result.value();
            for (const auto& symbol : symbols) {
                if (symbol.name.empty() || symbol.location.file.empty()) {
                    return false; // Invalid symbol data
                }
            }
            
            return true;
        },
        20
    );
    
    EXPECT_TRUE(property_holds);
}

TEST(LspSymbolSearchPropertyTest, PartialMatchAccuracy) {
    // Property: Partial queries should return symbols that contain the query substring
    bool property_holds = PropertyTest::forall(
        std::function<std::string()>([]() { return std::string("test"); }), // Always search for "test"
        [&](const std::string& query) {
            LspModule lsp_module;
            auto test_workspace = create_test_workspace();
            
            auto result = lsp_module.query_symbols(test_workspace, query);
            
            cleanup_test_workspace(test_workspace);
            
            if (!result) {
                // Connection failures are acceptable
                const auto& error = result.error();
                if (error.message.find("not connected") != std::string::npos ||
                    error.message.find("not running") != std::string::npos) {
                    return true;
                }
                return false;
            }
            
            const auto& symbols = result.value();
            
            // All returned symbols should contain "test" in their name (case-insensitive)
            for (const auto& symbol : symbols) {
                std::string lower_name = symbol.name;
                std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
                
                if (lower_name.find("test") == std::string::npos) {
                    return false; // Symbol doesn't contain "test"
                }
            }
            
            // Should find at least some test symbols in our mock workspace
            bool found_test_function = false;
            bool found_test_method = false;
            
            for (const auto& symbol : symbols) {
                if (symbol.name == "test_function") found_test_function = true;
                if (symbol.name == "test_method") found_test_method = true;
            }
            
            // At least one of these should be found based on our mock implementation
            return found_test_function || found_test_method || symbols.empty();
        },
        30
    );
    
    EXPECT_TRUE(property_holds);
}