# Learn Meld

A practical tour of the language. Work through these sections in order — each builds on the previous.

## Philosophy

Meld is built on three principles:

1. **Minimal kernel** — ~22 C++ builtins bootstrap the language. Control flow (`when/then/else`), collections (`forEach`, `map`, `filter`, `reduce`), and pattern matching are native for performance. Error handling (`Ok`, `Err`, `unwrap`) and utilities (`contains`, `push`, `assert`) are pure Meld library code.
2. **Explicit effects** — Side effects are tracked. Pure functions stay pure.
3. **AI-native tooling** — Structured diagnostics, JSON output, and machine-readable fix suggestions.

## Values and Types

```meld
// Immutable binding
val name = "Alice"
val age = 30
val pi = 3.14159
val active = true

// Mutable binding
var count = 0
count = count + 1

// Type annotations are on parameters and return types
fnc greet(name: string) -> string {
    rtn "Hello, " + name
}
```

**Primitive types:** `int`, `float`, `string`, `bool`, `list`, `any`

**Type inference:** Meld infers types from values. Annotations are required on function signatures.

## Functions

```meld
// Named function
fnc square(x: int) -> int {
    rtn x * x
}

// Multiple parameters
fnc clamp(value: int, min: int, max: int) -> int {
    rtn when(value < min).then({ rtn min })
        .when(value > max).then({ rtn max })
        .else({ rtn value })
}

// Inline lambda
val double = fnc(x: int) -> int { rtn x * 2 }

// Functions are values — pass them around
fnc apply-twice(f: any, x: int) -> int {
    rtn f(f(x))
}
// apply-twice(double, 3) => 12
```

## Control Flow

No keywords. All control flow is library functions:

```meld
// Conditional expression
val result = when(x > 0).then({ rtn "positive" })
    .when(x == 0).then({ rtn "zero" })
    .else({ rtn "negative" })

// Side-effect conditional
when(debug).ifTrue({ println("debug mode") })

// Pattern matching
val day = match(n, [
    [1, { rtn "Monday" }],
    [2, { rtn "Tuesday" }],
    [3, { rtn "Wednesday" }]
])

// Iteration
forEach([1, 2, 3], fnc(n: any) -> () { println(n) })

// Transform
val squares = map([1, 2, 3, 4], fnc(n: any) -> any { rtn n * n })

// Filter
val evens = filter([1, 2, 3, 4], fnc(n: any) -> bool { rtn n % 2 == 0 })

// Reduce
val sum = reduce([1, 2, 3, 4], 0, fnc(acc: any, n: any) -> any { rtn acc + n })
```

## Structs

```meld
// Create structs with literal syntax
val point = Vec2 { x = 3, y = 4 }

// Access fields
println(point.x)

// Mutation (only on var bindings)
var p = Vec2 { x = 0, y = 0 }
p.x = 10
```

Structs are value types — assignment copies.

## Methods and Dispatch

Any function whose first parameter is a type can be called with dot syntax:

```meld
fnc magnitude(v: Vec2) -> float {
    rtn (v.x * v.x + v.y * v.y)
}

// These are equivalent:
magnitude(point)
point.magnitude()
```

**Multiple dispatch** — define the same function name for different types:

```meld
fnc area(c: Circle) -> float { rtn 3.14 * c.radius * c.radius }
fnc area(r: Rect) -> float { rtn r.w * r.h }

// Dispatches based on argument type
println(area(my_circle))
println(area(my_rect))
```

## Operator Overloading

Define `add`, `sub`, `mul`, `div`, `eq` for your types:

```meld
fnc add(a: Vec2, b: Vec2) -> Vec2 {
    rtn Vec2 { x = a.x + b.x, y = a.y + b.y }
}

val c = Vec2 { x = 1, y = 2 } + Vec2 { x = 3, y = 4 }
// c = Vec2 { x = 4, y = 6 }
```

## Collections

```meld
// Arrays (lists)
val nums = [1, 2, 3, 4, 5]
println(len(nums))       // 5
println(nums[0])         // 1

// Push returns a new list (immutable by default)
val more = push(nums, 6)

// String operations
val s = "hello world"
println(len(s))                    // 11
println(contains(s, "world"))      // true
println(starts-with(s, "hello"))   // true
println(split(s, " "))             // ["hello", "world"]
println(replace(s, "world", "meld"))  // "hello meld"
```

## Error Handling

```meld
// Result type: Ok(value) or Err(message)
fnc parse(s: string) -> Result {
    rtn when(s == "42").then({ rtn Ok(42) })
        .else({ rtn Err("not a number: " + s) })
}

val r = parse("42")
println(unwrap(r))  // 42

// Option type: Some(value) or None
val name = Some("Alice")
println(unwrap(name))  // "Alice"

// Chain fallible operations
fnc load-config(path: string) -> Result {
    val content = read-file(path)
    rtn when(content.is-ok == false).then({ rtn content })
        .else({ rtn parse(unwrap(content)) })
}
```

## Closures

```meld
// Closures capture their environment
var count = 0
val tick = { count = count + 1 }
tick()
tick()
println(count)  // 2

// Closures with parameters
val adder = fnc(x: int) -> fnc {
    rtn fnc(y: int) -> int { rtn x + y }
}
val add5 = adder(5)
println(add5(3))  // 8
```

## Template Strings

```meld
val name = "World"
val x = 42
println(`Hello ${name}, the answer is ${x}`)
println(`2 + 2 = ${2 + 2}`)
```

## Modules

```meld
// Import another .meld file from the same directory
imp mathlib

fnc main() -> () {
    println(square(7))  // function from mathlib.meld
}
```

## Effects

```meld
// Effects make side effects explicit
Console.println("via effect system")

// Pure functions have no effects
fnc pure-add(a: int, b: int) -> int { rtn a + b }

// Effectful functions declare what they do
fnc log-and-add(a: int, b: int) -> int {
    Console.println("adding " + a + " + " + b)
    rtn a + b
}
```

## Enums and Pattern Matching

```meld
enum Direction { North, South, East, West }

// Match on values
val name = match(dir, [
    ["North", { rtn "up" }],
    ["South", { rtn "down" }],
    ["East", { rtn "right" }],
    ["West", { rtn "left" }]
])
```

## Assertions and Testing

```meld
// Runtime assertions
assert(1 + 1 == 2, "basic math")
assert(len("hello") == 5, "string length")

// Test pattern
fnc test-addition() -> () {
    assert(add(2, 3) == 5, "2+3 should be 5")
    assert(add(0, 0) == 0, "0+0 should be 0")
    assert(add(-1, 1) == 0, "-1+1 should be 0")
    println("all addition tests passed")
}
```

## What's Different About Meld

| Traditional | Meld |
|---|---|
| `if (x > 0) { ... }` | `when(x > 0).then({ ... })` |
| `for (x in list) { ... }` | `forEach(list, fnc(x) { ... })` |
| `return value` | `rtn value` |
| `function` / `def` / `fn` | `fnc` |
| `const` / `let` | `val` (immutable) / `var` (mutable) |
| `import module` | `imp module` |
| `x.method()` | Same — calls `method(x)` |
| `try/catch` | `Result` + `when(...).then(...)` |

## Next Steps

- [Language Reference](LANGUAGE_REFERENCE.md) — complete syntax and semantics
- [Standard Library](STANDARD_LIBRARY.md) — all built-in functions
- [Examples Index](EXAMPLES_INDEX.md) — 60+ runnable examples by concept
- [Agent Guide](AGENT_GUIDE.md) — AI-native workflow documentation
