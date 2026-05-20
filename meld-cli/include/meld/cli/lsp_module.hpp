#pragma once

#include "command_handler.hpp"
#include "error_handler.hpp"
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <expected>
#include <optional>
#include <thread>
#include <atomic>
#include <mutex>

namespace meld::cli {

/**
 * LSP server configuration
 */
struct LspConfig {
    std::optional<int> port;
    bool stdio = false;
    std::vector<std::filesystem::path> workspace_folders;
    std::map<std::string, std::string> initialization_options;
    std::string trace_level = "off"; // off, messages, verbose
    std::string log_file;
    bool enable_diagnostics = true;
    bool enable_completions = true;
    bool enable_hover = true;
    bool enable_goto_definition = true;
    bool enable_find_references = true;
    bool enable_rename = true;
    bool enable_formatting = true;
};

/**
 * LSP error information
 */
struct LspError {
    std::string message;
    int code = 0;
    std::string data;
    
    LspError(std::string msg, int c = 0, std::string d = "")
        : message(std::move(msg)), code(c), data(std::move(d)) {}
    
    std::string format() const;
};

/**
 * Position in a text document
 */
struct Position {
    size_t line = 0;
    size_t column = 0;
    
    Position() = default;
    Position(size_t l, size_t c) : line(l), column(c) {}
    
    bool operator==(const Position& other) const {
        return line == other.line && column == other.column;
    }
    
    bool operator<(const Position& other) const {
        return line < other.line || (line == other.line && column < other.column);
    }

    bool operator>=(const Position& other) const {
        return !(*this < other);
    }

    bool operator<=(const Position& other) const {
        return !(other < *this);
    }

    bool operator>(const Position& other) const {
        return other < *this;
    }
};

/**
 * Range in a text document
 */
struct Range {
    Position start;
    Position end;
    
    Range() = default;
    Range(Position s, Position e) : start(s), end(e) {}
    Range(size_t start_line, size_t start_col, size_t end_line, size_t end_col)
        : start(start_line, start_col), end(end_line, end_col) {}
    
    bool contains(const Position& pos) const {
        return pos >= start && pos < end;
    }
    
    bool is_valid() const {
        return start <= end;
    }
};

/**
 * Location in a workspace
 */
struct Location {
    std::filesystem::path file;
    Range range;
    
    Location() = default;
    Location(std::filesystem::path f, Range r) : file(std::move(f)), range(r) {}
    Location(std::filesystem::path f, size_t line, size_t col)
        : file(std::move(f)), range(Position(line, col), Position(line, col)) {}
    
    std::string format() const;
};

/**
 * Symbol information
 */
enum class SymbolKind {
    File = 1,
    Module = 2,
    Namespace = 3,
    Package = 4,
    Class = 5,
    Method = 6,
    Property = 7,
    Field = 8,
    Constructor = 9,
    Enum = 10,
    Interface = 11,
    Function = 12,
    Variable = 13,
    Constant = 14,
    String = 15,
    Number = 16,
    Boolean = 17,
    Array = 18,
    Object = 19,
    Key = 20,
    Null = 21,
    EnumMember = 22,
    Struct = 23,
    Event = 24,
    Operator = 25,
    TypeParameter = 26
};

struct Symbol {
    std::string name;
    SymbolKind kind;
    Location location;
    std::string detail;
    std::string documentation;
    bool deprecated = false;
    
    Symbol(std::string n, SymbolKind k, Location loc)
        : name(std::move(n)), kind(k), location(std::move(loc)) {}
    
    std::string format() const;
};

/**
 * Diagnostic severity levels
 */
enum class DiagnosticSeverity {
    Error = 1,
    Warning = 2,
    Information = 3,
    Hint = 4
};

/**
 * Diagnostic information
 */
struct Diagnostic {
    Range range;
    DiagnosticSeverity severity;
    std::string code;
    std::string source;
    std::string message;
    std::vector<std::string> tags;
    
    Diagnostic(Range r, DiagnosticSeverity s, std::string msg)
        : range(r), severity(s), message(std::move(msg)) {}
    
