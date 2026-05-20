#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace meld::lsp {

// ── Transport mode ──────────────────────────────────────────────────

enum class Transport {
    Stdio,  ///< JSON-RPC over stdin/stdout (default, per LSP spec)
    Tcp     ///< JSON-RPC over TCP socket
};

// ── Lightweight LSP protocol types ──────────────────────────────────
// These mirror the LSP specification types used internally by the
// server.  The meld::lsp::Position / Range from ownership_lsp.hpp are
// reused where possible; the structs below cover the remaining
// protocol surface.

struct LspPosition {
    size_t line = 0;
    size_t character = 0;
};

struct LspRange {
    LspPosition start;
    LspPosition end;
};

struct LspLocation {
    std::string uri;
    LspRange range;
};

enum class LspDiagnosticSeverity {
    Error = 1,
    Warning = 2,
    Information = 3,
    Hint = 4
};

struct LspDiagnostic {
    LspRange range;
    LspDiagnosticSeverity severity = LspDiagnosticSeverity::Error;
    std::string source = "meld";
    std::string message;
};

struct LspCompletionItem {
    std::string label;
    int kind = 1;  // LSP CompletionItemKind (1 = Text, 3 = Function, …)
    std::string detail;
    std::string documentation;
    std::string insert_text;
};

struct LspTextEdit {
    LspRange range;
    std::string new_text;
};

// ── Document store entry ────────────────────────────────────────────

struct TextDocument {
    std::string uri;
    std::string language_id;
    int version = 0;
    std::string content;
};

// ── LspServer ───────────────────────────────────────────────────────
//
// The library-layer LSP server.  Lives in meld-lang so it can be
// reused by the CLI module, tests, and any future embedding.
//
// Implements:
//   • JSON-RPC message framing (Content-Length header + JSON body)
//   • initialize / initialized / shutdown / exit lifecycle
//   • textDocument/didOpen, textDocument/didChange  (document sync)
//   • textDocument/publishDiagnostics               (via Parser)
//   • textDocument/completion                       (symbol table)
//   • textDocument/definition                       (AST id resolution)
//   • textDocument/references                       (AST symbol search)
//   • textDocument/formatting                       (CodeFormatter)
//
// Requirements: 14.1, 14.2, 14.3

class LspServer {
public:
    LspServer();
    ~LspServer();

    // Non-copyable, non-movable (owns threads / IO state).
    LspServer(const LspServer&) = delete;
    LspServer& operator=(const LspServer&) = delete;

    /// Start the server on the given transport.
    /// Blocks until the client sends `exit` or the server is stopped.
    /// Returns 0 on clean shutdown, non-zero on error.
    int start(Transport transport);

    /// Request a graceful shutdown from another thread.
    void request_shutdown();

    /// True while the main loop is running.
    bool is_running() const noexcept { return running_.load(); }

    // ── Handler signatures (virtual for testability) ────────────

    /// textDocument/diagnostics — parse the document and return errors.
    virtual std::vector<LspDiagnostic> handle_diagnostics(const std::string& uri);

    /// textDocument/completion — return completion items at a position.
    virtual std::vector<LspCompletionItem> handle_completion(
        const std::string& uri, const LspPosition& position);

    /// textDocument/definition — resolve identifier to definition location.
    virtual std::vector<LspLocation> handle_definition(
        const std::string& uri, const LspPosition& position);

    /// textDocument/references — find all references to the symbol.
    virtual std::vector<LspLocation> handle_references(
        const std::string& uri, const LspPosition& position);

    /// textDocument/formatting — format the entire document.
    virtual std::vector<LspTextEdit> handle_formatting(const std::string& uri);

private:
    // ── JSON-RPC framing ────────────────────────────────────────

    /// Read one JSON-RPC message from the input stream.
    /// Returns empty string on EOF / error.
    std::string read_message();

    /// Write a JSON-RPC message to the output stream.
    void write_message(const std::string& json);

    // ── Protocol dispatch ───────────────────────────────────────

    /// Route an incoming JSON-RPC message to the appropriate handler.
    /// Returns the JSON response string (empty for notifications).
    std::string dispatch(const std::string& json);

    /// Build the `initialize` response with server capabilities.
    std::string handle_initialize(const std::string& id, const std::string& params_json);

    /// Handle textDocument/didOpen notification.
    void handle_did_open(const std::string& params_json);

    /// Handle textDocument/didChange notification.
    void handle_did_change(const std::string& params_json);

    /// Publish diagnostics for a document (server → client notification).
    void publish_diagnostics(const std::string& uri);

    // ── Minimal JSON helpers ────────────────────────────────────

    static std::string json_string(const std::string& key, const std::string& value);
    static std::string json_int(const std::string& key, int value);
    static std::string json_bool(const std::string& key, bool value);

    // ── State ───────────────────────────────────────────────────

    std::atomic<bool> running_{false};
    std::atomic<bool> shutdown_requested_{false};

    /// Open documents keyed by URI.
    std::map<std::string, TextDocument> documents_;
    mutable std::mutex documents_mutex_;

    /// I/O streams (set by start() based on transport).
    std::istream* input_ = nullptr;
    std::ostream* output_ = nullptr;
    mutable std::mutex output_mutex_;

    /// Next JSON-RPC request id for server-initiated requests.
    int next_request_id_ = 1;
};

} // namespace meld::lsp
