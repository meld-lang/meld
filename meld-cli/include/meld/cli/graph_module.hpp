#pragma once

#include "command_handler.hpp"
#include <string>
#include <vector>

namespace meld::cli {

/**
 * Handles the `meld graph` subcommand.
 * Scans a file or directory for imp statements and emits
 * the module dependency graph as structured output.
 *
 * Usage:
 *   meld graph src/main.meld         Human-readable graph
 *   meld graph --json src/            JSON module graph
 */
class GraphModule : public BaseCommandHandler {
public:
    GraphModule();
    ~GraphModule() = default;

    CommandResult execute(const CommandArgs& args) override;
    std::string get_help() const override;
    std::string get_usage() const override;
    std::vector<std::string> get_completions(const std::string& partial) const override;
    bool validate_args(const CommandArgs& args, std::string& error_message) const override;
};

} // namespace meld::cli