    std::string format() const;
};

/**
 * Completion item
 */
enum class CompletionItemKind {
    Text = 1,
    Method = 2,
    Function = 3,
    Constructor = 4,
    Field = 5,
    Variable = 6,
    Class = 7,
    Interface = 8,
    Module = 9,
    Property = 10,
    Unit = 11,
    Value = 12,
    Enum = 13,
    Keyword = 14,
    Snippet = 15,
    Color = 16,
    File = 17,
    Reference = 18,
    Folder = 19,
    EnumMember = 20,
    Constant = 21,
    Struct = 22,
    Event = 23,
    Operator = 24,
    TypeParameter = 25
};

struct CompletionItem {
    std::string label;
    CompletionItemKind kind;
    std::string detail;
    std::string documentation;
    std::string insert_text;
    std::string filter_text;
    std::string sort_text;
    bool deprecated = false;
    
    CompletionItem(std::string l, CompletionItemKind k)
        : label(std::move(l)), kind(k) {}
    
    std::string format() const;
};

/**
 * Hover information
 */
struct Hover {
    std::string contents;
    std::optional<Range> range;
    
    Hover(std::string c) : contents(std::move(c)) {}
    Hover(std::string c, Range r) : contents(std::move(c)), range(r) {}
    
    std::string format() const;
};

/**
 * Text edit for workspace modifications
 */
struct TextEdit {
    Range range;
    std::string new_text;
    
    TextEdit(Range r, std::string text) : range(r), new_text(std::move(text)) {}
    
    std::string format() const;
};

/**
 * Workspace edit containing changes to multiple files
 */
struct WorkspaceEdit {
    std::map<std::filesystem::path, std::vector<TextEdit>> changes;
    
    void add_edit(const std::filesystem::path& file, const TextEdit& edit) {
        changes[file].push_back(edit);
    }
    
    bool empty() const { return changes.empty(); }
    size_t total_edits() const;
    
    std::string format() const;
};

/**
 * LSP server process management
 */
class LspServer {
public:
    LspServer(const LspConfig& config);
    ~LspServer();
    
    /**
     * Start the LSP server
     */
    std::expected<void, LspError> start();
    
    /**
     * Stop the LSP server
     */
    std::expected<void, LspError> stop();
    
    /**
     * Check if server is running
     */
    bool is_running() const { return running_; }
    
    /**
     * Get server configuration
     */
    const LspConfig& get_config() const { return config_; }
    
    /**
     * Get server process ID (if running as separate process)
     */
    std::optional<int> get_process_id() const { return process_id_; }

private:
    LspConfig config_;
    std::atomic<bool> running_{false};
    std::optional<int> process_id_;
    std::unique_ptr<std::thread> server_thread_;
    std::mutex server_mutex_;
    
    void run_server();
    void run_stdio_server();
    void run_tcp_server();
};

/**
 * LSP client for communicating with LSP server
 */
class LspClient {
public:
    LspClient();
    ~LspClient();
    
    /**
     * Connect to LSP server
     */
    std::expected<void, LspError> connect(const LspConfig& config);
    
    /**
     * Disconnect from LSP server
     */
    void disconnect();
    
    /**
     * Check if connected to server
     */
    bool is_connected() const { return connected_; }
    
    /**
     * Initialize the LSP session
     */
    std::expected<void, LspError> initialize(const std::vector<std::filesystem::path>& workspace_folders);
    
    /**
     * Send shutdown request
     */
    std::expected<void, LspError> shutdown();
    
    /**
     * Query symbols in workspace
     */
    std::expected<std::vector<Symbol>, LspError> query_symbols(const std::string& query);
    
    /**
     * Get diagnostics for a file
     */
    std::expected<std::vector<Diagnostic>, LspError> get_diagnostics(const std::filesystem::path& file);
    
    /**
     * Format a document
     */
    std::expected<std::string, LspError> format_document(const std::filesystem::path& file);
    
    /**
     * Get completions at position
     */
    std::expected<std::vector<CompletionItem>, LspError> 
    get_completions(const std::filesystem::path& file, const Position& position);
    
    /**
     * Go to definition
     */
    std::expected<Location, LspError> 
    goto_definition(const std::filesystem::path& file, const Position& position);
    
    /**
     * Find references
     */
    std::expected<std::vector<Location>, LspError> 
    find_references(const std::filesystem::path& file, const Position& position);
    
    /**
     * Rename symbol
     */
    std::expected<WorkspaceEdit, LspError> 
    rename_symbol(const std::filesystem::path& file, const Position& position, const std::string& new_name);
    
