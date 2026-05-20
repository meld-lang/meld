#include "meld/daemon/daemon.hpp"
#include "meld/daemon/lsp_channel.hpp"
#include "meld/daemon/mcp_channel.hpp"
#include "meld/daemon/semantic_model.hpp"
#include "meld/daemon/file_watcher.hpp"
#include "meld/daemon/incremental_analyzer.hpp"
#include "meld/daemon/bazel_worker.hpp"
#include "meld/daemon/dependency_graph.hpp"
#include "meld/daemon/sandbox_lifecycle.hpp"
#include "meld/daemon/integrity_verifier.hpp"
#include "meld/daemon/mcp_advanced_tool_provider.hpp"
#include "meld/daemon/passthrough_embedding_provider.hpp"
#include "meld/daemon/vector_index.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <thread>

#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/un.h>

namespace meld::daemon {

namespace fs = std::filesystem;

// ============================================================================
// Workspace singleton lock
// Ensures only one meldd per workspace. The lock file contains PID and ports
// so clients (IDE, AI agents) can discover and connect to the running daemon.
// ============================================================================

struct DaemonLock {
    int fd = -1;
    fs::path path;

    static fs::path lock_path(const fs::path& workspace) {
        return workspace / ".meld" / "daemon.lock";
    }

    // Try to acquire exclusive lock. Returns false if another daemon owns it.
    bool acquire(const fs::path& workspace) {
        path = lock_path(workspace);
        fs::create_directories(path.parent_path());
        fd = open(path.c_str(), O_CREAT | O_RDWR, 0600);
        if (fd < 0) return false;
        if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
            close(fd);
            fd = -1;
            return false;
        }
        return true;
    }

    // Write connection info so clients can discover us
    void write_info(pid_t pid, int lsp_port, int mcp_port) {
        if (fd < 0) return;
        ftruncate(fd, 0);
        lseek(fd, 0, SEEK_SET);
        auto sock_path = (path.parent_path() / "mcp.sock").string();
        auto info = "{\"pid\":" + std::to_string(pid) +
                    ",\"lsp_port\":" + std::to_string(lsp_port) +
                    ",\"mcp_port\":" + std::to_string(mcp_port) +
                    ",\"mcp_socket\":\"" + sock_path + "\"}\n";
        [[maybe_unused]] auto _ = write(fd, info.c_str(), info.size());
    }

    // Read existing daemon info (for clients)
    static std::string read_info(const fs::path& workspace) {
        auto p = lock_path(workspace);
        if (!fs::exists(p)) return "";
        std::ifstream f(p);
        std::string content;
        std::getline(f, content);
        return content;
    }

    // Check if the PID in the lock file is still alive
    static bool is_alive(const fs::path& workspace) {
        auto p = lock_path(workspace);
        if (!fs::exists(p)) return false;
        std::ifstream f(p);
        std::string content;
        std::getline(f, content);
        // Extract PID from JSON
        auto pos = content.find("\"pid\":");
        if (pos == std::string::npos) return false;
        int pid = std::stoi(content.substr(pos + 6));
        return kill(pid, 0) == 0;
    }

    void release() {
        if (fd >= 0) {
            flock(fd, LOCK_UN);
            close(fd);
            fd = -1;
        }
        if (!path.empty()) {
            fs::remove(path);
        }
    }

    ~DaemonLock() { release(); }
};

MeldDaemon::MeldDaemon(DaemonConfig config)
    : config_(std::move(config)) {}

MeldDaemon::~MeldDaemon() {
    if (running_.load()) {
        shutdown();
    }
}

void MeldDaemon::run() {
    // Ignore SIGPIPE process-wide (prevents thread crashes on broken connections)
    signal(SIGPIPE, SIG_IGN);

    running_.store(true);

    // Clean up any stale resources from a previous crash (Req 8.4)
    cleanup_stale_resources();

    // Start accepting connections immediately (lazy init, Req 8.1)
    // Background indexing happens in initialize_subsystems()
    initialize_subsystems();

    // Enter the main event loop
    run_event_loop();
}

void MeldDaemon::shutdown() {
    if (!running_.exchange(false)) return;

    // Wait for indexing to finish
    if (indexing_thread_.joinable()) indexing_thread_.join();

    // Req 8.3: Graceful shutdown sequence
    // 1. Stop accepting new requests (handled by setting running_ = false)
    // 2. Wait for in-flight work (subsystems check running_ flag)
    // 3. Terminate active sandboxed processes
    if (sandbox_manager_) {
        // sandbox_manager_->terminate_all();
    }
    // 4. Clean up stale SRT policy files
    cleanup_stale_resources();
}

