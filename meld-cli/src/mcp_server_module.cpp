#include "meld/cli/mcp_server_module.hpp"
#include "meld/cli/daemon_bridge.hpp"
#include "meld/cli/error_handler.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include <regex>
#include <algorithm>
#include <random>
#include <iomanip>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

namespace meld::cli {

// ============================================================================
// McpResponse Implementation
// ============================================================================

std::string McpResponse::to_json() const {
    std::ostringstream oss;
    oss << "{";
    oss << "\"jsonrpc\":\"" << jsonrpc << "\",";
    oss << "\"id\":\"" << id << "\"";
    
    if (error) {
        oss << ",\"error\":{\"code\":-1,\"message\":\"" << *error << "\"}";
    } else {
        oss << ",\"result\":{";
        bool first = true;
        for (const auto& [key, value] : result) {
            if (!first) oss << ",";
            oss << "\"" << key << "\":\"" << value << "\"";
            first = false;
        }
        oss << "}";
    }
    
    oss << "}";
    return oss.str();
}

std::expected<McpResponse, std::string> McpResponse::from_json(const std::string& json) {
    // Simple JSON parsing for MCP responses
    // In a production implementation, use a proper JSON library
    McpResponse response;
    
    // Extract id
    std::regex id_regex(R"RE("id"\s*:\s*"([^"]*)")RE");
    std::smatch match;
    if (std::regex_search(json, match, id_regex)) {
        response.id = match[1].str();
    }
    
    // Check for error
    std::regex error_regex(R"RE("error"\s*:\s*\{[^}]*"message"\s*:\s*"([^"]*)")RE");
    if (std::regex_search(json, match, error_regex)) {
        response.error = match[1].str();
        return response;
    }
    
    // Extract result fields
    std::regex result_regex(R"RE("result"\s*:\s*\{([^}]*)\})RE");
    if (std::regex_search(json, match, result_regex)) {
        std::string result_content = match[1].str();
        std::regex field_regex(R"RE("([^"]+)"\s*:\s*"([^"]*)")RE");
        std::sregex_iterator iter(result_content.begin(), result_content.end(), field_regex);
        std::sregex_iterator end;
        
        for (; iter != end; ++iter) {
            response.result[(*iter)[1].str()] = (*iter)[2].str();
        }
    }
    
    return response;
}

// ============================================================================
// CodeAnalysisTool Implementation
// ============================================================================

CodeAnalysisTool::CodeAnalysisTool(const std::filesystem::path& workspace_root)
    : workspace_root_(workspace_root) {}

std::string CodeAnalysisTool::description() const {
    return "Analyze Meld source code files for structure, symbols, and patterns";
}

std::string CodeAnalysisTool::schema() const {
    return R"JSON({
        "type": "object",
        "properties": {
            "file_path": {
                "type": "string",
                "description": "Path to the file to analyze (relative to workspace root)"
            },
            "analysis_type": {
                "type": "string",
                "enum": ["structure", "symbols", "full"],
                "description": "Type of analysis to perform",
                "default": "full"
            }
        },
        "required": ["file_path"]
    })JSON";
}

McpToolResult CodeAnalysisTool::execute(const std::map<std::string, std::string>& params) {
    auto start_time = std::chrono::steady_clock::now();
    McpToolResult result;
    
    auto file_path_it = params.find("file_path");
    if (file_path_it == params.end()) {
        result.error_message = "Missing required parameter: file_path";
        return result;
    }
    
    std::filesystem::path full_path = workspace_root_ / file_path_it->second;
    
    if (!std::filesystem::exists(full_path)) {
        result.error_message = "File does not exist: " + full_path.string();
        return result;
    }
    
    try {
        std::string analysis_type = "full";
        auto type_it = params.find("analysis_type");
        if (type_it != params.end()) {
            analysis_type = type_it->second;
        }
        
        result.data["file_path"] = full_path.string();
        result.data["analysis_type"] = analysis_type;
        
        if (analysis_type == "structure" || analysis_type == "full") {
            result.data["structure"] = get_file_structure(full_path);
        }
        
        if (analysis_type == "symbols" || analysis_type == "full") {
            result.data["symbols"] = get_symbol_information(full_path);
        }
        
        if (analysis_type == "full") {
            result.data["analysis"] = analyze_file(full_path);
        }
        
        result.success = true;
        
    } catch (const std::exception& e) {
        result.error_message = "Analysis failed: " + std::string(e.what());
    }
    
    auto end_time = std::chrono::steady_clock::now();
    result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    return result;
}

std::string CodeAnalysisTool::analyze_file(const std::filesystem::path& file_path) const {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        return "Error: Could not open file";
    }
    
    std::ostringstream analysis;
    std::string line;
    int line_count = 0;
    int function_count = 0;
    int class_count = 0;
    int comment_lines = 0;
    
    while (std::getline(file, line)) {
        line_count++;
        
        // Count functions
        if (line.find("fnc ") != std::string::npos || line.find("function ") != std::string::npos) {
            function_count++;
        }
        
        // Count classes/structs
        if (line.find("class ") != std::string::npos || line.find("struct ") != std::string::npos) {
            class_count++;
        }
        
        // Count comments
        if (line.find("//") != std::string::npos || line.find("/*") != std::string::npos) {
            comment_lines++;
        }
    }
    
    analysis << "Lines: " << line_count << ", ";
    analysis << "Functions: " << function_count << ", ";
    analysis << "Classes: " << class_count << ", ";
    analysis << "Comments: " << comment_lines;
    
    return analysis.str();
}

std::string CodeAnalysisTool::get_file_structure(const std::filesystem::path& path) const {
    std::ifstream file(path);
    if (!file.is_open()) {
        return "Error: Could not open file";
    }
    
    std::ostringstream structure;
    std::string line;
    int indent_level = 0;
    
    while (std::getline(file, line)) {
        // Simple structure detection based on braces and keywords
        if (line.find("class ") != std::string::npos || 
            line.find("struct ") != std::string::npos ||
            line.find("fnc ") != std::string::npos) {
            structure << std::string(indent_level * 2, ' ') << line << "\n";
        }
        
        // Track indentation
        if (line.find("{") != std::string::npos) indent_level++;
        if (line.find("}") != std::string::npos && indent_level > 0) indent_level--;
    }
    
    return structure.str();
}

std::string CodeAnalysisTool::get_symbol_information(const std::filesystem::path& file_path) const {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        return "Error: Could not open file";
    }
    
    std::ostringstream symbols;
    std::string line;
    std::vector<std::string> found_symbols;
    
    while (std::getline(file, line)) {
        // Extract function names
        std::regex func_regex(R"(fnc\s+(\w+)\s*\()");
        std::smatch match;
        if (std::regex_search(line, match, func_regex)) {
            found_symbols.push_back("function:" + match[1].str());
        }
        
        // Extract class names
        std::regex class_regex(R"(class\s+(\w+))");
        if (std::regex_search(line, match, class_regex)) {
            found_symbols.push_back("class:" + match[1].str());
        }
        
        // Extract variable declarations
        std::regex var_regex(R"(val\s+(\w+)\s*[:=])");
        if (std::regex_search(line, match, var_regex)) {
            found_symbols.push_back("variable:" + match[1].str());
        }
    }
    
    for (const auto& symbol : found_symbols) {
        symbols << symbol << "\n";
    }
    
    return symbols.str();
}

// ============================================================================
// CodeGenerationTool Implementation
// ============================================================================

CodeGenerationTool::CodeGenerationTool(const std::filesystem::path& workspace_root)
    : workspace_root_(workspace_root) {}

std::string CodeGenerationTool::description() const {
    return "Generate Meld source code based on specifications and patterns";
}

std::string CodeGenerationTool::schema() const {
    return R"JSON({
        "type": "object",
        "properties": {
            "specification": {
                "type": "string",
                "description": "Natural language specification of what to generate"
            },
            "output_file": {
                "type": "string",
                "description": "Output file path (relative to workspace root)"
            },
            "template": {
                "type": "string",
                "enum": ["function", "class", "module", "test"],
                "description": "Code template to use",
                "default": "function"
            }
        },
        "required": ["specification"]
    })JSON";
}

