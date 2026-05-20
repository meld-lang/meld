#include <gtest/gtest.h>
#include "../../include/meld/compiler/cap.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace meld::compiler::cap;

class CAPTest : public ::testing::Test {
protected:
    void SetUp() override {
        cap = std::make_unique<CompilerAgentProtocol>();
        
        // Create a temporary directory for test files
        test_dir = std::filesystem::temp_directory_path() / "cap_test";
        std::filesystem::create_directories(test_dir);
    }
    
    void TearDown() override {
        // Clean up test files
        if (std::filesystem::exists(test_dir)) {
            std::filesystem::remove_all(test_dir);
        }
    }
    
    void create_test_file(const std::string& filename, const std::string& content) {
        std::filesystem::path file_path = test_dir / filename;
        std::ofstream file(file_path);
        file << content;
        file.close();
    }
    
    std::unique_ptr<CompilerAgentProtocol> cap;
    std::filesystem::path test_dir;
};

// Test basic compilation success
TEST_F(CAPTest, CompileValidSource) {
    std::string valid_source = R"(
        val x = 42
        val y = "hello"
    )";
    
    auto result = cap->compile_source(valid_source, "test.meld");
    
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.file_path, "test.meld");
    EXPECT_GT(result.lines_of_code, 0);
    EXPECT_GT(result.compilation_time.count(), 0);
    EXPECT_FALSE(result.has_errors());
}

// Test compilation with type error
TEST_F(CAPTest, CompileWithTypeError) {
    std::string invalid_source = R"(
        val x = undefined_variable
    )";
    
    auto result = cap->compile_source(invalid_source, "test.meld");
    
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.has_errors());
    
    auto errors = result.get_errors();
    EXPECT_GT(errors.size(), 0);
    
    // Check error structure
    const auto& error = errors[0];
    EXPECT_EQ(error.severity, MessageSeverity::ERROR);
    EXPECT_FALSE(error.code.empty());
    EXPECT_FALSE(error.message.empty());
    EXPECT_EQ(error.location.file, "test.meld");
}

// Test fix suggestions generation
TEST_F(CAPTest, GenerateFixSuggestions) {
    cap->set_include_suggestions(true);
    cap->set_confidence_threshold(ConfidenceLevel::LOW);
    
    std::string source_with_error = R"(
        val x = undefined_variable
    )";
    
    auto result = cap->compile_source(source_with_error, "test.meld");
    
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.has_errors());
    
    auto errors = result.get_errors();
    EXPECT_GT(errors.size(), 0);
    
    const auto& error = errors[0];
    EXPECT_GT(error.suggestions.size(), 0);
    
    // Check suggestion structure
    const auto& suggestion = error.suggestions[0];
    EXPECT_FALSE(suggestion.description.empty());
    EXPECT_FALSE(suggestion.replacement_text.empty());
    EXPECT_GE(static_cast<int>(suggestion.confidence), static_cast<int>(ConfidenceLevel::LOW));
}

// Test batch compilation
TEST_F(CAPTest, BatchCompilation) {
    // Create test files
    create_test_file("file1.meld", "val x = 42");
    create_test_file("file2.meld", "val y = \"hello\"");
    create_test_file("file3.meld", "val z = undefined_var"); // This will have an error
    
    std::vector<std::filesystem::path> files = {
        test_dir / "file1.meld",
        test_dir / "file2.meld",
        test_dir / "file3.meld"
    };
    
    auto batch_result = cap->compile_batch(files);
    
    EXPECT_EQ(batch_result.total_files, 3);
    EXPECT_EQ(batch_result.successful_files, 2); // file1 and file2 should succeed
    EXPECT_GT(batch_result.total_errors, 0); // file3 should have errors
    EXPECT_FALSE(batch_result.overall_success); // Overall failure due to file3
    EXPECT_GT(batch_result.total_time.count(), 0);
    
    EXPECT_EQ(batch_result.results.size(), 3);
}

