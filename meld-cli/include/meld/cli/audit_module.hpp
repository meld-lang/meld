#pragma once

#include "command_handler.hpp"
#include <string>
#include <vector>

namespace meld::effects {
    struct EffectTree;
}

namespace meld::cli {

/**
 * Handles the `meld audit` subcommand.
 * Builds an effect tree for the project and its transitive dependencies,
 * then displays it in human-readable or JSON format.
 *
 * Requirements: 10.1, 10.2, 10.3
 */
class AuditModule : public BaseCommandHandler {
public:
    AuditModule();
    ~AuditModule() = default;

    // CommandHandler interface
    CommandResult execute(const CommandArgs& args) override;
    std::string get_help() const override;
    std::string get_usage() const override;
    std::vector<std::string> get_completions(const std::string& partial) const override;
    bool validate_args(const CommandArgs& args, std::string& error_message) const override;

private:
    /// Print the effect tree in human-readable format.
    void print_human_tree(const effects::EffectTree& tree);

    /// Print the effect tree as JSON.
    void print_json_tree(const effects::EffectTree& tree);
};

} // namespace meld::cli
