#include <gtest/gtest.h>
#include "../src/project_manager.hpp"

namespace meld::build {

class ProjectManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager = std::make_unique<ProjectManager>();
    }
    
    std::unique_ptr<ProjectManager> manager;
};

TEST_F(ProjectManagerTest, CreateProject) {
    // Test that project can be created without errors
    EXPECT_NO_THROW(manager->createProject("test-project", "/tmp/test"));
}

TEST_F(ProjectManagerTest, LoadProject) {
    // Test that project can be loaded without errors
    EXPECT_NO_THROW(manager->loadProject("/tmp/test"));
}

TEST_F(ProjectManagerTest, GetSourceFiles) {
    // Test that source files can be retrieved
    auto files = manager->getSourceFiles();
    EXPECT_FALSE(files.empty());
}

TEST_F(ProjectManagerTest, AddDependency) {
    // Test that dependencies can be added without errors
    EXPECT_NO_THROW(manager->addDependency("some-library"));
}

} // namespace meld::build