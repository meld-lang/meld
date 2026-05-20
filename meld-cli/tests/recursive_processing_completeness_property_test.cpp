#include <gtest/gtest.h>
#include <random>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <set>
#include <algorithm>
#include "meld/cli/dev_tools_module.hpp"

/**
 * **Feature: meld-cli, Property 27: Recursive Processing Completeness**
 * **Validates: Requirements 8.6**
 * 
 * Property: For any directory tree containing Meld files, recursive operations 
 * should process all files while preserving directory structure
 */

class RecursiveProcessingCompletenessTest : public ::testing::Test {
protected:
    void SetUp() override {
        gen.seed(std::random_device{}());
        formatter = std::make_unique<meld::cli::CodeFormatter>();
        linter = std::make_unique<meld::cli::CodeLinter>();
        
        // Create temporary directory for test files
        temp_dir = std::filesystem::temp_directory_path() / "meld_recursive_test";
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
    std::unique_ptr<meld::cli::CodeLinter> linter;
    std::filesystem::path temp_dir;

    // Generate random Meld code
    std::string generate_meld_code() {
        std::vector<std::string> code_samples = {
            "val x = 42\nval y = x + 1\nprint(y)",
            "fnc add(a: int, b: int) -> int {\n    return a + b\n}",
            "class Point {\n    val x: int\n    val y: int\n}",
            "if (condition) {\n    doSomething()\n} else {\n    doOther()\n}",
            "for (i in 0..10) {\n    print(i)\n}",
            "val list = [1, 2, 3, 4, 5]",
            "match (value) {\n    case 1 -> print(\"one\")\n    default -> print(\"other\")\n}"
        };
        
        std::uniform_int_distribution<> dist(0, code_samples.size() - 1);
        return code_samples[dist(gen)];
    }

    // Create a directory tree with Meld files
    struct DirectoryTree {
        std::filesystem::path root;
        std::vector<std::filesystem::path> meld_files;
        std::vector<std::filesystem::path> non_meld_files;
        std::vector<std::filesystem::path> directories;
        int depth;
        int total_files;
    };

    DirectoryTree create_directory_tree(int max_depth = 3, int files_per_dir = 3) {
        DirectoryTree tree;
        static int tree_counter = 0;
        
        tree.root = temp_dir / ("tree_" + std::to_string(tree_counter++));
        tree.depth = max_depth;
        tree.total_files = 0;
        
        std::filesystem::create_directories(tree.root);
        tree.directories.push_back(tree.root);
        
        // Create files and subdirectories recursively
        create_tree_recursive(tree, tree.root, 0, max_depth, files_per_dir);
        
        return tree;
    }

    void create_tree_recursive(DirectoryTree& tree, const std::filesystem::path& current_dir, 
                              int current_depth, int max_depth, int files_per_dir) {
        std::uniform_int_distribution<> file_dist(files_per_dir - 1, files_per_dir + 1);
        std::uniform_int_distribution<> subdir_dist(0, 2);
        
        // Create Meld files in current directory
        int num_files = file_dist(gen);
        for (int i = 0; i < num_files; ++i) {
            std::string filename = "file_" + std::to_string(tree.total_files++) + ".meld";
            auto file_path = current_dir / filename;
            
            std::ofstream file(file_path);
            file << generate_meld_code();
            file.close();
            
            tree.meld_files.push_back(file_path);
        }
        
        // Occasionally create non-Meld files
        if (gen() % 3 == 0) {
            std::string filename = "readme_" + std::to_string(tree.total_files++) + ".txt";
            auto file_path = current_dir / filename;
            
            std::ofstream file(file_path);
            file << "This is a readme file";
            file.close();
            
            tree.non_meld_files.push_back(file_path);
        }
        
        // Create subdirectories if not at max depth
        if (current_depth < max_depth) {
            int num_subdirs = subdir_dist(gen);
            for (int i = 0; i < num_subdirs; ++i) {
                std::string dirname = "subdir_" + std::to_string(current_depth) + "_" + std::to_string(i);
                auto subdir_path = current_dir / dirname;
                
                std::filesystem::create_directories(subdir_path);
                tree.directories.push_back(subdir_path);
                
                // Recursively create files in subdirectory
                create_tree_recursive(tree, subdir_path, current_depth + 1, max_depth, files_per_dir);
            }
        }
    }

    // Collect all Meld files in a directory tree
    std::set<std::filesystem::path> collect_meld_files_manually(const std::filesystem::path& root, bool recursive) {
        std::set<std::filesystem::path> files;
        
        if (recursive) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
                if (entry.is_regular_file() && entry.path().extension() == ".meld") {
                    files.insert(entry.path());
                }
            }
        } else {
            for (const auto& entry : std::filesystem::directory_iterator(root)) {
                if (entry.is_regular_file() && entry.path().extension() == ".meld") {
                    files.insert(entry.path());
                }
            }
        }
        
