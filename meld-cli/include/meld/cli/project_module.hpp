#pragma once

#include "command_handler.hpp"
#include "error_handler.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <filesystem>
#include <optional>

namespace meld::cli {

/**
 * Project template types
 */
enum class ProjectType {
    Library,
    Executable,
    Mixed
};

/**
 * Build system types
 */
enum class BuildSystem {
    Bazel,
    Native,
    Unknown
};

/**
 * Project template configuration
 */
struct ProjectTemplate {
    std::string name;
    ProjectType type;
    std::string description;
    std::vector<std::string> directories;
    std::map<std::string, std::string> files; // filename -> content template
    std::vector<std::string> dependencies;
};

/**
 * Target configuration for multi-target builds
 */
struct TargetConfig {
    std::string name;
    std::string type; // "jvm", "go", "cpp", "wasm"
    std::string output;
    std::map<std::string, std::string> settings; // target-specific settings like "standard: cpp17"
};

/**
 * Project configuration
 */
struct ProjectConfig {
    std::string name;
    std::string version = "0.1.0";
    ProjectType type;
    BuildSystem build_system;
    std::vector<std::string> dependencies;
    std::vector<TargetConfig> targets; // Multi-target configuration
    std::map<std::string, std::string> metadata;
};

/**
 * Build result information
 */
struct BuildResult {
    bool success;
    std::string output;
    std::vector<std::string> artifacts;
    double build_time_seconds;
};

/**
 * Test execution result
 */
struct TestResult {
    bool success;
    int total_tests;
    int passed_tests;
    int failed_tests;
    std::vector<std::string> failures;
    double execution_time_seconds;
};

/**
 * Dependency information
 */
struct Dependency {
    std::string name;
    std::string version;
    std::string source; // registry, git, local
    std::map<std::string, std::string> metadata;
};

/**
 * Project management module for scaffolding, building, and testing
 */
class ProjectModule : public BaseCommandHandler {
public:
    explicit ProjectModule(std::shared_ptr<ErrorHandler> error_handler);
    ~ProjectModule() = default;

    // CommandHandler interface
    CommandResult execute(const CommandArgs& args) override;
    std::string get_help() const override;
    std::string get_usage() const override;
    std::vector<std::string> get_completions(const std::string& partial) const override;
    bool validate_args(const CommandArgs& args, std::string& error_message) const override;

    // Project scaffolding
    bool create_project(const std::string& name, const ProjectTemplate& tmpl, const std::filesystem::path& target_dir = ".");
    bool scaffold_library_project(const std::string& name, const std::filesystem::path& target_dir = ".");
    bool scaffold_executable_project(const std::string& name, const std::filesystem::path& target_dir = ".");

    // Build file generation
    bool generate_bazel_build_file(const ProjectConfig& config, const std::filesystem::path& project_dir);
    bool generate_meld_yaml(const ProjectConfig& config, const std::filesystem::path& project_dir);
    bool generate_sample_source_files(const ProjectConfig& config, const std::filesystem::path& project_dir);
    bool generate_documentation(const ProjectConfig& config, const std::filesystem::path& project_dir);

    // Configuration parsing
    std::optional<ProjectConfig> parse_meld_yaml(const std::filesystem::path& yaml_file);
    std::optional<ProjectConfig> load_project_config(const std::filesystem::path& project_dir);

    // Build system detection and integration
    BuildSystem detect_build_system(const std::filesystem::path& project_dir);
    BuildResult build_project(const std::filesystem::path& project_dir, const std::vector<std::string>& options = {});
    BuildResult build_with_bazel(const std::filesystem::path& project_dir, const std::vector<std::string>& options);
    BuildResult build_with_native(const std::filesystem::path& project_dir, const std::vector<std::string>& options);

    // Test execution
    TestResult run_tests(const std::filesystem::path& project_dir, const std::vector<std::string>& options = {});
    TestResult run_bazel_tests(const std::filesystem::path& project_dir, const std::vector<std::string>& options);
    TestResult run_native_tests(const std::filesystem::path& project_dir, const std::vector<std::string>& options);

    // Clean operations
    bool clean_project(const std::filesystem::path& project_dir);
    bool clean_bazel_artifacts(const std::filesystem::path& project_dir);
    bool clean_native_artifacts(const std::filesystem::path& project_dir);

    // Dependency management
    bool fetch_dependencies(const std::filesystem::path& project_dir);
    bool resolve_dependencies(const std::vector<Dependency>& dependencies, std::vector<Dependency>& resolved);
    bool install_dependency(const Dependency& dep, const std::filesystem::path& project_dir);

    // Template management
    void register_template(const ProjectTemplate& tmpl);
    std::vector<ProjectTemplate> get_available_templates() const;
    ProjectTemplate get_template(const std::string& name) const;

private:
    std::shared_ptr<ErrorHandler> error_handler_;
    std::map<std::string, ProjectTemplate> templates_;

    // Helper methods
    CommandResult handle_new_command(const CommandArgs& args);
    CommandResult handle_build_command(const CommandArgs& args);
    CommandResult handle_test_command(const CommandArgs& args);
    CommandResult handle_clean_command(const CommandArgs& args);
    CommandResult handle_deps_command(const CommandArgs& args);

    bool create_directory_structure(const std::vector<std::string>& directories, const std::filesystem::path& base_dir);
    bool write_template_files(const std::map<std::string, std::string>& files, const std::filesystem::path& base_dir, const ProjectConfig& config);
    std::string expand_template(const std::string& template_content, const ProjectConfig& config);
    bool file_exists(const std::filesystem::path& path);
    bool directory_exists(const std::filesystem::path& path);
    std::string execute_command(const std::string& command, const std::filesystem::path& working_dir = ".");
    
    void initialize_default_templates();
    ProjectTemplate create_library_template();
    ProjectTemplate create_executable_template();
};

} // namespace meld::cli