    /**
     * Get hover information
     */
    std::expected<Hover, LspError> 
    get_hover_info(const std::filesystem::path& file, const Position& position);

private:
    bool connected_{false};
    LspConfig config_;
    std::mutex client_mutex_;
    
    // Communication methods
    std::expected<std::string, LspError> send_request(const std::string& method, const std::string& params);
    std::expected<void, LspError> send_notification(const std::string& method, const std::string& params);
    
    // Helper methods
    std::string read_file_content(const std::filesystem::path& file);
public:
    Position parse_position(const std::string& line_col);
private:
    std::string format_position(const Position& pos);
    std::string format_range(const Range& range);
};

/**
 * LSP integration module - provides command-line interface to LSP functionality
 */
class LspModule : public BaseCommandHandler {
public:
    LspModule();
    ~LspModule() override = default;
    
    // CommandHandler interface
    CommandResult execute(const CommandArgs& args) override;
    std::string get_help() const override;
    std::string get_usage() const override;
    std::vector<std::string> get_completions(const std::string& partial) const override;
    bool validate_args(const CommandArgs& args, std::string& error_message) const override;
    
    /**
     * Start LSP server
     */
    std::expected<void, LspError> start_server(const LspConfig& config = {});
    
    /**
     * Stop LSP server
     */
    std::expected<void, LspError> stop_server();
    
    /**
     * Query symbols across workspace
     */
    std::expected<std::vector<Symbol>, LspError> 
    query_symbols(const std::filesystem::path& workspace, const std::string& query);
    
    /**
     * Get diagnostics for file
     */
    std::expected<std::vector<Diagnostic>, LspError> 
    get_diagnostics(const std::filesystem::path& file);
    
    /**
     * Format document
     */
    std::expected<std::string, LspError> 
    format_document(const std::filesystem::path& file);
    
    /**
     * Get completions at position
     */
    std::expected<std::vector<CompletionItem>, LspError> 
    get_completions(const std::filesystem::path& file, size_t line, size_t column);
    
    /**
     * Go to definition
     */
    std::expected<Location, LspError> 
    goto_definition(const std::filesystem::path& file, size_t line, size_t column);
    
    /**
     * Find references
     */
    std::expected<std::vector<Location>, LspError> 
    find_references(const std::filesystem::path& file, size_t line, size_t column);
    
    /**
     * Rename symbol
     */
    std::expected<WorkspaceEdit, LspError> 
    rename_symbol(const std::filesystem::path& file, size_t line, size_t column, const std::string& new_name);
    
    /**
     * Get hover information
     */
    std::expected<Hover, LspError> 
    get_hover_info(const std::filesystem::path& file, size_t line, size_t column);

private:
    std::unique_ptr<LspServer> server_;
    std::unique_ptr<LspClient> client_;
    std::mutex lsp_mutex_;
    
    // Command handlers
    CommandResult handle_start_command(const CommandArgs& args);
    CommandResult handle_stop_command(const CommandArgs& args);
    CommandResult handle_status_command(const CommandArgs& args);
    CommandResult handle_symbols_command(const CommandArgs& args);
    CommandResult handle_diagnostics_command(const CommandArgs& args);
    CommandResult handle_format_command(const CommandArgs& args);
    CommandResult handle_completions_command(const CommandArgs& args);
    CommandResult handle_definition_command(const CommandArgs& args);
    CommandResult handle_references_command(const CommandArgs& args);
    CommandResult handle_rename_command(const CommandArgs& args);
    CommandResult handle_hover_command(const CommandArgs& args);
    
    // Helper methods
    LspConfig parse_lsp_config(const CommandArgs& args);
    Position parse_position_arg(const std::string& line_col);
    std::filesystem::path resolve_file_path(const std::string& file_arg);
    std::filesystem::path find_workspace_root(const std::filesystem::path& file);
    void print_symbols(const std::vector<Symbol>& symbols);
    void print_diagnostics(const std::vector<Diagnostic>& diagnostics);
    void print_completions(const std::vector<CompletionItem>& completions);
    void print_locations(const std::vector<Location>& locations);
    void print_workspace_edit(const WorkspaceEdit& edit);
    void print_hover(const Hover& hover);
    void print_lsp_error(const LspError& error);
    
    // Ensure server is running for operations that need it
    std::expected<void, LspError> ensure_server_running();
    std::expected<void, LspError> ensure_client_connected();
};

} // namespace meld::cli