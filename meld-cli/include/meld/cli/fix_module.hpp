#pragma once

#include "command_handler.hpp"
#include <string>
#include <vector>

namespace meld::cli {

/**
 * Handles the `meld fix` subcommand.
 * Compiles a file, collects diagnostics with fix suggestions,
 * and emits a structured fix plan that agents can apply directly.
 *
 * Usage:
 *   meld fix --plan program.meld          Human-readable fix plan
 *   meld fix --plan --json program.meld   Machine-readable JSON fix plan
 */
class FixModule : public BaseCommandHandler {
public:
    FixModule();
    ~FixModule() = default;

    CommandResult execute(const CommandArgs& args) override;
    std::string get_help() const override;
    std::string get_usage() const override;
    std::vector<std::string> get_completions(const std::string& partial) const override;
    bool validate_args(const CommandArgs& args, std::string& error_message) const override;

private:
    void print_plan_human(const std::string& file,
                          const std::vector<struct FixAction>& actions) const;
    void print_plan_json(const std::string& file,
                         const std::vector<struct FixAction>& actions) const;
};

} // namespace meld::cli
