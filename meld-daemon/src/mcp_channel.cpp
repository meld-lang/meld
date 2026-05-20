#include "meld/daemon/mcp_channel.hpp"

#include "meld/parser/parser.hpp"
#include "meld/interpreter/ast_interpreter.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include <unistd.h>

namespace meld::daemon {

McpChannel::McpChannel(SemanticModel& model, int port)
    : model_(model), port_(port) {
    // Register built-in tools
    tools_["analyze_safety"] = [this](const nlohmann::json& args) {
        return handle_analyze_safety(args);
    };
    tools_["trace_effect"] = [this](const nlohmann::json& args) {
        return handle_trace_effect(args);
    };
    tools_["query_type"] = [this](const nlohmann::json& args) {
        return handle_query_type(args);
    };
    tools_["query_ownership"] = [this](const nlohmann::json& args) {
        return handle_query_ownership(args);
    };
    tools_["get_diagnostics"] = [this](const nlohmann::json& args) {
        return handle_get_diagnostics(args);
    };
    tools_["structural_diff"] = [this](const nlohmann::json& args) {
        return handle_structural_diff(args);
    };
    tools_["meld_eval"] = [this](const nlohmann::json& args) {
        return handle_meld_eval(args);
    };
    tools_["meld_check"] = [this](const nlohmann::json& args) {
        return handle_meld_check(args);
    };
    tools_["search_api"] = [this](const nlohmann::json& args) {
        return handle_search_api(args);
    };
    tools_["execute_script"] = [this](const nlohmann::json& args) {
        return handle_execute_script(args);
    };
}

McpChannel::~McpChannel() {
    stop();
}

void McpChannel::start() {
    running_ = true;
    // In production: start HTTP server on port_, accept SSE connections
    // For now, the event loop is driven externally
}

void McpChannel::stop() {
    running_ = false;
}

void McpChannel::run_stdio(std::istream& in, std::ostream& out) {
    running_ = true;

    auto send = [&](const nlohmann::json& msg) {
        out << msg.dump() << "\n";
        out.flush();
    };

    auto read_message = [&]() -> nlohmann::json {
        std::string line;
        if (!std::getline(in, line)) return nullptr;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) return nullptr;
        return nlohmann::json::parse(line, nullptr, false);
    };

    // Build tools/list response
    auto make_tools_list = [&]() {
        nlohmann::json tools_arr = nlohmann::json::array();
        for (const auto& [name, _] : tools_) {
            nlohmann::json tool;
            tool["name"] = name;
            tool["description"] = "Meld " + name + " tool";
            tool["inputSchema"] = {{"type", "object"}};
            tools_arr.push_back(tool);
        }
        return tools_arr;
    };

    while (running_ && in.good()) {
        auto msg = read_message();
        if (msg.is_null() || msg.is_discarded()) break;

        auto method = msg.value("method", "");
        auto id = msg.contains("id") ? msg["id"] : nlohmann::json(nullptr);

        nlohmann::json resp;
        resp["jsonrpc"] = "2.0";
        resp["id"] = id;

        if (method == "initialize") {
            resp["result"] = {
                {"protocolVersion", "2024-11-05"},
                {"capabilities", {{"tools", {{"listChanged", false}}}}},
                {"serverInfo", {{"name", "meldd"}, {"version", "0.1.0"}}}
            };
            send(resp);
        } else if (method == "notifications/initialized") {
            // No response needed for notifications
        } else if (method == "tools/list") {
            resp["result"] = {{"tools", make_tools_list()}};
            send(resp);
        } else if (method == "tools/call") {
            auto params = msg.value("params", nlohmann::json::object());
            McpToolRequest req;
            req.tool_name = params.value("name", "");
            req.request_id = id.is_string() ? id.get<std::string>() : id.dump();
            req.arguments = params.value("arguments", nlohmann::json::object());
            auto result = handle_tool_call(req);
            nlohmann::json content = nlohmann::json::array();
            content.push_back({{"type", "text"}, {"text", result.result.dump()}});
            resp["result"] = {{"content", content}, {"isError", result.is_error}};
            send(resp);
        } else if (method == "ping") {
            resp["result"] = nlohmann::json::object();
            send(resp);
        } else {
            resp["error"] = {{"code", -32601}, {"message", "Method not found: " + method}};
            send(resp);
        }
    }

    running_ = false;
}

