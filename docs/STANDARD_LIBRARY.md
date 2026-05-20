# Standard Library Reference

All functions available in Meld programs. The standard library is pure Meld code loaded from the prelude — no special runtime support beyond the 11 kernel primitives.

## Output

```meld
println(value)          // Print value + newline
print(value)            // Print value (no newline)
```

## Collections

```meld
len(collection)         // Length of string or list
push(list, item)        // Return new list with item appended
forEach(list, fn)       // Call fn(item) for each element
map(list, fn)           // Return new list with fn(item) applied
filter(list, fn)        // Return items where fn(item) is true
reduce(list, init, fn)  // Fold: fn(accumulator, item) -> new_acc
```

### Examples
```meld
val nums = [1, 2, 3, 4, 5]
println(len(nums))                                          // 5
println(push(nums, 6))                                      // [1,2,3,4,5,6]
forEach(nums, fnc(n: any) -> () { print(n + " ") })        // 1 2 3 4 5
println(map(nums, fnc(n: any) -> any { rtn n * 2 }))       // [2,4,6,8,10]
println(filter(nums, fnc(n: any) -> bool { rtn n > 3 }))   // [4,5]
println(reduce(nums, 0, fnc(a: any, b: any) -> any { rtn a + b }))  // 15
```

## Strings

```meld
len(s)                      // String length
contains(s, substr)         // true if s contains substr
starts-with(s, prefix)      // true if s starts with prefix
ends-with(s, suffix)        // true if s ends with suffix
replace(s, old, new)        // Replace all occurrences
split(s, delimiter)         // Split into list of strings
substr(s, start, length)    // Extract substring
```

### Examples
```meld
val s = "hello world"
println(len(s))                     // 11
println(contains(s, "world"))       // true
println(starts-with(s, "hello"))    // true
println(ends-with(s, "world"))      // true
println(replace(s, "world", "meld"))// "hello meld"
println(split(s, " "))             // ["hello", "world"]
```

## Control Flow

```meld
when(cond).then({ ... })                    // Conditional (returns value)
when(cond).then({ ... }).else({ ... })      // If-else expression
when(cond).ifTrue({ ... })                  // Side-effect on true
when(cond).ifFalse({ ... })                 // Side-effect on false
match(value, [[pat, { rtn result }], ...])  // Pattern matching
```

### Chained Conditions
```meld
when(x > 100).then({ rtn "large" })
    .when(x > 10).then({ rtn "medium" })
    .else({ rtn "small" })
```

## Error Handling

### Result
```meld
Ok(value)               // Create success result
Err(message)            // Create error result
unwrap(result)          // Extract value (panics on Err)
result.is-ok            // Boolean: is this Ok?
result.error            // String: error message (on Err)
```

### Option
```meld
Some(value)             // Create present option
None                    // Absent value
unwrap(option)          // Extract value (panics on None)
```

## Type Introspection

```meld
type-of(value)          // Returns type name as string: "int", "string", etc.
```

## Assertions

```meld
assert(condition, message)  // Panic if condition is false
panic(message)              // Abort execution with message
```

## Effects

```meld
Console.println(msg)    // Print via effect system
Console.print(msg)      // Print without newline via effect system
```

## Prelude Definitions

The prelude auto-loads before all user code and defines:

```meld
val true = ...          // Boolean true
val false = ...         // Boolean false
val nil = ...           // Null value
```

## Standard Library Modules (std/)

These modules are available for import in full Meld programs:

| Module | Purpose |
|--------|---------|
| `std/kernel.meld` | Low-level primitives (suspend, resume, call, eval) |
| `std/result.meld` | Result type implementation |
| `std/option.meld` | Option type implementation |
| `std/collections.meld` | Advanced collection types |
| `std/error.meld` | Error types and handling |
| `std/console.meld` | Console I/O effect |
| `std/fs.meld` | Filesystem operations |
| `std/net.meld` | Network operations |
| `std/time.meld` | Time and duration |
| `std/mem.meld` | Memory management |
| `std/crypto.meld` | Cryptographic operations |
| `std/random.meld` | Random number generation |
| `std/serde.meld` | Serialization/deserialization |
| `std/cache.meld` | Caching utilities |

## Kernel Primitives (C++ Bridge)

These functions are implemented as native C++ builtins in the interpreter:

```
// I/O
println(value)          // Print + newline
print(value)            // Print (no newline)

// Strings
len(collection)         // Length of string or list
str-find(s, sub)        // Find substring index
substr(s, start, len)   // Extract substring
replace(s, old, new)    // Replace all occurrences (iterative)
split(s, delim)         // Split to list (iterative)
to-string(value)        // Convert to string
int-to-str(n)           // Int -> string
float-to-str(f)         // Float -> string

// Reflection
type-of(value)          // Type name as string

// Control flow (native for bootstrap)
when(cond)              // Create When value
then(when, body)        // Resolve true branch
else(when_then, body)   // Resolve false branch
ifTrue(when, body)      // Side-effect on true
ifFalse(when, body)     // Side-effect on false
forEach(list, fn)       // Iterate
map(list, fn)           // Transform
filter(list, fn)        // Select
reduce(list, init, fn)  // Fold
match(val, cases)       // Pattern match

// Error
panic(msg)              // Abort
```

Note: `when`/`then`/`else` and the collection functions are native builtins that bootstrap the language. They are dispatched through the normal method call path. The long-term design makes these expressible as pure Meld library forms.

All other standard library functions (`Ok`, `Err`, `Some`, `None`, `push`, `assert`, `contains`, `starts-with`, `ends-with`, `unwrap`) are implemented in pure Meld on top of these primitives.
