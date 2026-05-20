#include "meld/cli/vm_module.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <thread>

namespace meld::cli {

// -----------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------

VmModule::VmModule()
    : BaseCommandHandler("vm", "Manage the background Lima VM (Firecracker/containerd host)") {
}

// -----------------------------------------------------------------------
// execute — route to sub-subcommand
// -----------------------------------------------------------------------

CommandResult VmModule::execute(const CommandArgs& args) {
    bool json_output = args.flags.count("json") > 0;

    std::string subcmd;
    if (!args.positional.empty()) {
        subcmd = args.positional[0];
    }

    if (subcmd.empty() || subcmd == "status") {
        return handle_status(json_output);
    }
    if (subcmd == "start") {
        return handle_start(json_output);
    }
    if (subcmd == "stop") {
        return handle_stop(json_output);
    }
    if (subcmd == "restart") {
        return handle_restart(json_output);
    }
    if (subcmd == "shell") {
        return handle_shell(json_output);
    }
    if (subcmd == "prune") {
        return handle_prune(json_output);
    }
    if (subcmd == "logs") {
        VmLogOptions lopts;
        lopts.follow = args.flags.count("no-follow") == 0;
        return handle_logs(lopts);
    }

    std::cerr << "error: unknown vm subcommand: " << subcmd << std::endl;
    std::cerr << "usage: " << get_usage() << std::endl;
    return CommandResult::InvalidArguments;
}

// -----------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------

bool VmModule::is_native_linux() const {
#ifdef __linux__
    return std::filesystem::exists("/dev/kvm");
#else
    return false;
#endif
}

CommandOutput VmModule::run_limactl(const std::vector<std::string>& args) const {
    std::ostringstream cmd;
    cmd << "limactl";
    for (const auto& a : args) {
        cmd << " " << a;
    }
    cmd << " 2>&1";

    CommandOutput out;
    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) {
        out.exit_code = -1;
        return out;
    }
    char buf[256];
    while (fgets(buf, sizeof(buf), pipe) != nullptr) {
        out.stdout_text += buf;
    }
    int status = pclose(pipe);
#ifndef _WIN32
    out.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#else
    out.exit_code = status;
#endif
    return out;
}

CommandOutput VmModule::run_in_vm(const std::vector<std::string>& args) const {
    std::vector<std::string> full_args = {"shell", "meld-vm", "--"};
    full_args.insert(full_args.end(), args.begin(), args.end());
    return run_limactl(full_args);
}

// -----------------------------------------------------------------------
// 57.2  handle_status — query VM state  (Req 25.1, 25.9, 25.10)
// -----------------------------------------------------------------------

CommandResult VmModule::handle_status(bool json_output) {
    if (is_native_linux()) {
        // On native Linux report KVM and containerd status.
        bool kvm_available = std::filesystem::exists("/dev/kvm");
        // Check if containerd is running via pidof.
        CommandOutput ctd;
        FILE* pipe = popen("pidof containerd 2>/dev/null", "r");
        bool ctd_running = false;
        if (pipe) {
            char buf[64];
            if (fgets(buf, sizeof(buf), pipe) != nullptr) {
                ctd_running = true;
            }
            pclose(pipe);
        }

        if (json_output) {
            std::cout << "{\"native_linux\":true,\"kvm_available\":"
                      << (kvm_available ? "true" : "false")
                      << ",\"containerd_running\":"
                      << (ctd_running ? "true" : "false")
                      << "}" << std::endl;
        } else {
            std::cout << "Native Linux host" << std::endl;
            std::cout << "  KVM: " << (kvm_available ? "available" : "unavailable") << std::endl;
            std::cout << "  containerd: " << (ctd_running ? "running" : "stopped") << std::endl;
        }
        return CommandResult::Success;
    }

    // macOS path — query limactl.
    auto result = run_limactl({"ls", "--json"});
    if (result.exit_code != 0) {
        std::cerr << "error: limactl ls failed: " << result.stdout_text << std::endl;
        return CommandResult::Error;
    }

    VmStatus status;

    // Simple parsing: look for the meld-vm instance in the JSON lines output.
    // limactl ls --json emits one JSON object per line.
    std::istringstream stream(result.stdout_text);
    std::string line;
    bool found = false;
    while (std::getline(stream, line)) {
        if (line.find("\"meld-vm\"") != std::string::npos ||
            line.find("\"name\":\"meld-vm\"") != std::string::npos) {
            found = true;
            status.running = line.find("\"status\":\"Running\"") != std::string::npos;

            // Extract fields with simple substring search.
            auto extract = [&](const std::string& key) -> std::string {
                auto pos = line.find("\"" + key + "\":\"");
                if (pos == std::string::npos) return "";
                pos += key.size() + 4; // skip `"key":"`
                auto end = line.find("\"", pos);
                if (end == std::string::npos) return "";
                return line.substr(pos, end - pos);
            };

            status.ram_usage = extract("memory");
            if (status.ram_usage.empty()) status.ram_usage = "unknown";
            status.cpu_usage = extract("cpus");
            if (status.cpu_usage.empty()) status.cpu_usage = "unknown";
            break;
        }
    }

    if (!found) {
        if (json_output) {
            std::cout << "{\"running\":false,\"instance\":\"meld-vm\",\"error\":\"instance not found\"}" << std::endl;
        } else {
            std::cout << "VM: meld-vm instance not found" << std::endl;
            std::cout << "  Run `meld vm start` to create and boot the VM." << std::endl;
        }
        return CommandResult::Success;
    }

    // Query active microvms and containers inside the VM if running.
    if (status.running) {
        auto fc_out = run_in_vm({"sh", "-c", "ls /run/firecracker-* 2>/dev/null | wc -l"});
        if (fc_out.exit_code == 0) {
            try { status.active_microvms = static_cast<uint32_t>(std::stoul(fc_out.stdout_text)); } catch (...) {}
        }
        auto ct_out = run_in_vm({"nerdctl", "ps", "-q"});
        if (ct_out.exit_code == 0) {
            uint32_t count = 0;
            std::istringstream cs(ct_out.stdout_text);
            std::string cline;
            while (std::getline(cs, cline)) {
                if (!cline.empty()) ++count;
            }
            status.active_containers = count;
        }
    }

    if (json_output) {
        std::cout << "{\"running\":" << (status.running ? "true" : "false")
                  << ",\"instance\":\"meld-vm\""
                  << ",\"ram_usage\":\"" << status.ram_usage << "\""
                  << ",\"cpu_usage\":\"" << status.cpu_usage << "\""
                  << ",\"active_microvms\":" << status.active_microvms
                  << ",\"active_containers\":" << status.active_containers
                  << "}" << std::endl;
    } else {
        std::cout << "VM: " << (status.running ? "running" : "stopped") << std::endl;
        std::cout << "  Instance:    meld-vm" << std::endl;
        std::cout << "  RAM:         " << status.ram_usage << std::endl;
        std::cout << "  CPU:         " << status.cpu_usage << std::endl;
        std::cout << "  MicroVMs:    " << status.active_microvms << std::endl;
        std::cout << "  Containers:  " << status.active_containers << std::endl;
    }
    return CommandResult::Success;
}

