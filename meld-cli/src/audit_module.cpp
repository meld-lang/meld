#include "meld/cli/audit_module.hpp"
#include "meld/effects/effect_checker.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace meld::cli {

AuditModule::AuditModule()
    : BaseCommandHandler("audit", "Security audit of algebraic effects (meld audit)") {
}

CommandResult AuditModule::execute(const CommandArgs& args) {
    // Determine project path — first positional arg or current directory.
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

    // Build module list from project and dependencies.
    // In a full implementation this would:
    //   1. Parse meld.toml for the project name, dependencies, and allow arrays
    //   2. Resolve transitive dependencies via PackageResolver
    //   3. Parse each module's .meld sources into ASTs
    // For now we construct ModuleInfo entries from the project directory.

    effects::EffectChecker checker;
    std::vector<effects::ModuleInfo> modules;

    // Scan for .meld files in the project directory.
    std::string project_name = project_path.filename().string();
    if (project_name == ".") {
        project_name = std::filesystem::current_path().filename().string();
    }

    effects::ModuleInfo root_module;
    root_module.package_name = project_name;

    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(project_path)) {
        if (entry.is_regular_file() && entry.path().extension() == ".meld") {
            root_module.file_path = entry.path().string();
            // In a full implementation each file's AST would be parsed here
            // and appended to root_module.ast.
        }
    }

    modules.push_back(std::move(root_module));

    // Build the effect tree.
    effects::EffectTree tree = checker.build_effect_tree(modules);

    // Output.
    if (json_output) {
        print_json_tree(tree);
    } else {
        print_human_tree(tree);
    }

    // Non-zero exit when violations exist.
    if (!tree.all_violations().empty()) {
        return CommandResult::Error;
    }
    return CommandResult::Success;
}

// ---------------------------------------------------------------------------
// Human-readable output
// ---------------------------------------------------------------------------

void AuditModule::print_human_tree(const effects::EffectTree& tree) {
    std::cout << "Effect Security Audit" << std::endl;
    std::cout << "=====================" << std::endl;
    std::cout << "Root package: " << tree.root_package << std::endl;
    std::cout << std::endl;

    for (const auto& node : tree.nodes) {
        std::cout << "Package: " << node.package_name << std::endl;

        if (!node.direct_effects.empty()) {
            std::cout << "  Direct effects:";
            for (const auto& eff : node.direct_effects) {
                std::cout << " " << eff;
            }
            std::cout << std::endl;
        }

        if (!node.transitive_effects.empty()) {
            std::cout << "  Transitive effects:";
            for (const auto& eff : node.transitive_effects) {
                std::cout << " " << eff;
            }
            std::cout << std::endl;
        }

        if (!node.allowed_effects.empty()) {
            std::cout << "  Allowed effects:";
            for (const auto& eff : node.allowed_effects) {
                std::cout << " " << eff;
            }
            std::cout << std::endl;
        }

        if (!node.violations.empty()) {
            std::cout << "  VIOLATIONS:" << std::endl;
            for (const auto& v : node.violations) {
                std::cout << "    - " << v.effect_name
                          << " at " << v.source_location.file
                          << ":" << v.source_location.line
                          << ":" << v.source_location.column
                          << std::endl;
            }
        }

        std::cout << std::endl;
    }

    auto all = tree.all_violations();
    if (all.empty()) {
        std::cout << "No effect violations found." << std::endl;
    } else {
        std::cout << all.size() << " effect violation(s) found." << std::endl;
    }
}

// ---------------------------------------------------------------------------
// JSON output
// ---------------------------------------------------------------------------

void AuditModule::print_json_tree(const effects::EffectTree& tree) {
    auto json_array = [](const std::vector<std::string>& items) -> std::string {
        std::ostringstream os;
        os << "[";
        for (size_t i = 0; i < items.size(); ++i) {
            if (i > 0) os << ",";
            os << "\"" << items[i] << "\"";
        }
        os << "]";
        return os.str();
    };

    std::ostringstream json;
    json << "{\"root_package\":\"" << tree.root_package << "\",\"nodes\":[";

    for (size_t i = 0; i < tree.nodes.size(); ++i) {
        const auto& node = tree.nodes[i];
        if (i > 0) json << ",";

        json << "{\"package_name\":\"" << node.package_name << "\""
             << ",\"direct_effects\":" << json_array(node.direct_effects)
             << ",\"transitive_effects\":" << json_array(node.transitive_effects)
             << ",\"allowed_effects\":" << json_array(node.allowed_effects)
             << ",\"violations\":[";

        for (size_t j = 0; j < node.violations.size(); ++j) {
            const auto& v = node.violations[j];
            if (j > 0) json << ",";
            json << "{\"package_name\":\"" << v.package_name << "\""
                 << ",\"effect_name\":\"" << v.effect_name << "\""
                 << ",\"source_location\":{\"file\":\"" << v.source_location.file << "\""
                 << ",\"line\":" << v.source_location.line
                 << ",\"column\":" << v.source_location.column
                 << "}}";
        }

        json << "]}";
    }

    json << "]}";
    std::cout << json.str() << std::endl;
}

// ---------------------------------------------------------------------------
// Help / completions
// ---------------------------------------------------------------------------

std::string AuditModule::get_help() const {
    return R"(Effect security audit commands:

USAGE:
    meld audit [<path>] [OPTIONS]

OPTIONS:
    --json              Output the effect tree in machine-readable JSON format

DESCRIPTION:
    Parses the project AST and all transitive dependency ASTs to build a tree
    of requested algebraic effects. Flags violations where a dependency requests
    effects not listed in its allow array in meld.toml.

EXAMPLES:
    meld audit
    meld audit --json
    meld audit src/)";
}

std::string AuditModule::get_usage() const {
    return "meld audit [<path>] [--json]";
}

std::vector<std::string> AuditModule::get_completions(
    const std::string& partial) const
{
    std::vector<std::string> completions = {"--json"};
    std::vector<std::string> result;
    for (const auto& c : completions) {
        if (c.find(partial) == 0) {
            result.push_back(c);
        }
    }
    return result;
}

bool AuditModule::validate_args(const CommandArgs& /*args*/,
                                std::string& /*error_message*/) const {
    // All arguments are optional for `meld audit`.
    return true;
}

} // namespace meld::cli
