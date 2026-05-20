#include "meld/cli/module_command.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace meld::cli {

// -----------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------

ModuleCommandHandler::ModuleCommandHandler()
    : BaseCommandHandler("module",
                         "Manage packages and dependencies (install, uninstall, list, update, fetch, clean)") {}

// -----------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------

ModuleCommandHandler::CmdResult ModuleCommandHandler::run_cmd(const std::string& cmd) const {
    CmdResult r;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) { r.exit_code = -1; return r; }
    std::array<char, 256> buf{};
    while (fgets(buf.data(), static_cast<int>(buf.size()), pipe)) {
        r.output += buf.data();
    }
    int status = pclose(pipe);
#ifndef _WIN32
    r.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#else
    r.exit_code = status;
#endif
    return r;
}

std::filesystem::path ModuleCommandHandler::cache_dir() const {
    const char* home = std::getenv("HOME");
    if (!home) home = "/tmp";
    return std::filesystem::path(home) / ".meld" / "cache";
}

std::filesystem::path ModuleCommandHandler::toml_path() const {
    return std::filesystem::current_path() / "meld.toml";
}

std::filesystem::path ModuleCommandHandler::lock_path() const {
    return std::filesystem::current_path() / "meld.lock";
}

// -----------------------------------------------------------------------
// execute — route to subcommand
// -----------------------------------------------------------------------

CommandResult ModuleCommandHandler::execute(const CommandArgs& args) {
    std::string subcmd;
    if (!args.positional.empty()) {
        subcmd = args.positional[0];
    }

    bool json_output = args.flags.count("json") > 0 || args.options.count("json") > 0;

    if (subcmd.empty()) {
        std::cout << get_help() << std::endl;
        return CommandResult::Success;
    }

    if (subcmd == "clean") {
        return handle_clean(json_output);
    }
    if (subcmd == "fetch") {
        return handle_fetch(json_output);
    }
    if (subcmd == "install") {
        std::string package;
        if (args.positional.size() >= 2) {
            package = args.positional[1];
        }
        if (package.empty()) {
            std::cerr << "error: package name required for install" << std::endl;
            return CommandResult::InvalidArguments;
        }
        bool dev = args.flags.count("dev") > 0 || args.options.count("dev") > 0;
        return handle_install(package, dev, json_output);
    }
    if (subcmd == "list") {
        return handle_list(json_output);
    }
    if (subcmd == "uninstall") {
        std::string package;
        if (args.positional.size() >= 2) {
            package = args.positional[1];
        }
        if (package.empty()) {
            std::cerr << "error: package name required for uninstall" << std::endl;
            return CommandResult::InvalidArguments;
        }
        return handle_uninstall(package, json_output);
    }
    if (subcmd == "update") {
        std::string package;
        if (args.positional.size() >= 2) {
            package = args.positional[1];
        }
        return handle_update(package, json_output);
    }

    std::cerr << "error: unknown subcommand '" << subcmd
              << "' — valid subcommands: clean, fetch, install, list, uninstall, update"
              << std::endl;
    return CommandResult::InvalidArguments;
}

// -----------------------------------------------------------------------
// Subcommand implementations
// -----------------------------------------------------------------------

CommandResult ModuleCommandHandler::handle_clean(bool json_output) {
    auto dir = cache_dir();
    std::error_code ec;
    if (std::filesystem::exists(dir)) {
        std::filesystem::remove_all(dir, ec);
        if (ec) {
            std::cerr << "error: failed to clean cache: " << ec.message() << std::endl;
            return CommandResult::Error;
        }
    }
    if (json_output) {
        std::cout << "{\"cleaned\":true,\"cache\":\"" << dir.string() << "\"}" << std::endl;
    } else {
        std::cout << "Dependency cache cleaned: " << dir.string() << std::endl;
    }
    return CommandResult::Success;
}

