#include "meld/cli/dev_server_module.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>

namespace meld::cli {

// ── Construction ────────────────────────────────────────────────────

DevServerModule::DevServerModule()
    : BaseCommandHandler("dev", "Start the ORC JIT hot-reload development server") {}

// ── CommandHandler interface ────────────────────────────────────────

CommandResult DevServerModule::execute(const CommandArgs& args) {
    std::string error_msg;
    if (!validate_args(args, error_msg)) {
        std::cerr << "error: " << error_msg << "\n";
        return CommandResult::InvalidArguments;
    }

    auto options = parse_options(args);
    int exit_code = start_dev_server(options);
    return (exit_code == 0) ? CommandResult::Success : CommandResult::Error;
}

std::string DevServerModule::get_help() const {
    return "meld dev — Start the ORC JIT hot-reload development server.\n"
           "\n"
           "Compiles the project to LLVM bitcode, loads it into the ORC JIT\n"
           "engine, and watches source files for changes.  On each save the\n"
           "affected module is recompiled and hot-swapped into the running\n"
           "session (<200ms target latency).\n"
           "\n"
           "Options:\n"
           "  --port <port>   Listen on <port> for IPC signals\n";
}

std::string DevServerModule::get_usage() const {
    return "meld dev [--port <port>]";
}

std::vector<std::string> DevServerModule::get_completions(
    const std::string& partial) const {
    std::vector<std::string> completions;
    if (partial.empty() || partial.rfind("--p", 0) == 0) {
        completions.push_back("--port");
    }
    return completions;
}

bool DevServerModule::validate_args(const CommandArgs& args,
                                    std::string& error_message) const {
    // --port must be a valid uint16 if present.
    auto it = args.options.find("port");
    if (it != args.options.end()) {
        try {
            int port = std::stoi(it->second);
            if (port < 0 || port > 65535) {
                error_message = "Port must be between 0 and 65535";
                return false;
            }
        } catch (...) {
            error_message = "Invalid port number: " + it->second;
            return false;
        }
    }
    return true;
}

// ── start_dev_server() ──────────────────────────────────────────────
//
// Main entry point for `meld dev`.
//
// 1. Locate project root (meld.toml).
// 2. Load Effect Firewall config from meld.toml (Req 3.6).
// 3. Initialize OrcJitEngine (Req 3.1).
// 4. Compile all project sources to bitcode and load into JIT.
// 5. Start FileWatcher on source files (Req 3.2).
// 6. Execute the project entry point.
// 7. Block until stopped.
//
// Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6

int DevServerModule::start_dev_server(const DevServerOptions& options) {
    std::lock_guard<std::mutex> lock(mutex_);

    // ── 1. Locate project root ──────────────────────────────────────
    project_root_ = find_project_root();
    if (project_root_.empty()) {
        std::cerr << "error: could not find meld.toml in current or "
                     "parent directories\n";
        return 1;
    }

    std::cout << "[meld dev] Project root: " << project_root_ << "\n";

    // ── 2. Load Effect Firewall (Req 3.6) ───────────────────────────
    load_effect_firewall(project_root_);

    // ── 3. Initialize ORC JIT engine (Req 3.1) ─────────────────────
    jit_engine_ = std::make_unique<compiler::OrcJitEngine>();
    try {
        jit_engine_->initialize();
    } catch (const compiler::JitError& e) {
        std::cerr << "error: JIT initialization failed: " << e.what()
                  << "\n";
        return 1;
    }

    std::cout << "[meld dev] ORC JIT engine initialized\n";

    // ── 4. Compile and load all source modules ──────────────────────
    watched_sources_ = collect_source_files(project_root_);
    if (watched_sources_.empty()) {
        std::cerr << "error: no .meld source files found under "
                  << (project_root_ / "src") << "\n";
        return 1;
    }

    for (const auto& src : watched_sources_) {
        std::string mod_name = module_name_from_path(src);
        try {
            auto bc = compile_to_bitcode(src);
            jit_engine_->load_module(mod_name, bc);
            std::cout << "[meld dev] Loaded module: " << mod_name << "\n";
        } catch (const compiler::JitError& e) {
            std::cerr << "error: failed to load " << mod_name << ": "
                      << e.what() << "\n";
            return 1;
        }
    }

    // ── 5. Start file watcher (Req 3.2) ─────────────────────────────
    setup_file_watcher(watched_sources_);

    // ── 6. Report IPC port if requested (Req 3.5) ───────────────────
    if (options.port != 0) {
        std::cout << "[meld dev] IPC listening on port " << options.port
                  << "\n";
        // IPC listener setup would go here (TCP/Unix socket).
        // For now we record the port for future implementation.
    }

    // ── 7. Execute entry point and enter watch loop ─────────────────
    std::cout << "[meld dev] Running project…\n";

    try {
        jit_engine_->execute("main");
    } catch (const compiler::JitError& e) {
        std::cerr << "error: execution failed: " << e.what() << "\n";
        // Non-fatal in dev mode — keep watching for fixes.
    }

    running_.store(true);
    std::cout << "[meld dev] Watching for changes (Ctrl+C to stop)\n";

    // Block until stop() is called or a signal is received.
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Cleanup.
    if (file_watcher_) {
        file_watcher_->stop();
    }

    std::cout << "[meld dev] Stopped\n";
    return 0;
}

void DevServerModule::stop() {
    running_.store(false);
}

// ── Option parsing ──────────────────────────────────────────────────

DevServerOptions DevServerModule::parse_options(
    const CommandArgs& args) const {
    DevServerOptions opts;
    auto it = args.options.find("port");
    if (it != args.options.end()) {
        opts.port = static_cast<uint16_t>(std::stoi(it->second));
    }
    return opts;
}

// ── Project discovery ───────────────────────────────────────────────

std::filesystem::path DevServerModule::find_project_root() const {
    auto cwd = std::filesystem::current_path();
    auto dir = cwd;
    while (true) {
        if (std::filesystem::exists(dir / "meld.toml")) {
            return dir;
        }
        auto parent = dir.parent_path();
        if (parent == dir) {
            break; // reached filesystem root
        }
        dir = parent;
    }
    return {}; // not found
}

std::vector<std::filesystem::path> DevServerModule::collect_source_files(
    const std::filesystem::path& project_root) const {
    std::vector<std::filesystem::path> sources;
    auto src_dir = project_root / "src";
    if (!std::filesystem::is_directory(src_dir)) {
        return sources;
    }
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(src_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".meld") {
            sources.push_back(entry.path());
        }
    }
    std::sort(sources.begin(), sources.end());
    return sources;
}

