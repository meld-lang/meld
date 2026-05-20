# CLI Reference

Complete reference for the `meld` command-line interface.

## Commands

### `meld run <file.meld>`

Run a Meld program.

```bash
meld run program.meld           # Execute program
meld run program.meld -i        # Execute then enter REPL
meld run -i                     # Interactive REPL only
```

**Flags:**
| Flag | Purpose |
|------|---------|
| `-i` | Enter interactive REPL after execution |
| `--json` | Structured JSON output (diagnostics + results) |
| `--debug` | Start DAP debug server (VS Code attaches) |
| `--verbose` | Show compilation details |
| `--emit-llvm` | Emit LLVM IR instead of executing |

### `meld check <file.meld>`

Check a program for errors without executing.

```bash
meld check program.meld
meld check program.meld --json
```

### `meld build <file.meld>`

Compile to native binary (requires LLVM backend — in progress).

```bash
meld build program.meld --emit exe --target linux-musl-x64 --out ./build/program
meld build program.meld --emit-llvm
```

### `meld test <file.meld>`

Run test functions in a file.

```bash
meld test tests.meld
meld test tests.meld --json
```

### `meld explain <code>`

Look up a diagnostic code with rich, actionable explanation.

```bash
meld explain E001              # Human-readable explanation
meld explain E4002 --json      # Structured JSON for agents
meld explain --list            # All known diagnostic codes
meld explain --list --json     # Codes as JSON array
```

### `meld fix --plan <file.meld>`

Generate a structured fix plan for all diagnostics in a file.

```bash
meld fix --plan src/main.meld              # Human-readable plan
meld fix --plan --json src/main.meld       # JSON plan for agents
```

### `meld guide [topic]`

Version-matched language guidance, served directly from the binary.

```bash
meld guide                    # List available topics
meld guide syntax             # Syntax rules
meld guide workflow           # Agent edit loop
meld guide all                # Full reference
meld guide all --json         # Structured JSON
```

Topics: `syntax`, `control-flow`, `errors`, `stdlib`, `diagnostics`, `workflow`, `builds`, `all`

### `meld fmt <file.meld>`

Format source code.

```bash
meld fmt program.meld
meld fmt --check program.meld   # Check without modifying
```

## JSON Output Format

When `--json` is passed, output is structured:

```json
{
  "diagnostics": [
    {
      "level": "error",
      "code": "E001",
      "message": "undefined variable 'x'",
      "location": {
        "file": "program.meld",
        "line": 5,
        "column": 12
      },
      "fix": {
        "description": "Define variable before use",
        "replacement_text": "val x = ...",
        "confidence": 0.9
      }
    }
  ]
}
```

Each diagnostic includes:
- **level** — `error`, `warning`, `info`, `hint`
- **code** — Stable identifier (e.g., `E001`, `W003`)
- **message** — Human-readable description
- **location** — File, line, column
- **fix** (optional) — Machine-readable repair suggestion with confidence score

## REPL Commands

In interactive mode (`meld run -i`):

```
> val x = 42
> println(x)
42
> fnc double(n: int) -> int { rtn n * 2 }
> double(x)
84
```

The REPL maintains persistent state — definitions carry across lines.

## DAP Debugging

```bash
meld run program.meld --debug
```

Starts a Debug Adapter Protocol server. VS Code with the Meld extension connects automatically. Supports:
- Breakpoints (line-based)
- Step over / step into / step out
- Variable inspection
- Call stack display

## Environment Variables

| Variable | Purpose |
|----------|---------|
| `MELD_PATH` | Additional module search paths |
| `MELD_DEBUG` | Enable debug logging |
| `MELD_JSON` | Default to JSON output |

## Exit Codes

| Code | Meaning |
|------|---------|
| 0 | Success |
| 1 | Runtime error |
| 2 | Compilation error |
| 3 | File not found |

## Daemon Commands

```bash
meldd start                 # Start background daemon
meldd stop                  # Stop daemon
meldd status                # Check daemon status
```

The daemon provides LSP and MCP services over Unix sockets.