// Test incremental compilation
TEST_F(CAPTest, IncrementalCompilation) {
    // Create test files
    create_test_file("file1.meld", "val x = 42");
    create_test_file("file2.meld", "val y = \"hello\"");
    
    std::vector<IncrementalChange> changes = {
        IncrementalChange(IncrementalChange::ChangeType::FILE_MODIFIED, (test_dir / "file1.meld").string()),
        IncrementalChange(IncrementalChange::ChangeType::FILE_ADDED, (test_dir / "file2.meld").string())
    };
    
    auto batch_result = cap->compile_incremental(changes);
    
    EXPECT_EQ(batch_result.total_files, 2);
    EXPECT_EQ(batch_result.successful_files, 2);
    EXPECT_EQ(batch_result.total_errors, 0);
    EXPECT_TRUE(batch_result.overall_success);
}

// Test JSON serialization
TEST_F(CAPTest, JSONSerialization) {
    std::string source = R"(
        val x = 42
    )";
    
    auto result = cap->compile_source(source, "test.meld");
    
    // Test JSON conversion
    std::string json_str = CompilerAgentProtocol::to_json_string(result, true);
    EXPECT_FALSE(json_str.empty());
    
    // Parse JSON to verify structure
    auto json = nlohmann::json::parse(json_str);
    EXPECT_TRUE(json.contains("file_path"));
    EXPECT_TRUE(json.contains("success"));
    EXPECT_TRUE(json.contains("messages"));
    EXPECT_TRUE(json.contains("compilation_time_ms"));
    EXPECT_TRUE(json.contains("lines_of_code"));
    EXPECT_TRUE(json.contains("ast_node_count"));
    
    EXPECT_EQ(json["file_path"], "test.meld");
    EXPECT_EQ(json["success"], true);
}

// Test Location serialization
TEST_F(CAPTest, LocationSerialization) {
    Location loc("test.meld", 10, 5, 10, 15);
    
    auto json = loc.to_json();
    EXPECT_EQ(json["file"], "test.meld");
    EXPECT_EQ(json["line"], 10);
    EXPECT_EQ(json["column"], 5);
    EXPECT_EQ(json["end_line"], 10);
    EXPECT_EQ(json["end_column"], 15);
    
    // Test round-trip
    auto restored_loc = Location::from_json(json);
    EXPECT_EQ(restored_loc.file, loc.file);
    EXPECT_EQ(restored_loc.line, loc.line);
    EXPECT_EQ(restored_loc.column, loc.column);
    EXPECT_EQ(restored_loc.end_line, loc.end_line);
    EXPECT_EQ(restored_loc.end_column, loc.end_column);
}

// Test FixSuggestion serialization
TEST_F(CAPTest, FixSuggestionSerialization) {
    Location loc("test.meld", 5, 10, 5, 20);
    FixSuggestion suggestion(
        FixType::REPLACE_TEXT,
        "Replace with correct variable name",
        loc,
        "correct_variable",
        ConfidenceLevel::HIGH
    );
    
    auto json = suggestion.to_json();
    EXPECT_EQ(json["type"], "replace_text");
    EXPECT_EQ(json["description"], "Replace with correct variable name");
    EXPECT_EQ(json["replacement_text"], "correct_variable");
    EXPECT_EQ(json["confidence"], static_cast<int>(ConfidenceLevel::HIGH));
    
    // Test round-trip
    auto restored_suggestion = FixSuggestion::from_json(json);
    EXPECT_EQ(restored_suggestion.type, suggestion.type);
    EXPECT_EQ(restored_suggestion.description, suggestion.description);
    EXPECT_EQ(restored_suggestion.replacement_text, suggestion.replacement_text);
    EXPECT_EQ(restored_suggestion.confidence, suggestion.confidence);
}

