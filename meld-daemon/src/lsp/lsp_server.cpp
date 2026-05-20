#include "meld/lsp/lsp_server.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>
#include <string>

namespace meld::lsp {

// =====================================================================
// Construction / destruction
// =====================================================================

LspServer::LspServer() = default;

LspServer::~LspServer() {
    request_shutdown();
}

// =====================================================================
// JSON-RPC message framing  (Content-Length header + JSON body)
// =====================================================================

std::string LspServer::read_message() {
    if (!input_) return {};

    // Read headers until blank line.
    std::string header_line;
    int content_length = -1;

    while (std::getline(*input_, header_line)) {
        // Strip trailing \r if present (HTTP-style CRLF).
        if (!header_line.empty() && header_line.back() == '\r') {
            header_line.pop_back();
        }
        if (header_line.empty()) {
            break;  // End of headers.
        }
        // Parse "Content-Length: <n>"
        const std::string prefix = "Content-Length: ";
        if (header_line.compare(0, prefix.size(), prefix) == 0) {
            content_length = std::stoi(header_line.substr(prefix.size()));
        }
        // Other headers (Content-Type) are accepted but ignored.
    }

    if (content_length <= 0) {
        return {};  // EOF or malformed header.
    }

    std::string body(static_cast<size_t>(content_length), '\0');
    input_->read(body.data(), content_length);

    if (input_->gcount() != content_length) {
        return {};  // Truncated read.
    }
    return body;
}

void LspServer::write_message(const std::string& json) {
    if (!output_) return;

    std::lock_guard<std::mutex> lock(output_mutex_);
    *output_ << "Content-Length: " << json.size() << "\r\n"
             << "\r\n"
             << json;
    output_->flush();
}

// =====================================================================
// Minimal JSON helpers (no external JSON library dependency)
// =====================================================================

static std::string escape_json_string(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

std::string LspServer::json_string(const std::string& key, const std::string& value) {
    return "\"" + key + "\":\"" + escape_json_string(value) + "\"";
}

std::string LspServer::json_int(const std::string& key, int value) {
    return "\"" + key + "\":" + std::to_string(value);
}

std::string LspServer::json_bool(const std::string& key, bool value) {
    return "\"" + key + "\":" + (value ? "true" : "false");
}

// =====================================================================
// Tiny JSON extraction helpers (avoid full parser dependency)
// =====================================================================

/// Extract a string value for a given key from a flat JSON object.
static std::string json_extract_string(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return {};

    // Skip past key, colon, optional whitespace, opening quote.
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return {};
    pos = json.find('"', pos + 1);
    if (pos == std::string::npos) return {};
    ++pos;  // skip opening quote

    std::string result;
    while (pos < json.size() && json[pos] != '"') {
        if (json[pos] == '\\' && pos + 1 < json.size()) {
            ++pos;
            switch (json[pos]) {
                case 'n': result += '\n'; break;
                case 't': result += '\t'; break;
                case 'r': result += '\r'; break;
                default:  result += json[pos]; break;
            }
        } else {
            result += json[pos];
        }
        ++pos;
    }
    return result;
}

/// Extract an integer value for a given key.
static int json_extract_int(const std::string& json, const std::string& key, int fallback = 0) {
    std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return fallback;
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return fallback;
    ++pos;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
    try { return std::stoi(json.substr(pos)); } catch (...) { return fallback; }
}

// =====================================================================
// Protocol dispatch
// =====================================================================

std::string LspServer::dispatch(const std::string& json) {
    // Extract JSON-RPC fields.
    std::string method = json_extract_string(json, "method");
    std::string id     = json_extract_string(json, "id");

    // If "id" wasn't a string, try integer form.
    if (id.empty()) {
        int id_int = json_extract_int(json, "id", -1);
        if (id_int >= 0) id = std::to_string(id_int);
    }

    // Find the "params" sub-object (simple brace matching).
    std::string params_json = "{}";
    {
        auto ppos = json.find("\"params\"");
        if (ppos != std::string::npos) {
            ppos = json.find_first_of("{[", ppos);
            if (ppos != std::string::npos) {
                char open = json[ppos];
                char close = (open == '{') ? '}' : ']';
                int depth = 1;
                size_t end = ppos + 1;
                while (end < json.size() && depth > 0) {
                    if (json[end] == open) ++depth;
                    else if (json[end] == close) --depth;
                    ++end;
                }
                params_json = json.substr(ppos, end - ppos);
            }
        }
    }

    // ── Lifecycle ───────────────────────────────────────────────
    if (method == "initialize") {
        return handle_initialize(id, params_json);
    }
    if (method == "initialized") {
        return {};  // No response required.
    }
    if (method == "shutdown") {
        shutdown_requested_ = true;
        return "{\"jsonrpc\":\"2.0\",\"id\":" + id + ",\"result\":null}";
    }
    if (method == "exit") {
        running_ = false;
        return {};
    }

    // ── Document synchronisation ─────────────────────────────────
    if (method == "textDocument/didOpen") {
        handle_did_open(params_json);
        return {};
    }
    if (method == "textDocument/didChange") {
        handle_did_change(params_json);
        return {};
    }

    // ── Feature requests (require a response) ───────────────────
    if (method == "textDocument/diagnostic" || method == "textDocument/diagnostics") {
        std::string uri = json_extract_string(params_json, "uri");
        auto diags = handle_diagnostics(uri);
        std::ostringstream items;
        items << "[";
        for (size_t i = 0; i < diags.size(); ++i) {
            if (i > 0) items << ",";
            const auto& d = diags[i];
            items << "{\"range\":{\"start\":{\"line\":" << d.range.start.line
                  << ",\"character\":" << d.range.start.character
                  << "},\"end\":{\"line\":" << d.range.end.line
                  << ",\"character\":" << d.range.end.character
                  << "}},\"severity\":" << static_cast<int>(d.severity)
                  << ",\"source\":\"" << escape_json_string(d.source)
                  << "\",\"message\":\"" << escape_json_string(d.message) << "\"}";
        }
        items << "]";
        return "{\"jsonrpc\":\"2.0\",\"id\":" + id +
               ",\"result\":{\"kind\":\"full\",\"items\":" + items.str() + "}}";
    }

    if (method == "textDocument/completion") {
        std::string uri = json_extract_string(params_json, "uri");
        // Extract position from params.position
        std::string pos_json = "{}";
        {
            auto ppos = params_json.find("\"position\"");
            if (ppos != std::string::npos) {
                ppos = params_json.find('{', ppos);
                if (ppos != std::string::npos) {
                    auto epos = params_json.find('}', ppos);
                    if (epos != std::string::npos)
                        pos_json = params_json.substr(ppos, epos - ppos + 1);
                }
            }
        }
        LspPosition pos;
        pos.line = static_cast<size_t>(json_extract_int(pos_json, "line"));
        pos.character = static_cast<size_t>(json_extract_int(pos_json, "character"));

        auto items = handle_completion(uri, pos);
        std::ostringstream arr;
        arr << "[";
        for (size_t i = 0; i < items.size(); ++i) {
            if (i > 0) arr << ",";
            arr << "{\"label\":\"" << escape_json_string(items[i].label) << "\""
                << ",\"kind\":" << items[i].kind
                << ",\"detail\":\"" << escape_json_string(items[i].detail) << "\""
                << ",\"documentation\":\"" << escape_json_string(items[i].documentation) << "\""
                << ",\"insertText\":\"" << escape_json_string(items[i].insert_text) << "\"}";
        }
        arr << "]";
        return "{\"jsonrpc\":\"2.0\",\"id\":" + id +
               ",\"result\":{\"isIncomplete\":false,\"items\":" + arr.str() + "}}";
    }

    if (method == "textDocument/definition") {
        std::string uri = json_extract_string(params_json, "uri");
        std::string pos_json = "{}";
        {
            auto ppos = params_json.find("\"position\"");
            if (ppos != std::string::npos) {
                ppos = params_json.find('{', ppos);
                if (ppos != std::string::npos) {
                    auto epos = params_json.find('}', ppos);
                    if (epos != std::string::npos)
                        pos_json = params_json.substr(ppos, epos - ppos + 1);
                }
            }
        }
        LspPosition pos;
        pos.line = static_cast<size_t>(json_extract_int(pos_json, "line"));
        pos.character = static_cast<size_t>(json_extract_int(pos_json, "character"));

        auto locs = handle_definition(uri, pos);
        std::ostringstream arr;
        arr << "[";
        for (size_t i = 0; i < locs.size(); ++i) {
            if (i > 0) arr << ",";
            arr << "{\"uri\":\"" << escape_json_string(locs[i].uri) << "\""
                << ",\"range\":{\"start\":{\"line\":" << locs[i].range.start.line
                << ",\"character\":" << locs[i].range.start.character
                << "},\"end\":{\"line\":" << locs[i].range.end.line
                << ",\"character\":" << locs[i].range.end.character << "}}}";
        }
        arr << "]";
        return "{\"jsonrpc\":\"2.0\",\"id\":" + id + ",\"result\":" + arr.str() + "}";
    }

    if (method == "textDocument/references") {
        std::string uri = json_extract_string(params_json, "uri");
        std::string pos_json = "{}";
        {
            auto ppos = params_json.find("\"position\"");
            if (ppos != std::string::npos) {
                ppos = params_json.find('{', ppos);
                if (ppos != std::string::npos) {
                    auto epos = params_json.find('}', ppos);
                    if (epos != std::string::npos)
                        pos_json = params_json.substr(ppos, epos - ppos + 1);
                }
            }
        }
        LspPosition pos;
        pos.line = static_cast<size_t>(json_extract_int(pos_json, "line"));
        pos.character = static_cast<size_t>(json_extract_int(pos_json, "character"));

        auto locs = handle_references(uri, pos);
        std::ostringstream arr;
        arr << "[";
        for (size_t i = 0; i < locs.size(); ++i) {
            if (i > 0) arr << ",";
            arr << "{\"uri\":\"" << escape_json_string(locs[i].uri) << "\""
                << ",\"range\":{\"start\":{\"line\":" << locs[i].range.start.line
                << ",\"character\":" << locs[i].range.start.character
                << "},\"end\":{\"line\":" << locs[i].range.end.line
                << ",\"character\":" << locs[i].range.end.character << "}}}";
        }
        arr << "]";
        return "{\"jsonrpc\":\"2.0\",\"id\":" + id + ",\"result\":" + arr.str() + "}";
    }

    if (method == "textDocument/formatting") {
        std::string uri = json_extract_string(params_json, "uri");
        auto edits = handle_formatting(uri);
        std::ostringstream arr;
        arr << "[";
        for (size_t i = 0; i < edits.size(); ++i) {
            if (i > 0) arr << ",";
            arr << "{\"range\":{\"start\":{\"line\":" << edits[i].range.start.line
                << ",\"character\":" << edits[i].range.start.character
                << "},\"end\":{\"line\":" << edits[i].range.end.line
                << ",\"character\":" << edits[i].range.end.character
                << "}},\"newText\":\"" << escape_json_string(edits[i].new_text) << "\"}";
        }
        arr << "]";
        return "{\"jsonrpc\":\"2.0\",\"id\":" + id + ",\"result\":" + arr.str() + "}";
    }

    // Unknown method — return MethodNotFound (-32601).
    if (!id.empty()) {
        return "{\"jsonrpc\":\"2.0\",\"id\":" + id +
               ",\"error\":{\"code\":-32601,\"message\":\"Method not found: " +
               escape_json_string(method) + "\"}}";
    }
    return {};  // Unknown notification — silently ignore.
}

// =====================================================================
// initialize handler — returns server capabilities
// =====================================================================

std::string LspServer::handle_initialize(const std::string& id,
                                         const std::string& /*params_json*/) {
    // Build capabilities object.
    std::ostringstream caps;
    caps << "{"
         // Full document sync (client sends entire content on change).
         << "\"textDocumentSync\":1"
         // Completion support.
         << ",\"completionProvider\":{\"triggerCharacters\":[\".\",\":\"]}"
         // Go-to-definition.
         << ",\"definitionProvider\":true"
         // Find references.
         << ",\"referencesProvider\":true"
         // Document formatting.
         << ",\"documentFormattingProvider\":true"
         // Pull-model diagnostics.
         << ",\"diagnosticProvider\":{\"interFileDependencies\":false,\"workspaceDiagnostics\":false}"
         << "}";

    std::ostringstream result;
    result << "{\"capabilities\":" << caps.str()
           << ",\"serverInfo\":{\"name\":\"meld-lsp\",\"version\":\"0.1.0\"}}";

    return "{\"jsonrpc\":\"2.0\",\"id\":" + id +
           ",\"result\":" + result.str() + "}";
}

// =====================================================================
// textDocument/didOpen
// =====================================================================

void LspServer::handle_did_open(const std::string& params_json) {
    // Extract textDocument fields.
    // The params shape is: { textDocument: { uri, languageId, version, text } }
    std::string td_json = params_json;
    {
        auto pos = params_json.find("\"textDocument\"");
        if (pos != std::string::npos) {
            pos = params_json.find('{', pos + 14);
            if (pos != std::string::npos) {
                int depth = 1;
                size_t end = pos + 1;
                while (end < params_json.size() && depth > 0) {
                    if (params_json[end] == '{') ++depth;
                    else if (params_json[end] == '}') --depth;
                    ++end;
                }
                td_json = params_json.substr(pos, end - pos);
            }
        }
    }

    TextDocument doc;
    doc.uri         = json_extract_string(td_json, "uri");
    doc.language_id = json_extract_string(td_json, "languageId");
    doc.version     = json_extract_int(td_json, "version");
    doc.content     = json_extract_string(td_json, "text");

    {
        std::lock_guard<std::mutex> lock(documents_mutex_);
        documents_[doc.uri] = std::move(doc);
    }

    // Publish initial diagnostics for the opened document.
    publish_diagnostics(doc.uri.empty() ? "" : documents_.begin()->first);
}

// =====================================================================
// textDocument/didChange
// =====================================================================

void LspServer::handle_did_change(const std::string& params_json) {
    // Extract URI from textDocument.
    std::string td_json = params_json;
    {
        auto pos = params_json.find("\"textDocument\"");
        if (pos != std::string::npos) {
            pos = params_json.find('{', pos + 14);
            if (pos != std::string::npos) {
                int depth = 1;
                size_t end = pos + 1;
                while (end < params_json.size() && depth > 0) {
                    if (params_json[end] == '{') ++depth;
                    else if (params_json[end] == '}') --depth;
                    ++end;
                }
                td_json = params_json.substr(pos, end - pos);
            }
        }
    }

    std::string uri = json_extract_string(td_json, "uri");
    int version     = json_extract_int(td_json, "version");

    // With full document sync (textDocumentSync = 1), the first
    // contentChanges entry contains the full new text.
    std::string new_text;
    {
        auto pos = params_json.find("\"contentChanges\"");
        if (pos != std::string::npos) {
            // Find the first object inside the array.
            pos = params_json.find('[', pos);
            if (pos != std::string::npos) {
                pos = params_json.find('{', pos);
                if (pos != std::string::npos) {
                    int depth = 1;
                    size_t end = pos + 1;
                    while (end < params_json.size() && depth > 0) {
                        if (params_json[end] == '{') ++depth;
                        else if (params_json[end] == '}') --depth;
                        ++end;
                    }
                    std::string change_json = params_json.substr(pos, end - pos);
                    new_text = json_extract_string(change_json, "text");
                }
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(documents_mutex_);
        auto it = documents_.find(uri);
        if (it != documents_.end()) {
            it->second.version = version;
            it->second.content = new_text;
        }
    }

    // Re-publish diagnostics after content change.
    publish_diagnostics(uri);
}

// =====================================================================
// publish_diagnostics — server → client notification
// =====================================================================

void LspServer::publish_diagnostics(const std::string& uri) {
    auto diags = handle_diagnostics(uri);

    std::ostringstream items;
    items << "[";
    for (size_t i = 0; i < diags.size(); ++i) {
        if (i > 0) items << ",";
        const auto& d = diags[i];
        items << "{\"range\":{\"start\":{\"line\":" << d.range.start.line
              << ",\"character\":" << d.range.start.character
              << "},\"end\":{\"line\":" << d.range.end.line
              << ",\"character\":" << d.range.end.character
              << "}},\"severity\":" << static_cast<int>(d.severity)
              << ",\"source\":\"" << escape_json_string(d.source)
              << "\",\"message\":\"" << escape_json_string(d.message) << "\"}";
    }
    items << "]";

    std::string notification =
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\""
        ",\"params\":{\"uri\":\"" + escape_json_string(uri) +
        "\",\"diagnostics\":" + items.str() + "}}";

    write_message(notification);
}

// =====================================================================
// Feature handlers
// =====================================================================

// ── textDocument/diagnostics ────────────────────────────────────────
// Uses the Parser to detect syntax errors in the document content.
// In a full implementation this would invoke meld::parser::Parser;
// here we perform lightweight brace/keyword validation.

std::vector<LspDiagnostic> LspServer::handle_diagnostics(const std::string& uri) {
    std::vector<LspDiagnostic> result;

    std::string content;
    {
        std::lock_guard<std::mutex> lock(documents_mutex_);
        auto it = documents_.find(uri);
        if (it == documents_.end()) return result;
        content = it->second.content;
    }

    // Simple brace-balance check as a stand-in for full Parser integration.
    int brace_depth = 0;
    size_t line = 0, col = 0;
    size_t open_line = 0, open_col = 0;
    for (size_t i = 0; i < content.size(); ++i) {
        char c = content[i];
        if (c == '\n') { ++line; col = 0; continue; }
        if (c == '{') {
            if (brace_depth == 0) { open_line = line; open_col = col; }
            ++brace_depth;
        } else if (c == '}') {
            --brace_depth;
            if (brace_depth < 0) {
                LspDiagnostic d;
                d.range = {{line, col}, {line, col + 1}};
                d.severity = LspDiagnosticSeverity::Error;
                d.message = "Unexpected closing brace '}'";
                result.push_back(d);
                brace_depth = 0;
            }
        }
        ++col;
    }
    if (brace_depth > 0) {
        LspDiagnostic d;
        d.range = {{open_line, open_col}, {open_line, open_col + 1}};
        d.severity = LspDiagnosticSeverity::Error;
        d.message = "Unmatched opening brace '{'";
        result.push_back(d);
    }

    // Flag use of banned import keywords (only `imp` is valid in Meld).
    std::istringstream stream(content);
    std::string src_line;
    size_t ln = 0;
    while (std::getline(stream, src_line)) {
        // Check for banned keywords: import, from, as (as import aliases).
        for (const auto& banned : {"import", "from"}) {
            std::string kw(banned);
            auto pos = src_line.find(kw);
            while (pos != std::string::npos) {
                // Ensure it's a whole word.
                bool word_start = (pos == 0 || !std::isalnum(static_cast<unsigned char>(src_line[pos - 1])));
                bool word_end   = (pos + kw.size() >= src_line.size() ||
                                   !std::isalnum(static_cast<unsigned char>(src_line[pos + kw.size()])));
                if (word_start && word_end) {
                    LspDiagnostic d;
                    d.range = {{ln, pos}, {ln, pos + kw.size()}};
                    d.severity = LspDiagnosticSeverity::Error;
                    d.message = "Use 'imp' instead of '" + kw + "' — Meld uses the 'imp' keyword for imports";
                    result.push_back(d);
                }
                pos = src_line.find(kw, pos + kw.size());
            }
        }
        ++ln;
    }

    return result;
}

// ── textDocument/completion ──────────────────────────────────────────
// Returns completion items from the symbol table of the parsed AST.
// In a full implementation this would walk the AST produced by Parser;
// here we extract identifiers from the document as a lightweight proxy.

std::vector<LspCompletionItem> LspServer::handle_completion(
        const std::string& uri, const LspPosition& /*position*/) {
    std::vector<LspCompletionItem> items;

    std::string content;
    {
        std::lock_guard<std::mutex> lock(documents_mutex_);
        auto it = documents_.find(uri);
        if (it == documents_.end()) return items;
        content = it->second.content;
    }

    // Collect identifiers that look like function definitions (fnc <name>)
    // or val/var bindings.
    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        // fnc <name>
        auto fpos = line.find("fnc ");
        if (fpos != std::string::npos) {
            size_t start = fpos + 4;
            size_t end = start;
            while (end < line.size() && (std::isalnum(static_cast<unsigned char>(line[end])) || line[end] == '_'))
                ++end;
            if (end > start) {
                LspCompletionItem ci;
                ci.label = line.substr(start, end - start);
                ci.kind = 3;  // Function
                ci.detail = "function";
                ci.insert_text = ci.label;
                items.push_back(std::move(ci));
            }
        }
        // val <name> / var <name>
        for (const char* kw : {"val ", "var "}) {
            auto vpos = line.find(kw);
            if (vpos != std::string::npos) {
                size_t start = vpos + 4;
                size_t end = start;
                while (end < line.size() && (std::isalnum(static_cast<unsigned char>(line[end])) || line[end] == '_'))
                    ++end;
                if (end > start) {
                    LspCompletionItem ci;
                    ci.label = line.substr(start, end - start);
                    ci.kind = 6;  // Variable
                    ci.detail = (kw[2] == 'l') ? "immutable binding" : "mutable binding";
                    ci.insert_text = ci.label;
                    items.push_back(std::move(ci));
                }
            }
        }
    }

    // Add Meld language keywords as snippet completions.
    static const std::vector<std::pair<std::string, std::string>> keywords = {
        {"fnc",    "function definition"},
        {"val",    "immutable binding"},
        {"var",    "mutable binding"},
        {"if",     "conditional"},
        {"else",   "else branch"},
        {"match",  "pattern match"},
        {"imp",    "import declaration"},
        {"struct", "struct definition"},
        {"enum",   "enum definition"},
        {"effect", "effect definition"},
        {"handle", "effect handler"},
        {"perform","perform effect"},
        {"return", "return value"},
    };
    for (const auto& [kw, desc] : keywords) {
        LspCompletionItem ci;
        ci.label = kw;
        ci.kind = 14;  // Keyword
        ci.detail = desc;
        ci.insert_text = kw;
        items.push_back(std::move(ci));
    }

    return items;
}

// ── textDocument/definition ──────────────────────────────────────────
// Resolves an identifier at the given position to its definition site.
// In a full implementation this would use AST identifier resolution;
// here we scan the document for the word under the cursor and find
// its first occurrence as a definition (fnc/val/var/struct/enum).

std::vector<LspLocation> LspServer::handle_definition(
        const std::string& uri, const LspPosition& position) {
    std::vector<LspLocation> result;

    std::string content;
    {
        std::lock_guard<std::mutex> lock(documents_mutex_);
        auto it = documents_.find(uri);
        if (it == documents_.end()) return result;
        content = it->second.content;
    }

    // Split content into lines.
    std::vector<std::string> lines;
    {
        std::istringstream s(content);
        std::string l;
        while (std::getline(s, l)) lines.push_back(l);
    }

    if (position.line >= lines.size()) return result;
    const std::string& cur_line = lines[position.line];

    // Extract the word under the cursor.
    size_t col = position.character;
    if (col >= cur_line.size()) return result;

    size_t word_start = col;
    while (word_start > 0 &&
           (std::isalnum(static_cast<unsigned char>(cur_line[word_start - 1])) ||
            cur_line[word_start - 1] == '_'))
        --word_start;

    size_t word_end = col;
    while (word_end < cur_line.size() &&
           (std::isalnum(static_cast<unsigned char>(cur_line[word_end])) ||
            cur_line[word_end] == '_'))
        ++word_end;

    std::string word = cur_line.substr(word_start, word_end - word_start);
    if (word.empty()) return result;

    // Search for a definition site: fnc <word>, val <word>, var <word>,
    // struct <word>, enum <word>.
    static const std::vector<std::string> def_keywords = {
        "fnc ", "val ", "var ", "struct ", "enum "
    };

    for (size_t ln = 0; ln < lines.size(); ++ln) {
        for (const auto& kw : def_keywords) {
            auto pos = lines[ln].find(kw);
            if (pos == std::string::npos) continue;
            size_t name_start = pos + kw.size();
            size_t name_end = name_start;
            while (name_end < lines[ln].size() &&
                   (std::isalnum(static_cast<unsigned char>(lines[ln][name_end])) ||
                    lines[ln][name_end] == '_'))
                ++name_end;
            if (lines[ln].substr(name_start, name_end - name_start) == word) {
                LspLocation loc;
                loc.uri = uri;
                loc.range = {{ln, name_start}, {ln, name_end}};
                result.push_back(loc);
                return result;  // Return first definition found.
            }
        }
    }

    return result;
}

// ── textDocument/references ──────────────────────────────────────────
// Finds all occurrences of the symbol under the cursor in the document.
// In a full implementation this would use AST symbol search across
// the workspace; here we do a simple whole-word text search.

std::vector<LspLocation> LspServer::handle_references(
        const std::string& uri, const LspPosition& position) {
    std::vector<LspLocation> result;

    std::string content;
    {
        std::lock_guard<std::mutex> lock(documents_mutex_);
        auto it = documents_.find(uri);
        if (it == documents_.end()) return result;
        content = it->second.content;
    }

    // Split into lines.
    std::vector<std::string> lines;
    {
        std::istringstream s(content);
        std::string l;
        while (std::getline(s, l)) lines.push_back(l);
    }

    if (position.line >= lines.size()) return result;
    const std::string& cur_line = lines[position.line];

    // Extract word under cursor.
    size_t col = position.character;
    if (col >= cur_line.size()) return result;

    size_t ws = col;
    while (ws > 0 && (std::isalnum(static_cast<unsigned char>(cur_line[ws - 1])) || cur_line[ws - 1] == '_'))
        --ws;
    size_t we = col;
    while (we < cur_line.size() && (std::isalnum(static_cast<unsigned char>(cur_line[we])) || cur_line[we] == '_'))
        ++we;

    std::string word = cur_line.substr(ws, we - ws);
    if (word.empty()) return result;

    // Find all whole-word occurrences across all lines.
    for (size_t ln = 0; ln < lines.size(); ++ln) {
        size_t search_pos = 0;
        while (search_pos < lines[ln].size()) {
            auto found = lines[ln].find(word, search_pos);
            if (found == std::string::npos) break;

            bool word_start = (found == 0 ||
                               !(std::isalnum(static_cast<unsigned char>(lines[ln][found - 1])) ||
                                 lines[ln][found - 1] == '_'));
            bool word_end = (found + word.size() >= lines[ln].size() ||
                             !(std::isalnum(static_cast<unsigned char>(lines[ln][found + word.size()])) ||
                               lines[ln][found + word.size()] == '_'));

            if (word_start && word_end) {
                LspLocation loc;
                loc.uri = uri;
                loc.range = {{ln, found}, {ln, found + word.size()}};
                result.push_back(loc);
            }
            search_pos = found + word.size();
        }
    }

    return result;
}