CommandResult ModuleCommandHandler::handle_fetch(bool json_output) {
    auto toml = toml_path();
    if (!std::filesystem::exists(toml)) {
        std::cerr << "error: meld.toml not found in current directory" << std::endl;
        return CommandResult::Error;
    }

    // Parse [dependencies] from meld.toml (simple line-based parser)
    std::ifstream file(toml);
    std::string line;
    bool in_deps = false;
    std::vector<std::pair<std::string, std::string>> deps;

    while (std::getline(file, line)) {
        auto trimmed = line;
        auto start = trimmed.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        trimmed = trimmed.substr(start);

        if (trimmed.starts_with("[")) {
            in_deps = (trimmed.find("[dependencies]") != std::string::npos);
            continue;
        }

        if (in_deps) {
            auto eq = trimmed.find('=');
            if (eq != std::string::npos) {
                auto name = trimmed.substr(0, eq);
                auto val = trimmed.substr(eq + 1);
                // Trim
                auto trim = [](std::string& s) {
                    auto a = s.find_first_not_of(" \t\"'");
                    auto b = s.find_last_not_of(" \t\"'");
                    s = (a != std::string::npos) ? s.substr(a, b - a + 1) : "";
                };
                trim(name);
                trim(val);
                if (!name.empty()) {
                    deps.emplace_back(name, val);
                }
            }
        }
    }

    if (deps.empty()) {
        if (json_output) {
            std::cout << "{\"fetched\":0,\"dependencies\":[]}" << std::endl;
        } else {
            std::cout << "No dependencies declared in meld.toml" << std::endl;
        }
        return CommandResult::Success;
    }

    auto dir = cache_dir();
    std::filesystem::create_directories(dir);

    int fetched = 0;
    for (const auto& [name, url] : deps) {
        auto dest = dir / name;
        if (!json_output) {
            std::cout << "Fetching " << name << " (" << url << ")..." << std::endl;
        }

        if (std::filesystem::exists(dest)) {
            // Pull latest
            auto r = run_cmd("git -C " + dest.string() + " pull --ff-only 2>&1");
            if (r.exit_code != 0) {
                std::cerr << "error: failed to update " << name << ": " << r.output;
                continue;
            }
        } else {
            // Clone
            auto r = run_cmd("git clone " + url + " " + dest.string() + " 2>&1");
            if (r.exit_code != 0) {
                std::cerr << "error: failed to fetch " << name << ": " << r.output;
                continue;
            }
        }
        ++fetched;
    }

    if (json_output) {
        std::cout << "{\"fetched\":" << fetched << ",\"total\":" << deps.size() << "}" << std::endl;
    } else {
        std::cout << "Fetched " << fetched << " of " << deps.size() << " dependencies" << std::endl;
    }
    return CommandResult::Success;
}

CommandResult ModuleCommandHandler::handle_install(const std::string& package,
                                                    bool dev, bool json_output) {
    auto toml = toml_path();
    if (!std::filesystem::exists(toml)) {
        std::cerr << "error: meld.toml not found — run 'meld new' or 'meld init' first" << std::endl;
        return CommandResult::Error;
    }

    // Append to [dependencies] or [dev-dependencies] in meld.toml
    std::string section = dev ? "[dev-dependencies]" : "[dependencies]";

    // Check if already installed
    std::ifstream check(toml);
    std::string content((std::istreambuf_iterator<char>(check)),
                         std::istreambuf_iterator<char>());
    check.close();

    if (content.find(package) != std::string::npos) {
        if (json_output) {
            std::cout << "{\"installed\":false,\"reason\":\"already installed\",\"package\":\""
                      << package << "\"}" << std::endl;
        } else {
            std::cout << package << " is already in " << section << std::endl;
        }
        return CommandResult::Success;
    }

    // Ensure section exists, then append
    if (content.find(section) == std::string::npos) {
        content += "\n" + section + "\n";
    }

    // Insert after the section header
    auto pos = content.find(section);
    auto insert_pos = content.find('\n', pos);
    if (insert_pos != std::string::npos) {
        content.insert(insert_pos + 1, package + " = \"*\"\n");
    }

    std::ofstream out(toml);
    out << content;
    out.close();

    if (json_output) {
        std::cout << "{\"installed\":true,\"package\":\"" << package
                  << "\",\"version\":\"*\",\"dev\":" << (dev ? "true" : "false") << "}" << std::endl;
    } else {
        std::cout << "Installed " << package << " to " << section << std::endl;
    }
    return CommandResult::Success;
}

