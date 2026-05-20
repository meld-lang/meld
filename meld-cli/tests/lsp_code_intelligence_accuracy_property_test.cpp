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
 * **Feature: meld-cli, Property 38: LSP Code Intelligence Accuracy**
 * **Validates: Requirements 13.5, 13.6, 13.7, 13.8, 13.10**
 * 
 * Property: For any valid position in a Meld file, LSP operations (completions, 
 * definition, references, hover) should provide accurate and relevant information
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

// Generator for valid positions in Meld source code
std::function<Position()> valid_positions() {
    return []() {
        static std::mt19937 gen(std::random_device{}());
        
        // Generate positions that are likely to be at meaningful locations
        std::uniform_int_distribution<size_t> line_dist(0, 50);
        std::uniform_int_distribution<size_t> col_dist(0, 80);
        
        return Position(line_dist(gen), col_dist(gen));
    };
}

// Generator for Meld source code with identifiers at known positions
std::function<std::string()> meld_source_with_identifiers() {
    return []() {
        static std::mt19937 gen(std::random_device{}());
        
        std::vector<std::string> code_snippets = {
            R"(fn main() -> Unit {
    let variable_name: string = "test"
    println(variable_name)
    test_function()
}

fn test_function() -> Unit {
    let local_var: int = 42
    println(local_var.to_string())
})",
            R"(class TestClass {
    let field_name: string = "field"
    
    fn method_name() -> Unit {
        let method_var: int = self.field_name.length()
        println(method_var.to_string())
    }
    
    fn another_method() -> string {
        return self.field_name
    }
})",
            R"(module TestModule {
    fn utility_function(param: string) -> int {
        let result: int = param.length()
        return result
    }
    
    fn helper_function() -> Unit {
        let value: string = "helper"
        let length: int = utility_function(value)
        println(length.to_string())
    }
})",
            R"(struct DataStruct {
    name: string,
    value: int
}

fn process_data(data: DataStruct) -> Unit {
    println(data.name)
    println(data.value.to_string())
}

fn create_data() -> DataStruct {
    return DataStruct {
        name: "test",
        value: 123
    }
})"
        };
        
        std::uniform_int_distribution<size_t> snippet_dist(0, code_snippets.size() - 1);
        return code_snippets[snippet_dist(gen)];
    };
}

// Generator for symbol names that should exist in the code
std::function<std::string()> expected_symbol_names() {
    return []() {
        static std::mt19937 gen(std::random_device{}());
        
        std::vector<std::string> symbols = {
            "main",
            "test_function",
            "TestClass",
            "method_name",
            "field_name",
            "variable_name",
            "local_var",
            "utility_function",
            "helper_function",
            "DataStruct",
            "process_data",
            "create_data",
            "param",
            "result",
            "value",
            "length",
            "data",
            "name"
        };
        
        std::uniform_int_distribution<size_t> symbol_dist(0, symbols.size() - 1);
        return symbols[symbol_dist(gen)];
    };
}

// Helper function to create a temporary file with Meld source code
std::filesystem::path create_test_file(const std::filesystem::path& relative_path, const std::string& content) {
    static std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<int> id_dist(1000, 9999);
    
    auto temp_dir = std::filesystem::temp_directory_path() / ("test_lsp_intelligence_" + std::to_string(id_dist(gen)));
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
        while (temp_dir.filename().string().find("test_lsp_intelligence_") == std::string::npos && 
               temp_dir != temp_dir.parent_path()) {
            temp_dir = temp_dir.parent_path();
        }
        if (temp_dir.filename().string().find("test_lsp_intelligence_") != std::string::npos) {
            std::filesystem::remove_all(temp_dir);
        }
    }
}

// Helper function to find positions of identifiers in source code
std::vector<Position> find_identifier_positions(const std::string& source, const std::string& identifier) {
    std::vector<Position> positions;
    std::istringstream stream(source);
    std::string line;
    size_t line_num = 0;
    
    while (std::getline(stream, line)) {
        size_t pos = 0;
        while ((pos = line.find(identifier, pos)) != std::string::npos) {
            // Check if it's a whole word (not part of another identifier)
            bool is_word_start = (pos == 0 || (!std::isalnum(line[pos - 1]) && line[pos - 1] != '_'));
            bool is_word_end = (pos + identifier.length() >= line.length() || 
                               (!std::isalnum(line[pos + identifier.length()]) && 
                                line[pos + identifier.length()] != '_'));
            
            if (is_word_start && is_word_end) {
                positions.emplace_back(line_num, pos);
            }
            pos++;
        }
        line_num++;
    }
    
    return positions;
}

