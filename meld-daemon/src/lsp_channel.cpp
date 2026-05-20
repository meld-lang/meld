#include "meld/daemon/lsp_channel.hpp"
#include "meld/daemon/navigation_provider.hpp"

#include <csignal>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <thread>
#include <unistd.h>

namespace meld::daemon {

LspChannel::LspChannel(SemanticModel& model, IncrementalAnalyzer* analyzer)
    : model_(model), nav_provider_(model), completion_provider_(model),
      analyzer_(analyzer), in_(std::cin), out_(std::cout) {
    debounce_thread_ = std::thread([this]() {
        while (!debounce_stop_.load()) {
            std::unique_lock<std::mutex> lock(debounce_mutex_);
            debounce_cv_.wait(lock, [this] { return debounce_pending_.load() || debounce_stop_.load(); });
            if (debounce_stop_.load()) break;

            // Wait 500ms for more changes
            debounce_cv_.wait_for(lock, std::chrono::milliseconds(500));
            if (debounce_stop_.load()) break;

            if (debounce_pending_.load()) {
                debounce_pending_.store(false);
                auto path = debounce_path_;
                auto content = debounce_content_;
                lock.unlock();

                if (analyzer_) {
                    try {
                        if (!content.empty()) {
                            analyzer_->analyze_change(std::filesystem::path(path), content);
                        } else {
                            analyzer_->analyze_change(path);
                        }
                    } catch (...) {}
                }
            }
        }
    });
}

LspChannel::LspChannel(SemanticModel& model, std::istream& in, std::ostream& out,
                       IncrementalAnalyzer* analyzer)
    : model_(model), nav_provider_(model), completion_provider_(model),
      analyzer_(analyzer), in_(in), out_(out) {
    debounce_thread_ = std::thread([this]() {
        while (!debounce_stop_.load()) {
            std::unique_lock<std::mutex> lock(debounce_mutex_);
            debounce_cv_.wait(lock, [this] { return debounce_pending_.load() || debounce_stop_.load(); });
            if (debounce_stop_.load()) break;

            // Wait 500ms for more changes
            debounce_cv_.wait_for(lock, std::chrono::milliseconds(500));
            if (debounce_stop_.load()) break;

            if (debounce_pending_.load()) {
                debounce_pending_.store(false);
                auto path = debounce_path_;
                auto content = debounce_content_;
                lock.unlock();

                if (analyzer_) {
                    try {
                        if (!content.empty()) {
                            analyzer_->analyze_change(std::filesystem::path(path), content);
                        } else {
                            analyzer_->analyze_change(path);
                        }
                    } catch (...) {}
                }
            }
        }
    });
}

LspChannel::~LspChannel() {
    debounce_stop_.store(true);
    debounce_cv_.notify_all();
    if (debounce_thread_.joinable()) debounce_thread_.join();
    stop();
}

void LspChannel::start() {
    running_ = true;
    while (running_) {
        auto msg = read_message();
        if (!msg) break;
        handle_message(*msg);
        if (shutdown_requested_) break;
    }
}

void LspChannel::run_on_fd(int fd) {
    int fd_dup = dup(fd);
    if (fd_dup < 0) { close(fd); return; }

    FILE* f_read = fdopen(fd, "r");
    FILE* f_write = fdopen(fd_dup, "w");
    if (!f_read || !f_write) {
        if (f_read) fclose(f_read); else close(fd);
        if (f_write) fclose(f_write); else close(fd_dup);
        return;
    }
    setvbuf(f_write, nullptr, _IONBF, 0);

    auto fd_write_json = [&](const nlohmann::json& j) {
        std::string body = j.dump();
        fprintf(f_write, "Content-Length: %zu\r\n\r\n%s",
                body.size(), body.c_str());
        fflush(f_write);
    };

    auto fd_send_response = [&](const nlohmann::json& id, const nlohmann::json& result) {
        nlohmann::json msg;
        msg["jsonrpc"] = "2.0";
        msg["id"] = id;
        msg["result"] = result;
        fd_write_json(msg);
    };

    running_ = true;
    try {
    while (running_) {
        char line_buf[256];
        int content_length = -1;
        while (fgets(line_buf, sizeof(line_buf), f_read)) {
            std::string line(line_buf);
            if (line == "\r\n" || line == "\n") break;
            if (line.find("Content-Length:") == 0)
                content_length = std::stoi(line.substr(15));
        }
        if (content_length < 0) break;

        std::string body(content_length, '\0');
        if (fread(body.data(), 1, content_length, f_read)
                != static_cast<size_t>(content_length))
            break;

        auto j = nlohmann::json::parse(body, nullptr, false);
        if (j.is_discarded()) continue;

        JsonRpcMessage msg;
        msg.method = j.value("method", "");
        msg.params = j.value("params", nlohmann::json::object());
        msg.id = j.value("id", nlohmann::json{});

        try {

        if (msg.method == "initialize") {
            fd_send_response(msg.id, handle_initialize(msg.params));
        } else if (msg.method == "initialized") {
            handle_initialized();
        } else if (msg.method == "$/cancelRequest") {
            // Acknowledged but not actionable — requests run synchronously
        } else if (msg.method == "$/setTrace") {
            // Acknowledged — trace level changes not implemented
        } else if (msg.method == "shutdown") {
            fd_send_response(msg.id, handle_shutdown());
        } else if (msg.method == "exit") {
            break;  // Clean exit after shutdown
        } else if (msg.method == "textDocument/didOpen") {
            handle_text_document_did_open(msg.params);
            // Publish diagnostics via fd (publish_diagnostics writes to out_, not fd)
            try {
                auto uri = msg.params["textDocument"]["uri"].get<std::string>();
                std::string p = uri.substr(uri.find("file://") == 0 ? 7 : 0);
                auto diags = model_.get_diagnostics(p);
                nlohmann::json diag_params;
                diag_params["uri"] = uri;
                nlohmann::json diag_array = nlohmann::json::array();
                for (const auto& d : diags) {
                    auto dv = DualVoiceFormatter::format_diagnostic(d);
                    diag_array.push_back(DualVoiceFormatter::to_lsp_json(dv));
                }
                diag_params["diagnostics"] = diag_array;
                nlohmann::json notif;
                notif["jsonrpc"] = "2.0";
                notif["method"] = "textDocument/publishDiagnostics";
                notif["params"] = diag_params;
                fd_write_json(notif);
            } catch (...) {}
        } else if (msg.method == "textDocument/didChange") {
            handle_text_document_did_change(msg.params);
            // Publish diagnostics via fd
            try {
                auto uri = msg.params["textDocument"]["uri"].get<std::string>();
                std::string p = uri.substr(uri.find("file://") == 0 ? 7 : 0);
                auto diags = model_.get_diagnostics(p);
                nlohmann::json diag_params;
                diag_params["uri"] = uri;
                nlohmann::json diag_array = nlohmann::json::array();
                for (const auto& d : diags) {
                    auto dv = DualVoiceFormatter::format_diagnostic(d);
                    diag_array.push_back(DualVoiceFormatter::to_lsp_json(dv));
                }
                diag_params["diagnostics"] = diag_array;
                nlohmann::json notif;
                notif["jsonrpc"] = "2.0";
                notif["method"] = "textDocument/publishDiagnostics";
                notif["params"] = diag_params;
                fd_write_json(notif);
            } catch (...) {}
        } else if (msg.method == "textDocument/didClose") {
            handle_text_document_did_close(msg.params);
        } else if (msg.method == "textDocument/hover") {
            fd_send_response(msg.id, handle_text_document_hover(msg.params));
        } else if (msg.method == "textDocument/completion") {
            fd_send_response(msg.id, handle_text_document_completion(msg.params));
        } else if (msg.method == "textDocument/signatureHelp") {
            fd_send_response(msg.id, handle_signature_help(msg.params));
        } else if (msg.method == "textDocument/semanticTokens/full") {
            fd_send_response(msg.id, handle_semantic_tokens_full(msg.params));
        } else if (msg.method == "textDocument/definition") {
            fd_send_response(msg.id, handle_text_document_definition(msg.params));
        } else if (msg.method == "textDocument/references") {
            fd_send_response(msg.id, handle_text_document_references(msg.params));
        } else if (msg.method == "textDocument/documentSymbol") {
            fd_send_response(msg.id, handle_text_document_document_symbol(msg.params));
        } else if (msg.method == "textDocument/formatting") {
            fd_send_response(msg.id, handle_text_document_formatting(msg.params));
        } else if (msg.method == "textDocument/prepareRename") {
            fd_send_response(msg.id, handle_prepare_rename(msg.params));
        } else if (msg.method == "textDocument/rename") {
            fd_send_response(msg.id, handle_text_document_rename(msg.params));
        } else if (msg.method == "workspace/symbol") {
            fd_send_response(msg.id, handle_workspace_symbol(msg.params));
        } else if (msg.method == "meld/structuralDiff") {
            fd_send_response(msg.id, handle_structural_diff(msg.params));
        } else if (msg.method == "textDocument/foldingRange") {
            fd_send_response(msg.id, handle_folding_range(msg.params));
        } else if (msg.method == "textDocument/inlayHint") {
            fd_send_response(msg.id, handle_inlay_hint(msg.params));
        } else if (msg.method == "textDocument/codeAction") {
            fd_send_response(msg.id, handle_code_action(msg.params));
        } else if (msg.method == "textDocument/prepareCallHierarchy") {
            fd_send_response(msg.id, handle_prepare_call_hierarchy(msg.params));
        } else if (msg.method == "callHierarchy/incomingCalls") {
            fd_send_response(msg.id, handle_incoming_calls(msg.params));
        } else if (msg.method == "callHierarchy/outgoingCalls") {
            fd_send_response(msg.id, handle_outgoing_calls(msg.params));
        } else if (msg.method == "textDocument/selectionRange") {
            fd_send_response(msg.id, handle_selection_range(msg.params));
        } else if (msg.method == "textDocument/linkedEditingRange") {
            fd_send_response(msg.id, handle_linked_editing_range(msg.params));
        } else if (msg.method == "textDocument/codeLens") {
            fd_send_response(msg.id, handle_code_lens(msg.params));
        } else if (msg.method == "textDocument/prepareTypeHierarchy") {
            fd_send_response(msg.id, handle_prepare_type_hierarchy(msg.params));
        } else if (msg.method == "typeHierarchy/supertypes") {
            fd_send_response(msg.id, handle_type_supertypes(msg.params));
        } else if (msg.method == "typeHierarchy/subtypes") {
            fd_send_response(msg.id, handle_type_subtypes(msg.params));
        } // end dispatch

        } catch (const std::exception& e) {
            // Send error response if the request had an id
            if (!msg.id.is_null()) {
                nlohmann::json err_resp;
                err_resp["jsonrpc"] = "2.0";
                err_resp["id"] = msg.id;
                err_resp["error"] = {{"code", -32603}, {"message", std::string("Internal error: ") + e.what()}};
                fd_write_json(err_resp);
            }
        } catch (...) {
            if (!msg.id.is_null()) {
                nlohmann::json err_resp;
                err_resp["jsonrpc"] = "2.0";
                err_resp["id"] = msg.id;
                err_resp["error"] = {{"code", -32603}, {"message", "Internal error"}};
                fd_write_json(err_resp);
            }
        }

        if (shutdown_requested_) break;
    }
    } catch (...) {
        // Handler thread caught an unhandled exception — clean up gracefully
    }

    fclose(f_read);
    fclose(f_write);
}

void LspChannel::stop() {
    running_ = false;
}

void LspChannel::send_notification(const std::string& method, const nlohmann::json& params) {
    nlohmann::json msg;
    msg["jsonrpc"] = "2.0";
    msg["method"] = method;
    msg["params"] = params;

    std::string body = msg.dump();
    out_ << "Content-Length: " << body.size() << "\r\n\r\n" << body;
    out_.flush();
}

void LspChannel::publish_diagnostics(const std::filesystem::path& file,
                                     const std::vector<Diagnostic>& diags) {
    nlohmann::json params;
    params["uri"] = "file://" + file.string();
    nlohmann::json diag_array = nlohmann::json::array();
    for (const auto& d : diags) {
        auto dv = DualVoiceFormatter::format_diagnostic(d);
        diag_array.push_back(DualVoiceFormatter::to_lsp_json(dv));
    }
    params["diagnostics"] = diag_array;
    send_notification("textDocument/publishDiagnostics", params);
}

void LspChannel::report_progress(const std::string& token, int percentage,
                                 const std::string& message) {
    nlohmann::json params;
    params["token"] = token;
    nlohmann::json value;
    value["kind"] = percentage < 100 ? "report" : "end";
    value["percentage"] = percentage;
    value["message"] = message;
    params["value"] = value;
    send_notification("$/progress", params);
}

std::optional<JsonRpcMessage> LspChannel::read_message() {
    // Read Content-Length header
    std::string line;
    int content_length = -1;
    while (std::getline(in_, line)) {
        if (line.empty() || line == "\r") break;
        if (line.find("Content-Length:") == 0) {
            content_length = std::stoi(line.substr(15));
        }
    }
    if (content_length < 0) return std::nullopt;

    // Read body
    std::string body(content_length, '\0');
    in_.read(body.data(), content_length);
    if (!in_.good()) return std::nullopt;

    auto j = nlohmann::json::parse(body, nullptr, false);
    if (j.is_discarded()) return std::nullopt;

    JsonRpcMessage msg;
    msg.method = j.value("method", "");
    msg.params = j.value("params", nlohmann::json::object());
    msg.id = j.value("id", nlohmann::json{});
    return msg;
}

void LspChannel::send_response(const nlohmann::json& id, const nlohmann::json& result) {
    nlohmann::json msg;
    msg["jsonrpc"] = "2.0";
    msg["id"] = id;
    msg["result"] = result;

    std::string body = msg.dump();
    out_ << "Content-Length: " << body.size() << "\r\n\r\n" << body;
    out_.flush();
}

void LspChannel::handle_message(const JsonRpcMessage& msg) {
    if (msg.method == "initialize") {
        send_response(msg.id, handle_initialize(msg.params));
    } else if (msg.method == "initialized") {
        handle_initialized();
    } else if (msg.method == "$/cancelRequest") {
        // Acknowledged but not actionable — requests run synchronously
    } else if (msg.method == "$/setTrace") {
        // Acknowledged — trace level changes not implemented
    } else if (msg.method == "shutdown") {
        send_response(msg.id, handle_shutdown());
    } else if (msg.method == "textDocument/didOpen") {
        handle_text_document_did_open(msg.params);
        // Publish diagnostics
        auto uri = msg.params["textDocument"]["uri"].get<std::string>();
        std::string p = uri.substr(uri.find("file://") == 0 ? 7 : 0);
        auto diags = model_.get_diagnostics(p);
        publish_diagnostics(p, diags);
    } else if (msg.method == "textDocument/didChange") {
        handle_text_document_did_change(msg.params);
        // Publish diagnostics
        auto uri = msg.params["textDocument"]["uri"].get<std::string>();
        std::string p = uri.substr(uri.find("file://") == 0 ? 7 : 0);
        auto diags = model_.get_diagnostics(p);
        publish_diagnostics(p, diags);
    } else if (msg.method == "textDocument/didClose") {
        handle_text_document_did_close(msg.params);
    } else if (msg.method == "textDocument/hover") {
        send_response(msg.id, handle_text_document_hover(msg.params));
    } else if (msg.method == "textDocument/completion") {
        send_response(msg.id, handle_text_document_completion(msg.params));
    } else if (msg.method == "textDocument/signatureHelp") {
        send_response(msg.id, handle_signature_help(msg.params));
    } else if (msg.method == "textDocument/semanticTokens/full") {
        send_response(msg.id, handle_semantic_tokens_full(msg.params));
    } else if (msg.method == "textDocument/definition") {
        send_response(msg.id, handle_text_document_definition(msg.params));
    } else if (msg.method == "textDocument/references") {
        send_response(msg.id, handle_text_document_references(msg.params));
    } else if (msg.method == "textDocument/documentSymbol") {
        send_response(msg.id, handle_text_document_document_symbol(msg.params));
    } else if (msg.method == "textDocument/formatting") {
        send_response(msg.id, handle_text_document_formatting(msg.params));
    } else if (msg.method == "textDocument/prepareRename") {
        send_response(msg.id, handle_prepare_rename(msg.params));
    } else if (msg.method == "textDocument/rename") {
        send_response(msg.id, handle_text_document_rename(msg.params));
    } else if (msg.method == "workspace/symbol") {
        send_response(msg.id, handle_workspace_symbol(msg.params));
    } else if (msg.method == "meld/structuralDiff") {
        send_response(msg.id, handle_structural_diff(msg.params));
    } else if (msg.method == "textDocument/foldingRange") {
        send_response(msg.id, handle_folding_range(msg.params));
    } else if (msg.method == "textDocument/inlayHint") {
        send_response(msg.id, handle_inlay_hint(msg.params));
    } else if (msg.method == "textDocument/codeAction") {
        send_response(msg.id, handle_code_action(msg.params));
    } else if (msg.method == "textDocument/prepareCallHierarchy") {
        send_response(msg.id, handle_prepare_call_hierarchy(msg.params));
    } else if (msg.method == "callHierarchy/incomingCalls") {
        send_response(msg.id, handle_incoming_calls(msg.params));
    } else if (msg.method == "callHierarchy/outgoingCalls") {
        send_response(msg.id, handle_outgoing_calls(msg.params));
    } else if (msg.method == "textDocument/selectionRange") {
        send_response(msg.id, handle_selection_range(msg.params));
    } else if (msg.method == "textDocument/linkedEditingRange") {
        send_response(msg.id, handle_linked_editing_range(msg.params));
    } else if (msg.method == "textDocument/codeLens") {
        send_response(msg.id, handle_code_lens(msg.params));
    } else if (msg.method == "textDocument/prepareTypeHierarchy") {
        send_response(msg.id, handle_prepare_type_hierarchy(msg.params));
    } else if (msg.method == "typeHierarchy/supertypes") {
        send_response(msg.id, handle_type_supertypes(msg.params));
    } else if (msg.method == "typeHierarchy/subtypes") {
        send_response(msg.id, handle_type_subtypes(msg.params));
    }
}

nlohmann::json LspChannel::handle_initialize(const nlohmann::json& /*params*/) {
    nlohmann::json caps;
    caps["textDocumentSync"] = 1;  // Full sync
    caps["completionProvider"] = {{"triggerCharacters", {".", ":"}}};
    caps["signatureHelpProvider"] = {{"triggerCharacters", {"(", ","}}};
    caps["hoverProvider"] = true;
    caps["definitionProvider"] = true;
    caps["referencesProvider"] = true;
    caps["documentSymbolProvider"] = true;
    caps["workspaceSymbolProvider"] = true;
    caps["documentFormattingProvider"] = true;
    caps["renameProvider"] = {{"prepareProvider", true}};
    caps["foldingRangeProvider"] = true;
    caps["inlayHintProvider"] = true;
    caps["codeActionProvider"] = true;
    caps["callHierarchyProvider"] = true;
    caps["selectionRangeProvider"] = true;
    caps["linkedEditingRangeProvider"] = true;
    // Code lens re-enabled — symbol index makes reference counting O(1)
    caps["codeLensProvider"] = {{"resolveProvider", false}};
    caps["typeHierarchyProvider"] = true;

    // Semantic tokens capability
    nlohmann::json sem_legend;
    sem_legend["tokenTypes"] = SemanticTokenProvider::token_type_names();
    sem_legend["tokenModifiers"] = nlohmann::json::array();
    nlohmann::json sem_full;
    sem_full["legend"] = sem_legend;
    sem_full["full"] = true;
    caps["semanticTokensProvider"] = sem_full;

    nlohmann::json result;
    result["capabilities"] = caps;
    result["serverInfo"] = {{"name", "meldd"}, {"version", "0.1.0"}};
    return result;
}

void LspChannel::handle_initialized() {
    // Server is ready — could trigger initial workspace indexing notification
}

nlohmann::json LspChannel::handle_shutdown() {
    shutdown_requested_ = true;
    return nullptr;
}

void LspChannel::handle_text_document_did_open(const nlohmann::json& params) {
    auto uri = params["textDocument"]["uri"].get<std::string>();
    auto text = params["textDocument"].value("text", "");
    // Strip file:// prefix
    std::string path_str = uri.substr(uri.find("file://") == 0 ? 7 : 0);

    // Parse and cache semantic tokens (fast — needed for completions and semantic tokens)
    if (!text.empty()) {
        token_provider_.parse_document(uri, text);
    }

    // Trigger incremental analysis asynchronously to avoid blocking the message loop.
    // Only analyze if the file isn't already indexed.
    if (analyzer_ && !model_.has_file(path_str)) {
        auto* a = analyzer_;
        std::thread([a, path_str]() {
            try { a->analyze_change(path_str); } catch (...) {}
        }).detach();
    }
}

void LspChannel::handle_text_document_did_change(const nlohmann::json& params) {
    auto uri = params["textDocument"]["uri"].get<std::string>();
    std::string path_str = uri.substr(uri.find("file://") == 0 ? 7 : 0);

    // Extract new text from content changes (full sync mode)
    auto changes = params.value("contentChanges", nlohmann::json::array());
    if (!changes.empty()) {
        auto text = changes[0].value("text", std::string{});
        if (!text.empty()) {
            // Re-parse semantic tokens (fast, synchronous)
            token_provider_.parse_document(uri, text);

            // Debounce re-analysis: store latest change and notify the debounce thread
            if (analyzer_) {
                {
                    std::lock_guard<std::mutex> lock(debounce_mutex_);
                    debounce_path_ = path_str;
                    debounce_content_ = text;
                    debounce_pending_.store(true);
                }
                debounce_cv_.notify_one();
            }
        }
    }
}

void LspChannel::handle_text_document_did_close(const nlohmann::json& /*params*/) {
    // No action needed — the SemanticModel retains file data for workspace-wide analysis
}

nlohmann::json LspChannel::handle_text_document_hover(const nlohmann::json& params) {
    auto uri = params["textDocument"]["uri"].get<std::string>();
    std::string path_str = uri.substr(uri.find("file://") == 0 ? 7 : 0);
    auto line = params["position"]["line"].get<uint32_t>();
    auto character = params["position"]["character"].get<uint32_t>();

    // Check effects first (existing behavior)
    auto effects = model_.query_effects(path_str, line);
    if (effects) {
        std::string hover_text = "Effects: ";
        for (size_t i = 0; i < effects->required_effects.size(); ++i) {
            if (i > 0) hover_text += ", ";
            hover_text += effects->required_effects[i];
        }
        return {{"contents", {{"kind", "markdown"}, {"value", hover_text}}}};
    }

    // Try to get symbol info from NavigationProvider
    auto symbol = model_.get_symbol_at(path_str, line, character);
    if (!symbol.empty()) {
        auto hover_info = nav_provider_.get_hover(path_str, symbol);
        if (hover_info) {
            // Format as markdown
            std::string md = "**" + symbol + "**";
            if (!hover_info->type_signature.empty())
                md += "\n\nType: `" + hover_info->type_signature + "`";
            if (!hover_info->kind.empty())
                md += "\n\nKind: " + hover_info->kind;
            if (!hover_info->documentation.empty())
                md += "\n\n" + hover_info->documentation;
            if (!hover_info->effects.empty()) {
                md += "\n\nEffects: ";
                for (size_t i = 0; i < hover_info->effects.size(); ++i) {
                    if (i > 0) md += ", ";
                    md += hover_info->effects[i];
                }
            }
            return {{"contents", {{"kind", "markdown"}, {"value", md}}}};
        }
    }

    return nullptr;
}

nlohmann::json LspChannel::handle_text_document_completion(const nlohmann::json& params) {
    auto uri = params["textDocument"]["uri"].get<std::string>();
    std::string path_str = uri.substr(uri.find("file://") == 0 ? 7 : 0);
    auto line = params["position"]["line"].get<uint32_t>();
    auto character = params["position"]["character"].get<uint32_t>();

    // Get the line text from the cached document content
    std::string line_text;
    auto content = token_provider_.get_content(uri);
    if (content) {
        std::istringstream lines(*content);
        std::string l;
        uint32_t ln = 0;
        while (std::getline(lines, l)) {
            if (ln == line) {
                line_text = l;
                break;
            }
            ++ln;
        }
    }

    // Get completions from the provider
    auto result = completion_provider_.get_completions(
        std::filesystem::path(path_str), line, character, line_text);

    // Convert to LSP CompletionItem JSON format
    nlohmann::json items = nlohmann::json::array();
    for (const auto& item : result.items) {
        nlohmann::json lsp_item;
        lsp_item["label"] = item.label;

        // Map kind strings to LSP CompletionItemKind numbers
        int kind = 1;  // Text (default)
        if (item.kind == "function") kind = 3;
        else if (item.kind == "variable") kind = 6;
        else if (item.kind == "keyword") kind = 14;
        else if (item.kind == "type") kind = 22;
        else if (item.kind == "import") kind = 9;
        else if (item.kind == "snippet") kind = 15;
        lsp_item["kind"] = kind;

        if (!item.detail.empty()) lsp_item["detail"] = item.detail;
        if (!item.documentation.empty()) lsp_item["documentation"] = item.documentation;
        if (!item.insert_text.empty()) lsp_item["insertText"] = item.insert_text;

        items.push_back(std::move(lsp_item));
    }

    // Cap at 100 items for editor performance
    if (items.size() > 100) {
        items.erase(items.begin() + 100, items.end());
    }

    return {{"isIncomplete", items.size() >= 100}, {"items", items}};
}

nlohmann::json LspChannel::handle_semantic_tokens_full(const nlohmann::json& params) {
    auto uri = params["textDocument"]["uri"].get<std::string>();
    std::string path_str = uri.substr(uri.find("file://") == 0 ? 7 : 0);

    // Try to get content from the SemanticModel's AST, or use cached parse
    auto cached = token_provider_.get_cached(uri);
    if (cached) {
        auto encoded = SemanticTokenProvider::encode_semantic_tokens(cached->tokens);
        return {{"data", encoded}};
    }

    // No cached result — return empty
    return {{"data", nlohmann::json::array()}};
}

nlohmann::json LspChannel::handle_structural_diff(const nlohmann::json& params) {
    // Custom LSP extension: meld/structuralDiff
    // Accepts: textDocument.uri, optional vfs_session_id
    // Returns: structural diff as AST_Transform JSON array
    auto uri = params.value("uri", "");
    auto session_id = params.value("vfs_session_id", "");

    std::string path_str = uri.substr(uri.find("file://") == 0 ? 7 : 0);

    nlohmann::json result;
    result["uri"] = uri;

    auto current_ast = model_.get_ast(path_str);
    if (!current_ast) {
        result["changes"] = nlohmann::json::array();
        result["error"] = "File not indexed: " + path_str;
        return result;
    }

    // When a VFS session is provided, compare VFS overlay against on-disk.
    // Full VFS integration will be wired when the VFS subsystem is complete.
    if (session_id.empty()) {
        result["changes"] = nlohmann::json::array();
        result["summary"] = "No VFS session — on-disk state is current";
    } else {
        result["changes"] = nlohmann::json::array();
        result["summary"] = "VFS session " + session_id + " — diff pending VFS integration";
        result["vfs_session_id"] = session_id;
    }

    return result;
}

// ── Navigation handlers ─────────────────────────────────────────────

static std::string strip_file_uri(const std::string& uri) {
    return uri.substr(uri.find("file://") == 0 ? 7 : 0);
}

/// Map a kind string to an LSP SymbolKind number.
static int lsp_symbol_kind(const std::string& kind) {
    if (kind == "function") return 12;
    if (kind == "struct")   return 23;
    if (kind == "class")    return 5;
    if (kind == "enum")     return 10;
    if (kind == "trait")    return 11;  // Interface
    if (kind == "variable") return 13;
    if (kind == "type")     return 26;  // TypeParameter
    return 1;  // File (fallback)
}

nlohmann::json LspChannel::handle_text_document_definition(const nlohmann::json& params) {
    auto uri = params["textDocument"]["uri"].get<std::string>();
    auto line = params["position"]["line"].get<uint32_t>();
    auto character = params["position"]["character"].get<uint32_t>();
    std::string path_str = strip_file_uri(uri);

    // Try to get the symbol name from the model's AST at this position
    auto symbol = model_.get_symbol_at(path_str, line, character);
    if (symbol.empty()) {
        return nullptr;  // No symbol at position
    }

    // Search all indexed files for the definition (handles cross-file go-to-def
    // from call sites where the symbol is a reference, not a declaration).
    auto loc = nav_provider_.go_to_definition(path_str, symbol);
    if (!loc) {
        // Try with canonical path in case the URI path doesn't match indexed paths
        std::error_code ec;
        auto canonical = std::filesystem::canonical(path_str, ec);
        if (!ec && canonical.string() != path_str) {
            loc = nav_provider_.go_to_definition(canonical, symbol);
        }
    }
    // If still not found, try resolving through the file's imports
    if (!loc && analyzer_) {
        auto ast = model_.get_ast(path_str);
        if (ast) {
            for (const auto& child : ast->children) {
                if (child && child->kind == "import") {
                    auto resolved = analyzer_->resolve_import(child->name);
                    if (resolved) {
                        loc = nav_provider_.find_definition_in_file(*resolved, symbol);
                        if (loc) break;
                    }
                }
            }
        }
    }
    if (!loc) {
        return nullptr;  // Definition not found
    }

    nlohmann::json location;
    location["uri"] = "file://" + loc->file.string();
    location["range"] = {
        {"start", {{"line", loc->line}, {"character", loc->column}}},
        {"end",   {{"line", loc->line}, {"character", loc->column + loc->name.size()}}}
    };
    return nlohmann::json::array({location});
}

nlohmann::json LspChannel::handle_text_document_references(const nlohmann::json& params) {
    auto uri = params["textDocument"]["uri"].get<std::string>();
    auto line = params["position"]["line"].get<uint32_t>();
    auto character = params["position"]["character"].get<uint32_t>();
    std::string path_str = strip_file_uri(uri);

    auto symbol = model_.get_symbol_at(path_str, line, character);
    if (symbol.empty()) {
        return nlohmann::json::array();
    }

    try {
        auto refs = nav_provider_.find_references(symbol);
        nlohmann::json result = nlohmann::json::array();
        for (const auto& ref : refs) {
            result.push_back({
                {"uri", "file://" + ref.file.string()},
                {"range", {
                    {"start", {{"line", ref.line}, {"character", ref.column}}},
                    {"end",   {{"line", ref.line}, {"character", ref.column + ref.name.size()}}}
                }}
            });
        }
        return result;
    } catch (const std::exception& e) {
        // Log error but don't crash the handler thread
        return nlohmann::json::array();
    } catch (...) {
        return nlohmann::json::array();
    }
}

nlohmann::json LspChannel::handle_text_document_document_symbol(const nlohmann::json& params) {
    auto uri = params["textDocument"]["uri"].get<std::string>();
    std::string path_str = strip_file_uri(uri);

    // Directly walk the AST from the model to collect top-level declarations.
    // This bypasses NavigationProvider's build_document_symbols which may miss
    // some node kinds (e.g., "function_definition", "fnc") due to filtering.
    auto ast = model_.get_ast(path_str);
    if (!ast) {
        // Try canonical path
        std::error_code ec;
        auto canonical = std::filesystem::canonical(path_str, ec);
        if (!ec) ast = model_.get_ast(canonical);
    }
    if (!ast) return nlohmann::json::array();

    // Determine the children to iterate: if root is a module, use its children
    const auto& nodes = (ast->kind == "module" && !ast->children.empty())
                            ? ast->children
                            : std::vector<std::shared_ptr<ASTNode>>{ast};

    nlohmann::json result = nlohmann::json::array();
    for (const auto& child : nodes) {
        if (!child || child->name.empty()) continue;
        if (child->kind == "reference") continue;
        nlohmann::json j;
        j["name"] = child->name;
        j["detail"] = child->type_info;
        j["kind"] = lsp_symbol_kind(child->kind);
        j["range"] = {
            {"start", {{"line", child->location.line}, {"character", child->location.column}}},
            {"end", {{"line", child->location.line}, {"character", child->location.column + static_cast<uint32_t>(child->name.size())}}}
        };
        j["selectionRange"] = j["range"];
        result.push_back(j);
    }
    return result;
}

nlohmann::json LspChannel::handle_text_document_formatting(const nlohmann::json& params) {
    auto uri = params["textDocument"]["uri"].get<std::string>();
    std::string path_str = strip_file_uri(uri);

    // Get content from cache or disk
    std::string content;
    auto cached = token_provider_.get_content(uri);
    if (cached) {
        content = *cached;
    } else {
        std::ifstream ifs(path_str);
        if (!ifs.is_open()) return nlohmann::json::array();
        std::ostringstream oss;
        oss << ifs.rdbuf();
        content = oss.str();
    }

    if (content.empty()) return nlohmann::json::array();

    // Apply formatting: trim trailing whitespace, collapse multiple blank lines,
    // ensure final newline
    std::istringstream lines_stream(content);
    std::string line;
    std::vector<std::string> formatted_lines;
    int consecutive_blanks = 0;

    while (std::getline(lines_stream, line)) {
        // Remove trailing whitespace and \r
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t' || line.back() == '\r'))
            line.pop_back();

        if (line.empty()) {
            consecutive_blanks++;
            if (consecutive_blanks <= 1) {
                formatted_lines.push_back("");
            }
        } else {
            consecutive_blanks = 0;
            formatted_lines.push_back(line);
        }
    }

    // Remove trailing blank lines
    while (!formatted_lines.empty() && formatted_lines.back().empty())
        formatted_lines.pop_back();

    // Build result with newline after each line
    std::string result_text;
    for (size_t i = 0; i < formatted_lines.size(); ++i) {
        result_text += formatted_lines[i];
        result_text += "\n";
    }

    // If nothing changed, return empty edits
    if (result_text == content) return nlohmann::json::array();

    // Count lines in original for the end range
    uint32_t line_count = 0;
    for (char c : content) if (c == '\n') ++line_count;

    // Return whole-document replacement edit
    nlohmann::json edit;
    edit["range"] = {
        {"start", {{"line", 0}, {"character", 0}}},
        {"end", {{"line", line_count + 1}, {"character", 0}}}
    };
    edit["newText"] = result_text;
    return nlohmann::json::array({edit});
}

nlohmann::json LspChannel::handle_text_document_rename(const nlohmann::json& params) {
    auto uri = params["textDocument"]["uri"].get<std::string>();
    auto line = params["position"]["line"].get<uint32_t>();
    auto character = params["position"]["character"].get<uint32_t>();
    auto new_name = params["newName"].get<std::string>();
    std::string path_str = strip_file_uri(uri);

    auto symbol = model_.get_symbol_at(path_str, line, character);
    if (symbol.empty()) return nullptr;

    try {
        // Find all references across workspace (limited to prevent timeout)
        auto refs = nav_provider_.find_references(symbol);
        if (refs.empty()) return nullptr;

        // Cap at 500 edits to prevent massive renames from hanging
        if (refs.size() > 500) {
            refs.resize(500);
        }

        // Group edits by file URI
        nlohmann::json changes = nlohmann::json::object();
        for (const auto& ref : refs) {
            std::string ref_uri = "file://" + ref.file.string();
            if (!changes.contains(ref_uri)) {
                changes[ref_uri] = nlohmann::json::array();
            }
            nlohmann::json edit;
            edit["range"] = {
                {"start", {{"line", ref.line}, {"character", ref.column}}},
                {"end",   {{"line", ref.line}, {"character", ref.column + ref.name.size()}}}
            };
            edit["newText"] = new_name;
            changes[ref_uri].push_back(edit);
        }

        return {{"changes", changes}};
    } catch (...) {
        return nullptr;
    }
}

nlohmann::json LspChannel::handle_prepare_rename(const nlohmann::json& params) {
    auto uri = params["textDocument"]["uri"].get<std::string>();
    auto line = params["position"]["line"].get<uint32_t>();
    auto character = params["position"]["character"].get<uint32_t>();
    std::string path_str = strip_file_uri(uri);

    auto symbol = model_.get_symbol_at(path_str, line, character);
    if (symbol.empty()) {
        return nullptr;  // Can't rename at this position
    }

    // Find the exact node to get the column
    auto ast = model_.get_ast(path_str);
    if (!ast) {
        std::error_code ec;
        auto canonical = std::filesystem::canonical(path_str, ec);
        if (!ec) ast = model_.get_ast(canonical);
    }

    uint32_t sym_col = character;
    if (ast) {
        std::function<void(const std::shared_ptr<ASTNode>&)> find;
        find = [&](const std::shared_ptr<ASTNode>& node) {
            if (!node) return;
            if (node->name == symbol && node->location.line == line) {
                sym_col = node->location.column;
            }
            for (const auto& child : node->children) find(child);
        };
        find(ast);
    }

    return {
        {"range", {
            {"start", {{"line", line}, {"character", sym_col}}},
            {"end", {{"line", line}, {"character", sym_col + static_cast<uint32_t>(symbol.size())}}}
        }},
        {"placeholder", symbol}
    };
}

nlohmann::json LspChannel::handle_signature_help(const nlohmann::json& params) {
    auto uri = params["textDocument"]["uri"].get<std::string>();
    std::string path_str = strip_file_uri(uri);
    auto line = params["position"]["line"].get<uint32_t>();
    auto character = params["position"]["character"].get<uint32_t>();

    // Get line text for context
    std::string line_text;
    auto content = token_provider_.get_content(uri);
    if (content) {
        std::istringstream lines(*content);
        std::string l;
        uint32_t ln = 0;
        while (std::getline(lines, l)) {
            if (ln == line) { line_text = l; break; }
            ++ln;
        }
    } else {
        std::ifstream ifs(path_str);
        if (ifs.is_open()) {
            std::string l;
            uint32_t ln = 0;
            while (std::getline(ifs, l)) {
                if (ln == line) { line_text = l; break; }
                ++ln;
            }
        }
    }

    if (line_text.empty()) return nullptr;

    auto sig_result = completion_provider_.get_signature_help(
        std::filesystem::path(path_str), line, character, line_text);

    if (sig_result.signatures.empty()) return nullptr;

    nlohmann::json signatures = nlohmann::json::array();
    for (const auto& sig : sig_result.signatures) {
        nlohmann::json params_arr = nlohmann::json::array();
        for (const auto& p : sig.parameters) {
            std::string param_label = p.name + ": " + p.type;
            params_arr.push_back({{"label", param_label}, {"documentation", p.documentation}});
        }
        signatures.push_back({
            {"label", sig.label},
            {"documentation", sig.documentation},
            {"parameters", params_arr}
        });
    }

    return {
        {"signatures", signatures},
        {"activeSignature", sig_result.active_signature},
        {"activeParameter", sig_result.signatures.empty() ? 0 : sig_result.signatures[0].active_parameter}
    };
}

nlohmann::json LspChannel::handle_workspace_symbol(const nlohmann::json& params) {
    auto query = params.value("query", "");

    try {
        auto symbols = nav_provider_.get_workspace_symbols(query);

        nlohmann::json result = nlohmann::json::array();
        for (const auto& sym : symbols) {
            nlohmann::json loc;
            loc["uri"] = "file://" + sym.file.string();
            loc["range"] = {
                {"start", {{"line", sym.line}, {"character", sym.column}}},
                {"end",   {{"line", sym.line}, {"character", sym.column + sym.name.size()}}}
            };

            result.push_back({
                {"name", sym.name},
                {"kind", lsp_symbol_kind(sym.kind)},
                {"location", loc},
                {"containerName", sym.container}
            });
        }
        return result;
    } catch (...) {
        return nlohmann::json::array();
    }
}

nlohmann::json LspChannel::handle_folding_range(const nlohmann::json& params) {
    auto uri = params["textDocument"]["uri"].get<std::string>();
    std::string path_str = strip_file_uri(uri);

    // Get content
    std::string content;
    auto cached = token_provider_.get_content(uri);
    if (cached) {
        content = *cached;
    } else {
        std::ifstream ifs(path_str);
        if (!ifs.is_open()) return nlohmann::json::array();
        std::ostringstream oss;
        oss << ifs.rdbuf();
        content = oss.str();
    }

    nlohmann::json result = nlohmann::json::array();

    // Track brace-delimited blocks
    std::vector<uint32_t> brace_stack;  // line numbers of opening braces
    std::istringstream lines(content);
    std::string line;
    uint32_t line_num = 0;
    bool in_comment = false;
    uint32_t comment_start = 0;

    while (std::getline(lines, line)) {
        // Track multi-line comments
        if (!in_comment && line.find("/*") != std::string::npos) {
            in_comment = true;
            comment_start = line_num;
        }
        if (in_comment && line.find("*/") != std::string::npos) {
            in_comment = false;
            if (line_num > comment_start) {
                result.push_back({
                    {"startLine", comment_start},
                    {"endLine", line_num},
                    {"kind", "comment"}
                });
            }
        }

        // Track braces (skip strings and comments)
        bool in_string = false;
        for (size_t i = 0; i < line.size(); ++i) {
            if (line[i] == '"' && (i == 0 || line[i-1] != '\\')) {
                in_string = !in_string;
                continue;
            }
            if (in_string) continue;
            if (i + 1 < line.size() && line[i] == '/' && line[i+1] == '/') break;

            if (line[i] == '{') {
                brace_stack.push_back(line_num);
            } else if (line[i] == '}' && !brace_stack.empty()) {
                uint32_t start = brace_stack.back();
                brace_stack.pop_back();
                if (line_num > start) {
                    result.push_back({
                        {"startLine", start},
                        {"endLine", line_num},
                        {"kind", "region"}
                    });
                }
            }
        }
        ++line_num;
    }

    return result;
}

nlohmann::json LspChannel::handle_inlay_hint(const nlohmann::json& params) {
    auto uri = params["textDocument"]["uri"].get<std::string>();
    std::string path_str = strip_file_uri(uri);

    auto ast = model_.get_ast(path_str);
    if (!ast) {
        std::error_code ec;
        auto canonical = std::filesystem::canonical(path_str, ec);
        if (!ec) ast = model_.get_ast(canonical);
    }
    if (!ast) return nlohmann::json::array();

    nlohmann::json result = nlohmann::json::array();

    std::function<void(const std::shared_ptr<ASTNode>&)> collect;
    collect = [&](const std::shared_ptr<ASTNode>& node) {
        if (!node) return;
        // Show type hints for val/var declarations that have inferred types
        if ((node->kind == "val_declaration" || node->kind == "var_declaration") &&
            !node->type_info.empty() && !node->name.empty()) {
            result.push_back({
                {"position", {{"line", node->location.line}, {"character", node->location.column + static_cast<uint32_t>(node->name.size())}}},
                {"label", ": " + node->type_info},
                {"kind", 1},  // Type hint
                {"paddingLeft", false},
                {"paddingRight", true}
            });
        }
        for (const auto& child : node->children) {
            collect(child);
        }
    };
    collect(ast);

    return result;
}

nlohmann::json LspChannel::handle_code_action(const nlohmann::json& params) {
    auto uri = params["textDocument"]["uri"].get<std::string>();
    auto range = params["range"];
    auto context = params.value("context", nlohmann::json::object());
    std::string path_str = strip_file_uri(uri);

    nlohmann::json result = nlohmann::json::array();

    // Always offer "Sort Imports" as a source action
    result.push_back({
        {"title", "Sort Imports"},
        {"kind", "source.organizeImports"},
        {"isPreferred", false}
    });

    // If cursor is on a val/var without type annotation, offer "Add type annotation"
    auto start_line = range["start"]["line"].get<uint32_t>();
    auto ast = model_.get_ast(path_str);
    if (!ast) {
        std::error_code ec;
        auto canonical = std::filesystem::canonical(path_str, ec);
        if (!ec) ast = model_.get_ast(canonical);
    }
    if (ast) {
        std::function<void(const std::shared_ptr<ASTNode>&)> find_at;
        find_at = [&](const std::shared_ptr<ASTNode>& node) {
            if (!node) return;
            if (node->location.line == start_line &&
                (node->kind == "val_declaration" || node->kind == "var_declaration") &&
                !node->type_info.empty()) {
                // Offer to add explicit type annotation
                nlohmann::json edit;
                edit["range"] = {
                    {"start", {{"line", node->location.line}, {"character", node->location.column + static_cast<uint32_t>(node->name.size())}}},
                    {"end", {{"line", node->location.line}, {"character", node->location.column + static_cast<uint32_t>(node->name.size())}}}
                };
                edit["newText"] = ": " + node->type_info;

                result.push_back({
                    {"title", "Add type annotation: " + node->type_info},
                    {"kind", "quickfix"},
                    {"edit", {{"changes", {{uri, nlohmann::json::array({edit})}}}}}
                });
            }
            for (const auto& child : node->children) {
                find_at(child);
            }
        };
        find_at(ast);
    }

    return result;
}

// ── Call Hierarchy handlers ─────────────────────────────────────────────

nlohmann::json LspChannel::handle_prepare_call_hierarchy(const nlohmann::json& params) {
    auto uri = params["textDocument"]["uri"].get<std::string>();
    auto line = params["position"]["line"].get<uint32_t>();
    auto character = params["position"]["character"].get<uint32_t>();
    std::string path_str = strip_file_uri(uri);

    auto symbol = model_.get_symbol_at(path_str, line, character);
    if (symbol.empty()) return nlohmann::json::array();

    // Return the call hierarchy item for this symbol
    nlohmann::json item;
    item["name"] = symbol;
    item["kind"] = 12;  // Function
    item["uri"] = uri;
    item["range"] = {
        {"start", {{"line", line}, {"character", character}}},
        {"end", {{"line", line}, {"character", character + static_cast<uint32_t>(symbol.size())}}}
    };
    item["selectionRange"] = item["range"];

    return nlohmann::json::array({item});
}

nlohmann::json LspChannel::handle_incoming_calls(const nlohmann::json& params) {
    auto item = params["item"];
    auto symbol = item["name"].get<std::string>();

    try {
        // Find all references to this symbol — these are the callers
        auto refs = nav_provider_.find_references(symbol);

        nlohmann::json result = nlohmann::json::array();
        for (const auto& ref : refs) {
            // Skip definitions (only include call sites)
            if (ref.kind == "function" || ref.kind == "struct" ||
                ref.kind == "enum" || ref.kind == "trait" || ref.kind == "type")
                continue;

            nlohmann::json caller;
            caller["from"] = {
                {"name", ref.file.stem().string()},
                {"kind", 12},
                {"uri", "file://" + ref.file.string()},
                {"range", {{"start", {{"line", ref.line}, {"character", ref.column}}}, {"end", {{"line", ref.line}, {"character", ref.column + static_cast<uint32_t>(ref.name.size())}}}}},
                {"selectionRange", {{"start", {{"line", ref.line}, {"character", ref.column}}}, {"end", {{"line", ref.line}, {"character", ref.column + static_cast<uint32_t>(ref.name.size())}}}}}
            };
            caller["fromRanges"] = nlohmann::json::array({{
                {"start", {{"line", ref.line}, {"character", ref.column}}},
                {"end", {{"line", ref.line}, {"character", ref.column + static_cast<uint32_t>(ref.name.size())}}}
            }});
            result.push_back(caller);

            if (result.size() >= 50) break;  // Limit results
        }
        return result;
    } catch (...) {
        return nlohmann::json::array();
    }
}

nlohmann::json LspChannel::handle_outgoing_calls(const nlohmann::json& params) {
    auto item = params["item"];
    auto uri = item["uri"].get<std::string>();
    auto item_line = item["range"]["start"]["line"].get<uint32_t>();
    std::string path_str = strip_file_uri(uri);

    // Find function calls within this function's body
    auto ast = model_.get_ast(path_str);
    if (!ast) return nlohmann::json::array();

    nlohmann::json result = nlohmann::json::array();

    // Walk the AST looking for reference nodes after the function definition line
    std::function<void(const std::shared_ptr<ASTNode>&)> collect;
    collect = [&](const std::shared_ptr<ASTNode>& node) {
        if (!node) return;
        if (result.size() >= 50) return;

        if (node->kind == "reference" && node->location.line > item_line && !node->name.empty()) {
            // Check if this reference is a known function
            auto def = nav_provider_.go_to_definition(path_str, node->name);
            if (def && def->kind == "function") {
                nlohmann::json callee;
                callee["to"] = {
                    {"name", node->name},
                    {"kind", 12},
                    {"uri", "file://" + def->file.string()},
                    {"range", {{"start", {{"line", def->line}, {"character", def->column}}}, {"end", {{"line", def->line}, {"character", def->column + static_cast<uint32_t>(def->name.size())}}}}},
                    {"selectionRange", {{"start", {{"line", def->line}, {"character", def->column}}}, {"end", {{"line", def->line}, {"character", def->column + static_cast<uint32_t>(def->name.size())}}}}}
                };
                callee["fromRanges"] = nlohmann::json::array({{
                    {"start", {{"line", node->location.line}, {"character", node->location.column}}},
                    {"end", {{"line", node->location.line}, {"character", node->location.column + static_cast<uint32_t>(node->name.size())}}}
                }});
                result.push_back(callee);
            }
        }

        for (const auto& child : node->children) {
            collect(child);
        }
    };
    collect(ast);

    return result;
}

// ── Selection Range handler ─────────────────────────────────────────────

nlohmann::json LspChannel::handle_selection_range(const nlohmann::json& params) {
    auto uri = params["textDocument"]["uri"].get<std::string>();
    auto positions = params["positions"];
    std::string path_str = strip_file_uri(uri);

    std::string content;
    auto cached = token_provider_.get_content(uri);
    if (cached) content = *cached;
    else {
        std::ifstream ifs(path_str);
        if (!ifs.is_open()) return nlohmann::json::array();
        std::ostringstream oss;
        oss << ifs.rdbuf();
        content = oss.str();
    }

    // Build line offsets
    std::vector<uint32_t> line_starts;
    line_starts.push_back(0);
    for (size_t i = 0; i < content.size(); ++i) {
        if (content[i] == '\n') line_starts.push_back(i + 1);
    }

    // For each position, find enclosing brace scopes
    nlohmann::json result = nlohmann::json::array();
    for (const auto& pos : positions) {
        uint32_t line = pos["line"].get<uint32_t>();
        uint32_t character = pos["character"].get<uint32_t>();

        // Convert to offset
        uint32_t offset = 0;
        if (line < line_starts.size()) {
            offset = line_starts[line] + character;
        }

        // Find enclosing braces from innermost to outermost
        struct Scope { uint32_t start_line; uint32_t start_char; uint32_t end_line; uint32_t end_char; };
        std::vector<Scope> scopes;

        // Current line scope
        std::string current_line;
        uint32_t line_start_char = 0;
        if (line < line_starts.size()) {
            size_t ls = line_starts[line];
            size_t le = (line + 1 < line_starts.size()) ? line_starts[line + 1] : content.size();
            current_line = content.substr(ls, le - ls);
            while (line_start_char < current_line.size() && current_line[line_start_char] == ' ')
                ++line_start_char;
        }
        scopes.push_back({line, line_start_char, line, static_cast<uint32_t>(current_line.size())});

        // Find enclosing brace scopes
        std::vector<size_t> brace_opens;
        for (size_t i = 0; i < content.size() && i <= offset; ++i) {
            if (content[i] == '{') brace_opens.push_back(i);
            else if (content[i] == '}' && !brace_opens.empty()) {
                if (i < offset) brace_opens.pop_back();
            }
        }
        // Find matching close for each open
        for (auto it = brace_opens.rbegin(); it != brace_opens.rend(); ++it) {
            size_t open = *it;
            int depth = 1;
            size_t close = open + 1;
            while (close < content.size() && depth > 0) {
                if (content[close] == '{') ++depth;
                else if (content[close] == '}') --depth;
                ++close;
            }
            // Convert offsets to line/char
            uint32_t sl = 0, sc = 0, el = 0, ec = 0;
            for (uint32_t l = 0; l < line_starts.size(); ++l) {
                if (line_starts[l] <= open) { sl = l; sc = open - line_starts[l]; }
                if (line_starts[l] <= close) { el = l; ec = close - line_starts[l]; }
            }
            scopes.push_back({sl, sc, el, ec});
        }

        // Whole file scope
        uint32_t last_line = line_starts.size() > 0 ? line_starts.size() - 1 : 0;
        scopes.push_back({0, 0, last_line, 0});

        // Build nested SelectionRange (innermost first)
        nlohmann::json sel_range = nullptr;
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            nlohmann::json range = {
                {"start", {{"line", it->start_line}, {"character", it->start_char}}},
                {"end", {{"line", it->end_line}, {"character", it->end_char}}}
            };
            if (sel_range.is_null()) {
                sel_range = {{"range", range}};
            } else {
                sel_range = {{"range", range}, {"parent", sel_range}};
            }
        }
        result.push_back(sel_range);
    }
    return result;
}

