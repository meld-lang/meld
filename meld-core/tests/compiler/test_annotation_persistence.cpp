#include <gtest/gtest.h>
#include "../../include/meld/compiler/annotation_persister.hpp"
#include "../../include/meld/compiler/compiler.hpp"
#include <fstream>
#include <filesystem>

using namespace meld::compiler;
using namespace meld::parser;
using namespace meld::parser;

class AnnotationPersistenceTest : public ::testing::Test {
protected:
    void SetUp() override {
        persister = std::make_unique<AnnotationPersister>();
        compiler = std::make_unique<Compiler>();
        
        // Create temporary directory for test files
        test_dir = std::filesystem::temp_directory_path() / "meld_annotation_test";
        std::filesystem::create_directories(test_dir);
    }
    
    void TearDown() override {
        // Clean up test files
        if (std::filesystem::exists(test_dir)) {
            std::filesystem::remove_all(test_dir);
        }
    }
    
    std::string create_test_file(const std::string& content) {
        static int file_counter = 0;
        std::string filename = "test_" + std::to_string(file_counter++) + ".meld";
        std::string filepath = test_dir / filename;
        
        std::ofstream file(filepath);
        file << content;
        file.close();
        
        return filepath;
    }
    
    std::string read_file(const std::string& filepath) {
        std::ifstream file(filepath);
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
    
    std::unique_ptr<AnnotationPersister> persister;
    std::unique_ptr<Compiler> compiler;
    std::filesystem::path test_dir;
};

// TASK 35.10: Test writing inferred @uses annotations to source file
// Requirement 41.9: Write inferred @uses annotations into the source code
TEST_F(AnnotationPersistenceTest, WriteAnnotationsToSource) {
    std::string source_code = R"(
fnc pure_function(x: int) -> int {
    rtn x + 1
}

fnc io_function(filename: string) -> string {
    val content = File.read(filename)
    rtn content
}

fnc network_function(url: string) -> string {
    val response = http.get(url)
    rtn response
}
)";
    
    // Create mock function definitions and inferred effects
    std::vector<ast::function_definition> functions;
    std::map<std::string, std::set<std::string>> inferred_effects;
    
    // Mock pure function
    ast::function_definition pure_func;
    pure_func.name.name = "pure_function";
    functions.push_back(pure_func);
    inferred_effects["pure_function"] = {}; // Pure function
    
    // Mock IO function
    ast::function_definition io_func;
    io_func.name.name = "io_function";
    functions.push_back(io_func);
    inferred_effects["io_function"] = {"EffectIO"};
    
    // Mock network function
    ast::function_definition network_func;
    network_func.name.name = "network_function";
    functions.push_back(network_func);
    inferred_effects["network_function"] = {"EffectNetwork"};
    
    // Write annotations to source
    auto result = persister->write_annotations_to_source(source_code, functions, inferred_effects);
    
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.annotations_added, 3);
    EXPECT_EQ(result.annotations_updated, 0);
    
    // Check that annotations were added
    EXPECT_TRUE(result.modified_source.find("@uses()") != std::string::npos); // Pure function
    EXPECT_TRUE(result.modified_source.find("@uses(EffectIO)") != std::string::npos);
    EXPECT_TRUE(result.modified_source.find("@uses(EffectNetwork)") != std::string::npos);
}

// TASK 35.10: Test updating annotations when implementation changes
// Requirement 41.10: Update @uses annotation on next save when function changes
TEST_F(AnnotationPersistenceTest, UpdateAnnotationsOnChange) {
    std::string source_code = R"(
@uses(EffectIO)
fnc changed_function(filename: string) -> string {
    val content = File.read(filename)
    val response = http.get("http://example.com")
    rtn content + response
}
)";
    
    // Create mock function with changed effects
    std::vector<ast::function_definition> functions;
    std::map<std::string, std::set<std::string>> inferred_effects;
    
    ast::function_definition func;
    func.name.name = "changed_function";
    functions.push_back(func);
    
    // Function now uses both IO and Network effects
    inferred_effects["changed_function"] = {"EffectIO", "EffectNetwork"};
    
    // Update annotations
    auto result = persister->update_annotations_in_source(source_code, functions, inferred_effects);
    
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.annotations_added, 0);
    EXPECT_EQ(result.annotations_updated, 1);
    
    // Check that annotation was updated to include both effects
    EXPECT_TRUE(result.modified_source.find("@uses(EffectIO, EffectNetwork)") != std::string::npos ||
                result.modified_source.find("@uses(EffectNetwork, EffectIO)") != std::string::npos);
}

// Test persisting annotations to file
TEST_F(AnnotationPersistenceTest, PersistAnnotationsToFile) {
    std::string source_code = R"(
fnc test_function(x: int) -> int {
    System.out.println("Hello")
    rtn x + 1
}
)";
    
    std::string filepath = create_test_file(source_code);
    
    // Create mock function with console effect
    std::vector<ast::function_definition> functions;
    std::map<std::string, std::set<std::string>> inferred_effects;
    
    ast::function_definition func;
    func.name.name = "test_function";
    functions.push_back(func);
    inferred_effects["test_function"] = {"EffectIO"}; // Console output is IO effect
    
    // Persist annotations to file
    bool success = persister->persist_annotations_to_file(filepath, functions, inferred_effects);
    
    EXPECT_TRUE(success);
    
    // Read file and check that annotation was added
    std::string modified_content = read_file(filepath);
    EXPECT_TRUE(modified_content.find("@uses(EffectIO)") != std::string::npos);
    
    // Check that backup file was created
    EXPECT_TRUE(std::filesystem::exists(filepath + ".bak"));
}

