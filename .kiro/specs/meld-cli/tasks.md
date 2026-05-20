# Implementation Plan: Meld Unified CLI

## Overview

Complete the unified `meld` CLI by implementing the AST interpreter engine in `meld-core/`, rewiring the stubbed `InterpreterModule` in `meld-cli/`, wiring the existing `CodeFormatter` to the `meld fmt` subcommand, and implementing the full debugging architecture (DAP server, debug hooks, DWARF/PDB emission, LLDB data formatters, effect context inspection). All new logic lives in `meld-core/` as libraries; `meld-cli/` modules consume them.

> **Note — Bytecode pipeline (Tasks 6–12):** Tasks 6–12 implement a custom bytecode pipeline (BytecodeGenerator, BytecodeOptimizer, BytecodeSerializer, BytecodeInterpreter) that was built during the bootstrap phase as an intermediate execution backend before the LLVM pipeline was operational. This bytecode pipeline is NOT part of the user-facing three-tier execution model (Tier 1: AST Interpreter, Tier 2: LLVM ORC JIT, Tier 3: LLVM AOT) and has no corresponding requirements in `requirements.md`. The code remains in the codebase as internal infrastructure but is superseded by the LLVM backend for Tier 2/3 execution. These tasks are retained here for historical completeness. The `_Requirements:` annotations in Tasks 6–12 reference an older numbering scheme that predates the current `requirements.md` and do not map to current requirement numbers.

> **Note — Requirement reference numbering (Tasks 13–14):** Tasks 13 and 14 were written against an earlier requirement numbering scheme. Task 13 references `4.x` which maps to current Req 6 (Code Formatter). Task 14 references `8.x` which maps to current Req 17 (Error Reporting). The criterion sub-numbers (e.g., `4.1` → Req 6.1) remain correct within their respective requirements.

## Tasks

- [x] 1. Implement AST Interpreter core (Environment + AstInterpreter)
  - [x] 1.1 Create `meld-core/include/meld/interpreter/ast_interpreter.hpp` with `Environment`, `Binding`, `InterpreterError`, `SourceLocation`, `StackFrame`, and `AstInterpreter` class declarations per the design
    - Define `Environment` with `define()`, `assign()`, `lookup()`, `has()`, `create_child()`, scope-chain via `parent_` pointer
    - Define `InterpreterError` extending `std::runtime_error` with `SourceLocation` and `std::vector<StackFrame>`
    - Define `AstInterpreter` with `evaluate_program()`, `evaluate()`, `environment()`, `set_source_file()`, and private visitor methods for each AST node type
    - _Requirements: 1.1, 1.6, 1.7, 1.8, 1.9, 1.15_

  - [x] 1.2 Implement `meld-core/src/interpreter/ast_interpreter.cpp` — Environment class
    - Implement `define()` storing `Binding{value, is_mutable}` in `bindings_` map
    - Implement `assign()` walking the scope chain, throwing `InterpreterError` if name is unbound or binding is immutable
    - Implement `lookup()` walking the scope chain from current to global, throwing `InterpreterError` if unbound
    - Implement `create_child()` returning a new `Environment` with `this` as parent
    - _Requirements: 1.6, 1.7, 1.8, 1.9_

  - [x] 1.3 Implement `AstInterpreter` literal evaluation and identifier lookup
    - Implement `eval_integer_literal` producing `kernel::Integer`
    - Implement `eval_string_literal` producing `kernel::String`
    - Implement `eval_boolean_literal` producing `kernel::Boolean`
    - Implement `eval_identifier` calling `env_->lookup()`, propagating `InterpreterError` with source location
    - Implement the top-level `evaluate()` dispatch using `std::visit` on the `expression` variant
    - _Requirements: 1.2, 1.3, 1.4, 1.5, 1.8, 1.9_

  - [x] 1.4 Implement `AstInterpreter` declarations, functions, and control flow
    - Implement `eval_val_declaration` binding name as immutable in current Environment
    - Implement `eval_var_declaration` binding name as mutable in current Environment
    - Implement `eval_function_definition` creating a `kernel::Function` closure capturing the defining Environment, parameter list, and body AST
    - Implement `eval_function_call` — evaluate callee, evaluate args left-to-right, create child Environment from closure env with parameter bindings, evaluate body
    - Implement `eval_binary_operation` delegating to `apply_binary_op` using kernel primitives (arithmetic, comparison, logical)
    - Implement `eval_list_expression` producing `kernel::Vec`
    - Implement `eval_block_expression` evaluating in a new child Environment, returning last expression value
    - Implement `eval_lambda_expression` similar to function_definition but anonymous
    - Implement `evaluate_program` iterating top-level expressions, returning last value
    - Push/pop `StackFrame` entries on `call_stack_` around function calls for error reporting
    - On runtime errors (division by zero, type mismatch), throw `InterpreterError` with source location and captured `call_stack_`
    - _Requirements: 1.2, 1.6, 1.7, 1.10, 1.11, 1.12, 1.13, 1.14, 1.15_

  - [ ]* 1.5 Write unit tests for AstInterpreter in `meld-core/tests/interpreter/ast_interpreter_test.cpp`
    - Test literal evaluation (integer, string, boolean)
    - Test val/var declarations and identifier lookup
    - Test function definition and call with closures
    - Test binary operations (arithmetic, comparison, logical)
    - Test list and block expressions
    - Test `InterpreterError` thrown for unbound identifiers, division by zero, immutable reassignment
    - Test stack trace capture on nested function call errors
    - _Requirements: 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 1.10, 1.11, 1.12, 1.13, 1.14, 1.15_

  - [x] 1.6 Add `interpreter` library target to `meld-core/BUILD.bazel`
    - Add `cc_library(name = "interpreter", srcs = ["src/interpreter/ast_interpreter.cpp"], hdrs = ["include/meld/interpreter/ast_interpreter.hpp"], includes = ["include"], deps = [":parser", ":kernel"], visibility = ["//visibility:public"])`
    - Add test target for `ast_interpreter_test.cpp` depending on `:interpreter`, `:testing`, `@googletest//:gtest_main`
    - _Requirements: 1.16_

- [x] 2. Checkpoint — AST Interpreter core
  - Ensure all tests pass, ask the user if questions arise.

- [x] 3. Rewire InterpreterModule to use AstInterpreter (meld run / meld repl)
  - [x] 3.1 Update `MeldRuntime` in `meld-cli/src/interpreter_module.cpp` to use `AstInterpreter`
    - Replace the stub `execute_internal()` with: read file → `Parser::parse_file()` → `AstInterpreter::evaluate_program()`
    - Replace the stub `execute_source()` with: `Parser::parse_file()` on source string → `AstInterpreter::evaluate_program()`
    - Bind `args` as a built-in `kernel::Vec` of `kernel::String` in the global Environment before evaluation
    - On `Parser` failure, format error as `<file>:<line>:<column>: error: <message>` and populate `ExecutionResult::errors`
    - On `InterpreterError`, format with source location and stack trace, populate `ExecutionResult::errors`
    - _Requirements: 2.1, 2.2, 2.3, 2.4_

  - [x] 3.2 Update `ReplSession` to use `AstInterpreter` with persistent Environment
    - Replace the stub `evaluate_line()` with: `Parser::parse_expression()` → `AstInterpreter::evaluate()` on a persistent `Environment`
    - Print result value with its type (e.g., `42 : Integer`)
    - On parse/runtime error, print error message and return to prompt without terminating
    - _Requirements: 2.5, 2.6, 2.7, 2.9_

  - [x] 3.3 Implement `:load <file.meld>` REPL command
    - In `handle_repl_special_command`, when command is `load`, read the file path argument, parse it, evaluate in the current session Environment via `AstInterpreter`
    - On error, print message and return to prompt
    - _Requirements: 2.8, 2.9_

  - [x] 3.4 Update `interpreter_module` Bazel target in `meld-cli/BUILD.bazel`
    - Add `"//meld-core:interpreter"` to the `deps` list of the `interpreter_module` cc_library
    - _Requirements: 2.13_

  - [ ]* 3.5 Write unit tests for InterpreterModule integration in `meld-cli/tests/interpreter_module_integration_test.cpp`
    - Test `run_file` with a valid `.meld` file producing correct output
    - Test `run_file` with parse errors producing formatted error messages
    - Test `run_source` with runtime errors producing stack traces
    - Test `ReplSession::evaluate_line` with expressions, declarations, and errors
    - Test `:load` command evaluating file in session environment
    - _Requirements: 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 2.7, 2.8, 2.9_

- [x] 4. Checkpoint — InterpreterModule integration
  - Ensure all tests pass, ask the user if questions arise.

- [x] 5. Implement AST-to-IR lowering
  - [x] 5.1 Implement or extend `ASTToIR` in `meld-core/src/compiler/ast_to_ir.cpp`
    - Implement lowering of `function_definition` → `ir::Function` with parameter list and entry `BasicBlock`
    - Implement lowering of `val_declaration` → evaluate RHS instructions + `Alloca`/`Store` pair
    - Implement lowering of `binary_operation` with `+` → `ir::Instruction` with `Opcode::Add` and operand references
    - Implement lowering of `function_call` → evaluate arguments + `Call` instruction
    - Implement lowering of `block_expression` → sequential instructions, last expression as block result
    - Implement lowering of `integer_literal` → `ConstInt` instruction
    - Use `IRBuilder` to manage current function, insert point, and temp value creation
    - _Requirements: 5.1, 5.2, 5.3, 5.4, 5.5, 5.6_

  - [ ]* 5.2 Write unit tests for AST-to-IR lowering in `meld-core/tests/compiler/ast_to_ir_lowering_test.cpp`
    - Test function_definition produces ir::Function with correct parameters and entry block
    - Test val_declaration produces Alloca+Store pair
    - Test binary_operation `+` produces Add instruction
    - Test function_call produces argument evaluation + Call instruction
    - Test integer_literal produces ConstInt
    - _Requirements: 5.1, 5.2, 5.3, 5.4, 5.5, 5.6_