// ── Linked Editing Range handler ────────────────────────────────────────

nlohmann::json LspChannel::handle_linked_editing_range(const nlohmann::json& params) {
    auto uri = params["textDocument"]["uri"].get<std::string>();
    auto line = params["position"]["line"].get<uint32_t>();
    auto character = params["position"]["character"].get<uint32_t>();
    std::string path_str = strip_file_uri(uri);

    auto symbol = model_.get_symbol_at(path_str, line, character);
    if (symbol.empty()) return nullptr;

    // Find all occurrences in the current file
    auto ast = model_.get_ast(path_str);
    if (!ast) {
        std::error_code ec;
        auto canonical = std::filesystem::canonical(path_str, ec);
        if (!ec) ast = model_.get_ast(canonical);
    }
    if (!ast) return nullptr;

    nlohmann::json ranges = nlohmann::json::array();
    std::function<void(const std::shared_ptr<ASTNode>&)> collect;
    collect = [&](const std::shared_ptr<ASTNode>& node) {
        if (!node) return;
        if (node->name == symbol) {
            ranges.push_back({
                {"start", {{"line", node->location.line}, {"character", node->location.column}}},
                {"end", {{"line", node->location.line}, {"character", node->location.column + static_cast<uint32_t>(node->name.size())}}}
            });
        }
        for (const auto& child : node->children) collect(child);
    };
    collect(ast);

    if (ranges.empty()) return nullptr;
    return {{"ranges", ranges}};
}