// Test CompilationMessage serialization
TEST_F(CAPTest, CompilationMessageSerialization) {
    Location loc("test.meld", 3, 8, 3, 15);
    CompilationMessage message(
        MessageSeverity::ERROR,
        "E101",
        "Undefined variable 'unknown'",
        loc
    );
    message.context = "Variable must be declared before use";
    
    FixSuggestion suggestion(
        FixType::INSERT_TEXT,
        "Add variable declaration",
        loc,
        "val unknown = /* TODO */",
        ConfidenceLevel::MEDIUM
    );
    message.suggestions.push_back(suggestion);
    
    auto json = message.to_json();
    EXPECT_EQ(json["severity"], "error");
    EXPECT_EQ(json["code"], "E101");
    EXPECT_EQ(json["message"], "Undefined variable 'unknown'");
    EXPECT_EQ(json["context"], "Variable must be declared before use");
    EXPECT_TRUE(json.contains("suggestions"));
    EXPECT_EQ(json["suggestions"].size(), 1);
    
    // Test round-trip
    auto restored_message = CompilationMessage::from_json(json);
    EXPECT_EQ(restored_message.severity, message.severity);
    EXPECT_EQ(restored_message.code, message.code);
    EXPECT_EQ(restored_message.message, message.message);
    EXPECT_EQ(restored_message.context, message.context);
    EXPECT_EQ(restored_message.suggestions.size(), 1);
}

// Test confidence threshold filtering
TEST_F(CAPTest, ConfidenceThresholdFiltering) {
    cap->set_include_suggestions(true);
    cap->set_confidence_threshold(ConfidenceLevel::HIGH);
    cap->set_max_suggestions_per_error(5);
    
    std::string source_with_error = R"(
        val x = undefined_variable
    )";
    
    auto result = cap->compile_source(source_with_error, "test.meld");
    
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.has_errors());
    
    auto errors = result.get_errors();
    EXPECT_GT(errors.size(), 0);
    
    const auto& error = errors[0];
    // All suggestions should meet the confidence threshold
    for (const auto& suggestion : error.suggestions) {
        EXPECT_GE(static_cast<int>(suggestion.confidence), static_cast<int>(ConfidenceLevel::HIGH));
    }
}

// Test max suggestions per error limit
TEST_F(CAPTest, MaxSuggestionsLimit) {
    cap->set_include_suggestions(true);
    cap->set_confidence_threshold(ConfidenceLevel::LOW);
    cap->set_max_suggestions_per_error(2);
    
    std::string source_with_error = R"(
        val x = undefined_variable
    )";
    
    auto result = cap->compile_source(source_with_error, "test.meld");
    
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.has_errors());
    
    auto errors = result.get_errors();
    EXPECT_GT(errors.size(), 0);
    
    const auto& error = errors[0];
    EXPECT_LE(error.suggestions.size(), 2);
}

// Test file compilation
TEST_F(CAPTest, CompileFile) {
    create_test_file("valid.meld", "val x = 42\nval y = \"hello\"");
    
    auto result = cap->compile_file(test_dir / "valid.meld");
    
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.file_path, (test_dir / "valid.meld").string());
    EXPECT_EQ(result.lines_of_code, 2);
    EXPECT_FALSE(result.has_errors());
}

// Test file not found error
TEST_F(CAPTest, CompileNonExistentFile) {
    auto result = cap->compile_file(test_dir / "nonexistent.meld");
    
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.has_errors());
    
    auto errors = result.get_errors();
    EXPECT_GT(errors.size(), 0);
    EXPECT_EQ(errors[0].code, "E001"); // File read error
}

// Test utility functions
TEST_F(CAPTest, UtilityFunctions) {
    // Test confidence conversion
    auto confidence_json = confidence_to_json(ConfidenceLevel::HIGH);
    EXPECT_EQ(confidence_json, static_cast<int>(ConfidenceLevel::HIGH));
    
    auto confidence_back = confidence_from_json(confidence_json);
    EXPECT_EQ(confidence_back, ConfidenceLevel::HIGH);
    
    // Test severity conversion
    auto severity_json = severity_to_json(MessageSeverity::WARNING);
    EXPECT_EQ(severity_json, "warning");
    
    auto severity_back = severity_from_json(severity_json);
    EXPECT_EQ(severity_back, MessageSeverity::WARNING);
    
    // Test fix type conversion
    auto fix_type_json = fix_type_to_json(FixType::ADD_IMPORT);
    EXPECT_EQ(fix_type_json, "add_import");
    
    auto fix_type_back = fix_type_from_json(fix_type_json);
    EXPECT_EQ(fix_type_back, FixType::ADD_IMPORT);
}