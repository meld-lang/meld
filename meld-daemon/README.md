# meld-daemon

The unified Meld daemon (`meldd`) — a single long-lived C++20 process per workspace that serves as the central intelligence hub of the Meld toolchain. It holds the entire semantic state of a project in memory and exposes it simultaneously to both human editors and AI agents through two protocol channels: LSP for IDEs and MCP for AI agents.

## Architecture

```
                          ┌──────────────────────────────────────────────────┐
                          │                    meldd                         │
                          │              (one per workspace)                 │
                          │                                                  │
┌─────────────┐  socat    │  ┌────────────┐        ┌──────────────────────┐  │
│  VS Code /  │  bridge   │  │            │        │                      │  │
│  Kiro       │◄── LSP ──►│  │ LspChannel │        │    SemanticModel     │  │
│  (editor)   │  JSON-RPC │  │            │───────►│                      │  │
└─────────────┘           │  └────────────┘  R/W   │  ┌────────────────┐  │  │
  .meld/lsp.sock          │                  Lock   │  │ AST per file   │  │  │
                          │                        │  │ Symbol table   │  │  │
┌─────────────┐  socat    │  ┌────────────┐        │  │ Type info      │  │  │
│  Kiro Agent │  bridge   │  │            │───────►│  │ Effect annots  │  │  │
│  (AI tools) │◄── MCP ──►│  │ McpChannel │        │  │ Ownership map  │  │  │
│             │  JSON-RPC │  │            │        │  │ Diagnostics    │  │  │
└─────────────┘           │  └────────────┘        │  └────────────────┘  │  │
  .meld/mcp.sock          │                        │                      │  │
                          │                        └──────────────────────┘  │
                          │  ┌────────────────────────────────────────────┐  │
                          │  │              Subsystems                    │  │
                          │  │                                            │  │
                          │  │  FileWatcher ──► IncrementalAnalyzer       │  │
                          │  │  BazelWorker     DependencyGraph           │  │
                          │  │  IntegrityVerifier  SandboxLifecycle       │  │
                          │  │  BinaryFreshness    Tier0Sandbox           │  │
                          │  │  LimaVmManager      VectorIndex           │  │
                          │  │  DeterministicContext                      │  │
                          │  │  DualVoiceFormatter                       │  │
                          │  └────────────────────────────────────────────┘  │
                          └──────────────────────────────────────────────────┘
```

A single `meldd` process owns the workspace lock (`.meld/daemon.lock`), preventing duplicate instances. Both the LSP and MCP channels read from and write to the same `SemanticModel`, protected by a shared mutex that allows concurrent reads with exclusive writes.

## How It Works

### Startup Sequence

1. **Lock acquisition** — `meldd` takes an exclusive `flock` on `.meld/daemon.lock`. If another instance already holds it, startup fails immediately. The lock file contains the PID and socket paths so clients can discover the running daemon.

2. **Stale resource cleanup** — Orphaned temp files from a previous crash (SRT policy files, stale sockets) are removed.

3. **Subsystem initialization** — Background subsystems are created. The LSP and MCP channels begin accepting connections immediately; workspace indexing runs asynchronously so the editor gets responses before the full index is built.

4. **Dual-channel event loop** — Two background threads run accept loops on Unix domain sockets (`.meld/lsp.sock` and `.meld/mcp.sock`), spawning a handler thread per client connection. The main thread blocks until a shutdown signal is received.

### The SemanticModel

The `SemanticModel` is the daemon's core data structure — an in-memory representation of the entire workspace:

- **AST per file** — Parsed syntax trees for every `.meld` file, updated incrementally on each edit.
- **Symbol table** — All exported and local symbols with their types, locations, and visibility.
- **Type information** — Inferred types for every expression, queryable by file and symbol name.
- **Effect annotations** — Which effects each function requires, with the call chain showing how each effect was introduced.
- **Ownership map** — Ownership kind (`Own`/`Link`) and lifecycle state (`Valid`/`Moved`/`PotentiallyDangling`) for every reference.
- **Diagnostics** — Errors, warnings, and hints with stable rule IDs (e.g., `E0042-effect-leak`), AST selectors, and context hashes.

### Communication Channels

