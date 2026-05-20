#include "meld/cli/daemon_client.hpp"

#include <fstream>
#include <iostream>

#ifndef _WIN32
#include <csignal>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace meld::cli {

DaemonClient::DaemonClient(const std::filesystem::path& workspace)
    : workspace_(workspace) {
}

bool DaemonClient::is_daemon_running() const {
    auto pid = read_daemon_pid();
    if (!pid) return false;
#ifndef _WIN32
    return kill(*pid, 0) == 0;
#else
    return false;
#endif
}

std::optional<FreshnessResult> DaemonClient::check_binary_freshness(
    const std::filesystem::path& binary_path) const {
    if (!is_daemon_running()) {
        return std::nullopt;  // graceful fallback
    }

    // Connect to daemon via Unix domain socket at .meld/daemon.sock
    auto sock_path = workspace_ / ".meld" / "daemon.sock";
    if (!std::filesystem::exists(sock_path)) {
        return std::nullopt;
    }

#ifndef _WIN32
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return std::nullopt;

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    auto sock_str = sock_path.string();
    if (sock_str.size() >= sizeof(addr.sun_path)) {
        close(fd);
        return std::nullopt;
    }
    std::copy(sock_str.begin(), sock_str.end(), addr.sun_path);

    if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(fd);
        return std::nullopt;
    }

    // Send freshness check request as simple JSON-RPC.
    std::string request = "{\"method\":\"check_binary_freshness\",\"params\":{\"binary_path\":\""
                          + binary_path.string() + "\"}}\n";
    write(fd, request.data(), request.size());

    // Read response (blocking — daemon blocks until rebuild completes per Req 12.4).
    std::string response;
    char buf[4096];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        response += buf;
        if (response.find('\n') != std::string::npos) break;
    }
    close(fd);

    // Parse response — minimal JSON parsing.
    FreshnessResult result;
    result.is_current = response.find("\"is_current\":true") != std::string::npos;
    result.rebuild_triggered = response.find("\"rebuild_triggered\":true") != std::string::npos;

    // Extract build_error if present.
    auto err_pos = response.find("\"build_error\":\"");
    if (err_pos != std::string::npos) {
        auto start = err_pos + 15;
        auto end = response.find('"', start);
        if (end != std::string::npos) {
            result.build_error = response.substr(start, end - start);
        }
    }

    return result;
#else
    return std::nullopt;
#endif
}

std::optional<std::filesystem::path> DaemonClient::resolve_debug_sidecar(
    const std::string& debug_id) const {
    if (!is_daemon_running()) {
        return std::nullopt;
    }

    auto sock_path = workspace_ / ".meld" / "daemon.sock";
    if (!std::filesystem::exists(sock_path)) {
        return std::nullopt;
    }

#ifndef _WIN32
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return std::nullopt;

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    auto sock_str = sock_path.string();
    if (sock_str.size() >= sizeof(addr.sun_path)) {
        close(fd);
        return std::nullopt;
    }
    std::copy(sock_str.begin(), sock_str.end(), addr.sun_path);

    if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(fd);
        return std::nullopt;
    }

    std::string request = "{\"method\":\"resolve_debug_sidecar\",\"params\":{\"debug_id\":\""
                          + debug_id + "\"}}\n";
    write(fd, request.data(), request.size());

    std::string response;
    char buf[4096];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        response += buf;
        if (response.find('\n') != std::string::npos) break;
    }
    close(fd);

    // Extract path from response.
    auto path_pos = response.find("\"path\":\"");
    if (path_pos != std::string::npos) {
        auto start = path_pos + 8;
        auto end = response.find('"', start);
        if (end != std::string::npos) {
            auto p = std::filesystem::path(response.substr(start, end - start));
            if (std::filesystem::exists(p)) {
                return p;
            }
        }
    }

    return std::nullopt;
#else
    return std::nullopt;
#endif
}

std::filesystem::path DaemonClient::pid_file_path() const {
    return workspace_ / ".meld" / "daemon.pid";
}

std::optional<int> DaemonClient::read_daemon_pid() const {
    auto path = pid_file_path();
    if (!std::filesystem::exists(path)) return std::nullopt;

    std::ifstream file(path);
    int pid = 0;
    file >> pid;
    return pid > 0 ? std::optional<int>(pid) : std::nullopt;
}

} // namespace meld::cli