McpToolResult CodeGenerationTool::execute(const std::map<std::string, std::string>& params) {
    auto start_time = std::chrono::steady_clock::now();
    McpToolResult result;
    
    auto spec_it = params.find("specification");
    if (spec_it == params.end()) {
        result.error_message = "Missing required parameter: specification";
        return result;
    }
    
    try {
        std::string template_type = "function";
        auto template_it = params.find("template");
        if (template_it != params.end()) {
            template_type = template_it->second;
        }
        
        std::string generated_code = generate_meld_code(spec_it->second);
        
        if (!validate_generated_code(generated_code)) {
            result.error_message = "Generated code failed validation";
            return result;
        }
        
        std::string formatted_code = format_code(generated_code);
        
        result.data["specification"] = spec_it->second;
        result.data["template"] = template_type;
        result.data["generated_code"] = formatted_code;
        result.data["validation_status"] = "passed";
        
        // Optionally write to file
        auto output_it = params.find("output_file");
        if (output_it != params.end()) {
            std::filesystem::path output_path = workspace_root_ / output_it->second;
            std::ofstream output_file(output_path);
            if (output_file.is_open()) {
                output_file << formatted_code;
                result.data["output_file"] = output_path.string();
            } else {
                result.data["warning"] = "Could not write to output file: " + output_path.string();
            }
        }
        
        result.success = true;
        
    } catch (const std::exception& e) {
        result.error_message = "Code generation failed: " + std::string(e.what());
    }
    
    auto end_time = std::chrono::steady_clock::now();
    result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    return result;
}

