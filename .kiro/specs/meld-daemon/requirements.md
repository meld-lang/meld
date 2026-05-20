# Meld Daemon (`meldd`) — Requirements

## Introduction

This specification defines the requirements for `meldd`, the unified Meld daemon — a single long-lived C++20 process that serves as the central orchestrator of the Meld toolchain. The daemon holds a memory-resident AST, Symbol Table, and Bazel dependency graph, and exposes this shared semantic state through two communication channels: JSON-RPC (LSP) for human IDEs and MCP-HTTP/SSE for AI agents. Both channels observe the same state with zero drift.

Beyond serving as a language server, the daemon acts as a persistent Bazel worker (keeping the LLVM context hot), synchronizes the dependency graph with `meld.toml`, manages sandbox policy lifecycle, verifies binary integrity before execution, and formats all responses in a dual-voice format (human-readable + machine-readable).

This spec covers REQUIREMENTS.md §6.1 (Core Architecture), §6.2 (Bazel Worker Protocol), §6.3 (Dependency Graph Sync), §6.4 (SRT Lifecycle Management), §6.5 (Integrity Verification), and §6.6 (Dual-Voice Response).

**Cross-references:**
- `.kiro/specs/meld-manifest/requirements.md` — Req 12 (Ephemeral Policy Lifecycle), Req 13 (SRT CLI Integration); the daemon delegates sandbox policy generation and cleanup to the SandboxProvider
- `.kiro/specs/meld-manifest/requirements.md` — Req 7 (Verification Modes); the daemon performs pre-execution integrity verification using the manifest/tombstone chain
- `.kiro/specs/meld-compiler/requirements.md` — Req 6 (ORC JIT Dev Server), Req 9 (Bazel Integration Rules); the daemon's Bazel worker protocol complements the dev server and build rules
- `.kiro/specs/meld-build/requirements.md` — Req 8–9 (meld.toml parsing); the daemon observes `meld.toml` and triggers Bazel sync to keep the dependency graph aligned

## Glossary

- **meldd**: The Meld daemon binary — a single long-lived C++20 process serving both LSP and MCP channels from a shared semantic state
- **SemanticModel**: The daemon's in-memory representation of the entire workspace: parsed ASTs, symbol tables, type information, effect annotations, and resolved dependency graph
- **Bazel_Worker_Protocol**: The persistent worker interface (`WorkRequest`/`WorkResponse` protobuf messages over stdin/stdout) that allows Bazel to reuse the daemon's LLVM context across compilation actions
- **WorkRequest**: A protobuf message from Bazel containing the compilation arguments and input file paths for a single build action
- **WorkResponse**: A protobuf message from the daemon back to Bazel containing the exit code, output, and diagnostics for a completed build action
- **Dependency_Graph**: The daemon's in-memory representation of the Bazel-resolved dependency tree, synchronized with `meld.toml` and `MODULE.bazel`
- **Dual_Voice_Response**: A response format where every daemon output contains both a `message` field (human-readable prose) and an `agent_context`/`agent_data` field (machine-readable metadata: rule IDs, AST selectors, fix suggestions)
- **File_Watcher**: A filesystem notification mechanism (inotify on Linux, FSEvents on macOS) that triggers incremental re-analysis when source files change
- **Incremental_Reanalysis**: The process of re-parsing and re-checking only the changed portions of the AST when a file is modified, rather than reprocessing the entire workspace
- **AST_RWLock**: A read-write lock on the SemanticModel ensuring both LSP and MCP observers see a consistent snapshot — multiple readers (queries) can proceed concurrently, but writes (re-analysis) are exclusive
- **LLVM_Context_Pool**: The daemon's persistent LLVM context and module cache, kept "hot" across Bazel worker invocations to avoid re-initialization overhead
- **Client_Editor**: Any LSP-compatible editor or IDE (VS Code, Neovim, etc.) that connects to the daemon's LSP channel
- **Meld_Parser**: The existing Boost Spirit X3 parser for Meld language, reused by the LSP channel for parsing and diagnostics
- **AI_Agent**: An artificial intelligence system that interacts with the daemon's MCP channel
- **Structured_Diagnostic_Frame (SDF)**: A standardized error format where every diagnostic includes `rule_id`, `ast_selector`, `context_hash`, and `version_hash`, with fix-it suggestions as atomic AST_Patch objects
- **Proof_Carrying_Failure**: A memory safety violation diagnostic that includes a trace of Origin → Escape → Conflict
- **AST_Patch**: An atomic, machine-applicable code transformation expressed as an AST-level operation (insert, replace, delete node)
- **VFS (Virtual File System)**: An in-memory file overlay that allows agents to preview code changes without modifying disk
- **Code_Mode**: An advanced MCP interaction pattern where AI agents write and execute Meld scripts against the SemanticModel via the Tier 0 sandbox
- **AST_Pattern**: A code template containing Metavariable placeholders that is parsed into an AST and matched structurally against indexed ASTs — used for pattern-based search and rewrite operations inspired by ast-grep
- **Metavariable**: A placeholder token in an AST_Pattern: `$NAME` matches any single AST node, `$$$NAME` matches zero or more consecutive AST nodes (variadic); captured nodes can be referenced by name in rewrite replacements


## Requirements

### Requirement 1: Unified Single-Binary Architecture

**User Story:** As a Meld developer, I want a single daemon process that serves both my IDE and AI agents from the same semantic state, so that I never see inconsistencies between what my editor shows and what an agent reports.

#### Acceptance Criteria

1. THE `meldd` binary SHALL be a single long-lived C++20 process that initializes once and runs until explicitly stopped or the workspace is closed
2. THE daemon SHALL hold a memory-resident SemanticModel containing the parsed AST, Symbol Table, type information, effect annotations, and resolved dependency graph for the entire workspace
3. THE daemon SHALL expose two communication channels from the same process: JSON-RPC (LSP) for human IDEs and MCP-HTTP/SSE for AI agents
4. BOTH the LSP and MCP channels SHALL read from the same SemanticModel instance, ensuring zero semantic drift between human and agent views
5. THE daemon SHALL use an AST_RWLock (read-write lock) on the SemanticModel so that multiple LSP/MCP read queries can proceed concurrently while write operations (re-analysis) are exclusive
6. THE daemon SHALL accept a `--workspace=<path>` argument specifying the project root directory

### Requirement 2: Incremental File Watching and Re-Analysis

**User Story:** As a Meld developer, I want the daemon to detect file changes and re-analyze only what changed, so that diagnostics and completions update in sub-millisecond time without reprocessing the entire project.

#### Acceptance Criteria

