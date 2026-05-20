// app_exec.cpp — Application launch and lifecycle (Tasks 13, 14)
#include "meld/init/types.hpp"
#include <cstring>
#include <unistd.h>
#include <sys/reboot.h>

namespace meld::init {

extern SignalState g_signal_state;

static void diag(const char* msg) {
    write(1, msg, strlen(msg));
}

pid_t app_exec(const char* path) {
    pid_t pid = fork();
    if (pid == 0) {
        char* argv[] = {const_cast<char*>(path), nullptr};
        execv(path, argv);
        diag("meldi: fatal: execv failed\n");
        _exit(127);
    }
    return pid;
}

int compute_exit_code() {
    if (g_signal_state.app_exited) {
        return g_signal_state.app_exit_status;
    }
    return 1;
}

void shutdown() {
    sync();
    reboot(RB_POWER_OFF);
}

} // namespace meld::init
