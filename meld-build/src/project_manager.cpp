#include "project_manager.hpp"
#include <iostream>

namespace meld::build {

ProjectManager::ProjectManager() = default;

void ProjectManager::createProject(const std::string& name, const std::string& path) {
    std::cout << "Creating project '" << name << "' at path: " << path << std::endl;
    // Placeholder: Create project directory structure and files
}

void ProjectManager::loadProject(const std::string& path) {
    std::cout << "Loading project from path: " << path << std::endl;
    // Placeholder: Load project configuration and metadata
}

std::vector<std::string> ProjectManager::getSourceFiles() const {
    // Placeholder: Return list of source files in the project
    return {"src/main.meld", "src/lib.meld"};
}

void ProjectManager::addDependency(const std::string& dependency) {
    std::cout << "Adding dependency: " << dependency << std::endl;
    // Placeholder: Add dependency to project configuration
}

} // namespace meld::build