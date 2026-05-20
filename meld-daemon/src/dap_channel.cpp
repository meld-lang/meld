#include "meld/daemon/dap_channel.hpp"

#include <meld/parser/parser.hpp>
#include <nlohmann/json.hpp>

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <condition_variable>
#include <mutex>
#include <thread>

namespace meld::daemon {

using json = nlohmann::json;
using meld::interpreter::AstInterpreter;
using meld::interpreter::Environment;
using meld::interpreter::SourceLocation;
using meld::interpreter::StackFrame;
namespace kernel = meld::kernel;

// ─── File-static debug state ────────────────────────────────────────
// These are effectively per-instance since only one DapServer exists at
// a time. The header cannot be modified, so they live here.

namespace {

struct BreakpointEntry {
    std::string file;
    size_t line;
    std::string condition;
};

std::vector<BreakpointEntry> s_breakpoints;
std::mutex s_pause_mutex;
std::condition_variable s_pause_cv;
std::atomic<bool> s_paused{false};
std::atomic<AstInterpreter::DebugAction> s_next_action{
    AstInterpreter::DebugAction::Continue};
size_t s_step_depth = 0;
std::vector<StackFrame> s_last_stack_trace;
bool s_exception_breakpoints_enabled = false;
std::atomic<int> s_seq_counter{1};

int next_seq() { return s_seq_counter.fetch_add(1); }

} // anonymous namespace

// ─── Helper: build a proper DAP response ────────────────────────────

static std::string make_response(int request_seq, const std::string& command,
                                 const json& body, bool success = true) {
    json resp;
    resp["seq"] = next_seq();
    resp["type"] = "response";
    resp["request_seq"] = request_seq;
    resp["success"] = success;
    resp["command"] = command;
    resp["body"] = body;
    return resp.dump();
}

static std::string make_event(const std::string& event, const json& body) {
    json evt;
    evt["seq"] = next_seq();
    evt["type"] = "event";
    evt["event"] = event;
    evt["body"] = body;
    return evt.dump();
}

// ─── Helper: determine if a Value is compound (expandable) ──────────

static bool is_compound(const kernel::Value& val) {
    return val.is<kernel::Vec>() ||
           val.is<meld::types::StructInstance>();
}

// ─── Helper: get type name string for a Value ───────────────────────

static std::string type_name(const kernel::Value& val) {
    if (val.is<kernel::Integer>()) return "Integer";
    if (val.is<kernel::Float>()) return "Float";
    if (val.is<kernel::Boolean>()) return "Boolean";
    if (val.is<kernel::String>()) return "String";
    if (val.is<kernel::Vec>()) return "Vec";
    if (val.is<kernel::Symbol>()) return "Symbol";
    if (val.is<kernel::Function>()) return "Function";
    if (val.is<kernel::Empty>()) return "Empty";
    if (val.is<meld::types::StructInstance>()) return "Struct";
    if (val.is<kernel::Cons>()) return "Cons";
    return "Unknown";
}

// ─── Constructor / Destructor ───────────────────────────────────────

DapServer::DapServer(AstInterpreter& interpreter, uint16_t port)
    : interpreter_(interpreter), port_(port) {
    // Install debug hook
    interpreter_.set_debug_hook(
        [this](const AstInterpreter::DebugContext& ctx) -> AstInterpreter::DebugAction {
            // Capture stack trace for later use
            s_last_stack_trace = ctx.call_stack;

            // Check breakpoints
            for (auto& bp : s_breakpoints) {
                if (bp.file == ctx.location.file &&
                    bp.line == ctx.location.line) {
                    // Conditional breakpoint evaluation
                    if (!bp.condition.empty()) {
                        try {
                            auto env_ptr = std::const_pointer_cast<Environment>(
                                std::shared_ptr<const Environment>(
                                    &ctx.env, [](const Environment*) {}));
                            // Parse and evaluate the condition expression
                            meld::parser::Parser parser;
                            meld::parser::ast::expression expr_ast;
                            if (parser.parse_expression(bp.condition, expr_ast)) {
                                auto old_env = interpreter_.environment();
                                auto result = interpreter_.evaluate(expr_ast);
                                if (!result.is_truthy()) {
                                    continue;  // Condition false, skip
                                }
                            }
                            // If parse fails, treat as unconditional
                        } catch (...) {
                            // Evaluation failed — treat as unconditional, pause
                        }
                    }
                    s_paused = true;
                    json body;
                    body["reason"] = "breakpoint";
                    body["threadId"] = 1;
                    send_event("stopped", body.dump());
                    break;
                }
            }

            // Check step actions
            auto action = s_next_action.load();
            if (!s_paused.load()) {
                if (action == AstInterpreter::DebugAction::StepOver &&
                    ctx.stack_depth <= s_step_depth) {
                    s_paused = true;
                    json body;
                    body["reason"] = "step";
                    body["threadId"] = 1;
                    send_event("stopped", body.dump());
                }
                else if (action == AstInterpreter::DebugAction::StepIn) {
                    s_paused = true;
                    json body;
                    body["reason"] = "step";
                    body["threadId"] = 1;
                    send_event("stopped", body.dump());
                } else if (action == AstInterpreter::DebugAction::StepOut &&
                           ctx.stack_depth < s_step_depth) {
                    s_paused = true;
                    json body;
                    body["reason"] = "step";
                    body["threadId"] = 1;
                    send_event("stopped", body.dump());
                } else if (action == AstInterpreter::DebugAction::Pause) {
                    s_paused = true;
                    json body;
                    body["reason"] = "pause";
                    body["threadId"] = 1;
                    send_event("stopped", body.dump());
                }
            }

            // If paused, block until resumed
            if (s_paused.load()) {
                paused_location_ = ctx.location;
                paused_env_ = std::const_pointer_cast<Environment>(
                    std::shared_ptr<const Environment>(
                        &ctx.env, [](const Environment*) {}));
                std::unique_lock<std::mutex> lock(s_pause_mutex);
                s_pause_cv.wait(lock, [] { return !s_paused.load(); });
            }

            return s_next_action.load();
        });
}

DapServer::~DapServer() { stop(); }

// ─── Server lifecycle ───────────────────────────────────────────────

void DapServer::start(bool wait_for_attach) {
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) return;

    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(server_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(server_fd_);
        server_fd_ = -1;
        return;
    }
    if (listen(server_fd_, 1) < 0) {
        close(server_fd_);
        server_fd_ = -1;
        return;
    }

