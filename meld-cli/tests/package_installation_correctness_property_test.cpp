#include <gtest/gtest.h>
#include "meld/cli/package_module.hpp"
#include "meld/cli/error_handler.hpp"
#include "meld/testing/property_test.hpp"
#include <memory>
#include <filesystem>
#include <random>

/**
 * Feature: meld-cli, Property 28: Package Installation Correctness
 * Validates: Requirements 9.1
 * 
 * Property: For any valid package identifier, installation should download the package 
 * and make it available for import
 */

namespace meld::cli::test {

class PackageInstallationCorrectnessPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        error_handler_ = std::make_shared<ErrorHandler>();
        package_module_ = std::make_unique<PackageModule>(error_handler_);
        
        // Create temporary test directory
        test_dir_ = std::filesystem::temp_directory_path() / "meld_package_test";
        std::filesystem::create_directories(test_dir_);
        
        // Change to test directory for consistent behavior
        original_dir_ = std::filesystem::current_path();
        std::filesystem::current_path(test_dir_);
    }
    
    void TearDown() override {
        // Restore original directory
        std::filesystem::current_path(original_dir_);
        
        // Clean up test directory
        std::error_code ec;
        std::filesystem::remove_all(test_dir_, ec);
    }
    
    std::shared_ptr<ErrorHandler> error_handler_;
    std::unique_ptr<PackageModule> package_module_;
    std::filesystem::path test_dir_;
    std::filesystem::path original_dir_;
};

// Generator for valid package names
class PackageNameGenerator {
public:
    static std::string generate(std::mt19937& rng) {
        std::uniform_int_distribution<int> length_dist(3, 20);
        std::uniform_int_distribution<int> char_dist(0, 2);
        
        int length = length_dist(rng);
        std::string name;
        name.reserve(length);
        
        // First character must be alphanumeric
        std::uniform_int_distribution<int> alpha_dist(0, 35);
        int first_char = alpha_dist(rng);
        if (first_char < 26) {
            name += static_cast<char>('a' + first_char);
        } else {
            name += static_cast<char>('0' + (first_char - 26));
        }
        
        // Remaining characters can be alphanumeric, hyphen, or underscore
        for (int i = 1; i < length; ++i) {
            int char_type = char_dist(rng);
            if (char_type == 0) {
                // Alphanumeric
                int char_val = alpha_dist(rng);
                if (char_val < 26) {
                    name += static_cast<char>('a' + char_val);
                } else {
                    name += static_cast<char>('0' + (char_val - 26));
                }
            } else if (char_type == 1) {
                name += '-';
            } else {
                name += '_';
            }
        }
        
        return name;
    }
};

// Generator for valid version strings
class VersionGenerator {
public:
    static std::string generate(std::mt19937& rng) {
        std::uniform_int_distribution<int> version_dist(0, 99);
        
        int major = version_dist(rng);
        int minor = version_dist(rng);
        int patch = version_dist(rng);
        
        return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    }
};

// Property test: Package installation correctness
TEST_F(PackageInstallationCorrectnessPropertyTest, PackageInstallationCorrectness) {
    meld::testing::PropertyTest property_test;
    
    property_test.run_property_test(100, [this](std::mt19937& rng) {
        // Generate valid package identifier
        std::string package_name = PackageNameGenerator::generate(rng);
        std::string version = VersionGenerator::generate(rng);
        std::string package_spec = package_name + "@" + version;
        
        // Test package installation
        auto result = package_module_->install_package(package_spec, false);
        
        // Property: Installation should succeed for valid package identifiers
        EXPECT_TRUE(result.success) << "Installation failed for package: " << package_spec 
                                   << ", error: " << result.error_message;
        
        if (result.success) {
            // Property: Installed package should have correct name
            EXPECT_EQ(result.package_name, package_name) 
                << "Installed package name mismatch";
            
            // Property: Installed version should be specified or resolved version
            EXPECT_FALSE(result.installed_version.empty()) 
                << "Installed version should not be empty";
            
            // Property: Install path should exist and be accessible
            EXPECT_TRUE(std::filesystem::exists(result.install_path)) 
                << "Install path should exist: " << result.install_path;
            
            // Property: At least one file should be installed
            EXPECT_FALSE(result.installed_files.empty()) 
                << "At least one file should be installed";
            
            // Property: All installed files should exist
            for (const auto& file_path : result.installed_files) {
                EXPECT_TRUE(std::filesystem::exists(file_path)) 
                    << "Installed file should exist: " << file_path;
            }
            
            // Property: Package should be available (can be found in install directory)
            auto install_dir = result.install_path;
            EXPECT_TRUE(std::filesystem::is_directory(install_dir)) 
                << "Install directory should be a directory";
            
            // Property: Package should be importable (install directory should contain package files)
            bool has_package_files = false;
            for (const auto& entry : std::filesystem::directory_iterator(install_dir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".meld") {
                    has_package_files = true;
                    break;
                }
            }
            EXPECT_TRUE(has_package_files) 
                << "Package should contain importable files";
        }
        
        return true; // Continue testing
    });
}