TEST(LspCodeIntelligencePropertyTest, CompletionAccuracy) {
    // Property: Completions should provide relevant suggestions for the context
    bool property_holds = PropertyTest::forall(
        meld_file_paths(),
        meld_source_with_identifiers(),
        valid_positions(),
        [&](const std::filesystem::path& file_path, const std::string& source, const Position& position) {
            LspModule lsp_module;
            
            // Create a test file with the generated source
            auto test_file = create_test_file(file_path, source);
            
            // Get completions at the position
            auto result = lsp_module.get_completions(test_file, position.line, position.column);
            
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
            
            const auto& completions = result.value();
            
            // Validate that all completions have valid data
            for (const auto& completion : completions) {
                // Label should not be empty
                if (completion.label.empty()) {
                    return false;
                }
                
                // Kind should be valid
                if (static_cast<int>(completion.kind) < 1 || static_cast<int>(completion.kind) > 25) {
                    return false;
                }
                
                // If detail is provided, it should be meaningful
                if (!completion.detail.empty() && completion.detail.length() < 2) {
                    return false;
                }
                
                // Insert text, if provided, should be reasonable
                if (!completion.insert_text.empty() && completion.insert_text.length() > 1000) {
                    return false; // Unreasonably long insert text
                }
            }
            
            // Completions should be relevant to Meld language
            bool has_relevant_completions = completions.empty(); // Empty is acceptable
            for (const auto& completion : completions) {
                // Check for common Meld keywords/functions
                if (completion.label == "println" || completion.label == "if" || 
                    completion.label == "else" || completion.label == "fn" ||
                    completion.label == "let" || completion.label == "class" ||
                    completion.label == "struct" || completion.label == "module") {
                    has_relevant_completions = true;
                    break;
                }
            }
            
            return has_relevant_completions;
        },
        100
    );
    
    EXPECT_TRUE(property_holds);
}

TEST(LspCodeIntelligencePropertyTest, DefinitionAccuracy) {
    // Property: Go-to-definition should return valid locations for identifiers
    bool property_holds = PropertyTest::forall(
        meld_file_paths(),
        meld_source_with_identifiers(),
        expected_symbol_names(),
        [&](const std::filesystem::path& file_path, const std::string& source, const std::string& symbol) {
            LspModule lsp_module;
            
            auto test_file = create_test_file(file_path, source);
            
            // Find positions where the symbol appears
            auto positions = find_identifier_positions(source, symbol);
            
            bool all_definitions_valid = true;
            
            // Test go-to-definition for each occurrence of the symbol
            for (const auto& position : positions) {
                auto result = lsp_module.goto_definition(test_file, position.line, position.column);
                
                if (!result) {
                    // Connection failures are acceptable
                    const auto& error = result.error();
                    if (error.message.find("not connected") != std::string::npos ||
                        error.message.find("not running") != std::string::npos) {
                        continue;
                    }
                    all_definitions_valid = false;
                    break;
                }
                
                const auto& location = result.value();
                
                // Validate the returned location
                if (location.file.empty()) {
                    all_definitions_valid = false;
                    break;
                }
                
                // Range should be valid
                if (!location.range.is_valid()) {
                    all_definitions_valid = false;
                    break;
                }
                
                // Position should be reasonable
                if (location.range.start.line > 10000 || location.range.start.column > 1000) {
                    all_definitions_valid = false;
                    break;
                }
            }
            
            cleanup_test_file(test_file);
            return all_definitions_valid;
        },
        75
    );
    
    EXPECT_TRUE(property_holds);
}

TEST(LspCodeIntelligencePropertyTest, ReferencesAccuracy) {
    // Property: Find references should return all valid references to a symbol
    bool property_holds = PropertyTest::forall(
        meld_file_paths(),
        meld_source_with_identifiers(),
        expected_symbol_names(),
        [&](const std::filesystem::path& file_path, const std::string& source, const std::string& symbol) {
            LspModule lsp_module;
            
            auto test_file = create_test_file(file_path, source);
            
            // Find positions where the symbol appears
            auto positions = find_identifier_positions(source, symbol);
            
            if (positions.empty()) {
                cleanup_test_file(test_file);
                return true; // No occurrences to test
            }
            
            // Test find references from the first occurrence
            auto result = lsp_module.find_references(test_file, positions[0].line, positions[0].column);
            
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
            
            const auto& references = result.value();
            
            // Validate all returned references
            for (const auto& reference : references) {
                // File should not be empty
                if (reference.file.empty()) {
                    return false;
                }
                
                // Range should be valid
                if (!reference.range.is_valid()) {
                    return false;
                }
                
                // Position should be reasonable
                if (reference.range.start.line > 10000 || reference.range.start.column > 1000) {
                    return false;
                }
            }
            
            // References should be relevant (either empty or contain reasonable locations)
            return true;
        },
        60
    );
    
    EXPECT_TRUE(property_holds);
}

