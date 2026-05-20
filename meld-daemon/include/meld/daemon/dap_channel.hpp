#pragma once

#include <meld/interpreter/ast_interpreter.hpp>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>


namespace meld::daemon {
using meld::interpreter::AstInterpreter;
using meld::interpreter::Environment;
using meld::interpreter::SourceLocation;
using meld::interpreter::StackFrame;
namespace kernel = meld::kernel;

// ─── DAP data types ─────────────────────────────────────────────────

// DAP scope descriptor (Local, Closure, Global, Effect Context)
struct DapScope {
    std::string name;
    int variables_reference = 0;   // Handle for variables request
    bool expensive = false;        // Hint: fetching variables may be slow
};

// DAP variable descriptor
struct DapVariable {
    std::string name;
    std::string value;             // String representation of the value
    std::string type;              // Type name (Integer, String, Vec, etc.)
    int variables_reference = 0;   // Non-zero if expandable (compound values)
    bool is_mutable = false;
};

// DAP stack frame descriptor
struct DapStackFrame {
    int id = 0;
    std::string name;
    std::string source_path;
    size_t line = 0;
    size_t column = 0;
};

// DAP breakpoint response
struct DapBreakpoint {
    int id = 0;
    bool verified = false;
    std::string source_path;
    size_t line = 0;
};

// DAP capabilities advertised during initialize
struct DapCapabilities {
    bool supports_conditional_breakpoints = true;
    bool supports_log_points = true;
    bool supports_evaluate_for_hovers = true;
    bool supports_step_back = false;
    bool supports_set_variable = false;
};

// ─── DAP Server ─────────────────────────────────────────────────────

/// Debug Adapter Protocol server that hooks into the AstInterpreter.
///
/// Listens on a TCP port for DAP client connections (VS Code, Neovim,
/// IntelliJ) and translates DAP requests into interpreter debug operations.
///
/// Usage:
///   AstInterpreter interp;
///   DapServer dap(interp, 4711);
///   dap.start(true);  // blocks until IDE attaches
///   interp.evaluate_program(ast);
///   dap.stop();
class DapServer {
public:
    /// Construct a DAP server attached to the given interpreter.
    /// @param interpreter  The AST interpreter to debug
    /// @param port         TCP port to listen on (default 4711)
    explicit DapServer(AstInterpreter& interpreter, uint16_t port = 4711);

    ~DapServer();

    // Non-copyable, non-movable
    DapServer(const DapServer&) = delete;
    DapServer& operator=(const DapServer&) = delete;
    DapServer(DapServer&&) = delete;
    DapServer& operator=(DapServer&&) = delete;

    /// Start the DAP server.
    /// @param wait_for_attach  If true, block until an IDE debug client
    ///                         connects before returning.
    void start(bool wait_for_attach = false);

    /// Stop the DAP server and close all connections.
    void stop();

    /// Check if the server is currently running.
    bool is_running() const;

    /// Get the port the server is listening on.
    uint16_t port() const;

    /// Get the advertised capabilities.
    DapCapabilities capabilities() const;

    // ─── DAP request handlers ───────────────────────────────────────

    /// Handle an incoming DAP JSON request message.
    /// Returns the JSON response string.
    std::string handle_request(const std::string& json_message);

private:
    AstInterpreter& interpreter_;
    uint16_t port_;
    std::atomic<bool> running_{false};
    std::atomic<bool> client_connected_{false};
    int server_fd_ = -1;
    int client_fd_ = -1;
    mutable std::mutex mutex_;

    // Variable reference counter for compound value expansion
    int next_var_ref_ = 1;
    // Map from variable reference ID to the Environment it represents
    std::unordered_map<int, std::shared_ptr<Environment>> scope_refs_;
    // Map from variable reference ID to a compound kernel::Value
    std::unordered_map<int, kernel::Value> value_refs_;

    // Paused state
    SourceLocation paused_location_;
    std::shared_ptr<Environment> paused_env_;

    // ─── Protocol helpers ───────────────────────────────────────────

    // Read a single DAP message from the client socket (Content-Length framing)
    std::string read_message();

    // Write a DAP message to the client socket
    void write_message(const std::string& json);

    // Send a DAP event (stopped, terminated, output, etc.)
    void send_event(const std::string& event, const std::string& body_json = "{}");

    // ─── Request dispatch ───────────────────────────────────────────

    std::string handle_initialize(const std::string& args_json);
    std::string handle_configuration_done();
    std::string handle_set_breakpoints(const std::string& args_json);
    std::string handle_threads();
    std::string handle_stack_trace(const std::string& args_json);
    std::string handle_scopes(const std::string& args_json);
    std::string handle_variables(const std::string& args_json);
    std::string handle_evaluate(const std::string& args_json);
    std::string handle_continue();
    std::string handle_next();       // step-over
    std::string handle_step_in();
    std::string handle_step_out();
    std::string handle_pause();
    std::string handle_disconnect();

    // ─── Scope building ─────────────────────────────────────────────

    // Build DAP scopes from the Environment chain
    std::vector<DapScope> build_scopes(std::shared_ptr<Environment> env);

    // Build the Effect Context scope (Req 12D)
    DapScope build_effect_context_scope();

    // Build variables for a given scope reference
    std::vector<DapVariable> build_variables(int variables_reference);

    // Convert a kernel::Value to a DapVariable
    DapVariable value_to_variable(const std::string& name,
                                  const kernel::Value& val,
                                  bool is_mutable);

    // ─── Interpreter callbacks ──────────────────────────────────────

    // Called by the interpreter when it pauses at a breakpoint/step
    void on_interpreter_pause(const SourceLocation& loc,
                              std::shared_ptr<Environment> env);

    // Called by the interpreter for logpoint output
    void on_logpoint(const SourceLocation& loc, const std::string& message);

    // TCP accept loop (runs in background)
    void accept_loop();

    // Message processing loop for a connected client
    void message_loop();
};

} // namespace meld::daemon
