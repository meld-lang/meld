#include "meld/cli/project_module.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <cstdlib>
#include <chrono>
#include <optional>

namespace meld::cli {

ProjectModule::ProjectModule(std::shared_ptr<ErrorHandler> error_handler)
    : BaseCommandHandler("project", "Project management commands (new, build, test, clean)")
    , error_handler_(error_handler) {
    initialize_default_templates();
}

CommandResult ProjectModule::execute(const CommandArgs& args) {
    if (args.subcommand.empty()) {
        std::cout << get_help() << std::endl;
        return CommandResult::InvalidArguments;
    }

    if (args.subcommand == "new") {
        return handle_new_command(args);
    } else if (args.subcommand == "build") {
        return handle_build_command(args);
    } else if (args.subcommand == "test") {
        return handle_test_command(args);
    } else if (args.subcommand == "clean") {
        return handle_clean_command(args);
    } else if (args.subcommand == "deps") {
        return handle_deps_command(args);
    } else {
        std::cerr << "Unknown subcommand: " << args.subcommand << std::endl;
        std::cout << get_help() << std::endl;
        return CommandResult::InvalidArguments;
    }
}

std::string ProjectModule::get_help() const {
    return R"(Project management commands:

USAGE:
    meld project <SUBCOMMAND> [OPTIONS]

SUBCOMMANDS:
    new <name>      Create a new project
    build           Build the current project
    test            Run tests for the current project
    clean           Clean build artifacts
    deps            Manage project dependencies

OPTIONS for 'new':
    --template <name>   Use specific project template (lib, bin)
    --lib               Create a library project
    --bin               Create an executable project

OPTIONS for 'build':
    --target <name>     Build specific target
    --release           Build in release mode

OPTIONS for 'test':
    --filter <pattern>  Run tests matching pattern
    --verbose           Show detailed test output

EXAMPLES:
    meld project new myapp --bin
    meld project new mylib --lib
    meld project build --release
    meld project test --verbose
    meld project clean)";
}

std::string ProjectModule::get_usage() const {
    return "meld project <new|build|test|clean|deps> [OPTIONS]";
}

std::vector<std::string> ProjectModule::get_completions(const std::string& partial) const {
    std::vector<std::string> completions = {"new", "build", "test", "clean", "deps"};
    std::vector<std::string> result;
    
    for (const auto& cmd : completions) {
        if (cmd.find(partial) == 0) {
            result.push_back(cmd);
        }
    }
    
    return result;
}

bool ProjectModule::validate_args(const CommandArgs& args, std::string& error_message) const {
    if (args.subcommand.empty()) {
        error_message = "Subcommand required";
        return false;
    }
    
    std::vector<std::string> valid_subcommands = {"new", "build", "test", "clean", "deps"};
    if (std::find(valid_subcommands.begin(), valid_subcommands.end(), args.subcommand) == valid_subcommands.end()) {
        error_message = "Invalid subcommand: " + args.subcommand;
        return false;
    }
    
    if (args.subcommand == "new" && args.positional.empty()) {
        error_message = "Project name required for 'new' command";
        return false;
    }
    
    return true;
}

