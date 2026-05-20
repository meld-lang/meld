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
 * Package metadata information
 */
struct PackageMetadata {
    std::string name;
    std::string version;
    std::string description;
    std::string author;
    std::string license;
    std::vector<std::string> keywords;
    std::string homepage;
    std::string repository;
    std::map<std::string, std::string> extra_fields;
};

/**
 * Package dependency specification
 */
struct PackageDependency {
    std::string name;
    std::string version_constraint; // e.g., ">=1.0.0", "~1.2.0", "^2.0.0"
    bool is_dev_dependency = false;
    std::string source = "registry"; // registry, git, local
    std::string source_url;
    std::map<std::string, std::string> metadata;
};

/**
 * Package installation result
 */
struct PackageInstallResult {
    bool success;
    std::string package_name;
    std::string installed_version;
    std::filesystem::path install_path;
    std::vector<std::string> installed_files;
    std::vector<PackageDependency> resolved_dependencies;
    std::string error_message;
};

/**
 * Package search result
 */
struct PackageSearchResult {
    std::string name;
    std::string version;
    std::string description;
    std::vector<std::string> keywords;
    double relevance_score;
    std::string registry_url;
};

/**
 * Package registry configuration
 */
struct PackageRegistry {
    std::string name;
    std::string url;
    std::string auth_token;
    bool is_default = false;
    std::map<std::string, std::string> headers;
};

/**
 * Dependency conflict information
 */
struct DependencyConflict {
    std::string package_name;
    std::vector<std::string> conflicting_versions;
    std::vector<std::string> required_by;
    std::string resolution_strategy;
};

/**
 * Native dependency specification
 */
struct NativeDependency {
    std::string name;
    std::string platform; // linux, windows, macos, etc.
    std::string architecture; // x86_64, arm64, etc.
    std::string package_manager; // apt, brew, vcpkg, etc.
    std::string install_command;
    std::vector<std::string> library_paths;
    std::vector<std::string> include_paths;
};

/**
 * Package management module for installation, publishing, and dependency management
 */
class PackageModule : public BaseCommandHandler {
public:
    explicit PackageModule(std::shared_ptr<ErrorHandler> error_handler);
    ~PackageModule() = default;

    // CommandHandler interface
    CommandResult execute(const CommandArgs& args) override;
    std::string get_help() const override;
    std::string get_usage() const override;
    std::vector<std::string> get_completions(const std::string& partial) const override;
    bool validate_args(const CommandArgs& args, std::string& error_message) const override;

    // Package installation
    PackageInstallResult install_package(const std::string& package_spec, bool is_dev_dependency = false);
    PackageInstallResult install_package_from_registry(const std::string& name, const std::string& version, const PackageRegistry& registry);
    PackageInstallResult install_package_from_git(const std::string& git_url, const std::string& ref = "main");
    PackageInstallResult install_local_package(const std::filesystem::path& package_path);
    bool uninstall_package(const std::string& package_name);

    // Package publishing
    bool publish_package(const std::filesystem::path& package_dir, const PackageRegistry& registry);
    bool create_package_archive(const std::filesystem::path& package_dir, const std::filesystem::path& output_path);
    bool validate_package_metadata(const PackageMetadata& metadata, std::vector<std::string>& errors);
    bool upload_package_to_registry(const std::filesystem::path& package_archive, const PackageRegistry& registry);

    // Package search
    std::vector<PackageSearchResult> search_packages(const std::string& query, const std::vector<PackageRegistry>& registries = {});
    std::vector<PackageSearchResult> search_registry(const std::string& query, const PackageRegistry& registry);
    std::vector<PackageSearchResult> rank_search_results(const std::vector<PackageSearchResult>& results, const std::string& query);

    // Dependency management
    std::vector<PackageDependency> resolve_dependencies(const std::vector<PackageDependency>& dependencies);
    std::vector<DependencyConflict> detect_conflicts(const std::vector<PackageDependency>& dependencies);
    std::vector<PackageDependency> resolve_conflicts(const std::vector<DependencyConflict>& conflicts);
    bool update_lock_file(const std::vector<PackageDependency>& resolved_dependencies);
    std::vector<PackageDependency> load_lock_file();