void McpChannel::run_on_fd(int fd) {
    running_ = true;

    // Buffered read helper
    std::string buf;

    auto read_line = [&]() -> std::string {
        while (true) {
            auto pos = buf.find('\n');
            if (pos != std::string::npos) {
                auto line = buf.substr(0, pos);
                buf.erase(0, pos + 1);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                return line;
            }
            char tmp[4096];
            auto r = ::read(fd, tmp, sizeof(tmp));
            if (r <= 0) return "";
            buf.append(tmp, r);
        }
    };

    auto send_msg = [&](const nlohmann::json& msg) {
        auto line = msg.dump() + "\n";
        const char* p = line.c_str();
        size_t remaining = line.size();
        while (remaining > 0) {
            auto w = ::write(fd, p, remaining);
            if (w <= 0) return;
            p += w;
            remaining -= w;
        }
    };

    auto make_tools_list = [&]() {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& [name, _] : tools_) {
            arr.push_back({{"name", name}, {"description", "Meld " + name + " tool"}, {"inputSchema", {{"type", "object"}}}});
        }
        return arr;
    };

    while (running_) {
        // MCP uses newline-delimited JSON (not LSP Content-Length framing)
        auto line = read_line();
        if (line.empty()) break;

        auto msg = nlohmann::json::parse(line, nullptr, false);
        if (msg.is_discarded()) break;

        auto method = msg.value("method", "");
        auto id = msg.contains("id") ? msg["id"] : nlohmann::json(nullptr);

        nlohmann::json resp;
        resp["jsonrpc"] = "2.0";
        resp["id"] = id;

        if (method == "initialize") {
            resp["result"] = {
                {"protocolVersion", "2024-11-05"},
                {"capabilities", {{"tools", {{"listChanged", false}}}}},
                {"serverInfo", {{"name", "meldd"}, {"version", "0.1.0"}}}
            };
            send_msg(resp);
        } else if (method == "notifications/initialized") {
            // no response
        } else if (method == "tools/list") {
            resp["result"] = {{"tools", make_tools_list()}};
            send_msg(resp);
        } else if (method == "tools/call") {
            auto params = msg.value("params", nlohmann::json::object());
            McpToolRequest req;
            req.tool_name = params.value("name", "");
            req.request_id = id.is_string() ? id.get<std::string>() : id.dump();
            req.arguments = params.value("arguments", nlohmann::json::object());
            auto result = handle_tool_call(req);
            nlohmann::json content = nlohmann::json::array();
            content.push_back({{"type", "text"}, {"text", result.result.dump()}});
            resp["result"] = {{"content", content}, {"isError", result.is_error}};
            send_msg(resp);
        } else if (method == "ping") {
            resp["result"] = nlohmann::json::object();
            send_msg(resp);
        } else {
            resp["error"] = {{"code", -32601}, {"message", "Method not found: " + method}};
            send_msg(resp);
        }
    }

    ::close(fd);
    running_ = false;
}

void McpChannel::push_diagnostic_event(const std::filesystem::path& file,
                                       const std::vector<Diagnostic>& diags) {
    // Format as dual-voice and push via SSE
    nlohmann::json event;
    event["type"] = "diagnostics";
    event["uri"] = file.string();
    nlohmann::json diag_array = nlohmann::json::array();
    for (const auto& d : diags) {
        auto dv = DualVoiceFormatter::format_diagnostic(d);
        diag_array.push_back(DualVoiceFormatter::to_mcp_json(dv));
    }
    event["diagnostics"] = diag_array;
    // SSE push would happen here
}

void McpChannel::register_tool(const std::string& name, ToolHandler handler) {
    tools_[name] = std::move(handler);
}

McpToolResponse McpChannel::handle_tool_call(const McpToolRequest& request) {
    McpToolResponse response;
    response.request_id = request.request_id;

    auto it = tools_.find(request.tool_name);
    if (it == tools_.end()) {
        response.is_error = true;
        response.result = {{"error", "Unknown tool: " + request.tool_name}};
        return response;
    }

    try {
        response.result = it->second(request.arguments);
    } catch (const std::exception& e) {
        response.is_error = true;
        response.result = {{"error", e.what()}};
    }
    return response;
}