std::string CodeGenerationTool::generate_meld_code(const std::string& specification) const {
    // Simple code generation based on specification keywords
    std::ostringstream code;
    
    if (specification.find("function") != std::string::npos) {
        code << "fnc generated_function() -> Unit {\n";
        code << "    // Generated based on: " << specification << "\n";
        code << "    // TODO: Implement function logic\n";
        code << "}\n";
    } else if (specification.find("class") != std::string::npos) {
        code << "class GeneratedClass {\n";
        code << "    // Generated based on: " << specification << "\n";
        code << "    // TODO: Add class members and methods\n";
        code << "}\n";
    } else {
        code << "// Generated code based on: " << specification << "\n";
        code << "// TODO: Implement the specified functionality\n";
    }
    
    return code.str();
}

bool CodeGenerationTool::validate_generated_code(const std::string& code) const {
    // Basic validation - check for balanced braces
    int brace_count = 0;
    for (char c : code) {
        if (c == '{') brace_count++;
        if (c == '}') brace_count--;
    }
    return brace_count == 0;
}

std::string CodeGenerationTool::format_code(const std::string& code) const {
    // Simple formatting - ensure proper indentation
    std::istringstream iss(code);
    std::ostringstream formatted;
    std::string line;
    int indent_level = 0;
    
    while (std::getline(iss, line)) {
        // Decrease indent for closing braces
        if (line.find("}") != std::string::npos && indent_level > 0) {
            indent_level--;
        }
        
        // Add indentation
        formatted << std::string(indent_level * 4, ' ') << line << "\n";
        
        // Increase indent for opening braces
        if (line.find("{") != std::string::npos) {
            indent_level++;
        }
    }
    
    return formatted.str();
}

// ============================================================================
// ProjectStructureTool Implementation
// ============================================================================

ProjectStructureTool::ProjectStructureTool(const std::filesystem::path& workspace_root)
    : workspace_root_(workspace_root) {}

std::string ProjectStructureTool::description() const {
    return "Explore and analyze project structure and file organization";
}

std::string ProjectStructureTool::schema() const {
    return R"JSON({
        "type": "object",
        "properties": {
            "action": {
                "type": "string",
                "enum": ["tree", "file_info", "find_files"],
                "description": "Action to perform",
                "default": "tree"
            },
            "path": {
                "type": "string",
                "description": "Path to analyze (relative to workspace root)",
                "default": "."
            },
            "pattern": {
                "type": "string",
                "description": "File pattern for find_files action (e.g., '*.meld')"
            },
            "max_depth": {
                "type": "integer",
                "description": "Maximum depth for tree traversal",
                "default": 5
            }
        }
    })JSON";
}

McpToolResult ProjectStructureTool::execute(const std::map<std::string, std::string>& params) {
    auto start_time = std::chrono::steady_clock::now();
    McpToolResult result;
    
    try {
        std::string action = "tree";
        auto action_it = params.find("action");
        if (action_it != params.end()) {
            action = action_it->second;
        }
        
        std::string path = ".";
        auto path_it = params.find("path");
        if (path_it != params.end()) {
            path = path_it->second;
        }
        
        std::filesystem::path full_path = workspace_root_ / path;
        
        result.data["action"] = action;
        result.data["path"] = full_path.string();
        
        if (action == "tree") {
            int max_depth = 5;
            auto depth_it = params.find("max_depth");
            if (depth_it != params.end()) {
                max_depth = std::stoi(depth_it->second);
            }
            result.data["tree"] = get_project_tree(full_path, max_depth);
            
        } else if (action == "file_info") {
            if (!std::filesystem::exists(full_path)) {
                result.error_message = "Path does not exist: " + full_path.string();
                return result;
            }
            result.data["file_info"] = get_file_info(full_path);
            
        } else if (action == "find_files") {
            auto pattern_it = params.find("pattern");
            if (pattern_it == params.end()) {
                result.error_message = "Missing required parameter for find_files: pattern";
                return result;
            }
            
            auto files = find_files_by_pattern(pattern_it->second);
            std::ostringstream files_str;
            for (const auto& file : files) {
                files_str << file << "\n";
            }
            result.data["files"] = files_str.str();
            result.data["count"] = std::to_string(files.size());
        }
        
        result.success = true;
        
    } catch (const std::exception& e) {
        result.error_message = "Project structure analysis failed: " + std::string(e.what());
    }
    
    auto end_time = std::chrono::steady_clock::now();
    result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    return result;
}