1. THE daemon SHALL register a File_Watcher (inotify on Linux, FSEvents on macOS) on the workspace directory tree for `.meld` source files and `meld.toml`
2. WHEN a `.meld` file is modified, THE daemon SHALL perform Incremental_Reanalysis: re-parse only the changed file, update its AST node in the SemanticModel, and re-run type checking and effect inference for the changed file and its direct dependents
3. THE daemon SHALL NOT re-parse unchanged files during incremental re-analysis
4. THE Incremental_Reanalysis SHALL complete and publish updated diagnostics within 100ms for typical single-file changes in projects with up to 10,000 source files
5. WHEN a new file is created or an existing file is deleted, THE daemon SHALL update the SemanticModel's module graph accordingly
6. THE daemon SHALL debounce rapid file-change events (e.g., during a `git checkout`) to avoid redundant re-analysis, with a configurable debounce window (default 50ms)

### Requirement 3: Bazel Worker Protocol

**User Story:** As a build system integrator, I want the daemon to act as a persistent Bazel worker, so that compilation actions reuse the daemon's LLVM context and avoid cold-start overhead on every build action.

#### Acceptance Criteria

1. THE daemon SHALL implement the Bazel persistent worker protocol, reading `WorkRequest` protobuf messages from stdin and writing `WorkResponse` protobuf messages to stdout
2. THE daemon SHALL maintain an LLVM_Context_Pool that persists across WorkRequest invocations, avoiding re-initialization of the LLVM context, target machine, and standard library AST on each compile
3. WHEN a WorkRequest arrives, THE daemon SHALL compile the specified `.meld` source files to `.bc` bitcode using the persistent LLVM context and the SemanticModel's cached type/effect information
4. THE daemon SHALL perform incremental AST invalidation: only re-parse files within the Bazel target that have changed since the last WorkRequest for that target
5. THE WorkResponse SHALL include the exit code (0 for success, non-zero for failure) and any diagnostic messages in the output field
6. IF the daemon encounters an unrecoverable error during a WorkRequest (e.g., LLVM crash), THE daemon SHALL return a non-zero exit code in the WorkResponse and reset the LLVM context for the next request, rather than terminating the daemon process

### Requirement 4: Dependency Graph Synchronization

**User Story:** As a Meld developer, I want the daemon to automatically keep its dependency graph in sync with `meld.toml`, so that LSP completions and MCP queries reflect the current project dependencies without manual restarts.

#### Acceptance Criteria

1. THE daemon SHALL be the primary observer of `meld.toml` via the File_Watcher
2. WHEN `meld.toml` is modified, THE daemon SHALL re-parse the dependency declarations and compare them against the current in-memory Dependency_Graph
3. IF dependencies have changed, THE daemon SHALL trigger `bazel sync` (or equivalent repository resolution) to update `MODULE.bazel` and fetch new external dependencies
4. AFTER Bazel sync completes, THE daemon SHALL run `bazel query` to obtain the updated dependency closure and refresh the in-memory Dependency_Graph
5. THE daemon SHALL publish updated diagnostics to both LSP and MCP channels after dependency graph refresh (e.g., new unresolved imports become resolved, or removed dependencies cause new errors)
6. THE daemon SHALL handle `meld.toml` parse errors gracefully by retaining the previous valid dependency graph and emitting a diagnostic on the `meld.toml` file

### Requirement 5: SRT Lifecycle Management

**User Story:** As a Meld developer, I want the daemon to manage sandbox policy files automatically, so that I don't have to manually create or clean up SRT configuration for each execution.

#### Acceptance Criteria

1. WHEN the daemon receives an execution request (via `meld run`, `meld test`, or MCP tool invocation), THE daemon SHALL generate an ephemeral `srt-settings.json` by reading the target binary's `.meld` manifest and constructing a `SandboxConfig`
2. THE daemon SHALL delegate policy generation and sandboxed process launching to the `SandboxProvider` interface (see `meld-manifest` spec Req 10)
3. WHEN the sandboxed process exits, THE daemon SHALL delete the ephemeral `srt-settings.json` file
4. ON daemon startup, THE daemon SHALL scan the temporary directory for stale `srt-settings.json` files from previous sessions and delete them
5. THE daemon SHALL track all active sandboxed processes and forcefully terminate any that exceed a configurable timeout (default 5 minutes for test, 0/unlimited for run)

### Requirement 6: Pre-Execution Integrity Verification

**User Story:** As a security-conscious developer, I want the daemon to verify binary integrity before execution, so that tampered binaries are never run.

#### Acceptance Criteria

1. BEFORE spawning a sandboxed process, THE daemon SHALL read the target binary's `.note.meld` Tombstone section and the co-located `.meld` manifest
2. THE daemon SHALL verify the Tombstone's `signature_blob` against the manifest content using the configured trust root (Sigstore bundle or public key)
3. THE daemon SHALL verify that the manifest's `code_hash` matches the actual hash of the binary's executable segments
4. THE daemon SHALL verify that the Tombstone's `manifest_hash` matches the actual hash of the manifest file
5. IF any verification check fails, THE daemon SHALL refuse to spawn the process and emit a structured audit log entry containing: the binary path, the specific check that failed, and the expected vs actual hash values
6. THE verification mode (offline vs online) SHALL be read from `meld.toml` or daemon configuration, as specified in `meld-manifest` Req 7

### Requirement 7: Dual-Voice Response Format

**User Story:** As a tool integrator, I want every daemon response to contain both human-readable and machine-readable content, so that IDEs can display friendly messages while agents can parse structured metadata.

#### Acceptance Criteria

1. EVERY diagnostic, error, and informational response from the daemon SHALL contain a `message` field with human-readable prose suitable for display in an IDE
2. EVERY diagnostic, error, and informational response from the daemon SHALL contain an `agent_context` field with machine-readable metadata including: `rule_id` (unique identifier for the diagnostic rule), `ast_selector` (JSONPath-like path to the relevant AST node), and `context_hash` (hash of the surrounding code context for deduplication)
3. WHEN the daemon produces a fix suggestion, THE `agent_context` SHALL include a `fix` object containing an atomic AST-Patch that agents can apply via the `apply_patch` MCP tool
4. THE LSP channel SHALL serialize the `message` field as the primary diagnostic text and include `agent_context` in the diagnostic's `data` field (LSP 3.17 `Diagnostic.data`)
5. THE MCP channel SHALL serialize both `message` and `agent_context` as top-level fields in the JSON response
6. THE dual-voice format SHALL be consistent across all daemon subsystems: compiler diagnostics, dependency resolution errors, sandbox violations, and integrity verification failures

### Requirement 8: Graceful Startup and Shutdown

**User Story:** As a Meld developer, I want the daemon to start up quickly and shut down cleanly, so that IDE responsiveness is not impacted and no resources are leaked.

#### Acceptance Criteria

