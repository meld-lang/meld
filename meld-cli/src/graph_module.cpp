#include "meld/cli/graph_module.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <regex>
#include <map>
#include <set>
#include <algorithm>

namespace meld::cli {

namespace fs = std::filesystem;

struct ModuleNode {
    std::string file;
    std::vector<std::string> imports;
};

static std::vector<std::string> extract_imports(const fs::path& file) {
    std::vector<std::string> imports;
    std::ifstream in(file);
    if (!in) return imports;

    std::regex imp_regex(R"(^\s*imp\s+(\S+))");
    std::string line;
    while (std::getline(in, line)) {
        std::smatch match;
        if (std::regex_search(line, match, imp_regex)) {
            imports.push_back(match[1].str());
        }
    }
    return imports;
}

static std::vector<ModuleNode> scan_path(const fs::path& path) {
    std::vector<ModuleNode> nodes;

    if (fs::is_regular_file(path) && path.extension() == ".meld") {
        nodes.push_back({path.string(), extract_imports(path)});
    } else if (fs::is_directory(path)) {
        for (const auto& entry : fs::recursive_directory_iterator(path)) {
            if (entry.is_regular_file() && entry.path().extension() == ".meld") {
                nodes.push_back({entry.path().string(), extract_imports(entry.path())});
            }
        }
    }

    std::sort(nodes.begin(), nodes.end(),
              [](const auto& a, const auto& b) { return a.file < b.file; });
    return nodes;
}

// ═══════════════════════════════════════════════════════════════════════════

GraphModule::GraphModule()
    : BaseCommandHandler("graph", "Show module dependency graph") {}

CommandResult GraphModule::execute(const CommandArgs& args) {
    // JSON is the default output; --text for human-readable
    bool text = args.flags.count("text") > 0 || args.options.count("text") > 0;
    std::string target;
    for (const auto& arg : args.positional) {
        if (arg == "--text") { text = true; continue; }
        if (arg.substr(0, 2) != "--") { target = arg; }
    }
    bool json = !text;

    if (target.empty()) {
        target = ".";
    }

    if (!fs::exists(target)) {
        std::cerr << "Path not found: " << target << "\n";
        return CommandResult::Error;
    }

    auto nodes = scan_path(fs::path(target));

    if (json) {
        std::cout << "{\n";
        std::cout << "  \"root\": \"" << target << "\",\n";
        std::cout << "  \"modules\": [\n";
        for (size_t i = 0; i < nodes.size(); ++i) {
            if (i > 0) std::cout << ",\n";
            std::cout << "    {\"file\": \"" << nodes[i].file << "\", \"imports\": [";
            for (size_t j = 0; j < nodes[i].imports.size(); ++j) {
                if (j > 0) std::cout << ", ";
                std::cout << "\"" << nodes[i].imports[j] << "\"";
            }
            std::cout << "]}";
        }
        std::cout << "\n  ]\n}\n";
    } else {
        std::cout << "Module graph for " << target << " (" << nodes.size() << " files):\n\n";
        for (const auto& node : nodes) {
            std::cout << "  " << node.file;
            if (node.imports.empty()) {
                std::cout << " (no imports)\n";
            } else {
                std::cout << "\n";
                for (const auto& imp : node.imports) {
                    std::cout << "    -> " << imp << "\n";
                }
            }
        }
    }
    return CommandResult::Success;
}

std::string GraphModule::get_help() const {
    return R"(Usage: meld graph [path] [--text]

Show the module dependency graph for a file or directory.
Output is JSON by default. Use --text for human-readable.

Arguments:
  path       File or directory to scan (defaults to current directory)

Flags:
  --text     Output as human-readable plain text

Examples:
  meld graph src/main.meld         JSON graph (default)
  meld graph --text src/           Human-readable graph
  meld graph .                     Full project graph as JSON)";
}

std::string GraphModule::get_usage() const {
    return "meld graph [path] [--text]";
}

std::vector<std::string> GraphModule::get_completions(const std::string& partial) const {
    return {"--text"};
}

bool GraphModule::validate_args(const CommandArgs& args, std::string& error_message) const {
    return true;
}

} // namespace meld::cli