CommandResult ProjectModule::handle_new_command(const CommandArgs& args) {
    if (args.positional.empty()) {
        std::cerr << "Error: Project name required" << std::endl;
        return CommandResult::InvalidArguments;
    }
    
    std::string project_name = args.positional[0];
    std::string template_name = "lib"; // default
    
    // Check for template specification
    if (args.options.count("template")) {
        template_name = args.options.at("template");
    } else if (args.flags.count("lib")) {
        template_name = "lib";
    } else if (args.flags.count("bin")) {
        template_name = "bin";
    }
    
    try {
        ProjectTemplate tmpl = get_template(template_name);
        bool success = create_project(project_name, tmpl);
        
        if (success) {
            std::cout << "Successfully created project '" << project_name << "' using template '" << template_name << "'" << std::endl;
            return CommandResult::Success;
        } else {
            std::cerr << "Failed to create project '" << project_name << "'" << std::endl;
            return CommandResult::Error;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error creating project: " << e.what() << std::endl;
        return CommandResult::Error;
    }
}

CommandResult ProjectModule::handle_build_command(const CommandArgs& args) {
    std::filesystem::path project_dir = std::filesystem::current_path();
    std::vector<std::string> build_options;
    
    // Extract build options
    if (args.flags.count("release")) {
        build_options.push_back("--config=opt");
    }
    
    if (args.options.count("target")) {
        build_options.push_back("--target=" + args.options.at("target"));
    }
    
    try {
        BuildResult result = build_project(project_dir, build_options);
        
        if (result.success) {
            std::cout << "Build completed successfully in " << result.build_time_seconds << " seconds" << std::endl;
            if (!result.artifacts.empty()) {
                std::cout << "Generated artifacts:" << std::endl;
                for (const auto& artifact : result.artifacts) {
                    std::cout << "  " << artifact << std::endl;
                }
            }
            return CommandResult::Success;
        } else {
            std::cerr << "Build failed:" << std::endl;
            std::cerr << result.output << std::endl;
            return CommandResult::Error;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error during build: " << e.what() << std::endl;
        return CommandResult::Error;
    }
}

CommandResult ProjectModule::handle_test_command(const CommandArgs& args) {
    std::filesystem::path project_dir = std::filesystem::current_path();
    std::vector<std::string> test_options;
    
    // Extract test options
    if (args.flags.count("verbose")) {
        test_options.push_back("--verbose");
    }
    
    if (args.options.count("filter")) {
        test_options.push_back("--filter=" + args.options.at("filter"));
    }
    
    try {
        TestResult result = run_tests(project_dir, test_options);
        
        std::cout << "Test execution completed in " << result.execution_time_seconds << " seconds" << std::endl;
        std::cout << "Tests: " << result.total_tests << " total, " 
                  << result.passed_tests << " passed, " 
                  << result.failed_tests << " failed" << std::endl;
        
        if (!result.failures.empty()) {
            std::cout << "Failures:" << std::endl;
            for (const auto& failure : result.failures) {
                std::cout << "  " << failure << std::endl;
            }
        }
        
        return result.success ? CommandResult::Success : CommandResult::Error;
    } catch (const std::exception& e) {
        std::cerr << "Error during test execution: " << e.what() << std::endl;
        return CommandResult::Error;
    }
}

CommandResult ProjectModule::handle_clean_command(const CommandArgs& args) {
    std::filesystem::path project_dir = std::filesystem::current_path();
    
    try {
        bool success = clean_project(project_dir);
        
        if (success) {
            std::cout << "Clean completed successfully" << std::endl;
            return CommandResult::Success;
        } else {
            std::cerr << "Clean operation failed" << std::endl;
            return CommandResult::Error;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error during clean: " << e.what() << std::endl;
        return CommandResult::Error;
    }
}

CommandResult ProjectModule::handle_deps_command(const CommandArgs& args) {
    std::filesystem::path project_dir = std::filesystem::current_path();
    
    try {
        bool success = fetch_dependencies(project_dir);
        
        if (success) {
            std::cout << "Dependencies fetched successfully" << std::endl;
            return CommandResult::Success;
        } else {
            std::cerr << "Failed to fetch dependencies" << std::endl;
            return CommandResult::Error;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error fetching dependencies: " << e.what() << std::endl;
        return CommandResult::Error;
    }
}

bool ProjectModule::create_project(const std::string& name, const ProjectTemplate& tmpl, const std::filesystem::path& target_dir) {
    std::filesystem::path project_dir = target_dir / name;
    
    // Check if project directory already exists
    if (directory_exists(project_dir)) {
        std::cerr << "Error: Directory '" << project_dir << "' already exists" << std::endl;
        return false;
    }
    
    try {
        // Create project directory
        std::filesystem::create_directories(project_dir);
        
        // Create directory structure
        if (!create_directory_structure(tmpl.directories, project_dir)) {
            return false;
        }
        
        // Create project configuration
        ProjectConfig config;
        config.name = name;
        config.type = tmpl.type;
        config.build_system = BuildSystem::Bazel; // Default to Bazel
        config.dependencies = tmpl.dependencies;
        
        // Generate build files
        if (!generate_bazel_build_file(config, project_dir)) {
            return false;
        }
        
        if (!generate_meld_yaml(config, project_dir)) {
            return false;
        }
        
        // Write template files
        if (!write_template_files(tmpl.files, project_dir, config)) {
            return false;
        }
        
        // Generate sample source files
        if (!generate_sample_source_files(config, project_dir)) {
            return false;
        }
        
        // Generate documentation
        if (!generate_documentation(config, project_dir)) {
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error creating project: " << e.what() << std::endl;
        return false;
    }
}

bool ProjectModule::scaffold_library_project(const std::string& name, const std::filesystem::path& target_dir) {
    ProjectTemplate lib_template = get_template("lib");
    return create_project(name, lib_template, target_dir);
}

bool ProjectModule::scaffold_executable_project(const std::string& name, const std::filesystem::path& target_dir) {
    ProjectTemplate bin_template = get_template("bin");
    return create_project(name, bin_template, target_dir);
}

bool ProjectModule::generate_bazel_build_file(const ProjectConfig& config, const std::filesystem::path& project_dir) {
    std::filesystem::path build_file = project_dir / "BUILD.bazel";
    
    std::ostringstream content;
    content << "load(\"@rules_cc//cc:defs.bzl\", \"cc_library\", \"cc_binary\", \"cc_test\")\n\n";
    
    if (config.type == ProjectType::Library || config.type == ProjectType::Mixed) {
        content << "cc_library(\n";
        content << "    name = \"" << config.name << "\",\n";
        content << "    srcs = glob([\"src/**/*.cpp\"]),\n";
        content << "    hdrs = glob([\"include/**/*.hpp\"]),\n";
        content << "    includes = [\"include\"],\n";
        content << "    visibility = [\"//visibility:public\"],\n";
        content << ")\n\n";
    }
    
    if (config.type == ProjectType::Executable || config.type == ProjectType::Mixed) {
        content << "cc_binary(\n";
        content << "    name = \"" << config.name << "_bin\",\n";
        content << "    srcs = [\"src/main.cpp\"],\n";
        if (config.type == ProjectType::Mixed) {
            content << "    deps = [\":\" + \"" << config.name << "\"],\n";
        }
        content << ")\n\n";
    }
    
    content << "cc_test(\n";
    content << "    name = \"" << config.name << "_test\",\n";
    content << "    srcs = glob([\"tests/**/*.cpp\"]),\n";
    if (config.type != ProjectType::Executable) {
        content << "    deps = [\n";
        content << "        \":\" + \"" << config.name << "\",\n";
        content << "        \"@googletest//:gtest_main\",\n";
        content << "    ],\n";
    } else {
        content << "    deps = [\"@googletest//:gtest_main\"],\n";
    }
    content << ")\n";
    
    try {
        std::ofstream file(build_file);
        file << content.str();
        return file.good();
    } catch (const std::exception& e) {
        std::cerr << "Error writing BUILD.bazel: " << e.what() << std::endl;
        return false;
    }
}

bool ProjectModule::generate_meld_yaml(const ProjectConfig& config, const std::filesystem::path& project_dir) {
    std::filesystem::path yaml_file = project_dir / "meld.yaml";
    
    std::ostringstream content;
    content << "project: \"" << config.name << "\"\n";
    content << "version: \"" << config.version << "\"\n";
    
    // Add targets section if targets are defined
    if (!config.targets.empty()) {
        content << "\ntargets:\n";
        for (const auto& target : config.targets) {
            content << "  - name: \"" << target.name << "\"\n";
            content << "    type: \"" << target.type << "\"\n";
            content << "    output: \"" << target.output << "\"\n";
            
            // Add target-specific settings
            for (const auto& setting : target.settings) {
                content << "    " << setting.first << ": \"" << setting.second << "\"\n";
            }
        }
    } else {
        // Legacy format for backward compatibility
        content << "type: ";
        switch (config.type) {
            case ProjectType::Library:
                content << "library\n";
                break;
            case ProjectType::Executable:
                content << "executable\n";
                break;
            case ProjectType::Mixed:
                content << "mixed\n";
                break;
        }
        
        content << "\nbuild_system: ";
        switch (config.build_system) {
            case BuildSystem::Bazel:
                content << "bazel\n";
                break;
            case BuildSystem::Native:
                content << "native\n";
                break;
            case BuildSystem::Unknown:
                content << "unknown\n";
                break;
        }
    }
    
    if (!config.dependencies.empty()) {
        content << "\ndependencies:\n";
        for (const auto& dep : config.dependencies) {
            content << "  - " << dep << "\n";
        }
    }
    
    try {
        std::ofstream file(yaml_file);
        file << content.str();
        return file.good();
    } catch (const std::exception& e) {
        std::cerr << "Error writing meld.yaml: " << e.what() << std::endl;
        return false;
    }
}

bool ProjectModule::generate_sample_source_files(const ProjectConfig& config, const std::filesystem::path& project_dir) {
    try {
        if (config.type == ProjectType::Library || config.type == ProjectType::Mixed) {
            // Generate library header
            std::filesystem::path header_file = project_dir / "include" / config.name / (config.name + ".hpp");
            std::filesystem::create_directories(header_file.parent_path());
            
            std::ofstream header(header_file);
            header << "#pragma once\n\n";
            header << "namespace " << config.name << " {\n\n";
            header << "/**\n";
            header << " * Example function for " << config.name << " library\n";
            header << " */\n";
            header << "int example_function(int value);\n\n";
            header << "} // namespace " << config.name << "\n";
            
            // Generate library source
            std::filesystem::path source_file = project_dir / "src" / (config.name + ".cpp");
            std::ofstream source(source_file);
            source << "#include \"" << config.name << "/" << config.name << ".hpp\"\n\n";
            source << "namespace " << config.name << " {\n\n";
            source << "int example_function(int value) {\n";
            source << "    return value * 2;\n";
            source << "}\n\n";
            source << "} // namespace " << config.name << "\n";
        }
        
        if (config.type == ProjectType::Executable || config.type == ProjectType::Mixed) {
            // Generate main.cpp
            std::filesystem::path main_file = project_dir / "src" / "main.cpp";
            std::ofstream main(main_file);
            main << "#include <iostream>\n";
            if (config.type == ProjectType::Mixed) {
                main << "#include \"" << config.name << "/" << config.name << ".hpp\"\n";
            }
            main << "\n";
            main << "int main(int argc, char* argv[]) {\n";
            main << "    std::cout << \"Hello from " << config.name << "!\" << std::endl;\n";
            if (config.type == ProjectType::Mixed) {
                main << "    std::cout << \"Example function result: \" << " << config.name << "::example_function(21) << std::endl;\n";
            }
            main << "    return 0;\n";
            main << "}\n";
        }
        
        // Generate test file
        std::filesystem::path test_file = project_dir / "tests" / (config.name + "_test.cpp");
        std::ofstream test(test_file);
        test << "#include <gtest/gtest.h>\n";
        if (config.type != ProjectType::Executable) {
            test << "#include \"" << config.name << "/" << config.name << ".hpp\"\n";
        }
        test << "\n";
        if (config.type != ProjectType::Executable) {
            test << "TEST(" << config.name << "Test, ExampleFunction) {\n";
            test << "    EXPECT_EQ(" << config.name << "::example_function(5), 10);\n";
            test << "}\n\n";
        }
        test << "TEST(" << config.name << "Test, BasicTest) {\n";
        test << "    EXPECT_TRUE(true);\n";
        test << "}\n";
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error generating sample source files: " << e.what() << std::endl;
        return false;
    }
}

bool ProjectModule::generate_documentation(const ProjectConfig& config, const std::filesystem::path& project_dir) {
    try {
        // Generate README.md
        std::filesystem::path readme_file = project_dir / "README.md";
        std::ofstream readme(readme_file);
        readme << "# " << config.name << "\n\n";
        readme << "A Meld ";
        switch (config.type) {
            case ProjectType::Library:
                readme << "library";
                break;
            case ProjectType::Executable:
                readme << "application";
                break;
            case ProjectType::Mixed:
                readme << "project";
                break;
        }
        readme << " project.\n\n";
        readme << "## Building\n\n";
        readme << "```bash\n";
        readme << "meld project build\n";
        readme << "```\n\n";
        readme << "## Testing\n\n";
        readme << "```bash\n";
        readme << "meld project test\n";
        readme << "```\n\n";
        readme << "## Running\n\n";
        if (config.type == ProjectType::Executable || config.type == ProjectType::Mixed) {
            readme << "```bash\n";
            readme << "bazel run :" << config.name << "_bin\n";
            readme << "```\n\n";
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error generating documentation: " << e.what() << std::endl;
        return false;
    }
}

BuildSystem ProjectModule::detect_build_system(const std::filesystem::path& project_dir) {
    if (file_exists(project_dir / "BUILD.bazel") || file_exists(project_dir / "BUILD")) {
        return BuildSystem::Bazel;
    } else if (file_exists(project_dir / "meld.yaml")) {
        return BuildSystem::Native;
    } else {
        return BuildSystem::Unknown;
    }
}

BuildResult ProjectModule::build_project(const std::filesystem::path& project_dir, const std::vector<std::string>& options) {
    BuildSystem build_system = detect_build_system(project_dir);
    
    switch (build_system) {
        case BuildSystem::Bazel:
            return build_with_bazel(project_dir, options);
        case BuildSystem::Native:
            return build_with_native(project_dir, options);
        default:
            BuildResult result;
            result.success = false;
            result.output = "No supported build system detected";
            result.build_time_seconds = 0.0;
            return result;
    }
}

BuildResult ProjectModule::build_with_bazel(const std::filesystem::path& project_dir, const std::vector<std::string>& options) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::ostringstream cmd;
    cmd << "bazel build //...";
    for (const auto& option : options) {
        cmd << " " << option;
    }
    
    std::string output = execute_command(cmd.str(), project_dir);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    BuildResult result;
    result.success = output.find("FAILED") == std::string::npos;
    result.output = output;
    result.build_time_seconds = duration.count() / 1000.0;
    
    // Extract artifacts (simplified)
    if (result.success) {
        result.artifacts.push_back("bazel-bin/");
    }
    
    return result;
}

BuildResult ProjectModule::build_with_native(const std::filesystem::path& project_dir, const std::vector<std::string>& options) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Placeholder for native build system
    std::string output = "Native build system not yet implemented";
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    BuildResult result;
    result.success = false;
    result.output = output;
    result.build_time_seconds = duration.count() / 1000.0;
    
    return result;
}

TestResult ProjectModule::run_tests(const std::filesystem::path& project_dir, const std::vector<std::string>& options) {
    BuildSystem build_system = detect_build_system(project_dir);
    
    switch (build_system) {
        case BuildSystem::Bazel:
            return run_bazel_tests(project_dir, options);
        case BuildSystem::Native:
            return run_native_tests(project_dir, options);
        default:
            TestResult result;
            result.success = false;
            result.total_tests = 0;
            result.passed_tests = 0;
            result.failed_tests = 0;
            result.execution_time_seconds = 0.0;
            result.failures.push_back("No supported build system detected");
            return result;
    }
}

TestResult ProjectModule::run_bazel_tests(const std::filesystem::path& project_dir, const std::vector<std::string>& options) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::ostringstream cmd;
    cmd << "bazel test //...";
    for (const auto& option : options) {
        cmd << " " << option;
    }
    
    std::string output = execute_command(cmd.str(), project_dir);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    TestResult result;
    result.success = output.find("FAILED") == std::string::npos;
    result.execution_time_seconds = duration.count() / 1000.0;
    
    // Parse test results (simplified)
    result.total_tests = 1; // Placeholder
    result.passed_tests = result.success ? 1 : 0;
    result.failed_tests = result.success ? 0 : 1;
    
    if (!result.success) {
        result.failures.push_back("Test execution failed");
    }
    
    return result;
}

TestResult ProjectModule::run_native_tests(const std::filesystem::path& project_dir, const std::vector<std::string>& options) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Placeholder for native test system
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    TestResult result;
    result.success = false;
    result.total_tests = 0;
    result.passed_tests = 0;
    result.failed_tests = 0;
    result.execution_time_seconds = duration.count() / 1000.0;
    result.failures.push_back("Native test system not yet implemented");
    
    return result;
}

bool ProjectModule::clean_project(const std::filesystem::path& project_dir) {
    BuildSystem build_system = detect_build_system(project_dir);
    
    switch (build_system) {
        case BuildSystem::Bazel:
            return clean_bazel_artifacts(project_dir);
        case BuildSystem::Native:
            return clean_native_artifacts(project_dir);
        default:
            std::cerr << "No supported build system detected" << std::endl;
            return false;
    }
}

bool ProjectModule::clean_bazel_artifacts(const std::filesystem::path& project_dir) {
    std::string output = execute_command("bazel clean", project_dir);
    return output.find("ERROR") == std::string::npos;
}

bool ProjectModule::clean_native_artifacts(const std::filesystem::path& project_dir) {
    // Placeholder for native clean
    return false;
}

bool ProjectModule::fetch_dependencies(const std::filesystem::path& project_dir) {
    // Placeholder for dependency fetching
    std::cout << "Dependency fetching not yet implemented" << std::endl;
    return true;
}

bool ProjectModule::resolve_dependencies(const std::vector<Dependency>& dependencies, std::vector<Dependency>& resolved) {
    // Placeholder for dependency resolution
    resolved = dependencies;
    return true;
}

bool ProjectModule::install_dependency(const Dependency& dep, const std::filesystem::path& project_dir) {
    // Placeholder for dependency installation
    return true;
}

void ProjectModule::register_template(const ProjectTemplate& tmpl) {
    templates_[tmpl.name] = tmpl;
}

std::vector<ProjectTemplate> ProjectModule::get_available_templates() const {
    std::vector<ProjectTemplate> templates;
    for (const auto& pair : templates_) {
        templates.push_back(pair.second);
    }
    return templates;
}

ProjectTemplate ProjectModule::get_template(const std::string& name) const {
    auto it = templates_.find(name);
    if (it != templates_.end()) {
        return it->second;
    }
    throw std::runtime_error("Template not found: " + name);
}

bool ProjectModule::create_directory_structure(const std::vector<std::string>& directories, const std::filesystem::path& base_dir) {
    try {
        for (const auto& dir : directories) {
            std::filesystem::create_directories(base_dir / dir);
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error creating directory structure: " << e.what() << std::endl;
        return false;
    }
}

bool ProjectModule::write_template_files(const std::map<std::string, std::string>& files, const std::filesystem::path& base_dir, const ProjectConfig& config) {
    try {
        for (const auto& pair : files) {
            std::filesystem::path file_path = base_dir / pair.first;
            std::filesystem::create_directories(file_path.parent_path());
            
            std::string content = expand_template(pair.second, config);
            std::ofstream file(file_path);
            file << content;
            
            if (!file.good()) {
                return false;
            }
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error writing template files: " << e.what() << std::endl;
        return false;
    }
}

std::string ProjectModule::expand_template(const std::string& template_content, const ProjectConfig& config) {
    std::string result = template_content;
    
    // Simple template variable replacement
    std::regex name_regex(R"(\{\{name\}\})");
    result = std::regex_replace(result, name_regex, config.name);
    
    std::regex version_regex(R"(\{\{version\}\})");
    result = std::regex_replace(result, version_regex, config.version);
    
    return result;
}

bool ProjectModule::file_exists(const std::filesystem::path& path) {
    return std::filesystem::exists(path) && std::filesystem::is_regular_file(path);
}

bool ProjectModule::directory_exists(const std::filesystem::path& path) {
    return std::filesystem::exists(path) && std::filesystem::is_directory(path);
}

std::string ProjectModule::execute_command(const std::string& command, const std::filesystem::path& working_dir) {
    // Change to working directory
    std::filesystem::path original_dir = std::filesystem::current_path();
    if (working_dir != ".") {
        std::filesystem::current_path(working_dir);
    }
    
    // Execute command
    std::string result;
    FILE* pipe = popen(command.c_str(), "r");
    if (pipe) {
        char buffer[128];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
        pclose(pipe);
    }
    
    // Restore original directory
    if (working_dir != ".") {
        std::filesystem::current_path(original_dir);
    }
    
    return result;
}

std::optional<ProjectConfig> ProjectModule::parse_meld_yaml(const std::filesystem::path& yaml_file) {
    if (!file_exists(yaml_file)) {
        return std::nullopt;
    }
    
    try {
        std::ifstream file(yaml_file);
        std::string line;
        ProjectConfig config;
        
        // State for parsing
        bool in_targets_section = false;
        bool in_dependencies_section = false;
        TargetConfig current_target;
        
        while (std::getline(file, line)) {
            // Trim whitespace
            line.erase(0, line.find_first_not_of(" \t"));
            line.erase(line.find_last_not_of(" \t") + 1);
            
            // Skip empty lines and comments
            if (line.empty() || line[0] == '#') {
                continue;
            }
            
            // Check for section headers
            if (line == "targets:") {
                in_targets_section = true;
                in_dependencies_section = false;
                continue;
            } else if (line == "dependencies:") {
                in_dependencies_section = true;
                in_targets_section = false;
                continue;
            }
            
            // Parse based on current section
            if (in_targets_section) {
                if (line.find("- name:") == 0 || line.find("  - name:") == 0) {
                    // Save previous target if exists
                    if (!current_target.name.empty()) {
                        config.targets.push_back(current_target);
                        current_target = TargetConfig{};
                    }
                    
                    // Parse target name
                    size_t colon_pos = line.find(':');
                    if (colon_pos != std::string::npos) {
                        std::string value = line.substr(colon_pos + 1);
                        value.erase(0, value.find_first_not_of(" \t\""));
                        value.erase(value.find_last_not_of(" \t\"") + 1);
                        current_target.name = value;
                    }
                } else if (line.find("type:") != std::string::npos && line.find("    ") == 0) {
                    // Parse target type
                    size_t colon_pos = line.find(':');
                    if (colon_pos != std::string::npos) {
                        std::string value = line.substr(colon_pos + 1);
                        value.erase(0, value.find_first_not_of(" \t\""));
                        value.erase(value.find_last_not_of(" \t\"") + 1);
                        current_target.type = value;
                    }
                } else if (line.find("output:") != std::string::npos && line.find("    ") == 0) {
                    // Parse target output
                    size_t colon_pos = line.find(':');
                    if (colon_pos != std::string::npos) {
                        std::string value = line.substr(colon_pos + 1);
                        value.erase(0, value.find_first_not_of(" \t\""));
                        value.erase(value.find_last_not_of(" \t\"") + 1);
                        current_target.output = value;
                    }
                } else if (line.find("    ") == 0 && line.find(':') != std::string::npos) {
                    // Parse target-specific settings
                    size_t colon_pos = line.find(':');
                    std::string key = line.substr(4, colon_pos - 4); // Remove leading spaces
                    std::string value = line.substr(colon_pos + 1);
                    
                    // Trim key and value
                    key.erase(0, key.find_first_not_of(" \t"));
                    key.erase(key.find_last_not_of(" \t") + 1);
                    value.erase(0, value.find_first_not_of(" \t\""));
                    value.erase(value.find_last_not_of(" \t\"") + 1);
                    
                    // Skip already parsed fields
                    if (key != "name" && key != "type" && key != "output") {
                        current_target.settings[key] = value;
                    }
                }
            } else if (in_dependencies_section) {
                if (line.find("- ") == 0 || line.find("  - ") == 0) {
                    std::string dep = line.substr(line.find("- ") + 2);
                    dep.erase(0, dep.find_first_not_of(" \t"));
                    dep.erase(dep.find_last_not_of(" \t") + 1);
                    config.dependencies.push_back(dep);
                }
            } else {
                // Parse top-level fields
                if (line.find("project:") == 0 || line.find("name:") == 0) {
                    size_t colon_pos = line.find(':');
                    if (colon_pos != std::string::npos) {
                        std::string value = line.substr(colon_pos + 1);
                        value.erase(0, value.find_first_not_of(" \t\""));
                        value.erase(value.find_last_not_of(" \t\"") + 1);
                        config.name = value;
                    }
                } else if (line.find("version:") == 0) {
                    size_t colon_pos = line.find(':');
                    if (colon_pos != std::string::npos) {
                        std::string value = line.substr(colon_pos + 1);
                        value.erase(0, value.find_first_not_of(" \t\""));
                        value.erase(value.find_last_not_of(" \t\"") + 1);
                        config.version = value;
                    }
                } else if (line.find("type:") == 0) {
                    size_t colon_pos = line.find(':');
                    if (colon_pos != std::string::npos) {
                        std::string value = line.substr(colon_pos + 1);
                        value.erase(0, value.find_first_not_of(" \t\""));
                        value.erase(value.find_last_not_of(" \t\"") + 1);
                        
                        if (value == "library") {
                            config.type = ProjectType::Library;
                        } else if (value == "executable") {
                            config.type = ProjectType::Executable;
                        } else if (value == "mixed") {
                            config.type = ProjectType::Mixed;
                        }
                    }
                } else if (line.find("build_system:") == 0) {
                    size_t colon_pos = line.find(':');
                    if (colon_pos != std::string::npos) {
                        std::string value = line.substr(colon_pos + 1);
                        value.erase(0, value.find_first_not_of(" \t\""));
                        value.erase(value.find_last_not_of(" \t\"") + 1);
                        
                        if (value == "bazel") {
                            config.build_system = BuildSystem::Bazel;
                        } else if (value == "native") {
                            config.build_system = BuildSystem::Native;
                        } else {
                            config.build_system = BuildSystem::Unknown;
                        }
                    }
                }
            }
        }
        
        // Save last target if exists
        if (!current_target.name.empty()) {
            config.targets.push_back(current_target);
        }
        
        return config;
    } catch (const std::exception& e) {
        std::cerr << "Error parsing meld.yaml: " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::optional<ProjectConfig> ProjectModule::load_project_config(const std::filesystem::path& project_dir) {
    std::filesystem::path yaml_file = project_dir / "meld.yaml";
    return parse_meld_yaml(yaml_file);
}

void ProjectModule::initialize_default_templates() {
    register_template(create_library_template());
    register_template(create_executable_template());
}

ProjectTemplate ProjectModule::create_library_template() {
    ProjectTemplate tmpl;
    tmpl.name = "lib";
    tmpl.type = ProjectType::Library;
    tmpl.description = "Library project template";
    tmpl.directories = {"src", "include", "tests", "docs"};
    
    // Template files will be generated by the specific generation functions
    
    return tmpl;
}

ProjectTemplate ProjectModule::create_executable_template() {
    ProjectTemplate tmpl;
    tmpl.name = "bin";
    tmpl.type = ProjectType::Executable;
    tmpl.description = "Executable project template";
    tmpl.directories = {"src", "tests", "docs"};
    
    // Template files will be generated by the specific generation functions
    
    return tmpl;
}

} // namespace meld::cli