// ── Code Lens handler ───────────────────────────────────────────────────

nlohmann::json LspChannel::handle_code_lens(const nlohmann::json& params) {
    auto uri = params["textDocument"]["uri"].get<std::string>();
    std::string path_str = strip_file_uri(uri);

    auto ast = model_.get_ast(path_str);
    if (!ast) {
        std::error_code ec;
        auto canonical = std::filesystem::canonical(path_str, ec);
        if (!ec) ast = model_.get_ast(canonical);
    }
    if (!ast) return nlohmann::json::array();

    // Count declarations — skip code lens if too many (performance)
    int decl_count = 0;
    for (const auto& child : ast->children) {
        if (child && child->kind != "reference" && !child->name.empty()) ++decl_count;
    }
    if (decl_count > 50) return nlohmann::json::array();  // Cap at 50 declarations

    nlohmann::json result = nlohmann::json::array();

    // For each declaration, count references
    for (const auto& child : ast->children) {
        if (!child || child->name.empty()) continue;
        if (child->kind == "reference") continue;
        if (child->kind != "function" && child->kind != "struct" &&
            child->kind != "class" && child->kind != "enum" &&
            child->kind != "trait") continue;

        try {
            auto refs = nav_provider_.find_references(child->name);
            // Subtract 1 for the declaration itself
            int ref_count = static_cast<int>(refs.size()) - 1;
            if (ref_count < 0) ref_count = 0;

            std::string title = std::to_string(ref_count) + " reference" + (ref_count != 1 ? "s" : "");

            result.push_back({
                {"range", {
                    {"start", {{"line", child->location.line}, {"character", child->location.column}}},
                    {"end", {{"line", child->location.line}, {"character", child->location.column + static_cast<uint32_t>(child->name.size())}}}
                }},
                {"command", {
                    {"title", title},
                    {"command", "meld.showReferences"},
                    {"arguments", nlohmann::json::array({uri, {{"line", child->location.line}, {"character", child->location.column}}})}
                }}
            });
        } catch (...) {
            // Skip if reference counting fails
        }
    }

    return result;
}