- [x] 6. Implement BytecodeGenerator
  - [x] 6.1 Implement `BytecodeGenerator::generate()` and `generate_function()` in `meld-core/src/compiler/bytecode_generator.cpp`
    - `generate(const ir::Module&)` — create `BytecodeModule`, iterate functions calling `generate_function`, return module
    - `generate_function(const ir::Function&)` — create `BytecodeFunction` with name, map parameters to locals, iterate basic blocks calling `generate_block`, call `resolve_jumps`
    - _Requirements: 3.1, 3.2_

  - [x] 6.2 Implement `generate_block()` and `generate_instruction()` with all opcode handlers
    - `generate_block` — record label→address in `label_to_address_`, iterate instructions
    - `generate_instruction` — switch on `ir::Opcode`, delegate to helper methods:
      - `generate_arithmetic` for `Add`, `Sub`, `Mul`, `Div`, `Mod` → emit `ADD`, `SUB`, `MUL`, `DIV`, `MOD`
      - `generate_comparison` for `Eq`, `Ne`, `Lt`, `Le`, `Gt`, `Ge`
      - `generate_logical` for `And`, `Or`, `Not`
      - `generate_memory` for `Alloca`, `Load`, `Store`
      - `generate_control_flow` for `Branch` → `JUMP`, `CondBranch` → `JUMP_IF_TRUE`/`JUMP_IF_FALSE`, `Call` → `CALL`, `Return` → `RETURN`
      - `generate_constant` for `ConstInt`, `ConstFloat`, `ConstBool`, `ConstString`
    - Implement `resolve_jumps()` patching `pending_jumps_` with resolved `label_to_address_` values
    - Implement `get_or_create_local()` and `get_constant_index()` helpers
    - _Requirements: 3.2, 3.3, 3.4, 3.5, 3.6, 3.7_

  - [ ]* 6.3 Write unit tests for BytecodeGenerator in `meld-core/tests/compiler/bytecode_generator_test.cpp`
    - Test arithmetic IR instructions produce correct bytecode opcodes
    - Test branch/cond_branch produce JUMP/JUMP_IF_FALSE with resolved addresses
    - Test Call instruction produces CALL with function index and arg count
    - Test Return instruction produces RETURN
    - Test full function generation with correct parameter_count and local_count
    - _Requirements: 3.2, 3.3, 3.4, 3.5, 3.6, 3.7_

- [x] 7. Implement BytecodeOptimizer
  - [x] 7.1 Implement the four optimization passes in `meld-core/src/compiler/bytecode_generator.cpp`
    - `eliminate_dead_code` — remove instructions whose results are never used
    - `fold_constants` — replace `LOAD_CONST + LOAD_CONST + OP` sequences with `LOAD_CONST(result)`
    - `optimize_jumps` — collapse jump chains (JUMP→JUMP becomes single JUMP)
    - `eliminate_redundant_loads` — remove consecutive identical `LOAD_LOCAL` instructions
    - `optimize_function` calls all four passes in sequence
    - `optimize` iterates all functions in the module
    - _Requirements: 3.8_

  - [ ]* 7.2 Write unit tests for BytecodeOptimizer in `meld-core/tests/compiler/bytecode_optimizer_test.cpp`
    - Test dead code elimination removes unused instructions
    - Test constant folding replaces LOAD+LOAD+ADD with single LOAD_CONST
    - Test jump chain optimization collapses chained jumps
    - Test redundant load elimination
    - _Requirements: 3.8_

- [x] 8. Checkpoint — Bytecode generation and optimization
  - Ensure all tests pass, ask the user if questions arise.

- [x] 9. Implement Bytecode Serialization
  - [x] 9.1 Implement `BytecodeModule::serialize()` and `BytecodeModule::deserialize()` in `meld-core/src/compiler/bytecode_generator.cpp`
    - Serialize: write magic `MLDC` (4 bytes), version (2 bytes), constant pool (count + type-tagged entries), global names (count + length-prefixed UTF-8), functions (count + per-function: name, param_count, local_count, instruction bytes)
    - Implement `BytecodeFunction::serialize()` / `deserialize()` and `BytecodeInstruction::serialize()` / `deserialize()`
    - Deserialize: validate magic number and version, return error on mismatch, reconstruct module
    - _Requirements: 6.1, 6.2, 6.3, 6.4_

  - [ ]* 9.2 Write unit tests for bytecode serialization in `meld-core/tests/compiler/bytecode_serialization_test.cpp`
    - Test round-trip: serialize then deserialize produces identical module (same functions, constants, global names)
    - Test invalid magic number returns error
    - Test unsupported version returns error
    - Test instruction serialization encodes opcode as single byte + operand data
    - _Requirements: 6.1, 6.2, 6.3, 6.4_

- [x] 10. Implement BytecodeInterpreter
  - [x] 10.1 Implement `BytecodeInterpreter::execute()` and `execute_function()` in `meld-core/src/compiler/bytecode_generator.cpp`
    - `execute(const BytecodeModule&)` — locate `main` function, call `execute_function`, return exit code
    - `execute_function` — set up locals from args, iterate instructions calling `execute_instruction`
    - Implement `execute_instruction` switching on opcode:
      - `ADD` — pop two, sum, push result (and `SUB`, `MUL`, `DIV`, `MOD` similarly)
      - `CALL` — save current frame (PC, locals), set up new frame with arg values, execute callee
      - `RETURN` — restore caller frame, push return value
      - `JUMP_IF_FALSE` — pop top, jump if false
      - `LOAD_CONST_*`, `LOAD_LOCAL`, `STORE_LOCAL`, `LOAD_GLOBAL`, `STORE_GLOBAL`
      - `EQ`, `NE`, `LT`, `LE`, `GT`, `GE` comparisons
    - On stack underflow, report runtime error with instruction address and opcode
    - On unknown opcode, report runtime error identifying the invalid opcode value
    - _Requirements: 7.1, 7.2, 7.3, 7.4, 7.5, 7.6, 7.7_

  - [ ]* 10.2 Write unit tests for BytecodeInterpreter in `meld-core/tests/compiler/bytecode_interpreter_test.cpp`
    - Test ADD pops two values, pushes sum
    - Test CALL/RETURN frame management
    - Test JUMP_IF_FALSE conditional branching
    - Test stack underflow produces runtime error
    - Test unknown opcode produces runtime error
    - _Requirements: 7.1, 7.2, 7.3, 7.4, 7.5, 7.6, 7.7_

- [x] 11. Wire CompilerModule to bytecode pipeline (meld build)
  - [x] 11.1 Update `CompilerModule` in `meld-cli/src/compiler_module.cpp` for the `build` subcommand
    - On `meld build <file.meld>`: parse source → `ASTToIR::lower()` → `BytecodeGenerator::generate()` → `BytecodeModule::serialize()` → write to output file
    - On `--optimize` flag: run `BytecodeOptimizer::optimize()` before serialization
    - On `--output=<path>`: write to specified path; otherwise write to `<basename>.meldc`
    - On compilation errors: report with source locations, exit non-zero
    - _Requirements: 3.1, 3.8, 3.9, 3.10, 3.11_

  - [ ]* 11.2 Write unit tests for CompilerModule build path in `meld-cli/tests/compiler_module_build_test.cpp`
    - Test `meld build` produces `.meldc` output file
    - Test `--output` flag writes to specified path
    - Test `--optimize` flag applies optimization passes
    - Test compilation errors are reported with source locations
    - _Requirements: 3.1, 3.9, 3.10, 3.11_

- [x] 12. Checkpoint — Bytecode pipeline end-to-end
  - Ensure all tests pass, ask the user if questions arise.

- [x] 13. Wire CodeFormatter to `meld fmt` subcommand
  - [x] 13.1 Update `DevToolsModule::handle_format_command()` in `meld-cli/src/dev_tools_module.cpp`
    - Default mode (`meld fmt <file.meld>`): call `formatter_->format_file(path, options)`, write formatted output back to the same file
    - `--check` mode: call `formatter_->needs_formatting(source, options)`, exit code 0 if matches, exit code 1 if differs, do not modify file
    - `--stdout` mode: call `formatter_->format_code(source, options)`, print to stdout instead of modifying file
    - Set `FormatOptions::indent_size = 4`, ensure opening braces on same line, closing braces aligned
    - Handle long parameter lists (>80 chars) by placing each parameter on its own line
    - Ensure `imp` keyword is preserved in formatted output (never rewrite to `import`)
    - On parse error: print error, exit non-zero, do not modify original file
    - _Requirements: 6.1, 6.2, 6.3, 6.5, 6.6, 6.7, 6.8, 6.9_

  - [x] 13.2 Add `//meld-core:parser` dependency to `dev_tools_module` in `meld-cli/BUILD.bazel`
    - The formatter needs the parser to parse source to AST before pretty-printing
    - _Requirements: 6.4_

  - [ ]* 13.3 Write unit tests for formatter wiring in `meld-cli/tests/dev_tools_format_test.cpp`
    - Test default mode writes formatted output back to file
    - Test `--check` returns exit code 0 for already-formatted code, 1 for unformatted
    - Test `--stdout` prints to stdout without modifying file
    - Test round-trip property: parse(format(parse(source))) ≡ parse(source)
    - Test `imp` keyword is preserved
    - Test parse error does not modify original file
    - _Requirements: 6.1, 6.2, 6.3, 6.4, 6.8, 6.9_

- [x] 14. Implement unified error reporting
  - [x] 14.1 Ensure consistent error format across all subcommands
    - Verify parse errors in `InterpreterModule`, `CompilerModule`, and `DevToolsModule` all use `<file>:<line>:<column>: error: <message>` format
    - Verify runtime errors in `AstInterpreter` and `BytecodeInterpreter` include stack traces
    - Add file-not-found check: print `error: file not found: <path>` and exit code 1
    - Add non-`.meld` extension warning: print warning but proceed with processing
    - _Requirements: 17.1, 17.2, 17.3, 17.4_

  - [ ]* 14.2 Write unit tests for error reporting consistency in `meld-cli/tests/error_reporting_test.cpp`
    - Test parse error format matches `<file>:<line>:<column>: error: <message>`
    - Test runtime error includes stack trace
    - Test file-not-found produces correct message and exit code 1
    - Test non-`.meld` extension produces warning but continues
    - _Requirements: 17.1, 17.2, 17.3, 17.4_

- [x] 15. Create interpreter example with separate .cpp and .meld files
  - [x] 15.1 Create `meld-core/examples/interpreter-demo.meld` with pure Meld code
    - Demonstrate val/var declarations, function definitions, closures, list expressions, block expressions
    - Use `imp` keyword for any imports (never `import`)
    - Include inline comments documenting each language feature
    - _Requirements: 1.2, 1.3, 1.6, 1.7, 1.10, 1.13, 1.14_

  - [x] 15.2 Create `meld-core/examples/interpreter-demo.cpp` with C++ test harness
    - Read `interpreter-demo.meld` from file, parse with `Parser`, evaluate with `AstInterpreter`
    - Print results and validate expected outputs
    - Demonstrate error handling for runtime errors
    - _Requirements: 1.2, 1.15_

- [x] 16. Final checkpoint — Full integration
  - Ensure all tests pass, ask the user if questions arise.