        return files;
    }

    // Generate format options
    meld::cli::FormatOptions generate_format_options(bool recursive) {
        meld::cli::FormatOptions options;
        
        std::uniform_int_distribution<> bool_dist(0, 1);
        std::uniform_int_distribution<> indent_dist(2, 8);
        
        options.check_only = bool_dist(gen);
        options.recursive = recursive;
        options.indent_size = indent_dist(gen);
        options.max_line_length = 100;
        options.use_tabs = bool_dist(gen);
        options.preserve_newlines = bool_dist(gen);
        
        return options;
    }

    // Generate lint options
    meld::cli::LintOptions generate_lint_options(bool recursive) {
        meld::cli::LintOptions options;
        
        std::uniform_int_distribution<> bool_dist(0, 1);
        
        options.auto_fix = bool_dist(gen);
        options.recursive = recursive;
        options.min_severity = meld::cli::LintSeverity::Hint;
        
        return options;
    }
};

TEST_F(RecursiveProcessingCompletenessTest, RecursiveFormattingProcessesAllFiles) {
    // Property: Recursive formatting should process all Meld files in directory tree
    
    for (int iteration = 0; iteration < 50; ++iteration) {
        // Create directory tree
        auto tree = create_directory_tree(3, 2);
        
        // Get expected files
        auto expected_files = collect_meld_files_manually(tree.root, true);
        
        // Run recursive formatting
        auto options = generate_format_options(true);
        auto result = formatter->format_directory(tree.root, options);
        
        if (result.success) {
            // Property: Should process all Meld files
            EXPECT_EQ(result.files_processed, expected_files.size())
                << "Should process all Meld files in tree for iteration " << iteration
                << "\nExpected: " << expected_files.size()
                << "\nProcessed: " << result.files_processed
                << "\nTree root: " << tree.root;
            
            // Property: Should not process non-Meld files
            EXPECT_LE(result.files_processed, tree.meld_files.size())
                << "Should not process more files than Meld files exist for iteration " << iteration;
        }
    }
}

TEST_F(RecursiveProcessingCompletenessTest, NonRecursiveProcessingOnlyProcessesTopLevel) {
    // Property: Non-recursive processing should only process files in top-level directory
    
    for (int iteration = 0; iteration < 50; ++iteration) {
        // Create directory tree
        auto tree = create_directory_tree(3, 2);
        
        // Get expected top-level files
        auto expected_files = collect_meld_files_manually(tree.root, false);
        
        // Run non-recursive formatting
        auto options = generate_format_options(false);
        auto result = formatter->format_directory(tree.root, options);
        
        if (result.success) {
            // Property: Should only process top-level files
            EXPECT_EQ(result.files_processed, expected_files.size())
                << "Should only process top-level Meld files for iteration " << iteration
                << "\nExpected: " << expected_files.size()
                << "\nProcessed: " << result.files_processed;
            
            // Property: Should process fewer files than recursive mode
            auto all_files = collect_meld_files_manually(tree.root, true);
            if (all_files.size() > expected_files.size()) {
                EXPECT_LT(result.files_processed, all_files.size())
                    << "Non-recursive should process fewer files than total for iteration " << iteration;
            }
        }
    }
}