// -----------------------------------------------------------------------
// 57.3  handle_start / handle_stop  (Req 25.2, 25.3, 25.9)
// -----------------------------------------------------------------------

CommandResult VmModule::handle_start(bool json_output) {
    if (is_native_linux()) {
        if (json_output) {
            std::cout << "{\"info\":\"Native Linux detected, VMM bridge disabled\"}" << std::endl;
        } else {
            std::cout << "info: Native Linux detected, VMM bridge disabled" << std::endl;
        }
        return CommandResult::Success;
    }

    // Check if already running.
    auto check = run_limactl({"ls", "--json"});
    if (check.exit_code == 0 &&
        check.stdout_text.find("\"meld-vm\"") != std::string::npos &&
        check.stdout_text.find("\"status\":\"Running\"") != std::string::npos) {
        if (json_output) {
            std::cout << "{\"status\":\"already_running\",\"instance\":\"meld-vm\"}" << std::endl;
        } else {
            std::cout << "VM meld-vm is already running." << std::endl;
        }
        return handle_status(json_output);
    }

    if (!json_output) {
        std::cout << "Starting meld-vm..." << std::endl;
    }

    auto result = run_limactl({"start", "meld-vm"});
    if (result.exit_code != 0) {
        std::cerr << "error: failed to start meld-vm: " << result.stdout_text << std::endl;
        return CommandResult::Error;
    }

    if (json_output) {
        std::cout << "{\"status\":\"started\",\"instance\":\"meld-vm\"}" << std::endl;
    } else {
        std::cout << "VM meld-vm started." << std::endl;
    }
    return CommandResult::Success;
}

CommandResult VmModule::handle_stop(bool json_output) {
    if (is_native_linux()) {
        if (json_output) {
            std::cout << "{\"info\":\"Native Linux detected, VMM bridge disabled\"}" << std::endl;
        } else {
            std::cout << "info: Native Linux detected, VMM bridge disabled" << std::endl;
        }
        return CommandResult::Success;
    }

    if (!json_output) {
        std::cout << "Stopping meld-vm..." << std::endl;
    }

    auto result = run_limactl({"stop", "meld-vm"});
    if (result.exit_code != 0) {
        std::cerr << "error: failed to stop meld-vm: " << result.stdout_text << std::endl;
        return CommandResult::Error;
    }

    if (json_output) {
        std::cout << "{\"status\":\"stopped\",\"instance\":\"meld-vm\"}" << std::endl;
    } else {
        std::cout << "VM meld-vm stopped." << std::endl;
    }
    return CommandResult::Success;
}

// -----------------------------------------------------------------------
// 57.4  handle_restart / handle_shell / handle_prune  (Req 25.4–25.6, 25.9)
// -----------------------------------------------------------------------

