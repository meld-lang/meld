# Implementation Plan: Meld Daemon (`meldd`)

## Overview

Implements the unified Meld daemon — a single long-lived C++20 process that holds a memory-resident SemanticModel and exposes it through two communication channels: JSON-RPC (LSP) for human IDEs and MCP-HTTP/SSE for AI agents. The daemon also acts as a persistent Bazel worker, synchronizes the dependency graph with `meld.toml`, manages sandbox policy lifecycle, verifies binary integrity before execution, formats all responses in a dual-voice format, maintains a vector-augmented symbol index, resolves debug sidecars, provides a deterministic execution context, checks binary freshness, hosts an in-process Tier 0 JIT sandbox, and manages the Lima VM lifecycle on macOS.

Implementation follows the dependency order: Core Architecture → File Watching → Bazel Worker → Dependency Sync → SRT Lifecycle → Integrity Verification → Dual-Voice Response → Startup/Shutdown → Vector Index → Debug Sidecar → Deterministic Context → Binary Freshness → Tier 0 Sandbox → Lima VM Lifecycle → LSP Channel Features → MCP Channel Features.

## Tasks

- [x] 1. Set up project structure and build configuration
  - [x] 1.1 Create directory structure and Bazel BUILD files
    - Create `meld-daemon/include/meld/daemon/` and `meld-daemon/src/` directories
    - Create `meld-daemon/tests/` directory
    - Write `meld-daemon/BUILD.bazel` with deps on `//meld-core:api`, `//shared:common`, `@nlohmann_json//:json`, `@googletest//:gtest`, `@rapidcheck//:rapidcheck`
    - Create `meld-daemon/README.md` with package overview
    - _Requirements: 1.1, 1.6_

  - [x] 1.2 Create SemanticModel foundation (`semantic_model.hpp` / `semantic_model.cpp`)
    - Define `SemanticModel` class holding: parsed AST map (file path → AST), symbol table, type info cache, effect annotation cache, resolved dependency graph
    - Implement `AST_RWLock` using `std::shared_mutex` for concurrent read / exclusive write access
    - Implement `get_ast(path)`, `update_ast(path, ast)`, `remove_ast(path)` with proper locking
    - Implement `get_diagnostics(path)`, `get_all_diagnostics()` accessors
    - _Requirements: 1.2, 1.5_

  - [x] 1.3 Write unit tests for SemanticModel
    - Test concurrent read access (multiple readers don't block)
    - Test exclusive write access (writer blocks readers)
    - Test AST update/remove operations
    - _Requirements: 1.2, 1.5_


- [x] 2. Implement Unified Single-Binary Architecture (Req 1)
  - [x] 2.1 Create MeldDaemon core class (`daemon.hpp` / `daemon.cpp`)
    - Define `MeldDaemon` class owning: `SemanticModel`, workspace root path, configuration
    - Implement `MeldDaemon(DaemonConfig config)` constructor accepting `--workspace=<path>` and other CLI args
    - Implement `run()` method that initializes subsystems and enters the main event loop
    - Implement `shutdown()` method for graceful teardown
    - _Requirements: 1.1, 1.6_

  - [x] 2.2 Create LSP channel adapter (`lsp_channel.hpp` / `lsp_channel.cpp`)
    - Implement `LspChannel` with JSON-RPC stdin/stdout transport for IDE communication
    - Wire LSP read queries to `SemanticModel` via shared reference
    - Implement LSP initialization, capabilities negotiation, and shutdown
    - _Requirements: 1.3, 1.4_

  - [x] 2.3 Create MCP channel adapter (`mcp_channel.hpp` / `mcp_channel.cpp`)
    - Implement `McpChannel` with HTTP/SSE transport for AI agent communication
    - Wire MCP tool queries to `SemanticModel` via shared reference
    - Implement MCP tool registration and capability discovery
    - _Requirements: 1.3, 1.4_

  - [x] 2.4 Write property test for zero-drift between channels
    - **Property 1: Channel Consistency** — For any SemanticModel state, querying the same symbol via LSP and MCP SHALL return equivalent type/effect information
    - **Validates: Requirements 1.4**

  - [x] 2.5 Write integration test for dual-channel startup
    - Test that daemon starts both LSP and MCP channels from a single process
    - Test that both channels share the same SemanticModel instance
    - _Requirements: 1.1, 1.3, 1.4_

- [x] 3. Checkpoint — Core architecture
  - Ensure all tests pass, ask the user if questions arise.

- [x] 4. Implement Incremental File Watching and Re-Analysis (Req 2)
  - [x] 4.1 Create FileWatcher abstraction (`file_watcher.hpp` / `file_watcher.cpp`)
    - Implement `FileWatcher` using FSEvents on macOS (kqueue fallback) and inotify on Linux
    - Support recursive directory watching for `.meld` files and `meld.toml`
    - Implement debounce logic with configurable window (default 50ms)
    - _Requirements: 2.1, 2.6_

  - [x] 4.2 Implement incremental re-analysis pipeline (`incremental_analyzer.hpp` / `incremental_analyzer.cpp`)
    - Implement `IncrementalAnalyzer` that accepts a changed file path and the SemanticModel
    - Re-parse only the changed file, update its AST node in the SemanticModel
    - Re-run type checking and effect inference for the changed file and its direct dependents
    - Publish updated diagnostics to both LSP and MCP channels
    - _Requirements: 2.2, 2.3, 2.4, 2.5_

  - [x] 4.3 Write property test for incremental correctness
    - **Property 2: Incremental Equivalence** — For any file change, incremental re-analysis SHALL produce the same diagnostics as a full re-analysis of the entire workspace
    - **Validates: Requirements 2.2, 2.3**

  - [x] 4.4 Write unit tests for FileWatcher
    - Test debounce coalescing of rapid events
    - Test file create/modify/delete detection
    - Test `.meld` and `meld.toml` filtering
    - _Requirements: 2.1, 2.5, 2.6_

- [x] 5. Checkpoint — File watching and incremental analysis
  - Ensure all tests pass, ask the user if questions arise.

- [x] 6. Implement Bazel Worker Protocol (Req 3)
  - [x] 6.1 Create BazelWorker class (`bazel_worker.hpp` / `bazel_worker.cpp`)
    - Implement `BazelWorker` reading `WorkRequest` protobuf messages from stdin
    - Implement `WorkResponse` protobuf writing to stdout
    - Maintain `LLVM_Context_Pool` that persists across invocations
    - _Requirements: 3.1, 3.2_

  - [x] 6.2 Implement incremental AST invalidation for worker requests
    - Track which files within a Bazel target have changed since last WorkRequest
    - Only re-parse changed files, reuse cached AST/type info for unchanged files
    - Compile specified `.meld` sources to `.bc` bitcode using persistent LLVM context
    - _Requirements: 3.3, 3.4_

  - [x] 6.3 Implement error recovery for worker requests
    - On unrecoverable error (LLVM crash), return non-zero exit code in WorkResponse
    - Reset LLVM context for next request without terminating daemon
    - _Requirements: 3.5, 3.6_

  - [x] 6.4 Write unit tests for BazelWorker
    - Test WorkRequest parsing and WorkResponse serialization
    - Test LLVM context reuse across invocations
    - Test error recovery (bad input doesn't crash daemon)
    - _Requirements: 3.1, 3.5, 3.6_

- [x] 7. Checkpoint — Bazel worker protocol
  - Ensure all tests pass, ask the user if questions arise.

- [x] 8. Implement Dependency Graph Synchronization (Req 4)
  - [x] 8.1 Create DependencyGraph class (`dependency_graph.hpp` / `dependency_graph.cpp`)
    - Implement in-memory dependency graph representation
    - Support add/remove/update of dependency nodes
    - Implement diff detection between current and new dependency state
    - _Requirements: 4.2, 4.3_

  - [x] 8.2 Implement meld.toml observer and Bazel sync trigger
    - Watch `meld.toml` via FileWatcher for changes
    - On change, re-parse dependency declarations and compare against current graph
    - If dependencies changed, trigger `bazel sync` and `bazel query` to refresh
    - Handle `meld.toml` parse errors gracefully (retain previous valid graph, emit diagnostic)
    - _Requirements: 4.1, 4.3, 4.4, 4.5, 4.6_

  - [x] 8.3 Write unit tests for DependencyGraph
    - Test graph diff detection (added, removed, changed dependencies)
    - Test graceful handling of malformed `meld.toml`
    - Test diagnostic publishing after graph refresh
    - _Requirements: 4.2, 4.5, 4.6_

- [x] 9. Checkpoint — Dependency graph sync
  - Ensure all tests pass, ask the user if questions arise.


- [x] 10. Implement SRT Lifecycle Management (Req 5)
  - [x] 10.1 Create SandboxLifecycleManager (`sandbox_lifecycle.hpp` / `sandbox_lifecycle.cpp`)
    - Implement ephemeral `srt-settings.json` generation from target binary's `.meld` manifest
    - Construct `SandboxConfig` from manifest effect map
    - Delegate to `SandboxProvider` interface (from `meld-manifest` / `libmeld_core`)
    - _Requirements: 5.1, 5.2_

  - [x] 10.2 Implement policy cleanup and stale detection
    - Delete ephemeral `srt-settings.json` after sandboxed process exits
    - On daemon startup, scan temp directory for stale policy files from previous sessions and delete them
    - Track active sandboxed processes with configurable timeout (default 5min for test, unlimited for run)
    - Forcefully terminate processes exceeding timeout
    - _Requirements: 5.3, 5.4, 5.5_

  - [x] 10.3 Write unit tests for SandboxLifecycleManager
    - Test policy file generation and cleanup
    - Test stale file detection on startup
    - Test timeout enforcement
    - _Requirements: 5.1, 5.3, 5.4, 5.5_

- [x] 11. Implement Pre-Execution Integrity Verification (Req 6)
  - [x] 11.1 Create IntegrityVerifier (`integrity_verifier.hpp` / `integrity_verifier.cpp`)
    - Read target binary's `.note.meld` Tombstone section and co-located `.meld` manifest
    - Verify Tombstone `signature_blob` against manifest content using configured trust root
    - Verify manifest `code_hash` matches actual hash of binary's executable segments
    - Verify Tombstone `manifest_hash` matches actual hash of manifest file
    - _Requirements: 6.1, 6.2, 6.3, 6.4_

  - [x] 11.2 Implement verification failure handling
    - On any check failure, refuse to spawn process
    - Emit structured audit log entry with: binary path, specific failed check, expected vs actual hash
    - Read verification mode (offline vs online) from `meld.toml` or daemon config
    - _Requirements: 6.5, 6.6_

  - [x] 11.3 Write unit tests for IntegrityVerifier
    - Test successful verification with valid manifest/tombstone pair
    - Test failure on tampered binary (code_hash mismatch)
    - Test failure on tampered manifest (manifest_hash mismatch)
    - Test failure on invalid signature
    - Test audit log output format
    - _Requirements: 6.1, 6.2, 6.3, 6.4, 6.5_

- [x] 12. Checkpoint — SRT lifecycle and integrity verification
  - Ensure all tests pass, ask the user if questions arise.

- [x] 13. Implement Dual-Voice Response Format (Req 7)
  - [x] 13.1 Create DualVoiceFormatter (`dual_voice.hpp` / `dual_voice.cpp`)
    - Define `DualVoiceResponse` struct with `message` (human-readable) and `agent_context` (machine-readable: `rule_id`, `ast_selector`, `context_hash`)
    - Implement `format_diagnostic(Diagnostic) -> DualVoiceResponse` conversion
    - When fix suggestions are available, include `fix` object with AST_Patch in `agent_context`
    - _Requirements: 7.1, 7.2, 7.3_

  - [x] 13.2 Implement channel-specific serialization
    - LSP channel: serialize `message` as primary diagnostic text, `agent_context` in `Diagnostic.data` field (LSP 3.17)
    - MCP channel: serialize both `message` and `agent_context` as top-level JSON fields
    - _Requirements: 7.4, 7.5_

  - [x] 13.3 Write property test for dual-voice consistency
    - **Property 3: Dual-Voice Completeness** — For any diagnostic, both `message` and `agent_context` fields SHALL be non-empty, and `agent_context` SHALL contain valid `rule_id` and `ast_selector`
    - **Validates: Requirements 7.1, 7.2, 7.6**

  - [x] 13.4 Write unit tests for DualVoiceFormatter
    - Test diagnostic conversion produces both fields
    - Test LSP serialization format
    - Test MCP serialization format
    - Test fix suggestion inclusion in agent_context
    - _Requirements: 7.1, 7.2, 7.3, 7.4, 7.5, 7.6_

- [x] 14. Checkpoint — Dual-voice response format
  - Ensure all tests pass, ask the user if questions arise.

- [x] 15. Implement Graceful Startup and Shutdown (Req 8)
  - [x] 15.1 Implement lazy startup sequence (`daemon.cpp` additions)
    - Start accepting LSP/MCP connections immediately on startup
    - Index workspace in background, publishing diagnostics incrementally
    - Report initialization progress via LSP `window/workDoneProgress` notifications
    - _Requirements: 8.1, 8.2_

  - [x] 15.2 Implement graceful shutdown sequence
    - On SIGTERM or LSP `shutdown` request: stop accepting new requests
    - Wait for in-flight WorkRequests (10-second timeout)
    - Terminate active sandboxed processes
    - Clean up stale SRT policy files
    - Exit with code 0
    - _Requirements: 8.3_

  - [x] 15.3 Implement crash recovery
    - On next startup after crash (SIGKILL), detect and clean up orphaned resources
    - Detect stale policy files and zombie sandboxed processes
    - _Requirements: 8.4_

  - [x] 15.4 Implement idle timeout
    - Support `--timeout=<seconds>` flag for auto-exit after idle period (no active connections)
    - Useful for CI environments
    - _Requirements: 8.5_

  - [x] 15.5 Write unit tests for startup/shutdown
    - Test lazy initialization (connections accepted before indexing completes)
    - Test graceful shutdown sequence ordering
    - Test crash recovery cleanup
    - Test idle timeout behavior
    - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5_

- [x] 16. Implement `meldd` CLI entry point (`main.cpp`)
  - Create `meld-daemon/src/main.cpp` with CLI argument parsing
  - Parse `--workspace=<path>`, `--timeout=<seconds>`, `--sandbox-verbose` flags
  - Initialize `MeldDaemon` and call `run()`
  - Wire signal handlers for SIGTERM/SIGINT
  - _Requirements: 1.1, 1.6, 8.5_

- [x] 17. Integration tests
  - [x] 17.1 Write end-to-end daemon lifecycle test
    - Start daemon → connect LSP client → send diagnostics request → verify response → shutdown
    - _Requirements: 1.1, 1.3, 8.1, 8.3_

  - [x] 17.2 Write file-change propagation test
    - Start daemon → modify `.meld` file → verify incremental diagnostics published to LSP
    - _Requirements: 2.2, 2.4_

  - [x] 17.3 Write dual-channel consistency test
    - Start daemon → query same symbol via LSP and MCP → verify equivalent results
    - _Requirements: 1.4_

- [x] 18. Final Checkpoint — Full daemon integration
  - Ensure all tests pass, ask the user if questions arise.

- [x] 19. Implement Vector-Augmented Symbol Index (Req 9)
  - [x] 19.1 Define `EmbeddingProvider` abstract interface (`embedding_provider.hpp`)
    - Define `EmbeddingProvider` abstract class with pure virtual `embed(text) -> std::vector<float>` and `dimensions() -> size_t`
    - Ensure interface is stateless per call — each `embed()` invocation is independent
    - _Requirements: 9.2, 9.10_

  - [x] 19.2 Implement `OnnxEmbeddingProvider` reference implementation (`onnx_embedding_provider.hpp` / `onnx_embedding_provider.cpp`)
    - Implement `OnnxEmbeddingProvider` using ONNX Runtime C++ API to load and run a local embedding model
    - Read configuration from `meld.toml` `[daemon.embeddings]` section: `provider`, `model_path`, `dimensions`
    - _Requirements: 9.3_

  - [x] 19.3 Implement `PassthroughEmbeddingProvider` fallback (`passthrough_embedding_provider.hpp`)
    - Implement fallback that disables vector search when no embedding model is configured
    - `find_intent` queries fall back to keyword-based symbol search
    - _Requirements: 9.4_

  - [x] 19.4 Implement `VectorIndex` component (`vector_index.hpp` / `vector_index.cpp`)
    - Implement `VectorIndex` within SemanticModel that indexes all exported symbols by semantic embedding alongside name, type signature, and effect profile
    - Implement approximate nearest-neighbor search returning top-K symbols (default K=10)
    - Implement incremental update: when a file changes, only re-embed and re-index symbols in that file
    - When a symbol has `@blueprint`, embed the concatenation of signature + summary + spec action descriptions
    - _Requirements: 9.1, 9.5, 9.6, 9.7_

  - [x] 19.5 Implement index persistence and invalidation
    - Persist index to disk in `.meld/` directory so daemon restarts don't require full re-indexing
    - Invalidate persisted index when embedding model configuration changes
    - Complete initial indexing of 10,000-symbol workspace within 30 seconds (background, non-blocking)
    - _Requirements: 9.8, 9.9_

  - [x] 19.6 Write unit tests for VectorIndex
    - Test incremental re-indexing on file change
    - Test nearest-neighbor search returns relevant symbols
    - Test `@blueprint`-enriched embeddings produce better matches than bare signatures
    - Test persistence round-trip (save to disk, reload, verify identical results)
    - Test `PassthroughEmbeddingProvider` falls back to keyword search
    - _Requirements: 9.1, 9.4, 9.5, 9.6, 9.7, 9.8_

  - [x] 19.7 Write property test for incremental index equivalence
    - **Property 4: Incremental Index Equivalence** — For any file change, incrementally updating the VectorIndex SHALL produce the same search results as rebuilding the entire index from scratch
    - **Validates: Requirements 9.5**

- [x] 20. Checkpoint — Vector-Augmented Symbol Index
  - Ensure all tests pass, ask the user if questions arise.

- [x] 21. Implement Debug Sidecar Resolution (Req 10)
  - [x] 21.1 Create `DebugSidecarResolver` (`debug_sidecar_resolver.hpp` / `debug_sidecar_resolver.cpp`)
    - Implement `resolve_debug_sidecar(debug_id) -> std::optional<std::filesystem::path>` API
    - Search for `.mdebug` sidecar in order: (a) co-located with binary, (b) `.meld/debug/` cache, (c) `bazel-bin/`
    - Verify sidecar header `debug_id` matches requested `debug_id` before returning path
    - If no match found, return empty result and emit diagnostic suggesting rebuild with `meld build --release`
    - _Requirements: 10.1, 10.2, 10.3, 10.4_

  - [x] 21.2 Implement sidecar path caching and invalidation
    - Cache resolved `debug_id → path` mappings in memory for O(1) repeated lookups
    - On new build artifacts (detected via FileWatcher), invalidate cached paths for binaries whose `debug_id` has changed
    - _Requirements: 10.5, 10.6_

  - [x] 21.3 Write unit tests for DebugSidecarResolver
    - Test co-located sidecar is found and returned
    - Test fallback to `.meld/debug/` cache directory
    - Test fallback to `bazel-bin/` output tree
    - Test `debug_id` mismatch returns empty result
    - Test cache hit returns O(1) without filesystem access
    - Test cache invalidation on new build
    - _Requirements: 10.1, 10.2, 10.3, 10.4, 10.5, 10.6_

- [x] 22. Implement Deterministic Runtime Context (Req 11)
  - [x] 22.1 Create `DeterministicContext` component (`deterministic_context.hpp` / `deterministic_context.cpp`)
    - Define `DeterministicConfig` struct: `seed` (uint64, default 0), `epoch` (ISO-8601 string, default `2024-01-01T00:00:00Z`), `time_increment_ms` (uint32, default 1)
    - Implement `DeterministicContext` that installs deterministic effect handlers when activated
    - Activation is explicit — not installed by default
    - _Requirements: 11.5, 11.6_

  - [x] 22.2 Implement deterministic effect handlers
    - `DeterministicTimeHandler`: intercept `EffectTime` — `Time.now()` returns configurable epoch advancing by fixed increment per call; `Time.sleep()` advances virtual clock without delay
    - `DeterministicRandomHandler`: intercept `EffectRandom` — delegate to seeded PRNG with configurable seed; `Random.seed()` calls ignored (seed locked)
    - `DeterministicScheduler`: replace concurrent task executor with sequential executor processing tasks in spawn order
    - All handlers are standard algebraic effect handlers — no new kernel primitives
    - _Requirements: 11.1, 11.2, 11.3, 11.4, 11.8_

  - [x] 22.3 Wire DeterministicContext into daemon execution paths
    - Activate via MCP tool `deterministic` parameter (meld-mcp-server Req 14) or CLI `--agent-test` flag (meld-cli Req 19)
    - When active, include `DeterministicConfig` in every MCP tool response for reproduction
    - _Requirements: 11.6, 11.7_

  - [x] 22.4 Write unit tests for DeterministicContext
    - Test `DeterministicTimeHandler` returns epoch and advances by configured increment
    - Test `DeterministicRandomHandler` produces identical sequence for same seed
    - Test `DeterministicScheduler` processes tasks in spawn order
    - Test context is not active by default
    - Test `DeterministicConfig` is echoed in responses when active
    - _Requirements: 11.1, 11.2, 11.3, 11.4, 11.5, 11.6, 11.7_

- [x] 23. Checkpoint — Debug Sidecar Resolution and Deterministic Context
  - Ensure sidecar resolution finds `.mdebug` files in all search locations
  - Ensure deterministic context produces reproducible results
  - Ask the user if questions arise.

- [x] 24. Implement Binary Freshness Check API (Req 12)
  - [x] 24.1 Create `BinaryFreshnessChecker` component (`binary_freshness.hpp` / `binary_freshness.cpp`)
    - Define `FreshnessResult` struct: `is_current` (bool), `stale_sources` (list of changed file paths), `rebuild_triggered` (bool), `build_diagnostics` (optional, on rebuild failure)
    - Implement `check_binary_freshness(binary_path) -> FreshnessResult` that compares binary build timestamp against source modification times using the SemanticModel's dependency graph
    - _Requirements: 12.1, 12.2_

  - [x] 24.2 Implement automatic rebuild triggering
    - When binary is stale, trigger `bazel build` for the corresponding target via the Bazel Worker Protocol (Req 3)
    - Block the `check_binary_freshness` call until rebuild completes or fails
    - On rebuild failure, return `is_current = false`, `rebuild_triggered = true`, and include build diagnostics
    - _Requirements: 12.3, 12.4, 12.5_

  - [x] 24.3 Implement freshness result caching and invalidation
    - Cache freshness results so repeated checks within the debounce window return immediately
    - When FileWatcher detects a file change, invalidate cached results for any binary whose transitive dependency set includes the changed file
    - _Requirements: 12.6, 12.7_

  - [x] 24.4 Expose API via LSP and MCP channels
    - Register `check_binary_freshness` as a custom LSP request and MCP tool
    - Return `FreshnessResult` in the dual-voice format (human message + agent context)
    - _Requirements: 12.1_

  - [x] 24.5 Write unit tests for BinaryFreshnessChecker
    - Test stale binary detection when source is newer than build timestamp
    - Test current binary returns `is_current = true` without triggering rebuild
    - Test rebuild is triggered and blocks until completion
    - Test rebuild failure returns diagnostics
    - Test cache hit returns immediately without Bazel query
    - Test cache invalidation on file change
    - _Requirements: 12.1, 12.2, 12.3, 12.4, 12.5, 12.6, 12.7_

- [x] 25. Checkpoint — Binary Freshness Check API
  - Ensure freshness check detects stale binaries
  - Ensure automatic rebuild works via Bazel Worker Protocol
  - Ensure caching and invalidation work correctly
  - Ask the user if questions arise.

- [x] 26. Implement Tier 0 Agent Script Sandbox (Req 13)
  - [x] 26.1 Create `Tier0Sandbox` component (`tier0_sandbox.hpp` / `tier0_sandbox.cpp`)
    - Define `Tier0Config` struct: `max_memory_bytes` (size_t, default 16MB), `max_execution_time` (milliseconds, default 5000ms)
    - Define `ScriptResult` struct: `success` (bool), `output` (string), `diagnostics` (optional vector)
    - Implement `Tier0Sandbox` that accepts a Meld source string, JIT-compiles it in-memory via LLVM ORC JIT, and executes it in an isolated context
    - _Requirements: 13.1, 13.2, 13.3_

  - [x] 26.2 Implement resource limits and sandbox isolation
    - Enforce configurable memory limit (default 16MB), terminating the script if exceeded
    - Enforce configurable execution time limit (default 5000ms), terminating the script if exceeded
    - Ensure NO filesystem access, NO network access, NO raw I/O — only injected RPC bindings
    - Destroy sandbox instance immediately after script returns or is terminated
    - _Requirements: 13.4, 13.5, 13.6, 13.9_

  - [x] 26.3 Implement read-only RPC bindings for SemanticModel access
    - Inject typed RPC bindings: `compiler.get_ast_node(id)`, `compiler.query_symbol(name)`, `compiler.list_symbols(module)`, `compiler.trace_effects(expr)`, `compiler.get_diagnostics(file)`
    - Ensure all bindings are read-only — scripts cannot modify the SemanticModel, AST, or source files
    - _Requirements: 13.7, 13.8_

  - [x] 26.4 Implement error handling and concurrent execution
    - On compilation failure, return structured diagnostics in SDF format
    - On runtime error, capture error with stack trace and return as structured diagnostic
    - Support concurrent script executions, each in an independent sandbox instance with its own limits
    - _Requirements: 13.10, 13.11, 13.12_

  - [x] 26.5 Write unit tests for Tier0Sandbox
    - Test successful script compilation and execution returns output
    - Test memory limit enforcement terminates script and returns diagnostic
    - Test execution time limit enforcement terminates script and returns diagnostic
    - Test RPC bindings provide read-only SemanticModel access
    - Test scripts cannot access filesystem, network, or raw I/O
    - Test concurrent sandbox executions are independent
    - Test compilation error returns structured diagnostics
    - Test runtime error returns stack trace diagnostic
    - _Requirements: 13.1, 13.3, 13.4, 13.5, 13.6, 13.7, 13.8, 13.10, 13.11, 13.12_

  - [x] 26.6 Write property tests for Tier0Sandbox
    - **Property 15: Tier 0 Sandbox Isolation** — For any script, the sandbox SHALL have NO ability to modify the SemanticModel, write to filesystem, open network, or perform raw I/O
    - **Property 16: Tier 0 Resource Limits** — For any script exceeding memory or time limits, the sandbox SHALL terminate and return a diagnostic; the sandbox instance SHALL be fully destroyed
    - **Property 17: Tier 0 Concurrent Independence** — For any two concurrent executions, a crash or timeout in one SHALL NOT affect the other
    - **Validates: Requirements 13.4, 13.5, 13.6, 13.7, 13.8, 13.9, 13.10**

- [x] 27. Checkpoint — Tier 0 Agent Script Sandbox
  - Ensure scripts compile and execute within 5ms for typical agent scripts
  - Ensure resource limits are enforced
  - Ensure sandbox isolation prevents all unauthorized host access
  - Ask the user if questions arise.

- [x] 28. Implement Lima VM Lifecycle Management (Req 14)
  - [x] 28.1 Create `LimaVmManager` component (`lima_vm_manager.hpp` / `lima_vm_manager.cpp`)
    - Define `VmStatus` struct: `running` (bool), `allocated_memory_mb` (size_t), `active_microvm_count` (uint32), `active_container_count` (uint32)
    - Implement `LimaVmManager` with `ensure_running()`, `shutdown()`, `check_status()`, and `check_idle_shutdown()` methods
    - Implement `is_required()` static method returning true on macOS, false on Linux
    - _Requirements: 14.1, 14.8, 14.9_

  - [x] 28.2 Implement POSIX file lock coordination
    - Use POSIX file lock at `/tmp/meld-vmm.lock` to coordinate VM lifecycle across multiple concurrent `meldd` instances
    - When lock acquired and VM stopped, start Lima VM via `limactl start meld-vm` and wait for KVM probe
    - When lock already held, wait for release then verify VM is running via `limactl ls`
    - _Requirements: 14.2, 14.3, 14.4_

  - [x] 28.3 Implement VM shutdown coordination and idle timeout
    - On daemon shutdown, release file lock; last daemon to shut down issues `limactl stop meld-vm`
    - Periodically check (every 60 seconds) whether other `meldd` instances are still running
    - If last daemon alive and no sandbox executions in flight, stop VM after configurable idle timeout (default 5 minutes)
    - _Requirements: 14.5, 14.6, 14.7_

  - [x] 28.4 Implement error handling and audit logging
    - On Lima VM start failure (QEMU error, insufficient resources, KVM unavailable), emit structured diagnostic with remediation suggestion
    - Log all Lima lifecycle events (start, stop, lock acquire, lock release) to daemon audit log
    - _Requirements: 14.10, 14.11_

  - [x] 28.5 Expose `check_vm_status()` API via LSP and MCP channels
    - Register `check_vm_status` as a custom LSP request and MCP tool
    - Return `VmStatus` in dual-voice format (human message + agent context)
    - _Requirements: 14.9_

  - [x] 28.6 Write unit tests for LimaVmManager
    - Test `is_required()` returns true on macOS, false on Linux
    - Test file lock acquisition and release
    - Test VM start is triggered when lock acquired and VM stopped
    - Test VM is not started when lock already held (waits for existing instance)
    - Test last-daemon shutdown stops the VM
    - Test idle timeout triggers VM stop when no other daemons active
    - Test error handling on VM start failure emits diagnostic
    - Test audit log entries for lifecycle events
    - _Requirements: 14.1, 14.2, 14.3, 14.4, 14.5, 14.6, 14.7, 14.10, 14.11_

  - [x] 28.7 Write property tests for LimaVmManager
    - **Property 18: Lima VM Single Instance** — For any number of concurrent daemons, exactly one `meld-vm` instance SHALL be running globally
    - **Property 19: Lima VM Last-Daemon Shutdown** — The last daemon to shut down SHALL stop the VM; no shutdown SHALL stop the VM while other daemons are active
    - **Property 20: Lima VM Platform Skip** — On Linux, all Lima lifecycle management SHALL be skipped
    - **Validates: Requirements 14.2, 14.3, 14.5, 14.6, 14.7, 14.8**

- [x] 29. Checkpoint — Lima VM Lifecycle Management
  - Ensure VM coordination works across multiple daemon instances
  - Ensure file lock prevents race conditions during start/stop
  - Ensure Linux skips all Lima management
  - Ask the user if questions arise.

- [x] 30. Final integration tests for Req 9–14 subsystems
  - [x] 30.1 Write integration test for Tier 0 sandbox via MCP `execute_script`
    - Start daemon → invoke `execute_script` MCP tool → verify script output → verify sandbox destroyed
    - _Requirements: 13.1, 13.7, 13.9_

  - [x] 30.2 Write integration test for Lima VM lifecycle with daemon startup/shutdown
    - Start daemon on macOS → verify VM started → shutdown daemon → verify VM stopped (last daemon case)
    - _Requirements: 14.1, 14.6_

  - [x] 30.3 Write integration test for binary freshness + rebuild + execution pipeline
    - Start daemon → modify source → call `check_binary_freshness` → verify rebuild triggered → verify `meld run` succeeds with fresh binary
    - _Requirements: 12.1, 12.3, 12.4_

- [x] 31. Final Checkpoint — All subsystems integrated
  - Ensure all tests pass across Req 1–14
  - Ask the user if questions arise.

---

## LSP Channel Feature Tasks (Req 15–21)

> These tasks implement the full LSP language features within the daemon's `LspChannel`, reading from the shared SemanticModel. They consolidate what was previously in the `meld-lsp-server` spec.

- [x] 32. Implement LSP semantic token provider and parser integration (Req 15)
  - Integrate with Meld's Boost Spirit X3 parser via the SemanticModel
  - Implement AST-to-LSP semantic token conversion for syntax highlighting
  - Implement parse error recovery to continue processing after syntax errors
  - Add incremental parsing and AST caching within the SemanticModel
  - _Requirements: 15.1, 15.2, 15.3, 15.4, 15.5_

  - [x] 32.1 Write property test for semantic token provision
    - **Property 21: Semantic Token Provision**
    - **Validates: Requirements 15.1**

  - [x] 32.2 Write property test for language construct classification
    - **Property 22: Language Construct Classification**
    - **Validates: Requirements 15.2**

  - [x] 32.3 Write property test for syntax error diagnostics
    - **Property 23: Syntax Error Diagnostics**
    - **Validates: Requirements 15.3**

  - [x] 32.4 Write property test for valid syntax parsing
    - **Property 24: Valid Syntax Parsing**
    - **Validates: Requirements 15.4**

  - [x] 32.5 Write property test for parse error recovery
    - **Property 25: Parse Error Recovery**
    - **Validates: Requirements 15.5**

- [x] 33. Implement LSP code completion and signature help (Req 16)
  - Create context-aware completion provider based on SemanticModel scope analysis
  - Implement symbol completion for variables, functions, types, and imports
  - Add function signature help with parameter information and documentation
  - Implement type annotation completions and context-sensitive filtering
  - _Requirements: 16.1, 16.2, 16.3, 16.4, 16.5_

  - [x] 33.1 Write property test for context-aware completions
    - **Property 26: Context-Aware Completions**
    - **Validates: Requirements 16.1**

  - [x] 33.2 Write property test for symbol completion accuracy
    - **Property 27: Symbol Completion Accuracy**
    - **Validates: Requirements 16.2**

  - [x] 33.3 Write property test for function signature help
    - **Property 28: Function Signature Help**
    - **Validates: Requirements 16.3**

  - [x] 33.4 Write property test for type completion accuracy
    - **Property 29: Type Completion Accuracy**
    - **Validates: Requirements 16.4**

  - [x] 33.5 Write property test for context-sensitive filtering
    - **Property 30: Context-Sensitive Filtering**
    - **Validates: Requirements 16.5**

- [x] 34. Implement LSP diagnostics and type checking (Req 17)
  - Integrate with SemanticModel's type checker for real-time semantic analysis
  - Implement type error reporting with clear descriptions and suggested fixes via DualVoiceFormatter
  - Add undefined symbol detection with similarity-based suggestions
  - Implement refinement type constraint validation and multiple dispatch ambiguity detection
  - _Requirements: 17.1, 17.2, 17.3, 17.4, 17.5_

  - [x] 34.1 Write property test for grammar conformance validation
    - **Property 31: Grammar Conformance Validation**
    - **Validates: Requirements 17.1**

  - [x] 34.2 Write property test for type error reporting
    - **Property 32: Type Error Reporting**
    - **Validates: Requirements 17.2**

  - [x] 34.3 Write property test for undefined symbol detection
    - **Property 33: Undefined Symbol Detection**
    - **Validates: Requirements 17.3**

  - [x] 34.4 Write property test for refinement constraint validation
    - **Property 34: Refinement Constraint Validation**
    - **Validates: Requirements 17.4**

  - [x] 34.5 Write property test for dispatch ambiguity detection
    - **Property 35: Dispatch Ambiguity Detection**
    - **Validates: Requirements 17.5**

- [x] 35. Implement LSP code navigation (Req 18)
  - Create "go to definition" using SemanticModel symbol resolution
  - Implement "find references" with workspace-wide symbol usage tracking
  - Add document and workspace symbol outline providers
  - Implement hover provider with type information, documentation, and signature details
  - _Requirements: 18.1, 18.2, 18.3, 18.4, 18.5_

  - [x] 35.1 Write property test for definition navigation accuracy
    - **Property 36: Definition Navigation Accuracy**
    - **Validates: Requirements 18.1**

  - [x] 35.2 Write property test for reference finding completeness
    - **Property 37: Reference Finding Completeness**
    - **Validates: Requirements 18.2**

  - [x] 35.3 Write property test for document symbol outline accuracy
    - **Property 38: Document Symbol Outline Accuracy**
    - **Validates: Requirements 18.3**

  - [x] 35.4 Write property test for workspace symbol search completeness
    - **Property 39: Workspace Symbol Search Completeness**
    - **Validates: Requirements 18.4**

  - [x] 35.5 Write property test for hover information accuracy
    - **Property 40: Hover Information Accuracy**
    - **Validates: Requirements 18.5**

- [x] 36. Implement LSP code formatting and refactoring (Req 19)
  - Create document formatter following Meld style conventions
  - Implement range formatting with precise boundary handling
  - Add symbol renaming with conflict detection and workspace-wide updates
  - _Requirements: 19.1, 19.2, 19.3, 19.4, 19.5_

  - [x] 36.1 Write property test for document formatting consistency
    - **Property 41: Document Formatting Consistency**
    - **Validates: Requirements 19.1**

  - [x] 36.2 Write property test for range formatting precision
    - **Property 42: Range Formatting Precision**
    - **Validates: Requirements 19.2**

  - [x] 36.3 Write property test for symbol renaming completeness
    - **Property 43: Symbol Renaming Completeness**
    - **Validates: Requirements 19.3**

  - [x] 36.4 Write property test for rename conflict detection
    - **Property 44: Rename Conflict Detection**
    - **Validates: Requirements 19.4**

  - [x] 36.5 Write property test for semantic preservation during formatting
    - **Property 45: Semantic Preservation During Formatting**
    - **Validates: Requirements 19.5**

- [x] 37. Implement LSP workspace management features (Req 20)
  - Wire workspace file discovery and indexing through SemanticModel and FileWatcher (Req 2)
  - Implement cross-file reference resolution using DependencyGraph (Req 4)
  - Add performance optimizations for large workspaces via incremental parsing and caching
  - _Requirements: 20.1, 20.2, 20.3, 20.4, 20.5_

  - [x] 37.1 Write property test for workspace file discovery completeness
    - **Property 46: Workspace File Discovery Completeness**
    - **Validates: Requirements 20.1**

  - [x] 37.2 Write property test for LSP incremental index updates
    - **Property 47: LSP Incremental Index Updates**
    - **Validates: Requirements 20.2**

  - [x] 37.3 Write property test for cross-file resolution accuracy
    - **Property 48: Cross-File Resolution Accuracy**
    - **Validates: Requirements 20.3**

  - [x] 37.4 Write property test for large workspace responsiveness
    - **Property 49: Large Workspace Responsiveness**
    - **Validates: Requirements 20.4**

  - [x] 37.5 Write property test for configuration change handling
    - **Property 50: Configuration Change Handling**
    - **Validates: Requirements 20.5**

- [x] 38. Implement LSP Meld-specific language feature support (Req 21)
  - Add homoiconic code analysis support
  - Implement macro system construct support
  - Add refinement type evaluation and multiple dispatch resolution
  - Implement tree initialization syntax validation
  - _Requirements: 21.1, 21.2, 21.3, 21.4, 21.5_

  - [x] 38.1 Write property test for homoiconic analysis correctness
    - **Property 51: Homoiconic Analysis Correctness**
    - **Validates: Requirements 21.1**

  - [x] 38.2 Write property test for macro system support
    - **Property 52: Macro System Support**
    - **Validates: Requirements 21.2**

  - [x] 38.3 Write property test for refinement type evaluation
    - **Property 53: Refinement Type Evaluation**
    - **Validates: Requirements 21.3**

  - [x] 38.4 Write property test for multiple dispatch resolution
    - **Property 54: Multiple Dispatch Resolution**
    - **Validates: Requirements 21.4**

  - [x] 38.5 Write property test for tree initialization validation
    - **Property 55: Tree Initialization Validation**
    - **Validates: Requirements 21.5**

- [x] 39. Checkpoint — LSP channel features
  - Ensure all LSP property tests pass
  - Ensure LSP features read from the shared SemanticModel (no separate state)
  - Ask the user if questions arise.

- [x] 40. LSP integration tests
  - [x] 40.1 Write end-to-end LSP feature test
    - Start daemon → connect LSP client → test completions, diagnostics, navigation, formatting → verify responses
    - _Requirements: 15.1, 16.1, 17.1, 18.1, 19.1_

  - [x] 40.2 Write LSP + MCP consistency test for diagnostics
    - Modify a file with type errors → verify LSP diagnostics match MCP diagnostics from the same SemanticModel
    - _Requirements: 1.4, 17.2_

  - [x] 40.3 Write LSP workspace-scale test
    - Open workspace with 100+ files → verify incremental diagnostics, cross-file navigation, and workspace symbols
    - _Requirements: 20.1, 20.3, 20.4_

- [x] 41. Final Checkpoint — Full daemon with LSP features
  - Ensure all tests pass across Req 1–21
  - Ask the user if questions arise.

---

## MCP Channel Feature Tasks (Req 22–36)

> These tasks implement the full MCP tool features within the daemon's `McpChannel`, reading from the shared SemanticModel. They consolidate what was previously in the `meld-mcp-server` spec.

- [x] 42. Implement MCP codebase exploration and resource providers (Req 22)
  - Implement workspace discovery and project structure exposure via SemanticModel
  - Expose AST representations, symbol tables, and type information as MCP resources
  - Implement dependency graph and documentation extraction from SemanticModel
  - _Requirements: 22.1, 22.2, 22.3, 22.4, 22.5_

  - [x] 42.1 Write property test for MCP project discovery completeness
    - **Property 56: MCP Project Discovery Completeness**
    - **Validates: Requirements 22.1**

  - [x] 42.2 Write property test for MCP codebase structure representation
    - **Property 57: MCP Codebase Structure Representation**
    - **Validates: Requirements 22.2**

  - [x] 42.3 Write property test for MCP code artifact exposure completeness
    - **Property 58: MCP Code Artifact Exposure Completeness**
    - **Validates: Requirements 22.3**

  - [x] 42.4 Write property test for MCP dependency analysis accuracy
    - **Property 59: MCP Dependency Analysis Accuracy**
    - **Validates: Requirements 22.4**

  - [x] 42.5 Write property test for MCP documentation extraction completeness
    - **Property 60: MCP Documentation Extraction Completeness**
    - **Validates: Requirements 22.5**

- [x] 43. Implement MCP code semantic analysis tools (Req 23)
  - Implement code analysis tools using SemanticModel's parser and type checker
  - Add type analysis, control flow analysis, and macro processing support
  - Implement code validation for syntax errors, type mismatches, and semantic violations
  - _Requirements: 23.1, 23.2, 23.3, 23.4, 23.5_

  - [x] 43.1 Write property test for MCP grammar parsing conformance
    - **Property 61: MCP Grammar Parsing Conformance**
    - **Validates: Requirements 23.1**

  - [x] 43.2 Write property test for MCP type analysis accuracy
    - **Property 62: MCP Type Analysis Accuracy**
    - **Validates: Requirements 23.2**

  - [x] 43.3 Write property test for MCP control flow analysis completeness
    - **Property 63: MCP Control Flow Analysis Completeness**
    - **Validates: Requirements 23.3**

  - [x] 43.4 Write property test for MCP macro processing accuracy
    - **Property 64: MCP Macro Processing Accuracy**
    - **Validates: Requirements 23.4**

  - [x] 43.5 Write property test for MCP code validation completeness
    - **Property 65: MCP Code Validation Completeness**
    - **Validates: Requirements 23.5**

- [x] 44. Implement MCP code search and query tools (Req 24)
  - Implement symbol search, type signature search, and code pattern identification
  - Add language feature filtering and semantic search via VectorIndex (Req 9)
  - _Requirements: 24.1, 24.2, 24.3, 24.4, 24.5_

  - [x] 44.1 Write property test for MCP symbol search completeness
    - **Property 66: MCP Symbol Search Completeness**
    - **Validates: Requirements 24.1**

  - [x] 44.2 Write property test for MCP type signature search accuracy
    - **Property 67: MCP Type Signature Search Accuracy**
    - **Validates: Requirements 24.2**

  - [x] 44.3 Write property test for MCP code pattern identification accuracy
    - **Property 68: MCP Code Pattern Identification Accuracy**
    - **Validates: Requirements 24.3**

  - [x] 44.4 Write property test for MCP language feature filtering accuracy
    - **Property 69: MCP Language Feature Filtering Accuracy**
    - **Validates: Requirements 24.4**

- [x] 45. Implement MCP code generation and validation tools (Req 25)
  - Implement code snippet generation with grammar validation
  - Add type definition generation, function generation with dispatch compatibility
  - Implement module generation with import/export validation and formatting
  - _Requirements: 25.1, 25.2, 25.3, 25.4, 25.5_

  - [x] 45.1 Write property test for MCP code generation syntax validation
    - **Property 70: MCP Code Generation Syntax Validation**
    - **Validates: Requirements 25.1**

  - [x] 45.2 Write property test for MCP type definition generation correctness
    - **Property 71: MCP Type Definition Generation Correctness**
    - **Validates: Requirements 25.2**

  - [x] 45.3 Write property test for MCP function generation dispatch compatibility
    - **Property 72: MCP Function Generation Dispatch Compatibility**
    - **Validates: Requirements 25.3**

  - [x] 45.4 Write property test for MCP module generation consistency
    - **Property 73: MCP Module Generation Consistency**
    - **Validates: Requirements 25.4**

  - [x] 45.5 Write property test for MCP generated code formatting compliance
    - **Property 74: MCP Generated Code Formatting Compliance**
    - **Validates: Requirements 25.5**

- [x] 46. Implement MCP code transformation and refactoring tools (Req 26)
  - Implement symbol renaming, function extraction, and module reorganization
  - Add design pattern application and code optimization with behavior preservation
  - _Requirements: 26.1, 26.2, 26.3, 26.4, 26.5_

  - [x] 46.1 Write property test for MCP symbol renaming semantic preservation
    - **Property 75: MCP Symbol Renaming Semantic Preservation**
    - **Validates: Requirements 26.1**

  - [x] 46.2 Write property test for MCP function extraction correctness
    - **Property 76: MCP Function Extraction Correctness**
    - **Validates: Requirements 26.2**

  - [x] 46.3 Write property test for MCP module reorganization consistency
    - **Property 77: MCP Module Reorganization Consistency**
    - **Validates: Requirements 26.3**

  - [x] 46.4 Write property test for MCP design pattern application correctness
    - **Property 78: MCP Design Pattern Application Correctness**
    - **Validates: Requirements 26.4**

  - [x] 46.5 Write property test for MCP code optimization functional equivalence
    - **Property 79: MCP Code Optimization Functional Equivalence**
    - **Validates: Requirements 26.5**

- [x] 47. Implement MCP project metadata and tool integration (Req 27–28)
  - Implement project configuration access, structure exposure, and build artifact analysis
  - Add Git integration, test suite identification, and Bazel build system integration
  - Wire compiler output access through DualVoiceFormatter and debugger support through DebugSidecarResolver
  - _Requirements: 27.1, 27.2, 27.3, 27.4, 27.5, 28.1, 28.2, 28.3, 28.4, 28.5_

  - [x] 47.1 Write property test for MCP project configuration access completeness
    - **Property 80: MCP Project Configuration Access Completeness**
    - **Validates: Requirements 27.1**

  - [x] 47.2 Write property test for MCP project structure exposure accuracy
    - **Property 81: MCP Project Structure Exposure Accuracy**
    - **Validates: Requirements 27.2**

  - [x] 47.3 Write property test for MCP build artifact analysis completeness
    - **Property 82: MCP Build Artifact Analysis Completeness**
    - **Validates: Requirements 27.3**

  - [x] 47.4 Write property test for MCP version control integration accuracy
    - **Property 83: MCP Version Control Integration Accuracy**
    - **Validates: Requirements 27.4**

  - [x] 47.5 Write property test for MCP test suite identification completeness
    - **Property 84: MCP Test Suite Identification Completeness**
    - **Validates: Requirements 27.5**

  - [x] 47.6 Write property test for MCP build system integration accuracy
    - **Property 85: MCP Build System Integration Accuracy**
    - **Validates: Requirements 28.1**

  - [x] 47.7 Write property test for MCP compiler output access completeness
    - **Property 86: MCP Compiler Output Access Completeness**
    - **Validates: Requirements 28.2**

  - [x] 47.8 Write property test for MCP testing framework integration accuracy
    - **Property 87: MCP Testing Framework Integration Accuracy**
    - **Validates: Requirements 28.4**

  - [x] 47.9 Write property test for MCP documentation tool integration completeness
    - **Property 88: MCP Documentation Tool Integration Completeness**
    - **Validates: Requirements 28.5**

- [x] 48. Checkpoint — MCP core tools
  - Ensure all MCP core tool property tests pass (Req 22–28)
  - Ask the user if questions arise.

- [x] 49. Implement MCP Structured Diagnostic Frames and named tools (Req 29–30)
  - Define SDF schema with `rule_id`, `ast_selector`, `context_hash`, `version_hash`, and optional `fix` field
  - Wire SDF format into all MCP tools that return diagnostics
  - Implement named tools: `analyze_safety`, `query_lifecycle`, `trace_effect`, `resolve_version_conflict`, `dry_run_patch`, `apply_patch`, `diagnose_build`, `verify_closure`
  - _Requirements: 29.1, 29.2, 29.3, 29.4, 29.5, 29.6, 29.7, 29.8, 30.1, 30.2, 30.3, 30.4, 30.5, 30.6_

  - [x] 49.1 Write unit tests for SDF format consistency
    - Test every diagnostic includes `rule_id`, `ast_selector`, `context_hash`, `version_hash`
    - Test fix field contains valid AST_Patch objects when available
    - Test SDF format is consistent across all MCP tools
    - _Requirements: 30.1, 30.2, 30.3, 30.4, 30.5, 30.6_

  - [x] 49.2 Write unit tests for named MCP tools
    - Test each named tool returns correct output format
    - Test `dry_run_patch` verifies on VFS without disk writes
    - Test `apply_patch` commits after VFS verification
    - _Requirements: 29.1, 29.2, 29.3, 29.4, 29.5, 29.6, 29.7, 29.8_

- [x] 50. Implement MCP Proof-Carrying Failures (Req 31)
  - Add provenance trace (Origin → Escape → Conflict) to `analyze_safety` violations
  - Include provenance trace in SDF `agent_context` field
  - _Requirements: 31.1, 31.2, 31.3, 31.4, 31.5, 31.6_

  - [x] 50.1 Write unit tests for proof-carrying failures
    - Test violations include Origin → Escape → Conflict trace with source locations and AST selectors
    - Test provenance trace is in SDF `agent_context` field
    - _Requirements: 31.1, 31.2, 31.3, 31.4, 31.5, 31.6_

- [x] 51. Implement MCP AI DX tools: get_module_specs, find_intent, refactor (Req 32–34)
  - Implement `get_module_specs` tool for executable spec streaming from `@blueprint` blocks
  - Implement `find_intent` tool for intent-based symbol discovery via VectorIndex
  - Implement `refactor` tool for intent-based refactoring with AST_Transform output
  - _Requirements: 32.1, 32.2, 32.3, 32.4, 32.5, 32.6, 32.7, 33.1, 33.2, 33.3, 33.4, 33.5, 33.6, 33.7, 34.1, 34.2, 34.3, 34.4, 34.5, 34.6, 34.7, 34.8_

  - [x] 51.1 Write unit tests for `get_module_specs`
    - Test retrieval of verified/unverified specs, symbol filtering, fallback to examples
    - _Requirements: 32.1, 32.2, 32.3, 32.4, 32.7_

  - [x] 51.2 Write unit tests for `find_intent`
    - Test natural language query returns relevant symbols, scope filtering, max_results, blueprint enrichment
    - _Requirements: 33.1, 33.3, 33.5, 33.6, 33.7_

  - [x] 51.3 Write unit tests for `refactor`
    - Test rename/extract/move intents produce correct AST_Transform, dry_run, disambiguation
    - _Requirements: 34.1, 34.2, 34.4, 34.6, 34.8_

- [x] 52. Implement MCP deterministic execution mode (Req 35)
  - Add `deterministic` and `deterministic_config` parameters to code-executing MCP tools
  - Wire activation to daemon's DeterministicContext (Req 11)
  - Echo deterministic config in responses for reproduction
  - _Requirements: 35.1, 35.2, 35.3, 35.4, 35.5, 35.6, 35.7_

  - [x] 52.1 Write unit tests for deterministic MCP parameter
    - Test activation/deactivation, config override, response echo, static tool exclusion
    - _Requirements: 35.1, 35.2, 35.4, 35.5, 35.7_

- [x] 53. Implement MCP Code Mode tools: search_api + execute_script (Req 36)
  - Implement `search_api` tool returning Meld interface definitions of SemanticModel API
  - Implement `execute_script` tool executing Meld scripts in Tier0Sandbox (Req 13)
  - Support timeout, memory limit, deterministic parameters, and SDF error format
  - _Requirements: 36.1, 36.2, 36.3, 36.4, 36.5, 36.6, 36.7, 36.8, 36.9_

  - [x] 53.1 Write unit tests for Code Mode tools
    - Test `search_api` returns Meld interface syntax with RPC binding signatures
    - Test `execute_script` compiles and executes scripts, returns output
    - Test timeout/memory limits, SDF error format, no filesystem/network access
    - _Requirements: 36.1, 36.2, 36.3, 36.4, 36.5, 36.6, 36.7_

- [x] 54. Checkpoint — MCP channel features
  - Ensure all MCP property tests pass
  - Ensure MCP tools read from the shared SemanticModel (no separate state)
  - Ask the user if questions arise.

- [x] 55. MCP integration tests
  - [x] 55.1 Write end-to-end MCP tool test
    - Start daemon → connect MCP client → test analyze_safety, find_intent, execute_script → verify responses
    - _Requirements: 22.1, 29.1, 33.1, 36.3_

  - [x] 55.2 Write MCP + LSP consistency test
    - Query same symbol via MCP and LSP → verify equivalent type/effect information from same SemanticModel
    - _Requirements: 1.4, 22.3_

  - [x] 55.3 Write MCP deterministic execution test
    - Execute same script twice with deterministic=true → verify identical output
    - _Requirements: 35.2, 35.5_

- [x] 56. Final Checkpoint — Full daemon with LSP and MCP features
  - Ensure all tests pass across Req 1–36
  - Ask the user if questions arise.
