#include <gtest/gtest.h>
#include "meld/cli/interpreter_module.hpp"
#include <vector>
#include <string>
#include <set>
#include <random>
#include <algorithm>

namespace meld::cli::test {

/**
 * **Feature: meld-cli, Property 9: REPL Module Loading**
 * **Validates: Requirements 3.4**
 * 
 * Property: For any valid module import in the REPL, the module's exported symbols 
 * should become available for use in subsequent expressions
 */
class ReplModuleLoadingPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        session = std::make_unique<ReplSession>();
        
        // Set up random number generator
        std::random_device rd;
        gen.seed(rd());
    }
    
    void TearDown() override {
        session.reset();
    }
    
    std::unique_ptr<ReplSession> session;
    std::mt19937 gen;
    
    // Generate random valid module names
    std::string generate_module_name() {
        std::vector<std::string> valid_names = {
            "std", "math", "io", "string", "collections", "async", "json",
            "http", "fs", "crypto", "test", "debug", "util", "core"
        };
        
        std::uniform_int_distribution<> dis(0, valid_names.size() - 1);
        return valid_names[dis(gen)];
    }
    
    // Generate a list of unique module names
    std::vector<std::string> generate_module_list(size_t count) {
        std::set<std::string> unique_modules;
        
        while (unique_modules.size() < count) {
            unique_modules.insert(generate_module_name());
        }
        
        return std::vector<std::string>(unique_modules.begin(), unique_modules.end());
    }
    
    // Check if module is loaded by checking environment
    bool is_module_loaded(const std::string& module_name) {
        const auto& env = session->get_environment();
        std::string env_key = "LOADED_" + module_name;
        return env.find(env_key) != env.end();
    }
};

TEST_F(ReplModuleLoadingPropertyTest, SingleModuleLoadingProperty) {
    // Property: Loading any valid module should make it available in subsequent operations
    for (int i = 0; i < 100; ++i) {
        // Generate a random module name
        std::string module_name = generate_module_name();
        
        // Load the module
        bool load_result = session->load_module(module_name);
        
        // Property: Module loading should succeed for valid names
        EXPECT_TRUE(load_result) << "Failed to load module: " << module_name;
        
        // Property: Module should be marked as loaded in environment
        EXPECT_TRUE(is_module_loaded(module_name)) 
            << "Module not marked as loaded: " << module_name;
        
        // Property: Loading the same module again should still succeed (idempotent)
        bool reload_result = session->load_module(module_name);
        EXPECT_TRUE(reload_result) << "Failed to reload module: " << module_name;
        
        // Clear session for next iteration
        session->clear();
    }
}

TEST_F(ReplModuleLoadingPropertyTest, MultipleModuleLoadingProperty) {
    // Property: Loading multiple modules should make all of them available
    for (int i = 0; i < 50; ++i) {
        // Generate 2-5 unique module names
        std::uniform_int_distribution<> count_dis(2, 5);
        size_t module_count = count_dis(gen);
        
        std::vector<std::string> modules = generate_module_list(module_count);
        
        // Load all modules
        std::vector<bool> load_results;
        for (const auto& module : modules) {
            load_results.push_back(session->load_module(module));
        }
        
        // Property: All modules should load successfully
        for (size_t j = 0; j < modules.size(); ++j) {
            EXPECT_TRUE(load_results[j]) << "Failed to load module: " << modules[j];
        }
        
        // Property: All modules should be available after loading
        for (const auto& module : modules) {
            EXPECT_TRUE(is_module_loaded(module)) 
                << "Module not available after loading: " << module;
        }
        
        // Clear session for next iteration
        session->clear();
    }
}

TEST_F(ReplModuleLoadingPropertyTest, ModuleLoadingPersistenceProperty) {
    // Property: Loaded modules should remain available throughout the session
    for (int i = 0; i < 30; ++i) {
        std::vector<std::string> modules = generate_module_list(3);
        
        // Load modules one by one and verify persistence
        for (size_t j = 0; j < modules.size(); ++j) {
            // Load current module
            bool load_result = session->load_module(modules[j]);
            EXPECT_TRUE(load_result) << "Failed to load module: " << modules[j];
            
            // Property: All previously loaded modules should still be available
            for (size_t k = 0; k <= j; ++k) {
                EXPECT_TRUE(is_module_loaded(modules[k])) 
                    << "Previously loaded module no longer available: " << modules[k];
            }
        }
        
        // Property: After loading all modules, they should all still be available
        for (const auto& module : modules) {
            EXPECT_TRUE(is_module_loaded(module)) 
                << "Module not persistent: " << module;
        }
        
        // Clear session for next iteration
        session->clear();
    }
}

TEST_F(ReplModuleLoadingPropertyTest, ModuleLoadingIdempotencyProperty) {
    // Property: Loading the same module multiple times should be idempotent
    for (int i = 0; i < 50; ++i) {
        std::string module_name = generate_module_name();
        
        // Load the module multiple times
        std::uniform_int_distribution<> load_count_dis(2, 10);
        int load_count = load_count_dis(gen);
        
        std::vector<bool> load_results;
        for (int j = 0; j < load_count; ++j) {
            load_results.push_back(session->load_module(module_name));
        }
        
        // Property: All load attempts should succeed
        for (int j = 0; j < load_count; ++j) {
            EXPECT_TRUE(load_results[j]) 
                << "Load attempt " << (j + 1) << " failed for module: " << module_name;
        }
        
        // Property: Module should be loaded exactly once (idempotent)
        EXPECT_TRUE(is_module_loaded(module_name)) 
            << "Module not loaded after multiple attempts: " << module_name;
        
        // Clear session for next iteration
        session->clear();
    }
}

TEST_F(ReplModuleLoadingPropertyTest, ModuleAvailabilityAfterLoadingProperty) {
    // Property: After loading a module, its symbols should be available for completion
    for (int i = 0; i < 30; ++i) {
        std::string module_name = generate_module_name();
        
        // Get completions before loading
        std::vector<std::string> completions_before = session->get_completions("");
        size_t completions_count_before = completions_before.size();
        
        // Load the module
        bool load_result = session->load_module(module_name);
        EXPECT_TRUE(load_result) << "Failed to load module: " << module_name;
        
        // Get completions after loading
        std::vector<std::string> completions_after = session->get_completions("");
        size_t completions_count_after = completions_after.size();
        
        // Property: Loading a module should not decrease available completions
        EXPECT_GE(completions_count_after, completions_count_before) 
            << "Completions decreased after loading module: " << module_name;
        
        // Property: Module should be marked as loaded
        EXPECT_TRUE(is_module_loaded(module_name)) 
            << "Module not marked as loaded: " << module_name;
        
        // Clear session for next iteration
        session->clear();
    }
}

} // namespace meld::cli::test