TEST_F(RecursiveProcessingCompletenessTest, RecursiveLintingProcessesAllFiles) {
    // Property: Recursive linting should process all Meld files in directory tree
    
    for (int iteration = 0; iteration < 50; ++iteration) {
        auto tree = create_directory_tree(3, 2);
        auto expected_files = collect_meld_files_manually(tree.root, true);
        
        auto options = generate_lint_options(true);
        auto result = linter->lint_directory(tree.root, options);
        
        if (result.success) {
            EXPECT_EQ(result.files_processed, expected_files.size())
                << "Recursive linting should process all Meld files for iteration " << iteration
                << "\nExpected: " << expected_files.size()
                << "\nProcessed: " << result.files_processed;
        }
    }
}

TEST_F(RecursiveProcessingCompletenessTest, DirectoryStructureIsPreserved) {
    // Property: Processing should not modify directory structure
    
    for (int iteration = 0; iteration < 50; ++iteration) {
        auto tree = create_directory_tree(3, 2);
        
        // Record original directory structure
        std::set<std::filesystem::path> original_dirs;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(tree.root)) {
            if (entry.is_directory()) {
                original_dirs.insert(entry.path());
            }
        }
        
        // Run recursive formatting
        auto options = generate_format_options(true);
        auto result = formatter->format_directory(tree.root, options);
        
        // Check directory structure after processing
        std::set<std::filesystem::path> after_dirs;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(tree.root)) {
            if (entry.is_directory()) {
                after_dirs.insert(entry.path());
            }
        }
        
        // Property: Directory structure should be unchanged
        EXPECT_EQ(original_dirs, after_dirs)
            << "Directory structure should be preserved for iteration " << iteration
            << "\nOriginal dirs: " << original_dirs.size()
            << "\nAfter dirs: " << after_dirs.size();
    }
}

TEST_F(RecursiveProcessingCompletenessTest, FilePathsArePreserved) {
    // Property: Processing should not move or rename files
    
    for (int iteration = 0; iteration < 50; ++iteration) {
        auto tree = create_directory_tree(3, 2);
        
        // Record original file paths
        std::set<std::filesystem::path> original_files = collect_meld_files_manually(tree.root, true);
        
        // Run recursive formatting
        auto options = generate_format_options(true);
        auto result = formatter->format_directory(tree.root, options);
        
        // Check file paths after processing
        std::set<std::filesystem::path> after_files = collect_meld_files_manually(tree.root, true);
        
        // Property: File paths should be unchanged
        EXPECT_EQ(original_files, after_files)
            << "File paths should be preserved for iteration " << iteration
            << "\nOriginal files: " << original_files.size()
            << "\nAfter files: " << after_files.size();
        
        // Property: All original files should still exist
        for (const auto& file : original_files) {
            EXPECT_TRUE(std::filesystem::exists(file))
                << "Original file should still exist: " << file
                << " for iteration " << iteration;
        }
    }
}

TEST_F(RecursiveProcessingCompletenessTest, NonMeldFilesAreIgnored) {
    // Property: Processing should ignore non-Meld files
    
    for (int iteration = 0; iteration < 50; ++iteration) {
        auto tree = create_directory_tree(3, 2);
        
        // Record original non-Meld files and their content
        std::map<std::filesystem::path, std::string> non_meld_content;
        for (const auto& file : tree.non_meld_files) {
            std::ifstream ifs(file);
            std::string content((std::istreambuf_iterator<char>(ifs)),
                               std::istreambuf_iterator<char>());
            non_meld_content[file] = content;
        }
        
        // Run recursive formatting
        auto options = generate_format_options(true);
        auto result = formatter->format_directory(tree.root, options);
        
        // Property: Non-Meld files should be unchanged
        for (const auto& [file, original_content] : non_meld_content) {
            if (std::filesystem::exists(file)) {
                std::ifstream ifs(file);
                std::string current_content((std::istreambuf_iterator<char>(ifs)),
                                           std::istreambuf_iterator<char>());
                
                EXPECT_EQ(original_content, current_content)
                    << "Non-Meld file should be unchanged: " << file
                    << " for iteration " << iteration;
            }
        }
    }
}

