// net_configure.cpp — Network setup (Task 5)
#include "meld/init/types.hpp"
#include <cstdio>
#include <cstdlib>

namespace meld::init {

bool net_configure(const CmdlineParams& params) {
    if (!params.has_network) return true;
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "ip addr add %s/24 dev eth0 2>/dev/null", params.ip);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "ip link set eth0 up 2>/dev/null");
    system(cmd);
    snprintf(cmd, sizeof(cmd), "ip route add default via %s 2>/dev/null", params.gw);
    system(cmd);
    return true;
}

} // namespace meld::init