    running_ = true;

    if (wait_for_attach) {
        client_fd_ = accept(server_fd_, nullptr, nullptr);
        client_connected_ = (client_fd_ >= 0);
        if (client_connected_) {
            std::thread([this] { message_loop(); }).detach();
        }
    } else {
        std::thread([this] { accept_loop(); }).detach();
    }
}

void DapServer::stop() {
    running_ = false;
    s_paused = false;
    s_pause_cv.notify_all();
    if (client_fd_ >= 0) { close(client_fd_); client_fd_ = -1; }
    if (server_fd_ >= 0) { close(server_fd_); server_fd_ = -1; }
}

bool DapServer::is_running() const { return running_; }
uint16_t DapServer::port() const { return port_; }
DapCapabilities DapServer::capabilities() const { return {}; }

void DapServer::accept_loop() {
    client_fd_ = accept(server_fd_, nullptr, nullptr);
    client_connected_ = (client_fd_ >= 0);
    if (client_connected_) message_loop();
}

// ─── DAP message I/O ────────────────────────────────────────────────

std::string DapServer::read_message() {
    if (client_fd_ < 0) return "";
    // Read Content-Length header
    std::string header;
    char c;
    while (::read(client_fd_, &c, 1) == 1) {
        header += c;
        if (header.size() >= 4 &&
            header.substr(header.size() - 4) == "\r\n\r\n")
            break;
    }
    auto pos = header.find("Content-Length: ");
    if (pos == std::string::npos) return "";
    int len = std::stoi(header.substr(pos + 16));
    std::string body(static_cast<size_t>(len), '\0');
    size_t read_total = 0;
    while (read_total < static_cast<size_t>(len)) {
        auto n = ::read(client_fd_, &body[read_total],
                        static_cast<size_t>(len) - read_total);
        if (n <= 0) break;
        read_total += static_cast<size_t>(n);
    }
    return body;
}

void DapServer::write_message(const std::string& msg_json) {
    if (client_fd_ < 0) return;
    std::string msg =
        "Content-Length: " + std::to_string(msg_json.size()) + "\r\n\r\n" + msg_json;
    ::write(client_fd_, msg.c_str(), msg.size());
}

void DapServer::send_event(const std::string& event,
                           const std::string& body_json) {
    try {
        json body = json::parse(body_json);
        std::string evt = make_event(event, body);
        write_message(evt);
    } catch (...) {
        // Fallback: send with empty body
        std::string evt = make_event(event, json::object());
        write_message(evt);
    }
}

// ─── Output events (helper, not a class method) ────────────────────
// send_event is a member, so output sending is done inline where needed.