TEST_F(RecursiveProcessingCompletenessTest, EmptyDirectoriesAreHandled) {
    // Property: Processing should handle empty directories gracefully
    
    for (int iteration = 0; iteration < 50; ++iteration) {
        // Create tree with some empty directories
        auto tree = create_directory_tree(2, 1);
        
        // Create additional empty directories
        std::vector<std::filesystem::path> empty_dirs;
        for (int i = 0; i < 3; ++i) {
            auto empty_dir = tree.root / ("empty_" + std::to_string(i));
            std::filesystem::create_directories(empty_dir);
            empty_dirs.push_back(empty_dir);
        }
        
        // Run recursive formatting
        auto options = generate_format_options(true);
        auto result = formatter->format_directory(tree.root, options);
        
        // Property: Should succeed even with empty directories
        EXPECT_TRUE(result.success)
            << "Should handle empty directories gracefully for iteration " << iteration;
        
        // Property: Empty directories should still exist
        for (const auto& dir : empty_dirs) {
            EXPECT_TRUE(std::filesystem::exists(dir))
                << "Empty directory should still exist: " << dir
                << " for iteration " << iteration;
        }
    }
}

TEST_F(RecursiveProcessingCompletenessTest, DeepNestingIsHandled) {
    // Property: Processing should handle deeply nested directory structures
    
    for (int iteration = 0; iteration < 30; ++iteration) {
        // Create deeply nested tree
        std::uniform_int_distribution<> depth_dist(5, 10);
        int max_depth = depth_dist(gen);
        
        auto tree = create_directory_tree(max_depth, 1);
        auto expected_files = collect_meld_files_manually(tree.root, true);
        
        // Run recursive formatting
        auto options = generate_format_options(true);
        auto result = formatter->format_directory(tree.root, options);
        
        // Property: Should process all files regardless of depth
        if (result.success) {
            EXPECT_EQ(result.files_processed, expected_files.size())
                << "Should process all files at any depth for iteration " << iteration
                << "\nMax depth: " << max_depth
                << "\nExpected files: " << expected_files.size()
                << "\nProcessed: " << result.files_processed;
        }
    }
}

TEST_F(RecursiveProcessingCompletenessTest, ProcessingOrderIsConsistent) {
    // Property: Multiple runs on same directory should process files consistently
    
    for (int iteration = 0; iteration < 30; ++iteration) {
        auto tree = create_directory_tree(3, 2);
        
        auto options = generate_format_options(true);
        
        // First run
        auto result1 = formatter->format_directory(tree.root, options);
        
        // Second run
        auto result2 = formatter->format_directory(tree.root, options);
        
        // Property: Should process same number of files
        if (result1.success && result2.success) {
            EXPECT_EQ(result1.files_processed, result2.files_processed)
                << "Should process same number of files on repeated runs for iteration " << iteration
                << "\nFirst run: " << result1.files_processed
                << "\nSecond run: " << result2.files_processed;
        }
    }
}

TEST_F(RecursiveProcessingCompletenessTest, MixedRecursiveAndNonRecursive) {
    // Property: Recursive and non-recursive modes should be independent
    
    for (int iteration = 0; iteration < 50; ++iteration) {
        auto tree = create_directory_tree(3, 2);
        
        auto top_level_files = collect_meld_files_manually(tree.root, false);
        auto all_files = collect_meld_files_manually(tree.root, true);
        
        // Run non-recursive first
        auto non_recursive_options = generate_format_options(false);
        auto non_recursive_result = formatter->format_directory(tree.root, non_recursive_options);
        
        // Then run recursive
        auto recursive_options = generate_format_options(true);
        auto recursive_result = formatter->format_directory(tree.root, recursive_options);
        
        // Property: Recursive should process more or equal files
        if (non_recursive_result.success && recursive_result.success) {
            EXPECT_GE(recursive_result.files_processed, non_recursive_result.files_processed)
                << "Recursive should process at least as many files as non-recursive for iteration " << iteration
                << "\nNon-recursive: " << non_recursive_result.files_processed
                << "\nRecursive: " << recursive_result.files_processed;
            
            // Property: If there are subdirectories with files, recursive should process more
            if (all_files.size() > top_level_files.size()) {
                EXPECT_GT(recursive_result.files_processed, non_recursive_result.files_processed)
                    << "Recursive should process more files when subdirectories exist for iteration " << iteration;
            }
        }
    }
}

// Run the property-based tests
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}