- [x] 17. Add debug hooks to AstInterpreter (Req 12A foundation)
  - [x] 17.1 Add `StepMode` enum and debug hook API to `ast_interpreter.hpp`
    - Add `enum class StepMode { Continue, StepOver, StepIn, StepOut }` to the interpreter namespace
    - Add `BreakpointInfo` struct with `file`, `line`, `condition` (optional), `is_logpoint`, `log_expression` fields
    - Add public methods to `AstInterpreter`: `set_breakpoint(file, line)`, `set_conditional_breakpoint(file, line, condition)`, `set_logpoint(file, line, log_expression)`, `remove_breakpoint(file, line)`, `set_step_mode(StepMode)`, `set_pause_callback(std::function<void(const SourceLocation&, std::shared_ptr<Environment>)>)`
    - Add private members: `breakpoints_` map, `step_mode_`, `pause_callback_`, `debug_mode_` flag
    - _Requirements: 12A.1, 12A.5, 12A.6, 12A.7_

  - [x] 17.2 Implement debug hook logic in `ast_interpreter.cpp`
    - Before each AST node evaluation, check if current source location matches a breakpoint
    - For conditional breakpoints, evaluate the condition expression in the current Environment; only pause if condition is truthy
    - For logpoints, evaluate the log expression and output the result without pausing
    - When paused, invoke `pause_callback_` with current `SourceLocation` and `Environment`
    - Implement step-over: pause at next statement at same or lower call depth
    - Implement step-in: pause at next statement regardless of call depth
    - Implement step-out: pause when returning to the caller's frame
    - _Requirements: 12A.1, 12A.2, 12A.4, 12A.5, 12A.6, 12A.7_

  - [ ]* 17.3 Write unit tests for debug hooks in `meld-core/tests/interpreter/ast_interpreter_debug_test.cpp`
    - Test breakpoint hit invokes pause callback with correct location and environment
    - Test conditional breakpoint only pauses when condition is true
    - Test logpoint evaluates expression without pausing
    - Test step-over skips into function calls
    - Test step-in enters function calls
    - Test step-out returns to caller
    - _Requirements: 12A.1, 12A.5, 12A.6, 12A.7_

- [x] 18. Implement DAP Server core (Req 12)
  - [x] 18.1 Create `meld-core/include/meld/interpreter/dap_server.hpp` with DapServer class
    - Define `DapServer` class with constructor taking `AstInterpreter&` and `uint16_t port = 4711`
    - Define `start(bool wait_for_attach = false)` and `stop()` methods
    - Define internal DAP protocol handler for JSON message parsing/serialization
    - Define `DapScope` struct with `name`, `variables_reference`, `expensive` fields
    - Define `DapVariable` struct with `name`, `value`, `type`, `variables_reference` fields
    - _Requirements: 12.1, 12.2, 12.3, 12.4_

  - [x] 18.2 Implement `meld-core/src/interpreter/dap_server.cpp` — TCP listener and protocol handler
    - Implement TCP server listening on specified port (default 4711)
    - Implement DAP message framing (Content-Length header + JSON body)
    - Implement `initialize` request handler returning capabilities: `supportsConditionalBreakpoints`, `supportsLogPoints`, `supportsEvaluateForHovers`, `supportsStepBack: false`
    - Implement `configurationDone` handler
    - Implement `disconnect` handler for clean shutdown
    - _Requirements: 12.1, 12.2, 12.4, 12.5_

  - [x] 18.3 Implement DAP breakpoint and execution control handlers
    - Implement `setBreakpoints` handler: register breakpoints with `AstInterpreter`, return verified locations
    - Implement `continue` handler: set `StepMode::Continue` on interpreter
    - Implement `next` (step-over) handler: set `StepMode::StepOver`
    - Implement `stepIn` handler: set `StepMode::StepIn`
    - Implement `stepOut` handler: set `StepMode::StepOut`
    - Implement `pause` handler: set a flag to pause at next evaluation
    - Implement `threads` handler: return single thread (AST interpreter is single-threaded)
    - _Requirements: 12.5, 12.6_

  - [x] 18.4 Implement DAP inspection handlers (scopes, variables, evaluate)
    - Implement `stackTrace` handler: map `AstInterpreter::call_stack_` to DAP `StackFrame` objects with source locations
    - Implement `scopes` handler: build scopes from `Environment` chain — Local (current env), Closure (parent envs up to global), Global (root env)
    - Implement `variables` handler: iterate bindings in the requested scope, return `DapVariable` with name, value string, type string, mutability
    - For compound values (`kernel::Vec`, `kernel::Function`), return a `variablesReference` allowing expansion
    - Implement `evaluate` handler: parse expression, evaluate in paused `Environment` via `AstInterpreter`, return result value and type
    - _Requirements: 12.5, 12.7, 12A.1, 12A.2, 12A.3, 12A.4_

  - [ ]* 18.5 Write unit tests for DapServer in `meld-core/tests/interpreter/dap_server_test.cpp`
    - Test initialize response includes expected capabilities
    - Test setBreakpoints returns verified breakpoint locations
    - Test stackTrace returns correct frames with source locations
    - Test scopes returns Local, Closure, Global scopes
    - Test variables returns bindings with correct types and values
    - Test evaluate returns expression result
    - _Requirements: 12.1, 12.5, 12.6, 12.7_

  - [x] 18.6 Add `dap_server` library target to `meld-core/BUILD.bazel`
    - Add `cc_library(name = "dap_server", srcs = ["src/interpreter/dap_server.cpp"], hdrs = ["include/meld/interpreter/dap_server.hpp"], deps = [":interpreter", ":parser", ":kernel"])`
    - Add test target for `dap_server_test.cpp`
    - _Requirements: 12.1_

- [x] 19. Implement Effect Context Inspection (Req 12D)
  - [x] 19.1 Add effect context inspection to DapServer
    - Add `build_effect_context_scope()` method to `DapServer` that queries the interpreter's active effect handlers
    - Include the "Effect Context" as an additional DAP scope alongside Local, Closure, Global
    - List each active handler with its effect type (e.g., `FileSystem`, `Network`, `Console`)
    - Show the `@uses` annotation of the currently executing function
    - If inside an Actor, display Actor identity and permitted effects from `meld.toml`
    - _Requirements: 12D.1, 12D.2, 12D.3, 12D.4_

  - [x] 19.2 Add effect context introspection API to AstInterpreter
    - Add `get_active_effect_handlers()` method returning a list of active handler names and effect types
    - Add `get_current_uses_annotation()` method returning the `@uses` effects of the currently executing function
    - Add `get_actor_context()` method returning Actor identity and permissions (if applicable)
    - _Requirements: 12D.1, 12D.2, 12D.3, 12D.4, 12D.5_

- [x] 20. Wire `--debug` and `--debug-wait` flags to InterpreterModule (Req 2, criteria 11-12)
  - [x] 20.1 Update `InterpreterModule` in `meld-cli/src/interpreter_module.cpp`
    - Add `--debug` flag to argument parser; when set, create `DapServer` with the `AstInterpreter`, call `start(false)`, then execute the program
    - Add `--debug-wait` flag; when set alongside `--debug`, call `DapServer::start(true)` to block until IDE attaches
    - Add `--port <port>` flag for custom DAP port (default 4711)
    - Ensure `DapServer::stop()` is called on program completion or error
    - _Requirements: 2.11, 2.12, 12.2, 12.3, 12.4_

  - [x] 20.2 Update `interpreter_module` Bazel target in `meld-cli/BUILD.bazel`
    - Add `"//meld-core:dap_server"` to the `deps` list
    - _Requirements: 2.11_

- [x] 21. Wire `--debug` flag to CompilerModule for DWARF/PDB emission (Req 4, Req 12B)
  - [x] 21.1 Update `CompilerModule` in `meld-cli/src/compiler_module.cpp`
    - Add `--debug` flag to argument parser; when set, pass debug info options to the AOT pipeline
    - When `--debug` is set, configure LLVM to emit DWARF (Linux/macOS) or PDB (Windows) debug info
    - When neither `--debug` nor `--release` is specified, default to debug info emission
    - When `--release` is set, strip debug info from output binary
    - _Requirements: 4.3, 4.4, 12B.1, 12B.4, 12B.5_

  - [x] 21.2 Implement source-level debug info in AST-to-LLVM-IR lowering
    - In `Compiler_Frontend` / `ASTToIR`, emit LLVM `DISubprogram` for each Meld function mapping to `.meld` source locations
    - Emit `DILocalVariable` for val/var declarations
    - Map LLVM-generated function names back to original Meld function names via `DISubprogram` linkage names
    - Include type descriptions for kernel types (`kernel::Integer`, `kernel::String`, `kernel::Vec`, etc.) as LLVM debug info types
    - _Requirements: 12B.1, 12B.2, 12B.3_

  - [ ]* 21.3 Write unit tests for debug info emission in `meld-core/tests/compiler/debug_info_test.cpp`
    - Test that compiling with debug flag produces LLVM module with `DISubprogram` metadata
    - Test that function names map back to Meld source names
    - Test that source locations are correctly mapped
    - _Requirements: 12B.1, 12B.2, 12B.3_

- [x] 22. Implement LLDB/GDB Data Formatters (Req 12C)
  - [x] 22.1 Create `meld-core/tools/lldb/meld_formatters.py` — LLDB Python formatter scripts
    - Implement `kernel::Vec` formatter displaying as `vec[T] { elem0, elem1, ... }` with element count
    - Implement `kernel::Optional` formatter displaying as `some(value)` or `nil`
    - Implement `kernel::Function` formatter displaying function name, parameter count, closure/native indicator
    - Implement `kernel::String` formatter displaying string content directly
    - Implement `kernel::Integer` formatter displaying numeric value
    - Implement `kernel::Boolean` formatter displaying `true`/`false`
    - Implement `kernel::Cons` formatter displaying cons cell structure
    - Add `__lldb_init_module` entry point to auto-register all formatters
    - _Requirements: 12C.1, 12C.2, 12C.3, 12C.4, 12C.5_

  - [x] 22.2 Create `.lldbinit` configuration for auto-loading formatters
    - Create `meld-core/tools/lldb/.lldbinit` that loads `meld_formatters.py`
    - Document usage instructions for manual LLDB sessions
    - _Requirements: 12C.1_

  - [x] 22.3 Add VS Code debug configuration for auto-loading formatters
    - Document the `launch.json` configuration that sets `initCommands` to load `meld_formatters.py`
    - This enables the VS Code extension to auto-configure LLDB with Meld formatters
    - _Requirements: 12C.6_

- [x] 23. Create debugging example with separate .cpp and .meld files
  - [x] 23.1 Create `meld-core/examples/debug-demo.meld` with pure Meld code
    - Demonstrate breakpoint-friendly code: functions with local variables, nested calls, closures
    - Include effect usage with `@uses` annotations for effect context inspection demo
    - Include list operations and optional values for data formatter demo
    - _Requirements: 12.5, 12A.1, 12D.1_

  - [x] 23.2 Create `meld-core/examples/debug-demo.cpp` with C++ test harness
    - Read `debug-demo.meld`, parse, set up `AstInterpreter` with debug hooks
    - Create `DapServer` and demonstrate programmatic breakpoint setting
    - Show scope inspection and variable enumeration
    - _Requirements: 12.1, 12A.2_

- [x] 24. Checkpoint — Debugging features complete
  - Ensure all debugging tasks are structurally correct
  - Verify DAP server, debug hooks, data formatters, and effect context inspection are wired together
  - Verify `--debug`/`--debug-wait` flags work in InterpreterModule
  - Verify `--debug` flag works in CompilerModule for DWARF/PDB emission