// ─── Message loop ───────────────────────────────────────────────────

void DapServer::message_loop() {
    while (running_ && client_connected_) {
        auto msg = read_message();
        if (msg.empty()) break;
        auto response = handle_request(msg);
        if (!response.empty()) write_message(response);
    }
}

// ─── Request dispatch ───────────────────────────────────────────────

std::string DapServer::handle_request(const std::string& json_message) {
    try {
        json req = json::parse(json_message);
        std::string command = req.value("command", "");
        int seq = req.value("seq", 0);
        std::string args_json = req.contains("arguments")
                                    ? req["arguments"].dump()
                                    : "{}";

        json body;
        if (command == "initialize")
            body = json::parse(handle_initialize(args_json));
        else if (command == "configurationDone")
            body = json::parse(handle_configuration_done());
        else if (command == "setBreakpoints")
            body = json::parse(handle_set_breakpoints(args_json));
        else if (command == "threads")
            body = json::parse(handle_threads());
        else if (command == "stackTrace")
            body = json::parse(handle_stack_trace(args_json));
        else if (command == "scopes")
            body = json::parse(handle_scopes(args_json));
        else if (command == "variables")
            body = json::parse(handle_variables(args_json));
        else if (command == "evaluate")
            body = json::parse(handle_evaluate(args_json));
        else if (command == "continue")
            body = json::parse(handle_continue());
        else if (command == "next")
            body = json::parse(handle_next());
        else if (command == "stepIn")
            body = json::parse(handle_step_in());
        else if (command == "stepOut")
            body = json::parse(handle_step_out());
        else if (command == "pause")
            body = json::parse(handle_pause());
        else if (command == "disconnect")
            body = json::parse(handle_disconnect());
        else if (command == "setExceptionBreakpoints") {
            // Store exception breakpoint filters
            json args = json::parse(args_json);
            auto filters = args.value("filters", json::array());
            s_exception_breakpoints_enabled = !filters.empty();
            body = json::object();
        } else if (command == "terminate") {
            // Stop the interpreter and close
            stop();
            body = json::object();
        } else {
            body = json::object();
        }

        return make_response(seq, command, body);
    } catch (const std::exception& e) {
        // Parse error or unknown command — return error response
        json err_body;
        err_body["error"] = e.what();
        json resp;
        resp["seq"] = next_seq();
        resp["type"] = "response";
        resp["request_seq"] = 0;
        resp["success"] = false;
        resp["command"] = "";
        resp["body"] = err_body;
        return resp.dump();
    }
}

// ─── Handler implementations ────────────────────────────────────────

std::string DapServer::handle_initialize(const std::string& /*args_json*/) {
    json body;
    body["supportsConditionalBreakpoints"] = true;
    body["supportsEvaluateForHovers"] = true;
    body["supportsExceptionInfoRequest"] = true;
    body["supportsTerminateRequest"] = true;
    body["supportsLogPoints"] = true;
    return body.dump();
}

std::string DapServer::handle_configuration_done() {
    send_event("initialized", "{}");
    return json::object().dump();
}

std::string DapServer::handle_set_breakpoints(const std::string& args_json) {
    try {
        json args = json::parse(args_json);
        std::string path;
        if (args.contains("source") && args["source"].contains("path")) {
            path = args["source"]["path"].get<std::string>();
        }

        // Clear breakpoints for this file
        std::erase_if(s_breakpoints, [&](const BreakpointEntry& bp) {
            return bp.file == path;
        });

        json result_bps = json::array();
        int id = 1;
        if (args.contains("breakpoints")) {
            for (auto& bp_json : args["breakpoints"]) {
                size_t line = bp_json.value("line", 0u);
                std::string condition = bp_json.value("condition", "");
                s_breakpoints.push_back({path, line, condition});

                json bp_resp;
                bp_resp["id"] = id++;
                bp_resp["verified"] = true;
                bp_resp["line"] = line;
                bp_resp["source"]["path"] = path;
                result_bps.push_back(bp_resp);
            }
        }

        json body;
        body["breakpoints"] = result_bps;
        return body.dump();
    } catch (...) {
        json body;
        body["breakpoints"] = json::array();
        return body.dump();
    }
}

std::string DapServer::handle_threads() {
    json body;
    body["threads"] = json::array({{{"id", 1}, {"name", "main"}}});
    return body.dump();
}

