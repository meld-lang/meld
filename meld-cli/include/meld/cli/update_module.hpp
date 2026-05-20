#pragma once

#include "command_handler.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace meld::cli {

/// Handles `meld update` — self-update the meld toolchain.
///
/// Default: download pre-built binaries from GitHub releases.
/// --from-source: git pull + bazel build (developer workflow).
class UpdateModule : public BaseCommandHandler {
public:
    UpdateModule();
    ~UpdateModule() override = default;

    CommandResult execute(const CommandArgs& args) override;
    std::string get_help() const override;
    std::string get_usage() const override;
    std::vector<std::string> get_completions(const std::string& partial) const override;
    bool validate_args(const CommandArgs& args, std::string& error_message) const override;

private:
    /// Download and install the latest release from GitHub.
    CommandResult update_from_release(bool check_only, bool json_output);

    /// Git pull + bazel build in the source tree.
    CommandResult update_from_source(bool json_output);

    /// Get the workspace root (for --from-source).
    std::filesystem::path find_workspace_root() const;

    /// Get the current version string.
    std::string current_version() const;

    /// Run a shell command and capture output.
    struct CmdResult { std::string output; int exit_code; };
    CmdResult run_cmd(const std::string& cmd) const;
};

}  // namespace meld::cli
