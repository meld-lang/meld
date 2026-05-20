# Examples Index

60+ runnable examples organized by concept. Start at the top and work down — each section builds on the previous.

Run any example:
```bash
meld run examples/<filename>.meld
```

## First Programs

| # | File | What It Teaches |
|---|------|-----------------|
| 01 | `01-hello-world.meld` | `fnc main`, `println`, basic functions |
| 02 | `02-variables-and-types.meld` | `val`, `var`, primitive types, type annotations |
| 03 | `03-functions.meld` | Parameters, return types, `rtn`, calling functions |
| 04 | `04-type-definitions.meld` | `struct`, `class`, `enum`, generics, type aliases |
| 05 | `05-control-flow.meld` | `when/then/else`, `ifTrue/ifFalse`, iteration |

## Data and Types

| # | File | What It Teaches |
|---|------|-----------------|
| 11 | `11-collections.meld` | Lists, `push`, `len`, indexing |
| 12 | `12-enums-and-variants.meld` | Enum declarations, variant matching |
| 13 | `13-generics.meld` | Generic functions, `any` type |
| 14 | `14-type-extensions.meld` | Adding methods to existing types |
| 15 | `15-refinement-types.meld` | Constrained types with predicates |
| 17 | `17-user-defined-types.meld` | Custom type kinds |
| 19 | `19-string-types.meld` | String operations, template strings |
| 21 | `21-union-intersection-types.meld` | Union and intersection types |
| 43 | `43-type-aliases.meld` | Compile-time type spelling |
| 44 | `44-const-values.meld` | Top-level immutable constants |
| 59 | `59-const-generics.meld` | Fixed-capacity containers with size parameters |

## Structs and Methods

| # | File | What It Teaches |
|---|------|-----------------|
| 31 | `31-structs-and-methods.meld` | Struct creation, fields, method dispatch |
| 24 | `24-operators.meld` | Operator overloading (`add`, `sub`, `mul`) |
| 41 | `41-nested-structs.meld` | Structs containing structs |
| 46 | `46-static-methods.meld` | Static constructors vs instance methods |
| 47 | `47-builder-pattern.meld` | Fluent builders with defaults and chaining |

## Functions and Closures

| # | File | What It Teaches |
|---|------|-----------------|
| 33 | `33-recursion.meld` | Recursive functions |
| 34 | `34-closures.meld` | Closures, mutable capture |
| 39 | `39-trailing-lambdas.meld` | Lambda syntax patterns |
| 18 | `18-multiple-dispatch.meld` | Same function name, different types |
| 22 | `22-traits.meld` | Trait-like interfaces via dispatch |
| 45 | `45-trait-bounds.meld` | Generic constraints via traits |

## Control Flow

| # | File | What It Teaches |
|---|------|-----------------|
| 32 | `32-conditionals.meld` | `when/then/else` patterns |
| 36 | `36-pattern-matching.meld` | `match` on values |
| 40 | `40-guard-clauses.meld` | Early-return patterns |
| 16 | `16-flow-state-machines.meld` | State machine patterns |

## Error Handling

| # | File | What It Teaches |
|---|------|-----------------|
| 06 | `06-null-safety.meld` | `Option`, `Some`, `None` |
| 25 | `25-error-handling.meld` | `Result`, `Ok`, `Err`, `unwrap` |
| 35 | `35-result-option.meld` | Combining Result and Option |
| 42 | `42-error-propagation.meld` | Chaining fallible operations |
| 48 | `48-typed-errors.meld` | Named error types, error composition |

## Effects and Capabilities

| # | File | What It Teaches |
|---|------|-----------------|
| 07 | `07-effects.meld` | Effect system basics, `Console` |
| 20 | `20-effect-sandboxing.meld` | Pure vs effectful functions |
| 37 | `37-templates-and-effects.meld` | Template strings with effects |

## Memory and Resources

| # | File | What It Teaches |
|---|------|-----------------|
| 23 | `23-ownership.meld` | Value semantics, copy behavior |
| 49 | `49-defer-cleanup.meld` | RAII-style deterministic cleanup |
| 55 | `55-explicit-allocation.meld` | Fixed buffers, arena allocators |

## Modules and Packages

| # | File | What It Teaches |
|---|------|-----------------|
| 09 | `09-modules.meld` | Module system basics |
| 38 | `38-module-imports.meld` | `imp` for cross-file imports |
| 57 | `57-multi-file-package/` | Multi-file package with manifest |

## Standard Library Patterns

| # | File | What It Teaches |
|---|------|-----------------|
| 50 | `50-file-io.meld` | File read/write with capabilities |
| 51 | `51-cli-args.meld` | Command-line argument parsing |
| 52 | `52-parsing.meld` | Scanner predicates, tokenization |
| 53 | `53-binary-encoding.meld` | Varint encoding, checksums |
| 60 | `60-path-manipulation.meld` | Path join, basename, extension |
| 61 | `61-json-data.meld` | JSON encoding/decoding |
| 62 | `62-platform-capabilities.meld` | Time, random, process, hashing |

## Interop and Advanced

| # | File | What It Teaches |
|---|------|-----------------|
| 54 | `54-ffi-interop.meld` | C-shaped structs, FFI patterns |
| 56 | `56-compile-time-meta.meld` | Type reflection, metadata |
| 10 | `10-advanced.meld` | Advanced patterns combined |
| 27 | `27-design-by-contract.meld` | Pre/post conditions |
| 28 | `28-concurrency.meld` | Async patterns (simulated) |
| 29 | `29-web-server.meld` | HTTP handler patterns |
| 26 | `26-real-world-api.meld` | Real-world API design |

## Testing

| # | File | What It Teaches |
|---|------|-----------------|
| 08 | `08-testing.meld` | Assert-based testing patterns |

## Agent Workflow

| # | File | What It Teaches |
|---|------|-----------------|
| 58 | `58-agent-repair-tour/broken.meld` | Deliberate errors for diagnostic demo |
| 58 | `58-agent-repair-tour/fixed.meld` | Repaired version showing fix patterns |

## Running Examples

```bash
# Run a single example
meld run examples/01-hello-world.meld

# Run with verbose output
meld run examples/01-hello-world.meld --verbose

# Run with JSON diagnostics (for agents)
meld run examples/01-hello-world.meld --json

# Interactive exploration after running
meld run examples/01-hello-world.meld -i
```
