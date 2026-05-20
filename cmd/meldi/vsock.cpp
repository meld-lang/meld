// vsock.cpp — VSOCK connection and mux framing (Tasks 8, 9)
#include "meld/init/types.hpp"
#include <sys/socket.h>
#include <linux/vm_sockets.h>
#include <unistd.h>

namespace meld::init {

VsockConn vsock_connect(uint16_t port) {
    VsockConn conn{};
    conn.port = port;
    conn.fd = socket(AF_VSOCK, SOCK_STREAM, 0);
    if (conn.fd < 0) return conn;

    struct sockaddr_vm addr{};
    addr.svm_family = AF_VSOCK;
    addr.svm_cid = 2;
    addr.svm_port = port;

    if (connect(conn.fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
        conn.connected = true;
    } else {
        close(conn.fd);
        conn.fd = -1;
    }
    return conn;
}

bool vsock_send_ready(const VsockConn& conn) {
    uint8_t msg[] = {CTRL_READY};
    return write(conn.fd, msg, 1) == 1;
}

bool mux_send(int fd, uint8_t stream_id, const void* data, uint32_t len) {
    uint8_t header[5];
    header[0] = stream_id;
    header[1] = (len >> 24) & 0xFF;
    header[2] = (len >> 16) & 0xFF;
    header[3] = (len >> 8) & 0xFF;
    header[4] = len & 0xFF;
    if (write(fd, header, 5) != 5) return false;
    if (len > 0 && write(fd, data, len) != (ssize_t)len) return false;
    return true;
}

bool mux_recv(int fd, MuxFrame& frame) {
    uint8_t header[5];
    if (read(fd, header, 5) != 5) return false;
    frame.stream_id = header[0];
    frame.length = ((uint32_t)header[1] << 24) | ((uint32_t)header[2] << 16) |
                   ((uint32_t)header[3] << 8) | header[4];
    if (frame.length > MAX_FRAME_PAYLOAD) return false;
    if (frame.length > 0) {
        if (read(fd, frame.payload, frame.length) != (ssize_t)frame.length)
            return false;
    }
    return true;
}

void host_disconnect(const VsockConn& conn, int exit_code) {
    uint8_t msg[2] = {CTRL_EXIT_CODE, (uint8_t)exit_code};
    write(conn.fd, msg, 2);
    close(conn.fd);
}

} // namespace meld::init
