#include "meld/cli/scaffold_module.hpp"
#include "meld/build/project_template.hpp"
#include <iostream>

namespace meld::cli {

ScaffoldModule::ScaffoldModule()
    : BaseCommandHandler("scaffold", "Create or initialize Meld projects (meld new / meld init)") {
}

CommandResult ScaffoldModule::execute(const CommandArgs& args) {
    if (args.command == "new") {
        return handle_new(args);
    } else if (args.command == "init") {
        return handle_init(args);
    }

    std::cerr << "Unknown scaffold command: " << args.command << std::endl;
    return CommandResult::InvalidArguments;
}

CommandResult ScaffoldModule::handle_new(const CommandArgs& args) {
    if (args.positional.empty()) {
        std::cerr << "error: project name required" << std::endl;
        std::cerr << "Usage: meld new <project_name> [--lib]" << std::endl;
        return CommandResult::InvalidArguments;
    }

    std::string project_name = args.positional[0];
    meld::build::ScaffoldOptions options;
    options.is_lib = has_lib_flag(args);

    meld::build::ProjectTemplate tmpl;
    auto result = tmpl.create_project(project_name, options);

    if (!result.success) {
        std::cerr << result.error_message << std::endl;
        return CommandResult::Error;
    }

    std::cout << "Created project '" << project_name << "'" << std::endl;
    return CommandResult::Success;
}

CommandResult ScaffoldModule::handle_init(const CommandArgs& args) {
    meld::build::ScaffoldOptions options;
    options.is_lib = has_lib_flag(args);

    meld::build::ProjectTemplate tmpl;
    auto result = tmpl.init_project(options);

    if (!result.success) {
        std::cerr << result.error_message << std::endl;
        return CommandResult::Error;
    }

    std::cout << "Initialized Meld project in current directory" << std::endl;
    return CommandResult::Success;
}

bool ScaffoldModule::has_lib_flag(const CommandArgs& args) const {
    return args.flags.count("lib") > 0;
}

std::string ScaffoldModule::get_help() const {
    return R"(Project scaffolding commands:

USAGE:
    meld new <project_name> [--lib]    Create a new project directory
    meld init [--lib]                  Initialize project in current directory

OPTIONS:
    --lib    Create a library project (src/module.meld instead of src/main.meld)

EXAMPLES:
    meld new myapp
    meld new mylib --lib
    meld init)";
}

std::string ScaffoldModule::get_usage() const {
    return "meld new <project_name> [--lib] | meld init [--lib]";
}

std::vector<std::string> ScaffoldModule::get_completions(const std::string& partial) const {
    std::vector<std::string> completions = {"new", "init", "--lib"};
    std::vector<std::string> result;
    for (const auto& c : completions) {
        if (c.find(partial) == 0) {
            result.push_back(c);
        }
    }
    return result;
}

bool ScaffoldModule::validate_args(const CommandArgs& args, std::string& error_message) const {
    if (args.command == "new" && args.positional.empty()) {
        error_message = "Project name required for 'meld new'";
        return false;
    }
    return true;
}

} // namespace meld::cli
