#pragma once

#include "command_handler.hpp"
#include <string>
#include <vector>

namespace meld::cli {

/**
 * Handles the `meld doctor` subcommand.
 * Checks the development environment and reports issues.
 *
 * Usage:
 *   meld doctor             Human-readable report
 *   meld doctor --json      Structured JSON for agents
 */
class DoctorModule : public BaseCommandHandler {
public:
    DoctorModule();
    ~DoctorModule() = default;

    CommandResult execute(const CommandArgs& args) override;
    std::string get_help() const override;
    std::string get_usage() const override;
    std::vector<std::string> get_completions(const std::string& partial) const override;
    bool validate_args(const CommandArgs& args, std::string& error_message) const override;
};

} // namespace meld::cli