void MeldDaemon::on_diagnostics_updated(DiagnosticCallback cb) {
    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    diagnostic_subscribers_.push_back(std::move(cb));
}

void MeldDaemon::publish_diagnostics(const std::vector<Diagnostic>& diags) {
    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    for (const auto& cb : diagnostic_subscribers_) {
        cb(diags);
    }
}

void MeldDaemon::initialize_subsystems() {
    // Subsystems are created but start background work
    // LSP/MCP channels begin accepting connections immediately
    // Workspace indexing runs in background, publishing diagnostics incrementally

    // Initialize dependency graph
    if (!dependency_graph_) {
        dependency_graph_ = std::make_unique<DependencyGraph>();
    }

    // Initialize vector index with passthrough embeddings (keyword search only)
    if (!vector_index_) {
        vector_index_ = std::make_unique<VectorIndex>(
            std::make_shared<PassthroughEmbeddingProvider>());
    }

    // Create IncrementalAnalyzer
    if (!incremental_analyzer_) {
        incremental_analyzer_ = std::make_unique<IncrementalAnalyzer>(model_);
    }

    // Run initial workspace indexing in a background thread (don't block startup)
    indexing_thread_ = std::thread([this]() {
        try {
            std::cerr << "[meldd] Indexing workspace (" << config_.workspace.string() << ")...\n";
            if (config_.verbose) {
                std::cerr << "[meldd] Starting workspace indexing...\n";
            }
            auto indexed = incremental_analyzer_->analyze_workspace(config_.workspace);
            // Publish diagnostics for all initially indexed files
            for (const auto& file : indexed) {
                try {
                    auto diags = model_.get_diagnostics(file);
                    if (!diags.empty()) {
                        publish_diagnostics(diags);
                    }
                } catch (...) {
                    // Skip files that cause diagnostic errors
                }
            }
            if (config_.verbose) {
                std::cerr << "[meldd] Initial indexing complete: "
                          << indexed.size() << " files\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "[meldd] Initial indexing failed: " << e.what() << "\n";
        } catch (...) {
            std::cerr << "[meldd] Initial indexing failed with unknown error\n";
        }
    });

    // Create FileWatcher and connect it to the analyzer
    if (!file_watcher_) {
        file_watcher_ = std::make_unique<FileWatcher>(config_.workspace);
        file_watcher_->start([this](const std::vector<FileChangeEvent>& events) {
            try {
                auto updated = incremental_analyzer_->analyze_changes(events);
                for (const auto& file : updated) {
                    auto diags = model_.get_diagnostics(file);
                    publish_diagnostics(diags);
                }
            } catch (...) {
                // Don't crash the daemon on file watcher errors
            }
        });
    }

    initialized_.store(true);

    if (config_.verbose) {
        std::cerr << "[meldd] Initialized for workspace: "
                  << config_.workspace.string() << "\n";
    }
}

void MeldDaemon::cleanup_stale_resources() {
    // Req 8.4: On startup after crash, detect and clean up orphaned resources
    // Scan temp directory for stale srt-settings.json files
    namespace fs = std::filesystem;
    auto tmp = fs::temp_directory_path() / "meld-daemon";
    if (fs::exists(tmp)) {
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(tmp, ec)) {
            if (entry.path().extension() == ".json" &&
                entry.path().filename().string().find("srt-settings") != std::string::npos) {
                fs::remove(entry.path(), ec);
            }
        }
    }
}

