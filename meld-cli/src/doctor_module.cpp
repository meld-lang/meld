#include "meld/cli/doctor_module.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <array>
#include <memory>

namespace meld::cli {

namespace fs = std::filesystem;

struct Check {
    std::string name;
    std::string status;  // "ok", "warning", "error"
    std::string detail;
};

static std::string exec_cmd(const std::string& cmd) {
    std::array<char, 256> buffer;
    std::string result;
    std::string full_cmd = cmd + " 2>/dev/null";
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(full_cmd.c_str(), "r"), pclose);
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    // Trim trailing newline
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
        result.pop_back();
    return result;
}

static Check check_bazel() {
    auto version = exec_cmd("bazel --version");
    if (version.empty()) return {"bazel", "error", "not found in PATH"};
    return {"bazel", "ok", version};
}

static Check check_compiler() {
    auto version = exec_cmd("clang++ --version | head -1");
    if (!version.empty()) return {"c++ compiler", "ok", version};
    version = exec_cmd("g++ --version | head -1");
    if (!version.empty()) return {"c++ compiler", "ok", version};
    return {"c++ compiler", "error", "no clang++ or g++ found"};
}

static Check check_daemon() {
    fs::path pid_file = ".meld/meldd.pid";
    if (!fs::exists(pid_file)) return {"daemon", "warning", "not running (no .meld/meldd.pid)"};
    std::ifstream f(pid_file);
    std::string pid; f >> pid;
    std::string check = exec_cmd("kill -0 " + pid + " 2>/dev/null && echo alive");
    if (check.find("alive") != std::string::npos)
        return {"daemon", "ok", "running (pid " + pid + ")"};
    return {"daemon", "warning", "stale pid file (pid " + pid + " not running)"};
}

static Check check_manifest() {
    if (fs::exists("meld.toml")) return {"manifest", "ok", "meld.toml found"};
    return {"manifest", "warning", "no meld.toml in current directory"};
}

static Check check_vscode_ext() {
    auto result = exec_cmd("code --list-extensions 2>/dev/null | grep -i meld");
    if (!result.empty()) return {"vscode extension", "ok", result};
    if (exec_cmd("which code").empty()) return {"vscode extension", "warning", "code CLI not found"};
    return {"vscode extension", "warning", "meld extension not installed"};
}

static Check check_lsp_socket() {
    if (fs::exists(".meld/lsp.sock")) return {"lsp socket", "ok", ".meld/lsp.sock exists"};
    return {"lsp socket", "warning", "not found (daemon may not be running)"};
}

// ═══════════════════════════════════════════════════════════════════════════

DoctorModule::DoctorModule()
    : BaseCommandHandler("doctor", "Check development environment") {}

CommandResult DoctorModule::execute(const CommandArgs& args) {
    bool text = args.flags.count("text") > 0 || args.options.count("text") > 0;
    for (const auto& arg : args.positional) {
        if (arg == "--text") text = true;
    }
    bool json = !text;

    std::vector<Check> checks = {
        check_bazel(),
        check_compiler(),
        check_daemon(),
        check_manifest(),
        check_vscode_ext(),
        check_lsp_socket(),
    };

    bool all_ok = true;
    for (const auto& c : checks) {
        if (c.status == "error") all_ok = false;
    }

    if (json) {
        std::cout << "{\n";
        std::cout << "  \"ok\": " << (all_ok ? "true" : "false") << ",\n";
        std::cout << "  \"checks\": [\n";
        for (size_t i = 0; i < checks.size(); ++i) {
            if (i > 0) std::cout << ",\n";
            std::cout << "    {\"name\": \"" << checks[i].name
                      << "\", \"status\": \"" << checks[i].status
                      << "\", \"detail\": \"" << checks[i].detail << "\"}";
        }
        std::cout << "\n  ]\n}\n";
    } else {
        std::cout << "Meld Doctor\n\n";
        for (const auto& c : checks) {
            const char* icon = c.status == "ok" ? "✓" : (c.status == "warning" ? "⚠" : "✗");
            std::cout << "  " << icon << " " << c.name << ": " << c.detail << "\n";
        }
        std::cout << "\n";
        if (all_ok) {
            std::cout << "All checks passed.\n";
        } else {
            std::cout << "Some checks failed. Fix errors above to proceed.\n";
        }
    }
    return all_ok ? CommandResult::Success : CommandResult::Error;
}

std::string DoctorModule::get_help() const {
    return R"(Usage: meld doctor [--text]

Check the development environment for common issues.
Output is JSON by default. Use --text for human-readable.

Flags:
  --text     Output as human-readable plain text

Checks:
  - Bazel version and availability
  - C++ compiler (clang++ or g++)
  - Meld daemon status
  - Project manifest (meld.toml)
  - VS Code extension
  - LSP socket

Examples:
  meld doctor              JSON report (default)
  meld doctor --text       Human-readable report)";
}

std::string DoctorModule::get_usage() const {
    return "meld doctor [--text]";
}

std::vector<std::string> DoctorModule::get_completions(const std::string& partial) const {
    return {"--text"};
}

bool DoctorModule::validate_args(const CommandArgs& args, std::string& error_message) const {
    return true;
}

} // namespace meld::cli
