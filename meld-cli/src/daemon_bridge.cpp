#include "meld/cli/daemon_bridge.hpp"

#include <array>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>  // _NSGetExecutablePath
#endif

namespace meld::cli {

// -----------------------------------------------------------------------
// Self-path detection (reused by make_paths and find_meldd)
// -----------------------------------------------------------------------

static std::filesystem::path get_self_path() {
    std::filesystem::path self_path;
#ifdef __APPLE__
    char buf[4096];
    uint32_t bufsize = sizeof(buf);
    if (_NSGetExecutablePath(buf, &bufsize) == 0) {
        // Don't use canonical() — it resolves through Bazel's internal
        // cache symlinks. Use absolute() to normalize without resolving.
        std::error_code ec;
        self_path = std::filesystem::absolute(buf, ec);
        if (ec) self_path.clear();
    }
#else
    std::error_code ec;
    self_path = std::filesystem::read_symlink("/proc/self/exe", ec);
#endif
    return self_path;
}

// -----------------------------------------------------------------------
// Path helpers
// -----------------------------------------------------------------------

DaemonBridge::Paths DaemonBridge::make_paths(
    const std::filesystem::path& workspace) {
    std::filesystem::path ws = workspace;

    if (ws.empty()) {
        // Strategy 1: Walk up from cwd looking for workspace markers
        auto dir = std::filesystem::current_path();
        for (int i = 0; i < 10 && !dir.empty() && dir != dir.parent_path(); ++i) {
            if (std::filesystem::exists(dir / "MODULE.bazel") ||
                std::filesystem::exists(dir / "WORKSPACE") ||
                std::filesystem::exists(dir / "WORKSPACE.bazel") ||
                std::filesystem::exists(dir / "meld.toml")) {
                ws = dir;
                break;
            }
            dir = dir.parent_path();
        }
    }

    if (ws.empty()) {
        // Strategy 2: Resolve the binary's symlink chain to find the
        // workspace. tools/meld -> bazel-bin/meld-cli/meld, and
        // bazel-bin is a symlink at the workspace root.
        auto self = get_self_path();
        if (!self.empty()) {
            // _NSGetExecutablePath may return the Bazel cache path.
            // Instead, try reading the /proc/self/exe or argv[0] symlink.
            // On macOS, check if the path contains "bazel-bin" and extract
            // the workspace root from before it.
            auto s = self.string();
            auto pos = s.find("/bazel-bin/");
            if (pos != std::string::npos) {
                ws = s.substr(0, pos);
            } else {
                // Walk up from binary location
                auto dir = self.parent_path();
                for (int i = 0; i < 10 && !dir.empty() && dir != dir.parent_path(); ++i) {
                    if (std::filesystem::exists(dir / "MODULE.bazel") ||
                        std::filesystem::exists(dir / "WORKSPACE") ||
                        std::filesystem::exists(dir / "meld.toml")) {
                        ws = dir;
                        break;
                    }
                    dir = dir.parent_path();
                }
            }
        }
    }

    if (ws.empty()) {
        ws = std::filesystem::current_path();
    }

    Paths p;
    p.meld_dir   = ws / ".meld";
    p.pid_file   = p.meld_dir / "meldd.pid";
    p.log_file   = p.meld_dir / "meldd.log";
    p.lsp_socket = p.meld_dir / "lsp.sock";
    p.mcp_socket = p.meld_dir / "mcp.sock";
    return p;
}

// -----------------------------------------------------------------------
// Find meldd binary
// -----------------------------------------------------------------------

std::optional<std::filesystem::path> DaemonBridge::find_meldd() {
    // 1. Check $PATH
    const char* path_env = std::getenv("PATH");
    if (path_env) {
        std::istringstream stream(path_env);
        std::string dir;
        while (std::getline(stream, dir, ':')) {
            auto candidate = std::filesystem::path(dir) / "meldd";
            if (std::filesystem::exists(candidate)) {
                return candidate;
            }
        }
    }

    // 2. Relative to this binary (same dir, or ../libexec/)
    auto self_path = get_self_path();
    if (!self_path.empty()) {
        auto bin_dir = self_path.parent_path();
        auto candidate = bin_dir / "meldd";
        if (std::filesystem::exists(candidate)) return candidate;
        candidate = bin_dir.parent_path() / "libexec" / "meldd";
        if (std::filesystem::exists(candidate)) return candidate;
        // Bazel layout: meld is at bazel-bin/meld-cli/meld,
        // meldd is at bazel-bin/meld-daemon/meldd
        candidate = bin_dir.parent_path() / "meld-daemon" / "meldd";
        if (std::filesystem::exists(candidate)) return candidate;

        // If self_path is a symlink (e.g. tools/meld -> bazel-bin/meld-cli/meld),
        // resolve it and retry the Bazel layout search.
        std::error_code ec;
        auto resolved = std::filesystem::canonical(self_path, ec);
        if (!ec && resolved != self_path) {
            auto rbin = resolved.parent_path();
            candidate = rbin / "meldd";
            if (std::filesystem::exists(candidate)) return candidate;
            candidate = rbin.parent_path() / "meld-daemon" / "meldd";
            if (std::filesystem::exists(candidate)) return candidate;
        }
    }

    // 3. bazel-bin fallback (development)
    auto cwd = std::filesystem::current_path();
    auto bazel_candidate = cwd / "bazel-bin" / "meld-daemon" / "meldd";
    if (std::filesystem::exists(bazel_candidate)) return bazel_candidate;

    return std::nullopt;
}

// -----------------------------------------------------------------------
// Daemon lifecycle
// -----------------------------------------------------------------------

bool DaemonBridge::daemon_alive(const Paths& paths) {
    if (!std::filesystem::exists(paths.pid_file)) return false;

    std::ifstream f(paths.pid_file);
    pid_t pid = 0;
    f >> pid;
    if (pid <= 0) return false;

    return kill(pid, 0) == 0;
}

bool DaemonBridge::start_daemon(
    const Paths& paths,
    const std::filesystem::path& meldd_binary,
    const std::filesystem::path& workspace) {

    std::filesystem::create_directories(paths.meld_dir);

    // Acquire an exclusive lock to prevent concurrent daemon spawns.
    auto lock_path = paths.meld_dir / "start.lock";
    int lock_fd = open(lock_path.c_str(), O_CREAT | O_RDWR, 0600);
    if (lock_fd >= 0 && flock(lock_fd, LOCK_EX) == 0) {
        // Re-check after acquiring lock — another process may have started it.
        if (daemon_alive(paths)) {
            flock(lock_fd, LOCK_UN);
            close(lock_fd);
            return true;
        }
    }

    // Remove stale sockets
    std::error_code ec;
    std::filesystem::remove(paths.lsp_socket, ec);
    std::filesystem::remove(paths.mcp_socket, ec);

    std::cerr << "[meld] Starting daemon (workspace: "
              << workspace.string() << ")..." << std::endl;

    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "[meld] fork() failed: " << std::strerror(errno)
                  << std::endl;
        return false;
    }

