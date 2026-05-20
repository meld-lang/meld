# vscode-meld

VS Code / Kiro extension for the Meld programming language. Provides syntax highlighting, IntelliSense, diagnostics, debugging, formatting, snippets, and build integration.

## Features

- **Syntax highlighting** — TextMate grammar for `.meld` files with support for Meld keywords, string interpolation, effects, and annotations.
- **IntelliSense** — Completions, hover, go-to-definition, find-references, and workspace symbols via the `meldd` language server.
- **Diagnostics** — Real-time errors and warnings as you type, with Meld-specific rule IDs and fix suggestions.
- **Semantic tokens** — Rich highlighting for effect annotations, ownership markers, and trait conformance (beyond what TextMate can express).
- **Debugging** — Two modes:
  - **Interpret** — AST interpreter with native DAP support. Set breakpoints in `.meld` files directly.
  - **Compiled** — Debug compiled binaries under LLDB with auto-loaded Meld type formatters.
- **Formatting** — Format on save or via command palette (`Meld: Format File`).
- **Snippets** — 20+ snippets for common patterns: `fnc`, `struct`, `trait`, `match`, `enum`, `effect`, `test`, etc.
- **Build tasks** — Auto-detected `build`, `run`, `test`, and `fmt` tasks for workspaces containing `meld.toml`.
- **File templates** — Create new files from templates via `Meld: New File from Template`.

## Prerequisites

- VS Code ≥ 1.80.0 or Kiro
- Node.js ≥ 18 (for building from source)
- The `meldd` daemon (for LSP features) — see [meld-daemon/README.md](../meld-daemon/README.md)

## Building from Source

```bash
cd vscode-meld

# Install dependencies
npm install

# Compile TypeScript
npm run compile

# Package as .vsix
npm run package
```

This produces `meld-0.1.0.vsix` in the `vscode-meld/` directory.

## Installing

### From .vsix (recommended)

A pre-built `.vsix` is included in the repository. Install it directly:

```bash
# VS Code
code --install-extension vscode-meld/meld-0.1.0.vsix

# Kiro (uses the same extension mechanism)
kiro --install-extension vscode-meld/meld-0.1.0.vsix
```

Or from the UI: Extensions panel → `⋯` menu → "Install from VSIX..." → select `meld-0.1.0.vsix`.

### For development

Open the `vscode-meld/` folder in VS Code and press `F5` to launch an Extension Development Host with the extension loaded. Changes to TypeScript source are picked up after recompiling (`npm run compile`) and reloading the host.

To watch for changes during development:

```bash
npm run watch
```

## Configuration

All settings are under the `meld.*` namespace in VS Code / Kiro settings.

### Language Server

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `meld.languageServer.enabled` | `boolean` | `true` | Enable the Meld language server. |
| `meld.languageServer.path` | `string` | `""` | Path to the language server binary. Leave empty to use `meld` from PATH. |
| `meld.languageServer.trace` | `string` | `"off"` | Trace level: `off`, `messages`, or `verbose`. |

