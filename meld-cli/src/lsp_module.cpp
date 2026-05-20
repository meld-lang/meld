#include "meld/cli/lsp_module.hpp"
#include "meld/cli/daemon_bridge.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <signal.h>
#include <regex>
#include <thread>
#include <chrono>
#include <cstdlib>

namespace meld::cli {

// LspError implementation
std::string LspError::format() const {
    std::ostringstream oss;
    oss << "LSP Error";
    if (code != 0) {
        oss << " (" << code << ")";
    }
    oss << ": " << message;
    if (!data.empty()) {
        oss << "\nData: " << data;
    }
    return oss.str();
}

// Location implementation
std::string Location::format() const {
    std::ostringstream oss;
    oss << file.string() << ":" << (range.start.line + 1) << ":" << (range.start.column + 1);
    if (range.start != range.end) {
        oss << "-" << (range.end.line + 1) << ":" << (range.end.column + 1);
    }
    return oss.str();
}

// Symbol implementation
std::string Symbol::format() const {
    std::ostringstream oss;
    oss << name << " (" << static_cast<int>(kind) << ") at " << location.format();
    if (!detail.empty()) {
        oss << " - " << detail;
    }
    return oss.str();
}

// Diagnostic implementation
std::string Diagnostic::format() const {
    std::ostringstream oss;
    oss << range.start.line + 1 << ":" << range.start.column + 1;
    
    switch (severity) {
        case DiagnosticSeverity::Error: oss << " error: "; break;
        case DiagnosticSeverity::Warning: oss << " warning: "; break;
        case DiagnosticSeverity::Information: oss << " info: "; break;
        case DiagnosticSeverity::Hint: oss << " hint: "; break;
    }
    
    oss << message;
    if (!code.empty()) {
        oss << " [" << code << "]";
    }
    return oss.str();
}

// CompletionItem implementation
std::string CompletionItem::format() const {
    std::ostringstream oss;
    oss << label;
    if (!detail.empty()) {
        oss << " - " << detail;
    }
    if (deprecated) {
        oss << " (deprecated)";
    }
    return oss.str();
}

// Hover implementation
std::string Hover::format() const {
    return contents;
}

// TextEdit implementation
std::string TextEdit::format() const {
    std::ostringstream oss;
    oss << "Replace " << range.start.line + 1 << ":" << range.start.column + 1
        << "-" << range.end.line + 1 << ":" << range.end.column + 1
        << " with: " << new_text;
    return oss.str();
}

// WorkspaceEdit implementation
size_t WorkspaceEdit::total_edits() const {
    size_t total = 0;
    for (const auto& [file, edits] : changes) {
        total += edits.size();
    }
    return total;
}

std::string WorkspaceEdit::format() const {
    std::ostringstream oss;
    oss << "Workspace edit with " << total_edits() << " changes across " << changes.size() << " files:";
    for (const auto& [file, edits] : changes) {
        oss << "\n  " << file.string() << ": " << edits.size() << " edits";
    }
    return oss.str();
}

// LspServer implementation
LspServer::LspServer(const LspConfig& config) : config_(config) {}

LspServer::~LspServer() {
    if (running_) {
        stop();
    }
}

std::expected<void, LspError> LspServer::start() {
    std::lock_guard<std::mutex> lock(server_mutex_);
    
    if (running_) {
        return std::unexpected(LspError("LSP server is already running"));
    }
    
    try {
        server_thread_ = std::make_unique<std::thread>(&LspServer::run_server, this);
        running_ = true;
        
        // Give the server a moment to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        return {};
    } catch (const std::exception& e) {
        return std::unexpected(LspError("Failed to start LSP server: " + std::string(e.what())));
    }
}

std::expected<void, LspError> LspServer::stop() {
    std::lock_guard<std::mutex> lock(server_mutex_);
    
    if (!running_) {
        return std::unexpected(LspError("LSP server is not running"));
    }
    
    running_ = false;
    
    if (server_thread_ && server_thread_->joinable()) {
        server_thread_->join();
    }
    
    server_thread_.reset();
    process_id_.reset();
    
    return {};
}

void LspServer::run_server() {
    if (config_.stdio) {
        run_stdio_server();
    } else {
        run_tcp_server();
    }
}

void LspServer::run_stdio_server() {
    // Mock LSP server implementation for stdio mode
    // In a real implementation, this would handle LSP protocol over stdin/stdout
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void LspServer::run_tcp_server() {
    // Mock LSP server implementation for TCP mode
    // In a real implementation, this would bind to a port and handle LSP protocol
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// LspClient implementation
LspClient::LspClient() = default;

LspClient::~LspClient() {
    if (connected_) {
        disconnect();
    }
}

std::expected<void, LspError> LspClient::connect(const LspConfig& config) {
    std::lock_guard<std::mutex> lock(client_mutex_);
    
    if (connected_) {
        return std::unexpected(LspError("Client is already connected"));
    }
    
    config_ = config;
    
    // Mock connection - in real implementation would establish connection
    connected_ = true;
    
    return {};
}

void LspClient::disconnect() {
    std::lock_guard<std::mutex> lock(client_mutex_);
    connected_ = false;
}

std::expected<void, LspError> LspClient::initialize(const std::vector<std::filesystem::path>& workspace_folders) {
    if (!connected_) {
        return std::unexpected(LspError("Client is not connected"));
    }
    
    // Mock initialization - in real implementation would send initialize request
    return {};
}

std::expected<void, LspError> LspClient::shutdown() {
    if (!connected_) {
        return std::unexpected(LspError("Client is not connected"));
    }
    
    // Mock shutdown - in real implementation would send shutdown request
    return {};
}

std::expected<std::vector<Symbol>, LspError> LspClient::query_symbols(const std::string& query) {
    if (!connected_) {
        return std::unexpected(LspError("Client is not connected"));
    }
    
    // Mock symbol search - return symbols that match the query
    std::vector<Symbol> symbols;
    
    // Convert query to lowercase for case-insensitive matching
    std::string lower_query = query;
    std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(), ::tolower);
    
    // Mock symbol database
    std::vector<std::pair<std::string, SymbolKind>> mock_symbols = {
        {"main", SymbolKind::Function},
        {"test_function", SymbolKind::Function},
        {"test_method", SymbolKind::Method},
        {"TestClass", SymbolKind::Class},
        {"test_variable", SymbolKind::Variable},
        {"function_test", SymbolKind::Function},
        {"utility_function", SymbolKind::Function},
        {"UtilityStruct", SymbolKind::Struct},
        {"UtilityEnum", SymbolKind::Enum},
        {"variable", SymbolKind::Variable},
        {"method", SymbolKind::Method},
        {"class", SymbolKind::Class},
        {"struct", SymbolKind::Struct},
        {"enum", SymbolKind::Enum},
        {"interface", SymbolKind::Interface},
        {"module", SymbolKind::Module}
    };
    
    // Find matching symbols
    for (const auto& [name, kind] : mock_symbols) {
        std::string lower_name = name;
        std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
        
        // Check if symbol matches query (substring match or empty query returns all)
        if (query.empty() || lower_name.find(lower_query) != std::string::npos) {
            // Create location based on symbol type
            std::string file_path;
            size_t line = 0;
            
            switch (kind) {
                case SymbolKind::Function:
                case SymbolKind::Method:
                    file_path = name.find("test") != std::string::npos ? "tests/test.meld" : "src/main.meld";
                    line = 10;
                    break;
                case SymbolKind::Class:
                case SymbolKind::Struct:
                case SymbolKind::Enum:
                case SymbolKind::Interface:
                    file_path = "src/types.meld";
                    line = 20;
                    break;
                case SymbolKind::Variable:
                    file_path = "src/variables.meld";
                    line = 5;
                    break;
                case SymbolKind::Module:
                    file_path = "src/modules.meld";
                    line = 1;
                    break;
                default:
                    file_path = "src/main.meld";
                    line = 0;
            }
            
            Location location(file_path, line, 0);
            Symbol symbol(name, kind, location);
            symbol.detail = "Mock symbol: " + name;
            symbols.push_back(symbol);
        }
    }
    
    return symbols;
}

std::expected<std::vector<Diagnostic>, LspError> LspClient::get_diagnostics(const std::filesystem::path& file) {
    if (!connected_) {
        return std::unexpected(LspError("Client is not connected"));
    }
    
    // Mock diagnostics - return empty for now
    std::vector<Diagnostic> diagnostics;
    
    // In a real implementation, this would request diagnostics from the LSP server
    
    return diagnostics;
}

std::expected<std::string, LspError> LspClient::format_document(const std::filesystem::path& file) {
    if (!connected_) {
        return std::unexpected(LspError("Client is not connected"));
    }
    
    // Mock formatting - just return the original content
    std::string content = read_file_content(file);
    return content;
}

std::expected<std::vector<CompletionItem>, LspError> 
LspClient::get_completions(const std::filesystem::path& file, const Position& position) {
    if (!connected_) {
        return std::unexpected(LspError("Client is not connected"));
    }
    
    // Mock completions
    std::vector<CompletionItem> completions;
    completions.emplace_back("println", CompletionItemKind::Function);
    completions.back().detail = "fn println(msg: string) -> Unit";
    
    completions.emplace_back("if", CompletionItemKind::Keyword);
    completions.emplace_back("else", CompletionItemKind::Keyword);
    
    return completions;
}

std::expected<Location, LspError> 
LspClient::goto_definition(const std::filesystem::path& file, const Position& position) {
    if (!connected_) {
        return std::unexpected(LspError("Client is not connected"));
    }
    
    // Mock go-to-definition
    return Location("src/definitions.meld", 42, 0);
}

std::expected<std::vector<Location>, LspError> 
LspClient::find_references(const std::filesystem::path& file, const Position& position) {
    if (!connected_) {
        return std::unexpected(LspError("Client is not connected"));
    }
    
    // Mock find references
    std::vector<Location> references;
    references.emplace_back(std::filesystem::path("src/main.meld"), 10, 5);
    references.emplace_back(std::filesystem::path("src/utils.meld"), 20, 10);
    
    return references;
}

std::expected<WorkspaceEdit, LspError> 
LspClient::rename_symbol(const std::filesystem::path& file, const Position& position, const std::string& new_name) {
    if (!connected_) {
        return std::unexpected(LspError("Client is not connected"));
    }
    
    // Mock rename
    WorkspaceEdit edit;
    edit.add_edit(file, TextEdit(Range(position, position), new_name));
    
    return edit;
}

std::expected<Hover, LspError> 
LspClient::get_hover_info(const std::filesystem::path& file, const Position& position) {
    if (!connected_) {
        return std::unexpected(LspError("Client is not connected"));
    }
    
    // Mock hover info
    return Hover("Type: string\nDescription: A string value");
}

std::expected<std::string, LspError> LspClient::send_request(const std::string& method, const std::string& params) {
    // Mock request sending
    return "{}";
}

std::expected<void, LspError> LspClient::send_notification(const std::string& method, const std::string& params) {
    // Mock notification sending
    return {};
}

std::string LspClient::read_file_content(const std::filesystem::path& file) {
    std::ifstream ifs(file);
    if (!ifs) {
        return "";
    }
    
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

Position LspClient::parse_position(const std::string& line_col) {
    std::regex pos_regex(R"((\d+):(\d+))");
    std::smatch match;
    
    if (std::regex_match(line_col, match, pos_regex)) {
        size_t line = std::stoull(match[1].str()) - 1; // Convert to 0-based
        size_t col = std::stoull(match[2].str()) - 1;  // Convert to 0-based
        return Position(line, col);
    }
    
    return Position(0, 0);
}

std::string LspClient::format_position(const Position& pos) {
    return std::to_string(pos.line + 1) + ":" + std::to_string(pos.column + 1);
}

std::string LspClient::format_range(const Range& range) {
    return format_position(range.start) + "-" + format_position(range.end);
}

// LspModule implementation
LspModule::LspModule() 
    : BaseCommandHandler("lsp", "Language Server Protocol integration for code intelligence") {
    client_ = std::make_unique<LspClient>();
}

CommandResult LspModule::execute(const CommandArgs& args) {
    // Subcommand may be in args.subcommand or args.positional[0]
    std::string subcmd = args.subcommand;
    if (subcmd.empty() && !args.positional.empty()) {
        subcmd = args.positional[0];
    }

    // Default: no subcommand means start in stdio mode via daemon bridge
    if (subcmd.empty() || subcmd == "start") {
        return handle_start_command(args);
    }
    
    try {
        if (subcmd == "stop") {
            return handle_stop_command(args);
        } else if (subcmd == "status") {
            return handle_status_command(args);
        } else if (subcmd == "symbols") {
            return handle_symbols_command(args);
        } else if (subcmd == "diagnostics") {
            return handle_diagnostics_command(args);
        } else if (subcmd == "format") {
            return handle_format_command(args);
        } else if (subcmd == "completions") {
            return handle_completions_command(args);
        } else if (subcmd == "definition") {
            return handle_definition_command(args);
        } else if (subcmd == "references") {
            return handle_references_command(args);
        } else if (subcmd == "rename") {
            return handle_rename_command(args);
        } else if (subcmd == "hover") {
            return handle_hover_command(args);
        } else {
            std::cerr << "Unknown LSP subcommand: " << subcmd << std::endl;
            return CommandResult::NotFound;
        }
    } catch (const std::exception& e) {
        std::cerr << "LSP command failed: " << e.what() << std::endl;
        return CommandResult::Error;
    }
}

std::string LspModule::get_help() const {
    return R"(LSP (Language Server Protocol) Integration

USAGE:
    meld lsp <subcommand> [options]

SUBCOMMANDS:
    start                    Start the LSP server
    stop                     Stop the LSP server
    symbols <query>          Search for symbols matching query
    diagnostics <file>       Get diagnostics for file
    format <file>            Format file using LSP
    completions <file> <line> <column>  Get completions at position
    definition <file> <line> <column>   Go to definition
    references <file> <line> <column>   Find references
    rename <file> <line> <column> <new-name>  Rename symbol
    hover <file> <line> <column>        Get hover information

OPTIONS:
    --port <port>           Port for TCP server (default: stdio)
    --stdio                 Use stdio communication
    --workspace <path>      Workspace root directory
    --trace <level>         Trace level (off, messages, verbose)
    --log-file <file>       Log file path

EXAMPLES:
    meld lsp start --stdio
    meld lsp symbols "main"
    meld lsp diagnostics src/main.meld
    meld lsp completions src/main.meld 10 5
    meld lsp definition src/main.meld 15 10
)";
}

std::string LspModule::get_usage() const {
    return "meld lsp <subcommand> [options]";
}

std::vector<std::string> LspModule::get_completions(const std::string& partial) const {
    std::vector<std::string> completions = {
        "start", "stop", "symbols", "diagnostics", "format",
        "completions", "definition", "references", "rename", "hover"
    };
    
    std::vector<std::string> matches;
    for (const auto& completion : completions) {
        if (completion.find(partial) == 0) {
            matches.push_back(completion);
        }
    }
    
    return matches;
}

bool LspModule::validate_args(const CommandArgs& args, std::string& error_message) const {
    // No subcommand is valid — means "start in stdio mode"
    std::string subcmd = args.subcommand;
    if (subcmd.empty() && !args.positional.empty()) {
        subcmd = args.positional[0];
    }
    
    // Validate position-based commands
    if (subcmd == "completions" || subcmd == "definition" ||
        subcmd == "references" || subcmd == "hover") {
        if (args.positional.size() < 3) {
            error_message = "File, line, and column are required for " + subcmd;
            return false;
        }
    }
    
    if (subcmd == "rename") {
        if (args.positional.size() < 4) {
            error_message = "File, line, column, and new name are required for rename";
            return false;
        }
    }
    
    return true;
}

CommandResult LspModule::handle_start_command(const CommandArgs& args) {
    LspConfig config = parse_lsp_config(args);
    
    // stdio mode: bridge to the daemon's LSP socket
    if (config.stdio || !config.port) {
        std::filesystem::path workspace;
        if (!config.workspace_folders.empty()) {
            workspace = config.workspace_folders[0];
        }
        int rc = DaemonBridge::run(DaemonBridge::Channel::LSP, workspace);
        return rc == 0 ? CommandResult::Success : CommandResult::Error;
    }
    
    // TCP mode: use the built-in server (legacy path)
    auto result = start_server(config);
    if (!result) {
        print_lsp_error(result.error());
        return CommandResult::Error;
    }
    
    std::cout << "LSP server started successfully on port " << *config.port
              << std::endl;
    return CommandResult::Success;
}

CommandResult LspModule::handle_stop_command(const CommandArgs& args) {
    auto result = stop_server();
    if (!result) {
        print_lsp_error(result.error());
        return CommandResult::Error;
    }
    
    std::cout << "LSP server stopped successfully" << std::endl;
    return CommandResult::Success;
}

CommandResult LspModule::handle_status_command(const CommandArgs& args) {
    bool json_output = args.flags.count("json") > 0 || args.options.count("json") > 0;

    // Check if the daemon is running by looking for the LSP socket
    auto ws = std::filesystem::current_path();
    auto socket_path = ws / ".meld" / "lsp.sock";
    bool daemon_running = std::filesystem::exists(socket_path);

    // Also check the PID file
    auto pid_file = ws / ".meld" / "meldd.pid";
    pid_t daemon_pid = 0;
    if (std::filesystem::exists(pid_file)) {
        std::ifstream pf(pid_file);
        pf >> daemon_pid;
    }

    bool pid_alive = daemon_pid > 0 && kill(daemon_pid, 0) == 0;

    if (json_output) {
        std::cout << "{\"running\":" << (daemon_running && pid_alive ? "true" : "false")
                  << ",\"socket\":\"" << socket_path.string() << "\""
                  << ",\"pid\":" << daemon_pid
                  << "}" << std::endl;
    } else {
        std::cout << "LSP Server Status:" << std::endl;
        std::cout << "  running: " << (daemon_running && pid_alive ? "true" : "false") << std::endl;
        if (daemon_pid > 0) {
            std::cout << "  daemon pid: " << daemon_pid << std::endl;
        }
        std::cout << "  socket: " << socket_path.string() << std::endl;
    }

    return CommandResult::Success;
}

CommandResult LspModule::handle_symbols_command(const CommandArgs& args) {
    if (args.positional.empty()) {
        std::cerr << "Query string is required for symbols command" << std::endl;
        return CommandResult::InvalidArguments;
    }
    
    auto ensure_result = ensure_client_connected();
    if (!ensure_result) {
        print_lsp_error(ensure_result.error());
        return CommandResult::Error;
    }
    
    std::string query = args.positional[0];
    std::filesystem::path workspace = std::filesystem::current_path();
    
    if (auto it = args.options.find("workspace"); it != args.options.end()) {
        workspace = it->second;
    }
    
    auto result = query_symbols(workspace, query);
    if (!result) {
        print_lsp_error(result.error());
        return CommandResult::Error;
    }
    
    print_symbols(result.value());
    return CommandResult::Success;
}

CommandResult LspModule::handle_diagnostics_command(const CommandArgs& args) {
    if (args.positional.empty()) {
        std::cerr << "File path is required for diagnostics command" << std::endl;
        return CommandResult::InvalidArguments;
    }
    
    auto ensure_result = ensure_client_connected();
    if (!ensure_result) {
        print_lsp_error(ensure_result.error());
        return CommandResult::Error;
    }
    
    std::filesystem::path file = resolve_file_path(args.positional[0]);
    
    auto result = get_diagnostics(file);
    if (!result) {
        print_lsp_error(result.error());
        return CommandResult::Error;
    }
    
    print_diagnostics(result.value());
    return CommandResult::Success;
}

CommandResult LspModule::handle_format_command(const CommandArgs& args) {
    if (args.positional.empty()) {
        std::cerr << "File path is required for format command" << std::endl;
        return CommandResult::InvalidArguments;
    }
    
    auto ensure_result = ensure_client_connected();
    if (!ensure_result) {
        print_lsp_error(ensure_result.error());
        return CommandResult::Error;
    }
    
    std::filesystem::path file = resolve_file_path(args.positional[0]);
    
    auto result = format_document(file);
    if (!result) {
        print_lsp_error(result.error());
        return CommandResult::Error;
    }
    
    std::cout << result.value() << std::endl;
    return CommandResult::Success;
}

CommandResult LspModule::handle_completions_command(const CommandArgs& args) {
    if (args.positional.size() < 3) {
        std::cerr << "File, line, and column are required for completions command" << std::endl;
        return CommandResult::InvalidArguments;
    }
    
    auto ensure_result = ensure_client_connected();
    if (!ensure_result) {
        print_lsp_error(ensure_result.error());
        return CommandResult::Error;
    }
    
    std::filesystem::path file = resolve_file_path(args.positional[0]);
    size_t line = std::stoull(args.positional[1]) - 1; // Convert to 0-based
    size_t column = std::stoull(args.positional[2]) - 1; // Convert to 0-based
    
    auto result = get_completions(file, line, column);
    if (!result) {
        print_lsp_error(result.error());
        return CommandResult::Error;
    }
    
    print_completions(result.value());
    return CommandResult::Success;
}

CommandResult LspModule::handle_definition_command(const CommandArgs& args) {
    if (args.positional.size() < 3) {
        std::cerr << "File, line, and column are required for definition command" << std::endl;
        return CommandResult::InvalidArguments;
    }
    
    auto ensure_result = ensure_client_connected();
    if (!ensure_result) {
        print_lsp_error(ensure_result.error());
        return CommandResult::Error;
    }
    
    std::filesystem::path file = resolve_file_path(args.positional[0]);
    size_t line = std::stoull(args.positional[1]) - 1; // Convert to 0-based
    size_t column = std::stoull(args.positional[2]) - 1; // Convert to 0-based
    
    auto result = goto_definition(file, line, column);
    if (!result) {
        print_lsp_error(result.error());
        return CommandResult::Error;
    }
    
    std::cout << "Definition: " << result.value().format() << std::endl;
    return CommandResult::Success;
}

CommandResult LspModule::handle_references_command(const CommandArgs& args) {
    if (args.positional.size() < 3) {
        std::cerr << "File, line, and column are required for references command" << std::endl;
        return CommandResult::InvalidArguments;
    }
    
    auto ensure_result = ensure_client_connected();
    if (!ensure_result) {
        print_lsp_error(ensure_result.error());
        return CommandResult::Error;
    }
    
    std::filesystem::path file = resolve_file_path(args.positional[0]);
    size_t line = std::stoull(args.positional[1]) - 1; // Convert to 0-based
    size_t column = std::stoull(args.positional[2]) - 1; // Convert to 0-based
    
    auto result = find_references(file, line, column);
    if (!result) {
        print_lsp_error(result.error());
        return CommandResult::Error;
    }
    
    print_locations(result.value());
    return CommandResult::Success;
}

CommandResult LspModule::handle_rename_command(const CommandArgs& args) {
    if (args.positional.size() < 4) {
        std::cerr << "File, line, column, and new name are required for rename command" << std::endl;
        return CommandResult::InvalidArguments;
    }
    
    auto ensure_result = ensure_client_connected();
    if (!ensure_result) {
        print_lsp_error(ensure_result.error());
        return CommandResult::Error;
    }
    
    std::filesystem::path file = resolve_file_path(args.positional[0]);
    size_t line = std::stoull(args.positional[1]) - 1; // Convert to 0-based
    size_t column = std::stoull(args.positional[2]) - 1; // Convert to 0-based
    std::string new_name = args.positional[3];
    
    auto result = rename_symbol(file, line, column, new_name);
    if (!result) {
        print_lsp_error(result.error());
        return CommandResult::Error;
    }
    
    print_workspace_edit(result.value());
    return CommandResult::Success;
}

CommandResult LspModule::handle_hover_command(const CommandArgs& args) {
    if (args.positional.size() < 3) {
        std::cerr << "File, line, and column are required for hover command" << std::endl;
        return CommandResult::InvalidArguments;
    }
    
    auto ensure_result = ensure_client_connected();
    if (!ensure_result) {
        print_lsp_error(ensure_result.error());
        return CommandResult::Error;
    }
    
    std::filesystem::path file = resolve_file_path(args.positional[0]);
    size_t line = std::stoull(args.positional[1]) - 1; // Convert to 0-based
    size_t column = std::stoull(args.positional[2]) - 1; // Convert to 0-based
    
    auto result = get_hover_info(file, line, column);
    if (!result) {
        print_lsp_error(result.error());
        return CommandResult::Error;
    }
    
    print_hover(result.value());
    return CommandResult::Success;
}

// Public API methods
std::expected<void, LspError> LspModule::start_server(const LspConfig& config) {
    std::lock_guard<std::mutex> lock(lsp_mutex_);
    
    if (server_ && server_->is_running()) {
        return std::unexpected(LspError("LSP server is already running"));
    }
    
    server_ = std::make_unique<LspServer>(config);
    return server_->start();
}

std::expected<void, LspError> LspModule::stop_server() {
    std::lock_guard<std::mutex> lock(lsp_mutex_);
    
    if (!server_) {
        return std::unexpected(LspError("No LSP server instance"));
    }
    
    auto result = server_->stop();
    server_.reset();
    return result;
}

std::expected<std::vector<Symbol>, LspError> 
LspModule::query_symbols(const std::filesystem::path& workspace, const std::string& query) {
    // Ensure client is connected
    auto ensure_result = ensure_client_connected();
    if (!ensure_result) {
        return std::unexpected(ensure_result.error());
    }
    
    return client_->query_symbols(query);
}

std::expected<std::vector<Diagnostic>, LspError> 
LspModule::get_diagnostics(const std::filesystem::path& file) {
    if (!client_->is_connected()) {
        return std::unexpected(LspError("LSP client is not connected"));
    }
    
    return client_->get_diagnostics(file);
}

std::expected<std::string, LspError> 
LspModule::format_document(const std::filesystem::path& file) {
    if (!client_->is_connected()) {
        return std::unexpected(LspError("LSP client is not connected"));
    }
    
    return client_->format_document(file);
}

std::expected<std::vector<CompletionItem>, LspError> 
LspModule::get_completions(const std::filesystem::path& file, size_t line, size_t column) {
    if (!client_->is_connected()) {
        return std::unexpected(LspError("LSP client is not connected"));
    }
    
    return client_->get_completions(file, Position(line, column));
}

std::expected<Location, LspError> 
LspModule::goto_definition(const std::filesystem::path& file, size_t line, size_t column) {
    if (!client_->is_connected()) {
        return std::unexpected(LspError("LSP client is not connected"));
    }
    
    return client_->goto_definition(file, Position(line, column));
}

std::expected<std::vector<Location>, LspError> 
LspModule::find_references(const std::filesystem::path& file, size_t line, size_t column) {
    if (!client_->is_connected()) {
        return std::unexpected(LspError("LSP client is not connected"));
    }
    
    return client_->find_references(file, Position(line, column));
}

std::expected<WorkspaceEdit, LspError> 
LspModule::rename_symbol(const std::filesystem::path& file, size_t line, size_t column, const std::string& new_name) {
    if (!client_->is_connected()) {
        return std::unexpected(LspError("LSP client is not connected"));
    }
    
    return client_->rename_symbol(file, Position(line, column), new_name);
}

std::expected<Hover, LspError> 
LspModule::get_hover_info(const std::filesystem::path& file, size_t line, size_t column) {
    if (!client_->is_connected()) {
        return std::unexpected(LspError("LSP client is not connected"));
    }
    
    return client_->get_hover_info(file, Position(line, column));
}

// Helper methods
LspConfig LspModule::parse_lsp_config(const CommandArgs& args) {
    LspConfig config;
    
    // Parse port option
    if (auto it = args.options.find("port"); it != args.options.end()) {
        config.port = std::stoi(it->second);
    }
    
    // Parse stdio flag
    if (args.flags.find("stdio") != args.flags.end()) {
        config.stdio = true;
    }
    
    // Parse workspace option
    if (auto it = args.options.find("workspace"); it != args.options.end()) {
        config.workspace_folders.push_back(it->second);
    } else {
        config.workspace_folders.push_back(std::filesystem::current_path());
    }
    
    // Parse trace level
    if (auto it = args.options.find("trace"); it != args.options.end()) {
        config.trace_level = it->second;
    }
    
    // Parse log file
    if (auto it = args.options.find("log-file"); it != args.options.end()) {
        config.log_file = it->second;
    }
    
    return config;
}

Position LspModule::parse_position_arg(const std::string& line_col) {
    return client_->parse_position(line_col);
}

std::filesystem::path LspModule::resolve_file_path(const std::string& file_arg) {
    std::filesystem::path file(file_arg);
    
    if (file.is_relative()) {
        file = std::filesystem::current_path() / file;
    }
    
    return file.lexically_normal();
}

std::filesystem::path LspModule::find_workspace_root(const std::filesystem::path& file) {
    std::filesystem::path current = file.parent_path();
    
    while (current != current.parent_path()) {
        // Look for common workspace indicators
        if (std::filesystem::exists(current / ".git") ||
            std::filesystem::exists(current / "BUILD.bazel") ||
            std::filesystem::exists(current / "meld.yaml") ||
            std::filesystem::exists(current / "Cargo.toml") ||
            std::filesystem::exists(current / "package.json")) {
            return current;
        }
        current = current.parent_path();
    }
    
    return std::filesystem::current_path();
}

void LspModule::print_symbols(const std::vector<Symbol>& symbols) {
    if (symbols.empty()) {
        std::cout << "No symbols found" << std::endl;
        return;
    }
    
    std::cout << "Found " << symbols.size() << " symbol(s):" << std::endl;
    for (const auto& symbol : symbols) {
        std::cout << "  " << symbol.format() << std::endl;
    }
}

void LspModule::print_diagnostics(const std::vector<Diagnostic>& diagnostics) {
    if (diagnostics.empty()) {
        std::cout << "No diagnostics found" << std::endl;
        return;
    }
    
    std::cout << "Found " << diagnostics.size() << " diagnostic(s):" << std::endl;
    for (const auto& diagnostic : diagnostics) {
        std::cout << "  " << diagnostic.format() << std::endl;
    }
}

void LspModule::print_completions(const std::vector<CompletionItem>& completions) {
    if (completions.empty()) {
        std::cout << "No completions available" << std::endl;
        return;
    }
    
    std::cout << "Available completions:" << std::endl;
    for (const auto& completion : completions) {
        std::cout << "  " << completion.format() << std::endl;
    }
}

void LspModule::print_locations(const std::vector<Location>& locations) {
    if (locations.empty()) {
        std::cout << "No locations found" << std::endl;
        return;
    }
    
    std::cout << "Found " << locations.size() << " location(s):" << std::endl;
    for (const auto& location : locations) {
        std::cout << "  " << location.format() << std::endl;
    }
}

void LspModule::print_workspace_edit(const WorkspaceEdit& edit) {
    if (edit.empty()) {
        std::cout << "No changes required" << std::endl;
        return;
    }
    
    std::cout << edit.format() << std::endl;
}

void LspModule::print_hover(const Hover& hover) {
    std::cout << "Hover information:" << std::endl;
    std::cout << hover.format() << std::endl;
}

void LspModule::print_lsp_error(const LspError& error) {
    std::cerr << error.format() << std::endl;
}

std::expected<void, LspError> LspModule::ensure_server_running() {
    std::lock_guard<std::mutex> lock(lsp_mutex_);
    
    if (!server_ || !server_->is_running()) {
        // Try to start server with default config
        LspConfig config;
        config.stdio = true;
        config.workspace_folders.push_back(std::filesystem::current_path());
        
        server_ = std::make_unique<LspServer>(config);
        auto result = server_->start();
        if (!result) {
            return result;
        }
    }
    
    return {};
}

std::expected<void, LspError> LspModule::ensure_client_connected() {
    if (!client_->is_connected()) {
        // Try to connect with default config
        LspConfig config;
        config.stdio = true;
        config.workspace_folders.push_back(std::filesystem::current_path());
        
        auto connect_result = client_->connect(config);
        if (!connect_result) {
            return connect_result;
        }
        
        auto init_result = client_->initialize(config.workspace_folders);
        if (!init_result) {
            return init_result;
        }
    }
    
    return {};
}

} // namespace meld::cli