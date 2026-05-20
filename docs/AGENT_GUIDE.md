# Agent Guide

How AI agents work with Meld. This document covers structured diagnostics, the repair loop, and Meld-specific patterns that agents must know.

## The Agent Repair Loop

```
1. Agent writes .meld code
2. Run: meld run file.meld --json
3. Parse JSON diagnostics (stable codes + fix suggestions)
4. Apply fixes
5. Re-run until clean
```

## JSON Diagnostic Format

```bash
meld run program.meld --json
```

Output:
```json
{
  "diagnostics": [
    {
      "level": "error",
      "code": "E001",
      "message": "undefined variable 'x'",
      "location": { "file": "program.meld", "line": 5, "column": 12 },
      "fix": {
        "description": "Define variable before use",
        "replacement_text": "val x = ...",
        "confidence": 0.9
      }
    }
  ]
}
```

**Fields agents should use:**
- `code` — Stable identifier. Match on this, not the message text.
- `fix.replacement_text` — Suggested code change.
- `fix.confidence` — How likely the fix is correct (0.0–1.0).
- `location` — Exact position for the edit.

## Critical Syntax Rules

Agents trained on mainstream languages will make these mistakes. **Memorize these:**

### ❌ No `if`/`else`/`while`/`for`/`match` keywords

```meld
// WRONG — these are not valid Meld
if (x > 0) { ... }
while (running) { ... }
for (item in list) { ... }

// CORRECT — use library functions
when(x > 0).then({ ... }).else({ ... })
forEach(list, fnc(item: any) -> () { ... })
```

### ❌ No `return` — use `rtn`

```meld
// WRONG
return value

// CORRECT
rtn value
```

### ❌ No `fn`/`func`/`def`/`function` — use `fnc`

```meld
// WRONG
fn add(a: int, b: int) -> int { ... }

// CORRECT
fnc add(a: int, b: int) -> int { ... }
```

### ❌ No `let`/`const` — use `val`/`var`

```meld
// WRONG
let x = 42
const PI = 3.14

// CORRECT
val x = 42
val PI = 3.14
var mutable = 0
```

### ❌ No `import` — use `imp`

```meld
// WRONG
import mathlib

// CORRECT
imp mathlib
```

### ❌ No `=>` in lambdas — use `->`

```meld
// WRONG
val f = (x) => x * 2

// CORRECT
val f = fnc(x: int) -> int { rtn x * 2 }
```

### ❌ No `extend` keyword — repeat the type name

```meld
// WRONG
extend Vec2 { fn length() { ... } }

// CORRECT — just define a function with Vec2 as first param
fnc length(v: Vec2) -> float { rtn (v.x * v.x + v.y * v.y) }
```

## Meld Idioms

### Conditional Expression
```meld
val label = when(score >= 90).then({ rtn "A" })
    .when(score >= 80).then({ rtn "B" })
    .when(score >= 70).then({ rtn "C" })
    .else({ rtn "F" })
```

### Error Handling Chain
```meld
fnc process(input: string) -> Result {
    val parsed = parse(input)
    rtn when(parsed.is-ok == false).then({ rtn parsed }).else({
        val validated = validate(unwrap(parsed))
        rtn when(validated.is-ok == false).then({ rtn validated }).else({
            rtn transform(unwrap(validated))
        })
    })
}
```

### Builder Pattern
```meld
val config = Config-default()
    .with-host("api.example.com")
    .with-port(443)
    .with-debug(true)
```

### Collection Pipeline
```meld
val result = filter(users, fnc(u: any) -> bool { rtn u.active })
val names = map(result, fnc(u: any) -> any { rtn u.name })
val count = len(names)
```

### Struct + Methods
```meld
// Define struct implicitly by using it
val p = Point { x = 3, y = 4 }

// Methods are functions with the type as first param
fnc distance(p: Point) -> float { rtn (p.x * p.x + p.y * p.y) }

// Call with dot syntax
println(p.distance())
```

## MCP Server Integration

The Meld daemon exposes an MCP (Model Context Protocol) server:

```bash
# Start daemon (auto-starts on first meld command)
meldd start

# MCP socket location
.meld/mcp.sock
```

**Available MCP tools:**
- `analyze` — Analyze a file for errors
- `fix` — Get fix suggestions
- `complete` — Code completion at position
- `hover` — Type info at position
- `references` — Find all usages
- `rename` — Rename symbol across files

## LSP Integration

The daemon also provides LSP over Unix socket:

```bash
.meld/lsp.sock
```

VS Code with the Meld extension connects automatically.

## File Conventions

```
project/
├── meld.toml           # Project manifest
├── src/
│   ├── main.meld       # Entry point
│   └── helpers.meld    # Imported with: imp helpers
└── tests/
    └── test_main.meld  # Test file
```

## Common Diagnostic Codes

| Code | Meaning | Typical Fix |
|------|---------|-------------|
| E001 | Undefined variable | Add `val x = ...` before use |
| E002 | Undefined function | Define the function or check spelling |
| E003 | Wrong argument count | Match parameter count to definition |
| E004 | Type mismatch | Ensure consistent types in operations |
| E005 | Missing return | Add `rtn value` to function body |
| W001 | Unused variable | Remove or prefix with `_` |
| W002 | Unreachable code | Remove dead code after `rtn` |

## Agent Workflow Checklist

1. **Before writing code:** Run `meld guide syntax` to get version-matched rules
2. **Use `val` not `let`**, `fnc` not `fn`, `rtn` not `return`
3. **No control flow keywords** — use `when().then().else()`
4. **Lambda syntax:** `fnc(param: type) -> type { rtn expr }`
5. **Method calls:** Define `fnc name(self: Type, ...) -> ...` then call `instance.name()`
6. **Effect declarations:** Add `@uses(EffectName)` before functions that use effects
7. **After writing:** Run `meld check file.meld` and parse JSON diagnostics
8. **Fix errors** using `meld fix --plan file.meld`
9. **Iterate** until `"ok": true`
10. **Execute:** Run `meld run file.meld`