// Test checking if file needs annotation updates
TEST_F(AnnotationPersistenceTest, NeedsAnnotationUpdate) {
    std::string source_code_without_annotation = R"(
fnc test_function(filename: string) -> string {
    val content = File.read(filename)
    rtn content
}
)";
    
    std::string source_code_with_correct_annotation = R"(
@uses(EffectIO)
fnc test_function(filename: string) -> string {
    val content = File.read(filename)
    rtn content
}
)";
    
    std::string source_code_with_wrong_annotation = R"(
@uses(EffectNetwork)
fnc test_function(filename: string) -> string {
    val content = File.read(filename)
    rtn content
}
)";
    
    // Create mock function
    std::vector<ast::function_definition> functions;
    std::map<std::string, std::set<std::string>> inferred_effects;
    
    ast::function_definition func;
    func.name.name = "test_function";
    functions.push_back(func);
    inferred_effects["test_function"] = {"EffectIO"};
    
    // Test cases
    EXPECT_TRUE(persister->needs_annotation_update(source_code_without_annotation, functions, inferred_effects));
    EXPECT_FALSE(persister->needs_annotation_update(source_code_with_correct_annotation, functions, inferred_effects));
    EXPECT_TRUE(persister->needs_annotation_update(source_code_with_wrong_annotation, functions, inferred_effects));
}

// Test getting functions that need updates
TEST_F(AnnotationPersistenceTest, GetFunctionsNeedingUpdates) {
    std::string source_code = R"(
fnc correct_function(x: int) -> int {
    rtn x + 1
}

@uses(EffectIO)
fnc wrong_function(filename: string) -> string {
    val response = http.get("http://example.com")
    rtn response
}

fnc missing_annotation_function(filename: string) -> string {
    val content = File.read(filename)
    rtn content
}
)";
    
    // Create mock functions
    std::vector<ast::function_definition> functions;
    std::map<std::string, std::set<std::string>> inferred_effects;
    
    // Correct function (pure)
    ast::function_definition correct_func;
    correct_func.name.name = "correct_function";
    functions.push_back(correct_func);
    inferred_effects["correct_function"] = {};
    
    // Wrong function (should be Network, not IO)
    ast::function_definition wrong_func;
    wrong_func.name.name = "wrong_function";
    functions.push_back(wrong_func);
    inferred_effects["wrong_function"] = {"EffectNetwork"};
    
    // Missing annotation function
    ast::function_definition missing_func;
    missing_func.name.name = "missing_annotation_function";
    functions.push_back(missing_func);
    inferred_effects["missing_annotation_function"] = {"EffectIO"};
    
    // Get functions needing updates
    auto functions_needing_updates = persister->get_functions_needing_updates(source_code, functions, inferred_effects);
    
    EXPECT_EQ(functions_needing_updates.size(), 2);
    EXPECT_TRUE(std::find(functions_needing_updates.begin(), functions_needing_updates.end(), "wrong_function") != functions_needing_updates.end());
    EXPECT_TRUE(std::find(functions_needing_updates.begin(), functions_needing_updates.end(), "missing_annotation_function") != functions_needing_updates.end());
    EXPECT_TRUE(std::find(functions_needing_updates.begin(), functions_needing_updates.end(), "correct_function") == functions_needing_updates.end());
}

// Test annotation parsing
TEST_F(AnnotationPersistenceTest, AnnotationParsing) {
    // Test various annotation formats
    std::string single_effect = "@uses(EffectIO)";
    std::string multiple_effects = "@uses(EffectIO, EffectNetwork)";
    std::string empty_annotation = "@uses()";
    std::string spaced_annotation = "@uses( EffectIO , EffectNetwork )";
    
    // Note: These are internal methods, so we test through the public interface
    std::string source_with_annotations = R"(
@uses(EffectIO)
fnc single_effect_func() {}

@uses(EffectIO, EffectNetwork)
fnc multiple_effects_func() {}

@uses()
fnc pure_func() {}

@uses( EffectIO , EffectNetwork )
fnc spaced_func() {}
)";
    
    std::vector<ast::function_definition> functions;
    std::map<std::string, std::set<std::string>> inferred_effects;
    
    // Create mock functions with matching effects
    ast::function_definition func1;
    func1.name.name = "single_effect_func";
    functions.push_back(func1);
    inferred_effects["single_effect_func"] = {"EffectIO"};
    
    ast::function_definition func2;
    func2.name.name = "multiple_effects_func";
    functions.push_back(func2);
    inferred_effects["multiple_effects_func"] = {"EffectIO", "EffectNetwork"};
    
    ast::function_definition func3;
    func3.name.name = "pure_func";
    functions.push_back(func3);
    inferred_effects["pure_func"] = {};
    
    ast::function_definition func4;
    func4.name.name = "spaced_func";
    functions.push_back(func4);
    inferred_effects["spaced_func"] = {"EffectIO", "EffectNetwork"};
    
    // All annotations should be correctly parsed and match inferred effects
    EXPECT_FALSE(persister->needs_annotation_update(source_with_annotations, functions, inferred_effects));
}