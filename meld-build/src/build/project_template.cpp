#include "meld/build/project_template.hpp"
#include <fstream>
#include <iostream>
#include <cstdlib>

namespace meld {
namespace build {

ScaffoldResult ProjectTemplate::create_project(const std::string& name,
                                               const ScaffoldOptions& options,
                                               const std::filesystem::path& parent_dir) {
    std::filesystem::path project_dir = parent_dir / name;

    // Create the project directory
    try {
        std::filesystem::create_directories(project_dir);
    } catch (const std::exception& e) {
        return {false, "Failed to create directory '" + project_dir.string() + "': " + e.what()};
    }

    auto result = scaffold(name, options, project_dir);
    if (!result.success) {
        return result;
    }

    // Run git init in the new project directory
    if (!run_git_init(project_dir)) {
        // Non-fatal — project is still usable without git
        std::cerr << "warning: failed to initialize git repository" << std::endl;
    }

    return {true, ""};
}

ScaffoldResult ProjectTemplate::init_project(const ScaffoldOptions& options,
                                             const std::filesystem::path& target_dir) {
    // Derive project name from the directory name
    std::string project_name = target_dir.filename().string();
    if (project_name.empty()) {
        project_name = "meld-project";
    }

    return scaffold(project_name, options, target_dir);
}

ScaffoldResult ProjectTemplate::scaffold(const std::string& project_name,
                                         const ScaffoldOptions& options,
                                         const std::filesystem::path& project_dir) {
    // Check for existing meld.toml — never overwrite
    std::filesystem::path toml_path = project_dir / "meld.toml";
    if (std::filesystem::exists(toml_path)) {
        return {false, "error: meld.toml already exists in '" + project_dir.string() + "'"};
    }

    // Create src/ directory
    std::filesystem::path src_dir = project_dir / "src";
    try {
        std::filesystem::create_directories(src_dir);
    } catch (const std::exception& e) {
        return {false, "Failed to create src/ directory: " + std::string(e.what())};
    }

    // Write meld.toml
    {
        std::ofstream file(toml_path);
        if (!file) {
            return {false, "Failed to write meld.toml"};
        }
        file << generate_meld_toml(project_name);
    }

    // Write entry source file
    if (options.is_lib) {
        std::filesystem::path entry = src_dir / "module.meld";
        std::ofstream file(entry);
        if (!file) {
            return {false, "Failed to write src/module.meld"};
        }
        file << generate_module_meld();
    } else {
        std::filesystem::path entry = src_dir / "main.meld";
        std::ofstream file(entry);
        if (!file) {
            return {false, "Failed to write src/main.meld"};
        }
        file << generate_main_meld();
    }

    // Write .gitignore
    {
        std::filesystem::path gitignore = project_dir / ".gitignore";
        std::ofstream file(gitignore);
        if (!file) {
            return {false, "Failed to write .gitignore"};
        }
        file << generate_gitignore();
    }

    return {true, ""};
}

std::string ProjectTemplate::generate_meld_toml(const std::string& project_name) const {
    return "[package]\nname = \"" + project_name + "\"\nversion = \"0.1.0\"\n\n[dependencies]\n\n[targets]\n";
}

std::string ProjectTemplate::generate_main_meld() const {
    return "// Entry point for the Meld application\n\nfnc main() {\n    print(\"Hello, Meld!\")\n}\n";
}

std::string ProjectTemplate::generate_module_meld() const {
    return "// Library module\n\nfnc greet(name: String) -> String {\n    \"Hello, \" + name + \"!\"\n}\n";
}

std::string ProjectTemplate::generate_gitignore() const {
    return "# Build artifacts\nbuild/\n*.meldc\n\n# Dependencies\n.meld/\nmeld.lock\n\n# IDE\n.vscode/\n.idea/\n";
}

bool ProjectTemplate::run_git_init(const std::filesystem::path& dir) const {
    std::string command = "git init \"" + dir.string() + "\" > /dev/null 2>&1";
    int result = std::system(command.c_str());
    return result == 0;
}

} // namespace build
} // namespace meld
