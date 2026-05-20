#pragma once

#include "command_handler.hpp"
#include <string>
#include <vector>

namespace meld::cli {

/**
 * Handles the `meld test` subcommand.
 * Discovers and runs @test-annotated functions via meld::test::TestRunner.
 */
class TestModule : public BaseCommandHandler {
public:
    TestModule();
    ~TestModule() = default;

    // CommandHandler interface
    CommandResult execute(const CommandArgs& args) override;
    std::string get_help() const override;
    std::string get_usage() const override;
    std::vector<std::string> get_completions(const std::string& partial) const override;
    bool validate_args(const CommandArgs& args, std::string& error_message) const override;

private:
    /// Format results as human-readable text and print to stdout.
    void print_human_results(const struct TestRunOutput& output);

    /// Format results as JSON and print to stdout.
    void print_json_results(const struct TestRunOutput& output);
};

} // namespace meld::cli