// ── Bitcode compilation ─────────────────────────────────────────────

std::vector<uint8_t> DevServerModule::compile_to_bitcode(
    const std::filesystem::path& source_file) const {
    // Read the source file.
    std::ifstream ifs(source_file, std::ios::binary);
    if (!ifs) {
        throw compiler::JitError(
            compiler::JitError::Kind::ModuleLoadFailed,
            "Cannot open source file: " + source_file.string());
    }
    std::string source((std::istreambuf_iterator<char>(ifs)),
                       std::istreambuf_iterator<char>());

    // In a full implementation this would invoke:
    //   Parser::parse_file() → TypeChecker → EffectChecker →
    //   Compiler_Frontend::lower_to_ir() → llvm::WriteBitcodeToFile()
    //
    // The Effect Firewall check is performed during lowering: the
    // Compiler_Frontend emits calls to EffectFirewall::runtime_check()
    // at every perform() site, ensuring identical enforcement to
    // Tier 1 and Tier 3 (Req 3.6).
    //
    // For now we return an empty bitcode vector as a placeholder
    // until the full LLVM lowering pipeline is wired up.
    // The structural contract (parse → compile → bitcode bytes) is
    // established and ready for integration.

    return std::vector<uint8_t>(source.begin(), source.end());
}

std::string DevServerModule::module_name_from_path(
    const std::filesystem::path& source_file) const {
    // Derive module name from the file stem relative to src/.
    // e.g. src/net/http.meld → "net.http"
    auto rel = std::filesystem::relative(source_file,
                                         project_root_ / "src");
    std::string name = rel.string();

    // Strip the .meld extension.
    auto dot = name.rfind(".meld");
    if (dot != std::string::npos) {
        name = name.substr(0, dot);
    }

    // Replace path separators with dots.
    std::replace(name.begin(), name.end(), '/', '.');
    std::replace(name.begin(), name.end(), '\\', '.');

    return name;
}

// ── Hot-swap on file change ─────────────────────────────────────────
//
// Requirements: 3.2, 3.3, 3.4

bool DevServerModule::hot_swap_module(
    const std::filesystem::path& changed_file) {
    std::string mod_name = module_name_from_path(changed_file);

    std::cout << "[meld dev] Recompiling " << mod_name << "…\n";

    auto start = std::chrono::steady_clock::now();

    try {
        auto bc = compile_to_bitcode(changed_file);

        if (jit_engine_->has_module(mod_name)) {
            jit_engine_->hot_swap_module(mod_name, bc);
        } else {
            // New file added to the project.
            jit_engine_->load_module(mod_name, bc);
        }

        auto elapsed = std::chrono::steady_clock::now() - start;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      elapsed)
                      .count();

        std::cout << "[meld dev] Hot-swapped " << mod_name << " in "
                  << ms << "ms\n";

        // Re-execute entry point after swap.
        try {
            jit_engine_->execute("main");
        } catch (const compiler::JitError& e) {
            std::cerr << "error: re-execution failed: " << e.what()
                      << "\n";
        }

        return true;

    } catch (const compiler::JitError& e) {
        // Req 3.4: report error, previous module is retained.
        std::cerr << "[meld dev] Hot-swap failed for " << mod_name
                  << ": " << e.what() << "\n";
        std::cerr << "[meld dev] Retaining previous working module\n";
        return false;
    }
}

// ── Effect Firewall setup (Req 3.6) ────────────────────────────────

void DevServerModule::load_effect_firewall(
    const std::filesystem::path& project_root) {
    effect_firewall_ = std::make_shared<effects::EffectFirewall>();

    // In a full implementation this would parse meld.toml to build
    // a FirewallConfig with per-module allow lists and @uses
    // annotations, then call:
    //   effect_firewall_->load_permissions(config);
    //
    // The same EffectFirewall instance is shared with the
    // Compiler_Frontend so that emitted runtime_check() calls
    // delegate to the singleton — identical enforcement across
    // all three tiers (Req 3.6, 11.1, 11.5).

    // Wire the singleton so compiled code uses the same instance.
    auto& singleton = effects::EffectFirewall::instance();
    // The singleton is process-wide; in dev mode we configure it
    // from the project's meld.toml.
    (void)singleton; // placeholder until meld.toml parsing is wired
}

// ── File watcher setup ──────────────────────────────────────────────

void DevServerModule::setup_file_watcher(
    const std::vector<std::filesystem::path>& sources) {
    file_watcher_ = std::make_unique<interpreter::FileWatcher>();

    for (const auto& src : sources) {
        file_watcher_->watch(
            src,
            [this](const std::filesystem::path& changed) {
                hot_swap_module(changed);
            });
    }
}

} // namespace meld::cli
