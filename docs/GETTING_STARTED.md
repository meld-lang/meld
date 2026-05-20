# Getting Started

Install Meld, run your first program, and explore interactively.

## Install

```bash
# Build from source (requires Bazel 7+)
bazel build //meld-examples:meld

# The binary is at:
bazel-bin/meld-examples/meld

# Symlink for convenience:
ln -sf $(pwd)/bazel-bin/meld-examples/meld /usr/local/bin/meld
```

## Run Your First Program

Create `hello.meld`:

```meld
fnc main() -> () {
    println("Hello from Meld!")
}
```

Run it:

```bash
meld run hello.meld
```

## The Basics

Every Meld program needs a `main` function. Here's what the syntax looks like:

```meld
// Functions use fnc, return with rtn
fnc add(a: int, b: int) -> int {
    rtn a + b
}

// val = immutable, var = mutable
val name = "Meld"
var count = 0

fnc main() -> () {
    println("2 + 3 = " + add(2, 3))
    println(`Hello ${name}`)
    count = count + 1
    println("count = " + count)
}
```

The important parts:
- `fnc` declares a function. Parameters are typed. Return type follows `->`.
- `rtn` returns a value (not `return`).
- `val` creates an immutable binding. `var` creates a mutable one.
- `println` writes to stdout. Template strings use backticks and `${expr}`.
- No semicolons. No `if`/`else`/`while` keywords — control flow is library-based.

## Control Flow

Meld has no `if`/`else`/`while`/`for` keywords. Control flow uses library functions:

```meld
fnc main() -> () {
    val x = 10

    // Conditional: when(...).then({...}).else({...})
    val label = when(x > 5).then({ rtn "big" }).else({ rtn "small" })
    println(label)

    // Boolean dispatch: when(cond).ifTrue({...}).ifFalse({...})
    when(x == 10).ifTrue({ println("it's ten!") })

    // Iteration: forEach, map, filter, reduce
    val nums = [1, 2, 3, 4, 5]
    forEach(nums, fnc(n: any) -> () { println("  " + n) })

    val doubled = map(nums, fnc(n: any) -> any { rtn n * 2 })
    println("doubled: " + doubled)
}
```

## Structs and Methods

```meld
fnc to-string(v: Vec2) -> string {
    rtn "(" + v.x + ", " + v.y + ")"
}

fnc add(a: Vec2, b: Vec2) -> Vec2 {
    rtn Vec2 { x = a.x + b.x, y = a.y + b.y }
}

fnc main() -> () {
    val a = Vec2 { x = 1, y = 2 }
    val b = Vec2 { x = 3, y = 4 }

    // Method syntax: obj.method() calls method(obj)
    println(a.to-string())

    // Operator overloading: + calls add(a, b)
    val c = a + b
    println(c.to-string())
}
```

Structs are created with `Name { field = value }`. Methods are just functions whose first parameter matches the type — call them with dot syntax.

## Error Handling

Meld uses `Result` and `Option` types (pure library code, not keywords):

```meld
fnc divide(a: int, b: int) -> Result {
    rtn when(b == 0)
        .then({ rtn Err("division by zero") })
        .else({ rtn Ok(a / b) })
}

fnc main() -> () {
    val r = divide(10, 2)
    println("10/2 = " + unwrap(r))

    val bad = divide(5, 0)
    println("error: " + bad.error)
}
```

## Interactive REPL

```bash
# Start REPL
meld run -i

# Load a file then explore
meld run hello.meld -i
```

The REPL maintains state between expressions — define functions, create values, and test interactively.

## Structured Output (for AI Agents)

```bash
# JSON diagnostics with fix suggestions
meld run program.meld --json
```

Output includes diagnostic codes, locations, and machine-readable fix suggestions that agents can act on directly.

## Debug with VS Code

```bash
# Start DAP debug server (VS Code attaches automatically)
meld run program.meld --debug
```

Set breakpoints, step through code, and inspect variables.

## Next Steps

- Read [Learn Meld](LEARN_MELD.md) for a complete language tour.
- Browse the [Examples Index](EXAMPLES_INDEX.md) organized by concept.
- See the [Language Reference](LANGUAGE_REFERENCE.md) for full syntax details.
- Check the [Agent Guide](AGENT_GUIDE.md) for AI-native workflows.