std::string ProjectStructureTool::get_project_tree(const std::filesystem::path& root, int max_depth) const {
    std::ostringstream tree;
    
    std::function<void(const std::filesystem::path&, int, const std::string&)> traverse = 
        [&](const std::filesystem::path& path, int depth, const std::string& prefix) {
            if (depth > max_depth) return;
            
            try {
                for (auto it = std::filesystem::directory_iterator(path); 
                     it != std::filesystem::directory_iterator(); ++it) {
                    
                    const auto& entry = *it;
                    bool is_last = (std::next(it) == std::filesystem::directory_iterator());
                    
                    tree << prefix << (is_last ? "└── " : "├── ") << entry.path().filename().string();
                    
                    if (entry.is_directory()) {
                        tree << "/\n";
                        std::string new_prefix = prefix + (is_last ? "    " : "│   ");
                        traverse(entry.path(), depth + 1, new_prefix);
                    } else {
                        tree << "\n";
                    }
                }
            } catch (const std::filesystem::filesystem_error&) {
                // Skip directories we can't read
            }
        };
    
    tree << root.filename().string() << "/\n";
    traverse(root, 0, "");
    
    return tree.str();
}

std::string ProjectStructureTool::get_file_info(const std::filesystem::path& file_path) const {
    std::ostringstream info;
    
    try {
        auto status = std::filesystem::status(file_path);
        auto size = std::filesystem::file_size(file_path);
        auto time = std::filesystem::last_write_time(file_path);
        
        info << "Path: " << file_path.string() << "\n";
        info << "Type: " << (std::filesystem::is_directory(status) ? "Directory" : "File") << "\n";
        info << "Size: " << size << " bytes\n";
        info << "Extension: " << file_path.extension().string() << "\n";
        
        // Convert file time to string (simplified)
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            time - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
        auto time_t = std::chrono::system_clock::to_time_t(sctp);
        info << "Modified: " << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << "\n";
        
    } catch (const std::exception& e) {
        info << "Error getting file info: " << e.what();
    }
    
    return info.str();
}

std::vector<std::string> ProjectStructureTool::find_files_by_pattern(const std::string& pattern) const {
    std::vector<std::string> matching_files;
    
    try {
        std::regex pattern_regex(pattern);
        
        for (const auto& entry : std::filesystem::recursive_directory_iterator(workspace_root_)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                if (std::regex_match(filename, pattern_regex)) {
                    auto relative_path = std::filesystem::relative(entry.path(), workspace_root_);
                    matching_files.push_back(relative_path.string());
                }
            }
        }
    } catch (const std::exception&) {
        // If regex fails, fall back to simple wildcard matching
        for (const auto& entry : std::filesystem::recursive_directory_iterator(workspace_root_)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                // Simple wildcard matching for *.ext patterns
                if (pattern.starts_with("*.") && filename.ends_with(pattern.substr(1))) {
                    auto relative_path = std::filesystem::relative(entry.path(), workspace_root_);
                    matching_files.push_back(relative_path.string());
                }
            }
        }
    }
    
    return matching_files;
}

// ============================================================================
// McpServer Implementation
// ============================================================================

McpServer::McpServer(const McpConfig& config) 
    : config_(config), start_time_(std::chrono::steady_clock::now()) {}

McpServer::~McpServer() {
    stop();
}

std::expected<void, std::string> McpServer::start() {
    if (running_.load()) {
        return std::unexpected("Server is already running");
    }
    
    should_stop_.store(false);
    
    try {
        if (config_.mode == McpCommunicationMode::Stdio) {
            server_thread_ = std::thread(&McpServer::run_stdio_server, this);
        } else {
            server_thread_ = std::thread(&McpServer::run_port_server, this);
        }
        
        running_.store(true);
        start_time_ = std::chrono::steady_clock::now();
        
        return {};
    } catch (const std::exception& e) {
        return std::unexpected("Failed to start server: " + std::string(e.what()));
    }
}

void McpServer::stop() {
    if (!running_.load()) {
        return;
    }
    
    should_stop_.store(true);
    running_.store(false);
    
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
}

void McpServer::register_tool(std::unique_ptr<McpTool> tool) {
    std::lock_guard<std::mutex> lock(tools_mutex_);
    std::string name = tool->name();
    tools_[name] = std::move(tool);
}