#### LSP Channel (`.meld/lsp.sock`)

The primary interface for editors. Implements the Language Server Protocol over JSON-RPC with `Content-Length` framing. Provides:

- **Completions** — Context-aware autocompletion for symbols, types, and effects.
- **Hover** — Type information and documentation on mouse-over.
- **Diagnostics** — Real-time error reporting as you type.
- **Semantic tokens** — Rich syntax highlighting beyond TextMate grammars (effect annotations, ownership markers, trait conformance).
- **Go to definition / Find references** — Cross-file navigation.
- **Formatting** — Delegates to `meld fmt` conventions.
- **Workspace symbols** — Outline view and symbol search.

#### MCP Channel (`.meld/mcp.sock`)

The AI agent interface. Implements the Model Context Protocol (version `2024-11-05`) over newline-delimited JSON-RPC on a Unix domain socket. Exposes these tools:

| Tool | Description |
|------|-------------|
| `analyze_safety` | Scans a file for safety and ownership violations |
| `trace_effect` | Traces the effect chain at a specific line |
| `query_type` | Returns the inferred type of a symbol |
| `query_ownership` | Returns ownership kind and lifecycle state |
| `get_diagnostics` | Returns all diagnostics for a file or the entire workspace |
| `structural_diff` | Compares on-disk AST against a VFS overlay |

Advanced tools (via `McpAdvancedToolProvider`):

| Tool | Description |
|------|-------------|
| `query_lifecycle` | Queries lifecycle state of a reference |
| `find_intent` | Searches for symbols by intent/description |
| `get_module_specs` | Returns module specifications and dependencies |
| `refactor` | Performs structural refactoring operations |

Both channels format responses using the **Dual-Voice** system: every diagnostic includes both a human-readable message and a machine-readable `AgentContext` with the stable rule ID, AST selector, context hash, and optional fix patch. The LSP channel puts the agent context in `Diagnostic.data`; the MCP channel returns both as top-level fields.

### Editor and Agent Integration

Editors and AI agents invoke the `tools/meld` CLI binary directly. The `meld lsp` and `meld mcp` subcommands serve JSON-RPC over stdio — no wrapper scripts or socket bridging required.

```
Editor ←→ stdin/stdout ←→ meld lsp
Agent  ←→ stdin/stdout ←→ meld mcp
```

## Configuring Kiro

Kiro uses two configuration files in `.kiro/settings/` to connect to the LSP and MCP channels.

### LSP Configuration (`.kiro/settings/lsp.json`)

This tells Kiro how to launch the Meld language server for `.meld` files:

```json
{
  "languages": {
    "meld": {
      "name": "meld",
      "command": "/absolute/path/to/meld/tools/meld",
      "args": ["lsp"],
      "file_extensions": ["meld"],
      "project_patterns": ["meld.toml"],
      "exclude_patterns": [
        "**/bazel-*/**",
        "**/.meld/vector_index/**"
      ],
      "multi_workspace": false,
      "initialization_options": {},
      "request_timeout_secs": 60
    }
  }
}
```

| Field | Purpose |
|-------|---------|
| `command` | Absolute path to the `tools/meld` binary. Must be absolute — Kiro does not resolve relative paths. |
| `args` | Must include `lsp` to start the language server. |
| `file_extensions` | Activates this language server for `.meld` files. |
| `project_patterns` | Kiro uses this to detect Meld workspaces. A `meld.toml` at the project root triggers activation. |
| `exclude_patterns` | Prevents indexing Bazel output directories and the vector index cache. |
| `request_timeout_secs` | How long Kiro waits for an LSP response before timing out. The daemon does heavy analysis, so 60s is appropriate. |

After editing, restart the LSP with:

```
/code init -f
```

### MCP Configuration (`.kiro/settings/mcp.json`)

This tells Kiro's AI agent how to connect to the MCP tools:

```json
{
  "mcpServers": {
    "meld": {
      "command": "/absolute/path/to/meld/tools/meld",
      "args": ["mcp"]
    }
  }
}
```

| Field | Purpose |
|-------|---------|
| `command` | Same `tools/meld` binary, absolute path. |
| `args` | Must include `mcp` to start the MCP server. |

