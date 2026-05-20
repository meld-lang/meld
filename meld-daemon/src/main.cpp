#include "meld/daemon/daemon.hpp"

#include <csignal>
#include <iostream>
#include <string>

namespace {

meld::daemon::MeldDaemon* g_daemon = nullptr;

void signal_handler(int /*sig*/) {
    if (g_daemon) {
        g_daemon->shutdown();
    }
}

void print_usage() {
    std::cerr << "Usage: meldd [options]\n"
              << "Options:\n"
              << "  --workspace=<path>   Project root directory (default: cwd)\n"
              << "  --timeout=<seconds>  Auto-exit after idle time (0 = disabled)\n"
              << "  --sandbox-verbose    Enable verbose sandbox logging\n"
              << "  --verbose            Enable verbose daemon logging\n"
              << "  --help               Show this help message\n"
              << "\n"
              << "The daemon listens on two Unix sockets:\n"
              << "  .meld/lsp.sock  — LSP (JSON-RPC) for IDE clients\n"
              << "  .meld/mcp.sock  — MCP (JSON-RPC) for AI agents\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    meld::daemon::DaemonConfig config;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg.find("--workspace=") == 0) {
            config.workspace = arg.substr(12);
        } else if (arg.find("--timeout=") == 0) {
            try {
                config.timeout_seconds = std::stoi(arg.substr(10));
            } catch (...) {
                std::cerr << "Invalid timeout value: " << arg.substr(10) << "\n";
                return 1;
            }
        } else if (arg == "--sandbox-verbose") {
            config.sandbox_verbose = true;
        } else if (arg == "--verbose") {
            config.verbose = true;
        } else if (arg == "--help") {
            print_usage();
            return 0;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage();
            return 1;
        }
    }

    if (config.workspace.empty()) {
        config.workspace = std::filesystem::current_path();
    }

    std::signal(SIGTERM, signal_handler);
    std::signal(SIGINT, signal_handler);

    meld::daemon::MeldDaemon daemon(std::move(config));
    g_daemon = &daemon;

    std::cerr << "[meldd] Starting daemon for workspace: "
              << daemon.config().workspace.string() << "\n";

    daemon.run();

    g_daemon = nullptr;
    return 0;
}