std::vector<std::string> McpServer::get_registered_tools() const {
    std::lock_guard<std::mutex> lock(tools_mutex_);
    std::vector<std::string> tool_names;
    for (const auto& [name, tool] : tools_) {
        tool_names.push_back(name);
    }
    return tool_names;
}

McpResponse McpServer::handle_request(const McpRequest& request) {
    requests_handled_.fetch_add(1);
    log_request(request);
    
    McpResponse response;
    response.id = request.id;
    
    try {
        if (request.method == "tools/list") {
            response = handle_list_tools_request(request);
        } else if (request.method == "tools/call") {
            response = handle_tool_execution_request(request);
        } else if (request.method == "initialize") {
            response = handle_capabilities_request(request);
        } else {
            response = create_error_response(request.id, "Unknown method: " + request.method);
        }
    } catch (const std::exception& e) {
        errors_count_.fetch_add(1);
        response = create_error_response(request.id, "Internal error: " + std::string(e.what()));
    }
    
    log_response(response);
    return response;
}

std::map<std::string, std::string> McpServer::get_statistics() const {
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);
    
    return {
        {"running", running_.load() ? "true" : "false"},
        {"requests_handled", std::to_string(requests_handled_.load())},
        {"errors_count", std::to_string(errors_count_.load())},
        {"uptime_seconds", std::to_string(uptime.count())},
        {"registered_tools", std::to_string(tools_.size())},
        {"communication_mode", config_.mode == McpCommunicationMode::Stdio ? "stdio" : "port"}
    };
}

void McpServer::run_stdio_server() {
    while (!should_stop_.load()) {
        try {
            std::string message = read_message_stdio();
            if (message.empty()) continue;
            
            auto request_result = parse_request(message);
            if (!request_result) {
                McpResponse error_response = create_error_response("", request_result.error());
                write_message_stdio(error_response.to_json());
                continue;
            }
            
            McpResponse response = handle_request(request_result.value());
            write_message_stdio(response.to_json());
            
        } catch (const std::exception& e) {
            // Log error but continue running
            std::cerr << "MCP Server error: " << e.what() << std::endl;
        }
    }
}

void McpServer::run_port_server() {
    if (!config_.port) {
        std::cerr << "Port not specified for port mode" << std::endl;
        return;
    }
    
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return;
    }
    
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(*config_.port);
    
    if (bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(server_socket);
        std::cerr << "Failed to bind to port " << *config_.port << std::endl;
        return;
    }
    
    if (listen(server_socket, 5) < 0) {
        close(server_socket);
        std::cerr << "Failed to listen on socket" << std::endl;
        return;
    }
    
    while (!should_stop_.load()) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        
        int client_socket = accept(server_socket, (sockaddr*)&client_addr, &client_len);
        if (client_socket < 0) {
            if (!should_stop_.load()) {
                std::cerr << "Failed to accept client connection" << std::endl;
            }
            continue;
        }
        
        // Handle client in separate thread for concurrent connections
        std::thread client_thread(&McpServer::handle_client_connection, this, client_socket);
        client_thread.detach();
    }
    
    close(server_socket);
}

