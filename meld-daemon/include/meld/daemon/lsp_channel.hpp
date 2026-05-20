#pragma once

#include "meld/daemon/completion_provider.hpp"
#include "meld/daemon/dual_voice.hpp"
#include "meld/daemon/incremental_analyzer.hpp"
#include "meld/daemon/navigation_provider.hpp"
#include "meld/daemon/semantic_model.hpp"
#include "meld/daemon/semantic_token_provider.hpp"

#include <nlohmann/json.hpp>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace meld::daemon {

/// JSON-RPC message envelope
struct JsonRpcMessage {
    std::string method;
    nlohmann::json params;
    nlohmann::json id;  // null for notifications
};

/// LSP channel adapter — wraps JSON-RPC stdin/stdout transport for IDE communication.
/// Reads from the shared SemanticModel and formats responses via DualVoiceFormatter.
class LspChannel {
public:
    explicit LspChannel(SemanticModel& model,
                        IncrementalAnalyzer* analyzer = nullptr);
    LspChannel(SemanticModel& model,
               std::istream& in,
               std::ostream& out,
               IncrementalAnalyzer* analyzer = nullptr);
    ~LspChannel();

    /// Start processing LSP messages (blocking)
    void start();

    /// Run LSP over a raw file descriptor (Unix socket client)
    void run_on_fd(int fd);

    /// Stop the channel
    void stop();

    /// Send a notification (no response expected)
    void send_notification(const std::string& method, const nlohmann::json& params);

    /// Publish diagnostics for a file to the IDE
    void publish_diagnostics(const std::filesystem::path& file,
                             const std::vector<Diagnostic>& diags);

    /// Send workDoneProgress notification (Req 8.2)
    void report_progress(const std::string& token, int percentage, const std::string& message);

    /// Check if the channel received a shutdown request
    bool shutdown_requested() const { return shutdown_requested_; }

private:
    /// Read a single LSP message from stdin
    std::optional<JsonRpcMessage> read_message();

    /// Write a JSON-RPC response
    void send_response(const nlohmann::json& id, const nlohmann::json& result);

    /// Handle an incoming request/notification
    void handle_message(const JsonRpcMessage& msg);

    /// LSP method handlers
    nlohmann::json handle_initialize(const nlohmann::json& params);
    void handle_initialized();
    nlohmann::json handle_shutdown();
    void handle_text_document_did_open(const nlohmann::json& params);
    void handle_text_document_did_change(const nlohmann::json& params);
    void handle_text_document_did_close(const nlohmann::json& params);
    nlohmann::json handle_text_document_hover(const nlohmann::json& params);
    nlohmann::json handle_text_document_completion(const nlohmann::json& params);
    nlohmann::json handle_semantic_tokens_full(const nlohmann::json& params);
    nlohmann::json handle_structural_diff(const nlohmann::json& params);
    nlohmann::json handle_text_document_definition(const nlohmann::json& params);
    nlohmann::json handle_text_document_references(const nlohmann::json& params);
    nlohmann::json handle_text_document_document_symbol(const nlohmann::json& params);
    nlohmann::json handle_text_document_formatting(const nlohmann::json& params);
    nlohmann::json handle_text_document_rename(const nlohmann::json& params);
    nlohmann::json handle_prepare_rename(const nlohmann::json& params);
    nlohmann::json handle_workspace_symbol(const nlohmann::json& params);
    nlohmann::json handle_signature_help(const nlohmann::json& params);
    nlohmann::json handle_folding_range(const nlohmann::json& params);
    nlohmann::json handle_inlay_hint(const nlohmann::json& params);
    nlohmann::json handle_code_action(const nlohmann::json& params);
    nlohmann::json handle_prepare_call_hierarchy(const nlohmann::json& params);
    nlohmann::json handle_incoming_calls(const nlohmann::json& params);
    nlohmann::json handle_outgoing_calls(const nlohmann::json& params);
    nlohmann::json handle_selection_range(const nlohmann::json& params);
    nlohmann::json handle_linked_editing_range(const nlohmann::json& params);
    nlohmann::json handle_code_lens(const nlohmann::json& params);
    nlohmann::json handle_prepare_type_hierarchy(const nlohmann::json& params);
    nlohmann::json handle_type_supertypes(const nlohmann::json& params);
    nlohmann::json handle_type_subtypes(const nlohmann::json& params);

    SemanticModel& model_;
    NavigationProvider nav_provider_;
    CompletionProvider completion_provider_;
    SemanticTokenProvider token_provider_;
    IncrementalAnalyzer* analyzer_{nullptr};
    std::istream& in_;
    std::ostream& out_;
    bool running_{false};
    bool shutdown_requested_{false};

    // Debounce members for didChange analysis
    std::mutex debounce_mutex_;
    std::condition_variable debounce_cv_;
    std::atomic<bool> debounce_pending_{false};
    std::string debounce_path_;
    std::string debounce_content_;
    std::thread debounce_thread_;
    std::atomic<bool> debounce_stop_{false};
};

}  // namespace meld::daemon
