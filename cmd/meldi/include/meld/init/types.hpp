// types.hpp — meldi shared types and constants (Req 1)
// Sub-1MB init system for Firecracker MicroVMs

#pragma once
#include <cstdint>
#include <cstddef>

namespace meld::init {

// Stream IDs for multiplexed vsock communication
constexpr uint8_t STREAM_STDOUT  = 1;
constexpr uint8_t STREAM_STDERR  = 2;
constexpr uint8_t STREAM_AUDIT   = 3;
constexpr uint8_t STREAM_CONTROL = 4;

// Control message types
constexpr uint8_t CTRL_READY     = 0x01;
constexpr uint8_t CTRL_EXIT_CODE = 0x02;
constexpr uint8_t CTRL_TOMBSTONE = 0x03;

// Maximum frame payload size
constexpr size_t MAX_FRAME_PAYLOAD = 65536;

// Kernel cmdline parameters
struct CmdlineParams {
    char ip[16]{};           // meld.ip
    char gw[16]{};           // meld.gw
    uint16_t vsock_port{};   // meld.vsock_port
    char app_path[256]{};    // meld.app_path
    bool has_network{};
};

// Vsock handshake payload
struct HandshakePayload {
    uint8_t version{1};
    uint16_t vsock_port{};
    char app_path[256]{};
};

// Environment variable
struct EnvVar {
    char key[64]{};
    char value[256]{};
};

// Vsock connection state
struct VsockConn {
    int fd{-1};
    uint16_t port{};
    bool connected{};
};

// Multiplexed frame header: [stream_id:1][length:4][payload:N]
struct MuxFrame {
    uint8_t stream_id{};
    uint32_t length{};
    uint8_t payload[MAX_FRAME_PAYLOAD]{};
};

// Process execution result
struct ExecResult {
    int exit_code{-1};
    bool signaled{};
    int signal_num{};
};

// Fault tombstone trace
struct TombstoneTrace {
    int signal_num{};
    void* fault_addr{};
    void* pc{};
    char message[512]{};
};

// Signal handler state (async-signal-safe)
struct SignalState {
    volatile sig_atomic_t app_pid{};
    volatile sig_atomic_t app_exited{};
    volatile sig_atomic_t app_exit_status{};
};

// RPC binding definition
struct RpcBindingDef {
    char method[64]{};
    uint8_t request_type{};
    uint8_t response_type{};
};

} // namespace meld::init
