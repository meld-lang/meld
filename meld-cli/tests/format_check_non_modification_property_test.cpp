#include <gtest/gtest.h>
#include <random>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <thread>
#include "meld/cli/dev_tools_module.hpp"

/**
 * **Feature: meld-cli, Property 24: Format Check Non-Modification**
 * **Validates: Requirements 8.2**
 * 
 * Property: For any source file, using the format check flag should never 
 * modify the file contents
 */

class FormatCheckNonModificationTest : public ::testing::Test {
protected:
    void SetUp() override {
        gen.seed(std::random_device{}());
        formatter = std::make_unique<meld::cli::CodeFormatter>();
        
        // Create temporary directory for test files
        temp_dir = std::filesystem::temp_directory_path() / "meld_format_test";
        std::filesystem::create_directories(temp_dir);
    }

    void TearDown() override {
        // Clean up temporary files
        if (std::filesystem::exists(temp_dir)) {
            std::filesystem::remove_all(temp_dir);
        }
    }

    std::mt19937 gen;
    std::unique_ptr<meld::cli::CodeFormatter> formatter;
    std::filesystem::path temp_dir;

    // Generator for various Meld code samples
    std::string generate_meld_code() {
        std::vector<std::string> code_samples = {
            // Well-formatted code
            "val x = 42\nval y = x + 1\nprint(y)",
            
            // Poorly formatted code with extra spaces
            "val   x    =   42\n  val y=x+1\n print(y)  ",
            
            // Code with inconsistent indentation
            "fnc calculate(a: int, b: int) -> int {\n  return a + b\n    }",
            
            // Code with trailing whitespace
            "class Point {  \n    val x: int  \n    val y: int  \n}  ",
            
            // Code with mixed line endings and spacing
            "if (condition) {\n    doSomething()\n} else {\n    doOther()\n}",
            
            // Code with long lines
            "val very_long_variable_name = some_function_with_very_long_name(parameter1, parameter2, parameter3, parameter4)",
            
            // Code with nested structures
            "match (value) {\n    case 1 -> {\n        print(\"one\")\n        doSomething()\n    }\n    case 2 -> print(\"two\")\n    default -> print(\"other\")\n}",
            
            // Empty file
            "",
            
            // File with only whitespace
            "   \n  \n\t\n   ",
            
            // Code with comments (if supported)
            "// This is a comment\nval x = 42 // Another comment\n/* Block comment */\nprint(x)"
        };
        
        std::uniform_int_distribution<> dist(0, code_samples.size() - 1);
        return code_samples[dist(gen)];
    }

    // Generate format options with check_only flag
    meld::cli::FormatOptions generate_check_options() {
        meld::cli::FormatOptions options;
        
        std::uniform_int_distribution<> bool_dist(0, 1);
        std::uniform_int_distribution<> indent_dist(2, 8);
        std::uniform_int_distribution<> length_dist(80, 120);
        
        options.check_only = true; // This is the key property we're testing
        options.recursive = bool_dist(gen);
        options.indent_size = indent_dist(gen);
        options.max_line_length = length_dist(gen);
        options.use_tabs = bool_dist(gen);
        options.preserve_newlines = bool_dist(gen);
        
        return options;
    }

    // Create a temporary file with given content
    std::filesystem::path create_temp_file(const std::string& content, const std::string& suffix = ".meld") {
        static int file_counter = 0;
        std::filesystem::path file_path = temp_dir / ("test_file_" + std::to_string(file_counter++) + suffix);
        
        std::ofstream file(file_path);
        file << content;
        file.close();
        
        return file_path;
    }

    // Read file content
    std::string read_file(const std::filesystem::path& file_path) {
        std::ifstream file(file_path);
        if (!file.is_open()) {
            return "";
        }
        
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        file.close();
        return content;
    }

    // Get file modification time
    std::filesystem::file_time_type get_file_time(const std::filesystem::path& file_path) {
        return std::filesystem::last_write_time(file_path);
    }
};

TEST_F(FormatCheckNonModificationTest, CheckModeNeverModifiesFiles) {
    // Property: Format check should never modify file contents
    
    for (int iteration = 0; iteration < 100; ++iteration) {
        // Generate test input
        std::string original_content = generate_meld_code();
        meld::cli::FormatOptions options = generate_check_options();
        
        // Create temporary file
        auto file_path = create_temp_file(original_content);
        
        // Record original state
        std::string content_before = read_file(file_path);
        auto time_before = get_file_time(file_path);
        
        // Add small delay to ensure time difference would be detectable
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        
        // Perform format check
        auto result = formatter->format_file(file_path, options);
        
        // Verify file was not modified
        std::string content_after = read_file(file_path);
        auto time_after = get_file_time(file_path);
        
        EXPECT_EQ(content_before, content_after)
            << "File content should not change in check mode for iteration " << iteration
            << "\nFile: " << file_path
            << "\nOriginal content: '" << content_before << "'"
            << "\nContent after check: '" << content_after << "'";
        
        EXPECT_EQ(time_before, time_after)
            << "File modification time should not change in check mode for iteration " << iteration
            << "\nFile: " << file_path;
        
        // The result should still indicate if formatting is needed
        if (result.success) {
            // We can check if needs_formatting is set correctly, but file should not be modified
            EXPECT_TRUE(result.formatted_code.empty() || !options.check_only || 
                       result.formatted_code == content_before)
                << "In check mode, formatted_code should be empty or unchanged for iteration " << iteration;
        }
    }
}