nlohmann::json McpChannel::handle_analyze_safety(const nlohmann::json& args) {
    auto file = args.value("file", "");
    auto diags = model_.get_diagnostics(file);

    nlohmann::json result;
    result["file"] = file;
    nlohmann::json issues = nlohmann::json::array();
    for (const auto& d : diags) {
        if (d.rule_id.find("safety") != std::string::npos ||
            d.rule_id.find("ownership") != std::string::npos) {
            auto dv = DualVoiceFormatter::format_diagnostic(d);
            issues.push_back(DualVoiceFormatter::to_mcp_json(dv));
        }
    }
    result["safety_issues"] = issues;
    result["safe"] = issues.empty();
    return result;
}

nlohmann::json McpChannel::handle_trace_effect(const nlohmann::json& args) {
    auto file = args.value("file", "");
    auto line = args.value("line", 0u);

    auto effects = model_.query_effects(file, line);
    nlohmann::json result;
    result["file"] = file;
    result["line"] = line;
    if (effects) {
        result["effects"] = effects->required_effects;
        result["call_chain"] = effects->call_chain;
    } else {
        result["effects"] = nlohmann::json::array();
        result["call_chain"] = nlohmann::json::array();
    }
    return result;
}

nlohmann::json McpChannel::handle_query_type(const nlohmann::json& args) {
    auto file = args.value("file", "");
    auto symbol = args.value("symbol", "");

    auto type_info = model_.query_type(file, symbol);
    nlohmann::json result;
    result["file"] = file;
    result["symbol"] = symbol;
    if (type_info) {
        result["type"] = type_info->qualified_type;
        result["type_params"] = type_info->type_params;
    } else {
        result["type"] = nullptr;
    }
    return result;
}

nlohmann::json McpChannel::handle_query_ownership(const nlohmann::json& args) {
    auto file = args.value("file", "");
    auto symbol = args.value("symbol", "");

    auto ownership = model_.query_ownership(file, symbol);
    nlohmann::json result;
    result["file"] = file;
    result["symbol"] = symbol;
    if (ownership) {
        switch (ownership->kind) {
            case OwnershipKind::Own:  result["ownership"] = "Own"; break;
            case OwnershipKind::Link: result["ownership"] = "Link"; break;
            default:                  result["ownership"] = "Unknown"; break;
        }
        switch (ownership->state) {
            case LifecycleState::Valid:              result["state"] = "Valid"; break;
            case LifecycleState::PotentiallyDangling: result["state"] = "PotentiallyDangling"; break;
            case LifecycleState::Moved:              result["state"] = "Moved"; break;
        }
        result["scope"] = ownership->scope;
    } else {
        result["ownership"] = nullptr;
    }
    return result;
}

nlohmann::json McpChannel::handle_get_diagnostics(const nlohmann::json& args) {
    auto file = args.value("file", "");
    nlohmann::json result;
    result["file"] = file;

    std::vector<Diagnostic> diags;
    if (file.empty()) {
        diags = model_.get_all_diagnostics();
    } else {
        diags = model_.get_diagnostics(file);
    }

    nlohmann::json diag_array = nlohmann::json::array();
    for (const auto& d : diags) {
        auto dv = DualVoiceFormatter::format_diagnostic(d);
        diag_array.push_back(DualVoiceFormatter::to_mcp_json(dv));
    }
    result["diagnostics"] = diag_array;
    result["count"] = diag_array.size();
    return result;
}

nlohmann::json McpChannel::handle_structural_diff(const nlohmann::json& args) {
    // Accepts: file (path) or uri (file:// URI), optional vfs_session_id
    // Returns: structural diff between on-disk AST and VFS overlay (or two file versions)
    auto file = args.value("file", "");
    if (file.empty()) {
        // MCP compatibility: also accept "uri" parameter
        auto uri = args.value("uri", "");
        if (!uri.empty()) {
            file = uri.substr(uri.find("file://") == 0 ? 7 : 0);
        }
    }
    auto session_id = args.value("vfs_session_id", "");

    nlohmann::json result;
    result["file"] = file;

    auto current_ast = model_.get_ast(file);
    if (!current_ast) {
        result["error"] = "File not indexed: " + file;
        result["changes"] = nlohmann::json::array();
        return result;
    }

    // When a VFS session is provided, compare the VFS overlay against on-disk.
    // When no session is provided, return an empty diff (no changes).
    // Full VFS integration will be wired when the VFS subsystem is complete.
    if (session_id.empty()) {
        result["changes"] = nlohmann::json::array();
        result["summary"] = "No VFS session — on-disk state is current";
    } else {
        // Placeholder: VFS overlay comparison.
        // In production, this retrieves the VFS overlay AST for the session,
        // calls meld::structural_diff(on_disk_ast, vfs_ast), and returns
        // the resulting AST_Transform objects as JSON.
        result["changes"] = nlohmann::json::array();
        result["summary"] = "VFS session " + session_id + " — diff pending VFS integration";
        result["vfs_session_id"] = session_id;
    }

    return result;
}