    // Development dependencies
    bool install_dev_dependencies(const std::filesystem::path& project_dir);
    bool remove_dev_dependencies(const std::filesystem::path& project_dir);
    std::vector<PackageDependency> get_dev_dependencies(const std::filesystem::path& project_dir);

    // Native dependencies
    bool install_native_dependency(const NativeDependency& native_dep);
    std::vector<NativeDependency> detect_native_dependencies(const std::filesystem::path& project_dir);
    bool configure_native_dependency_paths(const NativeDependency& native_dep, const std::filesystem::path& project_dir);
    std::string detect_platform();
    std::string detect_architecture();
    std::string detect_package_manager();

    // Registry management
    void add_registry(const PackageRegistry& registry);
    void remove_registry(const std::string& registry_name);
    std::vector<PackageRegistry> get_registries() const;
    PackageRegistry get_default_registry() const;
    void set_default_registry(const std::string& registry_name);

    // Package metadata
    std::optional<PackageMetadata> load_package_metadata(const std::filesystem::path& package_dir);
    bool save_package_metadata(const PackageMetadata& metadata, const std::filesystem::path& package_dir);
    std::optional<PackageMetadata> fetch_package_metadata(const std::string& package_name, const PackageRegistry& registry);

private:
    std::shared_ptr<ErrorHandler> error_handler_;
    std::vector<PackageRegistry> registries_;
    std::filesystem::path package_cache_dir_;
    std::filesystem::path global_packages_dir_;

    // meld mod subcommand handlers (Git-first package management)
    CommandResult handle_mod_fetch_command(const CommandArgs& args);
    CommandResult handle_mod_update_command(const CommandArgs& args);
    CommandResult handle_mod_clean_command(const CommandArgs& args);

    // Helper: parse meld.toml [dependencies] into Dependency list
    std::vector<std::string> parse_toml_dependencies(const std::filesystem::path& toml_path);

    // Helper: load meld.lock into LockFile model
    std::string load_lock_file_model(const std::filesystem::path& lock_path);

    // Helper: write LockFile model to meld.lock
    bool write_lock_file_model(const std::string& lock, const std::filesystem::path& lock_path);

    // Command handlers (registry-based, existing)
    CommandResult handle_install_command(const CommandArgs& args);
    CommandResult handle_uninstall_command(const CommandArgs& args);
    CommandResult handle_publish_command(const CommandArgs& args);
    CommandResult handle_search_command(const CommandArgs& args);
    CommandResult handle_update_command(const CommandArgs& args);
    CommandResult handle_list_command(const CommandArgs& args);
    CommandResult handle_registry_command(const CommandArgs& args);

    // Helper methods
    std::string parse_package_spec(const std::string& spec, std::string& name, std::string& version);
    bool is_valid_package_name(const std::string& name);
    bool is_valid_version(const std::string& version);
    bool version_satisfies_constraint(const std::string& version, const std::string& constraint);
    std::string resolve_version_constraint(const std::string& constraint, const std::vector<std::string>& available_versions);
    
    std::string http_get(const std::string& url, const std::map<std::string, std::string>& headers = {});
    std::string http_post(const std::string& url, const std::string& data, const std::map<std::string, std::string>& headers = {});
    bool download_file(const std::string& url, const std::filesystem::path& output_path);
    bool extract_archive(const std::filesystem::path& archive_path, const std::filesystem::path& extract_dir);
    
    std::filesystem::path get_package_install_dir(const std::string& package_name);
    std::filesystem::path get_package_cache_path(const std::string& package_name, const std::string& version);
    bool create_symlink_or_copy(const std::filesystem::path& source, const std::filesystem::path& target);
    
    void initialize_default_registries();
    void load_configuration();
    void save_configuration();
    
    double calculate_relevance_score(const PackageSearchResult& result, const std::string& query);
    std::vector<std::string> tokenize_query(const std::string& query);
    bool matches_keyword(const std::string& keyword, const std::string& query_token);
};

} // namespace meld::cli