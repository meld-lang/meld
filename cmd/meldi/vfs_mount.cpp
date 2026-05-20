// vfs_mount.cpp — Mount virtual filesystems (Task 2)
#include "meld/init/types.hpp"
#include <cstring>
#include <sys/mount.h>
#include <unistd.h>

namespace meld::init {

static void diag(const char* msg) {
    write(1, msg, strlen(msg));
}

bool vfs_mount() {
    if (mount("devtmpfs", "/dev", "devtmpfs", 0, nullptr) != 0) {
        diag("meldi: fatal: mount /dev failed\n");
        return false;
    }
    if (mount("proc", "/proc", "proc", MS_NOSUID | MS_NOEXEC, nullptr) != 0) {
        diag("meldi: fatal: mount /proc failed\n");
        return false;
    }
    if (mount("sysfs", "/sys", "sysfs", MS_NOSUID | MS_NOEXEC, nullptr) != 0) {
        diag("meldi: fatal: mount /sys failed\n");
        return false;
    }
    return true;
}

} // namespace meld::init
