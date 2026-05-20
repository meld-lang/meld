#pragma once

#include "command_handler.hpp"
#include "error_handler.hpp"
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <filesystem>
#include <expected>
#include <optional>
#include <chrono>

namespace meld::cli {

/**
 * MCP communication modes
 */
enum class McpCommunicationMode {
    Stdio,      // Standard input/output
    Port        // TCP port binding
};

/**
 * MCP server configuration
 */
struct McpConfig {
    McpCommunicationMode mode = McpCommunicationMode::Stdio;
    std::optional<int> port;
    std::string host = "localhost";
    std::filesystem::path workspace_root;
    bool enable_code_analysis = true;
    bool enable_code_generation = true;
    std::vector<std::string> allowed_tools;
    std::map<std::string, std::string> tool_config;
    std::chrono::seconds timeout{30};
};

/**
 * MCP request/response structures
 */
struct McpRequest {
    std::string id;
    std::string method;
    std::map<std::string, std::string> params;
    std::string jsonrpc = "2.0";
};

struct McpResponse {
    std::string id;
    std::map<std::string, std::string> result;
    std::optional<std::string> error;
    std::string jsonrpc = "2.0";
    
    std::string to_json() const;
    static std::expected<McpResponse, std::string> from_json(const std::string& json);
};

/**
 * MCP tool execution result
 */
struct McpToolResult {
    bool success = false;
    std::map<std::string, std::string> data;
    std::string error_message;
    std::chrono::milliseconds execution_time{0};
};

/**
 * Abstract base class for MCP tools
 */
class McpTool {
public:
    virtual ~McpTool() = default;
    
    /**
     * Get tool name
     */
    virtual std::string name() const = 0;
    
    /**
     * Get tool description
     */
    virtual std::string description() const = 0;
    
    /**
     * Get tool JSON schema for parameters
     */
    virtual std::string schema() const = 0;
    
    /**
     * Execute the tool with given parameters
     */
    virtual McpToolResult execute(const std::map<std::string, std::string>& params) = 0;
    
    /**
     * Check if tool is available (dependencies, permissions, etc.)
     */
    virtual bool is_available() const { return true; }
    
    /**
     * Get required permissions for this tool
     */
    virtual std::vector<std::string> required_permissions() const { return {}; }
};

/**
 * Code analysis MCP tool
 */
class CodeAnalysisTool : public McpTool {
public:
    explicit CodeAnalysisTool(const std::filesystem::path& workspace_root);
    
    std::string name() const override { return "analyze_code"; }
    std::string description() const override;
    std::string schema() const override;
    McpToolResult execute(const std::map<std::string, std::string>& params) override;
    
private:
    std::filesystem::path workspace_root_;
    
    std::string analyze_file(const std::filesystem::path& file_path) const;
    std::string get_file_structure(const std::filesystem::path& path) const;
    std::string get_symbol_information(const std::filesystem::path& file_path) const;
};

/**
 * Code generation MCP tool
 */
class CodeGenerationTool : public McpTool {
public:
    explicit CodeGenerationTool(const std::filesystem::path& workspace_root);
    
    std::string name() const override { return "generate_code"; }
    std::string description() const override;
    std::string schema() const override;
    McpToolResult execute(const std::map<std::string, std::string>& params) override;
    
private:
    std::filesystem::path workspace_root_;
    
    std::string generate_meld_code(const std::string& specification) const;
    bool validate_generated_code(const std::string& code) const;
    std::string format_code(const std::string& code) const;
};

/**
 * Project structure MCP tool
 */
class ProjectStructureTool : public McpTool {
public:
    explicit ProjectStructureTool(const std::filesystem::path& workspace_root);
    
    std::string name() const override { return "project_structure"; }
    std::string description() const override;
    std::string schema() const override;
    McpToolResult execute(const std::map<std::string, std::string>& params) override;
    
private:
    std::filesystem::path workspace_root_;
    
    std::string get_project_tree(const std::filesystem::path& root, int max_depth = 5) const;
    std::string get_file_info(const std::filesystem::path& file_path) const;
    std::vector<std::string> find_files_by_pattern(const std::string& pattern) const;
};

/**
 * MCP server implementation
 */
class McpServer {
public:
    explicit McpServer(const McpConfig& config);
    ~McpServer();
    
    /**
     * Start the MCP server
     */
    std::expected<void, std::string> start();
    
    /**
     * Stop the MCP server
     */
    void stop();
    
    /**
     * Check if server is running
     */
    bool is_running() const { return running_.load(); }
    
    /**
     * Register an MCP tool
     */
    void register_tool(std::unique_ptr<McpTool> tool);
    
    /**
     * Get registered tools
     */
    std::vector<std::string> get_registered_tools() const;
    
    /**
     * Handle MCP request
     */
    McpResponse handle_request(const McpRequest& request);
    
    /**
     * Get server statistics
     */
    std::map<std::string, std::string> get_statistics() const;

private:
    McpConfig config_;
    std::atomic<bool> running_{false};
    std::atomic<bool> should_stop_{false};
    std::thread server_thread_;
    std::map<std::string, std::unique_ptr<McpTool>> tools_;
    mutable std::mutex tools_mutex_;
    
    // Statistics
    std::atomic<size_t> requests_handled_{0};
    std::atomic<size_t> errors_count_{0};
    std::chrono::steady_clock::time_point start_time_;
    
    // Server implementation methods
    void run_stdio_server();
    void run_port_server();
    void handle_client_connection(int client_socket);
    std::string read_message_stdio();
    void write_message_stdio(const std::string& message);
    std::expected<McpRequest, std::string> parse_request(const std::string& message);
    McpResponse create_error_response(const std::string& id, const std::string& error);
    McpResponse handle_list_tools_request(const McpRequest& request);
    McpResponse handle_tool_execution_request(const McpRequest& request);
    McpResponse handle_capabilities_request(const McpRequest& request);
    void log_request(const McpRequest& request);
    void log_response(const McpResponse& response);
};

/**
 * MCP server module - handles MCP server functionality
 */
class McpServerModule : public BaseCommandHandler {
public:
    McpServerModule();
    ~McpServerModule() override;
    
    // CommandHandler interface
    CommandResult execute(const CommandArgs& args) override;
    std::string get_help() const override;
    std::string get_usage() const override;
    std::vector<std::string> get_completions(const std::string& partial) const override;
    bool validate_args(const CommandArgs& args, std::string& error_message) const override;
    
    /**
     * Start MCP server with given configuration
     */
    std::expected<void, std::string> start_server(const McpConfig& config);
    
    /**
     * Stop the running MCP server
     */
    void stop_server();
    
    /**
     * Check if server is running
     */
    bool is_server_running() const;
    
    /**
     * Get server status information
     */
    std::map<std::string, std::string> get_server_status() const;

private:
    std::unique_ptr<McpServer> server_;
    mutable std::mutex server_mutex_;
    
    // Helper methods
    McpConfig parse_mcp_config(const CommandArgs& args) const;
    void print_server_status() const;
    void print_available_tools() const;
    std::expected<int, std::string> parse_port(const std::string& port_str) const;
    std::filesystem::path get_workspace_root(const CommandArgs& args) const;
    void setup_default_tools(McpServer& server, const std::filesystem::path& workspace_root);
    void handle_server_signals();
};

} // namespace meld::cli