# Meld Keywords

## Core Keywords (5)

These are the fundamental building blocks. They cannot be used as identifiers.

| Keyword | Purpose | Example |
|---------|---------|---------|
| `fnc` | Define a function | `fnc add(a: int, b: int) -> int { ... }` |
| `val` | Immutable binding | `val x = 42` |
| `var` | Mutable binding | `var count = 0` |
| `rtn` | Return a value | `rtn x + 1` |
| `imp` | Import a module | `imp std.random` |

## Syntax Keywords

These have dedicated parser rules. They are not identifiers and cannot be
redefined, but they are not "core" in the sense that the language could
theoretically express them as macros over the core 5 (once the macro system
is implemented).

| Keyword | Purpose |
|---------|---------|
| `opr` | Define a custom operator |
| `effect` | Declare an effect interface |
| `handle` | Provide an effect handler |
| `extend` | Add methods to an existing type |
| `type` | Type declaration |
| `typealias` | Type alias |
| `newtype` | Newtype wrapper |
| `namespace` | Namespace declaration |
| `where` | Type constraints |
| `true` | Boolean literal |
| `false` | Boolean literal |

## Banned Words

These are reserved to produce helpful errors for users coming from other
languages. They cannot be used as identifiers.

`import`, `from`, `as`, `for`, `while`, `switch`, `case`, `break`,
`continue`, `async`, `await`, `return`, `function`, `void`

## Library-Defined Names

These are regular functions/values, not keywords. They can be shadowed.

| Name | Defined As |
|------|-----------|
| `when`, `then`, `else` | Native builtin functions (conditional dispatch) |
| `forEach`, `map`, `filter`, `reduce`, `match` | Native builtin functions (collections) |
| `println`, `print`, `len`, `panic` | Native builtin functions (I/O, core) |
| `ifTrue`, `ifFalse`, `push` | Prelude functions (pure Meld) |
| `Ok`, `Err`, `Some`, `None`, `unwrap` | Prelude functions (Result/Option) |
| `nil` | Predefined constant |

## Design Intent

The goal is to minimize core keywords and maximize library-defined surface
syntax. The syntax keywords (`effect`, `handle`, `extend`, etc.) currently
require parser support, but the long-term plan is to make them expressible
as hygienic macros once the MMS (Meld Macro System) is implemented. At that
point, only the core 5 would remain as true parser-level keywords.
