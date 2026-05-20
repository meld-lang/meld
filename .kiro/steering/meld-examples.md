# Meld Project Steering

## Language Syntax Rules

**Keywords (only 5):** `fnc`, `val`, `var`, `rtn`, `imp`

**Critical — do NOT use:**
- `if`/`else`/`while`/`for`/`match` — use `when().then().else()`, `forEach`, `map`, `filter`, `reduce`, `match()`
- `return` — use `rtn`
- `fn`/`func`/`def`/`function` — use `fnc`
- `let`/`const` — use `val` (immutable) / `var` (mutable)
- `import` — use `imp`
- `=>` in lambdas — use `->`
- `extend` keyword — define functions with the type as first param

## Examples Structure

All examples in `meld-examples/examples/` are `.meld` files run through a single generic runner.

```
meld-runner <file.meld> [--verbose] [--warnings-ok] [--json]
```

**Rules:**
- Every example is a `.meld` file — no per-example `.cpp` harnesses
- `.meld` files contain pure Meld code only
- Do NOT create additional `.cpp` files in the examples directory
- To add a new example, just create a `.meld` file and run it with `meld-runner`
- Borrow violations cause compilation failure; use `--warnings-ok` to override

**Numbering:** 01–05 basics, 06–10 core, 11–29 features, 30–42 patterns, 43–62 advanced

## CLI Commands

```bash
meld run file.meld              # Run program
meld run file.meld --json       # Structured output
meld explain E002               # Diagnostic explanation (JSON default)
meld explain E002 --text        # Human-readable
meld fix --plan file.meld       # Fix plan (JSON default)
meld guide syntax               # Language guidance (JSON default)
meld graph src/                 # Module dependency graph
meld doctor                     # Environment health check
```

## Conformance Tests

```bash
./meld-conformance/run.sh       # Run all fixtures (10 tests)
```

Each fixture is a `.meld` + `.expected` pair. The runner uses `--warnings-ok`.

## Architecture

- `meld-core/` — Parser (X3), compiler passes, kernel, effects, types
- `meld-interpreter/` — AST interpreter, DAP debugger, REPL
- `meld-daemon/` — LSP + MCP server (10 tools), semantic model
- `meld-cli/` — Unified CLI (`explain`, `fix`, `guide`, `graph`, `doctor`)
- `meld-examples/` — Runner binary + 60+ examples + lightweight LSP/MCP
- `meld-conformance/` — Fixture-based language behavior tests
- `vscode-meld/` — VS Code extension

## MCP Tools (Daemon)

`analyze_safety`, `trace_effect`, `query_type`, `query_ownership`, `get_diagnostics`, `structural_diff`, `meld_eval`, `meld_check`, `search_api`, `execute_script`

## Documentation

All docs in `docs/`: GETTING_STARTED, LEARN_MELD, LANGUAGE_REFERENCE, STANDARD_LIBRARY, CLI_REFERENCE, EXAMPLES_INDEX, AGENT_GUIDE
