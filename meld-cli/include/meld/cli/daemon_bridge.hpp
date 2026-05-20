#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace meld::cli {

/// Bridges a Unix domain socket to stdin/stdout for LSP or MCP stdio
/// transport. Ensures the meldd daemon is running before connecting.
///
/// Provides native C++ socket I/O for `meld lsp` and `meld mcp`
/// subcommands.
class DaemonBridge {
public:
    enum class Channel { LSP, MCP };

    /// Run the bridge: ensure daemon is running, connect to socket,
    /// relay stdin→socket and socket→stdout until EOF or error.
    /// Returns exit code (0 = clean shutdown).
    static int run(Channel channel,
                   const std::filesystem::path& workspace = {});

    /// Find the meldd binary: check $PATH, then relative to meld binary.
    static std::optional<std::filesystem::path> find_meldd();

private:
    /// Paths derived from workspace root
    struct Paths {
        std::filesystem::path meld_dir;     // <workspace>/.meld/
        std::filesystem::path pid_file;     // <workspace>/.meld/meldd.pid
        std::filesystem::path log_file;     // <workspace>/.meld/meldd.log
        std::filesystem::path lsp_socket;   // <workspace>/.meld/lsp.sock
        std::filesystem::path mcp_socket;   // <workspace>/.meld/mcp.sock
    };

    static Paths make_paths(const std::filesystem::path& workspace);

    /// Check if daemon is alive via PID file.
    static bool daemon_alive(const Paths& paths);

    /// Start the daemon in the background, wait for PID file.
    static bool start_daemon(const Paths& paths,
                             const std::filesystem::path& meldd_binary,
                             const std::filesystem::path& workspace);

    /// Wait for a Unix socket to appear (up to timeout_ms).
    static bool wait_for_socket(const std::filesystem::path& socket_path,
                                int timeout_ms = 30000);

    /// Bidirectional relay: stdin↔socket, socket↔stdout.
    /// Blocks until EOF on either side.
    static int relay(int socket_fd);
};

}  // namespace meld::cli