void MeldDaemon::run_event_loop() {
    auto meld_dir = config_.workspace / ".meld";
    fs::create_directories(meld_dir);

    // Helper: create a Unix domain socket listener
    auto make_listener = [&](const fs::path& sock_path) -> int {
        fs::remove(sock_path);  // clean up stale socket
        int fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) return -1;

        struct sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        auto path_str = sock_path.string();
        strncpy(addr.sun_path, path_str.c_str(), sizeof(addr.sun_path) - 1);

        if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 ||
            listen(fd, 4) != 0) {
            close(fd);
            return -1;
        }
        return fd;
    };

    // Helper: accept loop for a socket, spawning a handler per client
    auto accept_loop = [this](int server_fd, const fs::path& sock_path,
                              const std::string& channel_name,
                              std::function<void(int)> handler) {
        if (config_.verbose) {
            std::cerr << "[meldd] " << channel_name << " listening on "
                      << sock_path.string() << "\n";
        }
        while (running_.load()) {
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(server_fd, &fds);
            struct timeval tv{1, 0};
            int ret = select(server_fd + 1, &fds, nullptr, nullptr, &tv);
            if (ret <= 0) continue;

            int client_fd = accept(server_fd, nullptr, nullptr);
            if (client_fd < 0) continue;

            std::thread([handler, client_fd]() {
                handler(client_fd);
            }).detach();
        }
        close(server_fd);
        fs::remove(sock_path);
    };

    // --- LSP socket ---
    auto lsp_sock_path = meld_dir / "lsp.sock";
    int lsp_fd = make_listener(lsp_sock_path);
    if (lsp_fd < 0) {
        std::cerr << "[meldd] Failed to create LSP socket at "
                  << lsp_sock_path.string() << "\n";
    }

    // --- MCP socket ---
    auto mcp_sock_path = meld_dir / "mcp.sock";
    int mcp_fd = make_listener(mcp_sock_path);
    if (mcp_fd < 0) {
        std::cerr << "[meldd] Failed to create MCP socket at "
                  << mcp_sock_path.string() << "\n";
    }

    // Write daemon info for client discovery
    DaemonLock lock;
    if (lock.acquire(config_.workspace)) {
        lock.write_info(getpid(), 0, 0);
    }

    if (config_.verbose) {
        std::cerr << "[meldd] Daemon ready (PID " << getpid() << ")\n";
    }

    // Start accept threads for both channels
    std::thread lsp_thread;
    if (lsp_fd >= 0) {
        lsp_thread = std::thread([&]() {
            accept_loop(lsp_fd, lsp_sock_path, "LSP", [this](int client_fd) {
                LspChannel lsp(model_, incremental_analyzer_.get());
                lsp.run_on_fd(client_fd);
            });
        });
    }

    std::thread mcp_thread;
    if (mcp_fd >= 0) {
        mcp_thread = std::thread([&]() {
            accept_loop(mcp_fd, mcp_sock_path, "MCP", [this](int client_fd) {
                McpChannel mcp(model_);

                // Register Code Mode tools (Req 36)
                McpAdvancedToolProvider adv(model_, *dependency_graph_, *vector_index_);

                mcp.register_tool("search_api", [&adv](const nlohmann::json& args) {
                    auto query = args.value("query", "");
                    auto results = adv.search_api(query);
                    nlohmann::json arr = nlohmann::json::array();
                    for (const auto& r : results) {
                        arr.push_back({
                            {"name", r.name},
                            {"kind", r.kind},
                            {"definition", r.definition},
                            {"rpc_bindings", r.rpc_bindings}
                        });
                    }
                    return arr;
                });

                mcp.register_tool("execute_script", [&adv](const nlohmann::json& args) {
                    auto source = args.value("source", "");
                    auto timeout = args.value("timeout_ms", 5000u);
                    auto memory = args.value("memory_limit_mb", 16u);
                    auto deterministic = args.value("deterministic", false);

                    // Run with a hard timeout to prevent blocking the MCP handler
                    auto future = std::async(std::launch::async, [&]() {
                        return adv.execute_script(source, timeout, memory, deterministic);
                    });

                    auto status = future.wait_for(std::chrono::milliseconds(timeout + 1000));
                    ScriptExecutionResult result;
                    if (status == std::future_status::timeout) {
                        result.success = false;
                        result.output = "Execution timed out";
                    } else {
                        result = future.get();
                    }

                    nlohmann::json diags = nlohmann::json::array();
                    for (const auto& d : result.diagnostics) {
                        diags.push_back({
                            {"rule_id", d.rule_id},
                            {"ast_selector", d.ast_selector},
                            {"message", d.message},
                            {"severity", d.severity}
                        });
                    }
                    nlohmann::json j = {
                        {"success", result.success},
                        {"output", result.output},
                        {"diagnostics", diags}
                    };
                    if (result.deterministic_config) {
                        j["deterministic_config"] = {
                            {"seed", result.deterministic_config->seed}
                        };
                    }
                    return j;
                });

                mcp.run_on_fd(client_fd);
            });
        });
    }

    // Block until shutdown signal
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    if (lsp_thread.joinable()) lsp_thread.join();
    if (mcp_thread.joinable()) mcp_thread.join();
}

// --- Deterministic Context (Req 11.6, 11.7) ---

void MeldDaemon::activate_deterministic(const DeterministicConfig& cfg) {
    deterministic_ctx_ = std::make_unique<DeterministicContext>(cfg);
    deterministic_handlers_ = DeterministicHandlerSet::create(*deterministic_ctx_);
}

void MeldDaemon::deactivate_deterministic() {
    deterministic_handlers_.reset();
    deterministic_ctx_.reset();
}

bool MeldDaemon::is_deterministic() const {
    return deterministic_ctx_ != nullptr;
}

std::optional<DeterministicConfig> MeldDaemon::deterministic_config() const {
    if (deterministic_ctx_) {
        return deterministic_ctx_->config();
    }
    return std::nullopt;
}

}  // namespace meld::daemon
