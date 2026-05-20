#pragma once

#include <memory>
#include <string>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <unordered_map>
#include <vector>
#include <stdexcept>

#include <meld/kernel/primitives.hpp>
#include <meld/parser/ast.hpp>
#include <meld/effects/effect_firewall.hpp>

namespace meld::interpreter {

// Stepping mode for debug control flow
enum class StepMode {
    Continue,   // Run until next breakpoint
    StepOver,   // Step to next statement at same or lower call depth
    StepIn,     // Step to next statement regardless of call depth
    StepOut     // Step until returning to the caller's frame
};

// Breakpoint descriptor
struct BreakpointInfo {
    std::string file;
    size_t line = 0;
    std::optional<std::string> condition;      // Expression to evaluate; pause only if truthy
    bool is_logpoint = false;                  // If true, log instead of pausing
    std::string log_expression;                // Expression to evaluate and log (logpoints only)
};

// Breakpoint key for map lookup
struct BreakpointKey {
    std::string file;
    size_t line;
    bool operator<(const BreakpointKey& other) const {
        if (file != other.file) return file < other.file;
        return line < other.line;
    }
};

// Binding in an environment scope — tracks value and mutability
struct Binding {
    kernel::Value value;
    bool is_mutable;
};

// Source location for error reporting
struct SourceLocation {
    std::string file;
    size_t line = 0;
    size_t column = 0;
};

// Stack frame for call-stack traces
struct StackFrame {
    std::string function_name;
    SourceLocation location;
};

// Runtime error with source location and stack trace
class InterpreterError : public std::runtime_error {
public:
    InterpreterError(std::string message, SourceLocation location,
                     std::vector<StackFrame> stack_trace = {});

    const SourceLocation& location() const;
    const std::vector<StackFrame>& stack_trace() const;
    const std::vector<std::string>& suggestions() const { return suggestions_; }

    // Structured JSON output for machine consumption
    std::string to_json() const;

    // Generate suggestions based on error message
    static std::vector<std::string> make_suggestions(const std::string& message);

private:
    SourceLocation location_;
    std::vector<StackFrame> stack_trace_;
    std::vector<std::string> suggestions_;
};

// Non-local return: rtn inside a lambda propagates to the enclosing function
struct NonLocalReturn {
    kernel::Value value;
};

// Delimited continuation support
struct Continuation {
    std::function<kernel::Value(kernel::Value)> resume_fn;
    bool consumed = false;
};

struct SuspendSignal {
    std::string delimiter;
    kernel::Value callback;  // fnc(k: Continuation) -> any
};

// Scope-chain environment for variable bindings
class Environment : public std::enable_shared_from_this<Environment> {
public:
    explicit Environment(std::shared_ptr<Environment> parent = nullptr);

    // Define a new binding in the current scope
    void define(const std::string& name, kernel::Value value, bool is_mutable);

    // Assign to an existing binding (walks scope chain, throws if immutable or unbound)
    void assign(const std::string& name, kernel::Value value);

    // Lookup a binding (walks scope chain, throws if unbound)
    kernel::Value lookup(const std::string& name) const;

    // Check if a name is bound anywhere in the scope chain
    bool has(const std::string& name) const;

    // Get parent environment
    std::shared_ptr<Environment> parent() const;

    // Create a child environment with this as parent
    std::shared_ptr<Environment> create_child();

    // Get a binding value if it exists (no throw)
    std::optional<kernel::Value> get(const std::string& name) const;

    // Get all bindings in this scope (not parent)
    std::unordered_map<std::string, kernel::Value> all_bindings() const;

private:
    std::unordered_map<std::string, Binding> bindings_;
    std::shared_ptr<Environment> parent_;
};

// Forward declaration for visitor friend
struct EvalVisitor;

// Tree-walking AST interpreter producing kernel::Value results
class AstInterpreter {
    friend struct EvalVisitor;
public:
    explicit AstInterpreter(std::shared_ptr<Environment> env = nullptr);

    // Evaluate a full program (list of top-level expressions)
    kernel::Value evaluate_program(const std::vector<parser::ast::expression>& exprs);

    // Evaluate a single expression
    kernel::Value evaluate(const parser::ast::expression& expr);

    // Access the current environment (for REPL state persistence)
    std::shared_ptr<Environment> environment() const;

    // Set source file name for error reporting
    void set_source_file(const std::string& file);
    void load_prelude();

