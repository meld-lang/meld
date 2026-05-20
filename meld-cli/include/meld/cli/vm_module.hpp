#pragma once

#include "command_handler.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace meld::cli {

/// Options for `meld vm logs`.
struct VmLogOptions {
    bool follow = true;  // default: tail mode; --no-follow disables
};

/// Status information for the Lima VM or native Linux host.
struct VmStatus {
    bool running = false;
    std::string ram_usage;
    std::string cpu_usage;
    uint32_t active_microvms = 0;
    uint32_t active_containers = 0;
};

/// Output captured from a subprocess.
struct CommandOutput {
    std::string stdout_text;
    int exit_code = 0;
};

/**
 * Handles the `meld vm` subcommand.
 * Manages the background Alpine Linux Lima VM that hosts Firecracker and
 * containerd on macOS.  On native Linux all subcommands except `status`
 * print an info message and exit — Lima is not needed.
 *
 * Requirements: 25.1–25.11
 */
class VmModule : public BaseCommandHandler {
public:
    VmModule();
    ~VmModule() override = default;

    CommandResult execute(const CommandArgs& args) override;
    std::string get_help() const override;
    std::string get_usage() const override;
    std::vector<std::string> get_completions(const std::string& partial) const override;
    bool validate_args(const CommandArgs& args, std::string& error_message) const override;

private:
    CommandResult handle_status(bool json_output);
    CommandResult handle_start(bool json_output);
    CommandResult handle_stop(bool json_output);
    CommandResult handle_restart(bool json_output);
    CommandResult handle_shell(bool json_output);
    CommandResult handle_prune(bool json_output);
    CommandResult handle_logs(const VmLogOptions& opts);

    /// Returns true if running on native Linux with /dev/kvm available.
    bool is_native_linux() const;

    /// Run a limactl command and capture output.
    CommandOutput run_limactl(const std::vector<std::string>& args) const;

    /// Run a command inside the Lima VM via `limactl shell meld-vm`.
    CommandOutput run_in_vm(const std::vector<std::string>& args) const;
};

} // namespace meld::cli
