#include "meld/cli/package_module.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <random>

namespace meld::cli {

PackageModule::PackageModule(std::shared_ptr<ErrorHandler> error_handler)
    : BaseCommandHandler("package", "Package management commands (install, publish, search)")
    , error_handler_(error_handler) {
    
    // Initialize package directories
    package_cache_dir_ = std::filesystem::temp_directory_path() / "meld" / "package_cache";
    global_packages_dir_ = std::filesystem::temp_directory_path() / "meld" / "packages";
    
    std::filesystem::create_directories(package_cache_dir_);
    std::filesystem::create_directories(global_packages_dir_);
    
    initialize_default_registries();
    load_configuration();
}

CommandResult PackageModule::execute(const CommandArgs& args) {
    if (args.subcommand.empty()) {
        std::cout << get_help() << std::endl;
        return CommandResult::InvalidArguments;
    }

    // meld mod subcommands (Git-first package management)
    if (args.subcommand == "fetch") {
        return handle_mod_fetch_command(args);
    } else if (args.subcommand == "update" && args.command == "mod") {
        return handle_mod_update_command(args);
    } else if (args.subcommand == "clean" && args.command == "mod") {
        return handle_mod_clean_command(args);
    }

    // Existing registry-based subcommands
    if (args.subcommand == "install") {
        return handle_install_command(args);
    } else if (args.subcommand == "uninstall") {
        return handle_uninstall_command(args);
    } else if (args.subcommand == "publish") {
        return handle_publish_command(args);
    } else if (args.subcommand == "search") {
        return handle_search_command(args);
    } else if (args.subcommand == "update") {
        return handle_update_command(args);
    } else if (args.subcommand == "list") {
        return handle_list_command(args);
    } else if (args.subcommand == "registry") {
        return handle_registry_command(args);
    } else {
        std::cerr << "Unknown subcommand: " << args.subcommand << std::endl;
        std::cout << get_help() << std::endl;
        return CommandResult::InvalidArguments;
    }
}

std::string PackageModule::get_help() const {
    return R"(Package management commands:

USAGE:
    meld mod <SUBCOMMAND> [OPTIONS]
    meld package <SUBCOMMAND> [OPTIONS]

MOD SUBCOMMANDS (Git-first package management):
    fetch               Fetch all Git dependencies declared in meld.toml
    update              Resolve latest versions and update meld.lock
    clean               Purge the local dependency cache (~/.meld/cache/)

PACKAGE SUBCOMMANDS:
    install <package>   Install a package
    uninstall <package> Uninstall a package
    publish             Publish current package
    search <query>      Search for packages
    update              Update all packages
    list                List installed packages
    registry            Manage package registries

EXAMPLES:
    meld mod fetch
    meld mod update
    meld mod clean
    meld package install json-parser
    meld package search "http client")";
}

std::string PackageModule::get_usage() const {
    return "meld package <install|uninstall|publish|search|update|list|registry> [OPTIONS]";
}

std::vector<std::string> PackageModule::get_completions(const std::string& partial) const {
    std::vector<std::string> completions = {
        "fetch", "update", "clean",
        "install", "uninstall", "publish", "search", "list", "registry"
    };
    std::vector<std::string> result;
    
    for (const auto& cmd : completions) {
        if (cmd.find(partial) == 0) {
            result.push_back(cmd);
        }
    }
    
    return result;
}

bool PackageModule::validate_args(const CommandArgs& args, std::string& error_message) const {
    if (args.subcommand.empty()) {
        error_message = "Subcommand required";
        return false;
    }
    
    std::vector<std::string> valid_subcommands = {
        "fetch", "update", "clean",
        "install", "uninstall", "publish", "search", "list", "registry"
    };
    if (std::find(valid_subcommands.begin(), valid_subcommands.end(), args.subcommand) == valid_subcommands.end()) {
        error_message = "Invalid subcommand: " + args.subcommand;
        return false;
    }
    
    if ((args.subcommand == "install" || args.subcommand == "uninstall" || args.subcommand == "search") && args.positional.empty()) {
        error_message = "Package name or query required for '" + args.subcommand + "' command";
        return false;
    }
    
    return true;
}

