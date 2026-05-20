// signal_handler.cpp — SIGCHLD and fault handlers (Tasks 3, 12)
#include "meld/init/types.hpp"
#include <csignal>
#include <cstdio>
#include <cstring>
#include <sys/wait.h>
#include <unistd.h>

namespace meld::init {

extern SignalState g_signal_state;
extern TombstoneTrace g_tombstone;

static void sigchld_handler(int) {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (pid == g_signal_state.app_pid) {
            g_signal_state.app_exited = 1;
            g_signal_state.app_exit_status = WIFEXITED(status)
                ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
        }
    }
}

void register_sigchld_handler() {
    struct sigaction sa{};
    sa.sa_handler = sigchld_handler;
    sa.sa_flags = SA_NOCLDSTOP | SA_RESTART;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGCHLD, &sa, nullptr);
}

static void fault_handler(int sig, siginfo_t* info, void*) {
    g_tombstone.signal_num = sig;
    g_tombstone.fault_addr = info->si_addr;
    snprintf(g_tombstone.message, sizeof(g_tombstone.message),
             "meldi: fault signal=%d addr=%p\n", sig, info->si_addr);
    write(1, g_tombstone.message, strlen(g_tombstone.message));
}

void register_fault_handlers() {
    struct sigaction sa{};
    sa.sa_sigaction = fault_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGBUS, &sa, nullptr);
    sigaction(SIGFPE, &sa, nullptr);
    sigaction(SIGILL, &sa, nullptr);
}

} // namespace meld::init
