# Meld Programming Language - C++ Implementation

A C++ implementation of the Meld programming language — a homoiconic, library-first language with AI-native features, algebraic effects, and a minimal 20-primitive kernel.

## Key Design Principles

- **Minimal Kernel ("The Meld 20")**: Exactly 20 primitives organized into 7 categories
- **Library-First**: All control flow (effects, exceptions, async, generators, pattern matching) is built as library code — NO keywords for `match`/`case`, `try`/`catch`, `async`/`await`
- **AI-Native**: Algebraic effects for sandboxing AI-generated code, provenance tracking, trust levels
- **Homoiconic**: Code and data share the same representation (AST as first-class data)
- **Modern C++23**: `std::expected`, deducing this, concepts, ranges, spaceship operator

## The Meld 20 — Kernel Primitives

| Category | Primitives | Purpose |
|---|---|---|
| Data Structure (The Matter) | `cell`, `vec`, `symbol`, `type`, `scope` | Building blocks for AST, arrays, identifiers, types, environments |
| Scalar (The Values) | `int`, `float`, `bool`, `nil` | Numeric, boolean, and unit values |
| Execution (The Energy) | `lambda`, `apply`, `eval`, `quote` | Closures, function calls, interpretation, macro support |
| Control Flow (The Physics) | `primitive_suspend` | Single primitive for continuations — all control flow built on this |
| Memory & Binding (The Context) | `def`, `set`, `lookup` | Variable definition, mutation, scope chain traversal |
| AI & Metadata (The Provenance) | `meta_set`, `meta_get` | Hidden metadata attachment for provenance, docs, types |
| Interop (The Bridge) | `native_call`, `native_load` | FFI to host environment (C, JVM, JS) |

## Language Syntax

```meld
// Function declarations use `fnc`, returns use `rtn`
fnc factorial(n: int) -> int {
    (n <= 1).ifTrue { rtn 1 }.ifFalse { rtn n * factorial(n - 1) }
}

// Immutable by default: val (immutable), var (mutable)
val name = "Meld"
var counter = 0

// Effect annotations with @uses
@uses(FileSystem, Console)
fnc processFile(path: string) -> Result<string, Error> {
    val content = FileSystem.read(path)
    Console.println(content)
    rtn Ok(content)
}

// Library-based pattern matching via .match method
color.match {
    on<Color.Red> { "red" }
    on<Color.Blue> { "blue" }
    otherwise { "unknown" }
}

// Error handling with Result<T, E> and ? operator
fnc loadConfig(path: string) -> Result<Config, string> {
    val content = readFile(path)?
    val parsed = parseJson(content)?
    rtn Ok(Config.from(parsed))
}

// Elvis operator for nullable types
val displayName = user.name ?: "Anonymous"
```

## Project Structure

```
meld-core/
├── include/meld/
│   ├── kernel/          # The Meld 20 primitives
│   ├── parser/          # Boost Spirit X3 lexer/parser
│   ├── compiler/        # Type checker, IR, code generation
│   ├── meta/            # Metaprogramming and macro system
│   ├── types/           # Type system (struct, class, trait, generics)
│   ├── stdlib/          # Standard library (.meld files)
│   └── serialization/   # MELD-B binary format
├── src/                 # C++ implementation
├── tests/               # Test suites
├── examples/            # Demo files (.cpp + .meld pairs)
└── BUILD.bazel
```

## Building

### Prerequisites

- Bazel (see `.bazelversion`)
- C++23 compiler: GCC 13+, Clang 16+, or MSVC 19.35+

### Commands

```bash
bazel build //packages/meld-lang:all       # Build everything
bazel test //packages/meld-lang:tests      # Run tests
bazel run //packages/meld-lang:meld        # Run REPL/demos
```

## Examples

Examples follow a strict two-file structure per the project conventions:
- `example-name.cpp` — C++ test harness
- `example-name.meld` — Pure Meld language demonstration

All example files use kebab-case naming.

## License

TBD