1. ON startup, THE daemon SHALL perform a lazy initialization: start accepting LSP/MCP connections immediately, then index the workspace in the background, publishing diagnostics incrementally as files are processed
2. THE daemon SHALL report its initialization progress via LSP `window/workDoneProgress` notifications so the IDE can display a progress indicator
3. ON shutdown (SIGTERM or LSP `shutdown` request), THE daemon SHALL: stop accepting new requests, wait for in-flight WorkRequests to complete (with a 10-second timeout), terminate any active sandboxed processes, clean up stale SRT policy files, and exit with code 0
4. IF the daemon receives SIGKILL or crashes, THE next daemon startup SHALL detect and clean up orphaned resources (stale policy files, zombie sandboxed processes)
5. THE daemon SHALL support a `--timeout=<seconds>` flag that causes it to exit automatically after the specified idle time (no active LSP/MCP connections), useful for CI environments


---

> **Note:** Requirement 9 below was added to address the AI Developer Experience (AI_DX.md) gap: intent-based symbol discovery via vectorized embeddings.

### Requirement 9: Vector-Augmented Symbol Index

**User Story:** As an AI agent working in a large Meld project, I want the daemon to maintain a semantic vector index of all symbols, so that I can discover relevant functions and types by describing what I need in natural language rather than knowing exact names.

#### Acceptance Criteria

