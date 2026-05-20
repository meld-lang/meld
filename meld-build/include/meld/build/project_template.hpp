#pragma once

#include <string>
#include <filesystem>

namespace meld {
namespace build {

/**
 * Options for project scaffolding
 */
struct ScaffoldOptions {
    bool is_lib = false;  // Create library project (module.meld) instead of binary (main.meld)
};

/**
 * Result of a scaffolding operation
 */
struct ScaffoldResult {
    bool success = false;
    std::string error_message;
};

/**
 * Generates new Meld project directory structures with meld.toml,
 * entry source files, and .gitignore.
 */
class ProjectTemplate {
public:
    /**
     * Create a new project in a new directory named `name` under `parent_dir`.
     * Generates: <parent_dir>/<name>/meld.toml, src/main.meld (or src/module.meld), .gitignore
     * Runs `git init` in the new directory.
     * Returns error if meld.toml already exists in the target directory.
     */
    ScaffoldResult create_project(const std::string& name,
                                  const ScaffoldOptions& options,
                                  const std::filesystem::path& parent_dir = std::filesystem::current_path());

    /**
     * Initialize a project in the current directory (no parent dir creation).
     * Generates: meld.toml, src/main.meld (or src/module.meld), .gitignore
     * Returns error if meld.toml already exists.
     */
    ScaffoldResult init_project(const ScaffoldOptions& options,
                                const std::filesystem::path& target_dir = std::filesystem::current_path());

private:
    /**
     * Shared scaffolding logic — writes meld.toml, src/ entry file, .gitignore
     * into the given project_dir.
     */
    ScaffoldResult scaffold(const std::string& project_name,
                            const ScaffoldOptions& options,
                            const std::filesystem::path& project_dir);

    std::string generate_meld_toml(const std::string& project_name) const;
    std::string generate_main_meld() const;
    std::string generate_module_meld() const;
    std::string generate_gitignore() const;

    bool run_git_init(const std::filesystem::path& dir) const;
};

} // namespace build
} // namespace meld
