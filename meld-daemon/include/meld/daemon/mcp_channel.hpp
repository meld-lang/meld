#pragma once

#include "meld/daemon/dual_voice.hpp"
#include "meld/daemon/semantic_model.hpp"

#include <nlohmann/json.hpp>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace meld::daemon {

/// MCP tool request
struct McpToolRequest {
    std::string tool_name;
    nlohmann::json arguments;
    std::string request_id;
};

/// MCP tool response
struct McpToolResponse {
    std::string request_id;
    nlohmann::json result;
    bool is_error{false};
};

/// MCP channel adapter — wraps HTTP/SSE transport for AI agent communication.
/// Reads from the shared SemanticModel and formats responses via DualVoiceFormatter.
class McpChannel {
public:
    explicit McpChannel(SemanticModel& model, int port = 3000);
    ~McpChannel();

    /// Start the MCP HTTP/SSE server (blocking)
    void start();

    /// Run MCP over stdin/stdout using JSON-RPC (for Kiro MCP integration)
    void run_stdio(std::istream& in, std::ostream& out);

    /// Run MCP over a raw file descriptor (Unix socket client)
    void run_on_fd(int fd);

    /// Stop the channel
    void stop();

    /// Push a diagnostic event via SSE
    void push_diagnostic_event(const std::filesystem::path& file,
                               const std::vector<Diagnostic>& diags);

    /// Register a custom tool handler
    using ToolHandler = std::function<nlohmann::json(const nlohmann::json&)>;
    void register_tool(const std::string& name, ToolHandler handler);

private:
    /// Handle an incoming tool invocation
    McpToolResponse handle_tool_call(const McpToolRequest& request);

    /// Built-in MCP tool handlers
    nlohmann::json handle_analyze_safety(const nlohmann::json& args);
    nlohmann::json handle_trace_effect(const nlohmann::json& args);
    nlohmann::json handle_query_type(const nlohmann::json& args);
    nlohmann::json handle_query_ownership(const nlohmann::json& args);
    nlohmann::json handle_get_diagnostics(const nlohmann::json& args);
    nlohmann::json handle_structural_diff(const nlohmann::json& args);
    nlohmann::json handle_meld_eval(const nlohmann::json& args);
    nlohmann::json handle_meld_check(const nlohmann::json& args);
    nlohmann::json handle_search_api(const nlohmann::json& args);
    nlohmann::json handle_execute_script(const nlohmann::json& args);

    SemanticModel& model_;
    int port_;
    bool running_{false};
    std::unordered_map<std::string, ToolHandler> tools_;
};

}  // namespace meld::daemon