CommandResult ModuleCommandHandler::handle_list(bool json_output) {
    auto toml = toml_path();
    if (!std::filesystem::exists(toml)) {
        std::cerr << "error: meld.toml not found" << std::endl;
        return CommandResult::Error;
    }

    std::ifstream file(toml);
    std::string line;
    bool in_deps = false;
    std::vector<std::pair<std::string, std::string>> deps;

    while (std::getline(file, line)) {
        auto trimmed = line;
        auto start = trimmed.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        trimmed = trimmed.substr(start);

        if (trimmed.starts_with("[")) {
            in_deps = (trimmed.find("[dependencies]") != std::string::npos ||
                       trimmed.find("[dev-dependencies]") != std::string::npos);
            continue;
        }

        if (in_deps) {
            auto eq = trimmed.find('=');
            if (eq != std::string::npos) {
                auto name = trimmed.substr(0, eq);
                auto val = trimmed.substr(eq + 1);
                auto trim = [](std::string& s) {
                    auto a = s.find_first_not_of(" \t\"'");
                    auto b = s.find_last_not_of(" \t\"'");
                    s = (a != std::string::npos) ? s.substr(a, b - a + 1) : "";
                };
                trim(name);
                trim(val);
                if (!name.empty()) deps.emplace_back(name, val);
            }
        }
    }

    if (deps.empty()) {
        if (json_output) {
            std::cout << "{\"packages\":[]}" << std::endl;
        } else {
            std::cout << "No packages installed" << std::endl;
        }
        return CommandResult::Success;
    }

    if (json_output) {
        std::cout << "{\"packages\":[";
        for (size_t i = 0; i < deps.size(); ++i) {
            if (i > 0) std::cout << ",";
            std::cout << "{\"name\":\"" << deps[i].first
                      << "\",\"version\":\"" << deps[i].second << "\"}";
        }
        std::cout << "]}" << std::endl;
    } else {
        std::cout << "Installed packages:" << std::endl;
        for (const auto& [name, ver] : deps) {
            std::cout << "  " << name << " " << ver << std::endl;
        }
    }
    return CommandResult::Success;
}