void McpServer::handle_client_connection(int client_socket) {
    // Set socket to non-blocking for timeout handling
    int flags = fcntl(client_socket, F_GETFL, 0);
    fcntl(client_socket, F_SETFL, flags | O_NONBLOCK);
    
    char buffer[4096];
    std::string message_buffer;
    
    while (!should_stop_.load()) {
        ssize_t bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            message_buffer += buffer;
            
            // Process complete messages (assuming newline-delimited)
            size_t pos = 0;
            while ((pos = message_buffer.find('\n')) != std::string::npos) {
                std::string message = message_buffer.substr(0, pos);
                message_buffer.erase(0, pos + 1);
                
                auto request_result = parse_request(message);
                if (request_result) {
                    McpResponse response = handle_request(request_result.value());
                    std::string response_str = response.to_json() + "\n";
                    send(client_socket, response_str.c_str(), response_str.length(), 0);
                }
            }
        } else if (bytes_read == 0) {
            // Client disconnected
            break;
        } else {
            // No data available, sleep briefly
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    close(client_socket);
}

std::string McpServer::read_message_stdio() {
    std::string line;
    if (std::getline(std::cin, line)) {
        return line;
    }
    return "";
}

void McpServer::write_message_stdio(const std::string& message) {
    std::cout << message << std::endl;
    std::cout.flush();
}

std::expected<McpRequest, std::string> McpServer::parse_request(const std::string& message) {
    McpRequest request;
    
    // Simple JSON parsing - in production, use a proper JSON library
    std::regex id_regex(R"RE("id"\s*:\s*"([^"]*)")RE");
    std::regex method_regex(R"RE("method"\s*:\s*"([^"]*)")RE");
    std::regex params_regex(R"RE("params"\s*:\s*\{([^}]*)\})RE");    
    std::smatch match;
    
    if (std::regex_search(message, match, id_regex)) {
        request.id = match[1].str();
    }
    
    if (std::regex_search(message, match, method_regex)) {
        request.method = match[1].str();
    } else {
        return std::unexpected("Missing method in request");
    }
    
    if (std::regex_search(message, match, params_regex)) {
        std::string params_content = match[1].str();
        std::regex param_regex(R"RE("([^"]+)"\s*:\s*"([^"]*)")RE");
        std::sregex_iterator iter(params_content.begin(), params_content.end(), param_regex);
        std::sregex_iterator end;
        
        for (; iter != end; ++iter) {
            request.params[(*iter)[1].str()] = (*iter)[2].str();
        }
    }
    
    return request;
}

McpResponse McpServer::create_error_response(const std::string& id, const std::string& error) {
    McpResponse response;
    response.id = id;
    response.error = error;
    return response;
}

McpResponse McpServer::handle_list_tools_request(const McpRequest& request) {
    std::lock_guard<std::mutex> lock(tools_mutex_);
    
    McpResponse response;
    response.id = request.id;
    
    std::ostringstream tools_json;
    tools_json << "[";
    bool first = true;
    
    for (const auto& [name, tool] : tools_) {
        if (!first) tools_json << ",";
        tools_json << "{";
        tools_json << "\"name\":\"" << tool->name() << "\",";
        tools_json << "\"description\":\"" << tool->description() << "\",";
        tools_json << "\"schema\":" << tool->schema();
        tools_json << "}";
        first = false;
    }
    
    tools_json << "]";
    response.result["tools"] = tools_json.str();
    
    return response;
}

McpResponse McpServer::handle_tool_execution_request(const McpRequest& request) {
    std::lock_guard<std::mutex> lock(tools_mutex_);
    
    McpResponse response;
    response.id = request.id;
    
    auto tool_name_it = request.params.find("name");
    if (tool_name_it == request.params.end()) {
        response.error = "Missing tool name parameter";
        return response;
    }
    
    auto tool_it = tools_.find(tool_name_it->second);
    if (tool_it == tools_.end()) {
        response.error = "Tool not found: " + tool_name_it->second;
        return response;
    }
    
    // Extract tool parameters (simplified - assumes all params are strings)
    std::map<std::string, std::string> tool_params;
    for (const auto& [key, value] : request.params) {
        if (key != "name") {
            tool_params[key] = value;
        }
    }
    
    McpToolResult result = tool_it->second->execute(tool_params);
    
    if (result.success) {
        for (const auto& [key, value] : result.data) {
            response.result[key] = value;
        }
        response.result["execution_time_ms"] = std::to_string(result.execution_time.count());
    } else {
        response.error = result.error_message;
    }
    
    return response;
}

McpResponse McpServer::handle_capabilities_request(const McpRequest& request) {
    McpResponse response;
    response.id = request.id;
    
    response.result["protocolVersion"] = "2024-11-05";
    response.result["capabilities"] = R"({
        "tools": {
            "listChanged": true
        },
        "resources": {
            "subscribe": false,
            "listChanged": false
        }
    })";
    response.result["serverInfo"] = R"({
        "name": "meld-cli-mcp-server",
        "version": "1.0.0"
    })";
    
    return response;
}

void McpServer::log_request(const McpRequest& request) {
    // Simple logging - in production, use proper logging framework
    std::cerr << "[MCP] Request: " << request.method << " (id: " << request.id << ")" << std::endl;
}

void McpServer::log_response(const McpResponse& response) {
    std::cerr << "[MCP] Response: " << (response.error ? "ERROR" : "OK") 
              << " (id: " << response.id << ")" << std::endl;
}

// ============================================================================
// McpServerModule Implementation
// ============================================================================

McpServerModule::McpServerModule()
    : BaseCommandHandler("mcp", "Manage Model Context Protocol server") {}

McpServerModule::~McpServerModule() {
    stop_server();
}

CommandResult McpServerModule::execute(const CommandArgs& args) {
    // Subcommand may be in args.subcommand or args.positional[0]
    // depending on how the dispatcher parsed it.
    std::string subcmd = args.subcommand;
    if (subcmd.empty() && !args.positional.empty()) {
        subcmd = args.positional[0];
    }

    // Default: no subcommand means start in stdio mode via daemon bridge
    if (subcmd.empty() || subcmd == "start") {
        McpConfig config = parse_mcp_config(args);
        
        // stdio mode (default): bridge to the daemon's MCP socket
        if (config.mode == McpCommunicationMode::Stdio || !config.port) {
            std::filesystem::path workspace;
            auto ws_it = args.options.find("workspace");
            if (ws_it != args.options.end()) {
                workspace = ws_it->second;
            }
            int rc = DaemonBridge::run(DaemonBridge::Channel::MCP, workspace);
            return rc == 0 ? CommandResult::Success : CommandResult::Error;
        }
        
        // TCP mode: use the built-in server (legacy path)
        try {
            auto result = start_server(config);
            if (!result) {
                std::cerr << "Failed to start MCP server: " << result.error() << std::endl;
                return CommandResult::Error;
            }
            std::cout << "MCP server started on port " << *config.port << std::endl;
            print_server_status();
            std::string input;
            std::getline(std::cin, input);
            stop_server();
            return CommandResult::Success;
        } catch (const std::exception& e) {
            std::cerr << "Error starting MCP server: " << e.what() << std::endl;
            return CommandResult::Error;
        }
        
    } else if (subcmd == "stop") {
        stop_server();
        std::cout << "MCP server stopped" << std::endl;
        return CommandResult::Success;
        
    } else if (subcmd == "status") {
        bool json_output = args.flags.count("json") > 0 || args.options.count("json") > 0;
        auto ws = std::filesystem::current_path();
        auto socket_path = ws / ".meld" / "mcp.sock";
        bool socket_exists = std::filesystem::exists(socket_path);

        auto pid_file = ws / ".meld" / "meldd.pid";
        pid_t daemon_pid = 0;
        if (std::filesystem::exists(pid_file)) {
            std::ifstream pf(pid_file);
            pf >> daemon_pid;
        }
        bool pid_alive = daemon_pid > 0 && kill(daemon_pid, 0) == 0;
        bool running = socket_exists && pid_alive;

        if (json_output) {
            std::cout << "{\"running\":" << (running ? "true" : "false")
                      << ",\"socket\":\"" << socket_path.string() << "\""
                      << ",\"pid\":" << daemon_pid
                      << "}" << std::endl;
        } else {
            std::cout << "MCP Server Status:" << std::endl;
            std::cout << "  running: " << (running ? "true" : "false") << std::endl;
            if (daemon_pid > 0) {
                std::cout << "  daemon pid: " << daemon_pid << std::endl;
            }
            std::cout << "  socket: " << socket_path.string() << std::endl;
        }
        return CommandResult::Success;
        
    } else if (subcmd == "tools") {
        print_available_tools();
        return CommandResult::Success;
        
    } else {
        std::cerr << "Unknown MCP subcommand: " << subcmd << std::endl;
        return CommandResult::Error;
    }
}

std::string McpServerModule::get_help() const {
    return R"(MCP Server Module - Manage Model Context Protocol server

USAGE:
    meld mcp <SUBCOMMAND> [OPTIONS]

SUBCOMMANDS:
    start       Start the MCP server
    stop        Stop the running MCP server
    status      Show server status
    tools       List available MCP tools

START OPTIONS:
    --port <PORT>       Bind to TCP port (default: stdio mode)
    --stdio             Use stdio communication (default)
    --workspace <PATH>  Set workspace root directory
    --timeout <SECS>    Set request timeout in seconds (default: 30)

EXAMPLES:
    meld mcp start                    # Start server in stdio mode
    meld mcp start --port 8080        # Start server on port 8080
    meld mcp start --workspace ./src  # Start with custom workspace
    meld mcp status                   # Check server status
    meld mcp tools                    # List available tools
)";
}

