#include "meld/cli/test_module.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace meld::cli {

TestModule::TestModule()
    : BaseCommandHandler("test", "Discover and run tests (meld test)") {
}

CommandResult TestModule::execute(const CommandArgs& args) {
    // The test framework has moved to a pure Meld library (std.test).
    // Use: meld run <test_file.meld> to run tests directly.
    std::filesystem::path project_path = ".";
    if (!args.positional.empty()) {
        project_path = args.positional[0];
    }

    if (!std::filesystem::exists(project_path)) {
        std::cerr << "error: path not found: " << project_path.string()
                  << std::endl;
        return CommandResult::Error;
    }

    bool json_output = args.flags.count("json") > 0;

    // Discover .meld files with test blocks
    std::vector<std::filesystem::path> test_files;
    std::error_code ec;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(project_path, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".meld") {
            // Check if file contains test blocks
            std::ifstream ifs(entry.path());
            std::string content((std::istreambuf_iterator<char>(ifs)),
                                 std::istreambuf_iterator<char>());
            if (content.find("test ") != std::string::npos ||
                content.find("@test") != std::string::npos) {
                test_files.push_back(entry.path());
            }
        }
    }

    if (test_files.empty()) {
        if (json_output) {
            std::cout << R"({"tests":[],"passed":0,"failed":0,"skipped":0,"duration_ms":0})"
                      << std::endl;
        } else {
            std::cout << "No tests found." << std::endl;
        }
        return CommandResult::Success;
    }

    if (json_output) {
        std::cout << "{\"tests\":[],\"discovered\":" << test_files.size()
                  << ",\"message\":\"Test runner migrated to std.test. Use meld run <test_file.meld>.\"}"
                  << std::endl;
    } else {
        std::cout << "Discovered " << test_files.size() << " test file(s)." << std::endl;
        std::cout << "Note: Test runner has migrated to std.test." << std::endl;
        std::cout << "Run tests with: meld run <test_file.meld>" << std::endl;
        for (const auto& f : test_files) {
            std::cout << "  " << f.string() << std::endl;
        }
    }

    return CommandResult::Success;
}

std::string TestModule::get_help() const {
    return R"(Testing framework commands:

USAGE:
    meld test [<path>] [OPTIONS]

OPTIONS:
    --filter <regex>    Only run tests matching the regex
    --json              Output results in JSON format

EXAMPLES:
    meld test
    meld test src/
    meld test --filter "test_math.*"
    meld test --json)";
}

std::string TestModule::get_usage() const {
    return "meld test [<path>] [--filter <regex>] [--json]";
}

std::vector<std::string> TestModule::get_completions(
    const std::string& partial) const
{
    std::vector<std::string> completions = {"--filter", "--json"};
    std::vector<std::string> result;
    for (const auto& c : completions) {
        if (c.find(partial) == 0) {
            result.push_back(c);
        }
    }
    return result;
}

bool TestModule::validate_args(const CommandArgs& /*args*/,
                               std::string& /*error_message*/) const {
    return true;
}

} // namespace meld::cli
