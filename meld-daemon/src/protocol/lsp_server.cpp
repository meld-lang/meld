#include "lsp_server.hpp"

namespace meld::lsp::protocol {

LspServer::LspServer() {
    register_handlers();
}

void LspServer::register_handlers() {
    router_.register_request("initialize", [this](const json& params) {
        return handle_initialize(params);
    });

    router_.register_request("shutdown", [this](const json& params) {
        return handle_shutdown(params);
    });

    router_.register_notification("initialized", [this](const json& params) {
        handle_initialized(params);
    });

    router_.register_notification("exit", [this](const json& params) {
        handle_exit(params);
    });
}

int LspServer::run(std::istream& input, std::ostream& output) {
    while (!exit_requested_) {
        auto raw = JsonRpc::read_message(input);
        if (!raw) {
            // EOF or read error
            break;
        }

        auto response = process_message(*raw);
        if (response) {
            JsonRpc::write_message(output, *response);
        }
    }

    // LSP spec: exit code 0 if shutdown was requested before exit, 1 otherwise
    return (state_ == ServerState::Stopped) ? 0 : 1;
}

std::optional<std::string> LspServer::process_message(const std::string& raw) {
    auto msg = JsonRpc::parse(raw);
    if (!msg) {
        // Parse error — only respond if we can extract an id
        return JsonRpc::make_error(0, -32700, "Parse error");
    }

    // Before initialization, only initialize and exit are allowed
    if (state_ == ServerState::Uninitialized) {
        if (msg->type == MessageType::Request && msg->method != "initialize") {
            return JsonRpc::make_error(msg->id.value_or(0), -32002,
                "Server not initialized");
        }
        if (msg->type == MessageType::Notification && msg->method != "exit") {
            return std::nullopt; // silently ignore
        }
    }

    // After shutdown, only exit is allowed
    if (state_ == ServerState::ShuttingDown) {
        if (msg->type == MessageType::Notification && msg->method == "exit") {
            handle_exit(msg->params);
            return std::nullopt;
        }
        if (msg->type == MessageType::Request) {
            return JsonRpc::make_error(msg->id.value_or(0), -32600,
                "Server is shutting down");
        }
        return std::nullopt;
    }

    return router_.dispatch(*msg);
}

json LspServer::server_capabilities() {
    return {
        {"textDocumentSync", {
            {"openClose", true},
            {"change", 2}  // Incremental
        }},
        {"completionProvider", {
            {"triggerCharacters", json::array({".", ":", "@"})},
            {"resolveProvider", true}
        }},
        {"hoverProvider", true},
        {"definitionProvider", true},
        {"referencesProvider", true},
        {"documentFormattingProvider", true},
        {"documentRangeFormattingProvider", true},
        {"renameProvider", {
            {"prepareProvider", true}
        }},
        {"semanticTokensProvider", {
            {"full", true},
            {"legend", {
                {"tokenTypes", json::array({
                    "keyword", "function", "variable", "type",
                    "parameter", "property", "string", "number",
                    "comment", "operator", "decorator", "macro",
                    "namespace", "event"
                })},
                {"tokenModifiers", json::array({
                    "declaration", "definition", "readonly",
                    "static", "deprecated", "async",
                    "modification", "documentation"
                })}
            }}
        }},
        {"diagnosticProvider", {
            {"interFileDependencies", true},
            {"workspaceDiagnostics", false}
        }}
    };
}

json LspServer::handle_initialize(const json& /*params*/) {
    if (state_ != ServerState::Uninitialized) {
        throw std::runtime_error("Server already initialized");
    }
    state_ = ServerState::Initializing;

    return {
        {"capabilities", server_capabilities()},
        {"serverInfo", {
            {"name", "meld-lsp-server"},
            {"version", "0.1.0"}
        }}
    };
}

json LspServer::handle_shutdown(const json& /*params*/) {
    state_ = ServerState::ShuttingDown;
    return nullptr; // LSP spec: shutdown returns null
}

void LspServer::handle_initialized(const json& /*params*/) {
    state_ = ServerState::Running;
}

void LspServer::handle_exit(const json& /*params*/) {
    state_ = ServerState::Stopped;
    exit_requested_ = true;
}

} // namespace meld::lsp::protocol