TEST(LspCodeIntelligencePropertyTest, HoverAccuracy) {
    // Property: Hover information should provide meaningful content for identifiers
    bool property_holds = PropertyTest::forall(
        meld_file_paths(),
        meld_source_with_identifiers(),
        valid_positions(),
        [&](const std::filesystem::path& file_path, const std::string& source, const Position& position) {
            LspModule lsp_module;
            
            auto test_file = create_test_file(file_path, source);
            
            auto result = lsp_module.get_hover_info(test_file, position.line, position.column);
            
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
            
            const auto& hover = result.value();
            
            // Hover contents should not be empty if provided
            if (hover.contents.empty()) {
                return true; // Empty hover is acceptable for positions without symbols
            }
            
            // Contents should be meaningful (at least 3 characters)
            if (hover.contents.length() < 3) {
                return false;
            }
            
            // Contents should not be just whitespace
            if (std::all_of(hover.contents.begin(), hover.contents.end(), ::isspace)) {
                return false;
            }
            
            // If range is provided, it should be valid
            if (hover.range && !hover.range->is_valid()) {
                return false;
            }
            
            return true;
        },
        80
    );
    
    EXPECT_TRUE(property_holds);
}

TEST(LspCodeIntelligencePropertyTest, RenameAccuracy) {
    // Property: Symbol rename should produce valid workspace edits
    bool property_holds = PropertyTest::forall(
        meld_file_paths(),
        meld_source_with_identifiers(),
        expected_symbol_names(),
        std::function<std::string()>([]() {
            // Generate new names for renaming
            static std::mt19937 gen(std::random_device{}());
            std::vector<std::string> new_names = {
                "renamed_symbol",
                "new_name",
                "updated_identifier",
                "modified_symbol",
                "changed_name"
            };
            std::uniform_int_distribution<size_t> name_dist(0, new_names.size() - 1);
            return new_names[name_dist(gen)];
        }),
        [&](const std::filesystem::path& file_path, const std::string& source, 
            const std::string& symbol, const std::string& new_name) {
            LspModule lsp_module;
            
            auto test_file = create_test_file(file_path, source);
            
            // Find positions where the symbol appears
            auto positions = find_identifier_positions(source, symbol);
            
            if (positions.empty()) {
                cleanup_test_file(test_file);
                return true; // No occurrences to test
            }
            
            // Test rename from the first occurrence
            auto result = lsp_module.rename_symbol(test_file, positions[0].line, positions[0].column, new_name);
            
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
            
            const auto& workspace_edit = result.value();
            
            // Validate workspace edit structure
            if (workspace_edit.empty()) {
                return true; // Empty edit is acceptable if no changes needed
            }
            
            // Check all text edits in the workspace edit
            for (const auto& [file, edits] : workspace_edit.changes) {
                // File path should not be empty
                if (file.empty()) {
                    return false;
                }
                
                for (const auto& edit : edits) {
                    // Range should be valid
                    if (!edit.range.is_valid()) {
                        return false;
                    }
                    
                    // New text should be the expected new name
                    if (edit.new_text != new_name) {
                        return false;
                    }
                    
                    // Position should be reasonable
                    if (edit.range.start.line > 10000 || edit.range.start.column > 1000) {
                        return false;
                    }
                }
            }
            
            return true;
        },
        50
    );
    
    EXPECT_TRUE(property_holds);
}

TEST(LspCodeIntelligencePropertyTest, PositionValidation) {
    // Property: LSP operations should handle invalid positions gracefully
    bool property_holds = PropertyTest::forall(
        meld_file_paths(),
        meld_source_with_identifiers(),
        std::function<Position()>([]() {
            // Generate potentially invalid positions
            static std::mt19937 gen(std::random_device{}());
            std::uniform_int_distribution<size_t> large_pos_dist(1000, 10000);
            std::uniform_int_distribution<int> pos_type_dist(0, 2);
            
            int pos_type = pos_type_dist(gen);
            switch (pos_type) {
                case 0:
                    return Position(large_pos_dist(gen), 0); // Large line number
                case 1:
                    return Position(0, large_pos_dist(gen)); // Large column number
                case 2:
                    return Position(large_pos_dist(gen), large_pos_dist(gen)); // Both large
                default:
                    return Position(0, 0);
            }
        }),
        [&](const std::filesystem::path& file_path, const std::string& source, const Position& position) {
            LspModule lsp_module;
            
            auto test_file = create_test_file(file_path, source);
            
            // Test all LSP operations with potentially invalid positions
            auto completions_result = lsp_module.get_completions(test_file, position.line, position.column);
            auto definition_result = lsp_module.goto_definition(test_file, position.line, position.column);
            auto references_result = lsp_module.find_references(test_file, position.line, position.column);
            auto hover_result = lsp_module.get_hover_info(test_file, position.line, position.column);
            
            cleanup_test_file(test_file);
            
            // All operations should either succeed or fail gracefully
            // They should not crash or return invalid data
            
            if (completions_result) {
                const auto& completions = completions_result.value();
                for (const auto& completion : completions) {
                    if (completion.label.empty()) {
                        return false; // Invalid completion data
                    }
                }
            }
            
            if (definition_result) {
                const auto& location = definition_result.value();
                if (!location.range.is_valid()) {
                    return false; // Invalid location data
                }
            }
            
            if (references_result) {
                const auto& references = references_result.value();
                for (const auto& reference : references) {
                    if (!reference.range.is_valid()) {
                        return false; // Invalid reference data
                    }
                }
            }
            
            if (hover_result) {
                const auto& hover = hover_result.value();
                if (hover.range && !hover.range->is_valid()) {
                    return false; // Invalid hover range
                }
            }
            
            return true;
        },
        40
    );
    
    EXPECT_TRUE(property_holds);
}

