// firecracker_provider.hpp — Firecracker MicroVM Sandbox Provider (Req 18)

#pragma once
#include "meld/manifest/sandbox_provider.hpp"
#include <string>

namespace meld::manifest {

/// Execution configuration from meld.toml [execution] section
struct ExecutionConfig {
    std::string isolation = "process";       // "process" or "microvm"
    std::string runtime = "firecracker";     // "firecracker" or "cloud-hypervisor"
    std::string rootfs;                      // Optional rootfs path
    int vcpu_count = 1;
    int mem_size_mib = 128;
};

/// Firecracker MicroVM sandbox provider (Req 18)
/// Launches binaries inside Firecracker MicroVMs for hardware-level isolation
class FirecrackerProvider : public SandboxProvider {
public:
    explicit FirecrackerProvider(const ExecutionConfig& config = {});

    SandboxResult launch(const SandboxConfig& config,
                         const std::vector<std::string>& command) override;
    std::string name() const override { return "Firecracker"; }

private:
    ExecutionConfig exec_config_;

    std::string build_vm_config(const SandboxConfig& config,
                                 const std::vector<std::string>& command) const;
    std::string find_firecracker_binary() const;
};

/// Updated factory: select provider based on ExecutionConfig
std::unique_ptr<SandboxProvider> create_provider(const ExecutionConfig& config);

} // namespace meld::manifest
