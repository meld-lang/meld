#include "meld/manifest/sandbox_provider.hpp"

#include <array>
#include <cstdio>
#include <sstream>

namespace meld::manifest {

std::unique_ptr<SandboxProvider> create_default_provider() {
    // In production: check for SRT availability, return MeldSRTProvider
    // Fallback to PassthroughProvider
    return std::make_unique<PassthroughProvider>();
}

SandboxResult PassthroughProvider::launch(
    const SandboxConfig& /*config*/,
    const std::vector<std::string>& command) {

    if (command.empty()) {
        return {1, "", "No command specified", true};
    }

    // Build command string
    std::ostringstream cmd;
    for (size_t i = 0; i < command.size(); ++i) {
        if (i > 0) cmd << " ";
        cmd << command[i];
    }

    // Execute without sandboxing
    std::array<char, 4096> buffer{};
    std::string output;
    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) {
        return {1, "", "Failed to execute command", true};
    }
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe)) {
        output += buffer.data();
    }
    int exit_code = pclose(pipe);

    return {exit_code, output, "", false};
}

}  // namespace meld::manifest
