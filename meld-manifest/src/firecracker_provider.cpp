// firecracker_provider.cpp — Firecracker MicroVM Sandbox Provider (Req 18)

#include "meld/manifest/firecracker_provider.hpp"
#include "meld/manifest/srt_provider.hpp"
#include <nlohmann/json.hpp>
#include <sstream>
#include <cstdlib>
#include <array>
#include <filesystem>

namespace meld::manifest {

namespace fs = std::filesystem;

FirecrackerProvider::FirecrackerProvider(const ExecutionConfig& config)
    : exec_config_(config) {}

std::string FirecrackerProvider::find_firecracker_binary() const {
    // Check common locations
    for (const auto& path : {
        "/usr/local/bin/firecracker",
        "/usr/bin/firecracker",
        "firecracker"
    }) {
        if (fs::exists(path)) return path;
    }
    return "firecracker";  // Rely on PATH
}

std::string FirecrackerProvider::build_vm_config(
    const SandboxConfig& config,
    const std::vector<std::string>& command) const {
    nlohmann::json vm_config;

    // Boot source
    vm_config["boot-source"] = {
        {"kernel_image_path", exec_config_.rootfs.empty()
            ? "/var/lib/meld/vmlinux" : exec_config_.rootfs},
        {"boot_args", "console=ttyS0 reboot=k panic=1 pci=off"}
    };

    // Machine config
    vm_config["machine-config"] = {
        {"vcpu_count", exec_config_.vcpu_count},
        {"mem_size_mib", exec_config_.mem_size_mib}
    };

    // Drives — rootfs
    vm_config["drives"] = nlohmann::json::array({
        {{"drive_id", "rootfs"},
         {"path_on_host", exec_config_.rootfs.empty()
             ? "/var/lib/meld/rootfs.ext4" : exec_config_.rootfs},
         {"is_root_device", true},
         {"is_read_only", !config.vfs_mode}}
    });

    // Network — only if allowed_domains is non-empty
    if (!config.allowed_domains.empty()) {
        vm_config["network-interfaces"] = nlohmann::json::array({
            {{"iface_id", "eth0"},
             {"guest_mac", "AA:FC:00:00:00:01"},
             {"host_dev_name", "tap0"}}
        });
    }

    return vm_config.dump(2);
}

SandboxResult FirecrackerProvider::launch(
    const SandboxConfig& config,
    const std::vector<std::string>& command) {
    SandboxResult result;

    auto fc_binary = find_firecracker_binary();
    auto vm_config = build_vm_config(config, command);

    // Write VM config to temp file
    auto config_path = fs::temp_directory_path() / "meld_fc_config.json";
    {
        std::ofstream f(config_path);
        f << vm_config;
    }

    // Build firecracker command
    std::ostringstream cmd;
    cmd << fc_binary
        << " --api-sock /tmp/meld_fc.sock"
        << " --config-file " << config_path.string();

    // Execute
    std::array<char, 4096> buffer;
    std::string output;
    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) {
        result.sandbox_failed = true;
        result.stderr_output = "Failed to launch Firecracker";
        result.exit_code = -1;
        return result;
    }
    while (fgets(buffer.data(), buffer.size(), pipe)) {
        output += buffer.data();
    }
    result.exit_code = pclose(pipe);
    result.stdout_output = output;

    // Cleanup
    fs::remove(config_path);

    return result;
}

// ── Provider factory ──

std::unique_ptr<SandboxProvider> create_provider(const ExecutionConfig& config) {
    if (config.isolation == "microvm") {
        return std::make_unique<FirecrackerProvider>(config);
    }
    if (config.isolation == "process") {
        return std::make_unique<MeldSRTProvider>();
    }
    return std::make_unique<PassthroughProvider>();
}

} // namespace meld::manifest