std::string McpServerModule::get_usage() const {
    return "meld mcp <start|stop|status|tools> [OPTIONS]";
}

std::vector<std::string> McpServerModule::get_completions(const std::string& partial) const {
    std::vector<std::string> completions = {"start", "stop", "status", "tools"};
    
    std::vector<std::string> matches;
    for (const auto& completion : completions) {
        if (completion.starts_with(partial)) {
            matches.push_back(completion);
        }
    }
    
    return matches;
}

bool McpServerModule::validate_args(const CommandArgs& args, std::string& error_message) const {
    std::string subcmd = args.subcommand;
    if (subcmd.empty() && !args.positional.empty()) {
        subcmd = args.positional[0];
    }

    if (subcmd.empty()) {
        // No subcommand means "start in stdio mode" — valid
        return true;
    }
    
    std::vector<std::string> valid_subcommands = {"start", "stop", "status", "tools"};
    if (std::find(valid_subcommands.begin(), valid_subcommands.end(), subcmd) == valid_subcommands.end()) {
        error_message = "Invalid MCP subcommand: " + subcmd;
        return false;
    }
    
    if (subcmd == "start") {
        // Validate port if specified
        auto port_it = args.options.find("port");
        if (port_it != args.options.end()) {
            auto port_result = parse_port(port_it->second);
            if (!port_result) {
                error_message = port_result.error();
                return false;
            }
        }
    }
    
    return true;
}

