# Requirements Document

## Introduction

This document specifies the requirements for the unified `meld` CLI — a single binary that serves as the complete developer interface for the Meld programming language. The CLI covers project scaffolding, execution (three tiers), compilation, testing, package management, formatting, security auditing, debugging, LSP/MCP integration, and shell tooling. The CLI framework (`CliCore`, `CommandDispatcher`, module plugin architecture) is fully operational in `meld-cli/`.

Meld uses a three-tier execution model:

- **Tier 1 — AST Interpreter** (`meld run`): Instant startup, no compilation. Used for REPL and quick script execution. The same interpreter powers compile-time macro evaluation.
- **Tier 2 — LLVM ORC JIT** (`meld dev`): Sub-second module swapping via ibazel for development hot-reload.
- **Tier 3 — LLVM AOT** (`meld build`): O3 optimization, ThinLTO, static native binaries for production.

The Effect Firewall (algebraic effects sandbox) is enforced identically across all three tiers.

**Cross-references:**
- `.kiro/specs/meld-compiler/` — LLVM ORC JIT and AOT pipeline details
- `.kiro/specs/meld-build/` — Bazel integration and `meld.toml` configuration
- `.kiro/specs/meld-test/` — Testing framework internals
- `.kiro/specs/meld-lsp-server/` — LSP server details
- `.kiro/specs/meld-mcp-server/` — MCP server details
- `.kiro/specs/meld-core/requirements.md` (Req 99–118) — `@uses` annotation system and Effect Firewall
- `.kiro/specs/meld-manifest/requirements.md` (Req 5–8) — Cryptographic signing, Sigstore integration, `meldn` notary utility
- `.kiro/specs/meld-daemon/requirements.md` (Req 1–13) — Full daemon spec (`meldd` architecture, lifecycle, APIs, Lima lifecycle management)
- `.kiro/specs/meld-supervisor/requirements.md` (Req 1) — VMM Host Detection and Lima bridge for the `meld vm` subcommand suite

## Glossary

- **CLI**: The unified `meld` command-line binary in `meld-cli/src/main.cpp`, using `CliCore` and `CommandDispatcher`. In the Meld toolchain naming convention, the runner functionality (`meld run`) is conceptually referred to as `meldr` (Meld Runner), but it is implemented as a subcommand of the unified CLI, not a separate binary
- **AST_Interpreter**: A tree-walking interpreter class (`meld::interpreter::AstInterpreter`) in `meld-core/` that evaluates parsed `parser::ast::expression` nodes directly using Kernel_Primitives and an Environment
- **InterpreterModule**: The `meld::cli::InterpreterModule` in `meld-cli/` that handles the `run` subcommand (including `--interactive` REPL mode)
- **CompilerModule**: The `meld::cli::CompilerModule` in `meld-cli/` that handles the `build` subcommand
- **DevToolsModule**: The `meld::cli::DevToolsModule` in `meld-cli/` containing `CodeFormatter`, `Debugger`
- **Formatter**: The `CodeFormatter` class in `DevToolsModule` that parses Meld source to AST and pretty-prints it back to one canonical, unconfigurable style
- **REPL**: Read-Eval-Print Loop, an interactive session using the AST_Interpreter, accessed via `meld run --interactive`
- **Environment**: A scope-chain data structure mapping identifiers to `kernel::Value` instances, supporting nested lexical scopes
- **Kernel_Primitives**: The `meld::kernel` types (`Integer`, `String`, `Boolean`, `Function`, `Cell`, `Vec`, `Optional`, etc.) and operations (`car`, `cdr`, `cons`, `apply`, `eq`, `equal`)
- **Meld_Source**: A `.meld` file containing valid Meld language code
- **Parser**: The existing `meld::parser::Parser` class that produces `std::vector<parser::ast::expression>` from source text
- **ORC_JIT**: LLVM's On-Request Compilation JIT engine, used for in-memory hot-reload during development
- **AOT_Pipeline**: The Ahead-Of-Time compilation pipeline that links `.bc` modules through LLVM optimization passes and `lld` into a static native binary
- **Dev_Server**: A long-running background process embedding the ORC_JIT for hot-reload via ibazel
- **Bitcode**: LLVM's binary intermediate representation (`.bc` files) produced by the compiler frontend
- **Effect_Firewall**: The security sandbox that restricts which algebraic effects a module or dependency may perform, enforced via `@uses` annotations and `meld.toml` `allow` arrays
- **meld.toml**: The single source of truth for project configuration (name, version, dependencies, targets, effect permissions)
- **meld.lock**: Lock file pinning exact Git commits/tags for reproducible dependency resolution
- **Transpile_Backend**: A code generation backend that emits source code in another language (Go, JVM, C++, Wasm, Rust) from the Meld AST
- **DAP**: Debug Adapter Protocol — a standardized JSON-based protocol for communication between IDEs (debug clients) and language-specific debug servers, enabling breakpoints, stepping, variable inspection, and expression evaluation
- **DAP_Server**: The Meld CLI's built-in DAP server that listens on a TCP port and translates DAP requests into AST_Interpreter debugging operations
- **DWARF**: Debug information format used on Linux and macOS, mapping machine code addresses to source file locations
- **PDB**: Program Database — debug information format used on Windows
- **Data_Formatter**: LLDB Python scripts that teach the debugger how to display Meld kernel types (Vec, Optional, Function, etc.) in human-readable form
- **Effect_Context**: The set of active effect handlers and declared `@uses` permissions visible at a given point in the call stack during debugging
- **Lima**: A Linux virtual machine manager for macOS that provides `/dev/kvm` support, enabling Firecracker and containerd to run on macOS hosts via `limactl`
- **meld-vm**: The named Lima VM instance (Alpine Linux) that hosts Firecracker and containerd on macOS; managed by `meld vm` subcommands and auto-started by `meldd` when needed
- **VM_Module**: The `meld::cli::VmModule` in `meld-cli/` that handles the `meld vm` subcommand suite, wrapping `limactl` operations behind a clean CLI interface
- **SignModule**: The `meld::cli::SignModule` in `meld-cli/` that handles the `meld sign` subcommand suite, delegating to the `meldn` notary infrastructure for binary signing, verification, and air-gapped bundle preparation
- **meldn**: The Meld Notary — a standalone CLI utility (`meldn sign`, `meldn verify`, `meldn bundle`) that produces signed tombstones, verifies binary integrity, and prepares air-gapped bundles (defined in `.kiro/specs/meld-manifest/requirements.md` Req 8)
- **Combined_Integrity_Hash**: `H_total = Hash(code_hash + manifest_hash + debug_id)` — the composite hash signed during binary signing, covering code, manifest, and debug sidecar identity
- **Sigstore_Bundle**: A composite artifact containing the signature, a short-lived Fulcio certificate, and a Rekor Signed Entry Timestamp (SET), enabling keyless identity-based signing
- **DaemonModule**: The `meld::cli::DaemonModule` in `meld-cli/` that handles the `meld daemon` subcommand suite, managing the lifecycle of the `meldd` daemon process (start, stop, status, restart, logs)
- **meldd**: The Meld daemon binary — a single long-lived C++20 process serving both LSP and MCP channels from a shared semantic state (defined in `.kiro/specs/meld-daemon/requirements.md`)

## Requirements

### Requirement 1: AST Interpreter Engine (meld-core library)

**User Story:** As a Meld developer, I want a tree-walking AST interpreter as a reusable library, so that both the CLI `run` command and the REPL can execute Meld code without compilation (Tier 1 execution).

#### Acceptance Criteria