- [x] 25. Implement Project Scaffolding (meld new / meld init)
  - [x] 25.1 Create `meld-core/include/meld/build/project_template.hpp` with `ProjectTemplate` class
    - Define `create_project(name, options)` generating directory structure, `meld.toml`, entry file, `.gitignore`
    - Define `init_project(options)` for initializing in the current directory
    - Define `ScaffoldOptions` struct with `is_lib: bool` flag
    - _Requirements: 7.1, 7.2, 7.3, 7.4_

  - [x] 25.2 Implement `meld-core/src/build/project_template.cpp`
    - `create_project`: create `<name>/` dir, write `meld.toml` with `[package]`, `[dependencies]`, `[targets]` sections, create `src/main.meld` (or `src/module.meld` with `--lib`), write `.gitignore`, run `git init`
    - `init_project`: same as above but in current directory, no parent dir creation
    - Check for existing `meld.toml` — error without overwriting if present
    - Generated `meld.toml` SHALL contain project name, version `0.1.0`, and default config sections
    - _Requirements: 7.1, 7.2, 7.3, 7.4, 7.5, 7.6_

  - [x] 25.3 Create `ScaffoldModule` in `meld-cli/src/scaffold_module.cpp`
    - Register `meld new <project_name>` and `meld init` subcommands with `CommandDispatcher`
    - Parse `--lib` flag and pass to `ProjectTemplate`
    - Route to `create_project` or `init_project` based on subcommand
    - _Requirements: 7.1, 7.2, 7.4_

  - [ ]* 25.4 Write unit tests for scaffolding in `meld-cli/tests/scaffold_module_test.cpp`
    - Test `meld new` creates correct directory structure with `meld.toml` and `src/main.meld`
    - Test `meld new --lib` creates `src/module.meld` instead
    - Test `meld init` creates files in current directory
    - Test error when `meld.toml` already exists
    - _Requirements: 7.1, 7.2, 7.3, 7.4, 7.5_

