# Language Reference

Complete syntax and behavior reference for Meld.

## Lexical Structure

### Comments
```meld
// Single-line comment
```

### Identifiers
Identifiers may contain letters, digits, hyphens, and underscores. They must start with a letter.
```meld
my-variable
parse-int
Vec2
```

### Literals
```meld
42              // int
3.14            // float
"hello"         // string
`template ${x}` // template string
true / false    // bool
nil             // null value
[1, 2, 3]      // list
```

### Operators
```
+  -  *  /  %          // arithmetic
== != < > <= >=        // comparison
=                      // assignment
.                      // field access / method call
```

## Declarations

### Value Bindings
```meld
val x = 42          // immutable
var y = 0           // mutable
y = y + 1           // reassignment (var only)
```

### Functions
```meld
fnc name(param: Type, ...) -> ReturnType {
    rtn value
}
```

Functions are first-class values. The return keyword is `rtn`.

### Lambdas
```meld
// Inline lambda with parameters
val f = fnc(x: int) -> int { rtn x * 2 }

// Block lambda (no parameters)
val action = { println("hello") }
```

### Structs
```meld
struct Point {
    val x: float
    val y: float
}

// Creation
val p = Point { x = 1.0, y = 2.0 }

// Field access
println(p.x)

// Mutation (var binding only)
var q = Point { x = 0.0, y = 0.0 }
q.x = 5.0
```

### Enums
```meld
enum Color { Red, Green, Blue }

enum Shape {
    Circle(val radius: float),
    Rectangle(val width: float, val height: float),
    Point
}
```

### Type Aliases
```meld
type UserId -> int
type Name -> string
type PositiveInt -> int { it > 0 }
```

## Control Flow

All control flow is library-based. No `if`, `else`, `while`, `for`, `match` keywords.

### Conditional Expression
```meld
when(condition).then({ rtn value_if_true }).else({ rtn value_if_false })
```

### Chained Conditions
```meld
when(x > 100).then({ rtn "large" })
    .when(x > 10).then({ rtn "medium" })
    .else({ rtn "small" })
```

### Boolean Dispatch
```meld
when(condition).ifTrue({ /* side effect */ })
when(condition).ifFalse({ /* side effect */ })
```

### Pattern Matching
```meld
match(value, [
    [pattern1, { rtn result1 }],
    [pattern2, { rtn result2 }],
    [pattern3, { rtn result3 }]
])
```

### Iteration
```meld
forEach(list, fnc(item: any) -> () { /* body */ })
```

## Method Dispatch

A function whose first parameter is type `T` can be called as `instance.method(args)`:

```meld
fnc length(v: Vec2) -> float { rtn (v.x * v.x + v.y * v.y) }

// Equivalent calls:
length(my_vec)
my_vec.length()
```

### Multiple Dispatch

Define the same function name for different types:
```meld
fnc describe(c: Circle) -> string { rtn "circle" }
fnc describe(r: Rect) -> string { rtn "rectangle" }
```

The runtime dispatches based on the argument's actual type.

### Operator Overloading

Define these function names to overload operators:

| Operator | Function Name |
|----------|--------------|
| `+` | `add` |
| `-` | `sub` |
| `*` | `mul` |
| `/` | `div` |
| `==` | `eq` |

```meld
fnc add(a: Vec2, b: Vec2) -> Vec2 {
    rtn Vec2 { x = a.x + b.x, y = a.y + b.y }
}
// Now: Vec2{x=1,y=2} + Vec2{x=3,y=4} works
```

## Modules

```meld
imp module_name
```

Imports all exported functions from `module_name.meld` in the same directory.

## Error Handling

### Result Type
```meld
Ok(value)           // Success
Err(message)        // Failure
unwrap(result)      // Extract value (panics on Err)
result.is-ok        // Check success
result.error        // Get error message
```

### Option Type
```meld
Some(value)         // Present
None                // Absent
unwrap(option)      // Extract value (panics on None)
```

## Template Strings

```meld
`Hello ${name}, result is ${2 + 2}`
```

Backtick-delimited strings with `${expression}` interpolation.

## Effects

Effects make side effects explicit in function signatures:

```meld
// Direct effect call
Console.println("message")

// Effect-aware function
fnc log(msg: string) -> () {
    Console.println("[LOG] " + msg)
}
```

## Kernel Primitives

The C++ primitives that form Meld's foundation:

| Primitive | Purpose |
|-----------|---------|
| `println` | Print with newline |
| `print` | Print without newline |
| `len` | Length of string or list |
| `arr-push` | Push to array (internal) |
| `str-find` | Find substring |
| `substr` | Extract substring |
| `replace` | Replace all occurrences in string |
| `split` | Split string by delimiter |
| `to-string` | Convert to string |
| `int-to-str` | Int to string |
| `float-to-str` | Float to string |
| `type-of` | Runtime type name |
| `panic` | Abort with message |
| `when` | Create conditional (returns chainable When value) |
| `then` | Resolve conditional branch |
| `else` | Resolve fallback branch |
| `ifTrue` / `ifFalse` | Side-effect conditional dispatch |
| `forEach` | Iterate over list |
| `map` | Transform list |
| `filter` | Select from list |
| `reduce` | Fold list |
| `match` | Pattern matching |

Note: `when`, `then`, `else`, `forEach`, `map`, `filter`, `reduce`, and `match` are implemented as native builtins for performance and to bootstrap the language. The long-term design makes these expressible as library forms once the method dispatch protocol is generalized.

Everything else (`Ok`, `Err`, `Some`, `None`, `push`, `assert`, `contains`, `starts-with`, `ends-with`, `unwrap`) is pure Meld library code loaded from the prelude.

## Reserved Words

```
fnc  val  var  rtn  struct  class  enum  imp  type
```

These are the only reserved words in Meld. Notably absent: `if`, `else`, `while`, `for`, `return`, `match`, `import`, `fn`, `let`, `const`.