    // Debug hook — called before each statement execution.
    // Receives: source location, current environment, call stack.
    // The hook can block (for breakpoints/stepping) or inspect state.
    enum class DebugAction { Continue, StepOver, StepIn, StepOut, Pause };
    struct DebugContext {
        SourceLocation location;
        const Environment& env;
        const std::vector<StackFrame>& call_stack;
        size_t stack_depth;
    };
    using DebugHook = std::function<DebugAction(const DebugContext&)>;
    void set_debug_hook(DebugHook hook) { debug_hook_ = std::move(hook); }

    // ─── Debug hooks for DAP integration (Req 12A) ──────────────────

    // Enable/disable debug mode (breakpoint checking)
    void set_debug_mode(bool enabled);
    bool debug_mode() const;

    // Breakpoint management
    void set_breakpoint(const std::string& file, size_t line);
    void set_conditional_breakpoint(const std::string& file, size_t line,
                                    const std::string& condition);
    void set_logpoint(const std::string& file, size_t line,
                      const std::string& log_expression);
    void remove_breakpoint(const std::string& file, size_t line);
    void clear_all_breakpoints();

    // Stepping control
    void set_step_mode(StepMode mode);
    StepMode step_mode() const;

    // Pause callback — invoked when the interpreter pauses at a breakpoint or step
    // Receives the current source location and the active Environment
    using PauseCallback = std::function<void(const SourceLocation&,
                                             std::shared_ptr<Environment>)>;
    void set_pause_callback(PauseCallback callback);

    // Log callback — invoked when a logpoint fires
    using LogCallback = std::function<void(const SourceLocation&,
                                           const std::string& message)>;
    void set_log_callback(LogCallback callback);

    // Request an async pause (thread-safe flag checked at next eval)
    void request_pause();

    // Access the call stack (for DAP stackTrace)
    const std::vector<StackFrame>& call_stack() const;

    // ─── Effect Firewall integration (Req 11.1, 11.2) ──────────────

    // Set the effect firewall for runtime enforcement at perform() sites.
    // When set, eval_perform_expression and eval_implicit_effect_call will
    // call EffectFirewall::check() before dispatching to the handler.
    void set_effect_firewall(effects::EffectFirewall* firewall);

    // ─── Effect context introspection (Req 12D) ─────────────────────

    // Get names of active effect handlers on the handler stack
    std::vector<std::string> get_active_effect_handlers() const;

    // Get the @uses annotation of the currently executing function
    std::vector<std::string> get_current_uses_annotation() const;

private:
    std::shared_ptr<Environment> env_;
    std::string source_file_;
    DebugHook debug_hook_;
    effects::EffectFirewall* effect_firewall_ = nullptr;  // non-owning, optional
    std::vector<StackFrame> call_stack_;

    // Effect sandbox: when non-empty, only listed effects are allowed
    std::vector<std::set<std::string>> allowed_effects_stack_;

    // ─── Effect handler registry (Meld-side dispatch) ───────────────
    struct EffectKey {
        std::string effect;
        std::string operation;
        bool operator==(const EffectKey& o) const { return effect == o.effect && operation == o.operation; }
    };
    struct EffectKeyHash {
        size_t operator()(const EffectKey& k) const {
            return std::hash<std::string>{}(k.effect) ^ (std::hash<std::string>{}(k.operation) << 16);
        }
    };
    std::unordered_map<EffectKey, kernel::Value, EffectKeyHash> effect_handlers_;

    // ─── Metadata side-table (kernel.meta-set/get/has) ──────────────
    // Keyed by raw pointer of the Function shared_ptr (identity-based).
    std::unordered_map<const void*, std::unordered_map<std::string, kernel::Value>> metadata_table_;

    // ─── Continuation support ───────────────────────────────────────
    std::vector<kernel::Value> resume_values_;  // stack of resume values for replay
    std::unordered_map<const void*, std::shared_ptr<Continuation>> continuation_table_;

    // ─── Native function dispatch table (kernel.call) ───────────────
    using NativeFunction = std::function<kernel::Value(const std::vector<kernel::Value>&)>;
    std::unordered_map<std::string, NativeFunction> native_functions_;

    // ─── FFI library handles ────────────────────────────────────────
    std::vector<void*> ffi_handles_;
    kernel::Value ffi_dispatch_int(const std::vector<kernel::Value>& args);
    kernel::Value ffi_dispatch_float(const std::vector<kernel::Value>& args);
    kernel::Value ffi_dispatch_string(const std::vector<kernel::Value>& args);
    kernel::Value ffi_dispatch_void(const std::vector<kernel::Value>& args);

    // ─── @uses enforcement ──────────────────────────────────────────
    std::set<std::string> pending_effects_;  // set by uses(), consumed by next func def

public:
    void register_effect_handler(const std::string& effect, const std::string& op, kernel::Value fn);

private:

    // ─── Debug state ────────────────────────────────────────────────
    bool debug_mode_ = false;
    StepMode step_mode_ = StepMode::Continue;
    std::map<BreakpointKey, BreakpointInfo> breakpoints_;
    PauseCallback pause_callback_;
    LogCallback log_callback_;
    bool pause_requested_ = false;       // Async pause flag
    size_t step_start_depth_ = 0;        // Call depth when step was initiated

    // Check breakpoints/stepping before evaluating an AST node.
    // Called at the top of each eval_* method when debug_mode_ is true.
    void check_debug_pause(const SourceLocation& loc);

    // Evaluate a condition expression string in the given environment.
    // Returns true if the result is truthy.
    bool evaluate_condition(const std::string& condition_expr,
                            std::shared_ptr<Environment> env);

    // Evaluate an expression string and return its string representation.
    std::string evaluate_to_string(const std::string& expr,
                                   std::shared_ptr<Environment> env);

    // Extract source location from a position-tagged AST node
    template<typename T>
    SourceLocation source_location(const T& node) const;

    // Visitor methods for each AST node type
    kernel::Value eval_integer_literal(const parser::ast::integer_literal& lit);
    kernel::Value eval_float_literal(const parser::ast::float_literal& lit);
    kernel::Value eval_string_literal(const parser::ast::string_literal& lit);
    kernel::Value eval_boolean_literal(const parser::ast::boolean_literal& lit);
    kernel::Value eval_identifier(const parser::ast::identifier& id);
    kernel::Value eval_val_declaration(const parser::ast::val_declaration& decl);
    kernel::Value eval_var_declaration(const parser::ast::var_declaration& decl);
    kernel::Value eval_function_definition(const parser::ast::function_definition& def);
    kernel::Value eval_function_call(const parser::ast::function_call& call);
    kernel::Value eval_binary_operation(const parser::ast::binary_operation& op);
    kernel::Value eval_unary_operation(const parser::ast::unary_operation& op);
    kernel::Value eval_list_expression(const parser::ast::list_expression& list);
    kernel::Value eval_block_expression(const parser::ast::block_expression& block);
    kernel::Value eval_lambda_expression(const parser::ast::lambda_expression& lambda);
    kernel::Value eval_return_statement(const parser::ast::return_statement& ret);

    // Effect expression evaluation — Task 7.1, 7.2 (implicit-effect-calls)
    // Requirements 3.1, 3.2, 3.3
    kernel::Value eval_perform_expression(const parser::ast::perform_expression& perform);
    kernel::Value eval_implicit_effect_call(const parser::ast::implicit_effect_call& call);
    kernel::Value eval_handle_expression(const parser::ast::handle_expression& handle);
    kernel::Value eval_assertion(const parser::ast::assertion_expression& assertion);
    kernel::Value eval_test_block(const parser::ast::test_block& test);
    kernel::Value eval_initialization_block(const parser::ast::initialization_block& init);
    kernel::Value eval_import(const parser::ast::import_declaration& imp);
    kernel::Value eval_dot_access(const parser::ast::tuple_indexing& dot);
    kernel::Value eval_array_indexing(const parser::ast::array_indexing& idx);

    // Register built-in interpolation functions (__interpolate-default__, etc.)
    void register_builtins();
    void register_native_functions();

public:
    // Produce structural debug representation of a Value (type + content)
    static std::string value_to_debug_string(const kernel::Value& val);

    // Produce indented multi-line debug representation of a Value
    static std::string value_to_pretty_string(const kernel::Value& val, int indent = 0);

private:    // Shared helper: both perform_expression and implicit_effect_call evaluate
    // to the same runtime call — EffectRuntime::perform_effect — which invokes
    // primitive_suspend + continuation.  The returned Value is the resumed value,
    // correctly threading into value bindings (val x = Effect.op(args)).
    kernel::Value eval_effect_suspend(const std::string& effect_name,
                                      const std::string& operation_name,
                                      const std::vector<boost::spirit::x3::forward_ast<parser::ast::expression>>& arguments);

    // Helpers
    kernel::Value apply_function(const kernel::Value& callee,
                                 const std::vector<kernel::Value>& args,
                                 const SourceLocation& call_site);
    kernel::Value apply_binary_op(const std::string& op,
                                  const kernel::Value& left,
                                  const kernel::Value& right,
                                  const SourceLocation& loc);
    kernel::Value apply_unary_op(const std::string& op,
                                 const kernel::Value& operand,
                                 const SourceLocation& loc);
};

} // namespace meld::interpreter