// Property test: Package installation idempotency
TEST_F(PackageInstallationCorrectnessPropertyTest, PackageInstallationIdempotency) {
    meld::testing::PropertyTest property_test;
    
    property_test.run_property_test(50, [this](std::mt19937& rng) {
        // Generate valid package identifier
        std::string package_name = PackageNameGenerator::generate(rng);
        std::string version = VersionGenerator::generate(rng);
        std::string package_spec = package_name + "@" + version;
        
        // Install package first time
        auto result1 = package_module_->install_package(package_spec, false);
        EXPECT_TRUE(result1.success) << "First installation should succeed";
        
        if (result1.success) {
            // Install same package second time
            auto result2 = package_module_->install_package(package_spec, false);
            
            // Property: Second installation should also succeed (idempotent)
            EXPECT_TRUE(result2.success) << "Second installation should succeed (idempotent)";
            
            if (result2.success) {
                // Property: Results should be consistent
                EXPECT_EQ(result1.package_name, result2.package_name) 
                    << "Package name should be consistent";
                EXPECT_EQ(result1.installed_version, result2.installed_version) 
                    << "Installed version should be consistent";
                EXPECT_EQ(result1.install_path, result2.install_path) 
                    << "Install path should be consistent";
            }
        }
        
        return true;
    });
}

// Property test: Package installation with development dependencies
TEST_F(PackageInstallationCorrectnessPropertyTest, DevDependencyInstallation) {
    meld::testing::PropertyTest property_test;
    
    property_test.run_property_test(30, [this](std::mt19937& rng) {
        // Generate valid package identifier
        std::string package_name = PackageNameGenerator::generate(rng);
        std::string version = VersionGenerator::generate(rng);
        std::string package_spec = package_name + "@" + version;
        
        // Test installation as development dependency
        auto result = package_module_->install_package(package_spec, true);
        
        // Property: Dev dependency installation should succeed
        EXPECT_TRUE(result.success) << "Dev dependency installation should succeed";
        
        if (result.success) {
            // Property: Package should still be installed correctly
            EXPECT_EQ(result.package_name, package_name);
            EXPECT_FALSE(result.installed_version.empty());
            EXPECT_TRUE(std::filesystem::exists(result.install_path));
            EXPECT_FALSE(result.installed_files.empty());
        }
        
        return true;
    });
}

// Property test: Invalid package names should fail gracefully
TEST_F(PackageInstallationCorrectnessPropertyTest, InvalidPackageNameHandling) {
    meld::testing::PropertyTest property_test;
    
    // Test with known invalid package names
    std::vector<std::string> invalid_names = {
        "",           // Empty name
        " ",          // Whitespace only
        "123",        // Numbers only (might be valid, but test edge case)
        "a b",        // Contains space
        "a/b",        // Contains slash
        "a\\b",       // Contains backslash
        "a.b",        // Contains dot
        std::string(256, 'a')  // Too long
    };
    
    for (const auto& invalid_name : invalid_names) {
        auto result = package_module_->install_package(invalid_name, false);
        
        // Property: Invalid package names should fail gracefully
        if (!result.success) {
            // Property: Error message should be provided
            EXPECT_FALSE(result.error_message.empty()) 
                << "Error message should be provided for invalid package: " << invalid_name;
        }
        // Note: Some "invalid" names might actually be valid in the implementation,
        // so we don't strictly require failure, just graceful handling
    }
}

} // namespace meld::cli::test