// ── Type Hierarchy handlers ─────────────────────────────────────────────

nlohmann::json LspChannel::handle_prepare_type_hierarchy(const nlohmann::json& params) {
    auto uri = params["textDocument"]["uri"].get<std::string>();
    auto line = params["position"]["line"].get<uint32_t>();
    auto character = params["position"]["character"].get<uint32_t>();
    std::string path_str = strip_file_uri(uri);

    auto symbol = model_.get_symbol_at(path_str, line, character);
    if (symbol.empty()) return nlohmann::json::array();

    // Find the type definition
    auto loc = nav_provider_.go_to_definition(path_str, symbol);
    std::string kind = "struct";
    uint32_t def_line = line, def_col = character;
    std::string def_uri = uri;
    if (loc) {
        kind = loc->kind;
        def_line = loc->line;
        def_col = loc->column;
        def_uri = "file://" + loc->file.string();
    }

    return nlohmann::json::array({{
        {"name", symbol},
        {"kind", lsp_symbol_kind(kind)},
        {"uri", def_uri},
        {"range", {{"start", {{"line", def_line}, {"character", def_col}}}, {"end", {{"line", def_line}, {"character", def_col + static_cast<uint32_t>(symbol.size())}}}}},
        {"selectionRange", {{"start", {{"line", def_line}, {"character", def_col}}}, {"end", {{"line", def_line}, {"character", def_col + static_cast<uint32_t>(symbol.size())}}}}}
    }});
}

nlohmann::json LspChannel::handle_type_supertypes(const nlohmann::json& /*params*/) {
    // Would require trait/interface inheritance tracking in the AST
    return nlohmann::json::array();
}

nlohmann::json LspChannel::handle_type_subtypes(const nlohmann::json& /*params*/) {
    // Would require scanning for struct/class definitions that implement a trait
    return nlohmann::json::array();
}

}  // namespace meld::daemon
