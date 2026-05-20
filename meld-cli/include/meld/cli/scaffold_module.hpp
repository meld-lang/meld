#pragma once

#include "command_handler.hpp"
#include <memory>
#include <string>
#include <vector>

namespace meld::cli {

/**
 * Handles top-level `meld new <name>` and `meld init` commands.
 * Delegates to meld::build::ProjectTemplate for actual scaffolding.
 */
class ScaffoldModule : public BaseCommandHandler {
public:
    ScaffoldModule();
    ~ScaffoldModule() = default;

    // CommandHandler interface
    CommandResult execute(const CommandArgs& args) override;
    std::string get_help() const override;
    std::string get_usage() const override;
    std::vector<std::string> get_completions(const std::string& partial) const override;
    bool validate_args(const CommandArgs& args, std::string& error_message) const override;

private:
    CommandResult handle_new(const CommandArgs& args);
    CommandResult handle_init(const CommandArgs& args);

    bool has_lib_flag(const CommandArgs& args) const;
};

} // namespace meld::cli