std::expected<void, std::string> McpServerModule::start_server(const McpConfig& config) {
    std::lock_guard<std::mutex> lock(server_mutex_);
    
    if (server_ && server_->is_running()) {
        return std::unexpected("MCP server is already running");
    }
    
    server_ = std::make_unique<McpServer>(config);
    
    // Register default tools
    setup_default_tools(*server_, config.workspace_root);
    
    auto result = server_->start();
    if (!result) {
        server_.reset();
        return result;
    }
    
    return {};
}

void McpServerModule::stop_server() {
    std::lock_guard<std::mutex> lock(server_mutex_);
    
    if (server_) {
        server_->stop();
        server_.reset();
    }
}

bool McpServerModule::is_server_running() const {
    std::lock_guard<std::mutex> lock(server_mutex_);
    return server_ && server_->is_running();
}

std::map<std::string, std::string> McpServerModule::get_server_status() const {
    std::lock_guard<std::mutex> lock(server_mutex_);
    
    if (server_) {
        return server_->get_statistics();
    }
    
    return {{"running", "false"}};
}

McpConfig McpServerModule::parse_mcp_config(const CommandArgs& args) const {
    McpConfig config;
    
    // Parse communication mode
    auto port_it = args.options.find("port");
    if (port_it != args.options.end()) {
        config.mode = McpCommunicationMode::Port;
        auto port_result = parse_port(port_it->second);
        if (port_result) {
            config.port = port_result.value();
        }
    } else {
        config.mode = McpCommunicationMode::Stdio;
    }
    
    // Parse workspace root
    config.workspace_root = get_workspace_root(args);
    
    // Parse timeout
    auto timeout_it = args.options.find("timeout");
    if (timeout_it != args.options.end()) {
        try {
            int timeout_secs = std::stoi(timeout_it->second);
            config.timeout = std::chrono::seconds(timeout_secs);
        } catch (const std::exception&) {
            // Use default timeout on parse error
        }
    }
    
    return config;
}

void McpServerModule::print_server_status() const {
    auto status = get_server_status();
    
    std::cout << "MCP Server Status:" << std::endl;
    for (const auto& [key, value] : status) {
        std::cout << "  " << key << ": " << value << std::endl;
    }
}

void McpServerModule::print_available_tools() const {
    std::lock_guard<std::mutex> lock(server_mutex_);
    
    if (!server_) {
        std::cout << "MCP server is not running" << std::endl;
        return;
    }
    
    auto tools = server_->get_registered_tools();
    
    std::cout << "Available MCP Tools:" << std::endl;
    for (const auto& tool_name : tools) {
        std::cout << "  - " << tool_name << std::endl;
    }
    
    if (tools.empty()) {
        std::cout << "  No tools registered" << std::endl;
    }
}

std::expected<int, std::string> McpServerModule::parse_port(const std::string& port_str) const {
    try {
        int port = std::stoi(port_str);
        if (port < 1 || port > 65535) {
            return std::unexpected("Port must be between 1 and 65535");
        }
        return port;
    } catch (const std::exception&) {
        return std::unexpected("Invalid port number: " + port_str);
    }
}

std::filesystem::path McpServerModule::get_workspace_root(const CommandArgs& args) const {
    auto workspace_it = args.options.find("workspace");
    if (workspace_it != args.options.end()) {
        return std::filesystem::path(workspace_it->second);
    }
    
    return std::filesystem::current_path();
}

void McpServerModule::setup_default_tools(McpServer& server, const std::filesystem::path& workspace_root) {
    // Register default MCP tools
    server.register_tool(std::make_unique<CodeAnalysisTool>(workspace_root));
    server.register_tool(std::make_unique<CodeGenerationTool>(workspace_root));
    server.register_tool(std::make_unique<ProjectStructureTool>(workspace_root));
}

void McpServerModule::handle_server_signals() {
    // Set up signal handlers for graceful shutdown
    signal(SIGINT, [](int) {
        std::cout << "\nReceived interrupt signal, stopping MCP server..." << std::endl;
        // The server will be stopped in the main execution loop
    });
    
    signal(SIGTERM, [](int) {
        std::cout << "\nReceived termination signal, stopping MCP server..." << std::endl;
        // The server will be stopped in the main execution loop
    });
}

} // namespace meld::cli