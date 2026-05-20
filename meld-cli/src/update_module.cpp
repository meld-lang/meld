#include "meld/cli/update_module.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

namespace meld::cli {

// -----------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------

UpdateModule::UpdateModule()
    : BaseCommandHandler("update", "Update the meld toolchain to the latest version") {}

// -----------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------

UpdateModule::CmdResult UpdateModule::run_cmd(const std::string& cmd) const {
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

std::string UpdateModule::current_version() const {
    return "0.1.0";  // TODO: embed at build time via stamping
}

std::filesystem::path UpdateModule::find_workspace_root() const {
    // Walk up from the binary looking for MODULE.bazel
    std::filesystem::path self;
#ifdef __APPLE__
    char buf[4096];
    uint32_t sz = sizeof(buf);
    if (_NSGetExecutablePath(buf, &sz) == 0) {
        std::error_code ec;
        self = std::filesystem::absolute(buf, ec);
    }
#else
    std::error_code ec;
    self = std::filesystem::read_symlink("/proc/self/exe", ec);
#endif

    // Try extracting workspace from bazel-bin path
    auto s = self.string();
    auto pos = s.find("/bazel-bin/");
    if (pos != std::string::npos) {
        return s.substr(0, pos);
    }

    // Walk up from cwd
    auto dir = std::filesystem::current_path();
    for (int i = 0; i < 10 && !dir.empty() && dir != dir.parent_path(); ++i) {
        if (std::filesystem::exists(dir / "MODULE.bazel") ||
            std::filesystem::exists(dir / "WORKSPACE")) {
            return dir;
        }
        dir = dir.parent_path();
    }
    return std::filesystem::current_path();
}

// -----------------------------------------------------------------------
// GitHub release update
// -----------------------------------------------------------------------

CommandResult UpdateModule::update_from_release(bool check_only, bool json_output) {
    // Determine platform
#ifdef __APPLE__
    #ifdef __aarch64__
    std::string platform = "darwin-arm64";
    #else
    std::string platform = "darwin-amd64";
    #endif
#elif defined(__linux__)
    std::string platform = "linux-amd64";
#else
    std::string platform = "unknown";
#endif

    std::string repo = "meld-lang/meld";
    std::string api_url = "https://api.github.com/repos/" + repo + "/releases/latest";

    // Fetch latest release tag
    auto tag_result = run_cmd("curl -sL " + api_url + " | grep '\"tag_name\"' | head -1 | sed 's/.*\"v\\([^\"]*\\)\".*/\\1/'");
    std::string latest = tag_result.output;
    // Trim whitespace
    while (!latest.empty() && (latest.back() == '\n' || latest.back() == '\r' || latest.back() == ' ')) {
        latest.pop_back();
    }

    if (latest.empty()) {
        std::cerr << "error: could not fetch latest release from GitHub" << std::endl;
        return CommandResult::Error;
    }

    std::string current = current_version();

    if (json_output) {
        std::cout << "{\"current\":\"" << current << "\",\"latest\":\"" << latest
                  << "\",\"up_to_date\":" << (current == latest ? "true" : "false") << "}" << std::endl;
    } else {
        std::cout << "Current version: " << current << std::endl;
        std::cout << "Latest version:  " << latest << std::endl;
    }

    if (current == latest) {
        if (!json_output) std::cout << "Already up to date." << std::endl;
        return CommandResult::Success;
    }

    if (check_only) {
        if (!json_output) std::cout << "Update available: " << latest << std::endl;
        return CommandResult::Success;
    }

    // Download the release binary
    std::string asset_name = "meld-" + platform;
    std::string download_url = "https://github.com/" + repo + "/releases/download/v" + latest + "/" + asset_name;

    if (!json_output) {
        std::cout << "Downloading " << asset_name << " v" << latest << "..." << std::endl;
    }

    // Download to a temp file
    auto tmp = std::filesystem::temp_directory_path() / ("meld-update-" + latest);
    auto dl = run_cmd("curl -sL -o " + tmp.string() + " " + download_url);
    if (dl.exit_code != 0 || !std::filesystem::exists(tmp)) {
        std::cerr << "error: download failed" << std::endl;
        return CommandResult::Error;
    }

    // Make executable
    run_cmd("chmod +x " + tmp.string());

    // Replace the current binary
    std::filesystem::path self;
#ifdef __APPLE__
    char pathbuf[4096];
    uint32_t pathsz = sizeof(pathbuf);
    if (_NSGetExecutablePath(pathbuf, &pathsz) == 0) {
        std::error_code ec;
        self = std::filesystem::canonical(pathbuf, ec);
    }
#else
    {
        std::error_code ec;
        self = std::filesystem::canonical("/proc/self/exe", ec);
    }
#endif

    if (self.empty()) {
        std::cerr << "error: could not determine binary path for replacement" << std::endl;
        std::filesystem::remove(tmp);
        return CommandResult::Error;
    }

    std::error_code ec;
    std::filesystem::rename(tmp, self, ec);
    if (ec) {
        // rename failed (cross-device?), try copy
        std::filesystem::copy_file(tmp, self,
                                   std::filesystem::copy_options::overwrite_existing, ec);
        std::filesystem::remove(tmp);
        if (ec) {
            std::cerr << "error: could not replace binary: " << ec.message() << std::endl;
            return CommandResult::Error;
        }
    }

    if (json_output) {
        std::cout << "{\"updated\":true,\"version\":\"" << latest << "\"}" << std::endl;
    } else {
        std::cout << "Updated to v" << latest << std::endl;
    }
    return CommandResult::Success;
}

// -----------------------------------------------------------------------
// Source build update
// -----------------------------------------------------------------------

CommandResult UpdateModule::update_from_source(bool json_output) {
    auto ws = find_workspace_root();

    if (!std::filesystem::exists(ws / ".git")) {
        std::cerr << "error: not a git repository: " << ws.string() << std::endl;
        return CommandResult::Error;
    }

    if (!json_output) std::cout << "Pulling latest changes..." << std::endl;

    auto pull = run_cmd("git -C " + ws.string() + " pull --ff-only 2>&1");
    if (pull.exit_code != 0) {
        std::cerr << "error: git pull failed:" << std::endl;
        std::cerr << pull.output;
        return CommandResult::Error;
    }
    if (!json_output) std::cout << pull.output;

    if (!json_output) std::cout << "Building..." << std::endl;

    auto build = run_cmd("cd " + ws.string() + " && bazel build //meld-cli:meld //meld-daemon:meldd 2>&1");
    if (build.exit_code != 0) {
        std::cerr << "error: bazel build failed:" << std::endl;
        std::cerr << build.output;
        return CommandResult::Error;
    }

    if (json_output) {
        std::cout << "{\"updated\":true,\"method\":\"source\",\"workspace\":\""
                  << ws.string() << "\"}" << std::endl;
    } else {
        std::cout << "Build complete. meld and meldd updated." << std::endl;
    }
    return CommandResult::Success;
}

// -----------------------------------------------------------------------
// execute
// -----------------------------------------------------------------------

CommandResult UpdateModule::execute(const CommandArgs& args) {
    bool from_source = args.flags.count("from-source") > 0 || args.options.count("from-source") > 0;
    bool check_only = args.flags.count("check") > 0 || args.options.count("check") > 0;
    bool json_output = args.flags.count("json") > 0 || args.options.count("json") > 0;

    if (from_source) {
        return update_from_source(json_output);
    }
    return update_from_release(check_only, json_output);
}

// -----------------------------------------------------------------------
// Help
// -----------------------------------------------------------------------

std::string UpdateModule::get_help() const {
    return R"(Update the meld toolchain to the latest version.

USAGE:
    meld update [OPTIONS]

OPTIONS:
    --check                Check for updates without installing
    --from-source          Update via git pull + bazel build (developer workflow)
    --json                 Output in machine-readable JSON format

EXAMPLES:
    meld update                        Download and install latest release
    meld update --check                Check if an update is available
    meld update --from-source          Pull and rebuild from source
    meld update --json                 Machine-readable update status)";
}

std::string UpdateModule::get_usage() const {
    return "meld update [--check] [--from-source] [--json]";
}

std::vector<std::string> UpdateModule::get_completions(const std::string& partial) const {
    std::vector<std::string> all = {"--check", "--from-source", "--json"};
    std::vector<std::string> result;
    for (const auto& c : all) {
        if (c.find(partial) == 0) result.push_back(c);
    }
    return result;
}

bool UpdateModule::validate_args(const CommandArgs&, std::string&) const {
    return true;
}

}  // namespace meld::cli