1. THE AST_Interpreter SHALL be implemented as `meld::interpreter::AstInterpreter` class in `meld-core/include/meld/interpreter/ast_interpreter.hpp` and `meld-core/src/interpreter/ast_interpreter.cpp`
2. THE AST_Interpreter SHALL accept a `std::vector<parser::ast::expression>` and evaluate all top-level expressions in order, returning the value of the last expression
3. WHEN the AST_Interpreter evaluates an `integer_literal` node, IT SHALL produce a `kernel::Integer` value with the corresponding numeric value
4. WHEN the AST_Interpreter evaluates a `string_literal` node, IT SHALL produce a `kernel::String` value with the corresponding text
5. WHEN the AST_Interpreter evaluates a `boolean_literal` node, IT SHALL produce a `kernel::Boolean` value matching the literal
6. WHEN the AST_Interpreter evaluates a `val_declaration` node, IT SHALL bind the declared name to the evaluated right-hand-side value in the current Environment as an immutable binding
7. WHEN the AST_Interpreter evaluates a `var_declaration` node, IT SHALL bind the declared name to the evaluated right-hand-side value in the current Environment as a mutable binding
8. WHEN the AST_Interpreter evaluates an `identifier` node, IT SHALL look up the name in the Environment scope chain and return the bound value
9. IF the AST_Interpreter evaluates an `identifier` that is not bound in any enclosing Environment, THEN IT SHALL throw an `InterpreterError` with the identifier name and source location
10. WHEN the AST_Interpreter evaluates a `function_definition` node, IT SHALL create a closure value capturing the parameter list, body expressions, and the defining Environment
11. WHEN the AST_Interpreter evaluates a `function_call` node, IT SHALL evaluate the callee, evaluate each argument left-to-right, create a new child Environment extending the closure environment with parameter bindings, and evaluate the function body in that Environment
12. WHEN the AST_Interpreter evaluates a `binary_operation` node, IT SHALL evaluate both operands and apply the corresponding Kernel_Primitives operation (arithmetic, comparison, or logical)
13. WHEN the AST_Interpreter evaluates a `list_expression` node, IT SHALL evaluate each element and produce a `kernel::Vec` value containing the results
14. WHEN the AST_Interpreter evaluates a `block_expression` node, IT SHALL evaluate each expression in sequence in a new child Environment and return the value of the last expression
15. IF a runtime error occurs during AST interpretation (division by zero, type mismatch), THEN THE AST_Interpreter SHALL throw an `InterpreterError` with source location and a stack trace of active function calls
16. THE AST_Interpreter SHALL be added to `meld-core/BUILD.bazel` as an `interpreter` library target with deps on `:parser` and `:kernel`

### Requirement 2: InterpreterModule Integration (meld run)

**User Story:** As a Meld developer, I want `meld run` to execute Meld programs via the AST interpreter and optionally drop into an interactive REPL, so that I can run scripts instantly and experiment interactively from a single command.

#### Acceptance Criteria