CommandResult VmModule::handle_restart(bool json_output) {
    if (is_native_linux()) {
        if (json_output) {
            std::cout << "{\"info\":\"Native Linux detected, VMM bridge disabled\"}" << std::endl;
        } else {
            std::cout << "info: Native Linux detected, VMM bridge disabled" << std::endl;
        }
        return CommandResult::Success;
    }

    auto stop_result = handle_stop(false);  // quiet stop
    if (stop_result != CommandResult::Success && stop_result != CommandResult::Error) {
        return stop_result;
    }
    return handle_start(json_output);
}

CommandResult VmModule::handle_shell(bool json_output) {
    if (is_native_linux()) {
        if (json_output) {
            std::cout << "{\"info\":\"Native Linux detected, VMM bridge disabled\"}" << std::endl;
        } else {
            std::cout << "info: Native Linux detected, VMM bridge disabled" << std::endl;
        }
        return CommandResult::Success;
    }

    // Drop into interactive shell — use system() so the user gets a TTY.
    int rc = std::system("limactl shell meld-vm");
    return rc == 0 ? CommandResult::Success : CommandResult::Error;
}

CommandResult VmModule::handle_prune(bool json_output) {
    if (is_native_linux()) {
        if (json_output) {
            std::cout << "{\"info\":\"Native Linux detected, VMM bridge disabled\"}" << std::endl;
        } else {
            std::cout << "info: Native Linux detected, VMM bridge disabled" << std::endl;
        }
        return CommandResult::Success;
    }

    if (!json_output) {
        std::cout << "Pruning containers and images inside meld-vm..." << std::endl;
    }

    auto prune_result = run_in_vm({"nerdctl", "system", "prune", "-f"});
    if (prune_result.exit_code != 0) {
        std::cerr << "error: nerdctl system prune failed: " << prune_result.stdout_text << std::endl;
        return CommandResult::Error;
    }

    // Wipe stale socket files.
    auto sock_result = run_in_vm({"sh", "-c", "rm -f /tmp/meld/*.sock"});

    if (json_output) {
        std::cout << "{\"status\":\"pruned\",\"instance\":\"meld-vm\"}" << std::endl;
    } else {
        std::cout << "Prune complete." << std::endl;
        if (!prune_result.stdout_text.empty()) {
            std::cout << prune_result.stdout_text;
        }
    }
    return CommandResult::Success;
}

// -----------------------------------------------------------------------
// 57.5  handle_logs — tail VM system logs  (Req 25.7, 25.8, 25.9)
// -----------------------------------------------------------------------

CommandResult VmModule::handle_logs(const VmLogOptions& opts) {
    if (is_native_linux()) {
        std::cout << "info: Native Linux detected, VMM bridge disabled" << std::endl;
        return CommandResult::Success;
    }

    // Build the journalctl command to tail Firecracker and containerd logs.
    std::string journal_cmd = "journalctl -u containerd -u firecracker";
    if (opts.follow) {
        journal_cmd += " -f";
    } else {
        journal_cmd += " --no-pager -n 100";
    }

    auto result = run_in_vm({"sh", "-c", journal_cmd});
    if (result.exit_code != 0 && !opts.follow) {
        // Fallback: try reading log files directly.
        auto fallback = run_in_vm({"sh", "-c",
            "tail " + std::string(opts.follow ? "-f " : "-n 100 ") +
            "/var/log/containerd.log /var/log/firecracker.log 2>/dev/null"});
        if (!fallback.stdout_text.empty()) {
            std::cout << fallback.stdout_text;
        } else {
            std::cerr << "error: could not read VM logs" << std::endl;
            return CommandResult::Error;
        }
    } else {
        if (!result.stdout_text.empty()) {
            std::cout << result.stdout_text;
        }
    }

    return CommandResult::Success;
}

// -----------------------------------------------------------------------
// Help / completions
// -----------------------------------------------------------------------

std::string VmModule::get_help() const {
    return R"(VM management commands:

USAGE:
    meld vm <subcommand> [OPTIONS]

SUBCOMMANDS:
    logs        Tail Firecracker and containerd system logs
    prune       Remove unused containers/images and stale sockets
    restart     Hard reboot (stop + start)
    shell       Drop into a root shell inside the VM
    start       Boot the meld-vm Alpine host
    status      Show VM status (default if no subcommand)
    stop        Shut down the meld-vm Alpine host

OPTIONS:
    --json          Output in machine-readable JSON format
    --no-follow     Print logs and exit (logs only)

EXAMPLES:
    meld vm status
    meld vm start
    meld vm stop
    meld vm restart
    meld vm shell
    meld vm prune
    meld vm logs
    meld vm logs --no-follow
    meld vm status --json)";
}

std::string VmModule::get_usage() const {
    return "meld vm <status|start|stop|restart|shell|prune|logs> [OPTIONS]";
}

std::vector<std::string> VmModule::get_completions(const std::string& partial) const {
    std::vector<std::string> all = {
        "--json", "--no-follow",
        "logs", "prune", "restart", "shell", "start", "status", "stop"
    };
    std::vector<std::string> result;
    for (const auto& c : all) {
        if (c.find(partial) == 0) {
            result.push_back(c);
        }
    }
    return result;
}

bool VmModule::validate_args(const CommandArgs& /*args*/,
                              std::string& /*error_message*/) const {
    return true;
}

} // namespace meld::cli