1. THE daemon SHALL maintain a `VectorIndex` component within the SemanticModel that indexes all exported symbols (functions, classes, effects, types) by semantic embedding alongside their name, type signature, and effect profile
2. THE daemon SHALL define an `EmbeddingProvider` abstract interface with a pure virtual method `embed(text) -> std::vector<float>` that converts a text description into a fixed-dimensional embedding vector
3. THE daemon SHALL provide a reference `OnnxEmbeddingProvider` implementation that uses ONNX Runtime to run a small local embedding model, configurable via `meld.toml` (`[daemon.embeddings]` section: `provider`, `model_path`, `dimensions`)
4. THE daemon SHALL provide a `PassthroughEmbeddingProvider` fallback that disables vector search when no embedding model is configured, causing `find_intent` queries to fall back to keyword-based symbol search
5. THE `VectorIndex` SHALL be updated incrementally during Incremental_Reanalysis (Req 2) — when a file changes, only the symbols in that file are re-embedded and re-indexed
6. THE `VectorIndex` SHALL support approximate nearest-neighbor search returning the top-K symbols most similar to a query embedding, with configurable K (default 10)
7. WHEN a symbol has a `@blueprint` annotation, THE `VectorIndex` SHALL embed the concatenation of the symbol's signature, the blueprint's `summary`, and the blueprint's `spec` action descriptions, producing a richer embedding than symbols without blueprints
8. THE `VectorIndex` SHALL persist its index to disk (in the workspace's `.meld/` directory) so that daemon restarts do not require full re-indexing; the persisted index SHALL be invalidated when the embedding model configuration changes
9. THE `VectorIndex` SHALL complete initial indexing of a 10,000-symbol workspace within 30 seconds on startup (background, non-blocking per Req 8.1)
10. THE `EmbeddingProvider` interface SHALL be stateless per call — each `embed()` invocation is independent, allowing the daemon to swap providers without restarting

> **Cross-reference:** The `find_intent` MCP tool that queries this index is defined in Req 34 of this spec. The `@blueprint` enrichment is defined in `.kiro/specs/meld-core/requirements.md` Req 13 and Req 140.

---

> **Note:** Requirement 10 below was added to address the Shadow Debugging gap identified in NEW_REQUIREMENTS_2.md §5.

### Requirement 10: Debug Sidecar Resolution

**User Story:** As a debugger or crash analysis tool, I want the daemon to resolve a binary's `debug_id` to the corresponding `.mdebug` sidecar, so that shadow symbols can be loaded on demand without requiring the developer to manually locate debug artifacts.

#### Acceptance Criteria

1. THE daemon SHALL provide a `resolve_debug_sidecar(debug_id) -> std::optional<std::filesystem::path>` API that accepts a `debug_id` (from a binary's Tombstone) and returns the filesystem path to the corresponding `.mdebug` sidecar
2. THE daemon SHALL search for the `.mdebug` sidecar in the following locations, in order: (a) co-located with the binary (same directory, same base name with `.mdebug` extension), (b) the workspace's `.meld/debug/` cache directory, (c) the Bazel output tree (`bazel-bin/`)
3. WHEN the daemon locates a `.mdebug` sidecar, IT SHALL verify that the sidecar's header `debug_id` matches the requested `debug_id` before returning the path
4. IF no matching `.mdebug` sidecar is found in any search location, THE daemon SHALL return an empty result and emit a structured diagnostic suggesting the developer rebuild with `meld build --release` to generate the sidecar
5. THE daemon SHALL cache resolved `debug_id → path` mappings in memory so that repeated lookups for the same binary are O(1)
6. WHEN a new build produces updated artifacts, THE daemon SHALL invalidate cached sidecar paths for binaries whose `debug_id` has changed

> **Cross-reference:** The `.mdebug` format is defined in `.kiro/specs/meld-manifest/requirements.md` Req 17. The AOT pipeline emission is defined in `.kiro/specs/meld-compiler/requirements.md` Req 32. The `meld debug` orchestrator that consumes this API is defined in `.kiro/specs/meld-cli/requirements.md` Req 20.

---

> **Note:** Requirement 11 below was added to address the Deterministic Runtime gap identified in NEW_REQUIREMENTS_2.md §7.

### Requirement 11: Deterministic Runtime Context

**User Story:** As an AI agent executing code via MCP tools, I want the daemon to provide a deterministic execution context that virtualizes time, entropy, and scheduling, so that identical code produces identical results across runs and I can reproduce any previous execution by replaying its configuration.

#### Acceptance Criteria

1. THE daemon SHALL provide a `DeterministicContext` component that, when activated, installs deterministic effect handlers for time, entropy, and task scheduling into the execution environment
2. THE `DeterministicContext` SHALL install a `DeterministicTimeHandler` that intercepts `EffectTime` operations: `Time.now()` returns a configurable epoch (default `2024-01-01T00:00:00Z`) that advances by a configurable fixed increment per call (default `1ms`); `Time.sleep()` advances the virtual clock by the requested duration without actual delay
3. THE `DeterministicContext` SHALL install a `DeterministicRandomHandler` that intercepts `EffectRandom` operations: `Random.next()`, `Random.nextInt()`, and related functions delegate to a seeded PRNG with a configurable seed (default `0`); `Random.seed()` calls are ignored (seed is locked)
4. THE `DeterministicContext` SHALL install a `DeterministicScheduler` that replaces the concurrent task executor with a sequential executor that processes tasks in spawn order, ensuring deterministic interleaving regardless of system load
5. THE `DeterministicContext` SHALL accept a `DeterministicConfig` struct containing: `seed` (uint64, default 0), `epoch` (ISO-8601 string, default `2024-01-01T00:00:00Z`), and `time_increment_ms` (uint32, default 1)
6. THE `DeterministicContext` SHALL be activated explicitly — it is NOT installed by default; activation is triggered by MCP tool parameters (Req 36 of this spec) or the CLI `--agent-test` flag (`.kiro/specs/meld-cli/requirements.md` Req 19)
7. WHEN a `DeterministicContext` is active, THE daemon SHALL include the `DeterministicConfig` used in every MCP tool response, so that agents can capture and replay the exact configuration for reproduction
8. THE deterministic handlers SHALL be standard algebraic effect handlers — no new kernel primitives are required; the handlers intercept `perform` calls for `EffectTime`, `EffectRandom`, and the executor scheduling policy

> **Cross-reference:** The MCP tool `deterministic` parameter is defined in Req 36 of this spec. The CLI `--agent-test` flag is defined in `.kiro/specs/meld-cli/requirements.md` Req 19. The algebraic effects system is defined in `.kiro/specs/meld-core/requirements.md` Req 41 and Req 99–118.

---

> **Note:** Requirement 12 below was added to address the Runner Handshake gap identified in RUNTIME_REQUIREMENTS.md §3.1.

### Requirement 12: Binary Freshness Check API

**User Story:** As the `meld run` command executing a compiled binary, I want to query the daemon to check whether the binary is up-to-date with respect to the current source, so that stale binaries are automatically rebuilt before execution without manual intervention.

#### Acceptance Criteria

1. THE daemon SHALL provide a `check_binary_freshness(binary_path) -> FreshnessResult` API accessible via both the LSP and MCP channels, where `FreshnessResult` contains: `is_current` (bool), `stale_sources` (list of changed source files), and `rebuild_triggered` (bool)
2. WHEN `check_binary_freshness` is called, THE daemon SHALL compare the binary's build timestamp (from Bazel output metadata) against the modification times of its transitive source dependencies in the SemanticModel's dependency graph
3. IF the binary is stale (source files have been modified since the last build), THE daemon SHALL automatically trigger a `bazel build` for the corresponding target through the Bazel Worker Protocol (Req 3) and set `rebuild_triggered = true` in the response
4. THE daemon SHALL block the `check_binary_freshness` call until the triggered rebuild completes (or fails), returning the updated `FreshnessResult` with the rebuild outcome
5. IF the rebuild fails, THE daemon SHALL return `is_current = false`, `rebuild_triggered = true`, and include the build failure diagnostics in the response so that `meld run` can report them to the user or AI agent
6. THE daemon SHALL cache freshness results so that repeated checks for the same binary within the debounce window (Req 2.6) return immediately without re-querying Bazel
7. WHEN a file change is detected by the FileWatcher (Req 2), THE daemon SHALL invalidate cached freshness results for any binary whose transitive dependency set includes the changed file

> **Cross-reference:** The Bazel Worker Protocol is defined in Req 3 of this spec. The `meld run` daemon handshake that calls this API is defined in `.kiro/specs/meld-cli/requirements.md` Req 2.14. The FileWatcher and debounce behavior are defined in Req 2 of this spec.

---

> **Note:** Requirement 13 below was added to address the "Meld Code Mode" Tier 0 sandbox from UPDATES-2.md, providing the execution environment for the `execute_script` MCP tool.

### Requirement 13: Tier 0 Agent Script Sandbox

**User Story:** As an AI agent, I want the daemon to host an in-process JIT sandbox that can compile and execute ephemeral Meld scripts in under 5 milliseconds, so that I can perform complex Semantic Model queries via `execute_script` without the overhead of a full build pipeline or OS-level sandbox.

#### Acceptance Criteria

1. THE daemon SHALL provide a `Tier0Sandbox` component that accepts a Meld source string, JIT-compiles it in-memory, and executes it within an isolated context, returning the script's output string
2. THE `Tier0Sandbox` SHALL use the LLVM ORC JIT engine (or WebAssembly via Wasmtime) for in-memory compilation, completely bypassing the Bazel build pipeline
3. THE `Tier0Sandbox` SHALL achieve script startup (compile + execute) in under 5 milliseconds for typical agent scripts (under 100 lines)
4. THE `Tier0Sandbox` SHALL limit each script's memory footprint to a configurable maximum (default 16MB), terminating the script if the limit is exceeded
5. THE `Tier0Sandbox` SHALL limit each script's execution time to a configurable maximum (default 5000ms), terminating the script if the timeout is exceeded
6. THE `Tier0Sandbox` SHALL provide the script with NO filesystem access, NO network access, and NO raw I/O — the only way for the script to interact with the host is through injected RPC bindings
7. THE daemon SHALL inject RPC bindings into the `Tier0Sandbox` that provide typed access to the SemanticModel: `compiler.get_ast_node(id)`, `compiler.query_symbol(name)`, `compiler.list_symbols(module)`, `compiler.trace_effects(expr)`, `compiler.get_diagnostics(file)`, and other Semantic Model query functions
8. THE RPC bindings SHALL be read-only — scripts SHALL NOT be able to modify the SemanticModel, the AST, or any source files through the sandbox
9. THE `Tier0Sandbox` SHALL be destroyed immediately after the script returns or is terminated, releasing all memory and JIT-compiled code
10. THE `Tier0Sandbox` SHALL support concurrent script executions (multiple `execute_script` calls in flight), each in an independent sandbox instance with its own memory and time limits
11. WHEN a script fails to compile, THE `Tier0Sandbox` SHALL return structured compilation diagnostics in the SDF format (Req 31 of this spec)
12. WHEN a script encounters a runtime error, THE `Tier0Sandbox` SHALL capture the error (including stack trace within the script) and return it as a structured diagnostic

> **Cross-reference:** The `execute_script` MCP tool that invokes this sandbox is defined in Req 37 of this spec. The JIT compilation path is defined in `.kiro/specs/meld-compiler/requirements.md`. The `Tier0Provider` sandbox abstraction is defined in `.kiro/specs/meld-manifest/requirements.md`.


---

> **Note:** Requirement 14 below was added to address the multi-workspace Lima coordination and VM persistence model from UPDATES-3.md.

### Requirement 14: Lima VM Lifecycle Management

**User Story:** As a Meld developer on macOS, I want the daemon to automatically manage the background Lima VM lifecycle, so that the sandbox host is transparently available when I need it and doesn't waste resources when I don't.

#### Acceptance Criteria

1. ON macOS, WHEN the daemon starts and detects that the `meld-vm` Lima instance is stopped, THE daemon SHALL start it in the background via `limactl start meld-vm` and wait for the KVM probe to pass before accepting sandbox execution requests
2. THE daemon SHALL use a POSIX file lock at `/tmp/meld-vmm.lock` to coordinate Lima VM lifecycle across multiple concurrent `meldd` instances (one per workspace)
3. WHEN the daemon acquires the file lock and the `meld-vm` instance is stopped, THE daemon SHALL start the Lima VM and hold the lock until the VM is running
4. WHEN the daemon starts and the file lock is already held by another daemon instance, THE daemon SHALL wait for the lock to be released, then verify the VM is running via `limactl ls` and connect to the existing instance
5. THERE SHALL be exactly one `meld-vm` Lima instance running globally on the Mac, regardless of how many `meldd` daemon instances are active across different workspaces
6. WHEN the daemon shuts down (graceful or crash), IT SHALL release the file lock; the last daemon to shut down SHALL issue `limactl stop meld-vm` to free the Mac's RAM and CPU resources
7. THE daemon SHALL periodically check (every 60 seconds) whether any other `meldd` instances are still running; if the current daemon is the last one alive and no sandbox executions are in flight, IT SHALL stop the Lima VM after a configurable idle timeout (default 5 minutes)
8. ON Linux, THE daemon SHALL skip all Lima lifecycle management — `/dev/kvm` and `containerd` are available natively, and the `meld-vm` instance is not needed
9. THE daemon SHALL expose the Lima VM status via the `check_vm_status()` API accessible from both LSP and MCP channels, returning: running/stopped state, allocated memory, active MicroVM count, and active container count
10. WHEN the Lima VM fails to start (QEMU error, insufficient resources, KVM unavailable), THE daemon SHALL emit a structured diagnostic explaining the failure and suggesting remediation (e.g., "Run `meld vm start` manually or check that virtualization is enabled in your BIOS")
11. THE daemon SHALL log all Lima lifecycle events (start, stop, lock acquire, lock release) to the daemon's audit log for debugging

> **Cross-reference:** The `meld vm` CLI commands that wrap Lima operations are defined in `.kiro/specs/meld-cli/requirements.md` Req 25. The Lima instance `meld-vm` and its Alpine configuration are defined in `.kiro/specs/meld-supervisor/requirements.md` Req 1. The file lock coordination ensures that the architecture supports N daemons (one per workspace) sharing 1 global Lima VM.

---

> **Note:** Requirements 15–21 below consolidate the LSP protocol features previously specified in `.kiro/specs/meld-lsp-server/`. The LSP server is hosted as a channel within the daemon (Req 1.3), sharing the SemanticModel. These requirements define the language features exposed through that channel.

### Requirement 15: LSP Syntax Highlighting and Language Recognition

**User Story:** As a developer, I want syntax highlighting and basic language recognition for Meld files, so that I can easily read and understand Meld code structure.

#### Acceptance Criteria

1. WHEN a Client_Editor opens a .meld file THEN the LspChannel SHALL provide semantic tokens for syntax highlighting
2. WHEN the LspChannel processes Meld source code THEN it SHALL identify and classify language constructs including symbols, literals, keywords, operators, and comments
3. WHEN syntax errors are present THEN the LspChannel SHALL provide diagnostic information with precise error locations
4. WHEN the file contains valid Meld syntax THEN the LspChannel SHALL parse the content without errors
5. WHEN the LspChannel encounters parsing errors THEN it SHALL provide recovery mechanisms to continue processing the rest of the file

> **Cross-reference:** Parsing uses the Meld_Parser from `.kiro/specs/meld-compiler/requirements.md` Req 1–4. Diagnostics are formatted via the DualVoiceFormatter (Req 7).

### Requirement 16: LSP Code Completion and Signature Help

**User Story:** As a developer, I want intelligent code completion and suggestions, so that I can write Meld code efficiently with proper syntax and available symbols.

#### Acceptance Criteria

1. WHEN a developer types in a Meld file THEN the LspChannel SHALL provide context-aware completion suggestions based on the current scope
2. WHEN completing symbol names THEN the LspChannel SHALL suggest available variables, functions, types, and imported symbols from the SemanticModel
3. WHEN typing function calls THEN the LspChannel SHALL provide signature help with parameter information and documentation
4. WHEN completing type annotations THEN the LspChannel SHALL suggest valid type names including built-in types, user-defined types, and type aliases
5. WHEN completing within specific contexts THEN the LspChannel SHALL filter suggestions appropriately for the syntactic position

### Requirement 17: LSP Real-Time Diagnostics and Type Checking

**User Story:** As a developer, I want real-time error detection and type checking, so that I can identify and fix issues while writing code.

#### Acceptance Criteria

1. WHEN the LspChannel analyzes Meld code THEN it SHALL validate syntax according to the Meld grammar specification
2. WHEN type mismatches occur THEN the LspChannel SHALL report type errors with clear descriptions and suggested fixes
3. WHEN undefined symbols are referenced THEN the LspChannel SHALL report undefined symbol errors with suggestions for similar names
4. WHEN refinement type constraints are violated THEN the LspChannel SHALL validate logical predicates and report constraint violations
5. WHEN multiple dispatch function calls are ambiguous THEN the LspChannel SHALL detect and report dispatch resolution errors

> **Cross-reference:** Type checking integrates with the SemanticModel's type cache (Req 1.2). Refinement types and multiple dispatch are Meld-specific features defined in `.kiro/specs/meld-core/requirements.md`.

### Requirement 18: LSP Code Navigation

**User Story:** As a developer, I want code navigation features, so that I can quickly move between related code elements and understand code structure.

#### Acceptance Criteria

1. WHEN a developer requests "go to definition" THEN the LspChannel SHALL navigate to the declaration of symbols, functions, types, and variables
2. WHEN a developer requests "find references" THEN the LspChannel SHALL locate all usages of a symbol across the workspace using the SemanticModel
3. WHEN a developer requests document symbols THEN the LspChannel SHALL provide a hierarchical outline of functions, types, and declarations in the current file
4. WHEN a developer requests workspace symbols THEN the LspChannel SHALL provide searchable access to all symbols across the entire workspace
5. WHEN hovering over symbols THEN the LspChannel SHALL display type information, documentation, and signature details

### Requirement 19: LSP Code Formatting and Refactoring

**User Story:** As a developer, I want code formatting and refactoring support, so that I can maintain consistent code style and safely restructure code.

#### Acceptance Criteria

1. WHEN a developer requests document formatting THEN the LspChannel SHALL format Meld code according to standard style conventions
2. WHEN a developer requests range formatting THEN the LspChannel SHALL format only the selected code region while preserving surrounding formatting
3. WHEN a developer requests symbol renaming THEN the LspChannel SHALL safely rename symbols across all references in the workspace
4. WHEN renaming would cause conflicts THEN the LspChannel SHALL detect naming conflicts and prevent unsafe renames
5. WHEN formatting code THEN the LspChannel SHALL preserve semantic meaning while improving readability and consistency

### Requirement 20: LSP Workspace Management

**User Story:** As a developer, I want the LSP channel to provide accurate analysis across multiple files and dependencies.

#### Acceptance Criteria

1. WHEN a Workspace is opened THEN the LspChannel SHALL discover and index all Meld source files recursively via the SemanticModel
2. WHEN files are added, modified, or deleted THEN the LspChannel SHALL update the workspace index incrementally (leveraging Req 2 IncrementalAnalyzer)
3. WHEN analyzing cross-file references THEN the LspChannel SHALL resolve imports, exports, and module dependencies correctly using the SemanticModel's DependencyGraph
4. WHEN processing large workspaces THEN the LspChannel SHALL provide responsive performance through incremental parsing and caching
5. WHEN workspace configuration changes THEN the LspChannel SHALL reload and reindex affected files appropriately

> **Cross-reference:** Workspace indexing and incremental analysis are handled by the daemon's FileWatcher (Req 2) and IncrementalAnalyzer. The LspChannel reads from the shared SemanticModel (Req 1.2).

### Requirement 21: LSP Meld-Specific Language Feature Support

**User Story:** As a developer, I want the LSP channel to understand and support Meld-specific constructs.

#### Acceptance Criteria

1. WHEN analyzing homoiconic code THEN the LspChannel SHALL understand the relationship between AST representation and runtime values
2. WHEN processing macro definitions THEN the LspChannel SHALL provide appropriate support for meta-macro system constructs
3. WHEN validating refinement types THEN the LspChannel SHALL evaluate logical predicates and constraint expressions
4. WHEN analyzing multiple dispatch functions THEN the LspChannel SHALL resolve dispatch based on all argument types
5. WHEN processing tree initialization syntax THEN the LspChannel SHALL validate constructor block syntax and nested property assignments

> **Cross-reference:** Meld language features are defined in `.kiro/specs/meld-core/requirements.md`. The LspChannel delegates analysis to the SemanticModel's type checker and effect inference engine.

---

> **Note:** Requirements 22–37 below consolidate the MCP protocol features previously specified in `.kiro/specs/meld-mcp-server/`. The MCP server is hosted as a channel within the daemon (Req 1.3), sharing the SemanticModel. These requirements define the tools and capabilities exposed through the MCP channel.

### Requirement 22: MCP Codebase Exploration

**User Story:** As an AI agent, I want to explore and understand Meld codebases through structured interfaces, so that I can provide intelligent assistance with Meld development.

#### Acceptance Criteria

1. WHEN an AI_Agent connects to the McpChannel THEN it SHALL provide discovery of available Meld projects and workspaces
2. WHEN requesting codebase structure THEN the McpChannel SHALL provide hierarchical organization of files, modules, and symbols from the SemanticModel
3. WHEN exploring code artifacts THEN the McpChannel SHALL expose AST representations, symbol tables, and type information
4. WHEN analyzing dependencies THEN the McpChannel SHALL provide import/export relationships and module dependency graphs from the DependencyGraph
5. WHEN accessing documentation THEN the McpChannel SHALL extract and provide inline comments, docstrings, and metadata annotations

### Requirement 23: MCP Code Semantic Analysis

**User Story:** As an AI agent, I want to analyze Meld code semantics and structure, so that I can understand program behavior and provide accurate code suggestions.

#### Acceptance Criteria

1. WHEN analyzing Meld syntax THEN the McpChannel SHALL parse code using the official Meld grammar and provide detailed AST information
2. WHEN performing type analysis THEN the McpChannel SHALL validate refinement types, multiple dispatch signatures, and type constraints
3. WHEN examining control flow THEN the McpChannel SHALL analyze Meld's unique control flow patterns including pattern matching and functional constructs
4. WHEN processing macro definitions THEN the McpChannel SHALL understand meta-macro system constructs and expansion rules
5. WHEN validating code correctness THEN the McpChannel SHALL identify syntax errors, type mismatches, and semantic violations

### Requirement 24: MCP Code Search and Query

**User Story:** As an AI agent, I want to search and query Meld codebases efficiently, so that I can find relevant code patterns and examples quickly.

#### Acceptance Criteria

1. WHEN searching by symbol name THEN the McpChannel SHALL locate all definitions, references, and usages across the codebase
2. WHEN querying by type signature THEN the McpChannel SHALL find functions and methods matching specified type patterns
3. WHEN searching for code patterns THEN the McpChannel SHALL identify similar constructs, idioms, and implementation approaches
4. WHEN filtering by language features THEN the McpChannel SHALL locate code using specific Meld constructs like multiple dispatch or refinement types
5. WHEN performing semantic search THEN the McpChannel SHALL leverage the VectorIndex (Req 9) to find functionally similar implementations

### Requirement 25: MCP Code Generation and Validation

**User Story:** As an AI agent, I want to generate and validate Meld code, so that I can assist with code creation and ensure correctness.

#### Acceptance Criteria

1. WHEN generating code snippets THEN the McpChannel SHALL validate syntax according to Meld grammar rules
2. WHEN creating type definitions THEN the McpChannel SHALL ensure type safety and constraint satisfaction
3. WHEN generating function implementations THEN the McpChannel SHALL respect multiple dispatch rules and signature compatibility
4. WHEN producing complete modules THEN the McpChannel SHALL validate import/export consistency and dependency requirements
5. WHEN formatting generated code THEN the McpChannel SHALL apply standard Meld style conventions and formatting rules

### Requirement 26: MCP Code Transformations

**User Story:** As an AI agent, I want to perform code transformations and refactoring operations, so that I can help improve and restructure Meld code safely.

#### Acceptance Criteria

1. WHEN renaming symbols THEN the McpChannel SHALL update all references while preserving semantic correctness
2. WHEN extracting functions THEN the McpChannel SHALL maintain proper scoping, type signatures, and multiple dispatch compatibility
3. WHEN reorganizing modules THEN the McpChannel SHALL update import/export statements and dependency relationships
4. WHEN applying design patterns THEN the McpChannel SHALL transform code while preserving behavior and maintaining Meld idioms
5. WHEN optimizing code THEN the McpChannel SHALL suggest improvements while ensuring functional equivalence

### Requirement 27: MCP Project Metadata Access

**User Story:** As an AI agent, I want to access Meld project metadata and configuration, so that I can understand project structure and build requirements.

#### Acceptance Criteria

1. WHEN accessing project configuration THEN the McpChannel SHALL provide build settings, dependencies, and compilation options
2. WHEN examining project structure THEN the McpChannel SHALL expose directory organization, module hierarchies, and file relationships
3. WHEN analyzing build artifacts THEN the McpChannel SHALL provide information about generated files, compilation outputs, and intermediate representations
4. WHEN accessing version control information THEN the McpChannel SHALL integrate with Git to provide change history and branch information
5. WHEN examining test suites THEN the McpChannel SHALL identify test files, test cases, and coverage information

### Requirement 28: MCP Development Tool Integration

**User Story:** As an AI agent, I want to integrate with Meld development tools and workflows, so that I can provide seamless assistance within existing development environments.

#### Acceptance Criteria

1. WHEN integrating with build systems THEN the McpChannel SHALL interface with Bazel to understand compilation processes
2. WHEN accessing compiler outputs THEN the McpChannel SHALL provide compilation errors, warnings, and diagnostic information via the DualVoiceFormatter (Req 7)
3. WHEN interfacing with debuggers THEN the McpChannel SHALL support debugging information and runtime analysis capabilities via the DebugSidecarResolver (Req 10)
4. WHEN connecting to testing frameworks THEN the McpChannel SHALL execute tests and provide results and coverage data
5. WHEN integrating with documentation tools THEN the McpChannel SHALL generate and update API documentation and code examples

### Requirement 29: MCP Named Tool Definitions

**User Story:** As an AI agent, I want a well-defined set of named MCP tools with clear input/output contracts, so that I can invoke specific compiler and analysis capabilities by name.

#### Acceptance Criteria

1. THE McpChannel SHALL expose an `analyze_safety` tool that accepts a file path or module name and returns a JSON-L stream of memory safety violations
2. THE McpChannel SHALL expose a `query_lifecycle` tool that accepts a symbol reference and returns its ownership state (`Hold[T]` or `View[T]`), scope information, and validity
3. THE McpChannel SHALL expose a `trace_effect` tool that accepts an expression or function reference and returns the required Effect Firewall permissions
4. THE McpChannel SHALL expose a `resolve_version_conflict` tool that accepts a dependency coordinate and returns identified version collisions with suggested resolution strategies
5. THE McpChannel SHALL expose a `dry_run_patch` tool that accepts code transformations and verifies them against the Semantic Analyzer on the VFS without writing to disk
6. THE McpChannel SHALL expose an `apply_patch` tool that accepts an AST_Patch and applies it to source files, with compiler verification on the VFS before committing to disk
7. THE McpChannel SHALL expose a `diagnose_build` tool that accepts a build target and returns structured build errors with AST-patch fix suggestions
8. THE McpChannel SHALL expose a `verify_closure` tool that returns the full Bazel-resolved version tree for the workspace

> **Cross-reference:** `trace_effect` delegates to `query_required_effects` in `.kiro/specs/meld-core/requirements.md` Req 109. All tool executions are sandboxed per `.kiro/specs/meld-manifest/requirements.md` Req 15.

### Requirement 30: MCP Structured Diagnostic Frames (SDF)

**User Story:** As an AI agent, I want every error and warning from the MCP channel to follow a structured diagnostic format with machine-parseable identifiers, so that I can programmatically match errors to fix strategies.

#### Acceptance Criteria

1. EVERY diagnostic returned by any MCP tool SHALL include a `rule_id` field — a unique, stable identifier for the diagnostic rule
2. EVERY diagnostic SHALL include an `ast_selector` field — a JSONPath-like path to the relevant AST node
3. EVERY diagnostic SHALL include a `context_hash` field — a hash of the surrounding code context for staleness detection
4. EVERY diagnostic SHALL include a `version_hash` field — the version hash of the module containing the diagnostic
5. WHEN a diagnostic has an available fix, THE diagnostic SHALL include a `fix` field containing one or more AST_Patch objects
6. THE SDF format SHALL be consistent across all MCP tools

> **Cross-reference:** The daemon's dual-voice response format (Req 7) wraps SDF diagnostics with both human-readable `message` and machine-readable `agent_context` fields.

### Requirement 31: MCP Proof-Carrying Failures

**User Story:** As an AI agent diagnosing memory safety issues, I want violation diagnostics to include a full provenance trace, so that I can understand the causal chain and generate targeted fixes.

#### Acceptance Criteria

1. WHEN `analyze_safety` reports a memory safety violation, THE diagnostic SHALL include a `provenance_trace` with three phases: Origin, Escape, and Conflict
2. EACH phase in the `provenance_trace` SHALL include: the source location, the AST selector, and a human-readable description
3. THE Origin phase SHALL identify the allocation or binding that created the reference
4. THE Escape phase SHALL identify where the reference was passed beyond its safe scope
5. THE Conflict phase SHALL identify the specific access that is unsafe
6. THE `provenance_trace` SHALL be included in the SDF `agent_context` field

> **Cross-reference:** The memory model (`Hold[T]`/`View[T]`) is defined in `.kiro/specs/meld-core/requirements.md` (Req 119–129).

### Requirement 32: MCP Executable Spec Streaming (get_module_specs)

**User Story:** As an AI agent importing an unfamiliar module, I want to retrieve its compiler-verified usage examples as structured Action-Result pairs, so that I can understand the module's API through guaranteed-correct examples.

#### Acceptance Criteria

1. THE McpChannel SHALL expose a `get_module_specs` tool that accepts a module coordinate and returns all `@blueprint` spec blocks for that module's exported symbols
2. THE response SHALL return each spec as a structured object containing: symbol name, action source text, expected result, declared effects, and verification status
3. WHEN compiled with `--release`, THE verification status SHALL be `verified` for passing specs; otherwise `unverified`
4. THE tool SHALL support an optional `symbol` parameter to filter specs for a specific exported symbol
5. THE tool SHALL include `@blueprint` `summary` and `rules` fields alongside spec pairs
6. THE response SHALL use SDF format (Req 30) for retrieval failure diagnostics
7. WHEN a module has no spec blocks, THE tool SHALL return an empty list with fallback to `@blueprint` `examples` field if present

> **Cross-reference:** The `@blueprint` spec block syntax is defined in `.kiro/specs/meld-core/requirements.md` Req 140.

### Requirement 33: MCP Intent-Based Symbol Discovery (find_intent)

**User Story:** As an AI agent working in a large Meld project, I want to search for functions and types by describing what I need in natural language, so that I can discover relevant APIs without knowing exact names.

#### Acceptance Criteria

1. THE McpChannel SHALL expose a `find_intent` tool that accepts a natural language query and returns a ranked list of matching symbols
2. THE response SHALL return each match with: symbol name, module coordinate, type signature, effect profile, SRT security profile, relevance score, and summary
3. THE tool SHALL search across all exported symbols in the workspace and its resolved dependencies
4. THE tool SHALL leverage the daemon's VectorIndex (Req 9) for semantic matching
5. THE tool SHALL support optional `scope` (workspace/dependencies/all) and `max_results` (default 10) parameters
6. WHEN a symbol has `@blueprint`, THE tool SHALL use the blueprint's `summary` and `spec` fields to enrich match quality
7. THE tool SHALL return results within 200ms for workspaces with up to 100,000 indexed symbols

### Requirement 34: MCP Intent-Based Refactoring (refactor)

**User Story:** As an AI agent, I want to express refactoring operations as high-level intents rather than low-level AST patches, so that I can request changes without manually computing AST selectors.

#### Acceptance Criteria

1. THE McpChannel SHALL expose a `refactor` tool that accepts a high-level refactoring intent and returns concrete `AST_Transform` objects
2. THE tool SHALL support intent types: `rename`, `extract_function`, `move_symbol`, `change_signature`, and `inline`
3. THE intent SHALL be expressed as a structured object with `intent_type`, `target`, and intent-specific parameters
4. THE tool SHALL resolve the intent against the SemanticModel to produce concrete `AST_Transform` objects
5. THE tool SHALL validate transforms via `validate_transforms` before returning, including any validation diagnostics
6. THE tool SHALL support a `dry_run` parameter (default true); when false, transforms are applied via VFS and committed to disk
7. THE tool SHALL return transforms in the `AST_Transform` JSON schema
8. WHEN a refactoring intent is ambiguous, THE tool SHALL return a disambiguation response listing candidates

> **Cross-reference:** The `AST_Transform` wire format is defined in `.kiro/specs/meld-compiler/requirements.md` Req 29.

### Requirement 35: MCP Deterministic Execution Mode

**User Story:** As an AI agent, I want to explicitly request deterministic execution when running code through MCP tools, so that I can guarantee reproducible results.

#### Acceptance Criteria

1. ALL MCP tools that execute code SHALL accept an optional `deterministic` boolean parameter (default `false`)
2. WHEN `deterministic` is `true`, THE McpChannel SHALL activate the daemon's `DeterministicContext` (Req 11) for the duration of the tool execution
3. ALL code-executing tools SHALL also accept an optional `deterministic_config` object with: `seed`, `epoch`, and `time_increment_ms`
4. WHEN `deterministic` is `true` and `deterministic_config` is omitted, defaults SHALL be used (seed=0, epoch=`2024-01-01T00:00:00Z`, time_increment_ms=1)
5. WHEN `deterministic` is `true`, THE response SHALL include a `deterministic_config` field echoing the exact configuration used
6. WHEN `deterministic` is `false` or omitted, code SHALL execute with real time, entropy, and concurrent scheduling
7. THE `deterministic` parameter SHALL NOT affect static analysis tools (`analyze_safety`, `query_lifecycle`, `trace_effect`, `find_intent`, `get_module_specs`, `refactor`)

### Requirement 36: MCP Code Mode Tools (search_api + execute_script)

**User Story:** As an AI agent, I want to write and execute Meld scripts directly against the SemanticModel, so that I can perform complex multi-step queries in a single round-trip instead of chaining multiple sequential tool calls.

#### Acceptance Criteria

1. THE McpChannel SHALL expose a `search_api` tool that accepts a query and returns concise Meld `struct`/`interface` definitions of the compiler's internal AST and SemanticModel API surface
2. THE `search_api` response SHALL return type definitions in Meld interface syntax and include available RPC binding signatures
3. THE McpChannel SHALL expose an `execute_script` tool that accepts a Meld source string and executes it in the daemon's Tier0Sandbox (Req 13)
4. THE `execute_script` tool SHALL return the script's output as a string
5. THE tool SHALL accept optional `timeout_ms` (default 5000) and `memory_limit_mb` (default 16) parameters
6. WHEN execution fails, THE response SHALL use SDF format (Req 30) distinguishing compilation from runtime failures
7. THE `execute_script` tool SHALL NOT provide filesystem, network, or I/O access beyond RPC bindings
8. THE `search_api` and `execute_script` tools SHALL coexist alongside the named tools (Req 29)
9. THE `execute_script` tool SHALL support the `deterministic` parameter (Req 35)

> **Cross-reference:** The Tier 0 sandbox is defined in Req 13 of this spec.

---

> **Note:** Requirement 37 below extends the Code Mode tools defined in Req 36 with AST pattern-based structural search and rewriting, inspired by [ast-grep](https://ast-grep.github.io). Where Req 36.1 provides substring matching on AST node names and kinds (essentially grep over the AST), Req 37 adds the ability to express structural queries using code patterns with Metavariable wildcards — e.g., "find all functions that take an `&mut` parameter and return `Result`". The structural rewrite capability extends `execute_script` (Req 36.3) with safe, AST-level find-and-replace. The VectorIndex (Req 9) is used for candidate pre-filtering in large workspaces.

### Requirement 37: AST Pattern-Based Structural Search and Rewrite

**User Story:** As an AI agent, I want to search for code by structural shape using code patterns with wildcard placeholders, and apply structural rewrites that preserve AST correctness, so that I can perform precise codebase queries and refactoring that substring matching cannot express.

#### Acceptance Criteria

1. THE `search_api` tool (Req 36.1) SHALL accept an optional `pattern` parameter containing a Meld code pattern with Metavariable placeholders
2. WHEN a `pattern` parameter is provided, THE `search_api` tool SHALL parse the pattern into an AST template and match it structurally against indexed ASTs, instead of performing substring matching
3. THE pattern parser SHALL recognize single-node Metavariables (`$NAME`) that match exactly one AST node, and variadic Metavariables (`$$$NAME`) that match zero or more consecutive AST nodes
4. WHEN a structural match is found, THE `search_api` tool SHALL return results in the same `ApiDefinition` format as substring matches (Req 36.1), with each result including the matched source location and a `captures` map of Metavariable names to their matched AST node text
5. THE `execute_script` tool (Req 36.3) SHALL accept an optional `transform` parameter containing a `pattern` string and a `replacement` string, both using Metavariable syntax
6. WHEN a `transform` parameter is provided, THE `execute_script` tool SHALL find all structural matches of the `pattern` in the target scope and substitute each match with the `replacement`, where captured Metavariable values from the pattern are substituted into the replacement by name
7. THE structural rewrite SHALL operate on AST structure rather than raw text — the replacement template is parsed as an AST fragment and Metavariable substitution produces a well-formed AST
8. WHEN a `transform` is applied, THE `execute_script` tool SHALL return a list of applied changes, each containing: the file path, the matched source range, the before snippet, and the after snippet
9. THE structural rewrite SHALL execute within the Tier0Sandbox (Req 13) and respect the same `timeout_ms` and `memory_limit_mb` limits as regular script execution
10. IF a `pattern` fails to parse into a valid AST template (e.g., syntax error outside of Metavariable positions), THE tool SHALL return an SDF diagnostic (Req 30) describing the parse failure location within the pattern
11. IF a `replacement` template references a Metavariable name that was not captured by the `pattern`, THE tool SHALL return an SDF diagnostic identifying the unbound Metavariable
12. THE pattern matching engine SHALL complete a full-workspace structural search within 500ms for workspaces containing up to 10,000 indexed source files
13. WHEN both `pattern` and `query` parameters are provided to `search_api`, THE tool SHALL first filter candidate files using the substring `query` and then apply structural pattern matching only to the filtered set, combining both search modes for performance
14. THE structural rewrite SHALL NOT modify source files on disk — changes are applied to the VFS (Req 29.5) and committed only when the agent explicitly calls `apply_patch`

> **Cross-reference:** The `search_api` and `execute_script` tools are defined in Req 36 of this spec. The Tier 0 sandbox is defined in Req 13. The VFS preview and `apply_patch` commit flow are defined in Req 29.5–29.6. The VectorIndex (Req 9) may be used for candidate pre-filtering alongside substring queries. The SDF diagnostic format is defined in Req 30.
