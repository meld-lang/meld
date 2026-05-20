// meldi — Meld MicroVM Init System (PID 1)
// Sub-1MB statically linked binary for Firecracker MicroVMs
// Zero heap allocation in critical path
//
// Modules:
//   vfs_mount.cpp       — Mount /dev, /proc, /sys
//   signal_handler.cpp  — SIGCHLD + fault handlers
//   net_configure.cpp   — Network setup via ip commands
//   vsock.cpp           — VSOCK connection + mux framing
//   app_exec.cpp        — Fork/exec application, shutdown

#include "meld/init/types.hpp"
#include <cstring>
#include <unistd.h>
#include <sys/reboot.h>

namespace meld::init {

// Global state (async-signal-safe)
SignalState g_signal_state;
TombstoneTrace g_tombstone;

// Forward declarations (implemented in separate modules)
bool vfs_mount();
CmdlineParams parse_cmdline();
bool net_configure(const CmdlineParams& params);
void register_sigchld_handler();
void register_fault_handlers();
VsockConn vsock_connect(uint16_t port);
bool vsock_send_ready(const VsockConn& conn);
void host_disconnect(const VsockConn& conn, int exit_code);
pid_t app_exec(const char* path);
int compute_exit_code();
void shutdown();

// Cmdline parser (kept inline — small, no dependencies)
CmdlineParams parse_cmdline() {
    CmdlineParams params{};
    char buf[4096];
    int fd = open("/proc/cmdline", O_RDONLY);
    if (fd < 0) return params;
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return params;
    buf[n] = '\0';

    char* token = strtok(buf, " \n");
    while (token) {
        char* eq = strchr(token, '=');
        if (eq) {
            *eq = '\0';
            const char* key = token;
            const char* val = eq + 1;
            if (strcmp(key, "meld.ip") == 0)
                strncpy(params.ip, val, sizeof(params.ip) - 1);
            else if (strcmp(key, "meld.gw") == 0)
                strncpy(params.gw, val, sizeof(params.gw) - 1);
            else if (strcmp(key, "meld.vsock_port") == 0)
                params.vsock_port = (uint16_t)atoi(val);
            else if (strcmp(key, "meld.app_path") == 0)
                strncpy(params.app_path, val, sizeof(params.app_path) - 1);
        }
        token = strtok(nullptr, " \n");
    }
    params.has_network = (params.ip[0] != '\0' && params.gw[0] != '\0');
    return params;
}

} // namespace meld::init

int main() {
    using namespace meld::init;

    if (!vfs_mount()) { reboot(RB_POWER_OFF); return 1; }

    auto params = parse_cmdline();
    net_configure(params);
    register_sigchld_handler();
    register_fault_handlers();

    auto conn = vsock_connect(params.vsock_port);
    if (!conn.connected) {
        write(1, "meldi: fatal: vsock connect failed\n", 35);
        shutdown();
        return 1;
    }
    vsock_send_ready(conn);

    pid_t app_pid = app_exec(params.app_path);
    if (app_pid < 0) {
        write(1, "meldi: fatal: fork failed\n", 26);
        host_disconnect(conn, 127);
        shutdown();
        return 1;
    }
    g_signal_state.app_pid = app_pid;

    // Wait for app to exit
    while (!g_signal_state.app_exited) {
        usleep(10000);
    }

    int exit_code = compute_exit_code();
    host_disconnect(conn, exit_code);
    shutdown();
    return exit_code;
}
