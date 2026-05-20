#pragma once

#include "command_handler.hpp"
#include <string>
#include <vector>

namespace meld::cli {

/**
 * Handles the `meld check` subcommand.
 * Parses and type-checks a file without executing it.
 * JSON output by default for agent consumption.
 */
class CheckModule : public BaseCommandHandler {
public:
    CheckModule();
    ~CheckModule() = default;

    CommandResult execute(const CommandArgs& args) override;
    std::string get_help() const override;
    std::string get_usage() const override;
    std::vector<std::string> get_completions(const std::string& partial) const override;
    bool validate_args(const CommandArgs& args, std::string& error_message) const override;
};

} // namespace meld::cli