When running inside Kiro, the LSP is typically configured via `.kiro/settings/lsp.json` instead (see [meld-daemon/README.md](../meld-daemon/README.md#lsp-configuration-kirosettingslspjson)). The extension's built-in LSP client and Kiro's LSP configuration are independent — use one or the other:

- **Kiro's LSP** (`.kiro/settings/lsp.json`): Kiro manages the LSP via `tools/meld lsp`. Set `meld.languageServer.enabled` to `false` in VS Code settings to avoid running two LSP clients.
- **Extension's LSP** (`meld.languageServer.path`): The extension spawns `meld lsp` directly. Works in plain VS Code without Kiro.

### Formatting

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `meld.formatting.formatOnSave` | `boolean` | `false` | Auto-format `.meld` files on save. |

### Build

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `meld.build.showOutput` | `boolean` | `true` | Show the output panel when running build tasks. |

## Commands

Available from the command palette (`Cmd+Shift+P` / `Ctrl+Shift+P`):

| Command | Description |
|---------|-------------|
| `Meld: Build Project` | Run `meld build` in the workspace. |
| `Meld: Run Program` | Run `meld run` on the active file. |
| `Meld: Run Tests` | Run `meld test` in the workspace. |
| `Meld: Format File` | Run `meld fmt` on the active file. |
| `Meld: Restart Language Server` | Stop and restart the LSP client (with exponential backoff reconnection). |
| `Meld: Show Diagnostics` | Show extension diagnostics: LSP status, extension path, workspace info. |
| `Meld: New File from Template` | Create a new `.meld` file from a template (Main, Struct, Trait, Module). |

## Debugging

The extension registers a `meld` debug adapter with two modes.

### Interpret mode (default)

Runs the program through the Meld AST interpreter with built-in DAP support. Breakpoints, stepping, and variable inspection work directly on `.meld` source.

```jsonc
// .vscode/launch.json
{
  "type": "meld",
  "request": "launch",
  "name": "Meld: Interpret",
  "mode": "interpret",
  "program": "${file}"
}
```

### Compiled mode

Debugs a compiled Meld binary under LLDB. The extension auto-loads `meld_formatters.py` so kernel types (`MeldString`, `MeldArray`, `MeldResult`, etc.) display correctly in the Variables panel.

```jsonc
// .vscode/launch.json
{
  "type": "meld",
  "request": "launch",
  "name": "Meld: Compiled",
  "mode": "compiled",
  "program": "${workspaceFolder}/build/${workspaceFolderBasename}"
}
```

Build with debug info first:

```bash
meld build --debug
```

### Attach to process

For compiled mode, you can attach to a running process by PID:

```jsonc
{
  "type": "meld",
  "request": "launch",
  "name": "Meld: Attach",
  "mode": "compiled",
  "pid": 12345
}
```

### Auto-detection

If no `launch.json` exists and you press `F5`, the extension auto-detects:
- If `build/` contains a compiled binary → offers a choice between interpret and compiled mode.
- Otherwise → defaults to interpret mode on the active file.

## Snippets

| Prefix | Description |
|--------|-------------|
| `fnc` | Function declaration |
| `main` | Main entry point |
| `struct` | Struct declaration |
| `structtrait` | Struct with trait conformance |
| `trait` | Trait declaration |
| `class` | Class declaration |
| `enum` | Enum declaration |
| `if` | If statement |
| `ife` | If-else statement |
| `match` | Match expression |
| `for` | For-in loop |
| `while` | While loop |
| `val` | Immutable variable |
| `var` | Mutable variable |
| `imp` | Import module |
| `impd` | Import with destructuring |
| `type` | Type alias |
| `effect` | Effect trait declaration |
| `println` | Print line |
| `printlni` | Print with string interpolation |
| `test` | Test function |
| `fncg` | Generic trait-constrained function |

## Tasks

The extension auto-detects Meld projects (directories containing `meld.toml` or `.meld` files) and provides these tasks in the Tasks menu:

- **Meld: Build** — `meld build` (Build group)
- **Meld: Run** — `meld run`
- **Meld: Test** — `meld test` (Test group)
- **Meld: Format** — `meld fmt`

Custom tasks can be defined in `tasks.json`:

```jsonc
{
  "type": "meld",
  "command": "test",
  "file": "tests/my_test.meld"
}
```

## Problem Matcher

The extension includes a `$meld` problem matcher that parses Meld compiler error output:

```
error[E0042-effect-leak]: unhandled effect 'IO' --> src/main.meld:12:5
```

Errors appear in the Problems panel with clickable file locations.

## Project Structure

```
vscode-meld/
├── src/
│   ├── extension.ts           # Extension entry point (activate/deactivate)
│   ├── languageClient.ts      # LSP client with reconnection and backoff
│   ├── commandProvider.ts     # Command palette commands and file templates
│   ├── taskProvider.ts        # Auto-detected build/run/test/fmt tasks
│   ├── debugAdapterFactory.ts # Debug adapter routing (interpret vs compiled)
│   ├── debugConfigProvider.ts # Debug config resolution and auto-detection
│   └── breakpointMapper.ts    # Breakpoint path normalization
├── syntaxes/
│   └── meld.tmLanguage.json   # TextMate grammar
├── snippets/
│   └── meld.json              # Code snippets
├── formatters/
│   └── meld_formatters.py     # LLDB type formatters for compiled debugging
├── language-configuration.json # Bracket matching, comments, folding, indentation
├── icon.png                    # Extension icon
├── package.json                # Extension manifest
├── tsconfig.json               # TypeScript configuration
└── meld-0.1.0.vsix            # Pre-built extension package
```

## License

MIT — see [LICENSE.txt](LICENSE.txt).