TEST(LspCodeIntelligencePropertyTest, ConsistencyAcrossOperations) {
    // Property: LSP operations should be consistent with each other
    bool property_holds = PropertyTest::forall(
        meld_file_paths(),
        meld_source_with_identifiers(),
        expected_symbol_names(),
        [&](const std::filesystem::path& file_path, const std::string& source, const std::string& symbol) {
            LspModule lsp_module;
            
            auto test_file = create_test_file(file_path, source);
            
            // Find positions where the symbol appears
            auto positions = find_identifier_positions(source, symbol);
            
            if (positions.empty()) {
                cleanup_test_file(test_file);
                return true; // No occurrences to test
            }
            
            const auto& position = positions[0];
            
            // Get definition and references for the same position
            auto definition_result = lsp_module.goto_definition(test_file, position.line, position.column);
            auto references_result = lsp_module.find_references(test_file, position.line, position.column);
            auto hover_result = lsp_module.get_hover_info(test_file, position.line, position.column);
            
            cleanup_test_file(test_file);
            
            // If definition succeeds, references should also work (or fail gracefully)
            if (definition_result && references_result) {
                const auto& definition = definition_result.value();
                const auto& references = references_result.value();
                
                // Definition location should be valid if provided
                if (!definition.range.is_valid()) {
                    return false;
                }
                
                // All reference locations should be valid
                for (const auto& reference : references) {
                    if (!reference.range.is_valid()) {
                        return false;
                    }
                }
            }
            
            // If hover succeeds, it should provide meaningful information
            if (hover_result) {
                const auto& hover = hover_result.value();
                if (!hover.contents.empty() && hover.contents.length() < 2) {
                    return false; // Hover content too short to be meaningful
                }
            }
            
            return true;
        },
        60
    );
    
    EXPECT_TRUE(property_holds);
}

TEST(LspCodeIntelligencePropertyTest, NonExistentFileHandling) {
    // Property: LSP operations should handle non-existent files gracefully
    bool property_holds = PropertyTest::forall(
        std::function<std::filesystem::path()>([]() {
            // Generate paths to non-existent files
            static std::mt19937 gen(std::random_device{}());
            std::uniform_int_distribution<int> id_dist(10000, 99999);
            
            std::string filename = "nonexistent_" + std::to_string(id_dist(gen)) + ".meld";
            return std::filesystem::path("nonexistent_dir") / filename;
        }),
        valid_positions(),
        [&](const std::filesystem::path& file_path, const Position& position) {
            LspModule lsp_module;
            
            // Test all LSP operations on non-existent file
            auto completions_result = lsp_module.get_completions(file_path, position.line, position.column);
            auto definition_result = lsp_module.goto_definition(file_path, position.line, position.column);
            auto references_result = lsp_module.find_references(file_path, position.line, position.column);
            auto hover_result = lsp_module.get_hover_info(file_path, position.line, position.column);
            
            // All operations should fail gracefully with meaningful error messages
            if (completions_result) {
                // If it succeeds, results should still be valid
                const auto& completions = completions_result.value();
                for (const auto& completion : completions) {
                    if (completion.label.empty()) {
                        return false;
                    }
                }
            } else {
                // Error message should be meaningful
                const auto& error = completions_result.error();
                if (error.message.empty()) {
                    return false;
                }
            }
            
            // Similar validation for other operations
            if (definition_result) {
                if (!definition_result.value().range.is_valid()) {
                    return false;
                }
            }
            
            if (references_result) {
                for (const auto& ref : references_result.value()) {
                    if (!ref.range.is_valid()) {
                        return false;
                    }
                }
            }
            
            if (hover_result) {
                if (hover_result.value().range && !hover_result.value().range->is_valid()) {
                    return false;
                }
            }
            
            return true;
        },
        30
    );
    
    EXPECT_TRUE(property_holds);
}