- [x] 26. Implement `meld run --watch` (file watcher re-execution)
  - [x] 26.1 Create `meld-core/include/meld/interpreter/file_watcher.hpp` with `FileWatcher` class
    - Define `FileWatcher` with `watch(path, callback)` and `stop()` methods
    - Use platform-appropriate file system notification (inotify/ReadDirectoryChangesW/kqueue)
    - Debounce rapid saves (100ms window)
    - _Requirements: 2.10_

  - [x] 26.2 Implement `meld-core/src/interpreter/file_watcher.cpp`
    - Implement file change detection loop on a background thread
    - On change detected, invoke callback with changed file path
    - Handle file deletion/rename gracefully (re-watch if file reappears)
    - _Requirements: 2.10_

  - [x] 26.3 Wire `--watch` flag in `InterpreterModule`
    - In `meld-cli/src/interpreter_module.cpp`, when `--watch` is set: execute file once, then start `FileWatcher` on the file, re-execute on each change
    - Print `[watch] reloading <file>...` on each re-execution
    - On parse/runtime error during re-execution, print error and continue watching (don't exit)
    - _Requirements: 2.10_

  - [ ]* 26.4 Write unit tests for FileWatcher in `meld-core/tests/interpreter/file_watcher_test.cpp`
    - Test callback invoked on file modification
    - Test debounce prevents multiple rapid callbacks
    - _Requirements: 2.10_

- [x] 27. Checkpoint — Scaffolding and watch mode
  - Ensure scaffolding generates correct project structure
  - Ensure `--watch` re-executes on file saves

- [x] 28. Implement Testing Framework CLI (meld test)
  - [x] 28.1 Create `meld-core/include/meld/test/test_runner.hpp` with `TestRunner` class
    - Define `TestRunner` with `discover_tests(project_path)` returning list of `TestCase` structs
    - Define `run_tests(tests, options)` executing discovered tests and returning `TestResults`
    - Define `TestOptions` with `parallel: bool`, `filter: string`, `coverage: bool`, `json_output: bool`
    - Define `TestCase` with `name`, `file`, `line`, `function_ast`
    - Define `TestResult` with `name`, `passed`, `duration`, `failure_info` (expected vs actual, source location, stack trace)
    - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5, 8.6, 8.7_

  - [x] 28.2 Implement `meld-core/src/test/test_runner.cpp`
    - `discover_tests`: parse all `.meld` files in project, find functions annotated with `@test`
    - `run_tests`: for each test, create fresh `Environment`, evaluate test function via `AstInterpreter`
    - `--filter`: apply regex to test names, skip non-matching
    - `--parallel`: use `std::async` / thread pool to run tests concurrently (each test gets its own `AstInterpreter` + `Environment`)
    - `--coverage`: instrument AST evaluation to track which nodes are visited, produce coverage report
    - Collect pass/fail results, timing, and failure details
    - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5, 8.6_

  - [x] 28.3 Create `TestModule` in `meld-cli/src/test_module.cpp`
    - Register `meld test` subcommand with `CommandDispatcher`
    - Parse `--parallel`, `--filter <regex>`, `--coverage`, `--json` flags
    - Delegate to `TestRunner`, format and print results
    - Human-readable output by default; JSON when `--json` specified
    - Exit code 0 on all pass, non-zero on any failure
    - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5, 8.6, 8.7_

  - [ ]* 28.4 Write unit tests for TestRunner in `meld-core/tests/test/test_runner_test.cpp`
    - Test discovery finds `@test` annotated functions
    - Test filter regex excludes non-matching tests
    - Test pass/fail results are correctly reported
    - Test JSON output format
    - _Requirements: 8.1, 8.2, 8.6, 8.7_

- [x] 29. Implement Package Management (meld mod)
  - [x] 29.1 Create `meld-core/include/meld/build/package_resolver.hpp` with `PackageResolver` class
    - Define `PackageResolver` with `fetch(deps, lock)`, `update(deps)`, `clean()` methods
    - Define `Dependency` struct with `name`, `git_url`, `version_spec`
    - Define `ResolvedDependency` with `name`, `git_url`, `resolved_commit`, `cache_path`
    - Define `LockFile` model with `version`, `packages` list (each with `name`, `git_url`, `resolved_commit`, `tag`, `integrity`)
    - Cache directory: `~/.meld/cache/`
    - _Requirements: 9.1, 9.2, 9.3, 9.4, 9.5, 9.6_

  - [x] 29.2 Implement `meld-core/src/build/package_resolver.cpp`
    - `fetch`: for each dependency in `meld.toml`, clone/fetch Git repo to `~/.meld/cache/<name>/<commit>/`, respect `meld.lock` for pinned commits
    - `update`: resolve latest allowed tags/commits for each dependency, rewrite `meld.lock`
    - `clean`: remove all contents of `~/.meld/cache/`
    - On network error, invalid URL, or missing tag: report specific dependency and failure reason
    - On success: print summary of fetched dependencies and resolved versions
    - _Requirements: 9.1, 9.2, 9.3, 9.4, 9.5, 9.6_

  - [x] 29.3 Create `PackageModule` in `meld-cli/src/package_module.cpp`
    - Register `meld mod fetch`, `meld mod update`, `meld mod clean` subcommands
    - Parse `meld.toml` for dependency declarations, load `meld.lock` if present
    - Delegate to `PackageResolver` methods
    - _Requirements: 9.1, 9.2, 9.3_

  - [ ]* 29.4 Write unit tests for PackageResolver in `meld-core/tests/build/package_resolver_test.cpp`
    - Test fetch with lock file uses pinned commits
    - Test update resolves latest versions and writes lock file
    - Test clean removes cache directory
    - Test error reporting for invalid dependency URLs
    - _Requirements: 9.1, 9.2, 9.3, 9.4, 9.5_

- [x] 30. Checkpoint — Testing and package management
  - Ensure `meld test` discovers and runs `@test` functions
  - Ensure `meld mod` fetch/update/clean work correctly

- [x] 31. Implement Effect Security Audit (meld audit)
  - [x] 31.1 Create `meld-core/include/meld/effects/effect_checker.hpp` with `EffectChecker` class
    - Define `EffectChecker` with `build_effect_tree(modules)` returning `EffectTree`
    - Define `EffectTree` with `root_package`, `nodes` (each with `package_name`, `direct_effects`, `transitive_effects`, `allowed_effects`, `violations`)
    - Define `EffectViolation` with `package_name`, `effect_name`, `source_location`
    - _Requirements: 10.1, 10.2, 10.4, 10.5_

  - [x] 31.2 Implement `meld-core/src/effects/effect_checker.cpp`
    - Parse project AST and all transitive dependency ASTs
    - Walk ASTs to find `perform()` calls and `@uses` annotations
    - Build tree distinguishing direct vs transitive effects per package
    - Compare against `meld.toml` `allow` arrays, flag violations
    - _Requirements: 10.1, 10.2, 10.4, 10.5_

  - [x] 31.3 Create `AuditModule` in `meld-cli/src/audit_module.cpp`
    - Register `meld audit` subcommand
    - Parse `--json` flag for machine-readable output
    - Delegate to `EffectChecker::build_effect_tree()`
    - Display effect tree in human-readable format (default) or JSON
    - _Requirements: 10.1, 10.2, 10.3_

  - [ ]* 31.4 Write unit tests for EffectChecker in `meld-core/tests/effects/effect_checker_test.cpp`
    - Test effect tree correctly identifies direct and transitive effects
    - Test violations flagged when effects not in allow list
    - Test JSON output format
    - _Requirements: 10.1, 10.4, 10.5_

- [x] 32. Implement Effect Firewall cross-tier enforcement (Req 11)
  - [x] 32.1 Create `meld-core/include/meld/effects/effect_firewall.hpp` with `EffectFirewall` class
    - Define `EffectFirewall` with `load_permissions(config)`, `check(effect, module, call_site)`, `get_violations(module)`
    - Single implementation shared by all three tiers
    - _Requirements: 11.1, 11.2, 11.3, 11.4, 11.5_

  - [x] 32.2 Implement `meld-core/src/effects/effect_firewall.cpp`
    - `load_permissions`: parse `meld.toml` `allow` arrays and `@uses` annotations
    - `check`: verify effect is permitted by both `@uses` and `meld.toml` allow list
    - On violation: report effect name, offending module, and call site location
    - _Requirements: 11.1, 11.2, 11.3, 11.4_

  - [x] 32.3 Wire EffectFirewall into AstInterpreter (Tier 1)
    - In `AstInterpreter::eval_perform_expression`, call `EffectFirewall::check()` before dispatching to handler
    - On violation, throw `InterpreterError` with effect violation details
    - _Requirements: 11.1, 11.2_

  - [x] 32.4 Wire EffectFirewall into Compiler_Frontend (Tiers 2 & 3)
    - In AST-to-LLVM-IR lowering, emit calls to `EffectFirewall::check()` at `perform()` sites
    - Same runtime check function used by both ORC JIT and AOT paths
    - _Requirements: 11.1, 11.2, 11.5_

  - [ ]* 32.5 Write unit tests for EffectFirewall in `meld-core/tests/effects/effect_firewall_test.cpp`
    - Test permitted effect passes check
    - Test unper
mitted effect throws/reports violation with details
    - Test cross-tier consistency: same effect checked identically in interpreter and compiler paths
    - _Requirements: 11.1, 11.2, 11.3, 11.4, 11.5_

- [x] 33. Checkpoint — Effect audit and firewall
  - Ensure `meld audit` produces correct effect trees
  - Ensure EffectFirewall rejects unauthorized effects in all tiers

- [x] 34. Implement ORC JIT Dev Server (meld dev)
  - [x] 34.1 Create `meld-core/include/meld/compiler/orc_jit_engine.hpp` with `OrcJitEngine` class
    - Define `OrcJitEngine` with `initialize()`, `load_module(bitcode)`, `hot_swap_module(name, bitcode)`, `execute(entry_point)`
    - Define `JitError` for module load/verification failures
    - _Requirements: 3.1, 3.2, 3.4_

  - [x] 34.2 Implement `meld-core/src/compiler/orc_jit_engine.cpp`
    - Initialize LLVM ORC JIT with lazy compilation support
    - `load_module`: add LLVM bitcode module to JIT session
    - `hot_swap_module`: replace an existing module with updated bitcode, achieving <200ms swap latency
    - `execute`: look up and call the entry point function
    - On module verification failure, report error and retain previous working module
    - _Requirements: 3.1, 3.2, 3.3, 3.4_

  - [x] 34.3 Create `DevServerModule` in `meld-cli/src/dev_server_module.cpp`
    - Register `meld dev` subcommand
    - Parse `--port <port>` flag for IPC
    - Start `OrcJitEngine`, compile project to bitcode, load into JIT
    - Start `FileWatcher` on project source files
    - On file change: recompile affected module → `hot_swap_module`
    - Enforce Effect Firewall identically to other tiers
    - _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6_

  - [ ]* 34.4 Write unit tests for OrcJitEngine in `meld-core/tests/compiler/orc_jit_engine_test.cpp`
    - Test module load and execution
    - Test hot-swap replaces module correctly
    - Test verification failure retains previous module
    - _Requirements: 3.1, 3.2, 3.4_

- [x] 35. Implement LSP Server (meld lsp)
  - [x] 35.1 Create `meld-core/include/meld/lsp/lsp_server.hpp` with `LspServer` class
    - Define `LspServer` with `start(transport)` where transport is `stdio` or `tcp`
    - Define handlers for: `textDocument/diagnostics`, `textDocument/completion`, `textDocument/definition`, `textDocument/references`, `textDocument/formatting`
    - _Requirements: 14.1, 14.2, 14.3_

  - [x] 35.2 Implement `meld-core/src/lsp/lsp_server.cpp`
    - Implement LSP JSON-RPC message framing (Content-Length header + JSON body)
    - Implement `initialize` handler returning server capabilities
    - Implement `textDocument/didOpen` and `textDocument/didChange` for document sync
    - Implement `textDocument/diagnostics` using Parser for error detection
    - Implement `textDocument/completion` using symbol table from parsed AST
    - Implement `textDocument/definition` using AST identifier resolution
    - Implement `textDocument/references` using AST symbol search
    - Implement `textDocument/formatting` delegating to `CodeFormatter`
    - _Requirements: 14.1, 14.2_

  - [x] 35.3 Create `LspModule` in `meld-cli/src/lsp_module.cpp`
    - Register `meld lsp` subcommand
    - Parse `--stdio` flag (default) for stdio transport
    - Delegate to `LspServer::start()`
    - _Requirements: 14.1, 14.3_

  - [ ]* 35.4 Write unit tests for LspServer in `meld-core/tests/lsp/lsp_server_test.cpp`
    - Test initialize response includes expected capabilities
    - Test diagnostics returned for parse errors
    - Test completion returns symbols from current scope
    - _Requirements: 14.1, 14.2_

- [x] 36. Implement MCP Server (meld mcp)
  - [x] 36.1 Create `meld-core/include/meld/mcp/mcp_server.hpp` with `McpServer` class
    - Define `McpServer` with `start(transport)` where transport is `stdio`
    - Define tool handlers for AST access, symbol table queries, type information
    - _Requirements: 13.1, 13.2, 13.3_

  - [x] 36.2 Implement `meld-core/src/mcp/mcp_server.cpp`
    - Implement MCP JSON-RPC message framing
    - Implement `initialize` handler returning server capabilities and tool list
    - Implement tools for: reading AST representations, querying symbol tables, retrieving type information
    - _Requirements: 13.1, 13.2_

  - [x] 36.3 Create `McpModule` in `meld-cli/src/mcp_module.cpp`
    - Register `meld mcp` subcommand
    - Parse `--stdio` flag for stdio transport
    - Delegate to `McpServer::start()`
    - _Requirements: 13.1, 13.3_

  - [ ]* 36.4 Write unit tests for McpServer in `meld-core/tests/mcp/mcp_server_test.cpp`
    - Test initialize response includes tool list
    - Test AST access tool returns valid AST representation
    - _Requirements: 13.1, 13.2_

- [x] 37. Checkpoint — Dev server, LSP, and MCP
  - Ensure `meld dev` starts ORC JIT with hot-reload
  - Ensure `meld lsp` starts and responds to LSP requests
  - Ensure `meld mcp` starts and exposes project tools

- [x] 38. Implement Shell Integration and Global Flags (Req 15)
  - [x] 38.1 Create `ShellModule` in `meld-cli/src/shell_module.cpp`
    - Register `meld completion bash` and `meld completion zsh` subcommands
    - Generate Bash completion script listing all subcommands and their flags
    - Generate Zsh completion script with subcommand descriptions
    - Output scripts to stdout for shell eval
    - _Requirements: 15.1, 15.2_

  - [x] 38.2 Add global flags to `CliCore` in `meld-cli/src/cli_core.cpp`
    - Add `--json` flag: set output mode to JSON for all subcommands
    - Add `--quiet` flag: suppress informational output, print only errors
    - Add `--verbose` flag: print detailed diagnostic and progress information
    - Pass flags to all module `execute()` calls via `CommandArgs`
    - _Requirements: 15.3, 15.4, 15.5_

  - [ ]* 38.3 Write unit tests for shell completions in `meld-cli/tests/shell_module_test.cpp`
    - Test Bash completion script contains all subcommands
    - Test Zsh completion script contains subcommand descriptions
    - _Requirements: 15.1, 15.2_

- [x] 39. Implement Help and Version (Req 16)
  - [x] 39.1 Create `HelpModule` in `meld-cli/src/help_module.cpp`
    - Register `meld help` and `meld version` subcommands
    - `meld help`: print summary of all subcommands with brief descriptions
    - `meld help <subcommand>`: print detailed usage, flags, and examples for the subcommand
    - `meld version`: print Meld version, LLVM version, and build metadata
    - `meld` with no args: print help summary (same as `meld help`)
    - _Requirements: 16.1, 16.2, 16.3, 16.4_

  - [ ]* 39.2 Write unit tests for HelpModule in `meld-cli/tests/help_module_test.cpp`
    - Test help output lists all subcommands
    - Test subcommand help includes flags and examples
    - Test version output includes Meld and LLVM versions
    - _Requirements: 16.1, 16.2, 16.3_

- [x] 40. Final checkpoint — Full CLI coverage
  - Ensure all subcommands from CLI_SPEC_UPDATE.md are implemented
  - Ensure all requirements (1–17) have corresponding tasks (Req 18 superseded by Req 20; Reqs 19–26 covered in Tasks 44–60)
  - Verify Effect Firewall enforcement across all tiers
  - Verify consistent error reporting across all subcommands

- [x] 41. Implement Compiled Debug Attach (meld debug attach) (Req 18) — SUPERSEDED by Task 46
  - > **SUPERSEDED:** Req 18 has been replaced by Req 20 (`meld debug` Unified Orchestrator). Existing implementation from tasks 41.1–41.3 is preserved and will be refactored into the unified `meld debug` command in Task 46. Tests (41.4) should target the new Req 20 interface.

  - [x] 41.1 Create `DebugAttachModule` in `meld-cli/include/meld/cli/debug_attach_module.hpp` — _migrating to Task 46_
  - [x] 41.2 Implement `meld-cli/src/debug_attach_module.cpp` — _migrating to Task 46_
  - [x] 41.3 Implement `--dap` mode for compiled debug attach — _migrating to Task 46_
  - [ ]* 41.4 Write unit tests — _replaced by Task 46.5_

- [x] 42. Implement VS Code Seamless Debug Routing (moved to vscode-meld Req 9)
  - [x] 42.1 Create VS Code extension debug adapter factory
    - Create `vscode-meld/src/debugAdapterFactory.ts` with `MeldDebugAdapterDescriptorFactory` implementing `vscode.DebugAdapterDescriptorFactory`
    - In `resolveDebugAdapterDescriptor`: read `launch.json` `mode` field
    - If `mode === "interpret"` (or absent/default): spawn `meld run --debug --debug-wait --port <port>`, return `new vscode.DebugAdapterServer(port)`
    - If `mode === "compiled"`: delegate to CodeLLDB or `meld debug attach --dap`, with `initCommands` loading `meld_formatters.py`
    - _Requirements: vscode-meld Req 9.1, 9.2, 9.3_

  - [x] 42.2 Create default `launch.json` template and debug configuration provider
    - Create `vscode-meld/src/debugConfigProvider.ts` with `MeldDebugConfigurationProvider`
    - Provide two default configurations:
      - `"Meld: Interpret"`: `{ "type": "meld", "request": "launch", "mode": "interpret", "program": "${file}" }`
      - `"Meld: Compiled"`: `{ "type": "meld", "request": "launch", "mode": "compiled", "program": "${workspaceFolder}/build/${workspaceFolderBasename}" }`
    - Auto-detect: if `build/` contains a compiled binary, suggest the compiled configuration
    - _Requirements: vscode-meld Req 9.4, 9.5_

  - [x] 42.3 Implement breakpoint mapping for both modes
    - In interpret mode: send breakpoints to the AST interpreter DAP server as-is (file + line)
    - In compiled mode: map `.meld` file breakpoints to DWARF source breakpoints via the LLDB/CodeLLDB backend
    - Ensure breakpoints set in `.meld` files work correctly in both modes
    - _Requirements: vscode-meld Req 9.6_

  - [x] 42.4 Configure LLDB formatter auto-loading for compiled debug
    - In the compiled debug configuration, add `initCommands` to the launch config:
      ```json
      "initCommands": ["command script import ${extensionPath}/formatters/meld_formatters.py"]
      ```
    - Bundle `meld_formatters.py` with the VS Code extension package
    - Ensure kernel types (`Vec`, `Optional`, `Function`, `String`) display correctly in the Variables panel
    - _Requirements: vscode-meld Req 9.7_

  - [ ]* 42.5 Write tests for VS Code extension debug routing in `vscode-meld/tests/debugAdapter.test.ts`
    - Test interpret mode spawns `meld run --debug --debug-wait`
    - Test compiled mode delegates to LLDB with formatter init commands
    - Test default launch.json template contains both configurations
    - _Requirements: vscode-meld Req 9.1, 9.2, 9.3, 9.4_

- [x] 43. Checkpoint — Debug attach and VS Code routing
  - Ensure `meld debug attach <pid>` launches correct debugger with formatters loaded
  - Ensure `--dap` mode wraps debugger for IDE integration
  - Ensure VS Code extension routes to correct backend based on launch mode
  - Ensure breakpoints work in both interpret and compiled modes

- [x] 44. Implement `--agent-test` CLI flag (Req 19)
  - [x] 44.1 Add `--agent-test` flag to `meld test` and `meld run` subcommands
    - Add `--agent-test` flag to argument parser in `TestModule` and `InterpreterModule`
    - When active, activate the language runtime's Agent-Test mode (meld-core Req 143) before user code executes
    - Print notice to stderr: `[agent-test] Deterministic mode active: time=virtual, random=seed(0), scheduling=deterministic`
    - Reject `--agent-test` on `meld build` (runtime mode, not compilation mode)
    - _Requirements: 19.1, 19.2, 19.3, 19.8_

  - [x] 44.2 Implement execution trace output
    - With `meld test --agent-test`, write structured trace to `<project>/.meld/traces/<test-name>.trace.json` after each test
    - With `meld run --agent-test`, write trace to stdout when `--json` is also specified, or to `<project>/.meld/traces/run.trace.json` otherwise
    - _Requirements: 19.4, 19.5_

  - [x] 44.3 Ensure composability with other flags
    - Verify `--agent-test` composes with `--filter`, `--parallel`, `--debug`, `--json`
    - When combined with `--parallel`, deterministic actor scheduling ensures reproducible results regardless of system load
    - _Requirements: 19.6, 19.7_

  - [ ]* 44.4 Write unit tests for `--agent-test` flag
    - Test flag activates Agent-Test mode in runtime
    - Test stderr notice is printed
    - Test trace file is written to correct location
    - Test flag rejected on `meld build`
    - Test composability with `--parallel` and `--filter`
    - _Requirements: 19.1, 19.2, 19.3, 19.4, 19.6, 19.7_

- [x] 45. Checkpoint — Agent-Test mode CLI
  - Ensure `--agent-test` flag works on `meld test` and `meld run`
  - Ensure execution traces are written correctly
  - Ask the user if questions arise.

- [x] 46. Implement `meld debug` Unified Orchestrator (Req 20)
  - [x] 46.1 Refactor `DebugAttachModule` into `DebugOrchestratorModule` in `meld-cli/include/meld/cli/debug_orchestrator_module.hpp`
    - Define `DebugOrchestratorModule` extending `BaseCommandHandler`
    - Define `DebugOptions` struct with: `mode` (run/attach), `binary_path`, `pid`, `debugger` (lldb/gdb/auto), `breakpoints` (list of `file:line`), `sandbox` (bool), `dap_mode` (bool), `dap_port` (uint16, default 4711), `extra_args` (argv after `--`)
    - Register `meld debug` subcommand with `--run <binary>`, `--attach <pid>`, `--debugger`, `--break`, `--sandbox`, `--dap`, `--port` flags
    - _Requirements: 20.1, 20.2, 20.4, 20.5, 20.7, 20.8_

  - [x] 46.2 Implement `meld-cli/src/debug_orchestrator_module.cpp` — core debugger launch
    - Reuse existing `detect_debugger()` and `spawn_debugger()` logic from Task 41
    - `--run <binary>`: fork/exec debugger with run command (`lldb -- <binary> <args>` or `gdb --args <binary> <args>`)
    - `--attach <pid>`: fork/exec debugger with attach flags (`lldb -p <pid>` or `gdb -p <pid>`)
    - Auto-load Meld LLDB Python formatter scripts before handing control to the debugger
    - Translate `--break <file:line>` flags into debugger-specific breakpoint commands (`breakpoint set --file <file> --line <line>` for LLDB, `break <file>:<line>` for GDB) applied before execution begins
    - Pass through all standard debugger commands after setup
    - _Requirements: 20.1, 20.2, 20.3, 20.4, 20.5, 20.11_

  - [x] 46.3 Implement `.mdebug` sidecar resolution and loading
    - Read the target binary's `.note.meld` Tombstone section to extract `debug_id`
    - Call the daemon's `resolve_debug_sidecar(debug_id)` API to locate the `.mdebug` sidecar
    - If found, decompress the sidecar (zstd), extract the DWARF section, and load it into the debugger session (`target symbols add <path>` for LLDB, `add-symbol-file` for GDB)
    - If no sidecar found and binary lacks inline debug symbols, print warning: `warning: target binary lacks debug symbols — consider rebuilding with 'meld build --debug' or 'meld build --release' (for .mdebug sidecar)`
    - _Requirements: 20.6, 20.9_

  - [x] 46.4 Implement `--sandbox` and `--dap` modes
    - `--sandbox`: read the binary's `.meld` manifest, construct `SandboxConfig` from its effect map, launch the binary within an SRT sandbox before attaching the debugger
    - `--dap`: start a DAP server on `--port` wrapping LLDB-DAP or GDB/MI, allowing IDE clients to attach for graphical debugging
    - _Requirements: 20.7, 20.8_

  - [x] 46.5 Write unit tests for DebugOrchestratorModule in `meld-cli/tests/debug_orchestrator_module_test.cpp`
    - Test `--run` mode launches debugger with correct binary and args
    - Test `--attach` mode attaches to specified PID
    - Test `--debugger` flag overrides auto-detection
    - Test `--break` flags are translated to correct debugger commands
    - Test `.mdebug` sidecar resolution is attempted when Tombstone contains `debug_id`
    - Test warning printed when no debug symbols or sidecar found
    - Test error message when PID doesn't exist (attach mode)
    - Test `--sandbox` constructs SandboxConfig from manifest
    - _Requirements: 20.1, 20.2, 20.4, 20.5, 20.6, 20.7, 20.9, 20.10_

- [x] 47. Checkpoint — `meld debug` Unified Orchestrator
  - Ensure `meld debug --run` and `meld debug --attach` work with correct debugger selection
  - Ensure `.mdebug` sidecar resolution loads shadow symbols
  - Ensure `--sandbox` enforces SRT policy during debug sessions
  - Ensure `--dap` mode wraps debugger for IDE integration
  - Ensure no conflict with `meld run --debug` (interpreted DAP)
  - Ask the user if questions arise.

- [x] 48. Implement `melds` Production Supervisor Binary (Req 21)
  - [x] 48.1 Create `cmd/melds/` directory and BUILD.bazel target
    - Create `cmd/melds/main.cpp` with `supervisor_main` entry point
    - Define `SupervisorConfig` struct with `offline_mode` (bool, default true), `isolation` (string, default "process"), `trust_root` (path to `trusted_root.json`)
    - Configure as statically linked C++20 binary — must add less than 50ms to startup time (excluding VM boot)
    - Deps: `//meld-manifest:manifest`, `//meld-manifest:tombstone`, `//meld-manifest:verifier`, `//meld-manifest:sandbox_provider` — NO deps on `//meld-core`, `//meld-cli`, or LLVM
    - Parse CLI args: `melds <binary_path> [args...]` with `--offline` (default) / `--online`, `--isolation=[process|microvm]`
    - _Requirements: 21.1, 21.2, 21.5, 21.7, 21.8_

  - [x] 48.2 Implement `melds` execution chain
    - Read `.note.meld` Tombstone from binary
    - Verify Combined Integrity Hash against configured trust root (offline by default using `trusted_root.json`; `--online` queries Rekor for revocation)
    - On verification failure: print `INTEGRITY_FAILURE: <details>` to stderr, exit 127
    - Read co-located `.meld` manifest, extract effect policy (`@effect` permissions)
    - Select `SandboxProvider` based on `--isolation` flag or `meld.toml` `[execution]` section (default: `process` → MeldSRTProvider)
    - Translate effect policy into sandbox configuration, execute binary via `execvp` replacing the current process
    - Recommended as `ENTRYPOINT` for production Docker containers: `ENTRYPOINT ["melds", "./app.bin"]`
    - _Requirements: 21.3, 21.4, 21.7, 21.8, 21.10_

  - [x] 48.3 Implement audit logging and static execution mode
    - Log Sigstore Identity (signer OIDC identity) and Effect Policy to structured audit stream on each process launch
    - Implement "Static Execution" mode: pre-cache sandbox policy for a specific binary, achieving near-zero overhead on subsequent invocations
    - _Requirements: 21.5, 21.6, 21.9_

  - [ ]* 48.4 Write unit tests for `melds`
    - Test execution chain with valid signed binary (Tombstone → verify → manifest → sandbox → execvp)
    - Test INTEGRITY_FAILURE on tampered binary (hash mismatch, missing manifest, untrusted root)
    - Test offline vs online verification mode selection
    - Test provider selection from `--isolation` flag (process vs microvm)
    - Test audit log output format (identity + effects)
    - Test static execution mode caching
    - _Requirements: 21.3, 21.4, 21.5, 21.6, 21.7, 21.8, 21.9_

- [x] 49. Enhance `meld run` with Daemon Handshake and Shadow Symbols (Req 2.14–2.16)
  - [x] 49.1 Implement daemon handshake in InterpreterModule
    - When `meld run <binary>` is invoked with a compiled binary (not `.meld` source), connect to `meldd` and call `check_binary_freshness(binary_path)`
    - If binary is stale, wait for rebuild to complete; report build failures to user
    - If daemon is not running, skip freshness check and proceed with execution
    - _Requirements: 2.14_

  - [x] 49.2 Implement shadow symbol resolution
    - When executing a compiled binary with a Tombstone containing `debug_id`, call daemon's `resolve_debug_sidecar(debug_id)` API
    - If `.mdebug` sidecar found, map shadow symbols to the execution session for crash analysis
    - _Requirements: 2.15_

  - [x] 49.3 Implement AST-precision crash traces
    - If a compiled binary crashes within the sandbox, capture crash state (signal, PC, registers)
    - Use the `.mdebug` sidecar's AST-to-PC index to map the crash PC to an `ast_selector`
    - Format the trace for both human consumption (file:line + AST node description) and AI agent consumption (structured JSON with `ast_selector`)
    - _Requirements: 2.16_

  - [x] 49.4 Write unit tests for daemon handshake and crash traces
    - Test freshness check triggers rebuild when binary is stale
    - Test graceful fallback when daemon is not running
    - Test shadow symbol resolution with valid `.mdebug` sidecar
    - Test AST-precision trace formatting
    - _Requirements: 2.14, 2.15, 2.16_

- [x] 50. Implement `meld run --strict` Mode (Req 2.17)
  - [x] 50.1 Add `--strict` flag to InterpreterModule
    - When `meld run --strict <binary>` is invoked, delegate execution to the `melds` production supervisor binary
    - Locate `melds` on `$PATH` or relative to the `meld` binary installation directory
    - Pass through all arguments after the binary path to `melds`
    - This applies production-grade Sigstore verification and sandbox enforcement locally for testing purposes
    - If `melds` binary is not found, print `error: melds binary not found — install the production supervisor or add it to $PATH` and exit non-zero
    - _Requirements: 2.17_

  - [ ]* 50.2 Write unit tests for `--strict` mode
    - Test that `--strict` invokes `melds` with correct arguments
    - Test error message and exit code when `melds` binary not found
    - Test argument passthrough to `melds`
    - _Requirements: 2.17_

- [x] 51. Implement VFS Bridge Mode (Req 22)
  - [x] 51.1 Add `--vfs` flag to InterpreterModule
    - When `meld run --vfs <file_or_binary>` is invoked, set `SandboxConfig.vfs_mode = true`
    - Create a memory-only tmpfs mount (Linux) or memory-backed mount (macOS) as the working directory
    - Allow reads from real filesystem (source tree, dependencies) per effect policy; redirect all write operations to the memory-only mount
    - Destroy the tmpfs mount after the process exits
    - _Requirements: 22.1, 22.2, 22.3_

  - [x] 51.2 Implement `--vfs-output` for selective file extraction
    - When `--vfs-output <path>` is specified, copy specified output files from tmpfs to real filesystem after execution completes
    - Default behavior (no `--vfs-output`): discard all VFS contents on exit
    - _Requirements: 22.4_

  - [x] 51.3 Ensure VFS composability with other flags
    - Verify `--vfs` composes with `--agent-test`, `--strict`, `--debug`, and `--isolation`
    - When `--vfs` + `--strict`: delegate to `melds` with VFS enforcement, testing full production sandbox with ephemeral writes
    - Integrate with `SandboxProvider` interface by passing `vfs_mode` boolean in `SandboxConfig`
    - _Requirements: 22.5, 22.6, 22.7_

  - [ ]* 51.4 Write unit tests for VFS bridge
    - Test tmpfs creation and cleanup on process exit
    - Test write redirection to tmpfs (writes don't reach real filesystem)
    - Test read-through to real filesystem per effect policy
    - Test `--vfs-output` copies specified files to real filesystem
    - Test composability with `--agent-test`, `--strict`, and `--debug`
    - _Requirements: 22.1, 22.2, 22.3, 22.4, 22.5, 22.7_

- [x] 52. Checkpoint — Runtime layer additions
  - Ensure `melds` binary builds as standalone C++20 with no LLVM/meld-core deps
  - Ensure `melds` verifies Sigstore bundles and sandboxes correctly (offline and online modes)
  - Ensure `--strict` delegates to `melds` and reports error when `melds` not found
  - Ensure VFS bridge creates tmpfs, redirects writes, and cleans up on exit
  - Ensure `--vfs-output` extracts specified files from tmpfs
  - Ask the user if questions arise.

- [x] 53. Implement Binary Signing CLI (`meld sign`) (Req 23)
  - [x] 53.1 Create `SignModule` in `meld-cli/include/meld/cli/sign_module.hpp` and `meld-cli/src/sign_module.cpp`
    - Define `SignModule` extending `BaseCommandHandler`
    - Define `SignOptions` struct with `key_path`, `sigstore` (bool, default true), `fulcio_url`, `rekor_url`
    - Define `VerifyOptions` struct with `offline` (bool, default true), `online` (bool)
    - Register `meld sign` subcommand with `CommandDispatcher`
    - Parse flags: `--key <path>`, `--sigstore`, `--verify`, `--bundle`, `--offline`, `--online`, `--fulcio-url <url>`, `--rekor-url <url>`, `--json`
    - Implement `load_signing_config()` merging CLI flags with `meld.toml` `[signing]` section defaults
    - _Requirements: 23.1, 23.3, 23.11, 23.12, 23.13_

  - [x] 53.2 Implement `handle_sign()` — sign a compiled binary
    - Locate co-located `.meld` manifest sidecar; error if not found (Req 23.9)
    - Validate manifest's `code_hash` matches actual binary content; error on mismatch (Req 23.8)
    - Compute Combined Integrity Hash: `H_total = Hash(code_hash + manifest_hash + debug_id)`
    - Delegate to `meldn` notary library: sign with Sigstore (default) or private key (`--key`)
    - Embed signed Tombstone in binary's `.note.meld` section
    - Print signing result (human-readable or JSON with `--json`)
    - _Requirements: 23.1, 23.2, 23.3, 23.8, 23.9, 23.13_

  - [x] 53.3 Implement `handle_verify()` — verify binary integrity
    - Read `.note.meld` Tombstone from binary
    - Read co-located `.meld` manifest
    - Recompute `code_hash` from binary, `manifest_hash` from manifest
    - Verify `signature_blob` against Combined Integrity Hash
    - In `--offline` mode: verify Sigstore Bundle's SET against bundled trust root without network
    - In `--online` mode: re-query Rekor transparency log for revocation status
    - Print pass/fail report with specific check details; exit non-zero on failure
    - _Requirements: 23.4, 23.5, 23.6, 23.10, 23.13_

  - [x] 53.4 Implement `handle_bundle()` — prepare air-gapped bundle
    - Fetch Rekor SETs and Fulcio certificates for the signed binary
    - Embed them into the Tombstone as a self-contained Sigstore Bundle
    - Print bundle status (human-readable or JSON)
    - _Requirements: 23.7, 23.13_

  - [x] 53.5 Add `sign_module` Bazel target in `meld-cli/BUILD.bazel`
    - Add `cc_library` for `sign_module` with deps on `//meld-manifest:notary`, `//meld-manifest:tombstone`, `//meld-manifest:manifest`
    - Add `sign_module` to the `meld` binary's module list in `main.cpp`
    - _Requirements: 23.1_

  - [ ]* 53.6 Write unit tests for SignModule in `meld-cli/tests/sign_module_test.cpp`
    - Test sign command locates manifest and produces signed Tombstone
    - Test error when manifest not found
    - Test error when code_hash mismatch
    - Test verify command reports pass for valid binary
    - Test verify command reports failure details for tampered binary
    - Test `--offline` vs `--online` verification mode selection
    - Test `--json` output format
    - Test `--key` flag selects private key signing
    - Test `--fulcio-url` and `--rekor-url` override defaults
    - _Requirements: 23.1, 23.4, 23.5, 23.6, 23.8, 23.9, 23.10, 23.11, 23.13_

- [x] 54. Implement Daemon Management CLI (`meld daemon`) (Req 24)
  - [x] 54.1 Create `DaemonModule` in `meld-cli/include/meld/cli/daemon_module.hpp` and `meld-cli/src/daemon_module.cpp`
    - Define `DaemonModule` extending `BaseCommandHandler`
    - Define `DaemonStartOptions` struct with `workspace` (path), `foreground` (bool), `timeout_secs` (optional)
    - Define `DaemonLogOptions` struct with `lines` (uint32, default 50), `follow` (bool, default true)
    - Define `DaemonStatus` struct with `running`, `pid`, `workspace`, `uptime`, `lsp_clients`, `mcp_clients`, `indexing_progress`, `active_sandboxed_processes`
    - Register `meld daemon` subcommand with sub-subcommands: `start`, `stop`, `status`, `restart`, `logs`
    - Parse flags: `--workspace=<path>`, `--foreground`, `--timeout=<seconds>`, `--lines=<N>`, `--no-follow`, `--json`
    - _Requirements: 24.1, 24.2, 24.9, 24.10, 24.11, 24.12, 24.15, 24.16_

  - [x] 54.2 Implement daemon process discovery via PID file
    - Implement `find_running_daemon()` reading `<workspace>/.meld/daemon.pid`
    - Verify PID is alive via `kill(pid, 0)` (POSIX) or equivalent
    - If PID file exists but process is dead, remove stale PID file and return empty
    - Implement `pid_file_path()` and `log_file_path()` helpers
    - _Requirements: 24.13, 24.14_

  - [x] 54.3 Implement `handle_start()` — start or connect to daemon
    - Check for existing daemon via `find_running_daemon()`
    - If running: print PID and connection details, exit 0
    - If not running and `--foreground`: exec `meldd --workspace=<path>` in foreground (attached to terminal)
    - If not running and no `--foreground`: fork/exec `meldd --workspace=<path>` as background daemon
    - Pass `--timeout=<seconds>` to `meldd` if specified
    - Wait for daemon to write PID file and become ready (poll with timeout)
    - Print daemon PID and status on success
    - _Requirements: 24.1, 24.2, 24.3, 24.15, 24.16_

  - [x] 54.4 Implement `handle_stop()` — graceful daemon shutdown
    - Read PID from `find_running_daemon()`
    - If no daemon running: print info message, exit 0
    - Send `SIGTERM` to daemon process
    - Wait for clean exit (up to 10s timeout)
    - If timeout: send `SIGKILL`
    - Remove PID file after process exits
    - _Requirements: 24.4, 24.5_

  - [x] 54.5 Implement `handle_status()` — query daemon status
    - If no daemon running: print `stopped`, exit 0
    - If running: query daemon via IPC (Unix domain socket or HTTP) for status info
    - Display: running state, PID, workspace, uptime, LSP/MCP client counts, indexing progress, active sandbox count
    - Format as JSON when `--json` is specified
    - _Requirements: 24.6, 24.7, 24.12_

  - [x] 54.6 Implement `handle_restart()` — stop then start
    - Call `handle_stop()` then `handle_start()` with the same options
    - _Requirements: 24.8_

  - [x] 54.7 Implement `handle_logs()` — tail daemon logs
    - Open `<workspace>/.meld/daemon.log`
    - Print last N lines (`--lines=<N>`, default 50)
    - If `--no-follow`: print and exit
    - If follow mode (default): tail the log file, streaming new entries to terminal
    - _Requirements: 24.9, 24.10, 24.11_

  - [x] 54.8 Add `daemon_module` Bazel target in `meld-cli/BUILD.bazel`
    - Add `cc_library` for `daemon_module`
    - Add `daemon_module` to the `meld` binary's module list in `main.cpp`
    - _Requirements: 24.1_

  - [ ]* 54.9 Write unit tests for DaemonModule in `meld-cli/tests/daemon_module_test.cpp`
    - Test `start` detects existing daemon and prints connection details
    - Test `start` spawns new daemon when none running
    - Test `stop` sends SIGTERM and waits for exit
    - Test `stop` prints info when no daemon running
    - Test `status` reports running state with details
    - Test `status` reports stopped when no daemon
    - Test stale PID file detection and cleanup
    - Test `--foreground` flag runs daemon in foreground
    - Test `--json` output format for all subcommands
    - Test `logs --no-follow` prints and exits
    - _Requirements: 24.1, 24.3, 24.4, 24.5, 24.6, 24.7, 24.12, 24.13, 24.14, 24.15_

- [x] 55. Update Shell Completions and Help for new subcommands
  - [x] 55.1 Update `ShellModule` to include `meld sign` and `meld daemon` in completion scripts
    - Add `sign` subcommand with flags: `--key`, `--sigstore`, `--verify`, `--bundle`, `--offline`, `--online`, `--fulcio-url`, `--rekor-url`
    - Add `daemon` subcommand with sub-subcommands: `start`, `stop`, `status`, `restart`, `logs`
    - Add `daemon start` flags: `--workspace`, `--foreground`, `--timeout`
    - Add `daemon logs` flags: `--lines`, `--no-follow`
    - _Requirements: 15.1, 15.2_

  - [x] 55.2 Update `HelpModule` to include `meld sign` and `meld daemon` in help output
    - Add `sign` to the subcommand summary with description: "Sign, verify, or bundle Meld binaries"
    - Add `daemon` to the subcommand summary with description: "Manage the meldd daemon lifecycle"
    - Add detailed help for `meld help sign` and `meld help daemon` with flags and examples
    - _Requirements: 16.1, 16.2_

- [x] 56. Checkpoint — `meld sign` and `meld daemon`
  - Ensure `meld sign` delegates to notary library for sign/verify/bundle
  - Ensure `meld daemon start/stop/status/restart/logs` manage daemon lifecycle
  - Ensure shell completions and help include new subcommands
  - Ask the user if questions arise.

- [x] 57. Implement VM Management CLI (`meld vm`) (Req 25)
  - [x] 57.1 Create `VmModule` in `meld-cli/include/meld/cli/vm_module.hpp` and `meld-cli/src/vm_module.cpp`
    - Define `VmModule` extending `BaseCommandHandler`
    - Define `VmLogOptions` struct with `follow` (bool, default true)
    - Define `VmStatus` struct with `running` (bool), `ram_usage` (string), `cpu_usage` (string), `active_microvms` (uint32), `active_containers` (uint32)
    - Register `meld vm` subcommand with sub-subcommands: `status`, `start`, `stop`, `restart`, `shell`, `prune`, `logs`
    - Parse flags: `--no-follow`, `--json`
    - Implement `is_native_linux()` helper checking `/dev/kvm` availability
    - Implement `run_limactl()` helper to execute `limactl` commands and capture output
    - Implement `run_in_vm()` helper to execute commands inside the Lima VM via `limactl shell`
    - _Requirements: 25.9, 25.10, 25.11_

  - [x] 57.2 Implement `handle_status()` — query VM state
    - On macOS: run `limactl ls --json`, parse output for `meld-vm` instance, populate `VmStatus` struct with running/stopped state, RAM/CPU usage, active MicroVM and container counts
    - On native Linux: report KVM status (`/dev/kvm` available/unavailable) and host containerd status (running/stopped)
    - Format as JSON when `--json` is specified
    - _Requirements: 25.1, 25.9, 25.10_

  - [x] 57.3 Implement `handle_start()` and `handle_stop()`
    - `start`: run `limactl start meld-vm` to boot the Alpine host; if already running, print current status and exit 0
    - `stop`: run `limactl stop meld-vm` to shut down the Alpine host, freeing allocated memory and CPU resources
    - On native Linux: print `info: Native Linux detected, VMM bridge disabled` and exit 0 for both commands
    - _Requirements: 25.2, 25.3, 25.9_

  - [x] 57.4 Implement `handle_restart()`, `handle_shell()`, `handle_prune()`
    - `restart`: stop then start the `meld-vm` instance (hard reboot), useful for recovering from tangled TAP devices or KVM permission issues
    - `shell`: run `limactl shell meld-vm` to drop into root shell inside Alpine host for direct inspection of `/dev/kvm`, `iptables`, `containerd`, and Firecracker state
    - `prune`: run `nerdctl system prune` inside VM via `run_in_vm()` and wipe stale socket files under `/tmp/meld/*.sock` to recover disk space
    - On native Linux: print info message and exit 0 for all three
    - _Requirements: 25.4, 25.5, 25.6, 25.9_

  - [x] 57.5 Implement `handle_logs()` — tail VM system logs
    - Tail Firecracker and containerd system logs inside the Lima VM
    - `--no-follow`: print current log contents and exit immediately
    - Default (follow mode): stream new log entries as produced
    - On native Linux: print info message and exit 0
    - _Requirements: 25.7, 25.8, 25.9_

  - [x] 57.6 Add `vm_module` Bazel target in `meld-cli/BUILD.bazel`
    - Add `cc_library` for `vm_module` with appropriate deps
    - Add `vm_module` to the `meld` binary's module list in `main.cpp`
    - Register with `CommandDispatcher` with help text and subcommand listing
    - _Requirements: 25.11_

  - [ ]* 57.7 Write unit tests for VmModule in `meld-cli/tests/vm_module_test.cpp`
    - Test `status` on macOS parses `limactl ls` output correctly and populates `VmStatus`
    - Test `start` when VM already running prints status and exits 0
    - Test `stop` invokes `limactl stop meld-vm`
    - Test `restart` performs stop then start sequence
    - Test native Linux detection prints info message and exits 0 for non-status commands
    - Test native Linux `status` reports KVM and containerd state
    - Test `--json` output format for all subcommands
    - Test `prune` invokes `nerdctl system prune` inside VM and cleans socket files
    - _Requirements: 25.1, 25.2, 25.3, 25.4, 25.6, 25.9, 25.10_

- [x] 58. Implement Isolation Backend Override (`meld run --isolation`) (Req 26)
  - [x] 58.1 Add `--isolation` flag to InterpreterModule
    - Add `--isolation=<backend>` flag to argument parser where valid backends are: `srt`, `finch`, `microvm`
    - When specified, override the `local` key from `meld.toml` `[isolation]` section for that single execution
    - When not specified, read from `meld.toml` `[isolation].local` (default: `srt` — process-level sandboxing)
    - Validate backend value; print error and exit non-zero for unrecognized backends
    - _Requirements: 26.1, 26.2, 26.3_

  - [x] 58.2 Implement provider delegation for each backend
    - `--isolation=srt`: execute binary within SRT process-level sandbox derived from the binary's effect manifest, without hardware isolation
    - `--isolation=finch`: delegate to `melds` with FinchProvider, launching binary inside Alpine OCI container with `meldi` as PID 1 via containerd/nerdctl
    - `--isolation=microvm`: delegate to `melds` with MicroVMProvider, booting Firecracker MicroVM with Alpine ext4 rootfs and `meldi` as PID 1
    - _Requirements: 26.4, 26.5, 26.6_

  - [x] 58.3 Ensure composability with other flags
    - Verify `--isolation` composes with `--debug`, `--strict`, `--watch`, `--agent-test`, `--vfs`
    - When combined with `--strict`, apply both isolation backend override and production-grade Sigstore verification
    - When combined with `--vfs`, pass `vfs_mode` to the selected sandbox provider
    - _Requirements: 26.7, 26.8_

  - [ ]* 58.4 Write unit tests for `--isolation` flag
    - Test `--isolation=srt` uses SRT sandbox
    - Test `--isolation=finch` delegates to `melds` with FinchProvider
    - Test `--isolation=microvm` delegates to `melds` with MicroVMProvider
    - Test default reads from `meld.toml` `[isolation].local` when flag not specified
    - Test invalid backend value produces error
    - Test composability with `--strict` and `--vfs`
    - _Requirements: 26.1, 26.2, 26.3, 26.4, 26.5, 26.6, 26.7, 26.8_

- [x] 59. Update Shell Completions and Help for `meld vm` and `--isolation`
  - [x] 59.1 Update `ShellModule` to include `meld vm` in completion scripts
    - Add `vm` subcommand with sub-subcommands: `status`, `start`, `stop`, `restart`, `shell`, `prune`, `logs`
    - Add `vm logs` flags: `--no-follow`
    - Add `vm status` / `vm logs` flags: `--json`
    - Add `--isolation` flag to `meld run` completions with values: `srt`, `finch`, `microvm`
    - Generate completions for both Bash and Zsh
    - _Requirements: 15.1, 15.2_

  - [x] 59.2 Update `HelpModule` to include `meld vm` and `--isolation` in help output
    - Add `vm` to the subcommand summary with description: "Manage the background Lima VM (macOS)"
    - Add detailed help for `meld help vm` with all subcommands, flags, and usage examples
    - Add `--isolation=<backend>` to `meld help run` flag listing with description of valid backends (srt, finch, microvm)
    - _Requirements: 16.1, 16.2_

- [x] 60. Checkpoint — VM management and isolation override
  - Ensure `meld vm` subcommands wrap `limactl` correctly on macOS
  - Ensure native Linux detection skips Lima operations and reports KVM/containerd status
  - Ensure `--isolation` overrides `meld.toml` `[isolation].local` for single execution
  - Ensure `--isolation` composes with `--strict`, `--vfs`, `--debug`, `--agent-test`
  - Ensure shell completions and help include `meld vm` and `--isolation`
  - Ask the user if questions arise.

## Notes

- Tasks marked with `*` are optional and can be skipped for faster MVP
- Each task references specific requirements for traceability
- Checkpoints ensure incremental validation
- All examples follow the `.cpp` + `.meld` separation rule
- The `imp` keyword is the only import keyword; `import`, `from`, `as` are banned
- Bazel is not available in this environment — tests cannot be run locally but should be structurally correct
- Tasks 1-16 cover the original CLI implementation (AST interpreter, bytecode pipeline, formatter)
- Tasks 6-12 (bytecode pipeline) use legacy requirement numbering — see note at top of Tasks section
- Tasks 13-14 use legacy requirement numbering: Task 13 `4.x` → Req 6, Task 14 `8.x` → Req 17
- Tasks 17-24 cover debugging features (DAP server, debug hooks, DWARF/PDB, data formatters, effect context)
- Tasks 25-40 cover the remaining CLI commands (scaffolding, watch mode, testing, package management, audit, effect firewall, ORC JIT dev server, LSP, MCP, shell integration, help/version)
- Tasks 41-43 cover compiled debug attach and VS Code debug routing (Req 18 superseded by Req 20; VS Code routing moved to vscode-meld Req 9)
- Tasks 44-47 cover Agent-Test mode (Req 19) and `meld debug` unified orchestrator (Req 20)
- Tasks 48-52 cover the runtime layer additions: `melds` supervisor (Req 21), daemon handshake (Req 2.14-2.16), `--strict` mode (Req 2.17), and VFS bridge (Req 22)
- Tasks 53-56 cover binary signing `meld sign` (Req 23) and daemon management `meld daemon` (Req 24)
- Tasks 57-60 cover VM management `meld vm` (Req 25) and isolation backend override `--isolation` (Req 26)