TEST_F(FormatCheckNonModificationTest, CheckModeVsFormatModeConsistency) {
    // Property: Check mode should report the same formatting needs as format mode would produce
    
    for (int iteration = 0; iteration < 100; ++iteration) {
        std::string original_content = generate_meld_code();
        
        // Create two identical files
        auto check_file = create_temp_file(original_content, "_check.meld");
        auto format_file = create_temp_file(original_content, "_format.meld");
        
        // Generate options for both modes
        auto check_options = generate_check_options();
        auto format_options = check_options;
        format_options.check_only = false;
        
        // Run check mode
        auto check_result = formatter->format_file(check_file, check_options);
        
        // Run format mode
        auto format_result = formatter->format_file(format_file, format_options);
        
        // Verify check file was not modified
        std::string check_content_after = read_file(check_file);
        EXPECT_EQ(original_content, check_content_after)
            << "Check mode should not modify file for iteration " << iteration;
        
        // Verify consistency between modes
        if (check_result.success && format_result.success) {
            EXPECT_EQ(check_result.needs_formatting, format_result.needs_formatting)
                << "Check mode and format mode should agree on formatting needs for iteration " << iteration
                << "\nOriginal content: '" << original_content << "'";
            
            // If formatting was needed, the format mode should have changed the file
            if (format_result.needs_formatting) {
                std::string format_content_after = read_file(format_file);
                EXPECT_NE(original_content, format_content_after)
                    << "Format mode should modify file when formatting is needed for iteration " << iteration;
            }
        }
    }
}

TEST_F(FormatCheckNonModificationTest, CheckModeWithDirectoryProcessing) {
    // Property: Check mode should not modify any files when processing directories
    
    for (int iteration = 0; iteration < 50; ++iteration) {
        // Create a directory with multiple files
        auto test_dir = temp_dir / ("test_dir_" + std::to_string(iteration));
        std::filesystem::create_directories(test_dir);
        
        std::vector<std::filesystem::path> created_files;
        std::vector<std::string> original_contents;
        std::vector<std::filesystem::file_time_type> original_times;
        
        // Create 3-5 files with different content
        std::uniform_int_distribution<> file_count_dist(3, 5);
        int file_count = file_count_dist(gen);
        
        for (int i = 0; i < file_count; ++i) {
            std::string content = generate_meld_code();
            auto file_path = test_dir / ("file_" + std::to_string(i) + ".meld");
            
            std::ofstream file(file_path);
            file << content;
            file.close();
            
            created_files.push_back(file_path);
            original_contents.push_back(content);
            original_times.push_back(get_file_time(file_path));
        }
        
        // Add small delay
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        
        // Run format check on directory
        auto options = generate_check_options();
        options.recursive = true;
        
        auto result = formatter->format_directory(test_dir, options);
        
        // Verify no files were modified
        for (size_t i = 0; i < created_files.size(); ++i) {
            std::string content_after = read_file(created_files[i]);
            auto time_after = get_file_time(created_files[i]);
            
            EXPECT_EQ(original_contents[i], content_after)
                << "File " << created_files[i] << " should not be modified in check mode for iteration " << iteration;
            
            EXPECT_EQ(original_times[i], time_after)
                << "File " << created_files[i] << " modification time should not change in check mode for iteration " << iteration;
        }
        
        // Verify result indicates correct number of files processed
        if (result.success) {
            EXPECT_EQ(result.files_processed, static_cast<size_t>(file_count))
                << "Should process correct number of files for iteration " << iteration;
            
            // In check mode, files_changed should always be 0
            EXPECT_EQ(result.files_changed, 0)
                << "No files should be reported as changed in check mode for iteration " << iteration;
        }
    }
}

TEST_F(FormatCheckNonModificationTest, CheckModeWithPermissionRestrictions) {
    // Property: Check mode should work even on read-only files
    
    for (int iteration = 0; iteration < 50; ++iteration) {
        std::string content = generate_meld_code();
        auto file_path = create_temp_file(content);
        
        // Make file read-only (platform-specific)
        std::filesystem::permissions(file_path, 
            std::filesystem::perms::owner_read | std::filesystem::perms::group_read | std::filesystem::perms::others_read,
            std::filesystem::perm_options::replace);
        
        auto options = generate_check_options();
        
        // Check mode should work on read-only files
        auto result = formatter->format_file(file_path, options);
        
        // Should succeed (or fail gracefully) but not modify the file
        std::string content_after = read_file(file_path);
        EXPECT_EQ(content, content_after)
            << "Read-only file should not be modified in check mode for iteration " << iteration;
        
        // Restore write permissions for cleanup
        std::filesystem::permissions(file_path, 
            std::filesystem::perms::owner_all | std::filesystem::perms::group_read | std::filesystem::perms::others_read,
            std::filesystem::perm_options::replace);
    }
}

TEST_F(FormatCheckNonModificationTest, CheckModeReportsAccurateStatus) {
    // Property: Check mode should accurately report whether formatting is needed
    
    for (int iteration = 0; iteration < 100; ++iteration) {
        std::string content = generate_meld_code();
        auto file_path = create_temp_file(content);
        
        auto options = generate_check_options();
        
        auto result = formatter->format_file(file_path, options);
        
        if (result.success) {
            // If check mode says formatting is needed, format mode should actually change the file
            if (result.needs_formatting) {
                auto format_options = options;
                format_options.check_only = false;
                
                auto format_result = formatter->format_file(file_path, format_options);
                
                if (format_result.success) {
                    std::string formatted_content = read_file(file_path);
                    EXPECT_NE(content, formatted_content)
                        << "If check mode reports formatting needed, format mode should change content for iteration " << iteration
                        << "\nOriginal: '" << content << "'"
                        << "\nFormatted: '" << formatted_content << "'";
                }
            }
        }
    }
}

// Run the property-based tests
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}