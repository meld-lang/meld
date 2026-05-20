# Meld

The language that is understandable by humans, transformable by tools, and safely extensible by AI agents

Meld combines a minimal kernel with algebraic effects to make every side effect visible, interceptable, and auditable — so AI-generated code can be sandboxed by construction.

## Why Meld?

```meld
// AI-generated code declares what it does
fnc sync-files(src: string, dst: string) -> () {
    val content = FileSystem.read(src)
    FileSystem.write(dst, content)
    Console.println("synced " + src)
}

// You control what it's allowed to do
var log = []
handle Console {
    fnc println(msg: string) -> () {
        log = arr-push(log, msg)  // capture, don't print
    }
}
```

Every effect is interceptable. Mock the filesystem for tests. Capture all I/O for audit. Deny network access entirely. The sandbox is the language, not a container.

## Quick Start

```bash
git clone https://github.com/meld-lang/meld.git && cd meld
./install.sh                   # Build and install to ~/.local/bin

meld run program.meld          # Run a program (JSON output by default)
meld run --text program.meld   # Plain text output
meld run -i                    # Interactive REPL
meld run program.meld --debug  # DAP debugging (VS Code attaches)
```

## Agent-Native Tooling

```bash
# Structured diagnostics (JSON by default)
meld explain E002              # Rich explanation of any diagnostic code
meld fix --plan src/main.meld  # Machine-readable fix plan
meld guide syntax              # Version-matched language guidance
meld graph src/                # Module dependency graph
meld doctor                    # Environment health check
```

All agent-facing commands output JSON by default. Use `--text` for human-readable output.

## Language Features

```meld
// 5 keywords: fnc, val, var, rtn, imp — everything else is library

// Functions + conditionals (when/then/else is a library function, not syntax)
fnc factorial(n: int) -> int {
    rtn when(n == 0).then({ rtn 1 }).else({ rtn n * factorial(n - 1) })
}

// Structs + multiple dispatch
fnc area(c: Circle) -> float { rtn 3.14 * c.radius * c.radius }
fnc area(r: Rect) -> float { rtn r.w * r.h }

// Algebraic effects — side effects are explicit and interceptable
fnc greet(name: string) -> () {
    Console.println("Hello, " + name + "!")
}

// Effect sandboxing — override any effect for a scope
var log = []
handle Console {
    fnc println(msg: string) -> () { log = arr-push(log, msg) }
}

// Collections, closures, pattern matching, Result/Option — all library code
val doubled = map([1, 2, 3], fnc(x: int) -> int { rtn x * 2 })
val day = match(3, [[1, { rtn "Mon" }], [2, { rtn "Tue" }], [3, { rtn "Wed" }]])
```

## Architecture

**Kernel primitives (C++):** `println`, `print`, `len`, `replace`, `split`, `str-find`, `substr`, `to-string`, `type-of`, `panic`, `when`/`then`/`else`, `forEach`, `map`, `filter`, `reduce`, `match`

**Parser:** Boost.Spirit X3 (~1600 lines)

**Tooling:**
- VS Code extension (syntax highlighting, snippets, DAP debugging)
- `meld explain` / `meld fix --plan` / `meld guide` — agent repair loop
- `meld graph` / `meld doctor` — project inspection
- MCP server (10 tools: eval, check, safety, effects, types, ownership, diagnostics, diff, search, execute)
- LSP server (diagnostics, completions, hover, go-to-def, rename, semantic tokens)
- DAP debugger (breakpoints, stepping, variable inspection)
- REPL with persistent environment

## Documentation

- [Getting Started](docs/GETTING_STARTED.md) — Install, first program, REPL
- [Learn Meld](docs/LEARN_MELD.md) — Practical language tour
- [Language Reference](docs/LANGUAGE_REFERENCE.md) — Full syntax and semantics
- [Standard Library](docs/STANDARD_LIBRARY.md) — All built-in functions
- [CLI Reference](docs/CLI_REFERENCE.md) — Commands, flags, JSON format
- [Examples Index](docs/EXAMPLES_INDEX.md) — 60+ examples by concept
- [Agent Guide](docs/AGENT_GUIDE.md) — AI-native workflow documentation

## Examples

60+ executable examples in `meld-examples/examples/` covering:
arrays, structs, methods, operators, conditionals, recursion, closures,
Result/Option, pattern matching, effects, effect sandboxing, modules,
traits, multiple dispatch, error handling, state machines, type aliases,
const generics, builders, typed errors, defer/cleanup, file I/O, CLI args,
parsing, binary encoding, FFI interop, explicit allocation, metaprogramming,
multi-file packages, and agent repair workflows.

## Status

- ✅ X3 parser (all std/ files parse)
- ✅ Interpreter with unified kernel
- ✅ Algebraic effects with scoped handlers
- ✅ 60+ examples compile and execute
- ✅ Conformance test suite (13 fixtures)
- ✅ DAP debugger
- ✅ VS Code extension
- ✅ REPL
- ✅ JSON-first structured output
- ✅ `meld explain` / `meld fix` / `meld guide` (agent repair loop)
- ✅ MCP server (10 tools)
- ✅ LSP server
- ✅ Metadata system with namespace safety
- ✅ Delimited continuations (`kernel.suspend/resume/mark`)
- ✅ Effect enforcement (`uses()` + `sandbox()`)
- ⏳ `@uses` annotation syntax (currently `uses()` builtin)
- ⏳ LLVM native codegen