1. WHEN `meld run <file.meld>` is invoked, THE InterpreterModule SHALL read the file, parse it using the Parser, and evaluate it using the AST_Interpreter
2. WHEN `meld run <file.meld>` is invoked with additional positional arguments after the file path, THE InterpreterModule SHALL make those arguments available to the Meld program via a built-in `args` binding
3. IF the Parser fails to parse the input file, THEN THE InterpreterModule SHALL print the parse error in `<file>:<line>:<column>: error: <message>` format and exit with a non-zero exit code
4. IF a runtime error occurs during execution, THEN THE InterpreterModule SHALL print the error with source location and stack trace, and exit with a non-zero exit code
5. WHEN `meld run --interactive` (or `-i`) is invoked without a file path, THE InterpreterModule SHALL start an interactive REPL session using the AST_Interpreter with a fresh Environment
6. WHEN `meld run -i <file.meld>` is invoked, THE InterpreterModule SHALL parse and evaluate the file first, then drop into the REPL with that file's Environment state loaded
7. WHEN the user enters a complete expression in the REPL, THE InterpreterModule SHALL parse it, evaluate it using the AST_Interpreter, and print the resulting value with its type
8. WHEN the user enters `:load <file.meld>` in the REPL, THE InterpreterModule SHALL parse and evaluate the file contents in the current session Environment
9. IF a parse error or runtime error occurs during REPL evaluation, THEN THE InterpreterModule SHALL print the error message and return to the prompt without terminating the session
10. WHEN `meld run --watch <file.meld>` is invoked, THE InterpreterModule SHALL re-execute the file automatically on file saves
11. WHEN `meld run --debug` is invoked, THE CLI SHALL start the DAP server on a local TCP port and execute the program in debug mode via the AST_Interpreter, pausing at the first breakpoint
12. WHEN `meld run --debug --debug-wait` is invoked, THE CLI SHALL halt the AST_Interpreter before the first instruction and block until an IDE debug client attaches via DAP
13. THE `interpreter_module` Bazel target in `meld-cli/BUILD.bazel` SHALL add a dependency on `//meld-core:interpreter`
14. WHEN `meld run <binary>` is invoked with a compiled native binary (not a `.meld` source file), THE InterpreterModule SHALL perform a daemon handshake: ping `meldd` to verify the binary is current, and if not, trigger a `bazel build` through the daemon before execution
15. WHEN executing a compiled binary that contains a Tombstone with a `debug_id`, THE InterpreterModule SHALL automatically resolve the `.mdebug` sidecar via the daemon's `resolve_debug_sidecar` API and map the shadow symbols to the execution session
16. IF a compiled binary crashes within the sandbox, THE InterpreterModule SHALL capture the crash state and provide an AST-precision trace (PC → AST Node via the `.mdebug` sidecar's AST-to-PC index), bypassing the ambiguity of line numbers, formatted for both human and AI agent consumption
17. WHEN `meld run --strict <binary>` is invoked, THE CLI SHALL delegate execution to the `melds` production supervisor binary, applying production-grade Sigstore verification and sandbox enforcement locally for testing purposes

### Requirement 3: LLVM ORC JIT Dev Server (meld dev)

**User Story:** As a Meld developer, I want `meld dev` to start a hot-reloading development server powered by LLVM ORC JIT, so that I get sub-second feedback on code changes without restarting my application (Tier 2 execution).

#### Acceptance Criteria

1. WHEN `meld dev` is invoked in a project directory, THE CLI SHALL start the Dev_Server which embeds the ORC_JIT engine and watches for file changes via ibazel
2. WHEN a `.meld` source file changes, THE Dev_Server SHALL recompile the affected module to Bitcode and hot-swap it into the running JIT session
3. THE Dev_Server SHALL achieve module swap latency under 200ms for typical single-module changes
4. IF the Dev_Server fails to load or verify an updated Bitcode module, THEN THE Dev_Server SHALL report the error and retain the previous working module
5. WHEN `meld dev --port <port>` is specified, THE Dev_Server SHALL listen on the specified port for IPC signals
6. THE Dev_Server SHALL enforce the Effect_Firewall identically to `meld run` and `meld build` modes

> **Note:** Full ORC JIT and hot-reload internals are specified in `.kiro/specs/meld-compiler/requirements.md` (Requirements 6–7).

### Requirement 4: LLVM AOT Production Build (meld build)

**User Story:** As a Meld developer, I want `meld build` to compile my project into an optimized native binary via LLVM AOT, so that I can produce production-ready executables (Tier 3 execution).

#### Acceptance Criteria

1. WHEN `meld build` is invoked, THE CompilerModule SHALL parse all project sources, lower the AST to LLVM IR, and compile a native binary via the AOT_Pipeline
2. WHEN `meld build --release` is invoked, THE AOT_Pipeline SHALL apply O3 optimization, ThinLTO cross-module inlining, and strip debug symbols from the output binary
3. WHEN `meld build --debug` is invoked, THE AOT_Pipeline SHALL emit DWARF debug information (Linux/macOS) or PDB debug information (Windows) mapping machine code to `.meld` source lines, and skip optimization passes that would obscure source mapping
4. WHEN `meld build` is invoked without `--release` or `--debug`, THE AOT_Pipeline SHALL emit debug information by default (equivalent to `--debug`)
5. WHEN `meld build --target <triple>` is specified (e.g., `x86_64-linux-gnu`), THE AOT_Pipeline SHALL cross-compile for the specified target triple
6. WHEN `meld build --transpile <backend>` is specified, THE CompilerModule SHALL emit source code in the specified language backend instead of a native binary, where valid backends are: `go`, `jvm`, `cpp`, `wasm`, `rust`
7. WHEN `meld build --output <path>` is specified, THE CLI SHALL write the compiled artifact to the specified path
8. WHEN `meld build` is invoked without `--output`, THE CLI SHALL write the compiled binary to a file with the same base name as the project in the `build/` directory
9. IF the source contains errors detected during compilation, THEN THE CompilerModule SHALL report the errors with source locations and exit with a non-zero exit code
10. THE AOT_Pipeline SHALL produce a statically linked executable containing the Meld Kernel Runtime and the compiled program
11. THE AOT_Pipeline SHALL enforce the Effect_Firewall identically to `meld run` and `meld dev` modes

> **Note:** Full LLVM AOT pipeline internals are specified in `.kiro/specs/meld-compiler/requirements.md` (Requirement 8).

### Requirement 5: AST-to-LLVM-IR Lowering

**User Story:** As a compiler developer, I want the AST to be lowered to LLVM IR, so that both the ORC JIT and AOT pipeline have a well-structured input for compilation.

#### Acceptance Criteria

1. THE Compiler_Frontend SHALL accept parsed AST and produce LLVM Bitcode (`.bc`) as output
2. WHEN the Compiler_Frontend processes a `function_definition` AST node, IT SHALL emit an LLVM function definition with the appropriate signature
3. WHEN the Compiler_Frontend processes a `val_declaration` AST node, IT SHALL emit LLVM `alloca` and `store` instructions for the binding
4. WHEN the Compiler_Frontend processes a `binary_operation` AST node, IT SHALL emit the corresponding LLVM arithmetic or comparison instructions
5. WHEN the Compiler_Frontend processes a `function_call` AST node, IT SHALL emit an LLVM `call` instruction with evaluated arguments
6. THE Compiler_Frontend SHALL emit LLVM IR that passes LLVM's module verification (`llvm::verifyModule`) without errors
7. THE Compiler_Frontend SHALL produce deterministic Bitcode output for identical input files and compiler flags

> **Note:** Full IR generation details are specified in `.kiro/specs/meld-compiler/requirements.md` (Requirements 1, 4).

### Requirement 6: Code Formatter (meld fmt)

**User Story:** As a Meld developer, I want `meld fmt` to auto-format my Meld source files to one canonical, unconfigurable style (like gofmt), so that all Meld code looks the same everywhere.

#### Acceptance Criteria

1. WHEN `meld fmt` is invoked in a project directory, THE DevToolsModule SHALL format all `.meld` files in the project to the canonical style
2. WHEN `meld fmt <file.meld>` is invoked, THE DevToolsModule SHALL use the Formatter to parse the file to AST, pretty-print it back to Meld source, and write the formatted output back to the same file
3. WHEN `meld fmt --check` is invoked, THE DevToolsModule SHALL compare the formatted output to the original source and exit with exit code 0 if all files match, or exit code 1 if any differ, without modifying files
4. WHEN `meld fmt --stdout <file.meld>` is invoked, THE DevToolsModule SHALL write the formatted output to standard output instead of modifying the file
5. THE Formatter SHALL enforce one canonical style with no configuration options (no indent_size, max_line_length, or brace_style settings)
6. THE Formatter SHALL preserve all semantic content: parsing then formatting then parsing SHALL produce an AST equivalent to parsing the original source (round-trip property)
7. THE Formatter SHALL use consistent indentation of 4 spaces per nesting level
8. THE Formatter SHALL place opening braces on the same line as the declaration and closing braces on their own line aligned with the declaration start
9. WHEN formatting a `function_definition`, THE Formatter SHALL place each parameter on its own line when the parameter list exceeds 80 characters
10. WHEN formatting an `imp` declaration, THE Formatter SHALL use the `imp` keyword and preserve all symbol mappings
11. IF the Parser fails to parse the input file, THEN THE Formatter SHALL print the parse error and exit with a non-zero exit code without modifying the original file

### Requirement 7: Project Scaffolding (meld new / meld init)

**User Story:** As a Meld developer, I want `meld new` and `meld init` to scaffold new projects with `meld.toml` configuration, so that I can start coding quickly with the correct project structure.

#### Acceptance Criteria

1. WHEN `meld new <project_name>` is invoked, THE CLI SHALL create a new directory named `<project_name>` containing a `meld.toml`, a `src/main.meld` entry point, and a `.gitignore`
2. WHEN `meld new <project_name> --lib` is invoked, THE CLI SHALL create the project with `src/module.meld` instead of `src/main.meld`
3. THE generated `meld.toml` SHALL contain the project name, version `0.1.0`, and default configuration sections for `[package]`, `[dependencies]`, and `[targets]`
4. WHEN `meld init` is invoked in an existing directory, THE CLI SHALL create a `meld.toml` and `src/main.meld` in the current directory without creating a new parent directory
5. IF a `meld.toml` already exists in the target directory, THEN THE CLI SHALL print an error and exit without overwriting the existing configuration
6. THE CLI SHALL initialize a Git repository in the new project directory when `meld new` is invoked

### Requirement 8: Testing Framework CLI (meld test)

**User Story:** As a Meld developer, I want `meld test` to discover and run all tests in my project, so that I can validate my code with a single command.

#### Acceptance Criteria

1. WHEN `meld test` is invoked, THE CLI SHALL discover and execute all functions annotated with `@test` in the project
2. WHEN `meld test --filter <regex>` is invoked, THE CLI SHALL execute only tests whose names match the provided regular expression
3. WHEN `meld test --parallel` is invoked, THE CLI SHALL execute tests concurrently using the Actor-based shared-nothing concurrency model
4. WHEN `meld test --coverage` is invoked, THE CLI SHALL instrument the code and produce a coverage report after test execution
5. WHEN all tests pass, THE CLI SHALL exit with exit code 0 and print a summary of passed tests and execution time
6. WHEN any test fails, THE CLI SHALL print detailed failure information (expected vs actual, source location, stack trace) and exit with a non-zero exit code
7. THE CLI SHALL report test results in human-readable format by default, and in JSON format when `--json` is specified

> **Note:** Testing framework internals (assertions, mocking, BDD, property tests) are specified in `.kiro/specs/meld-test/requirements.md`.

### Requirement 9: Package Management (meld mod) — SUPERSEDED

> **SUPERSEDED:** This requirement has been replaced by Requirements 27–33 (`meld module`), which consolidate all package and module management under the unified `meld module` command. The Git-first subcommands (`fetch`, `update`, `clean`) from Req 9 are preserved in Req 27 alongside the new `install`, `uninstall`, and `list` subcommands. The `meld mod` and `meld package` aliases are deprecated in favor of `meld module`.

### Requirement 10: Effect Security Audit (meld audit)

**User Story:** As a Meld developer, I want `meld audit` to show me exactly which algebraic effects each dependency requests, so that I can make informed security decisions about third-party code.

#### Acceptance Criteria

1. WHEN `meld audit` is invoked, THE CLI SHALL parse the project AST and all transitive dependency ASTs to build a tree of requested algebraic effects
2. THE CLI SHALL display the effect tree showing each package and the effects it requests (e.g., `fs.read`, `fs.write`, `net`, `console`)
3. WHEN `meld audit --json` is invoked, THE CLI SHALL output the effect tree in machine-readable JSON format
4. WHEN a dependency requests effects not listed in its `allow` array in `meld.toml`, THE CLI SHALL flag the violation with a warning
5. THE CLI SHALL distinguish between effects a package directly performs and effects it transitively requires through its own dependencies

### Requirement 11: Cross-Mode Effect Firewall Enforcement

**User Story:** As a Meld developer, I want the Effect Firewall to be enforced identically across all three execution tiers, so that security guarantees are consistent regardless of how I run my code.

#### Acceptance Criteria

1. THE CLI SHALL enforce the Effect_Firewall identically in Tier 1 (AST Interpreter via `meld run`), Tier 2 (ORC JIT via `meld dev`), and Tier 3 (AOT via `meld build`)
2. WHEN a module performs an effect not declared in its `@uses` annotation, THE Effect_Firewall SHALL reject the operation at the same point in all three tiers
3. WHEN a dependency performs an effect not listed in its `allow` array in `meld.toml`, THE Effect_Firewall SHALL block the effect in all three tiers
4. IF the Effect_Firewall detects a violation, THEN THE CLI SHALL report the violation with the specific effect, the offending module, and the call site location
5. THE Effect_Firewall SHALL NOT permit a more permissive policy in any tier compared to the others

> **Note:** Effect annotation internals are specified in `.kiro/specs/meld-core/requirements.md` (Req 99–118).

### Requirement 12: Debug Adapter Protocol (DAP) Integration

**User Story:** As a Meld developer, I want the CLI to natively implement the Debug Adapter Protocol, so that I can debug Meld programs using standard IDE debuggers (VS Code, IntelliJ, Neovim) without proprietary tooling.

#### Acceptance Criteria

1. THE CLI SHALL implement a DAP server conforming to the Debug Adapter Protocol specification (version 1.x)
2. WHEN `meld run --debug` is invoked, THE CLI SHALL open a local DAP TCP port (default 4711) and pause execution until an IDE debug client attaches
3. WHEN `meld run --debug --debug-wait` is invoked, THE CLI SHALL halt the VM entirely before the first instruction, blocking until an IDE debug client attaches
4. WHEN `meld run --debug --port <port>` is invoked, THE CLI SHALL listen for DAP connections on the specified port instead of the default
5. THE DAP server SHALL support the standard DAP capabilities: `setBreakpoints`, `configurationDone`, `continue`, `next` (step-over), `stepIn`, `stepOut`, `pause`, `disconnect`, `threads`, `stackTrace`, `scopes`, `variables`, and `evaluate`
6. THE DAP server SHALL report breakpoint hits with the source file path, line number, and column number mapped to `.meld` source
7. WHEN an IDE sends an `evaluate` request at a paused breakpoint, THE DAP server SHALL evaluate the expression using the AST_Interpreter in the current Environment and return the result
8. THE CLI SHALL support the `--debug` flag on `meld run` only; `meld build` and `meld dev` do not start a DAP server

### Requirement 12A: Interpreted Debugging (AST Interpreter + DAP)

**User Story:** As a Meld developer, I want to debug programs running on the AST interpreter with granular inspection of AST nodes, scopes, and live REPL evaluation at breakpoints, so that I can deeply understand program behavior during development.

#### Acceptance Criteria

1. WHEN the AST_Interpreter is paused at a breakpoint, THE DAP server SHALL expose the current Environment scope chain as DAP `scopes` (Local, Closure, Global)
2. WHEN the DAP client requests `variables` for a scope, THE DAP server SHALL return all bindings in that Environment level with their names, values, types, and mutability status
3. WHEN the DAP client requests `variables` for a compound value (e.g., `kernel::Vec`, `kernel::Function`), THE DAP server SHALL expand the value to show its internal structure (elements for Vec, parameter list and closure environment for Function)
4. WHEN the DAP client sends an `evaluate` request, THE DAP server SHALL parse and evaluate the expression using the AST_Interpreter in the paused Environment, returning the result value and its type
5. THE DAP server SHALL support inspecting the current AST node being evaluated, exposing the node kind and source location
6. THE DAP server SHALL support conditional breakpoints where the condition expression is evaluated in the current Environment
7. THE DAP server SHALL support logpoint breakpoints that evaluate an expression and log the result without pausing execution

### Requirement 12B: Compiled Debugging (LLDB/GDB Integration)

**User Story:** As a Meld developer, I want to debug AOT-compiled native binaries using standard OS debuggers with Meld-aware formatting, so that I can diagnose production-grade issues in optimized code.

#### Acceptance Criteria

1. WHEN `meld build --debug` is invoked, THE AOT_Pipeline SHALL emit DWARF debug information (Linux/macOS) or PDB debug information (Windows) mapping machine code addresses to `.meld` source file locations (file, line, column)
2. THE debug information SHALL map LLVM-generated function names back to their original Meld function names
3. THE debug information SHALL include type descriptions for Meld kernel types (`kernel::Integer`, `kernel::String`, `kernel::Vec`, `kernel::Function`, `kernel::Optional`, etc.) so that OS debuggers can display them
4. WHEN `meld build` is invoked without `--debug` or `--release`, THE AOT_Pipeline SHALL emit debug information by default (debug is the default for non-release builds)
5. WHEN `meld build --release` is invoked, THE AOT_Pipeline SHALL strip debug information from the output binary

### Requirement 12C: LLDB/GDB Data Formatters

**User Story:** As a Meld developer, I want Meld's complex runtime types to display beautifully in IDE debugger views, so that I can read `vec[string]` or `optional[User]` values without deciphering raw memory layouts.

#### Acceptance Criteria

1. THE CLI distribution SHALL include LLDB Python formatter scripts for all Meld kernel types (`kernel::Integer`, `kernel::String`, `kernel::Boolean`, `kernel::Vec`, `kernel::Function`, `kernel::Cell`, `kernel::Optional`)
2. WHEN an LLDB session loads the Meld formatters, `kernel::Vec` values SHALL display as `vec[T] { element0, element1, ... }` with element count
3. WHEN an LLDB session loads the Meld formatters, `kernel::Optional` values SHALL display as `some(value)` or `nil` rather than raw tagged-union memory
4. WHEN an LLDB session loads the Meld formatters, `kernel::Function` values SHALL display the function name, parameter count, and whether it is a closure or native function
5. WHEN an LLDB session loads the Meld formatters, `kernel::String` values SHALL display the string content directly rather than the internal buffer pointer
6. THE VS Code extension SHALL automatically configure LLDB to load the Meld formatter scripts when debugging Meld binaries

### Requirement 12D: Effect Context Inspection

**User Story:** As a Meld developer, I want to inspect the current thread's Effect Context when paused in the debugger, so that I can see exactly which I/O permissions (e.g., `fs.read`, `net`) the current Actor or execution context holds.

#### Acceptance Criteria

1. WHEN the debugger is paused (in either interpreted or compiled mode), THE DAP server SHALL expose an "Effect Context" scope showing the active effect handlers on the current call stack
2. THE Effect Context scope SHALL list each active handler with the effect type it handles (e.g., `FileSystem`, `Network`, `Console`)
3. THE Effect Context scope SHALL show the `@uses` annotation of the currently executing function, listing its declared effects
4. IF the current execution context is within an Actor, THE Effect Context scope SHALL display the Actor's identity and its permitted effects as configured in `meld.toml`
5. THE Effect Context information SHALL be available in both interpreted debugging (via AST_Interpreter introspection) and compiled debugging (via runtime metadata embedded in debug builds)

### Requirement 13: MCP Server (meld mcp)

**User Story:** As a Meld developer, I want `meld mcp` to start an embedded MCP server, so that AI agents can interact with my Meld codebase through a standardized protocol.

#### Acceptance Criteria

1. WHEN `meld mcp` is invoked, THE CLI SHALL start a Model Context Protocol server that exposes the current project's codebase to AI agents
2. THE MCP server SHALL provide structured access to AST representations, symbol tables, and type information
3. WHEN `meld mcp --stdio` is invoked, THE CLI SHALL communicate via standard input/output for editor integration

> **Note:** Full MCP server capabilities are specified in `.kiro/specs/meld-mcp-server/requirements.md`.

### Requirement 14: LSP Server (meld lsp)

**User Story:** As a Meld developer, I want `meld lsp` to start the Language Server Protocol daemon, so that my editor provides intelligent code assistance.

#### Acceptance Criteria

1. WHEN `meld lsp` is invoked, THE CLI SHALL start the LSP server daemon for editor integration (VS Code, Neovim, etc.)
2. THE LSP server SHALL provide diagnostics, code completion, go-to-definition, find-references, and formatting capabilities
3. WHEN `meld lsp --stdio` is invoked, THE CLI SHALL communicate via standard input/output per the LSP specification

> **Note:** Full LSP server capabilities are specified in `.kiro/specs/meld-lsp-server/requirements.md`.

### Requirement 15: Shell Integration and Output Flags

**User Story:** As a Meld developer, I want shell completions and consistent output control flags, so that the CLI integrates smoothly into my terminal workflow.

#### Acceptance Criteria

1. WHEN `meld completion bash` is invoked, THE CLI SHALL output a Bash completion script to standard output
2. WHEN `meld completion zsh` is invoked, THE CLI SHALL output a Zsh completion script to standard output
3. WHEN any subcommand is invoked with `--json`, THE CLI SHALL format all output as machine-readable JSON
4. WHEN any subcommand is invoked with `--quiet`, THE CLI SHALL suppress informational output and print only errors
5. WHEN any subcommand is invoked with `--verbose`, THE CLI SHALL print detailed diagnostic and progress information

### Requirement 16: Help and Version

**User Story:** As a Meld developer, I want `meld help` and `meld version` to provide quick reference information, so that I can discover commands and verify my installation.

#### Acceptance Criteria

1. WHEN `meld help` is invoked, THE CLI SHALL print a summary of all available subcommands with brief descriptions
2. WHEN `meld help <subcommand>` is invoked, THE CLI SHALL print detailed usage information, flags, and examples for the specified subcommand
3. WHEN `meld version` is invoked, THE CLI SHALL print the Meld version number, LLVM version, and build metadata
4. WHEN `meld` is invoked with no arguments, THE CLI SHALL print the help summary (same as `meld help`)

### Requirement 17: Error Reporting

**User Story:** As a Meld developer, I want clear and consistent error messages across all CLI commands, so that I can quickly diagnose problems.

#### Acceptance Criteria

1. WHEN a parse error occurs in any subcommand, THE CLI SHALL print the error in the format `<file>:<line>:<column>: error: <message>`
2. WHEN a runtime error occurs in the AST_Interpreter, THE CLI SHALL print the error with a stack trace showing the chain of function calls leading to the error
3. WHEN a compilation error occurs during `meld build`, THE CLI SHALL print the error with source location and a description of the issue
4. WHEN a file specified on the command line does not exist, THE CLI SHALL print `error: file not found: <path>` and exit with exit code 1
5. WHEN a file specified on the command line does not have a `.meld` extension, THE CLI SHALL print a warning but proceed with processing
6. WHEN `--json` is active, THE CLI SHALL format error output as structured JSON with `file`, `line`, `column`, `severity`, and `message` fields

### Requirement 18: Compiled Debug Attach (meld debug attach) — SUPERSEDED

> **SUPERSEDED:** This requirement has been replaced by Requirement 20 (`meld debug` Unified Orchestrator), which consolidates `meld debug attach <pid>` and `meld debug run <binary>` into a single unified command with `--attach` and `--run` flags. All acceptance criteria from Req 18 are preserved in Req 20. Existing implementations should migrate to the Req 20 interface.

> **Note:** VS Code debug routing (formerly Requirement 19 in this spec) has been moved to vscode-meld Requirement 9, as it specifies VS Code extension behavior rather than CLI behavior.


---

> **Note:** Requirement 19 below was added to address the AI Developer Experience (AI_DX.md) gap: a single toggle for deterministic, reproducible execution.

### Requirement 19: Agent-Test Mode Flag (--agent-test)

**User Story:** As an AI agent or CI pipeline, I want a single CLI flag that forces all non-deterministic behavior to use deterministic defaults, so that test results are perfectly reproducible without manually configuring effect handlers.

#### Acceptance Criteria

1. THE CLI SHALL support an `--agent-test` flag on `meld test` and `meld run` subcommands that activates Agent-Test mode for the execution
2. WHEN `--agent-test` is active, THE CLI SHALL activate the language runtime's Agent-Test mode (`.kiro/specs/meld-core/requirements.md` Req 143), which automatically installs deterministic handlers for `time` (virtual clock at epoch 0), `random` (fixed seed 0), and deterministic actor scheduling
3. WHEN `--agent-test` is active, THE CLI SHALL print a notice to stderr: `[agent-test] Deterministic mode active: time=virtual, random=seed(0), scheduling=deterministic`
4. WHEN `--agent-test` is active with `meld test`, THE CLI SHALL write the structured execution trace to a file at `<project>/.meld/traces/<test-name>.trace.json` after each test completes
5. WHEN `--agent-test` is active with `meld run`, THE CLI SHALL write the execution trace to stdout when `--json` is also specified, or to `<project>/.meld/traces/run.trace.json` otherwise
6. THE `--agent-test` flag SHALL be composable with other flags: `meld test --agent-test --filter <regex>`, `meld test --agent-test --parallel`, `meld run --agent-test --debug`, etc.
7. WHEN `--agent-test` is combined with `--parallel`, THE deterministic actor scheduling SHALL ensure that parallel test execution produces the same results regardless of system load or timing
8. THE `--agent-test` flag SHALL NOT be supported on `meld build` — it is a runtime execution mode, not a compilation mode

> **Cross-reference:** Agent-Test mode runtime behavior is defined in `.kiro/specs/meld-core/requirements.md` Req 143. Deterministic actor scheduling is defined in `.kiro/specs/meld-async/requirements.md` Req 24. The daemon's `DeterministicContext` (which provides the underlying deterministic handlers) is defined in `.kiro/specs/meld-daemon/requirements.md` Req 11.

---

> **Note:** Requirement 20 below was added to address the Shadow Debugging gap identified in NEW_REQUIREMENTS_2.md §5, consolidating and superseding Requirement 18 (`meld debug attach`).

### Requirement 20: `meld debug` Unified Orchestrator

**User Story:** As a Meld developer, I want a single `meld debug` command that can both launch a binary under a debugger and attach to a running process, with automatic `.mdebug` sidecar loading, Meld data formatters, and optional SRT sandbox enforcement, so that I have one consistent entry point for all compiled-binary debugging workflows.

#### Acceptance Criteria

1. THE CLI SHALL provide `meld debug --run <binary>` which launches the specified binary under the appropriate OS debugger (LLDB on macOS, GDB on Linux), passing any arguments after `--` to the binary as its argv
2. THE CLI SHALL provide `meld debug --attach <pid>` which attaches the appropriate OS debugger to the specified running process
3. WHEN the debugger is launched (in either `--run` or `--attach` mode), THE CLI SHALL automatically load the Meld LLDB Python formatter scripts (from `meld-core/tools/lldb/meld_formatters.py`) so that kernel types (`Vec`, `Optional`, `Function`, `String`, etc.) display in human-readable form
4. WHEN `--debugger=[gdb|lldb]` is specified, THE CLI SHALL use the explicitly requested debugger instead of auto-detecting based on the host OS
5. WHEN `--break <file:line>` is specified (repeatable), THE CLI SHALL translate each breakpoint specification into the appropriate debugger command (`breakpoint set --file <file> --line <line>` for LLDB, `break <file>:<line>` for GDB) and apply them before execution begins
6. WHEN the target binary contains a Tombstone with a `debug_id`, THE CLI SHALL request the daemon's `resolve_debug_sidecar(debug_id)` API (`.kiro/specs/meld-daemon/requirements.md` Req 10) to locate the corresponding `.mdebug` sidecar, and if found, load the stripped DWARF symbols from the sidecar into the debugger session
7. WHEN `--sandbox` is specified, THE CLI SHALL launch the binary within an SRT sandbox derived from the binary's `.meld` manifest effect map, enforcing the same effect restrictions during the debug session as would apply in production
8. WHEN `--dap` is specified, THE CLI SHALL start a DAP server (on `--port`, default 4711) that wraps the underlying LLDB-DAP or GDB/MI interface, allowing IDE clients to attach for a graphical debugging experience
9. WHEN the target binary was not compiled with debug symbols and no `.mdebug` sidecar is found, THE CLI SHALL print a warning: `warning: target binary lacks debug symbols — consider rebuilding with 'meld build --debug' or 'meld build --release' (for .mdebug sidecar)`
10. WHEN the specified PID does not exist or cannot be attached to (in `--attach` mode), THE CLI SHALL print `error: cannot attach to process <pid>: <reason>` and exit with a non-zero exit code
11. THE CLI SHALL pass through all standard debugger commands to the underlying LLDB/GDB session after setup is complete
12. THE `meld debug` command SHALL NOT conflict with `meld run --debug`, which remains the entry point for interpreted DAP debugging (Tier 1); `meld debug` is exclusively for compiled native binaries (Tiers 2 and 3)

> **Cross-reference:** The `.mdebug` sidecar format is defined in `.kiro/specs/meld-manifest/requirements.md` Req 17. The Tombstone `debug_id` field is defined in `.kiro/specs/meld-manifest/requirements.md` Req 2. The daemon's sidecar resolution API is defined in `.kiro/specs/meld-daemon/requirements.md` Req 10. The AOT pipeline's `.mdebug` emission is defined in `.kiro/specs/meld-compiler/requirements.md` Req 32. The LLDB data formatters are defined in Req 12C of this spec.

---

> **Note:** Requirement 21 below was added to address the Production Supervisor gap identified in RUNTIME_REQUIREMENTS.md §2.

### Requirement 21: `melds` Production Supervisor Binary

**User Story:** As a platform engineer, I want a standalone minimalist production binary that verifies binary integrity and enforces the Effect Firewall via OS-level sandboxing, so that production deployments have "Ironclad" security without the overhead of the full development daemon.

#### Acceptance Criteria

1. THE `melds` binary SHALL be a standalone C++20 executable, separate from the unified `meld` CLI, built as a statically linked or minimal dynamic binary for portability across production containers and VMs
2. THE `melds` binary SHALL explicitly exclude the AST parser, LLVM backend, MCP server, and AI context — it contains only Sigstore verification logic, the Tombstone parser, the manifest parser, and the `SandboxProvider` interface
3. WHEN `melds <binary_path> [args...]` is invoked, IT SHALL execute the following chain: (a) parse the `.note.meld` Tombstone section, (b) verify the Sigstore Bundle against the configured trust root, (c) read the co-located `.meld` manifest to determine the required `@effect` permissions, (d) translate the effect policy into an SRT or MicroVM sandbox configuration, (e) execute the binary via `execvp`, replacing the current process
4. IF any verification check fails (signature mismatch, hash mismatch, missing manifest, untrusted root), THE `melds` binary SHALL print `INTEGRITY_FAILURE: <details>` to stderr and exit with code 127
5. THE `melds` binary SHALL add less than 50ms to total startup time (excluding VM boot for Tier 3), measured from invocation to `execvp` of the sandboxed binary
6. THE `melds` binary SHALL support a "Static Execution" mode where it pre-caches the sandbox policy for a specific binary, achieving near-zero overhead on subsequent invocations
7. THE `melds` binary SHALL support `--offline` mode (default) using a locally pre-installed `trusted_root.json` for air-gapped verification, and `--online` mode for Rekor revocation checking
8. THE `melds` binary SHALL read the sandbox tier selection from the co-located `meld.toml` `[execution]` section or from `--isolation=[process|microvm]` CLI flag, defaulting to `process` (SRT)
9. THE `melds` binary SHALL log the Sigstore Identity and Effect Policy to a structured audit stream on each process launch, providing a record of who signed the code and what it was allowed to do
10. THE `melds` binary SHALL be the recommended `ENTRYPOINT` for production Docker containers and Lambda-like functions: `ENTRYPOINT ["melds", "./app.bin"]`

> **Cross-reference:** The Tombstone format is defined in `.kiro/specs/meld-manifest/requirements.md` Req 2. The manifest format is defined in Req 1. The SandboxProvider interface is defined in Req 10. The verification modes are defined in Req 7. The `meldn` signing utility is defined in Req 8. The `meld run --strict` flag (Req 2.17 of this spec) delegates to `melds` for local testing.

---

> **Note:** Requirement 22 below was added to address the VFS Bridge gap identified in RUNTIME_REQUIREMENTS.md §3.5.

### Requirement 22: VFS Bridge Mode for AI Agent Dry Runs

**User Story:** As an AI agent, I want to run code in a temporary, memory-only virtual filesystem provided by the sandbox, so that I can perform "Dry Run" tests without persisting any side effects to the real filesystem.

#### Acceptance Criteria

1. WHEN `meld run --vfs <file_or_binary>` is invoked, THE CLI SHALL execute the program within a sandbox that provides a memory-only virtual filesystem mount as the working directory
2. THE VFS mount SHALL be a temporary in-memory filesystem (e.g., `tmpfs` on Linux, memory-backed mount on macOS) that is created before execution and destroyed after the process exits
3. THE VFS mode SHALL allow the executed program to read from the real filesystem (source tree, dependencies) according to its effect policy, but all write operations SHALL be redirected to the memory-only mount
4. WHEN the sandboxed process exits, THE CLI SHALL discard the VFS contents by default, or copy specified output files to the real filesystem when `--vfs-output <path>` is specified
5. THE VFS mode SHALL be composable with other flags: `meld run --vfs --agent-test`, `meld run --vfs --strict`, `meld run --vfs --debug`
6. THE VFS mode SHALL integrate with the `SandboxProvider` interface (`.kiro/specs/meld-manifest/requirements.md` Req 10) by adding a `vfs_mode` boolean to the `SandboxConfig` struct
7. WHEN `--vfs` is combined with `--strict`, THE CLI SHALL delegate to `melds` with VFS enforcement, testing the full production sandbox with ephemeral writes

> **Cross-reference:** The `SandboxProvider` interface is defined in `.kiro/specs/meld-manifest/requirements.md` Req 10. The `SandboxConfig` struct is defined in Req 10.3. The `--agent-test` flag is defined in Req 19 of this spec.


---

> **Note:** Requirement 23 below wraps the existing `meldn` notary utility (`.kiro/specs/meld-manifest/requirements.md` Req 8) as a first-class `meld` subcommand, providing a unified user-facing entry point for binary signing, verification, and air-gapped bundle preparation.

### Requirement 23: Binary Signing CLI (`meld sign`)

**User Story:** As a Meld developer or release engineer, I want `meld sign` to be the user-facing entry point for signing, verifying, and bundling Meld binaries, so that I can manage binary integrity from the unified CLI without invoking the separate `meldn` utility directly.

#### Acceptance Criteria

1. THE CLI SHALL provide a `meld sign <binary>` command that delegates to the `meldn` signing infrastructure to sign the specified compiled binary using the default Sigstore keyless mode (OIDC identity via Fulcio + Rekor)
2. WHEN `meld sign <binary>` is invoked, THE SignModule SHALL locate the co-located `.meld` manifest sidecar, compute the Combined_Integrity_Hash (`code_hash + manifest_hash + debug_id`), and produce a signed Tombstone embedded in the binary
3. WHEN `meld sign --key <path> <binary>` is invoked, THE SignModule SHALL sign the binary using the specified private key file instead of Sigstore keyless mode
4. WHEN `meld sign --verify <binary>` is invoked, THE SignModule SHALL read the binary's Tombstone and co-located manifest, verify the Combined_Integrity_Hash against the `signature_blob`, and report pass or fail with details of any failed check
5. WHEN `meld sign --verify --offline <binary>` is invoked, THE SignModule SHALL verify the Sigstore Bundle's Signed Entry Timestamp against the bundled trust root without network access
6. WHEN `meld sign --verify --online <binary>` is invoked, THE SignModule SHALL re-query the Rekor transparency log to check for certificate revocation or compromise in addition to local verification
7. WHEN `meld sign --bundle <binary>` is invoked, THE SignModule SHALL fetch the necessary Rekor Signed Entry Timestamps and Fulcio certificates and embed them into the binary's Tombstone as a self-contained Sigstore Bundle, preparing the binary for offline verification in air-gapped environments
8. IF the manifest's `code_hash` does not match the actual binary content when signing, THEN THE SignModule SHALL print a structured error identifying the hash mismatch and exit with a non-zero exit code without signing
9. IF the co-located `.meld` manifest sidecar is not found, THEN THE SignModule SHALL print `error: manifest not found for binary: <path>` and exit with a non-zero exit code
10. IF any verification check fails (signature mismatch, hash mismatch, revoked certificate), THEN THE SignModule SHALL print a structured report identifying the specific failed check and exit with a non-zero exit code
11. WHEN `meld sign --fulcio-url <url>` or `meld sign --rekor-url <url>` is specified, THE SignModule SHALL use the specified endpoints instead of the public Sigstore infrastructure, enabling private Sigstore instances for air-gapped or enterprise deployments
12. THE SignModule SHALL read default Fulcio and Rekor endpoint configuration from the `[signing]` section of `meld.toml` when CLI flags are not provided
13. WHEN `meld sign` is invoked with `--json`, THE SignModule SHALL format all output (signing result, verification report, bundle status) as structured JSON consistent with the CLI's `--json` output convention (Req 15.3)

> **Cross-reference:** The `meldn` notary utility is defined in `.kiro/specs/meld-manifest/requirements.md` Req 8. The Combined Integrity Hash is defined in Req 5. Sigstore/Cosign integration is defined in Req 6. Verification modes are defined in Req 7. The Tombstone format is defined in Req 2. The manifest format is defined in Req 1.

---

> **Note:** Requirement 24 below provides a user-facing CLI entry point for managing the `meldd` daemon lifecycle, following Bazel's command-line reference pattern (e.g., `bazel shutdown`). The daemon itself is fully specified in `.kiro/specs/meld-daemon/requirements.md` (Req 1–12).

### Requirement 24: Daemon Management CLI (`meld daemon`)

**User Story:** As a Meld developer, I want `meld daemon` to manage the lifecycle of the `meldd` daemon process from the unified CLI, so that I can start, stop, inspect, and troubleshoot the daemon without invoking the `meldd` binary directly.

#### Acceptance Criteria

1. WHEN `meld daemon start` is invoked, THE DaemonModule SHALL start a new `meldd` daemon process for the current workspace, or connect to an existing daemon if one is already running for the same workspace root
2. WHEN `meld daemon start --workspace=<path>` is invoked, THE DaemonModule SHALL start or connect to a daemon bound to the specified project root directory instead of the current working directory
3. IF a daemon is already running for the target workspace, THEN THE DaemonModule SHALL print the existing daemon's PID and connection details and exit with exit code 0 without starting a duplicate
4. WHEN `meld daemon stop` is invoked, THE DaemonModule SHALL send a graceful shutdown signal to the running daemon for the current workspace, allowing it to complete in-flight requests, terminate active sandboxed processes, clean up stale SRT policy files, and exit cleanly (per `.kiro/specs/meld-daemon/requirements.md` Req 8.3)
5. IF no daemon is running for the current workspace when `meld daemon stop` is invoked, THEN THE DaemonModule SHALL print `info: no daemon running for workspace: <path>` and exit with exit code 0
6. WHEN `meld daemon status` is invoked, THE DaemonModule SHALL query the running daemon and display: running/stopped state, daemon PID, workspace root path, uptime, number of connected LSP clients, number of connected MCP clients, SemanticModel indexing progress (if still initializing), and active sandboxed process count
7. IF no daemon is running when `meld daemon status` is invoked, THEN THE DaemonModule SHALL print `stopped` as the state and exit with exit code 0
8. WHEN `meld daemon restart` is invoked, THE DaemonModule SHALL perform a graceful stop of the running daemon followed by a fresh start, equivalent to `meld daemon stop && meld daemon start`
9. WHEN `meld daemon logs` is invoked, THE DaemonModule SHALL tail the daemon's log output to the terminal, streaming new log entries as they are produced (similar to `tail -f`)
10. WHEN `meld daemon logs --lines=<N>` is invoked, THE DaemonModule SHALL display the last N lines of the daemon log before entering tail mode
11. WHEN `meld daemon logs --no-follow` is invoked, THE DaemonModule SHALL print the current log contents and exit immediately without tailing
12. WHEN any `meld daemon` subcommand is invoked with `--json`, THE DaemonModule SHALL format all output as structured JSON consistent with the CLI's `--json` output convention (Req 15.3)
13. THE DaemonModule SHALL locate the daemon process via a PID file stored at `<workspace>/.meld/daemon.pid`, written by `meldd` on startup and removed on clean shutdown
14. IF the PID file exists but the referenced process is not running (stale PID file), THEN THE DaemonModule SHALL remove the stale PID file and treat the daemon as stopped
15. WHEN `meld daemon start` is invoked with `--foreground`, THE DaemonModule SHALL run the daemon in the foreground (attached to the terminal) instead of daemonizing, useful for debugging and CI environments
16. WHEN `meld daemon start` is invoked with `--timeout=<seconds>`, THE DaemonModule SHALL pass the timeout to the daemon process, causing it to exit automatically after the specified idle time with no active LSP/MCP connections (per `.kiro/specs/meld-daemon/requirements.md` Req 8.5)

> **Cross-reference:** The `meldd` daemon architecture is defined in `.kiro/specs/meld-daemon/requirements.md` Req 1. Graceful startup/shutdown behavior is defined in Req 8. The daemon's `--workspace` and `--timeout` flags are defined in Req 1.6 and Req 8.5 respectively. The LSP and MCP channels are defined in Req 1.3.


---

> **Note:** Requirement 25 below was added to address the `meld vm` CLI suite from UPDATES-3.md, providing developer control over the background Alpine Lima VM that hosts Firecracker and containerd on macOS.

### Requirement 25: VM Management CLI (`meld vm`)

**User Story:** As a Meld developer on macOS, I want `meld vm` to manage the background Alpine Linux VM that hosts Firecracker and containerd, so that I can control the sandbox host without memorizing raw `limactl` or `nerdctl` syntax.

#### Acceptance Criteria

1. WHEN `meld vm status` is invoked on macOS, THE VM_Module SHALL run `limactl ls` and display: whether the `meld-vm` instance is running or stopped, current RAM and CPU usage, and the number of active MicroVMs and containers
2. WHEN `meld vm start` is invoked on macOS, THE VM_Module SHALL run `limactl start meld-vm` to boot the background Alpine host; if the VM is already running, THE VM_Module SHALL print the current status and exit with exit code 0
3. WHEN `meld vm stop` is invoked on macOS, THE VM_Module SHALL run `limactl stop meld-vm` to shut down the background Alpine host, freeing the allocated memory and CPU resources
4. WHEN `meld vm restart` is invoked on macOS, THE VM_Module SHALL perform a hard reboot of the `meld-vm` instance by stopping and starting it, useful for recovering from tangled TAP devices or KVM permission issues
5. WHEN `meld vm shell` is invoked on macOS, THE VM_Module SHALL run `limactl shell meld-vm` to drop the developer into a root shell inside the Alpine host, enabling direct inspection of `/dev/kvm`, `iptables`, `containerd`, and Firecracker state
6. WHEN `meld vm prune` is invoked on macOS, THE VM_Module SHALL run `nerdctl system prune` inside the `meld-vm` instance and wipe stale socket files under `/tmp/meld/*.sock` to recover disk space from old MicroVM and container executions
7. WHEN `meld vm logs` is invoked on macOS, THE VM_Module SHALL tail the system logs for Firecracker and containerd inside the Lima VM, streaming new entries as they are produced
8. WHEN `meld vm logs --no-follow` is invoked, THE VM_Module SHALL print the current log contents and exit immediately without tailing
9. WHEN any `meld vm` subcommand is invoked on native Linux (where `/dev/kvm` is directly accessible), THE VM_Module SHALL print `info: Native Linux detected, VMM bridge disabled` and exit with exit code 0, since Lima is not needed on Linux — except for `meld vm status`, which SHALL report the native KVM status (available/unavailable) and the host containerd status
10. WHEN any `meld vm` subcommand is invoked with `--json`, THE VM_Module SHALL format all output as structured JSON consistent with the CLI's `--json` output convention (Req 15.3)
11. THE VM_Module SHALL be registered as a `meld vm` subcommand in the `CommandDispatcher` with its own help text and subcommand listing

> **Cross-reference:** The Lima VM instance `meld-vm` is defined in `.kiro/specs/meld-supervisor/requirements.md` Req 1. The daemon's Lima lifecycle management is defined in `.kiro/specs/meld-daemon/requirements.md` Req 14. The Alpine-based `meld-vmm.yaml` template is defined in the toolchain installer.

---

> **Note:** Requirement 26 below was added to address the `--isolation` flag from UPDATES-3.md, allowing developers to override the isolation backend for local execution.

### Requirement 26: Isolation Backend Override (`meld run --isolation`)

**User Story:** As a Meld developer, I want `meld run --isolation=<backend>` to override the isolation strategy for a single execution, so that I can test my binary under different isolation backends (Finch containers, Firecracker MicroVMs, or SRT process sandboxing) without modifying `meld.toml`.

#### Acceptance Criteria

1. THE CLI SHALL support an `--isolation=<backend>` flag on the `meld run` subcommand, where valid backends are: `srt` (process-level sandboxing), `finch` (OCI container via containerd/nerdctl), and `microvm` (Firecracker MicroVM)
2. WHEN `--isolation` is specified, THE InterpreterModule SHALL override the `local` key from the `[isolation]` section of `meld.toml` for that single execution
3. WHEN `--isolation` is not specified, THE InterpreterModule SHALL read the `local` key from `meld.toml` `[isolation]` section (default: `srt`)
4. WHEN `--isolation=finch` is specified, THE CLI SHALL delegate execution to `melds` with the FinchProvider, launching the binary inside an Alpine OCI container with `meldi` as PID 1
5. WHEN `--isolation=microvm` is specified, THE CLI SHALL delegate execution to `melds` with the MicroVMProvider, booting a Firecracker MicroVM with the Alpine ext4 rootfs and `meldi` as PID 1
6. WHEN `--isolation=srt` is specified, THE CLI SHALL execute the binary within an SRT process-level sandbox derived from the binary's effect manifest, without hardware isolation
7. THE `--isolation` flag SHALL be composable with other `meld run` flags: `--debug`, `--strict`, `--watch`, `--agent-test`, `--vfs`, etc.
8. WHEN `--isolation` is combined with `--strict`, THE CLI SHALL apply both the isolation backend override and production-grade Sigstore verification

> **Cross-reference:** The `[isolation]` configuration block is defined in `.kiro/specs/meld-supervisor/requirements.md` Req 11. The FinchProvider is defined in Req 13. The MicroVMProvider is defined in Req 3. The SRT sandbox is defined in `.kiro/specs/meld-manifest/requirements.md` Req 10–13.

### Requirement 27: Unified Module Command (meld module)

**User Story:** As a Meld developer, I want `meld module` to be the single unified command for all package and dependency management, consolidating the former `meld mod` and `meld package` functionality, so that there is one clear entry point for managing modules.

#### Acceptance Criteria

1. WHEN the CLI initializes, THE CLI_Core SHALL register a lazy handler for the `module` command that creates a `ModuleCommandHandler` instance.
2. WHEN a user invokes `meld module`, THE CLI_Dispatcher SHALL route the input to the Module_Command_Handler.
3. THE Module_Command_Handler SHALL be a separate `cc_library` target named `module_command` in the `meld-cli/BUILD.bazel` file with dependencies on `command_dispatcher` and `error_handler`.
4. WHEN a user invokes `meld module` without a subcommand, THE Module_Command_Handler SHALL display help text listing all subcommands (`install`, `uninstall`, `list`, `update`, `fetch`, `clean`) and return `InvalidArguments`.
5. THE Module_Command_Handler SHALL replace the existing `PackageModule` handler; the `meld mod` and `meld package` command names SHALL be removed from the dispatcher.
6. THE CLI SHALL use `meld.lock` for reproducible builds, fetching exact pinned commits when the lock file is present.

### Requirement 27A: Module Fetch Subcommand (meld module fetch)

**User Story:** As a Meld developer, I want `meld module fetch` to download all Git dependencies declared in `meld.toml`, so that I can pull dependencies without building them.

#### Acceptance Criteria

1. WHEN `meld module fetch` is invoked, THE Module_Command_Handler SHALL download all Git dependencies declared in `meld.toml` to `~/.meld/cache/` without building them.
2. WHEN `meld module fetch` completes successfully, THE Module_Command_Handler SHALL print a summary of fetched dependencies and their resolved versions.
3. WHEN a dependency declared in `meld.toml` cannot be fetched (network error, invalid URL, missing tag), THEN THE Module_Command_Handler SHALL report the specific dependency and failure reason and return `Error`.

### Requirement 27B: Module Clean Subcommand (meld module clean)

**User Story:** As a Meld developer, I want `meld module clean` to purge the local dependency cache, so that I can reclaim disk space or force a fresh fetch.

#### Acceptance Criteria

1. WHEN `meld module clean` is invoked, THE Module_Command_Handler SHALL purge the local dependency cache at `~/.meld/cache/` and print a confirmation message.

### Requirement 28: Module Install Subcommand (meld module install)

**User Story:** As a developer, I want to install packages using `meld module install <package>`, so that I can add dependencies to my project.

#### Acceptance Criteria

1. WHEN a user invokes `meld module install` with a valid Package_Spec, THE Module_Command_Handler SHALL install the specified package and print a success message including the package name and installed version.
2. WHEN a user invokes `meld module install` without a Package_Spec, THE Module_Command_Handler SHALL print an error message indicating that a package name is required and return `InvalidArguments`.
3. WHEN a user invokes `meld module install` with the `--dev` flag, THE Module_Command_Handler SHALL mark the package as a development dependency.
4. IF the specified package cannot be found or installation fails, THEN THE Module_Command_Handler SHALL print a descriptive error message and return `Error`.

### Requirement 29: Module Uninstall Subcommand (meld module uninstall)

**User Story:** As a developer, I want to uninstall packages using `meld module uninstall <package>`, so that I can remove dependencies from my project.

#### Acceptance Criteria

1. WHEN a user invokes `meld module uninstall` with a valid package name, THE Module_Command_Handler SHALL remove the specified package and print a success message.
2. WHEN a user invokes `meld module uninstall` without a package name, THE Module_Command_Handler SHALL print an error message indicating that a package name is required and return `InvalidArguments`.
3. IF the specified package is not installed, THEN THE Module_Command_Handler SHALL print an error message indicating the package was not found and return `Error`.

### Requirement 30: Module List Subcommand (meld module list)

**User Story:** As a developer, I want to list installed packages using `meld module list`, so that I can see what dependencies are in my project.

#### Acceptance Criteria

1. WHEN a user invokes `meld module list`, THE Module_Command_Handler SHALL display all installed packages with their names and versions.
2. WHEN no packages are installed, THE Module_Command_Handler SHALL display a message indicating no packages are installed and return `Success`.

### Requirement 31: Module Update Subcommand (meld module update)

**User Story:** As a developer, I want to update packages using `meld module update`, so that I can get the latest compatible versions of my dependencies.

#### Acceptance Criteria

1. WHEN a user invokes `meld module update` without arguments, THE Module_Command_Handler SHALL resolve the latest allowed tags or commits for each dependency, update `meld.lock` accordingly, and print a summary of updated packages.
2. WHEN a user invokes `meld module update` with a specific package name, THE Module_Command_Handler SHALL update only the specified package and its `meld.lock` entry.
3. IF no updates are available, THEN THE Module_Command_Handler SHALL print a message indicating all packages are up to date and return `Success`.

### Requirement 32: Module Argument Validation

**User Story:** As a developer, I want clear error messages when I misuse the `meld module` command, so that I can correct my input.

#### Acceptance Criteria

1. WHEN a user invokes `meld module` with an unrecognized subcommand, THE Module_Command_Handler SHALL print an error message listing the valid subcommands (`install`, `uninstall`, `list`, `update`, `fetch`, `clean`) and return `InvalidArguments`.
2. THE Module_Command_Handler SHALL validate that subcommands requiring a package name (`install`, `uninstall`) receive at least one positional argument.
3. WHEN the `--help` flag is passed to any subcommand, THE Module_Command_Handler SHALL display usage information for that subcommand and return `Success`.

### Requirement 33: Module Help Text and Completions

**User Story:** As a developer, I want helpful documentation and tab completions for `meld module`, so that I can discover available subcommands.

#### Acceptance Criteria

1. THE Module_Command_Handler SHALL provide help text that lists all subcommands with descriptions and usage examples.
2. THE Module_Command_Handler SHALL provide tab-completion candidates for partial subcommand input matching `install`, `uninstall`, `list`, `update`, `fetch`, and `clean`.