    if (pid == 0) {
        // Child: redirect stdout/stderr to log, detach from terminal
        setsid();

        int log_fd = open(paths.log_file.c_str(),
                          O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (log_fd >= 0) {
            dup2(log_fd, STDOUT_FILENO);
            dup2(log_fd, STDERR_FILENO);
            close(log_fd);
        }

        // Close stdin
        int null_fd = open("/dev/null", O_RDONLY);
        if (null_fd >= 0) {
            dup2(null_fd, STDIN_FILENO);
            close(null_fd);
        }

        std::string ws_arg = "--workspace=" + workspace.string();
        std::string verbose_arg = "--verbose";
        execl(meldd_binary.c_str(), meldd_binary.c_str(),
              ws_arg.c_str(), verbose_arg.c_str(), nullptr);

        // If exec fails
        _exit(127);
    }

    // Parent: write PID file and wait briefly for startup
    {
        std::ofstream pf(paths.pid_file);
        pf << pid;
    }

    // Release startup lock
    if (lock_fd >= 0) {
        flock(lock_fd, LOCK_UN);
        close(lock_fd);
    }

    // Give daemon a moment to initialize
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    return true;
}

// -----------------------------------------------------------------------
// Wait for socket
// -----------------------------------------------------------------------

bool DaemonBridge::wait_for_socket(
    const std::filesystem::path& socket_path, int timeout_ms) {

    auto deadline = std::chrono::steady_clock::now()
                    + std::chrono::milliseconds(timeout_ms);

    while (std::chrono::steady_clock::now() < deadline) {
        if (std::filesystem::exists(socket_path)) {
            // Verify it's actually a socket
            auto status = std::filesystem::status(socket_path);
            if (status.type() == std::filesystem::file_type::socket) {
                return true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return false;
}

// -----------------------------------------------------------------------
// Bidirectional relay: stdin ↔ socket ↔ stdout
// -----------------------------------------------------------------------

int DaemonBridge::relay(int sock_fd) {
    constexpr size_t BUF_SIZE = 8192;
    std::array<char, BUF_SIZE> buf{};

    struct pollfd fds[2];
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    fds[1].fd = sock_fd;
    fds[1].events = POLLIN;

    for (;;) {
        int ret = poll(fds, 2, -1);
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }

        // stdin → socket
        if (fds[0].revents & POLLIN) {
            ssize_t n = read(STDIN_FILENO, buf.data(), BUF_SIZE);
            if (n <= 0) break;  // EOF or error on stdin
            ssize_t written = 0;
            while (written < n) {
                ssize_t w = write(sock_fd, buf.data() + written,
                                  static_cast<size_t>(n - written));
                if (w <= 0) return 1;
                written += w;
            }
        }
        if (fds[0].revents & (POLLHUP | POLLERR)) break;

        // socket → stdout
        if (fds[1].revents & POLLIN) {
            ssize_t n = read(sock_fd, buf.data(), BUF_SIZE);
            if (n <= 0) break;  // EOF or error on socket
            ssize_t written = 0;
            while (written < n) {
                ssize_t w = write(STDOUT_FILENO, buf.data() + written,
                                  static_cast<size_t>(n - written));
                if (w <= 0) return 1;
                written += w;
            }
        }
        if (fds[1].revents & (POLLHUP | POLLERR)) break;
    }

    return 0;
}

// -----------------------------------------------------------------------
// Main entry point
// -----------------------------------------------------------------------

int DaemonBridge::run(Channel channel,
                      const std::filesystem::path& workspace) {
    auto paths = make_paths(workspace);

    // 1. Ensure daemon is running
    if (!daemon_alive(paths)) {
        auto meldd = find_meldd();
        if (!meldd) {
            std::cerr << "error: meldd binary not found — build with "
                         "'bazel build //meld-daemon:meldd' or add to $PATH"
                      << std::endl;
            return 1;
        }

        auto ws = workspace.empty()
                      ? std::filesystem::current_path()
                      : workspace;
        if (!start_daemon(paths, *meldd, ws)) {
            return 1;
        }
    }

    // 2. Wait for the requested socket
    const auto& socket_path = (channel == Channel::LSP)
                                  ? paths.lsp_socket
                                  : paths.mcp_socket;
    const char* channel_name = (channel == Channel::LSP) ? "LSP" : "MCP";

    if (!wait_for_socket(socket_path)) {
        std::cerr << "error: timed out waiting for " << channel_name
                  << " socket at " << socket_path.string() << std::endl;
        // Show last few lines of daemon log
        if (std::filesystem::exists(paths.log_file)) {
            std::cerr << "Last lines of daemon log:" << std::endl;
            std::string cmd = "tail -10 " + paths.log_file.string();
            std::system(cmd.c_str());
        }
        return 1;
    }

    // 3. Connect to the Unix socket
    int sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        std::cerr << "error: socket() failed: " << std::strerror(errno)
                  << std::endl;
        return 1;
    }

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::string path_str = socket_path.string();
    if (path_str.size() >= sizeof(addr.sun_path)) {
        std::cerr << "error: socket path too long" << std::endl;
        close(sock_fd);
        return 1;
    }
    std::strncpy(addr.sun_path, path_str.c_str(), sizeof(addr.sun_path) - 1);

    if (connect(sock_fd, reinterpret_cast<struct sockaddr*>(&addr),
                sizeof(addr)) < 0) {
        std::cerr << "error: connect to " << channel_name
                  << " socket failed: " << std::strerror(errno) << std::endl;
        close(sock_fd);
        return 1;
    }

    // 4. Relay stdin↔socket↔stdout
    int result = relay(sock_fd);
    close(sock_fd);
    return result;
}

}  // namespace meld::cli