Once configured, the Kiro agent can call MCP tools like `analyze_safety`, `trace_effect`, `query_type`, etc. Ensure MCP is enabled in VS Code settings:

```json
{
  "kiroAgent.configureMCP": "Enabled"
}
```

### Complete Setup Checklist

1. **Build the CLI:**
   ```bash
   bazel build //meld-cli:meld
   ```

2. **Ensure `meld.toml` exists** at the workspace root (triggers Kiro's project detection).

3. **Create `.kiro/settings/lsp.json`** with the `meld` language entry pointing to `tools/meld lsp`.

4. **Create `.kiro/settings/mcp.json`** with the `meld` server entry pointing to `tools/meld mcp`.

5. **Initialize the LSP** in Kiro:
   ```
   /code init -f
   ```

7. **Verify** — open a `.meld` file. You should see diagnostics, completions, and hover information. The Kiro agent should have access to MCP tools like `analyze_safety`.

## Troubleshooting

### LSP not starting (`/code init -f` fails)

1. **Check if the daemon is alive:**
   ```bash
   cat .meld/meldd.pid && kill -0 $(cat .meld/meldd.pid) 2>/dev/null && echo "alive" || echo "dead"
   ```

2. **Check the daemon log:**
   ```bash
   cat .meld/meldd.log
   ```

3. **Stale sockets** — If the daemon died but `.meld/lsp.sock` still exists, the launcher will try to connect to a dead socket. The launcher script handles this automatically by cleaning stale sockets on restart, but you can also clean up manually:
   ```bash
   rm -f .meld/meldd.pid .meld/lsp.sock .meld/mcp.sock
   ```

4. **socat not installed:**
   ```bash
   command -v socat || echo "Install with: brew install socat"
   ```

5. **Binary not built:**
   ```bash
   ls -la bazel-bin/meld-daemon/meldd || bazel build //meld-daemon:meldd
   ```

6. **Command path is relative** — The `command` field in `lsp.json` must be an absolute path. Kiro does not resolve relative paths from the workspace root.

### MCP tools not available

- Verify `kiroAgent.configureMCP` is `"Enabled"` in VS Code settings.
- Check that `mcp.json` uses `--mcp` in args.
- The daemon must be running — MCP and LSP share the same daemon process, so if LSP works, MCP should too.

### Daemon won't start (lock held)

Another `meldd` instance may be holding the workspace lock:

```bash
cat .meld/daemon.lock   # Shows PID of lock holder
kill $(cat .meld/meldd.pid)
rm -f .meld/daemon.lock .meld/meldd.pid .meld/lsp.sock .meld/mcp.sock
```

### Socket timeout (30s)

If the daemon takes too long to create sockets (e.g., first-time Bazel build), the launcher times out after 30 seconds. Pre-build the binary:

```bash
bazel build //meld-daemon:meldd
```

## Subsystems

| Subsystem | Description |
|-----------|-------------|
| `FileWatcher` | Monitors `.meld` files and `meld.toml` via FSEvents (macOS) / inotify (Linux). Debounces rapid edits (50ms). |
| `IncrementalAnalyzer` | Re-parses only changed files, updates AST, re-runs type checking and effect inference for the file and its dependents. |
| `DependencyGraph` | Tracks import relationships. Determines which downstream files need re-analysis on change. |
| `BazelWorker` | Persistent Bazel worker protocol. Keeps the LLVM compilation context hot across incremental builds. |
| `BinaryFreshness` | Checks if compiled binaries are up-to-date. Triggers rebuilds for the debug adapter. |
| `IntegrityVerifier` | Pre-execution binary integrity verification. Validates code hashes and signatures (Ironclad security). |
| `SandboxLifecycle` | Manages SRT (Sandboxed Runtime Trust) policy files for sandboxed execution. |
| `Tier0Sandbox` | In-process JIT sandbox for REPL-like evaluation. Memory limit: 16MB, timeout: 5s. |
| `LimaVmManager` | Manages Lima VM lifecycle on macOS for containerized builds and testing. |
| `VectorIndex` | Semantic search over the codebase. Indexes symbols with type signatures, effect profiles, and SRT security profiles. |
| `DeterministicContext` | Replaces non-deterministic system calls with deterministic alternatives for reproducible agent-mode debugging. |
| `DualVoiceFormatter` | Formats every diagnostic with both a human-readable message and a machine-readable `AgentContext`. |
| `DebugSidecarResolver` | Resolves `.mdebug` sidecar files by debug ID across co-located paths and the Bazel output tree. |

## Building

```bash
bazel build //meld-daemon:meldd
```

## Running

```bash
# Via the Meld CLI (recommended)
tools/meld lsp           # Start LSP server on stdin/stdout
tools/meld mcp           # Start MCP server on stdin/stdout
tools/meld daemon start  # Start daemon in background

# Direct (no CLI wrapper)
bazel-bin/meld-daemon/meldd --workspace=/path/to/project --verbose
```

### CLI Options

| Flag | Description |
|------|-------------|
| `--workspace=<path>` | Project root directory. Defaults to current directory. |
| `--timeout=<seconds>` | Auto-exit after idle time. 0 = disabled (default). |
| `--verbose` | Enable verbose daemon logging to stderr. |
| `--sandbox-verbose` | Enable verbose sandbox logging. |

### Client Discovery

Clients can discover a running daemon by reading `.meld/daemon.lock`:

```json
{"pid": 12345, "lsp_port": 0, "mcp_port": 0, "mcp_socket": "/path/to/.meld/mcp.sock"}
```

## Project Structure

```
meld-daemon/
├── include/meld/daemon/
│   ├── daemon.hpp                  # Top-level daemon class and config
│   ├── semantic_model.hpp          # Core data model (AST, types, effects, ownership)
│   ├── lsp_channel.hpp             # LSP JSON-RPC transport (Content-Length framing)
│   ├── mcp_channel.hpp             # MCP JSON-RPC transport (newline-delimited)
│   ├── mcp_tool_provider.hpp       # Core MCP tool implementations
│   ├── mcp_advanced_tool_provider.hpp # Advanced MCP tools with SDF support
│   ├── dual_voice.hpp              # Human + machine response formatting
│   ├── completion_provider.hpp     # LSP completions
│   ├── diagnostics_provider.hpp    # Error/warning generation
│   ├── navigation_provider.hpp     # Go-to-definition, find-references
│   ├── formatting_provider.hpp     # Code formatting
│   ├── semantic_token_provider.hpp # Rich syntax highlighting
│   ├── workspace_provider.hpp      # Workspace symbols and outline
│   ├── meld_feature_provider.hpp   # Meld-specific language features
│   ├── file_watcher.hpp            # Filesystem monitoring
│   ├── incremental_analyzer.hpp    # Incremental re-analysis pipeline
│   ├── dependency_graph.hpp        # Import graph tracking
│   ├── bazel_worker.hpp            # Persistent Bazel worker
│   ├── binary_freshness.hpp        # Stale binary detection
│   ├── integrity_verifier.hpp      # Binary integrity checks
│   ├── sandbox_lifecycle.hpp       # SRT policy management
│   ├── tier0_sandbox.hpp           # In-process JIT sandbox
│   ├── lima_vm_manager.hpp         # Lima VM lifecycle
│   ├── vector_index.hpp            # Semantic search index
│   ├── deterministic_context.hpp   # Reproducible execution context
│   ├── deterministic_handlers.hpp  # Deterministic system call replacements
│   └── debug_sidecar_resolver.hpp  # .mdebug file resolution
├── src/
│   ├── main.cpp                    # Entry point, CLI parsing, signal handling
│   ├── daemon.cpp                  # Lifecycle, lock, socket creation, event loop
│   ├── lsp_channel.cpp             # LSP protocol handling
│   ├── mcp_channel.cpp             # MCP protocol handling (stdio + fd modes)
│   ├── mcp_tool_provider.cpp       # Core tool implementations
│   ├── mcp_advanced_tool_provider.cpp # Advanced tools with SDF
│   ├── semantic_model.cpp          # SemanticModel implementation
│   └── ...                         # Other subsystem implementations
├── tests/                          # Property-based and unit tests
└── BUILD.bazel                     # Bazel build rules
```

## License

MIT — see [LICENSE-MIT](../LICENSE-MIT) in the repository root.