CommandResult PackageModule::handle_install_command(const CommandArgs& args) {
    if (args.positional.empty()) {
        std::cerr << "Error: Package name required" << std::endl;
        return CommandResult::InvalidArguments;
    }
    
    std::string package_spec = args.positional[0];
    bool is_dev = args.flags.count("dev") > 0;
    
    try {
        PackageInstallResult result = install_package(package_spec, is_dev);
        
        if (result.success) {
            std::cout << "Successfully installed " << result.package_name 
                      << " version " << result.installed_version << std::endl;
            if (!result.resolved_dependencies.empty()) {
                std::cout << "Also installed dependencies:" << std::endl;
                for (const auto& dep : result.resolved_dependencies) {
                    std::cout << "  " << dep.name << " " << dep.version_constraint << std::endl;
                }
            }
            return CommandResult::Success;
        } else {
            std::cerr << "Failed to install package: " << result.error_message << std::endl;
            return CommandResult::Error;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error installing package: " << e.what() << std::endl;
        return CommandResult::Error;
    }
}

CommandResult PackageModule::handle_uninstall_command(const CommandArgs& args) {
    if (args.positional.empty()) {
        std::cerr << "Error: Package name required" << std::endl;
        return CommandResult::InvalidArguments;
    }
    
    std::string package_name = args.positional[0];
    
    try {
        bool success = uninstall_package(package_name);
        
        if (success) {
            std::cout << "Successfully uninstalled " << package_name << std::endl;
            return CommandResult::Success;
        } else {
            std::cerr << "Failed to uninstall package: " << package_name << std::endl;
            return CommandResult::Error;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error uninstalling package: " << e.what() << std::endl;
        return CommandResult::Error;
    }
}

CommandResult PackageModule::handle_publish_command(const CommandArgs& args) {
    std::filesystem::path package_dir = std::filesystem::current_path();
    bool dry_run = args.flags.count("dry-run") > 0;
    
    try {
        // Load package metadata
        auto metadata = load_package_metadata(package_dir);
        if (!metadata) {
            std::cerr << "Error: No package metadata found (meld.yaml)" << std::endl;
            return CommandResult::Error;
        }
        
        // Validate metadata
        std::vector<std::string> errors;
        if (!validate_package_metadata(*metadata, errors)) {
            std::cerr << "Package metadata validation failed:" << std::endl;
            for (const auto& error : errors) {
                std::cerr << "  " << error << std::endl;
            }
            return CommandResult::Error;
        }
        
        if (dry_run) {
            std::cout << "Package validation successful (dry run)" << std::endl;
            return CommandResult::Success;
        }
        
        // Get registry
        PackageRegistry registry = get_default_registry();
        if (args.options.count("registry")) {
            std::string registry_name = args.options.at("registry");
            auto registries = get_registries();
            auto it = std::find_if(registries.begin(), registries.end(),
                [&registry_name](const PackageRegistry& r) { return r.name == registry_name; });
            if (it != registries.end()) {
                registry = *it;
            } else {
                std::cerr << "Registry not found: " << registry_name << std::endl;
                return CommandResult::Error;
            }
        }
        
        bool success = publish_package(package_dir, registry);
        
        if (success) {
            std::cout << "Successfully published " << metadata->name 
                      << " version " << metadata->version << std::endl;
            return CommandResult::Success;
        } else {
            std::cerr << "Failed to publish package" << std::endl;
            return CommandResult::Error;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error publishing package: " << e.what() << std::endl;
        return CommandResult::Error;
    }
}

CommandResult PackageModule::handle_search_command(const CommandArgs& args) {
    if (args.positional.empty()) {
        std::cerr << "Error: Search query required" << std::endl;
        return CommandResult::InvalidArguments;
    }
    
    std::string query = args.positional[0];
    int limit = 10; // default
    
    if (args.options.count("limit")) {
        try {
            limit = std::stoi(args.options.at("limit"));
        } catch (const std::exception&) {
            std::cerr << "Invalid limit value" << std::endl;
            return CommandResult::InvalidArguments;
        }
    }
    
    try {
        std::vector<PackageRegistry> search_registries;
        if (args.options.count("registry")) {
            std::string registry_name = args.options.at("registry");
            auto registries = get_registries();
            auto it = std::find_if(registries.begin(), registries.end(),
                [&registry_name](const PackageRegistry& r) { return r.name == registry_name; });
            if (it != registries.end()) {
                search_registries.push_back(*it);
            } else {
                std::cerr << "Registry not found: " << registry_name << std::endl;
                return CommandResult::Error;
            }
        }
        
        auto results = search_packages(query, search_registries);
        
        if (results.empty()) {
            std::cout << "No packages found matching '" << query << "'" << std::endl;
            return CommandResult::Success;
        }
        
        std::cout << "Found " << results.size() << " packages:" << std::endl;
        int count = 0;
        for (const auto& result : results) {
            if (count >= limit) break;
            
            std::cout << result.name << " (" << result.version << ")" << std::endl;
            std::cout << "  " << result.description << std::endl;
            if (!result.keywords.empty()) {
                std::cout << "  Keywords: ";
                for (size_t i = 0; i < result.keywords.size(); ++i) {
                    if (i > 0) std::cout << ", ";
                    std::cout << result.keywords[i];
                }
                std::cout << std::endl;
            }
            std::cout << std::endl;
            count++;
        }
        
        return CommandResult::Success;
    } catch (const std::exception& e) {
        std::cerr << "Error searching packages: " << e.what() << std::endl;
        return CommandResult::Error;
    }
}

CommandResult PackageModule::handle_update_command(const CommandArgs& args) {
    std::cout << "Package update functionality not yet implemented" << std::endl;
    return CommandResult::Success;
}

CommandResult PackageModule::handle_list_command(const CommandArgs& args) {
    std::cout << "Package list functionality not yet implemented" << std::endl;
    return CommandResult::Success;
}

CommandResult PackageModule::handle_registry_command(const CommandArgs& args) {
    std::cout << "Registry management functionality not yet implemented" << std::endl;
    return CommandResult::Success;
}

PackageInstallResult PackageModule::install_package(const std::string& package_spec, bool is_dev_dependency) {
    PackageInstallResult result;
    result.success = false;
    
    try {
        std::string name, version;
        parse_package_spec(package_spec, name, version);
        
        if (!is_valid_package_name(name)) {
            result.error_message = "Invalid package name: " + name;
            return result;
        }
        
        // Try to install from default registry
        PackageRegistry registry = get_default_registry();
        result = install_package_from_registry(name, version, registry);
        
        if (result.success) {
            result.package_name = name;
            
            // Resolve and install dependencies
            auto metadata = fetch_package_metadata(name, registry);
            if (metadata) {
                // Simulate dependency resolution
                std::vector<PackageDependency> deps;
                // In a real implementation, this would parse dependencies from metadata
                result.resolved_dependencies = deps;
            }
        }
        
        return result;
    } catch (const std::exception& e) {
        result.error_message = e.what();
        return result;
    }
}

PackageInstallResult PackageModule::install_package_from_registry(const std::string& name, const std::string& version, const PackageRegistry& registry) {
    PackageInstallResult result;
    result.success = false;
    
    try {
        // Simulate package installation
        std::filesystem::path install_dir = get_package_install_dir(name);
        std::filesystem::create_directories(install_dir);
        
        // Simulate downloading and extracting package
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Simulate network delay
        
        // Create a dummy package file
        std::filesystem::path package_file = install_dir / (name + ".meld");
        std::ofstream file(package_file);
        file << "// Package: " << name << std::endl;
        file << "// Version: " << (version.empty() ? "latest" : version) << std::endl;
        file << "// Installed from registry: " << registry.name << std::endl;
        
        result.success = true;
        result.package_name = name;
        result.installed_version = version.empty() ? "1.0.0" : version;
        result.install_path = install_dir;
        result.installed_files.push_back(package_file.string());
        
        return result;
    } catch (const std::exception& e) {
        result.error_message = e.what();
        return result;
    }
}

PackageInstallResult PackageModule::install_package_from_git(const std::string& git_url, const std::string& ref) {
    PackageInstallResult result;
    result.success = false;
    result.error_message = "Git installation not yet implemented";
    return result;
}

PackageInstallResult PackageModule::install_local_package(const std::filesystem::path& package_path) {
    PackageInstallResult result;
    result.success = false;
    result.error_message = "Local package installation not yet implemented";
    return result;
}

bool PackageModule::uninstall_package(const std::string& package_name) {
    try {
        std::filesystem::path install_dir = get_package_install_dir(package_name);
        if (std::filesystem::exists(install_dir)) {
            std::filesystem::remove_all(install_dir);
            return true;
        } else {
            std::cerr << "Package not found: " << package_name << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error uninstalling package: " << e.what() << std::endl;
        return false;
    }
}

bool PackageModule::publish_package(const std::filesystem::path& package_dir, const PackageRegistry& registry) {
    try {
        // Create package archive
        std::filesystem::path archive_path = package_cache_dir_ / "temp_package.tar.gz";
        if (!create_package_archive(package_dir, archive_path)) {
            return false;
        }
        
        // Upload to registry
        bool success = upload_package_to_registry(archive_path, registry);
        
        // Clean up temporary archive
        std::filesystem::remove(archive_path);
        
        return success;
    } catch (const std::exception& e) {
        std::cerr << "Error publishing package: " << e.what() << std::endl;
        return false;
    }
}

bool PackageModule::create_package_archive(const std::filesystem::path& package_dir, const std::filesystem::path& output_path) {
    // Simulate creating package archive
    try {
        std::ofstream archive(output_path, std::ios::binary);
        archive << "MOCK_PACKAGE_ARCHIVE_DATA" << std::endl;
        return archive.good();
    } catch (const std::exception& e) {
        std::cerr << "Error creating package archive: " << e.what() << std::endl;
        return false;
    }
}

bool PackageModule::validate_package_metadata(const PackageMetadata& metadata, std::vector<std::string>& errors) {
    errors.clear();
    
    if (metadata.name.empty()) {
        errors.push_back("Package name is required");
    } else if (!is_valid_package_name(metadata.name)) {
        errors.push_back("Invalid package name: " + metadata.name);
    }
    
    if (metadata.version.empty()) {
        errors.push_back("Package version is required");
    } else if (!is_valid_version(metadata.version)) {
        errors.push_back("Invalid version format: " + metadata.version);
    }
    
    if (metadata.description.empty()) {
        errors.push_back("Package description is required");
    }
    
    if (metadata.author.empty()) {
        errors.push_back("Package author is required");
    }
    
    return errors.empty();
}

bool PackageModule::upload_package_to_registry(const std::filesystem::path& package_archive, const PackageRegistry& registry) {
    // Simulate uploading to registry
    std::this_thread::sleep_for(std::chrono::milliseconds(200)); // Simulate network delay
    return true; // Assume success for simulation
}

std::vector<PackageSearchResult> PackageModule::search_packages(const std::string& query, const std::vector<PackageRegistry>& registries) {
    std::vector<PackageSearchResult> all_results;
    
    std::vector<PackageRegistry> search_registries = registries;
    if (search_registries.empty()) {
        search_registries = get_registries();
    }
    
    for (const auto& registry : search_registries) {
        auto results = search_registry(query, registry);
        all_results.insert(all_results.end(), results.begin(), results.end());
    }
    
    // Rank and sort results
    return rank_search_results(all_results, query);
}

std::vector<PackageSearchResult> PackageModule::search_registry(const std::string& query, const PackageRegistry& registry) {
    std::vector<PackageSearchResult> results;
    
    // Simulate search results
    std::vector<std::string> mock_packages = {
        "json-parser", "http-client", "test-framework", "logging-lib", 
        "crypto-utils", "string-utils", "math-lib", "network-tools"
    };
    
    for (const auto& package : mock_packages) {
        if (package.find(query) != std::string::npos || query.find(package) != std::string::npos) {
            PackageSearchResult result;
            result.name = package;
            result.version = "1.0.0";
            result.description = "A useful " + package + " for Meld applications";
            result.keywords = {package, "utility", "library"};
            result.relevance_score = calculate_relevance_score(result, query);
            result.registry_url = registry.url;
            results.push_back(result);
        }
    }
    
    return results;
}

std::vector<PackageSearchResult> PackageModule::rank_search_results(const std::vector<PackageSearchResult>& results, const std::string& query) {
    std::vector<PackageSearchResult> ranked_results = results;
    
    // Sort by relevance score (descending)
    std::sort(ranked_results.begin(), ranked_results.end(),
        [](const PackageSearchResult& a, const PackageSearchResult& b) {
            return a.relevance_score > b.relevance_score;
        });
    
    return ranked_results;
}

std::vector<PackageDependency> PackageModule::resolve_dependencies(const std::vector<PackageDependency>& dependencies) {
    // Simulate dependency resolution
    return dependencies; // For now, just return the input dependencies
}

std::vector<DependencyConflict> PackageModule::detect_conflicts(const std::vector<PackageDependency>& dependencies) {
    std::vector<DependencyConflict> conflicts;
    
    // Simulate conflict detection
    std::map<std::string, std::vector<std::string>> package_versions;
    for (const auto& dep : dependencies) {
        package_versions[dep.name].push_back(dep.version_constraint);
    }
    
    for (const auto& pair : package_versions) {
        if (pair.second.size() > 1) {
            // Check if versions are conflicting
            bool has_conflict = false;
            for (size_t i = 0; i < pair.second.size(); ++i) {
                for (size_t j = i + 1; j < pair.second.size(); ++j) {
                    if (pair.second[i] != pair.second[j]) {
                        has_conflict = true;
                        break;
                    }
                }
                if (has_conflict) break;
            }
            
            if (has_conflict) {
                DependencyConflict conflict;
                conflict.package_name = pair.first;
                conflict.conflicting_versions = pair.second;
                conflict.resolution_strategy = "use_latest";
                conflicts.push_back(conflict);
            }
        }
    }
    
    return conflicts;
}

std::vector<PackageDependency> PackageModule::resolve_conflicts(const std::vector<DependencyConflict>& conflicts) {
    std::vector<PackageDependency> resolved;
    
    // Simulate conflict resolution
    for (const auto& conflict : conflicts) {
        PackageDependency resolved_dep;
        resolved_dep.name = conflict.package_name;
        resolved_dep.version_constraint = conflict.conflicting_versions.back(); // Use last version as resolution
        resolved.push_back(resolved_dep);
    }
    
    return resolved;
}

bool PackageModule::update_lock_file(const std::vector<PackageDependency>& resolved_dependencies) {
    try {
        std::filesystem::path lock_file = std::filesystem::current_path() / "meld.lock";
        std::ofstream file(lock_file);
        
        file << "# Meld package lock file" << std::endl;
        file << "# Generated automatically - do not edit" << std::endl;
        file << std::endl;
        
        for (const auto& dep : resolved_dependencies) {
            file << dep.name << " = \"" << dep.version_constraint << "\"" << std::endl;
        }
        
        return file.good();
    } catch (const std::exception& e) {
        std::cerr << "Error updating lock file: " << e.what() << std::endl;
        return false;
    }
}

std::vector<PackageDependency> PackageModule::load_lock_file() {
    std::vector<PackageDependency> dependencies;
    
    try {
        std::filesystem::path lock_file = std::filesystem::current_path() / "meld.lock";
        if (!std::filesystem::exists(lock_file)) {
            return dependencies;
        }
        
        std::ifstream file(lock_file);
        std::string line;
        
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            
            // Parse line: package_name = "version"
            std::regex pattern(R"RE((\w+)\s*=\s*"([^"]+)")RE");
            std::smatch match;
            
            if (std::regex_match(line, match, pattern)) {
                PackageDependency dep;
                dep.name = match[1].str();
                dep.version_constraint = match[2].str();
                dependencies.push_back(dep);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error loading lock file: " << e.what() << std::endl;
    }
    
    return dependencies;
}

bool PackageModule::install_dev_dependencies(const std::filesystem::path& project_dir) {
    auto dev_deps = get_dev_dependencies(project_dir);
    
    for (const auto& dep : dev_deps) {
        auto result = install_package(dep.name + "@" + dep.version_constraint, true);
        if (!result.success) {
            std::cerr << "Failed to install dev dependency: " << dep.name << std::endl;
            return false;
        }
    }
    
    return true;
}

bool PackageModule::remove_dev_dependencies(const std::filesystem::path& project_dir) {
    auto dev_deps = get_dev_dependencies(project_dir);
    
    for (const auto& dep : dev_deps) {
        if (!uninstall_package(dep.name)) {
            std::cerr << "Failed to remove dev dependency: " << dep.name << std::endl;
            return false;
        }
    }
    
    return true;
}

std::vector<PackageDependency> PackageModule::get_dev_dependencies(const std::filesystem::path& project_dir) {
    std::vector<PackageDependency> dev_deps;
    
    // Load from project metadata
    auto metadata = load_package_metadata(project_dir);
    if (metadata) {
        // In a real implementation, this would parse dev dependencies from metadata
        // For now, return empty list
    }
    
    return dev_deps;
}

bool PackageModule::install_native_dependency(const NativeDependency& native_dep) {
    std::string platform = detect_platform();
    std::string arch = detect_architecture();
    
    if (native_dep.platform != platform) {
        std::cerr << "Native dependency platform mismatch: expected " 
                  << native_dep.platform << ", got " << platform << std::endl;
        return false;
    }
    
    // Simulate native dependency installation
    std::cout << "Installing native dependency: " << native_dep.name 
              << " for " << platform << "/" << arch << std::endl;
    
    return true;
}

std::vector<NativeDependency> PackageModule::detect_native_dependencies(const std::filesystem::path& project_dir) {
    std::vector<NativeDependency> native_deps;
    
    // Simulate detection of native dependencies
    // In a real implementation, this would scan project files for native library usage
    
    return native_deps;
}

bool PackageModule::configure_native_dependency_paths(const NativeDependency& native_dep, const std::filesystem::path& project_dir) {
    // Simulate configuring build system with native dependency paths
    return true;
}

std::string PackageModule::detect_platform() {
#ifdef _WIN32
    return "windows";
#elif __APPLE__
    return "macos";
#elif __linux__
    return "linux";
#else
    return "unknown";
#endif
}

std::string PackageModule::detect_architecture() {
#if defined(_M_X64) || defined(__x86_64__)
    return "x86_64";
#elif defined(_M_ARM64) || defined(__aarch64__)
    return "arm64";
#else
    return "unknown";
#endif
}

std::string PackageModule::detect_package_manager() {
    std::string platform = detect_platform();
    
    if (platform == "linux") {
        // Check for various Linux package managers
        if (system("which apt-get > /dev/null 2>&1") == 0) return "apt";
        if (system("which yum > /dev/null 2>&1") == 0) return "yum";
        if (system("which pacman > /dev/null 2>&1") == 0) return "pacman";
    } else if (platform == "macos") {
        if (system("which brew > /dev/null 2>&1") == 0) return "brew";
    } else if (platform == "windows") {
        if (system("where vcpkg > nul 2>&1") == 0) return "vcpkg";
        if (system("where choco > nul 2>&1") == 0) return "chocolatey";
    }
    
    return "unknown";
}

void PackageModule::add_registry(const PackageRegistry& registry) {
    registries_.push_back(registry);
    save_configuration();
}

void PackageModule::remove_registry(const std::string& registry_name) {
    registries_.erase(
        std::remove_if(registries_.begin(), registries_.end(),
            [&registry_name](const PackageRegistry& r) { return r.name == registry_name; }),
        registries_.end());
    save_configuration();
}

std::vector<PackageRegistry> PackageModule::get_registries() const {
    return registries_;
}

PackageRegistry PackageModule::get_default_registry() const {
    for (const auto& registry : registries_) {
        if (registry.is_default) {
            return registry;
        }
    }
    
    // Return first registry if no default is set
    if (!registries_.empty()) {
        return registries_[0];
    }
    
    // Return a mock default registry
    PackageRegistry default_registry;
    default_registry.name = "default";
    default_registry.url = "https://packages.meld-lang.org";
    default_registry.is_default = true;
    return default_registry;
}

void PackageModule::set_default_registry(const std::string& registry_name) {
    for (auto& registry : registries_) {
        registry.is_default = (registry.name == registry_name);
    }
    save_configuration();
}

std::optional<PackageMetadata> PackageModule::load_package_metadata(const std::filesystem::path& package_dir) {
    std::filesystem::path metadata_file = package_dir / "meld.yaml";
    
    if (!std::filesystem::exists(metadata_file)) {
        return std::nullopt;
    }
    
    try {
        std::ifstream file(metadata_file);
        std::string line;
        PackageMetadata metadata;
        
        // Simple YAML parsing (in a real implementation, use a proper YAML library)
        while (std::getline(file, line)) {
            if (line.find("name:") == 0) {
                metadata.name = line.substr(5);
                // Trim whitespace
                metadata.name.erase(0, metadata.name.find_first_not_of(" \t"));
                metadata.name.erase(metadata.name.find_last_not_of(" \t") + 1);
            } else if (line.find("version:") == 0) {
                metadata.version = line.substr(8);
                metadata.version.erase(0, metadata.version.find_first_not_of(" \t"));
                metadata.version.erase(metadata.version.find_last_not_of(" \t") + 1);
            } else if (line.find("description:") == 0) {
                metadata.description = line.substr(12);
                metadata.description.erase(0, metadata.description.find_first_not_of(" \t"));
                metadata.description.erase(metadata.description.find_last_not_of(" \t") + 1);
            } else if (line.find("author:") == 0) {
                metadata.author = line.substr(7);
                metadata.author.erase(0, metadata.author.find_first_not_of(" \t"));
                metadata.author.erase(metadata.author.find_last_not_of(" \t") + 1);
            } else if (line.find("license:") == 0) {
                metadata.license = line.substr(8);
                metadata.license.erase(0, metadata.license.find_first_not_of(" \t"));
                metadata.license.erase(metadata.license.find_last_not_of(" \t") + 1);
            }
        }
        
        return metadata;
    } catch (const std::exception& e) {
        std::cerr << "Error loading package metadata: " << e.what() << std::endl;
        return std::nullopt;
    }
}

bool PackageModule::save_package_metadata(const PackageMetadata& metadata, const std::filesystem::path& package_dir) {
    std::filesystem::path metadata_file = package_dir / "meld.yaml";
    
    try {
        std::ofstream file(metadata_file);
        
        file << "name: " << metadata.name << std::endl;
        file << "version: " << metadata.version << std::endl;
        file << "description: " << metadata.description << std::endl;
        file << "author: " << metadata.author << std::endl;
        if (!metadata.license.empty()) {
            file << "license: " << metadata.license << std::endl;
        }
        if (!metadata.homepage.empty()) {
            file << "homepage: " << metadata.homepage << std::endl;
        }
        if (!metadata.repository.empty()) {
            file << "repository: " << metadata.repository << std::endl;
        }
        if (!metadata.keywords.empty()) {
            file << "keywords:" << std::endl;
            for (const auto& keyword : metadata.keywords) {
                file << "  - " << keyword << std::endl;
            }
        }
        
        return file.good();
    } catch (const std::exception& e) {
        std::cerr << "Error saving package metadata: " << e.what() << std::endl;
        return false;
    }
}

std::optional<PackageMetadata> PackageModule::fetch_package_metadata(const std::string& package_name, const PackageRegistry& registry) {
    // Simulate fetching metadata from registry
    PackageMetadata metadata;
    metadata.name = package_name;
    metadata.version = "1.0.0";
    metadata.description = "A package from " + registry.name;
    metadata.author = "Package Author";
    metadata.license = "MIT";
    
    return metadata;
}

std::string PackageModule::parse_package_spec(const std::string& spec, std::string& name, std::string& version) {
    size_t at_pos = spec.find('@');
    if (at_pos != std::string::npos) {
        name = spec.substr(0, at_pos);
        version = spec.substr(at_pos + 1);
    } else {
        name = spec;
        version = ""; // latest
    }
    return name;
}

bool PackageModule::is_valid_package_name(const std::string& name) {
    if (name.empty()) return false;
    
    // Package name should contain only alphanumeric characters, hyphens, and underscores
    std::regex pattern(R"(^[a-zA-Z0-9_-]+$)");
    return std::regex_match(name, pattern);
}

bool PackageModule::is_valid_version(const std::string& version) {
    if (version.empty()) return false;
    
    // Simple semantic version pattern
    std::regex pattern(R"(^\d+\.\d+\.\d+(-[a-zA-Z0-9.-]+)?$)");
    return std::regex_match(version, pattern);
}

bool PackageModule::version_satisfies_constraint(const std::string& version, const std::string& constraint) {
    // Simplified version constraint checking
    if (constraint.empty() || constraint == "*") return true;
    if (constraint == version) return true;
    
    // In a real implementation, this would handle complex version constraints
    return false;
}

std::string PackageModule::resolve_version_constraint(const std::string& constraint, const std::vector<std::string>& available_versions) {
    if (available_versions.empty()) return "";
    
    // Simple resolution: return the latest version that satisfies the constraint
    for (auto it = available_versions.rbegin(); it != available_versions.rend(); ++it) {
        if (version_satisfies_constraint(*it, constraint)) {
            return *it;
        }
    }
    
    return available_versions.back(); // fallback to latest
}

std::string PackageModule::http_get(const std::string& url, const std::map<std::string, std::string>& headers) {
    // Simulate HTTP GET request
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    return "{\"status\": \"success\", \"data\": \"mock_response\"}";
}

std::string PackageModule::http_post(const std::string& url, const std::string& data, const std::map<std::string, std::string>& headers) {
    // Simulate HTTP POST request
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return "{\"status\": \"success\", \"message\": \"uploaded\"}";
}

bool PackageModule::download_file(const std::string& url, const std::filesystem::path& output_path) {
    // Simulate file download
    try {
        std::ofstream file(output_path, std::ios::binary);
        file << "MOCK_DOWNLOADED_FILE_CONTENT" << std::endl;
        return file.good();
    } catch (const std::exception&) {
        return false;
    }
}

bool PackageModule::extract_archive(const std::filesystem::path& archive_path, const std::filesystem::path& extract_dir) {
    // Simulate archive extraction
    try {
        std::filesystem::create_directories(extract_dir);
        
        // Create some mock extracted files
        std::ofstream file1(extract_dir / "extracted_file1.meld");
        file1 << "// Extracted file 1" << std::endl;
        
        std::ofstream file2(extract_dir / "extracted_file2.meld");
        file2 << "// Extracted file 2" << std::endl;
        
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

std::filesystem::path PackageModule::get_package_install_dir(const std::string& package_name) {
    return global_packages_dir_ / package_name;
}

std::filesystem::path PackageModule::get_package_cache_path(const std::string& package_name, const std::string& version) {
    return package_cache_dir_ / (package_name + "-" + version);
}

bool PackageModule::create_symlink_or_copy(const std::filesystem::path& source, const std::filesystem::path& target) {
    try {
        if (std::filesystem::exists(target)) {
            std::filesystem::remove_all(target);
        }
        
        std::filesystem::create_directories(target.parent_path());
        
        // Try to create symlink, fall back to copy
        std::error_code ec;
        std::filesystem::create_symlink(source, target, ec);
        
        if (ec) {
            // Symlink failed, try copy
            std::filesystem::copy(source, target, std::filesystem::copy_options::recursive, ec);
            return !ec;
        }
        
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

void PackageModule::initialize_default_registries() {
    PackageRegistry default_registry;
    default_registry.name = "default";
    default_registry.url = "https://packages.meld-lang.org";
    default_registry.is_default = true;
    
    registries_.push_back(default_registry);
}

void PackageModule::load_configuration() {
    // Load configuration from file (placeholder)
    // In a real implementation, this would load from a config file
}

void PackageModule::save_configuration() {
    // Save configuration to file (placeholder)
    // In a real implementation, this would save to a config file
}

double PackageModule::calculate_relevance_score(const PackageSearchResult& result, const std::string& query) {
    double score = 0.0;
    
    // Exact name match gets highest score
    if (result.name == query) {
        score += 100.0;
    } else if (result.name.find(query) != std::string::npos) {
        score += 50.0;
    }
    
    // Description match
    if (result.description.find(query) != std::string::npos) {
        score += 20.0;
    }
    
    // Keyword matches
    for (const auto& keyword : result.keywords) {
        if (keyword == query) {
            score += 30.0;
        } else if (keyword.find(query) != std::string::npos) {
            score += 10.0;
        }
    }
    
    return score;
}

std::vector<std::string> PackageModule::tokenize_query(const std::string& query) {
    std::vector<std::string> tokens;
    std::istringstream iss(query);
    std::string token;
    
    while (iss >> token) {
        tokens.push_back(token);
    }
    
    return tokens;
}

bool PackageModule::matches_keyword(const std::string& keyword, const std::string& query_token) {
    return keyword.find(query_token) != std::string::npos;
}

// ---------------------------------------------------------------------------
// meld mod fetch — download Git dependencies declared in meld.toml
// ---------------------------------------------------------------------------

CommandResult PackageModule::handle_mod_fetch_command(const CommandArgs& /*args*/) {
    std::cerr << "error: 'meld mod fetch' is not yet implemented" << std::endl;
    return CommandResult::Error;
}

// ---------------------------------------------------------------------------
// meld mod update — resolve latest versions and rewrite meld.lock
// ---------------------------------------------------------------------------

CommandResult PackageModule::handle_mod_update_command(const CommandArgs& /*args*/) {
    std::cerr << "error: 'meld mod update' is not yet implemented" << std::endl;
    return CommandResult::Error;
}

// ---------------------------------------------------------------------------
// meld mod clean — purge ~/.meld/cache/
// ---------------------------------------------------------------------------

CommandResult PackageModule::handle_mod_clean_command(const CommandArgs& /*args*/) {
    std::cerr << "error: 'meld mod clean' is not yet implemented" << std::endl;
    return CommandResult::Error;
}

// ---------------------------------------------------------------------------
// TOML dependency parsing (minimal — reads [dependencies] section)
// ---------------------------------------------------------------------------

std::vector<std::string> PackageModule::parse_toml_dependencies(
    const std::filesystem::path& /*toml_path*/) {
    return {};
}

// ---------------------------------------------------------------------------
// Lock file I/O
// ---------------------------------------------------------------------------

std::string PackageModule::load_lock_file_model(
    const std::filesystem::path& /*lock_path*/) {
    return {};
}

bool PackageModule::write_lock_file_model(
    const std::string& /*lock*/,
    const std::filesystem::path& /*lock_path*/) {
    return false;
}

} // namespace meld::cli