std::string DapServer::handle_stack_trace(const std::string& /*args_json*/) {
    json frames = json::array();

    // Use the captured stack trace from the debug hook
    if (!s_last_stack_trace.empty()) {
        int frame_id = 0;
        for (auto it = s_last_stack_trace.rbegin();
             it != s_last_stack_trace.rend(); ++it) {
            json frame;
            frame["id"] = frame_id++;
            frame["name"] = it->function_name.empty() ? "<anonymous>"
                                                      : it->function_name;
            frame["source"]["path"] = it->location.file;
            frame["line"] = it->location.line;
            frame["column"] = it->location.column;
            frames.push_back(frame);
        }
    }

    // Always include the current paused location as the top frame
    json current_frame;
    current_frame["id"] = static_cast<int>(frames.size());
    current_frame["name"] = "<current>";
    current_frame["source"]["path"] = paused_location_.file;
    current_frame["line"] = paused_location_.line;
    current_frame["column"] = paused_location_.column;
    frames.insert(frames.begin(), current_frame);

    json body;
    body["stackFrames"] = frames;
    body["totalFrames"] = static_cast<int>(frames.size());
    return body.dump();
}

std::string DapServer::handle_scopes(const std::string& /*args_json*/) {
    // Build scopes from the environment chain
    json scopes_arr = json::array();

    // Reset variable references for this pause
    scope_refs_.clear();
    value_refs_.clear();
    next_var_ref_ = 1;

    if (paused_env_) {
        // Local scope — current environment
        int local_ref = next_var_ref_++;
        scope_refs_[local_ref] = paused_env_;
        json local_scope;
        local_scope["name"] = "Local";
        local_scope["variablesReference"] = local_ref;
        local_scope["expensive"] = false;
        scopes_arr.push_back(local_scope);

        // Closure scope — parent environment (if exists)
        auto parent = paused_env_->parent();
        if (parent) {
            int closure_ref = next_var_ref_++;
            scope_refs_[closure_ref] = parent;
            json closure_scope;
            closure_scope["name"] = "Closure";
            closure_scope["variablesReference"] = closure_ref;
            closure_scope["expensive"] = false;
            scopes_arr.push_back(closure_scope);

            // Global scope — root environment
            auto root = parent;
            while (root->parent()) {
                root = root->parent();
            }
            if (root != parent) {
                int global_ref = next_var_ref_++;
                scope_refs_[global_ref] = root;
                json global_scope;
                global_scope["name"] = "Global";
                global_scope["variablesReference"] = global_ref;
                global_scope["expensive"] = false;
                scopes_arr.push_back(global_scope);
            }
        }
    }

    json body;
    body["scopes"] = scopes_arr;
    return body.dump();
}

std::string DapServer::handle_variables(const std::string& args_json) {
    json vars_arr = json::array();

    try {
        json args = json::parse(args_json);
        int var_ref = args.value("variablesReference", 0);

        // Check if this is a scope reference
        auto scope_it = scope_refs_.find(var_ref);
        if (scope_it != scope_refs_.end()) {
            auto env = scope_it->second;
            if (env) {
                auto bindings = env->all_bindings();
                for (auto& [name, val] : bindings) {
                    if (name.starts_with("__")) continue;
                    json var;
                    var["name"] = name;
                    var["value"] = val.to_string();
                    var["type"] = type_name(val);

                    // Compound value expansion
                    if (is_compound(val)) {
                        int ref = next_var_ref_++;
                        value_refs_[ref] = val;
                        var["variablesReference"] = ref;
                    } else {
                        var["variablesReference"] = 0;
                    }
                    vars_arr.push_back(var);
                }
            }
        }
        // Check if this is a compound value reference
        else {
            auto val_it = value_refs_.find(var_ref);
            if (val_it != value_refs_.end()) {
                const auto& val = val_it->second;

                if (val.is<kernel::Vec>()) {
                    auto vec = val.as<kernel::Vec>();
                    for (size_t i = 0; i < vec->size(); ++i) {
                        const auto& elem = vec->at(i);
                        json var;
                        var["name"] = std::to_string(i);
                        var["value"] = elem.to_string();
                        var["type"] = type_name(elem);
                        if (is_compound(elem)) {
                            int ref = next_var_ref_++;
                            value_refs_[ref] = elem;
                            var["variablesReference"] = ref;
                        } else {
                            var["variablesReference"] = 0;
                        }
                        vars_arr.push_back(var);
                    }
                }
                else if (val.is<meld::types::StructInstance>()) {
                    auto inst = val.as<meld::types::StructInstance>();
                    // StructInstance uses get_field — we iterate known fields
                    // Since we can't enumerate fields directly without the
                    // MetaType, show the string representation
                    json var;
                    var["name"] = "<struct>";
                    var["value"] = val.to_string();
                    var["type"] = "Struct";
                    var["variablesReference"] = 0;
                    vars_arr.push_back(var);
                }
            }
        }
    } catch (...) {
        // Return empty variables on error
    }

    json body;
    body["variables"] = vars_arr;
    return body.dump();
}

