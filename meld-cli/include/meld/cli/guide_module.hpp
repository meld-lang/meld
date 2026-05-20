#pragma once

#include "command_handler.hpp"
#include <string>
#include <vector>

namespace meld::cli {

/**
 * Handles the `meld guide` subcommand.
 * Emits version-matched agent guidance directly from the CLI binary.
 * Agents use this instead of scraping external documentation.
 *
 * Usage:
 *   meld guide                   List available topics
 *   meld guide syntax            Get syntax guidance
 *   meld guide all               Get all guidance (full dump)
 *   meld guide all --json        Structured JSON for agents
 */
class GuideModule : public BaseCommandHandler {
public:
    GuideModule();
    ~GuideModule() = default;

    CommandResult execute(const CommandArgs& args) override;
    std::string get_help() const override;
    std::string get_usage() const override;
    std::vector<std::string> get_completions(const std::string& partial) const override;
    bool validate_args(const CommandArgs& args, std::string& error_message) const override;

private:
    void print_topics() const;
    void print_topic(const std::string& topic, bool json) const;
    void print_all(bool json) const;

    static const char* version();
};

} // namespace meld::cli