CommandResult ModuleCommandHandler::handle_uninstall(const std::string& package,
                                                      bool json_output) {
    auto toml = toml_path();
    if (!std::filesystem::exists(toml)) {
        std::cerr << "error: meld.toml not found" << std::endl;
        return CommandResult::Error;
    }

    std::ifstream in(toml);
    std::string content((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
    in.close();

    // Find and remove the line containing the package
    std::istringstream stream(content);
    std::ostringstream result;
    std::string line;
    bool removed = false;

    while (std::getline(stream, line)) {
        auto trimmed = line;
        auto start = trimmed.find_first_not_of(" \t");
        if (start != std::string::npos) {
            trimmed = trimmed.substr(start);
        }
        if (trimmed.starts_with(package) && trimmed.find('=') != std::string::npos) {
            removed = true;
            continue;  // skip this line
        }
        result << line << "\n";
    }

    if (!removed) {
        if (json_output) {
            std::cout << "{\"uninstalled\":false,\"reason\":\"not found\",\"package\":\""
                      << package << "\"}" << std::endl;
        } else {
            std::cerr << "error: package '" << package << "' not found in meld.toml" << std::endl;
        }
        return CommandResult::Error;
    }

    std::ofstream out(toml);
    out << result.str();
    out.close();

    // Also remove from cache
    auto cached = cache_dir() / package;
    if (std::filesystem::exists(cached)) {
        std::error_code ec;
        std::filesystem::remove_all(cached, ec);
    }

    if (json_output) {
        std::cout << "{\"uninstalled\":true,\"package\":\"" << package << "\"}" << std::endl;
    } else {
        std::cout << "Uninstalled " << package << std::endl;
    }
    return CommandResult::Success;
}

CommandResult ModuleCommandHandler::handle_update(const std::string& package,
                                                    bool json_output) {
    auto toml = toml_path();
    if (!std::filesystem::exists(toml)) {
        std::cerr << "error: meld.toml not found" << std::endl;
        return CommandResult::Error;
    }

    auto dir = cache_dir();
    if (!std::filesystem::exists(dir)) {
        if (!json_output) std::cout << "No cached dependencies to update. Run 'meld module fetch' first." << std::endl;
        return CommandResult::Success;
    }

    int updated = 0;
    int total = 0;

    if (!package.empty()) {
        // Update specific package
        auto dest = dir / package;
        if (!std::filesystem::exists(dest)) {
            std::cerr << "error: package '" << package << "' not found in cache" << std::endl;
            return CommandResult::Error;
        }
        total = 1;
        auto r = run_cmd("git -C " + dest.string() + " pull --ff-only 2>&1");
        if (r.exit_code == 0) {
            ++updated;
            if (!json_output) std::cout << "Updated " << package << std::endl;
        } else {
            std::cerr << "error: failed to update " << package << ": " << r.output;
        }
    } else {
        // Update all cached packages
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (!entry.is_directory()) continue;
            auto name = entry.path().filename().string();
            ++total;
            auto r = run_cmd("git -C " + entry.path().string() + " pull --ff-only 2>&1");
            if (r.exit_code == 0) {
                ++updated;
                if (!json_output) std::cout << "Updated " << name << std::endl;
            } else {
                std::cerr << "warning: failed to update " << name << std::endl;
            }
        }
    }

    // Update meld.lock
    auto lock = lock_path();
    // Simple: write current HEAD commits for each cached dep
    std::ofstream lf(lock);
    lf << "# meld.lock — auto-generated, do not edit\n";
    if (std::filesystem::exists(dir)) {
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (!entry.is_directory()) continue;
            auto name = entry.path().filename().string();
            auto r = run_cmd("git -C " + entry.path().string() + " rev-parse HEAD 2>/dev/null");
            auto commit = r.output;
            while (!commit.empty() && (commit.back() == '\n' || commit.back() == '\r')) {
                commit.pop_back();
            }
            if (!commit.empty()) {
                lf << name << " = \"" << commit << "\"\n";
            }
        }
    }
    lf.close();

    if (json_output) {
        std::cout << "{\"updated\":" << updated << ",\"total\":" << total << "}" << std::endl;
    } else {
        if (updated == 0 && total > 0) {
            std::cout << "All packages are up to date" << std::endl;
        } else {
            std::cout << "Updated " << updated << " of " << total << " packages" << std::endl;
        }
    }
    return CommandResult::Success;
}

// -----------------------------------------------------------------------
// Help / completions / validation
// -----------------------------------------------------------------------

std::string ModuleCommandHandler::get_help() const {
    return R"(Manage packages and dependencies.

USAGE:
    meld module <subcommand> [OPTIONS]

SUBCOMMANDS:
    clean                  Purge the local dependency cache (~/.meld/cache/)
    fetch                  Download all Git dependencies from meld.toml
    install <package>      Add a package to the project
    list                   Show installed packages and versions
    uninstall <package>    Remove a package from the project
    update [package]       Update all or a specific package to latest version

OPTIONS:
    --dev                  Mark as development dependency (install only)
    --json                 Output in machine-readable JSON format

EXAMPLES:
    meld module install json-parser          Add a dependency
    meld module install --dev test-utils     Add a dev dependency
    meld module uninstall json-parser        Remove a dependency
    meld module list                         Show all dependencies
    meld module update                       Update all to latest
    meld module update json-parser           Update a specific package
    meld module fetch                        Download deps without building
    meld module clean                        Purge dependency cache)";
}

std::string ModuleCommandHandler::get_usage() const {
    return "meld module <clean|fetch|install|list|uninstall|update> [OPTIONS]";
}

std::vector<std::string> ModuleCommandHandler::get_completions(const std::string& partial) const {
    std::vector<std::string> all = {
        "--dev", "--json",
        "clean", "fetch", "install", "list", "uninstall", "update"
    };
    std::vector<std::string> result;
    for (const auto& c : all) {
        if (c.find(partial) == 0) result.push_back(c);
    }
    return result;
}

bool ModuleCommandHandler::validate_args(const CommandArgs&, std::string&) const {
    return true;  // execute() handles all validation with proper error messages
}

}  // namespace meld::cli