std::string DapServer::handle_evaluate(const std::string& args_json) {
    try {
        json args = json::parse(args_json);
        std::string expression = args.value("expression", "");

        if (expression.empty() || !paused_env_) {
            json body;
            body["result"] = "<no expression>";
            body["variablesReference"] = 0;
            return body.dump();
        }

        // Evaluate the expression in the paused environment
        meld::parser::Parser parser;
        meld::parser::ast::expression expr_ast;
        std::string result;
        if (parser.parse_expression(expression, expr_ast)) {
            auto val = interpreter_.evaluate(expr_ast);
            result = val.to_string();
        } else {
            result = "<parse error: " + parser.error_message() + ">";
        }

        json body;
        body["result"] = result;
        body["variablesReference"] = 0;
        return body.dump();
    } catch (const std::exception& e) {
        json body;
        body["result"] = std::string("<error: ") + e.what() + ">";
        body["variablesReference"] = 0;
        return body.dump();
    } catch (...) {
        json body;
        body["result"] = "<evaluation error>";
        body["variablesReference"] = 0;
        return body.dump();
    }
}

std::string DapServer::handle_continue() {
    s_next_action = AstInterpreter::DebugAction::Continue;
    s_paused = false;
    s_pause_cv.notify_all();
    json body;
    body["allThreadsContinued"] = true;
    return body.dump();
}

std::string DapServer::handle_next() {
    // Use the actual stack depth from the last debug context
    s_step_depth = s_last_stack_trace.size();
    s_next_action = AstInterpreter::DebugAction::StepOver;
    s_paused = false;
    s_pause_cv.notify_all();
    return json::object().dump();
}

std::string DapServer::handle_step_in() {
    s_next_action = AstInterpreter::DebugAction::StepIn;
    s_paused = false;
    s_pause_cv.notify_all();
    return json::object().dump();
}

std::string DapServer::handle_step_out() {
    s_step_depth = s_last_stack_trace.size();
    s_next_action = AstInterpreter::DebugAction::StepOut;
    s_paused = false;
    s_pause_cv.notify_all();
    return json::object().dump();
}

std::string DapServer::handle_pause() {
    s_next_action = AstInterpreter::DebugAction::Pause;
    return json::object().dump();
}

std::string DapServer::handle_disconnect() {
    stop();
    return json::object().dump();
}

// ─── Scope/variable helpers ─────────────────────────────────────────

std::vector<DapScope> DapServer::build_scopes(std::shared_ptr<Environment> env) {
    std::vector<DapScope> scopes;
    if (!env) return scopes;

    scopes.push_back({"Local", 1, false});

    auto parent = env->parent();
    if (parent) {
        scopes.push_back({"Closure", 2, false});
        auto root = parent;
        while (root->parent()) root = root->parent();
        if (root != parent) {
            scopes.push_back({"Global", 3, false});
        }
    }
    return scopes;
}

DapScope DapServer::build_effect_context_scope() {
    return {"Effects", next_var_ref_++, false};
}

std::vector<DapVariable> DapServer::build_variables(int variables_reference) {
    std::vector<DapVariable> vars;
    auto scope_it = scope_refs_.find(variables_reference);
    if (scope_it != scope_refs_.end() && scope_it->second) {
        auto bindings = scope_it->second->all_bindings();
        for (auto& [name, val] : bindings) {
            if (name.starts_with("__")) continue;
            vars.push_back(value_to_variable(name, val, false));
        }
    }
    return vars;
}

DapVariable DapServer::value_to_variable(const std::string& name,
                                         const kernel::Value& val,
                                         bool is_mutable) {
    int ref = 0;
    if (is_compound(val)) {
        ref = next_var_ref_++;
        value_refs_[ref] = val;
    }
    return {name, val.to_string(), type_name(val), ref, is_mutable};
}

// ─── Interpreter callbacks ──────────────────────────────────────────

void DapServer::on_interpreter_pause(const SourceLocation& loc,
                                     std::shared_ptr<Environment> env) {
    paused_location_ = loc;
    paused_env_ = env;
}

void DapServer::on_logpoint(const SourceLocation& /*loc*/,
                            const std::string& message) {
    json body;
    body["category"] = "console";
    body["output"] = message + "\n";
    send_event("output", body.dump());
}

} // namespace meld::daemon
