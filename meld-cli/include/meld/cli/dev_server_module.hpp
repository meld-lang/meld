#pragma once

#include "command_handler.hpp"
#include "error_handler.hpp"

#include <meld/compiler/orc_jit_engine.hpp>
#include <meld/effects/effect_firewall.hpp>
#include <meld/interpreter/file_watcher.hpp>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace meld::cli {

/**
 * Options for the `meld dev` hot-reload development server.
 *
 * Requirements: 3.1, 3.5
 */
struct DevServerOptions {
    uint16_t port = 0;  ///< IPC port (0 = no IPC listener)
};

/**
 * DevServerModule — CLI handler for `meld dev`.
 *
 * Starts an LLVM ORC JIT development server that watches project
 * source files and hot-swaps recompiled modules on change.
 *
 * Workflow:
 *   1. Parse `--port` flag.
 *   2. Initialize OrcJitEngine.
 *   3. Compile the project to bitcode and load into JIT.
 *   4. Start FileWatcher on project `.meld` sources.
 *   5. On file change → recompile affected module → hot_swap_module.
 *   6. Enforce Effect Firewall identically to Tier 1 and Tier 3.
 *
 * Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6
 */
class DevServerModule : public BaseCommandHandler {
public:
    DevServerModule();
    ~DevServerModule() override = default;

    // ── CommandHandler interface ─────────────────────────────────────

    CommandResult execute(const CommandArgs& args) override;
    std::string get_help() const override;
    std::string get_usage() const override;
    std::vector<std::string> get_completions(const std::string& partial) const override;
    bool validate_args(const CommandArgs& args, std::string& error_message) const override;

    // ── Public API ──────────────────────────────────────────────────

    /**
     * Start the ORC JIT dev server with the given options.
     *
     * Blocks until the server is stopped (e.g. via signal or IPC).
     * Returns the process exit code.
     */
    int start_dev_server(const DevServerOptions& options);

    /**
     * Request a graceful shutdown of the running dev server.
     */
    void stop();

    /**
     * Check whether the dev server is currently running.
     */
    bool is_running() const noexcept { return running_.load(); }

private:
    std::unique_ptr<compiler::OrcJitEngine> jit_engine_;
    std::unique_ptr<interpreter::FileWatcher> file_watcher_;
    std::shared_ptr<effects::EffectFirewall> effect_firewall_;

    std::atomic<bool> running_{false};
    mutable std::mutex mutex_;

    /// Project root directory (location of meld.toml).
    std::filesystem::path project_root_;

    /// Cached list of source files being watched.
    std::vector<std::filesystem::path> watched_sources_;

    // ── Internal helpers ────────────────────────────────────────────

    /// Parse DevServerOptions from CLI arguments.
    DevServerOptions parse_options(const CommandArgs& args) const;

    /// Discover the project root by searching for meld.toml.
    std::filesystem::path find_project_root() const;

    /// Collect all `.meld` source files under the project src/ dir.
    std::vector<std::filesystem::path> collect_source_files(
        const std::filesystem::path& project_root) const;

    /// Compile a single `.meld` source file to LLVM bitcode bytes.
    std::vector<uint8_t> compile_to_bitcode(
        const std::filesystem::path& source_file) const;

    /// Derive a logical module name from a source file path.
    std::string module_name_from_path(
        const std::filesystem::path& source_file) const;

    /// Hot-swap a single module after a file change.
    /// Returns true on success, false if the swap failed (previous
    /// module is retained per Req 3.4).
    bool hot_swap_module(const std::filesystem::path& changed_file);

    /// Load the Effect Firewall configuration from meld.toml.
    void load_effect_firewall(const std::filesystem::path& project_root);

    /// Set up the FileWatcher on all project source files.
    void setup_file_watcher(
        const std::vector<std::filesystem::path>& sources);
};

} // namespace meld::cli
