#pragma once

#include <string>
#include <vector>

namespace meld::build {

/**
 * Project management functionality for Meld build tool.
 * This is a minimal stub implementation.
 */
class ProjectManager {
public:
    ProjectManager();
    
    void createProject(const std::string& name, const std::string& path);
    void loadProject(const std::string& path);
    
    std::vector<std::string> getSourceFiles() const;
    void addDependency(const std::string& dependency);
    
private:
    // Placeholder: Project configuration and state
    std::string projectName_;
    std::string projectPath_;
    std::vector<std::string> dependencies_;
};

} // namespace meld::build