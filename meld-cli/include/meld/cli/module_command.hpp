#pragma once

#include "command_handler.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace meld::cli {

/// Handles `meld module` — unified package and dependency management.
/// Replaces the former `meld mod` and `meld package` commands.
///
/// Subcommands: clean, fetch, install, list, uninstall, update
///
/// Requirements: 27–33
class ModuleCommandHandler : public BaseCommandHandler {
public:
    ModuleCommandHandler();
    ~ModuleCommandHandler() override = default;

    CommandResult execute(const CommandArgs& args) override;
    std::string get_help() const override;
    std::string get_usage() const override;
    std::vector<std::string> get_completions(const std::string& partial) const override;
    bool validate_args(const CommandArgs& args, std::string& error_message) const override;

private:
    CommandResult handle_clean(bool json_output);
    CommandResult handle_fetch(bool json_output);
    CommandResult handle_install(const std::string& package, bool dev, bool json_output);
    CommandResult handle_list(bool json_output);
    CommandResult handle_uninstall(const std::string& package, bool json_output);
    CommandResult handle_update(const std::string& package, bool json_output);

    /// Path to the dependency cache (~/.meld/cache/)
    std::filesystem::path cache_dir() const;

    /// Path to meld.toml in the current project
    std::filesystem::path toml_path() const;

    /// Path to meld.lock in the current project
    std::filesystem::path lock_path() const;

    /// Run a shell command and capture output
    struct CmdResult { std::string output; int exit_code; };
    CmdResult run_cmd(const std::string& cmd) const;
};

}  // namespace meld::cli