nlohmann::json McpChannel::handle_meld_eval(const nlohmann::json& args) {
    auto code = args.value("code", "");
    if (code.empty()) return {{"error", "code argument is required"}};

    // Parse and evaluate
    meld::parser::Parser parser;
    std::vector<meld::parser::ast::expression> ast;
    if (!parser.parse_file(code, ast) || ast.empty()) {
        return {{"ok", false}, {"error", "Parse error: " + parser.error_message()}};
    }

    // Capture stdout
    std::ostringstream captured;
    auto* old_buf = std::cout.rdbuf(captured.rdbuf());
    std::string error;
    try {
        meld::interpreter::AstInterpreter interp;
        interp.set_source_file("<mcp-eval>");
        interp.evaluate_program(ast);
    } catch (const std::exception& e) {
        error = e.what();
    }
    std::cout.rdbuf(old_buf);

    nlohmann::json result;
    result["ok"] = error.empty();
    result["output"] = captured.str();
    if (!error.empty()) result["error"] = error;
    return result;
}

nlohmann::json McpChannel::handle_meld_check(const nlohmann::json& args) {
    auto code = args.value("code", "");
    auto file = args.value("file", "<mcp-check>");
    if (code.empty() && !file.empty() && file != "<mcp-check>") {
        std::ifstream f(file);
        if (f) code = std::string(std::istreambuf_iterator<char>(f), {});
    }
    if (code.empty()) return {{"error", "code or file argument is required"}};

    // Parse and check for errors
    meld::parser::Parser parser;
    std::vector<meld::parser::ast::expression> ast;
    nlohmann::json result;
    result["file"] = file;

    if (!parser.parse_file(code, ast)) {
        result["ok"] = false;
        result["diagnostics"] = nlohmann::json::array({
            {{"severity", "error"}, {"code", "E002"}, {"message", parser.error_message()}}
        });
    } else {
        result["ok"] = true;
        result["diagnostics"] = nlohmann::json::array();
        result["ast_nodes"] = ast.size();
    }
    return result;
}

nlohmann::json McpChannel::handle_search_api(const nlohmann::json& args) {
    auto query = args.value("query", "");
    auto scope = args.value("scope", "");
    auto max_results = args.value("max_results", 10);
    if (query.empty()) return {{"error", "query argument is required"}};

    // Use get_diagnostics on all files to find symbols (basic implementation)
    // In production this would use VectorIndex::find_by_intent
    auto all_diags = model_.get_all_diagnostics();
    nlohmann::json arr = nlohmann::json::array();
    // Return empty results with the query echoed — full implementation
    // requires VectorIndex integration
    return {{"query", query}, {"scope", scope}, {"results", arr}, {"count", 0},
            {"note", "Full semantic search requires VectorIndex. Use get_diagnostics for file-level analysis."}};
}

nlohmann::json McpChannel::handle_execute_script(const nlohmann::json& args) {
    auto file = args.value("file", "");
    if (file.empty()) return {{"error", "file argument is required"}};

    // Read file
    std::ifstream f(file);
    if (!f) return {{"ok", false}, {"error", "File not found: " + file}};
    std::string code(std::istreambuf_iterator<char>(f), {});

    // Parse
    meld::parser::Parser parser;
    std::vector<meld::parser::ast::expression> ast;
    if (!parser.parse_file(code, ast) || ast.empty()) {
        return {{"ok", false}, {"error", "Parse error: " + parser.error_message()}};
    }

    // Execute with captured output
    std::ostringstream captured;
    auto* old_buf = std::cout.rdbuf(captured.rdbuf());
    std::string error;
    try {
        meld::interpreter::AstInterpreter interp;
        interp.set_source_file(file);
        interp.evaluate_program(ast);
    } catch (const std::exception& e) {
        error = e.what();
    }
    std::cout.rdbuf(old_buf);

    nlohmann::json result;
    result["ok"] = error.empty();
    result["file"] = file;
    result["output"] = captured.str();
    if (!error.empty()) result["error"] = error;
    return result;
}

}  // namespace meld::daemon
