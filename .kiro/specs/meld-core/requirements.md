# Meld Language Requirements (v2.0)

**Based on:** CONSOLIDATED_v2.1.md  
**Status:** Active Development  
**Date:** 2024

---

## Introduction

Meld is **the first programming language designed explicitly for AI-Augmented Development**. It combines:

- **Minimal homoiconic kernel** where code is data
- **AI-native features** for structural search, semantic compression, and contract-based generation
- **Abstract Syntax Graph (ASG)** for explicit code relationships
- **Algebraic Effects System** for safe sandboxing of AI-generated code
- **Property-Based Testing** with forall macro quantification
- **Binary Context Format (MELD-B)** for efficient AI context loading
- **Visual Logic** with flow macro for state machines
- **Code Provenance** tracking for trust and authorship
- **Polyglot architecture** for universal deployment
- **Safety by default** with no exceptions, null safety, and structured concurrency
- **Powerful metaprogramming** through compile-time decorators and macros

### What's New in v2.0

Building on v1.8, Meld v2.0 introduces comprehensive AI-safety and debugging features:

1. **Algebraic Effects (Requirement 41)**: Replaces dependency injection with a native sandbox system, allowing AI-generated code to run safely by intercepting side effects (network, file_system, etc.) via handlers.

2. **Property-Based Testing (Requirement 42)**: Introduces forall macro in tests, forcing AI agents to prove invariants rather than just writing example tests.

3. **Visual Logic (Requirement 43)**: Adds the flow macro for defining state machines that render visually in IDEs, replacing nested if/else spaghetti.

4. **Flight Recorder (Requirement 44)**: A runtime capability to snapshot crash states (inputs + effect history) so agents can deterministically replay and fix bugs.

5. **Code Provenance (Requirement 45)**: Tracks authorship and trust levels of every AST node, distinguishing verified human logic from unverified agent suggestions.

6. **Shadow Provenance (Requirement 46)**: Stores AI conversation history separately from source code in .meld/history, keeping codebases clean while preserving full generation context.

Requirements 99–118 were merged from the former `effects-annotations` spec, which transforms Meld's algebraic effects system from keyword-based syntax to annotation-based syntax (`@effect`, `@uses`, library `perform()`/`handle()`/`resume()`), adds the `query_required_effects` semantic API and `EffectRequirement` resource resolution, and eliminates the `perform { ... }` wrapper in favor of implicit effect calls.

Requirements 119–129 were merged from the former `own-link-memory-model` spec (updated to Hold/View tenancy terminology), which formalizes the language-level `Hold[T]` and `View[T]` library types in `std.mem`, the `Storable` trait system, compiler diagnostics for memory safety, the `std.mem.move()` ownership transfer intrinsic, collection iteration behavior, effect handler ARC interaction, and C++20 concept backend enforcement.

Requirements 130–139 were merged from the former `meldobject-non-atomic-arc` spec, which introduces the `MeldObject` unified base type with embedded non-atomic reference counting, the `MeldRef<T>` intrusive smart pointer, kernel type migration, Value type replacement, metadata migration, cycle detection via weak references, and continuation/control flow migration.

Requirements 144–155 were merged from the former `void-return-semantics` spec, which formalizes the empty tuple `()` as Meld's unit type (the zero-element case of the tuple continuum), enforces mandatory explicit return type annotations on all functions and effect trait methods, distinguishes `()` from `nil`, and defines the kernel/runtime representation of the empty tuple value.

Requirements 156–164 add lifecycle methods (`@intrinsic(lifecycle_destructor)` and `@intrinsic(lifecycle_constructor)`) discovered via `meta_get`/`meta_set` rather than hardcoded naming, ergonomic `@constructor`/`@destructor` alias macros, the formal ARC model semantics, cross-Actor transfer via the `Send` trait, the LLVM ARC injection pipeline, polyglot backend ARC strategy, ARC performance characteristics, and a lifecycle demo example. These formalize content previously documented only in MEMORY_MANAGEMENT.md and introduce the annotation-based lifecycle discovery mechanism.

---

## Glossary

- **Meld Kernel**: The minimal, non-metaprogrammable core consisting of exactly 20 primitives organized into 7 categories
- **cell**: A pair (head, tail) primitive that builds the AST, linked lists, and S-expressions
- **vec**: A contiguous memory block primitive that backs arrays, strings, and buffers
- **symbol**: An interned unique identifier primitive (e.g., :id) representing variable names, keys, and AST nodes
- **type**: A library-level concept for the type system built on meta_set/meta_get primitives, enabling run-time type checking and multiple dispatch (not a kernel primitive)
- **scope**: A dictionary primitive binding symbols to values, representing environments, modules, and closures
- **primitive_suspend**: The single kernel primitive for capturing execution continuations, enabling algebraic effects, exceptions, async/await, and generators
- **mark_stack**: Library function (built on primitive_suspend) that places a delimiter on the call stack for effect handlers
- **suspend**: Library function (built on primitive_suspend) that pauses execution and captures continuation
- **resume**: Library function (built on primitive_suspend) that re-attaches continuation and continues execution
- **MMS (Meta-Macro System)**: The compile-time macro system that operates on the AST and serves as the primary language extension mechanism
- **AST (Abstract Syntax Tree)**: The first-class, homoiconic data structure representing program structure, built from cell primitives
- **ASG (Abstract Syntax Graph)**: Enhanced AST where edges represent data flow, control flow, and scoping relationships, represented as first-class Meld data built from kernel primitives
- **Homoiconic**: Property where code and data share the same representation, enabling powerful metaprogramming
- **Structural Search**: AST-based code search and transformation capabilities for AI agents
- **Holographic View**: Semantic compression technique that strips function bodies while preserving signatures and contracts
- **CAP (Compiler-Agent Protocol)**: Structured JSON protocol for AI agents to interact with the compiler
- **Refinement Type**: A type with logical predicates that constrain valid values
- **Design by Contract (DbC)**: Formal specification using require/ensure macros
- **Multiple Dispatch**: Runtime function resolution based on the types of all arguments
- **Structured Concurrency**: Concurrency model where task lifetimes are scoped and cancellation propagates
- **Result Type**: The primary error handling mechanism, representing either success or failure
- **MCP (Model Context Protocol)**: Standard protocol for exposing code as tools for AI agents
- **Compile-Time Decorator**: A macro that triggers code generation during compilation to reduce boilerplate
- **Class-Level Macro**: A decorator applied to a class that receives the ClassNode and iterates over child fields (top-down injection)
- **Field-Level Macro**: A decorator applied to a single field that receives only the FieldNode and uses `.parent()` to navigate up to the enclosing class (bottom-up injection)
- **ast.quote**: Quasiquoting mechanism for generating AST fragments from template code in macros
- **ast.abort**: Structured error reporting API for macro expansion failures
- **Parent Pointer**: A weak reference from each AST node to its enclosing parent node, enabling bottom-up traversal
- **Namespace**: A named scope for organizing code and preventing name conflicts
- **Partial Application**: The process of fixing some arguments of a function, producing a new function with fewer parameters
- **Currying**: Transforming a function with multiple arguments into a sequence of functions each taking a single argument
- **Algebraic Effect**: An abstract side effect interface that can be performed and handled separately from implementation
- **Effect Handler**: A construct that intercepts and implements effect operations, enabling sandboxing and testing
- **Delimited Continuation**: A captured execution context that can be resumed later, enabling algebraic effects
- **primitive_suspend**: The single kernel primitive that captures continuations and enables the entire effects system
- **Task[T]**: Library type representing an asynchronous computation that will eventually produce a value of type T
- **Completer[T]**: Handle for manually completing a deferred task
- **Executor**: Thread pool or execution context for running async operations
- **Deferred Task**: Task created without immediate execution, completed manually later
- **MELD-B**: Binary Context Format (.mldb) for compact code representation with 10x+ compression
- **Property-Based Testing**: Testing approach using forall macro quantification to prove invariants across all inputs
- **Flow Macro**: Standard Library macro providing state machine syntax that renders visually in IDEs
- **Flight Recorder**: Runtime system for capturing crash snapshots with inputs and effect history
- **Code Provenance**: Metadata tracking authorship and trust level of every AST node
- **Shadow Provenance**: Separate storage of AI conversation history in .meld/history directory
- **Origin Metadata**: Classification of code as Human, Agent, or Verified for trust tracking
- **Annotation**: A compile-time decorator that provides metadata or triggers code generation without introducing new keywords
- **Effect Declaration**: The definition of an abstract effect interface with its operations via `@effect`
- **Effect Performance**: The act of invoking an effect operation within a function
- **Effect Handling**: The interception and implementation of effect operations via handlers
- **@uses Annotation**: The declaration of which effects a function performs (formerly `imposes` clause)
- **query_required_effects**: A semantic analysis API that performs transitive call-graph traversal from an entry point to compute the complete set of effects required by that entry point
- **EffectRequirement**: A structured result type containing the resolved set of required capabilities, allowed network domains, and allowed filesystem paths for a given entry point
- **Resource_Resolution**: The process of combining static effects inferred from code with dynamic constraints declared in `meld.toml` to produce a complete `EffectRequirement`
- **Effect_Leak**: A compile-time error that occurs when a non-intrinsic function calls an effectful function without declaring the corresponding effect in its own `@uses` annotation
- **Intrinsic_Effect_Map**: The compiler's built-in mapping from intrinsic functions (e.g., `io.print`, `file.open`, `net.connect`) to `core::Effect` enum values
- **Implicit_Effect_Call**: An Effect_Call written without a `perform { ... }` wrapper, resolved by the compiler at compile time
- **Explicit_Effect_Call**: An Effect_Call wrapped in `perform { ... }` block syntax (the legacy approach, deprecated in favor of implicit calls)
- **Effect_Definition**: A trait annotated with `@effect` that declares named operations with typed signatures
- **Effect_Call**: An expression of the form `EffectName.operation(args)` where `EffectName` matches a registered Effect_Definition
- **LSP_Server**: The Meld Language Server Protocol implementation that provides IDE features like diagnostics and ghost text
- **Hold[T]**: A generic library type in `std.mem` representing a strong (owning) reference to a heap-allocated object of type `T`. Backed by `MeldRef<T>` in the C++ backend. Formerly `Own[T]`.
- **View[T]**: A generic library type in `std.mem` representing a non-owning (observation) reference to a heap-allocated object of type `T`. Backed by `WeakRef<T>` in the C++ backend. Formerly `Link[T]`.
- **Tenancy**: The relationship between a reference and the object it points to. `Hold[T]` is an active tenancy (keeps the object alive); `View[T]` is a passive tenancy (observes without extending lifetime).
- **Creator_Rule**: Type inference rule: when a variable is initialized with a constructor call, the compiler infers `Hold[T]`.
- **Guest_Rule**: Type inference rule: function parameters default to `View[T]` unless explicitly annotated as `Hold[T]`.
- **Generic_Mutability_Qualifier**: The `val` or `var` keyword placed before a type parameter in any generic type (e.g., `Hold[val T]`, `List[var T]`), controlling whether the caller may invoke mutating methods on the contained value through that generic wrapper.
- **Storable**: A trait annotated with `@intrinsic(memory_strategy)` that the compiler uses to enforce that only properly managed reference types can be stored in managed containers.
- **Managed_Container**: A collection type annotated with `@intrinsic(managed_container)` whose element type parameter is constrained to types implementing the Storable trait.
- **Upgrade**: The operation of converting a `View[T]` to a temporary strong reference via `if val` or `match`, which increments `strong_count` for the duration of the binding scope.
- **Move**: An ownership transfer operation via `std.mem.move()` that transfers a reference without touching the reference count. The source binding is statically invalidated by the compiler.
- **MeldObject**: The C++ base class for all Meld kernel types, containing an embedded non-atomic 64-bit reference counter, a type tag, and optional metadata storage.
- **MeldRef**: An intrusive smart pointer template that manages the lifetime of MeldObject-derived types by incrementing and decrementing the embedded non-atomic reference counter.
- **Non_Atomic_Reference_Counter**: A 64-bit integer embedded in the MeldObject header that tracks the number of live references to the object without atomic CPU instructions.
- **Type_Tag**: An enumeration embedded in the MeldObject header that identifies the runtime type of the object (e.g., Integer, String, Symbol, Cell, Vec).
- **Intrusive_Reference_Counting**: A reference counting strategy where the counter is embedded in the object itself, rather than in a separate control block.
- **Weak_Reference**: A non-owning reference to a MeldObject that does not increment the reference counter, used for breaking reference cycles.
- **Kernel_Type**: Any of the 14 concrete types in the Meld kernel Value variant: Symbol, Cell, Vec, Nil, Function, Integer, Float, Boolean, String, Placeholder, Optional, Continuation, NativeHandle, NativeFunction.
- **MetadataStore**: Currently a global singleton that maps object pointer addresses to metadata. To be replaced by per-object metadata embedded in MeldObject.
- **Empty_Tuple**: The built-in type written as `()` with exactly one inhabiting value (also written `()`). Represents successful completion without a meaningful result. The zero-element case of Meld's tuple type system. Analogous to Rust's `()`.
- **Empty_Tuple_Value**: The single canonical value of the Empty_Tuple type, written as `()` in expression position.
- **Tuple_Return_Continuum**: The unified model where all function return types are tuples: `()` (zero elements, no value), `(T)` (one element, sugar for `T`), `(T, U)` (two elements), etc.
- **Lifecycle_Method**: A method annotated with `@intrinsic(lifecycle_constructor)` or `@intrinsic(lifecycle_destructor)` that the compiler discovers via `meta_get` and invokes at object creation or destruction.
- **@intrinsic(lifecycle_constructor)**: An annotation on a method that marks it as the constructor to be called after object allocation and field initialization.
- **@intrinsic(lifecycle_destructor)**: An annotation on a method that marks it as the destructor to be called when the last `Hold[T]` reference is released (strong_count reaches zero), before deallocation.
- **@constructor**: Ergonomic decorator macro alias for `@intrinsic(lifecycle_constructor)`, exported by `std.mem`. Expands to `meta_set(node, :intrinsic, :lifecycle_constructor)`.
- **@destructor**: Ergonomic decorator macro alias for `@intrinsic(lifecycle_destructor)`, exported by `std.mem`. Expands to `meta_set(node, :intrinsic, :lifecycle_destructor)`.
- **Deallocation_Sequence**: The ordered steps when an object's strong_count reaches zero: (1) user-defined destructor runs, (2) field destructors run in reverse declaration order, (3) if weak_count is zero the object is freed immediately, (4) if weak_count is positive the object header is retained as a tombstone until weak_count also reaches zero.
- **Tombstone**: An object header that remains allocated after the object's payload has been destroyed (strong_count == 0, weak_count > 0), allowing `View[T]` references to safely detect that the object is dead.
- **ARC_Injection_Pass**: A compiler pass that performs lexical scope analysis to insert `intrinsic_retain` at reference creation and `intrinsic_release` at scope exit, handling control flow (phi nodes, branches, loops).
- **ARC_Optimization_Pass**: A compiler pass that eliminates redundant retain/release pairs, sinks retains, hoists releases, and applies copy-on-write deferral.
- **Send**: A trait that types must implement to be transferred across Actor boundaries via deep copy. Types with non-serializable resources (native handles, closures with captured state) do not implement `Send`.
- **Object_Header**: The contiguous memory layout at the start of every heap-allocated MeldObject, containing the vtable pointer, strong reference counter, weak reference counter, TypeTag, and metadata pointer.

---

## Requirements

### Requirement 1: Minimal Kernel Architecture

**User Story:** As a language implementer, I want a minimal non-metaprogrammable core consisting of exactly 20 primitives, so that the language remains simple to reason about and extend through metaprogramming.

#### Acceptance Criteria

1. THE Meld Kernel SHALL include exactly 20 primitives organized into 7 categories: Data Structure (cell, vec, symbol, scope), Scalar (int, float, bool, nil), Execution (lambda, apply, eval, quote), Control Flow (primitive_suspend), Memory & Binding (def, set, lookup), AI & Metadata (meta_set, meta_get), and Interop (native_call, native_load)
2. THE Meld Kernel SHALL represent the AST as a first-class data structure built from cell primitives
3. THE Meld Kernel SHALL exclude class, struct, enum, trait, and all control flow keywords as kernel primitives
4. THE Language Runtime SHALL make the AST accessible to macros at compile-time
5. THE Meld Kernel SHALL support homoiconicity where code and data share the same representation
6. THE Meld Kernel SHALL use int (64-bit signed) as the single integer primitive, with unsigned types implemented as library wrappers
7. THE Meld Kernel SHALL use float (64-bit IEEE) as the single floating-point primitive
8. THE Meld Kernel SHALL provide primitive_suspend as the single control flow primitive for implementing algebraic effects, exceptions, async/await, and generators

### Requirement 2: Meta-Macro System Foundation

**User Story:** As a language designer, I want a powerful meta-macro system, so that I can bootstrap complex language features from the minimal kernel.

#### Acceptance Criteria

1. THE MMS SHALL define type as the root of all types including itself, implemented as a library concept built on meta_set/meta_get primitives
2. THE MMS SHALL treat all types as first-class values that are instances of type
3. THE MMS SHALL provide a hygienic, compile-time macro system that operates on the AST
4. THE MMS SHALL enable bootstrapping of new type kinds through library macros
5. THE MMS SHALL make macros the only mechanism for introducing new syntax into the language
6. THE MMS SHALL implement class, struct, enum, and trait as library macros, not keywords
7. THE Compiler SHALL maintain a `.parent()` pointer on every AST node, enabling bottom-up traversal from any node to its enclosing parent node
8. THE `.parent()` pointer SHALL be implemented as a weak reference to prevent ARC reference cycles between parent and child AST nodes during compilation
9. THE MMS SHALL provide `ast.quote { ... }` quasiquoting mechanism for generating AST fragments from template code, enabling macro authors to write code templates instead of manual AST node construction
10. THE MMS SHALL provide `ast.abort(message)` API for structured error reporting when macro expansion fails (e.g., when a required parent pointer is nil or a node kind is unexpected)
11. THE MMS SHALL support the `node.parent() ?: ast.abort(...)` pattern for safe parent access in field-level macros that need to inject generated code into an enclosing class

### Requirement 3: User-Facing Syntax and Declarations

**User Story:** As a developer, I want clean, intuitive syntax for declarations and literals, so that I can write readable code quickly.

#### Acceptance Criteria

1. THE Language Runtime SHALL support val for immutable declarations with type inference
2. THE Language Runtime SHALL support var for mutable declarations with type inference
3. THE Language Runtime SHALL support multi-line string templates using backtick syntax (`)
4. THE Language Runtime SHALL support expression interpolation in strings using ${expression} syntax
5. THE Standard Library SHALL provide Regex class for regular expression pattern matching
6. THE Language Runtime SHALL support regex literals with /pattern/flags syntax

### Requirement 4: Kebab-Case Identifiers

**User Story:** As a developer, I want to use kebab-case (hyphenated) identifiers, so that I can write more readable multi-word variable and function names.

#### Acceptance Criteria

1. THE Language Runtime SHALL support hyphens (-) within identifiers for variables, functions, classes, and other named entities
2. THE Language Runtime SHALL NOT allow identifiers to start with a hyphen
3. THE Language Runtime SHALL NOT allow identifiers to end with a hyphen
4. THE Language Runtime SHALL distinguish between the subtraction operator (-) and hyphens in identifiers based on context
5. THE Language Runtime SHALL support mixing underscores and hyphens within the same identifier

### Requirement 5: Function Declaration Syntax

**User Story:** As a developer, I want clear and consistent function declaration syntax, so that I can define functions with typed parameters and return values, including default values for both inputs and outputs.

#### Acceptance Criteria

1. THE Language Runtime SHALL use `fnc` keyword for function declarations
2. THE Language Runtime SHALL support typed parameters with syntax: `parameter-name: parameter-type`
3. THE Language Runtime SHALL support default input parameter values with syntax: `parameter-name: parameter-type = default-value`
4. THE Language Runtime SHALL require an explicit return type annotation on every function declaration with syntax: `-> return-type` (use `-> ()` for functions that return no value); omitting the return type annotation SHALL be a compile-time error (see Requirement 146)
5. THE Language Runtime SHALL support named return values with syntax: `-> (output-name: output-type)`
6. THE Language Runtime SHALL support default output values with syntax: `output-name: output-type = default-value`
7. THE Language Runtime SHALL support multiple return values as tuples, following the tuple return continuum: `()` (zero elements, no value), `(T)` (one element, equivalent to `T`), `(T, U)` (two elements), etc. (see Requirement 144)
8. WHEN a function with default output values returns early without setting all outputs, THE Language Runtime SHALL use the default values for unset outputs
9. WHEN calling a function with default input parameters, THE Language Runtime SHALL allow omitting those parameters
10. THE Language Runtime SHALL use `rtn` keyword for explicit return statements; bare `rtn` (with no expression) SHALL be desugared by the parser into `rtn ()` (see Requirement 147)
11. THE Language Runtime SHALL NOT support `return` as a keyword
12. THE Language Runtime SHALL support ellipsis syntax (`...`) for rest parameters (variadic arguments) and spread/unpack operations on vectors; `...` is built-in syntax, not an overloadable operator (see Requirement 24C.18)

### Requirement 6: Variable Declaration Syntax and Immutability

**User Story:** As a developer, I want clear and consistent variable declaration syntax with optional type annotations and immutability by default, so that I can write safer, more predictable code.

#### Acceptance Criteria

1. THE Language Runtime SHALL require `val` or `var` keyword for all variable declarations
2. THE Language Runtime SHALL support `val` keyword for immutable variable declarations
3. THE Language Runtime SHALL support `var` keyword for mutable variable declarations
4. THE Language Runtime SHALL support type inference when type annotation is omitted
5. THE Language Runtime SHALL support explicit type annotation syntax: `val name: type = value`
6. THE Language Runtime SHALL support uninitialized variables with explicit types: `val name: type`
7. THE Language Runtime SHALL make val (immutable) the default declaration keyword
8. THE Language Runtime SHALL require explicit var keyword for mutable declarations
9. THE Language Runtime SHALL make all data structures immutable by default
10. THE Language Runtime SHALL provide mutable variants with explicit Mutable prefix
11. THE Language Runtime SHALL enforce immutability at compile-time

### Requirement 7: AI-Native Features

**User Story:** As an AI-augmented developer, I want native language support for AI collaboration, so that I can work seamlessly with AI agents in code generation, analysis, and maintenance.

#### Acceptance Criteria

1. THE Language Runtime SHALL provide a Structural Search API for AST-based code search and transformation
2. THE Language Runtime SHALL support Holographic View generation for semantic compression
3. THE Standard Library SHALL implement Design by Contract with require and ensure macros
4. THE Compiler SHALL implement Compiler-Agent Protocol (CAP) for structured AI interaction
5. THE Standard Library SHALL provide inline micro-tests co-located with functions via the test macro
6. THE MMS SHALL provide @blueprint macro for semantic documentation with vector embeddings
7. THE Compiler SHALL support auto-generation of Model Context Protocol (MCP) schemas

### Requirement 8: Structural Search API

**User Story:** As a developer using AI tools, I want native AST-based search capabilities, so that AI agents can precisely locate and modify code patterns.

#### Acceptance Criteria

1. THE Language Runtime SHALL provide Code.parse() for converting source to searchable AST
2. THE Language Runtime SHALL support pattern matching with variable capture using ast`` literals
3. THE Language Runtime SHALL provide findAll() method for pattern-based search
4. THE Language Runtime SHALL support replace() method for AST-based transformations
5. THE Language Runtime SHALL enable semantic search beyond syntactic patterns

### Requirement 9: Holographic View

**User Story:** As an AI agent, I want semantic compression of code, so that I can understand large codebases within token limits.

#### Acceptance Criteria

1. THE Language Runtime SHALL provide toHologram() method on modules and classes
2. THE Holographic View SHALL preserve function signatures while removing bodies
3. THE Holographic View SHALL preserve type definitions and contracts
4. THE Holographic View SHALL maintain import/export relationships
5. THE Holographic View SHALL achieve ~95% token reduction while preserving semantic meaning

### Requirement 10: Design by Contract (DbC)

**User Story:** As a developer working with AI, I want formal contracts, so that AI-generated code includes proper preconditions and postconditions.

#### Acceptance Criteria

1. THE Standard Library SHALL provide require as a macro for defining preconditions on functions
2. THE Standard Library SHALL provide ensure as a macro for defining postconditions on functions
3. THE Standard Library SHALL provide old() function for referencing pre-state values in ensure blocks
4. THE MMS SHALL support contract inheritance in class hierarchies via macro expansion
5. THE MMS SHALL support compile-time contract verification where possible

### Requirement 11: Compiler-Agent Protocol (CAP)

**User Story:** As an AI agent, I want structured compiler interaction, so that I can understand errors and suggest fixes programmatically.

#### Acceptance Criteria

1. THE Compiler SHALL output structured JSON for all errors and warnings
2. THE Compiler SHALL include fix suggestions in error messages
3. THE Compiler SHALL provide confidence scores for suggested fixes
4. THE Compiler SHALL support batch compilation with structured results
5. THE Compiler SHALL enable incremental compilation with change tracking

### Requirement 12: Inline Micro-Tests

**User Story:** As a developer, I want tests co-located with code, so that AI agents can verify generated code immediately.

#### Acceptance Criteria

1. THE Standard Library SHALL provide test as a macro for defining inline micro-tests within function definitions
2. THE MMS SHALL execute micro-tests during compilation via macro expansion
3. THE MMS SHALL report micro-test failures as compilation errors
4. THE test macro SHALL support property-based testing via the forall macro
5. THE test macro SHALL support inheritance in class hierarchies

### Requirement 13: @blueprint Macro for Doc-Driven Generation

**User Story:** As an AI agent, I want unified documentation and prompting, so that I can understand code intent and generate appropriate implementations from a single source of truth.

#### Acceptance Criteria

1. THE MMS SHALL provide @blueprint macro for semantic documentation and AI prompting
2. THE @blueprint macro SHALL support summary field for high-level description
3. THE @blueprint macro SHALL support rules field as an array of generation constraints
4. THE @blueprint macro SHALL support examples field with input-output pairs
5. THE @blueprint macro SHALL generate vector embeddings for semantic search
6. THE @blueprint macro SHALL integrate with IDE tooling for enhanced autocomplete
7. THE @blueprint macro SHALL support inheritance and composition of blueprints
8. THE Compiler SHALL use @blueprint metadata for auto-MCP generation
9. THE @blueprint macro SHALL serve as the single source of truth for both documentation and AI generation
10. THE Language Runtime SHALL enable querying blueprints at runtime for dynamic code generation

### Requirement 14: Type System with Null Safety

**User Story:** As a developer, I want a rich type system with explicit null safety, so that I can prevent null-reference errors at compile-time.

#### Design Principle: Absence vs Failure Separation

Meld draws a hard architectural line between **"Absence of Value"** (`optional[T]` or `T | nil`) and **"Action Failure"** (`Result[T, E]`). These two concepts serve fundamentally different purposes and SHOULD NOT be nested (e.g., `Result[optional[T], E]` is discouraged). This separation ensures each control flow operator in the language targets exactly one concern, preventing the confusion found in languages that conflate missing data with operation failure.

#### Acceptance Criteria

##### 14A: Core Type System

1. THE Language Runtime SHALL distinguish between struct (value types with copy-by-value semantics) and class (reference types with managed memory)
2. THE Language Runtime SHALL implement explicit nullability where `optional[T]` (alias for `T | nil`) represents absence of value; `Type?` is syntactic sugar for `optional[Type]`
3. THE Language Runtime SHALL support Union (|) and Intersection (&) type operators
4. THE Language Runtime SHALL support generic types and functions with variance annotations (in/out)
5. THE Language Runtime SHALL support union and intersection types within generic type parameters
6. THE Language Runtime SHALL support type aliases using -> syntax: `type Name -> Type`

##### 14A-NIL: Nil Safety Guarantees

20. THE Semantic Analyzer SHALL treat every standard type (`string`, `int`, `User`, `vec[T]`, etc.) as strictly non-nullable by default
21. THE Semantic Analyzer SHALL emit a fatal compile-time error when `nil` is assigned, passed, or returned where a non-optional type is expected
22. THE type `optional[T]` (alias for `T | nil`) SHALL be structurally distinct from `T`; methods and properties belonging to `T` SHALL NOT be callable directly on a value of type `optional[T]`
23. THE Semantic Analyzer SHALL implement flow-sensitive type narrowing: when a value of type `optional[T]` is checked against `nil` in a conditional (e.g., `if (x != nil) { ... }`), the compiler SHALL narrow the type of that value to `T` within the scope where `nil` has been excluded
24. THE flow-sensitive type narrowing SHALL apply to all conditional forms that exclude `nil`, including Smalltalk-style control flow methods and library-based pattern matching
25. THE safe navigation (`?.`), safe indexing (`?[]`), safe invocation (`?()`), and elvis (`?:`) operators SHALL perform implicit flow-sensitive type narrowing and extraction as part of their semantics
26. THE Language SHALL guarantee that runtime Null Pointer Exceptions are mathematically impossible in pure Meld code, excluding explicit use of the `!!` (force unwrap / panic) operator

##### 14B: Safe Chaining Operators (Target: `optional[T]`)

7. THE Language Runtime SHALL provide safe-navigation operator (`?.`) for `optional[T]` types that short-circuits the current expression chain to `nil` without returning from the function
8. THE Language Runtime SHALL provide safe-indexing operator (`?[]`) for `optional[T]` types that short-circuits the current expression chain to `nil` if the receiver is `nil` (e.g., `users?[0]?.name`)
9. THE Language Runtime SHALL provide safe-invocation operator (`?()`) for `optional[T]` types that short-circuits the current expression chain to `nil` if the callable is `nil`
10. THE `?.`, `?[]`, and `?()` operators SHALL target `optional[T]` only and SHALL NOT be applicable to `Result[T, E]`
11. THE `?.`, `?[]`, and `?()` operators SHALL be reserved and non-overloadable

##### 14C: Optional Return Operator (Target: `optional[T]`)

12. THE Language Runtime SHALL provide the `?` (safe return) postfix operator for `optional[T]` types: if the value is `nil`, it immediately returns `nil` from the enclosing function
13. THE `?` operator SHALL target `optional[T]` only and SHALL NOT be applicable to `Result[T, E]`
14. THE `?` operator SHALL be reserved and non-overloadable

##### 14D: Elvis / Default Operator (Target: `optional[T]`)

15. THE Language Runtime SHALL provide elvis operator (`?:`) for `optional[T]` null-coalescing: if the left side is `nil`, evaluates and returns the right side
16. THE `?:` operator SHALL target `optional[T]` only and SHALL NOT be applicable to `Result[T, E]`
17. THE `?:` operator SHALL be reserved and non-overloadable

##### 14E: Force Unwrap / Panic Operator (Target: `optional[T]` AND `Result[T, E]`)

18. THE Language Runtime SHALL provide the `!!` (force unwrap) postfix operator for both `optional[T]` and `Result[T, E]`: asserts presence/success and panics if it encounters `nil` or `Err`, crashing the current Fiber/Actor
19. THE `!!` operator SHALL be reserved and non-overloadable

### Requirement 15: Refinement Types

**User Story:** As a developer, I want types with logical constraints, so that invalid values are prevented at compile-time.

#### Acceptance Criteria

1. THE Language Runtime SHALL support refinement type syntax: `type Name -> BaseType where { predicate }`
2. THE Language Runtime SHALL validate refinement predicates at compile-time where possible
3. THE Language Runtime SHALL generate runtime checks for dynamic refinement validation
4. THE Language Runtime SHALL support refinement type composition and inheritance
5. THE Language Runtime SHALL provide built-in refinement types for common patterns

### Requirement 16: Memory Management and Object Model

**User Story:** As a developer, I want automatic memory management with clear semantics, so that I can focus on business logic without manual memory handling.

#### Acceptance Criteria

1. THE Language Runtime SHALL implement managed memory for class instances using Automatic Reference Counting (ARC)
2. THE Language Runtime SHALL define the Copyable trait for types that support copying
3. THE Language Runtime SHALL provide standard .copy() method for deep copying of Copyable types
4. THE Language Runtime SHALL support class, struct, and trait definitions
5. THE Language Runtime SHALL enable traits to provide default implementations for composition
6. THE Language Runtime SHALL support weak references for breaking cycles
7. THE Language Runtime SHALL use non-atomic reference counting within single-threaded execution contexts (Fibers/Actors), justified by the shared-nothing concurrency model
8. THE Language Runtime SHALL provide a unified `MeldObject` base type with an embedded reference counter for all heap-allocated objects
9. THE Compiler SHALL implement LLVM-level ARC injection (`intrinsic_retain`/`intrinsic_release`) based on lexical scope analysis for optimal retain/release placement
10. THE Compiler SHALL strip ARC instructions (retain/release) when targeting garbage-collected backends (JVM, Go), deferring memory management to the host runtime's GC

### Requirement 17: Explicit Property Generation via @Property Macro

**User Story:** As a developer, I want explicit property accessors generated by macros, so that I get boilerplate reduction without hidden control flow or magic operator overloading.

#### Design Principle: No Hidden Control Flow

Meld explicitly rejects C#-style properties where `user.name = "Alice"` secretly invokes a setter function. In Meld, the `.` (member access) operator is strictly forbidden from being overloaded — it always means raw field access or method dispatch. If code executes logic, it MUST look like a function call with visible parentheses (e.g., `user.name()` and `user.set_name("Alice")`). This ensures read-time clarity: a developer or AI agent reviewing code knows instantly whether `user.age` is a raw memory read or `user.age()` is a function call.

#### Acceptance Criteria

1. THE MMS SHALL provide @Property as a composite field-level macro that generates explicit getter and setter methods for annotated fields
2. WHEN @Property is applied to a field `name: T`, THE @Property macro SHALL rename the backing field to `_name` (underscore prefix)
3. WHEN @Property is applied to a field, THE @Property macro SHALL change the backing field visibility to package-private (`@visibility(pkg)`)
4. WHEN @Property is applied to a field `name: T`, THE @Property macro SHALL generate a public getter method `fnc name() -> T` that returns `this._name`
5. WHEN @Property is applied to a mutable field `name: T`, THE @Property macro SHALL generate a public setter method `fnc set_name(v: T)` that assigns `this._name = v`
6. THE Language Runtime SHALL NOT support C#-style implicit get/set blocks on properties
7. THE Language Runtime SHALL NOT allow the `.` (member access) operator to be overloaded to invoke getter or setter logic
8. THE call site SHALL use explicit function call syntax with visible parentheses for property access (e.g., `user.name()` for getting, `user.set_name("Alice")` for setting)
9. THE Standard Library SHALL provide by as a macro for property delegation (e.g., val p by lazy { ... }), desugaring to delegate field generation and accessor wiring

### Requirement 18: Tree Initialization

**User Story:** As a developer, I want block-style named construction, so that I can initialize objects with clear, readable syntax.

#### Acceptance Criteria

1. THE Language Runtime SHALL support block-style initialization syntax for object construction
2. THE Language Runtime SHALL enable named parameter assignment within initialization blocks
3. WHEN constructing an object, THE Language Runtime SHALL evaluate the initialization block to set properties
4. THE Language Runtime SHALL support syntax like: `val p = Person { firstName = "Jane", lastName = "Doe" }`
5. THE Language Runtime SHALL support nested initialization blocks for complex object graphs

### Requirement 19: Functional Programming Core

**User Story:** As a developer, I want first-class functional programming support, so that I can write expressive, composable code.

#### Acceptance Criteria

1. THE Language Runtime SHALL support first-class functions with lambda syntax (-> or {...})
2. THE Standard Library SHALL provide @extension annotation for adding methods to existing types (e.g., @extension struct SomeType { ... }), desugaring to meta_set(node, "declaration_mode", "extension")
3. THE Language Runtime SHALL provide pipeline operator (|>) for function composition
4. THE Standard Library SHALL provide persistent immutable data structures
5. THE Language Runtime SHALL treat functions as values that can be passed and returned

### Requirement 20: Smalltalk-Style Control Flow

**User Story:** As a developer, I want control flow implemented as methods on objects, so that the language remains minimal and extensible.

#### Acceptance Criteria

1. THE Meld Kernel SHALL NOT include reserved keywords for control flow (if, else, for, while, switch, match, case, break, continue)
2. THE Standard Library SHALL implement Boolean.ifTrue:ifFalse: methods for conditional execution
3. THE Standard Library SHALL implement Int.times: method for counted loops
4. THE Standard Library SHALL implement Int.to:do: method for range iteration
5. THE Standard Library SHALL implement Block.whileTrue: method for conditional loops
6. THE Standard Library SHALL implement Collection.forEach: method for collection iteration
7. WHEN a control flow method receives a block, THE Language Runtime SHALL execute the block according to the method's semantics
8. THE Standard Library SHALL provide if/else as a macro that desugars to Boolean.ifTrue:ifFalse: method calls, enabling familiar conditional expression syntax without kernel keywords

### Requirement 21: Library-Based Pattern Matching

**User Story:** As a developer, I want powerful pattern matching implemented as library methods, so that I can handle complex data structures elegantly without special keywords.

#### Acceptance Criteria

1. THE Meld Kernel SHALL NOT include match or case as reserved keywords
2. THE Standard Library SHALL provide pattern matching through methods on values (e.g., value.match { ... })
3. THE Standard Library SHALL support deep pattern matching with destructuring through pattern builder methods
4. THE Standard Library SHALL support type matching through pattern builder methods
5. THE Standard Library SHALL support conditional guards through pattern builder methods
6. THE Standard Library SHALL provide compile-time exhaustiveness checking through macro analysis
7. WHEN a pattern match is non-exhaustive, THE MMS SHALL generate a compile-time warning or error

### Requirement 22: LINQ-Style Collection Processing

**User Story:** As a developer, I want fluent, chainable collection operations, so that I can process data declaratively and efficiently.

#### Acceptance Criteria

1. THE Standard Library SHALL provide fluent method-chaining API including map, filter, reduce, groupBy, join, and sorted
2. THE Language Runtime SHALL implement deferred (lazy) execution by default for intermediate collection operations
3. THE Standard Library SHALL provide .parallel modifier for parallel execution of collection operations
4. THE Standard Library SHALL provide query as a macro for SQL-like declarative queries that desugars to fluent collection API calls (from, where, join, group, select map to map, filter, groupBy, join, sorted)
5. WHEN chaining collection operations, THE Language Runtime SHALL defer execution until a terminal operation is invoked

### Requirement 23: Multiple Dispatch

**User Story:** As a developer, I want multiple dispatch for function resolution, so that I can write polymorphic code based on all argument types.

#### Acceptance Criteria

1. THE Language Runtime SHALL resolve function calls based on the runtime types of all arguments
2. THE Language Runtime SHALL treat single dispatch as a subset of multiple dispatch
3. THE Language Runtime SHALL define ambiguity resolution rules for dispatch conflicts
4. WHEN multiple function signatures match, THE Language Runtime SHALL select the most specific signature
5. IF no unambiguous match exists, THEN THE Language Runtime SHALL raise a compile-time or runtime error

### Requirement 24: Operator Overloading and Custom Operators

**User Story:** As a developer, I want to overload existing operators and define custom operators, so that I can create domain-specific abstractions.

#### Acceptance Criteria

##### 24A: Operator Definition Syntax

1. THE Language Runtime SHALL support operator overloading via `opr` keyword combined with behavioral annotations
2. THE Language Runtime SHALL support `@infix` annotation for defining infix operators with configurable precedence and associativity
3. THE Language Runtime SHALL support `@prefix` annotation for defining prefix (unary) operators
4. THE Language Runtime SHALL support `@postfix` annotation for defining postfix (unary) operators
5. THE Language Runtime SHALL enable configurable precedence for custom infix operators via precedence parameter
6. THE Language Runtime SHALL enable configurable associativity for custom infix operators via assoc parameter (left or right)
7. THE Language Runtime SHALL NOT support ternary (3-operand) operators
8. THE Language Runtime SHALL treat `opr` as a macro wrapping `fnc` for operator definitions
9. THE Compiler SHALL map operator definitions to mangled names (e.g., `opr +` maps to `__op_add__`)

##### 24B: Trait-Based Operator Contracts

10. THE Standard Library SHALL define standard operator traits in `std.core` (e.g., `Addable`, `Subtractable`, `Comparable`, `Indexable`) that specify the contract for each overloadable operator
11. Types SHALL implement the corresponding trait to overload a standard operator (e.g., implementing `Addable` enables `+`)
12. Custom operators (user-defined symbols) SHALL use standalone `opr` definitions without requiring a trait

##### 24C: Non-Overloadable Operators (Forbidden List)

The following tokens are strictly forbidden from being overloaded to protect the compiler's minimal kernel, guarantee predictable control flow, and preserve zero-cost syntactic sugar:

**Structural Tokens (Parser's Geometry):**

13. THE Language Runtime SHALL NOT allow overloading of `.` (member access)
14. THE Language Runtime SHALL NOT allow overloading of `{` and `}` (block delimiters)
15. THE Language Runtime SHALL NOT allow overloading of `[` and `]` in type position (reserved for monomorphized generics, e.g., `vec[int]`); `[]` in expression position IS overloadable for indexing/subscript via the `Indexable` trait
16. THE Language Runtime SHALL NOT allow overloading of `:` (type ascription / dictionary key-value)
17. THE Language Runtime SHALL NOT allow overloading of `;` (statement terminator)
18. THE Language Runtime SHALL NOT allow overloading of `...` (spread / variadic arguments — see Requirement 5.13)

**Control Flow Quintet (Reserved Operators):**

19. THE Language Runtime SHALL NOT allow overloading of `?.`, `?[]`, `?()` (safe chaining — see Requirement 14B)
20. THE Language Runtime SHALL NOT allow overloading of `?` (safe return — see Requirement 14C)
21. THE Language Runtime SHALL NOT allow overloading of `?:` (elvis / default — see Requirement 14D)
22. THE Language Runtime SHALL NOT allow overloading of `?!` (error propagation — see Requirement 28.14)
23. THE Language Runtime SHALL NOT allow overloading of `!!` (force unwrap / panic — see Requirement 14E)

#### Rationale for Reserved Operators

**Why reserve the Control Flow Quintet?** Reserving these operators prevents the "macro soup" and unreadable control flow often found in languages with unrestricted operator overloading. When `?.`, `?`, `?:`, `?!`, and `!!` have fixed, compiler-guaranteed semantics, developers can read any Meld code and immediately understand the control flow without checking whether a library has redefined these operators. This is especially critical for AI agents reading and generating code — predictable control flow operators eliminate an entire class of semantic ambiguity.

#### Rationale for No Ternary Operators

**Design Decision**: Meld explicitly excludes ternary operators from the language for the following reasons:

1. **If-else macro expressions (Requirement 30) provide equivalent functionality with better readability**
   - Meld's expression-oriented design means the `if`/`else` macro already returns values
   - Syntax `if (condition) trueValue else falseValue` is equally concise (provided by Standard Library macro)
   - Supports complex logic with blocks, not just single expressions

2. **Ternary operators add parsing complexity without significant benefit**
   - Requires special precedence rules and ambiguity resolution
   - Conflicts with existing syntax (`:` for type annotations, named parameters)
   - Conflicts with elvis operator (`?:`) for null coalescing
   - Creates inconsistency (why ternary but not quaternary?)

3. **Maintaining a simple operator model (unary and binary only) keeps the language consistent**
   - Aligns with Meld's "minimal kernel" philosophy
   - Reduces cognitive load for developers
   - Follows modern language design trends (Rust, Swift, Kotlin)

4. **Users can implement ternary-like behavior through extension methods if desired**
   - Library-first approach allows experimentation
   - Doesn't pollute the core language

**Alternative**: Use if-else macro expressions for conditional values:

```meld
// Simple conditional value
val status = if (age >= 18) "adult" else "minor"

// Complex conditional with blocks
val message = if (user != null) {
    val name = user.name
    `Hello, ${name}!`
} else {
    "Hello, Guest!"
}

// Chained conditions
val grade = if (score >= 90) "A"
            else if (score >= 80) "B"
            else if (score >= 70) "C"
            else "F"
```

**Note**: The `?:` operator in Meld is the elvis operator (null coalescing), not a ternary operator. See Requirement 14.3.

### Requirement 25: Compile-Time Decorators

**User Story:** As a developer, I want compile-time decorators to reduce boilerplate, so that I can write concise, maintainable code.

#### Acceptance Criteria

##### 25A: Class-Level Decorators (Top-Down Injection)

1. THE MMS SHALL provide @Getter and @Setter as class-level decorators that receive the ClassNode, iterate over child FieldNodes, and inject accessor methods directly into the class body
2. THE MMS SHALL provide @ToString and @EqualsAndHashCode decorators for standard method generation
3. THE MMS SHALL provide @NoArgsConstructor, @RequiredArgsConstructor, and @AllArgsConstructor decorators
4. THE MMS SHALL provide @Data decorator as composite for mutable classes (combines @Getter, @Setter, @ToString, @EqualsAndHashCode, @RequiredArgsConstructor)
5. THE MMS SHALL provide @Value decorator as composite for immutable structs/classes
6. THE MMS SHALL provide @Builder decorator for Builder pattern generation
7. WHEN a class-level decorator is applied, THE MMS SHALL generate code at compile-time by operating on the ClassNode and its children

##### 25B: Field-Level Decorators (Bottom-Up Injection via .parent())

8. THE MMS SHALL support field-level decorators that receive only the FieldNode and use `.parent()` to navigate up to the enclosing ClassNode for method injection
9. THE MMS SHALL provide @Getter as a field-level decorator that generates a single getter method for the annotated field and injects it into the enclosing class via `.parent()`
10. THE MMS SHALL provide @Setter as a field-level decorator that generates a single setter method for the annotated field and injects it into the enclosing class via `.parent()`
11. THE MMS SHALL provide @Property as a composite field-level decorator that combines @Getter and @Setter behavior: renames the field to `_name`, changes visibility to package-private, and generates both `name()` getter and `set_name(v: T)` setter methods (see Requirement 17)
12. WHEN a field-level decorator's `.parent()` pointer is nil, THE decorator SHALL call `ast.abort(...)` with a descriptive error message
13. WHEN a field-level decorator generates methods, THE decorator SHALL use `ast.quote { ... }` for code generation and call `parent_class.add_method(...)` to inject the generated method into the enclosing class

### Requirement 26: Runtime Metaprogramming

**User Story:** As a developer, I want runtime reflection and dynamic type creation, so that I can build frameworks and tools that adapt at runtime.

#### Acceptance Criteria

1. THE Standard Library SHALL provide reflect.* API for runtime introspection of types and values
2. THE Standard Library SHALL provide Meta.createClass(...) API for dynamic type generation
3. THE Language Runtime SHALL enable querying type information at runtime
4. THE Language Runtime SHALL enable dynamic invocation of methods and property access
5. WHEN creating types dynamically, THE Language Runtime SHALL register them with the appropriate type instance

### Requirement 27: Structured Concurrency (Library-Based, 3-Tier Runtime)

**User Story:** As a developer, I want structured concurrency through a library, so that I can write concurrent code that is safe and maintainable without language keywords, backed by a 3-tier runtime for scalable execution.

#### Acceptance Criteria

1. THE Standard Library SHALL provide async/await functionality through library functions and types (not keywords)
2. THE Standard Library SHALL implement structured concurrency with scoped task lifetimes using library constructs
3. THE Standard Library SHALL propagate cancellation through task hierarchies
4. WHEN a parent scope is cancelled, THE Standard Library SHALL cancel all child tasks
5. THE Standard Library SHALL prevent task leaks by ensuring all tasks complete before scope exit
6. THE Standard Library SHALL make "fire and forget" operations illegal
7. THE Standard Library SHALL use algebraic effects for async I/O operations
8. THE Standard Library SHALL provide Task[T] type for representing asynchronous computations
9. THE Standard Library SHALL provide coroutineScope function for creating structured concurrency scopes
10. THE Standard Library SHALL provide launch function for creating child tasks within a scope
11. THE Language Runtime SHALL enforce that shared mutable state is illegal — all cross-boundary communication MUST use message passing
12. THE Language Runtime SHALL implement a 3-tier concurrency runtime: Fibers (green threads for I/O concurrency), Actors (OS threads for CPU parallelism), and Isolates (OS processes for fault isolation)
13. THE Language Runtime SHALL implement Fibers as green threads with hardware stack-switching (via `callcc`/`boost::context` or equivalent) for lightweight cooperative scheduling, supporting millions of fibers per thread
14. THE Language Runtime SHALL implement Actors as OS threads with thread-local non-atomic ARC heaps, communicating exclusively via message passing
15. THE Language Runtime SHALL implement Isolates as OS processes providing fault isolation and security sandboxing, complementing effect-based sandboxing
16. THE Language Runtime SHALL provide a per-Isolate reactor/event loop (e.g., `boost::asio::io_context` or equivalent) with epoll/kqueue multiplexing for I/O-bound async execution
17. THE Language Runtime SHALL schedule Fibers cooperatively on the reactor event loop within each Actor
18. THE Standard Library SHALL map `Task[T]`, `coroutineScope`, and `launch` onto the Fiber tier as the default execution model

### Requirement 28: Strict Error Handling

**User Story:** As a developer, I want predictable error handling, so that all errors are values and exceptions are implemented as library effects rather than language keywords.

#### Acceptance Criteria

1. THE Meld Kernel SHALL NOT include try, catch, throw, or finally as keywords
2. THE Language Runtime SHALL use Result[T, E] type as the primary error handling mechanism
3. THE Language Runtime SHALL enable pattern matching on Result values
4. THE Standard Library SHALL provide combinators for Result (map, flatMap, mapError, etc.)
5. THE Language Runtime SHALL provide Attempt.run blocks for error recovery
6. THE Language Runtime SHALL support onFailure and onSuccess chaining
7. THE Standard Library SHALL implement exceptions as library macros built on the algebraic effects system
8. THE Standard Library SHALL provide throw as a library function that performs Exception.raise effect
9. THE Standard Library SHALL provide try/catch as library macros that expand to handle blocks for Exception effect
10. WHEN an exception is thrown, THE effect handler SHALL discard the continuation to unwind the stack
11. THE Standard Library SHALL support custom exception types as effect data payloads
12. THE Standard Library SHALL enable pattern matching on exception types in catch blocks
13. THE `E` type parameter in `Result[T, E]` SHOULD be an anonymous union type (e.g., `NetworkError | Timeout`) for idiomatic error handling; raw `T | E` unions SHALL be discouraged to prevent type overlap ambiguity
14. THE Language Runtime SHALL provide the `?!` (error propagation) postfix operator for `Result[T, E]` types: if `Ok(v)`, unwraps the value; if `Err(e)`, immediately returns the error up the call stack to the caller
15. THE `?!` operator SHALL target `Result[T, E]` only and SHALL NOT be applicable to `optional[T]`
16. THE `?!` operator SHALL be reserved and non-overloadable

**User Story:** As a developer, I want utility types for transforming existing types, so that I can create derived types without boilerplate.

#### Acceptance Criteria

1. THE Language Runtime SHALL support Omit[T, K] utility type for removing properties
2. THE Language Runtime SHALL support Pick[T, K] utility type for selecting properties
3. THE Language Runtime SHALL support Partial[T] utility type for making all properties optional
4. THE Language Runtime SHALL support Required[T] utility type for making all properties required
5. THE Language Runtime SHALL support Readonly[T] utility type for making all properties immutable

### Requirement 30: Expression-Oriented Language

**User Story:** As a developer, I want everything to be an expression that returns a value, so that I can write more composable and functional code.

#### Acceptance Criteria

1. THE Language Runtime SHALL treat all control flow constructs as expressions that return values
2. THE Language Runtime SHALL NOT distinguish between statements and expressions
3. THE Language Runtime SHALL allow assignment expressions to return the assigned value
4. THE Language Runtime SHALL allow block expressions to return the value of their last expression
5. WHEN a construct does not naturally produce a value, THE Language Runtime SHALL return Unit type

#### Note on Ternary Operators

Because Meld is expression-oriented, **the if-else macro serves the same purpose as ternary operators** in other languages. The Standard Library provides `if`/`else` as a macro that desugars to `Boolean.ifTrue:ifFalse:` calls. The syntax `if (condition) trueValue else falseValue` provides equivalent functionality with better readability and consistency.

**Example:**
```meld
// Meld uses if-else macro (not ternary operators) — desugars to Boolean.ifTrue:ifFalse:
val status = if (age >= 18) "adult" else "minor"

// Complex conditions with blocks
val message = if (user != null) {
    val name = user.name
    `Hello, ${name}!`
} else {
    "Hello, Guest!"
}
```

Meld explicitly does not support ternary (`? :`) operators. See Requirement 24.6 for the rationale.

### Requirement 31: Modules & Imports

**User Story:** As a developer, I want a module and import system that reuses existing language constructs (assignment, destructuring, mapping), so that I can organize code into modules and bring external symbols into scope without learning special-purpose grammar.

#### Acceptance Criteria

##### 31A: The `imp` Keyword

1. THE Language Runtime SHALL use `imp` as the only keyword for bringing external symbols into scope
2. THE Language Runtime SHALL NOT support `import`, `from`, or `as` as keywords — they are banned from the Meld grammar
3. THE `imp` statement SHALL be a top-level statement that must appear at the beginning of a file, before any declarations or expressions

##### 31B: Import Forms

4. THE Language Runtime SHALL support basic import syntax (`imp std.math`) that binds the module to its terminal path segment name
5. THE Language Runtime SHALL support module aliasing syntax (`imp m = std.math`) that binds the entire module to a custom local name using standard assignment
6. THE Language Runtime SHALL support named import (destructuring) syntax (`imp { sin, cos } = std.math`) that extracts specific symbols into local scope
7. THE Language Runtime SHALL support named import with aliasing syntax (`imp { sin -> s, cos -> c } = std.math`) that renames symbols during destructuring using the mapping operator
8. THE Language Runtime SHALL allow mixing aliased and non-aliased members within the same destructuring block
9. THE Language Runtime SHALL support multi-line formatting for destructuring import blocks

##### 31C: Module Definition and Visibility

10. A `.meld` file SHALL implicitly be a module, with its name derived from the file path
11. All top-level declarations SHALL be private to their defining module by default
12. THE Language Runtime SHALL support `@visibility(pub)` annotation to make declarations public to external consumers
13. THE Language Runtime SHALL support `@visibility(pkg)` annotation to make declarations visible within the same package but hidden from external consumers

##### 31D: Module Resolution

13. THE Compiler SHALL resolve module paths using dot-separated segments
14. Paths beginning with `std.` SHALL resolve to the Meld standard library
15. Other paths SHALL resolve relative to the project root, mapping dots to directory separators (e.g., `app.services.auth` → `app/services/auth.meld`)
16. Paths beginning with a registered package name SHALL resolve via the package manager or Git URL dependencies defined in `meld.toml`
17. THE Language Runtime SHALL support the `~/` path modifier for importing relative to the package root (e.g., `imp db = "~/src/db/conn"`), avoiding relative-path hell in deep directory structures

##### 31E: Circular Import Prevention

17. THE Compiler SHALL enforce a DAG (Directed Acyclic Graph) constraint on module dependencies
18. THE Compiler SHALL reject any import cycle at compile time with a structured error identifying the full cycle path
19. THE circular import error SHALL be compatible with the Compiler-Agent Protocol (CAP) for automated resolution

##### 31F: Re-exports and Facade Pattern

20. A module SHALL be able to re-export symbols from another module by importing them and annotating the local alias with `@visibility(pub)`
21. Importers of a re-exporting module SHALL gain access to re-exported symbols without needing to know their origin modules
22. THE Language Runtime SHALL support a `module.meld` facade file at the package root (e.g., `src/module.meld`) that serves as the single public API surface for a library package
23. THE `module.meld` facade SHALL import internal symbols and selectively re-export them using `@visibility(pub)` annotations
24. THE `meld.toml` project configuration SHALL support an `entry` field specifying the facade file path (defaults to `src/module.meld`)
25. Consumers of a package SHALL only see symbols exported through the facade, regardless of `@visibility(pub)` annotations on internal modules

##### 31G: Namespaces

22. THE Standard Library SHALL provide namespace as a macro that desugars to scope creation using kernel primitives (scope, def, lookup)
23. THE namespace macro SHALL support nested namespaces via nested scope creation
24. THE Language Runtime SHALL resolve name conflicts using fully qualified names
25. THE Meld Kernel SHALL NOT include namespace as a reserved keyword

### Requirement 32: Partial Application and Currying

**User Story:** As a developer, I want partial application and currying, so that I can create specialized functions from general ones.

#### Acceptance Criteria

1. THE Language Runtime SHALL support partial application of function arguments
2. THE Language Runtime SHALL support currying syntax for multi-parameter functions
3. THE Language Runtime SHALL enable binding specific arguments while leaving others unbound
4. THE Language Runtime SHALL support placeholder syntax (e.g., _ or ?) for unbound arguments
5. THE Language Runtime SHALL automatically curry functions when partially applied

### Requirement 33: Anonymous Types and Collection Literals

**User Story:** As a developer, I want concise syntax for creating anonymous objects, maps, sets, arrays, and tuples, so that I can write expressive code without boilerplate.

#### Acceptance Criteria

1. THE Language Runtime SHALL support anonymous object syntax using curly braces with heterogeneous types: {field: value, ...}
2. THE Language Runtime SHALL support anonymous map syntax using curly braces with homogeneous value types: {key: value, ...}
3. THE Language Runtime SHALL support anonymous set syntax using curly braces with homogeneous element types: {"elem1", "elem2", ...}
4. THE Language Runtime SHALL support anonymous array syntax using square brackets with homogeneous element types: [elem1, elem2, ...]
5. THE Language Runtime SHALL support anonymous tuple syntax using square brackets with heterogeneous element types: [value1, value2, ...]
6. THE Language Runtime SHALL infer whether curly brace syntax creates an object, map, or set based on type homogeneity
7. THE Language Runtime SHALL infer whether square bracket syntax creates an array or tuple based on type homogeneity
8. THE Language Runtime SHALL support type annotations to disambiguate when inference is ambiguous

### Requirement 34: Polyglot Architecture

**User Story:** As a developer, I want multi-target compilation, so that I can deploy Meld code to different platforms.

#### Acceptance Criteria

1. THE Compiler SHALL support transpilation to JVM bytecode
2. THE Compiler SHALL support transpilation to Rust source code
3. THE Compiler SHALL support transpilation to C++17 source code
4. THE Compiler SHALL support WebAssembly compilation
5. THE Compiler SHALL use meld.toml for multi-target project configuration

### Requirement 35: @extern Macro for FFI

**User Story:** As a developer, I want seamless foreign function interface, so that I can use existing libraries from other languages.

#### Acceptance Criteria

1. THE MMS SHALL provide @extern macro for foreign function declarations
2. THE @extern macro SHALL support multiple target languages (java, rust, cpp, c)
3. THE @extern macro SHALL generate appropriate bindings for each target
4. THE @extern macro SHALL support type mapping between Meld and target languages
5. THE @extern macro SHALL enable seamless interop with existing ecosystems

### Requirement 36: Auto-MCP Generation

**User Story:** As a developer, I want automatic MCP generation, so that my code can be exposed as tools for AI agents.

#### Acceptance Criteria

1. THE Compiler SHALL support --target=mcp flag for MCP generation
2. THE Compiler SHALL generate JSON Schema for all public functions
3. THE Compiler SHALL extract tool definitions from @blueprint metadata
4. THE Compiler SHALL create MCP server configuration automatically
5. THE Compiler SHALL support incremental MCP updates

### Requirement 37: Tooling and Ecosystem Support

**User Story:** As a developer, I want integrated tooling support, so that I have a productive development experience.

#### Acceptance Criteria

1. THE Language Specification SHALL define standard hooks for REPL integration
2. THE Language Specification SHALL define WebAssembly (Wasm) as a compilation target
3. THE Language Specification SHALL define C-based FFI for interoperability
4. THE Language Specification SHALL define LSP (Language Server Protocol) support requirements
5. THE Language Specification SHALL define debugging protocol support requirements
6. THE Language Specification SHALL support hot reload for development
7. THE Language Specification SHALL integrate with AI coding assistants

### Requirement 38: Abstract Syntax Graph (ASG)

**User Story:** As an AI agent, I want code represented as a graph with explicit relationships, so that I can trace data flow and variable usage precisely without text-based analysis.

#### Acceptance Criteria

1. THE Language Runtime SHALL represent code as an Abstract Syntax Graph (ASG) where nodes are language constructs and edges represent relationships
2. THE ASG SHALL include Data Flow edges that connect variable definitions to all usage sites
3. THE ASG SHALL include Control Flow edges that represent execution paths through the program
4. THE ASG SHALL include Variable Scoping edges that link declarations to their lexical scopes
5. THE Language Runtime SHALL provide SemanticGraph API that replaces the Code API from Section 11
6. THE SemanticGraph API SHALL provide graph.findUsage(symbol) method that returns all usage edges for a symbol
7. THE SemanticGraph API SHALL provide graph.dataFlow(variable) method that traces data flow paths
8. THE SemanticGraph API SHALL provide graph.controlFlow(node) method that returns control flow successors
9. THE SemanticGraph API SHALL enable bidirectional traversal of all relationship edges
10. THE Compiler SHALL build the ASG during semantic analysis phase
11. THE ASG nodes SHALL be represented as first-class Meld values built from kernel primitives (cells, symbols, scopes), making them queryable, pattern-matchable, and transformable using standard Meld operations
12. THE ASG edges SHALL be represented as Meld values with symbol-tagged metadata, enabling standard Meld pattern matching and structural search on graph relationships
13. THE SemanticGraph API SHALL be a convenience layer over the underlying Meld data structures, not an opaque abstraction

### Requirement 39: Explicit Effect Tracking

**User Story:** As a developer working with AI, I want functions to explicitly declare their side effects, so that the compiler can enforce purity constraints and AI agents cannot hallucinate unsafe operations.

#### Acceptance Criteria

1. THE Language Runtime SHALL support effects clause in function signatures to declare side effects
2. THE Language Runtime SHALL provide built-in effect types: EffectPure, EffectIO, EffectNetwork, EffectState, EffectTime
3. THE Language Runtime SHALL treat functions without effects clause as EffectPure by default
4. THE Compiler SHALL enforce that function bodies only perform effects declared in the effects clause
5. THE Compiler SHALL enforce that calling a function with effects requires the caller to also declare those effects
6. THE Compiler SHALL allow effect composition where a function can declare multiple effects
7. THE Language Runtime SHALL support custom user-defined effect types
8. THE Compiler SHALL provide effect inference for lambda expressions based on their body
9. THE Compiler SHALL generate compile-time errors when undeclared effects are detected
10. THE Language Runtime SHALL enable effect polymorphism where generic functions can be parameterized by effects

### Requirement 40: Binary Context Format (MELD-B)

**User Story:** As an AI agent, I want a compact binary format for code representation, so that I can load large project contexts instantly without token-expensive text parsing.

#### Acceptance Criteria

1. THE Compiler SHALL support compilation to MELD-B (Meld Binary) format with .mldb file extension
2. THE MELD-B format SHALL store the complete ASG with all nodes, edges, and metadata
3. THE MELD-B format SHALL include pre-computed indices for symbol lookup, type information, and cross-references
4. THE MELD-B format SHALL use compression to minimize file size
5. THE MELD-B format SHALL be versioned to support format evolution
6. THE Language Runtime SHALL provide MeldBinary.load(path) API for loading .mldb files
7. THE Language Runtime SHALL provide MeldBinary.query(pattern) API for querying loaded binary contexts
8. THE Compiler SHALL support --target=mldb flag for generating binary context files
9. THE MELD-B format SHALL achieve at least 10x size reduction compared to source text
10. THE Compiler-Agent Protocol (CAP) SHALL support MELD-B as the standard exchange format for large codebases
11. WHEN loading a MELD-B file, THE Language Runtime SHALL deserialize into first-class Meld data structures (cells, symbols, scopes) that can be queried using standard Meld operations
12. THE MELD-B deserialized data SHALL be compatible with the SemanticGraph API and standard pattern matching

### Requirement 41: Algebraic Effects System (Library-First with Kernel Primitive)

**User Story:** As a developer working with AI agents, I want a library-first algebraic effect system built on a minimal kernel primitive, so that I can sandbox and intercept side effects without language keywords while maintaining the flexibility to implement exceptions, async/await, generators, and other control flow as library constructs.

#### Acceptance Criteria

1. THE Meld Kernel SHALL provide primitive_suspend as the single low-level primitive for capturing execution continuations
2. THE Standard Library SHALL implement effect as a library macro (not a keyword) for defining abstract effect interfaces
3. THE Standard Library SHALL implement perform as a library function (not a keyword) that calls primitive_suspend to invoke effects
4. THE Standard Library SHALL implement handle as a library macro (not a keyword) for intercepting and implementing effect operations
5. THE Standard Library SHALL enable effect handlers to call resume() to continue execution from the suspension point
6. THE Compiler SHALL automatically infer which effects a function performs based on its implementation
7. THE Compiler SHALL automatically insert @uses(...) annotations on functions that perform effects
8. THE IDE SHALL display inferred effect annotations as ghost text (inlay hints) before the compiler writes them
9. WHEN a file is saved or formatted, THE Compiler SHALL physically write inferred @uses annotations into the source code
10. WHEN a function's implementation changes, THE Compiler SHALL automatically update its @uses annotation on next save
11. WHEN a developer manually writes an @uses annotation, THE Compiler SHALL treat it as a constraint and verify the implementation matches
12. WHEN a function implementation performs effects not declared in a manual @uses annotation, THE Compiler SHALL raise a compilation error
13. WHEN a function performs an effect, THE Language Runtime SHALL search the dynamic scope for an appropriate handler
14. WHEN an effect handler provides an implementation, THE Language Runtime SHALL execute the handler code and capture the continuation
15. THE Language Runtime SHALL enable effect handlers to inspect effect parameters before resuming
16. THE Language Runtime SHALL enable effect handlers to modify return values from resumed computations
17. THE Language Runtime SHALL support multiple effect handlers in nested scopes with inner handlers taking precedence
18. THE Language Runtime SHALL enable AI safety patterns where AI-generated code can be sandboxed by wrapping in effect handlers
19. THE Standard Library SHALL provide common built-in effects including FileSystem, Network, Time, Random, Console, and Exception
20. THE Compiler SHALL propagate effects through the call graph automatically
21. WHEN a function calls another function with effects, THE Compiler SHALL include those effects in the caller's inferred @uses annotation
22. THE Standard Library SHALL implement exceptions as an effect where throw performs Exception.raise and try/catch expands to handle blocks
23. THE Standard Library SHALL implement async/await as effects where await performs Async.wait and the scheduler handles resumption
24. THE Standard Library SHALL implement generators as effects where yield performs Generator.yield and handlers resume multiple times
25. THE Meld Kernel SHALL NOT include try, catch, throw, async, await, yield, or any effect-related keywords

### Requirement 41B: Unified Control Flow Implementation

**User Story:** As a language implementer, I want all control flow mechanisms (exceptions, async/await, generators) implemented as algebraic effects, so that the language maintains a minimal kernel with a single unified control flow primitive.

#### Acceptance Criteria

1. THE Standard Library SHALL implement exceptions as an algebraic effect where throw performs Exception.raise and handlers discard continuations to unwind the stack
2. THE Standard Library SHALL implement async/await as an algebraic effect where await performs Async.wait and handlers store continuations for later resumption
3. THE Standard Library SHALL implement generators as an algebraic effect where yield performs Generator.yield and handlers resume continuations multiple times
4. THE Standard Library SHALL provide try/catch/throw as library macros that expand to handle blocks for the Exception effect
5. THE Standard Library SHALL provide Task[T] type and .await() method as library constructs using the Async effect
6. THE Standard Library SHALL provide generator functions using the Generator effect
7. WHEN an exception is thrown, THE effect handler SHALL discard the continuation to implement stack unwinding
8. WHEN an async operation awaits, THE effect handler SHALL store the continuation and resume it when the operation completes
9. WHEN a generator yields, THE effect handler SHALL resume the continuation to produce the next value
10. THE Language Runtime SHALL implement all three mechanisms (exceptions, async, generators) using only the primitive_suspend kernel primitive

### Requirement 41A: System.out Namespace for Console Output

**User Story:** As a developer, I want a standard namespace for console output operations, so that I can write output code that is consistent, discoverable, and aligned with the effects system for AI safety.

#### Acceptance Criteria

1. THE Standard Library SHALL provide System.out namespace for console output operations
2. THE System.out namespace SHALL provide println(message: string) function for printing with newline
3. THE System.out namespace SHALL provide print(message: string) function for printing without newline
4. THE Compiler SHALL automatically infer and annotate System.out.println with @uses(Console)
5. THE Compiler SHALL automatically infer and annotate System.out.print with @uses(Console)
6. WHEN System.out.println is called, THE Language Runtime SHALL perform Console.println effect
7. WHEN System.out.print is called, THE Language Runtime SHALL perform Console.print effect
8. THE Console effect SHALL define println(message: string) operation
9. THE Console effect SHALL define print(message: string) operation
10. THE Console effect SHALL define readLine() -> string operation for input

### Requirement 42: Property-Based Testing with forall Macro

**User Story:** As a developer working with AI, I want native property-based testing, so that AI agents generate correctness properties instead of just example-based tests.

#### Acceptance Criteria

1. THE Standard Library SHALL provide forall as a macro within test blocks for property-based testing (not a keyword)
2. THE Standard Library SHALL support type-based automatic input generation for forall quantified variables
3. THE Language Runtime SHALL generate at least 100 random test cases for each forall property by default
4. THE Language Runtime SHALL support configurable iteration count for property-based tests
5. THE Language Runtime SHALL generate edge cases automatically including empty collections, boundary values, and special characters
6. WHEN a property-based test fails, THE Language Runtime SHALL report the minimal failing input that violates the property
7. THE Language Runtime SHALL support shrinking of failing inputs to find minimal counterexamples
8. THE Language Runtime SHALL enable custom input generators for user-defined types
9. THE Language Runtime SHALL support property-based testing for functions with multiple parameters
10. THE Standard Library SHALL integrate forall properties with the Compiler-Agent Protocol to guide AI code generation
11. THE MMS SHALL execute property-based tests during compilation when used in test macro blocks
12. THE Standard Library SHALL support combining forall with effect handlers for deterministic testing of effectful code

### Requirement 43: Visual Logic with flow Macro

**User Story:** As a developer, I want a state machine macro that renders visually in IDEs, so that I can replace nested conditional logic with clear, maintainable state transitions.

#### Acceptance Criteria

1. THE Standard Library SHALL provide flow as a macro for defining state machines
2. THE flow macro SHALL require an initial state declaration
3. THE flow macro SHALL support state blocks that define named states
4. THE flow macro SHALL support on(Event) transitions within state blocks
5. THE flow macro SHALL support goto for state transitions
6. THE MMS SHALL validate at compile-time that all referenced states are defined
7. THE MMS SHALL validate at compile-time that all events are handled appropriately
8. THE flow macro SHALL expand to executable code implementing the state machine
9. THE IDE Integration SHALL render flow macro expansions as visual flowcharts
10. THE flow macro SHALL support guard conditions on transitions
11. THE flow macro SHALL support entry and exit actions for states
12. THE Standard Library SHALL enable runtime inspection of current state

### Requirement 44: Flight Recorder for Crash Replay

**User Story:** As a developer debugging AI-generated code, I want automatic crash state snapshots, so that AI agents can deterministically replay and fix bugs.

#### Acceptance Criteria

1. THE Language Runtime SHALL provide Runtime.snapshot() API for capturing execution state
2. THE snapshot SHALL include all function inputs at the time of crash
3. THE snapshot SHALL include the complete effect history (all side effects performed)
4. THE snapshot SHALL include the call stack with local variables
5. THE snapshot SHALL be serializable to disk for later analysis
6. THE Language Runtime SHALL automatically capture snapshots on unhandled errors
7. THE Language Runtime SHALL provide Runtime.replay(snapshot) API for deterministic replay
8. WHEN replaying a snapshot, THE Language Runtime SHALL restore all inputs and effect handlers
9. THE snapshot format SHALL be compatible with the Compiler-Agent Protocol
10. THE Language Runtime SHALL enable configurable snapshot verbosity levels
11. THE snapshot SHALL include timestamp and environment metadata
12. THE Language Runtime SHALL support snapshot comparison for regression analysis

### Requirement 45: Code Provenance and Trust Model

**User Story:** As a developer working with AI agents, I want automatic tracking of code authorship and trust levels, so that I can distinguish verified human logic from unverified AI suggestions.

#### Acceptance Criteria

1. THE Compiler SHALL attach provenance metadata to every AST node
2. THE provenance metadata SHALL include Origin.Human for manually typed code
3. THE provenance metadata SHALL include Origin.Agent(model, confidence) for AI-generated code
4. THE provenance metadata SHALL include Origin.Verified(reviewer) for approved AI code
5. THE IDE Integration SHALL render a trust heatmap over code using color tinting
6. THE IDE Integration SHALL use green tint for verified human code
7. THE IDE Integration SHALL use purple tint for high-confidence agent code
8. THE IDE Integration SHALL use red underline for low-confidence or failing agent code
9. THE Compiler SHALL detect provenance mismatches when code changes without blueprint updates
10. WHEN a provenance mismatch is detected, THE Compiler SHALL flag the deviation for review
11. THE Language Runtime SHALL provide Provenance.query(node) API for inspecting code origins
12. THE provenance metadata SHALL be preserved in MELD-B binary format
13. THE Compiler SHALL support --trust-level flag to enforce minimum trust requirements
14. THE Language Runtime SHALL enable filtering code by provenance in structural searches

### Requirement 46: Shadow Provenance for Clean Source Code

**User Story:** As a developer, I want AI conversation history stored separately from source code, so that my codebase remains clean while preserving full generation context.

#### Acceptance Criteria

1. THE Compiler SHALL store AI conversation history in .meld/history directory
2. THE shadow history SHALL include all refinements and previous drafts for each function
3. THE shadow history SHALL be linked to source code via AST node identifiers
4. THE IDE Integration SHALL render history as a gutter heatmap, not inline text
5. THE IDE Integration SHALL provide hover tooltips showing conversation excerpts
6. THE shadow history SHALL be version-controlled separately from source code
7. THE Language Runtime SHALL provide History.query(node) API for accessing conversation history
8. THE shadow history SHALL include timestamps and agent model information
9. THE Compiler SHALL support --include-history flag for exporting code with full context
10. THE shadow history SHALL be queryable by the Compiler-Agent Protocol
11. THE IDE Integration SHALL enable navigation through historical versions
12. THE shadow history SHALL preserve the relationship between blueprints and implementations

### Requirement 47: Unsigned Types and Numeric Wrappers (Library-Based)

**User Story:** As a systems programmer, I want unsigned integer types and specific bit-width integers without bloating the kernel, so that I can write low-level code while maintaining kernel simplicity.

#### Acceptance Criteria

1. THE Meld Kernel SHALL NOT include uint, u8, u16, u32, u64, i8, i16, or i32 as primitives
2. THE Standard Library SHALL define unsigned types as refinement types (e.g., type uint -> int where { it >= 0 })
3. THE Standard Library SHALL define bit-width types as value structs wrapping the kernel int primitive (e.g., struct u8 { bits: int })
4. THE Transpiler SHALL map library wrapper types to native machine types (e.g., u8 to C++ uint8_t, Java byte)
5. THE Standard Library SHALL provide bitwise operators for unsigned types via library methods
6. THE Standard Library SHALL support literal suffixes for typed integers via macros (e.g., 255u8, 0xDEAD_BEEF_u32)
7. THE Standard Library SHALL define char as a 32-bit Unicode scalar value (code point) wrapping int
8. THE Standard Library SHALL define i8 (byte) and i16 (short) as signed integer wrappers
9. THE Compiler SHALL validate refinement type constraints at compile-time where possible and runtime otherwise
10. THE Standard Library SHALL provide Buffer type for packed byte arrays with efficient memory layout

### Requirement 48: Kernel Primitive Categories

**User Story:** As a language implementer, I want clear categorization of the 20 kernel primitives, so that I understand the minimal foundation required to implement Meld.

#### Acceptance Criteria

1. THE Meld Kernel SHALL organize primitives into exactly 7 categories: Data Structure, Scalar, Execution, Control Flow, Memory & Binding, AI & Metadata, and Interop
2. THE Data Structure category SHALL include: cell (pair), vec (contiguous memory), symbol (interned identifier), scope (symbol-to-value dictionary)
3. THE Scalar category SHALL include: int (64-bit signed), float (64-bit IEEE), bool (true/false), nil (empty unit)
4. THE Execution category SHALL include: lambda (closure creation), apply (function invocation), eval (interpreter core), quote (prevent evaluation)
5. THE Control Flow category SHALL include: primitive_suspend (captures current execution context/continuation and jumps to delimiter)
6. THE Memory & Binding category SHALL include: def (define variable in current scope), set (mutate variable in nearest defining scope), lookup (traverse scope chain)
7. THE AI & Metadata category SHALL include: meta_set (attach hidden metadata), meta_get (retrieve hidden metadata)
8. THE Interop category SHALL include: native_call (call host function), native_load (load shared library)
9. THE Meld Kernel SHALL implement ALL control flow (loops, exceptions, async, generators) as library constructs, with if/else provided as a Standard Library macro and mark_stack, suspend, and resume as library functions built on primitive_suspend
10. THE Meld Kernel SHALL NOT include return, break, yield, throw, or await as primitives
11. THE type concept SHALL be implemented as a library feature built on meta_set and meta_get primitives, not as a kernel primitive

> **Note:** Advanced async enhancements (error handling, manual task completion, heterogeneous combining, state inspection, delay/timeout, combinators, execution control, task copying) are specified in `.kiro/specs/meld-async/requirements.md` Req 14–21. That spec is the sole authority for async enhancement requirements.

---

### Requirement 57: Mutating Methods and Mutable Parameters

**User Story:** As a developer, I want explicit syntax for methods that mutate their receiver and for parameters that can be mutated, so that mutation is always visible at both the declaration and call site.

#### Acceptance Criteria

1. THE Language Runtime SHALL support `var fnc` syntax for declaring methods that mutate `this` (e.g., `var fnc increment()`)
2. THE Language Runtime SHALL require `var fnc` for any method that modifies the receiver's state
3. THE Compiler SHALL reject methods that mutate `this` without the `var fnc` declaration
4. THE Language Runtime SHALL support `var` parameter modifier for mutable parameters (e.g., `fnc update(u: var User)`)
5. THE Compiler SHALL treat all function parameters as immutable (`val`) by default
6. THE Compiler SHALL reject mutation of parameters not declared with `var`
7. THE Language Runtime SHALL make mutation visible at the call site for `var` parameters
8. THE `var fnc` syntax SHALL be compatible with trait method declarations

### Requirement 58: Effect Firewall — Dependency Sandboxing

**User Story:** As a developer, I want dependencies sandboxed by default so that a compromised or malicious dependency cannot perform undeclared side effects, providing mathematical supply-chain security guarantees.

#### Acceptance Criteria

1. THE Compiler SHALL treat all third-party dependencies as sandboxed by default — they may only perform effects explicitly granted by the consuming project
2. THE `meld.toml` dependency configuration SHALL support an `allow` array per dependency specifying permitted effects (e.g., `allow = ["net", "fs"]`)

> **Short name mapping:** The `allow` array uses short names that map to Meld-level effect traits (meld-core Req 81) and the C++ `Effect` enum (meld-manifest Req 10) as follows:
> - `"net"` → Meld trait `network` → C++ `Effect::Network`
> - `"fs"` → Meld trait `file_system` → C++ `Effect::FileSystemRead` + `Effect::FileSystemWrite`
> - `"console"` → Meld trait `console` → C++ (mapped to I/O effects)
> - `"random"` → Meld trait `random` → C++ (mapped to `Effect::State`)
> - `"time"` → Meld trait `time` → C++ `Effect::SystemTime`
> - `"exec"` → (no Meld trait) → C++ `Effect::ProcessExec`
3. WHEN a dependency performs an effect not listed in its `allow` array, THE Compiler SHALL reject the build with a structured error identifying the undeclared effect and the dependency
4. WHEN a dependency has no `allow` array, THE Compiler SHALL treat it as pure — no effects permitted
5. THE Compiler SHALL transitively enforce effect permissions: if dependency A depends on B, A's allowed effects must be a superset of B's required effects
6. THE effect permission model SHALL integrate with the `@uses` annotation system — the compiler verifies dependency effect signatures against granted permissions
7. THE Compiler-Agent Protocol (CAP) SHALL include effect permission violations in structured error output with fix suggestions (e.g., "add `net` to allow list for dependency X")
8. THE Standard Library dependencies SHALL be exempt from Effect Firewall restrictions (they are trusted by default)

### Requirement 59: `meld.toml` as Single Source of Truth

**User Story:** As a developer, I want a single configuration file that defines my project metadata, dependencies, effect permissions, and build targets, so that I don't maintain duplicate configuration across multiple systems.

#### Acceptance Criteria

1. THE `meld.toml` file SHALL be the single source of truth for project configuration, located at the project root
2. THE `meld.toml` file SHALL support a `[project]` section with `name`, `version`, `entry` (facade path), and `targets` (compilation targets) fields
3. THE `meld.toml` file SHALL support a `[dependencies]` section where each dependency specifies a Git URL or registry name, version, and optional `allow` effect permissions array
4. THE `meld.toml` file SHALL support a `[build]` section for build-system-specific configuration (e.g., Bazel integration flags)
5. THE Compiler SHALL read `meld.toml` for dependency resolution, effect permission enforcement, and compilation target selection
6. THE `rules_meld` Bazel integration SHALL parse `meld.toml` during workspace evaluation and auto-generate `meld_library` targets from declared dependencies
7. THE `rules_meld` integration SHALL pipe `allow` effect arrays from `meld.toml` to compiler CLI flags (e.g., `--allow-effects=net,fs`)
8. WHEN `meld.toml` is absent, THE Compiler SHALL treat the project as a single-file script with no dependencies and default settings

---

> **Note:** Requirements 60–69 below were merged from the retired `.kiro/specs/meld-examples-syntax-sync/` spec. They cover synchronizing all `.meld` example files in `meld-core/examples/` with the current language syntax.

### Requirement 60: Example Function Declaration Consistency

**User Story:** As a developer learning Meld, I want all examples to use consistent function declaration syntax, so that I can understand the correct way to define functions.

#### Acceptance Criteria

1. THE Meld_Examples SHALL use `fnc` for all function declarations consistently
2. WHEN a function has effects, THE Meld_Examples SHALL use the `@uses(EffectName)` annotation syntax before the function declaration
3. WHEN a function has no effects, THE Meld_Examples SHALL omit the effects clause entirely
4. THE Meld_Examples SHALL use `rtn` for return statements consistently
5. WHEN a function has parameters, THE Meld_Examples SHALL use consistent type annotation syntax

### Requirement 61: Example String Interpolation Standardization

**User Story:** As a developer, I want all string interpolation in examples to follow the same syntax pattern, so that I can write consistent code.

#### Acceptance Criteria

1. THE Meld_Examples SHALL use backtick strings for all string interpolation
2. WHEN interpolating variables, THE Meld_Examples SHALL use `${variable}` syntax within backtick strings
3. THE Meld_Examples SHALL use regular double quotes for simple string literals without interpolation
4. WHEN using multi-line strings, THE Meld_Examples SHALL use backtick strings consistently

### Requirement 62: Example Effect Annotation Consistency

**User Story:** As a developer, I want to understand how to properly annotate functions with effects, so that I can write safe and predictable code.

#### Acceptance Criteria

1. WHEN a function performs side effects, THE Meld_Examples SHALL declare effects using `@uses(effect_name)` annotation syntax
2. THE Meld_Examples SHALL use standard effect names: `file_system`, `network`, `console`, `random`, `time`, `async`, `generator`, `exception`
3. WHEN multiple effects are used, THE Meld_Examples SHALL list them as separate `@uses` annotations or comma-separated within a single annotation
4. THE Meld_Examples SHALL place the `@uses` annotations before the function declaration

### Requirement 63: Example Property-Based Testing Syntax

**User Story:** As a developer, I want to see consistent property-based testing syntax in examples, so that I can write effective tests for my code.

#### Acceptance Criteria

1. THE Meld_Examples SHALL use `test "description" forall variable: type` syntax for property-based tests
2. WHEN using test configuration, THE Meld_Examples SHALL use `config { iterations: N, seed: N }` syntax
3. WHEN using assumptions, THE Meld_Examples SHALL use `assume condition` syntax
4. THE Meld_Examples SHALL use `assert condition, "message"` for test assertions
5. WHEN using custom generators, THE Meld_Examples SHALL use `generator { generator_function() }` syntax

### Requirement 64: Example Blueprint Annotation Modernization

**User Story:** As a developer, I want to see the latest blueprint annotation format in examples, so that I can document my code effectively for AI agents.

#### Acceptance Criteria

1. THE Meld_Examples SHALL use v2.1 blueprint format with structured input-output examples
2. WHEN providing examples, THE Meld_Examples SHALL use `{ input: "...", output: "...", description: "..." }` format
3. THE Meld_Examples SHALL include `summary`, `rules`, `examples`, `inputs`, `outputs`, `links`, and `tags` fields in blueprints
4. THE Meld_Examples SHALL avoid deprecated fields like `cost` and `intent`
5. WHEN using blueprint inheritance, THE Meld_Examples SHALL use the `extends` field correctly

### Requirement 65: Example Control Flow Modernization

**User Story:** As a developer, I want to see how to use Meld's "no keywords" approach to control flow in examples, so that I can write idiomatic Meld code.

#### Acceptance Criteria

1. THE Meld_Examples SHALL use method-based conditional logic instead of `if/else` keywords
2. WHEN demonstrating loops, THE Meld_Examples SHALL use collection methods like `forEach`, `map`, `filter`
3. WHEN demonstrating pattern matching, THE Meld_Examples SHALL use the `match` macro syntax
4. THE Meld_Examples SHALL use `flow` constructs for state machine demonstrations
5. THE Meld_Examples SHALL avoid traditional control flow keywords

### Requirement 66: Example Type System Consistency

**User Story:** As a developer, I want to see consistent type annotations and declarations in examples, so that I can understand Meld's type system.

#### Acceptance Criteria

1. THE Meld_Examples SHALL use consistent nullable type syntax with `type?`
2. WHEN demonstrating union types, THE Meld_Examples SHALL use `type1 | type2` syntax
3. WHEN demonstrating intersection types, THE Meld_Examples SHALL use `type1 & type2` syntax
4. THE Meld_Examples SHALL use `struct` for value types and `class` for reference types consistently
5. WHEN using generics, THE Meld_Examples SHALL use `Type[Parameter]` syntax consistently

### Requirement 67: Example Import and Module System

**User Story:** As a developer, I want to understand how to properly import modules and organize code using the `imp` keyword in examples, so that I can structure my projects effectively.

#### Acceptance Criteria

1. THE Meld_Examples SHALL use `imp module.path` syntax for basic imports (binding to terminal name)
2. WHEN importing specific items, THE Meld_Examples SHALL use `imp { Item1, Item2 } = module.path` destructuring syntax
3. WHEN using module aliases, THE Meld_Examples SHALL use `imp alias = module.path` assignment syntax
4. WHEN renaming imported symbols, THE Meld_Examples SHALL use `imp { OriginalName -> LocalName } = module.path` mapping syntax
5. THE Meld_Examples SHALL demonstrate proper module organization patterns
6. THE Meld_Examples SHALL use consistent import ordering and grouping
7. THE Meld_Examples SHALL NOT use `import`, `from`, or `as` keywords — these are banned from the Meld grammar

### Requirement 68: Example Error Handling Modernization

**User Story:** As a developer, I want to see how to handle errors without traditional try-catch blocks in examples, so that I can write robust Meld code.

#### Acceptance Criteria

1. THE Meld_Examples SHALL use `Result[T, E]` type for error-prone operations
2. WHEN demonstrating error handling, THE Meld_Examples SHALL use `match` on Result types
3. THE Meld_Examples SHALL use `require` and `ensure` for design-by-contract
4. WHEN using the Attempt pattern, THE Meld_Examples SHALL use `Attempt.run { }` syntax
5. THE Meld_Examples SHALL avoid traditional `try`, `catch`, `throw` keywords

### Requirement 69: Example File Structure Compliance

**User Story:** As a developer, I want examples to follow the established file separation pattern, so that I can understand the recommended project structure.

#### Acceptance Criteria

1. WHEN an example has both `.cpp` and `.meld` files, THE Meld_Examples SHALL maintain strict separation between C++ implementation and Meld language demonstration
2. THE `.meld` files SHALL contain only pure Meld language syntax and examples
3. THE `.meld` files SHALL not contain embedded C++ code or raw string literals
4. THE Meld_Examples SHALL include proper documentation comments in `.meld` files
5. THE Meld_Examples SHALL demonstrate realistic use cases and language features

---

> **Note:** Requirements 70–77 below were merged from the retired `.kiro/specs/mandatory-val-var-annotations/` spec. They cover making `val`/`var` mandatory on all declaration contexts.

### Requirement 70: Mandatory Mutability Annotation on Struct Fields

**User Story:** As a Meld developer, I want the compiler to require `val` or `var` on every struct field, so that every field's mutability intent is explicit and unambiguous.

#### Acceptance Criteria

1. WHEN a struct field declaration has the `val` keyword, THE Parser SHALL parse the field as immutable with `is_mutable = false` and `has_explicit_val = true`
2. WHEN a struct field declaration has the `var` keyword, THE Parser SHALL parse the field as mutable with `is_mutable = true` and `has_explicit_val = false`
3. IF a struct field declaration has neither `val` nor `var` keyword, THEN THE Parser SHALL report an error with the message "Missing mutability annotation: use 'val' for immutable or 'var' for mutable"
4. WHEN the Parser reports a missing mutability annotation error, THE Parser SHALL include the source line number and column of the field name in the error message

### Requirement 71: Mandatory Mutability Annotation on Class Fields

**User Story:** As a Meld developer, I want the compiler to require `val` or `var` on every class field, so that class field mutability is always stated explicitly.

#### Acceptance Criteria

1. WHEN a class field declaration has the `val` keyword, THE Parser SHALL parse the field as immutable with `is_mutable = false` and `has_explicit_val = true`
2. WHEN a class field declaration has the `var` keyword, THE Parser SHALL parse the field as mutable with `is_mutable = true` and `has_explicit_val = false`
3. IF a class field declaration has neither `val` nor `var` keyword, THEN THE Parser SHALL report an error with the message "Missing mutability annotation: use 'val' for immutable or 'var' for mutable"
4. WHEN the Parser reports a missing mutability annotation error on a class field, THE Parser SHALL include the source line number and column of the field name in the error message

### Requirement 72: Mandatory Mutability Annotation on Property Declarations

**User Story:** As a Meld developer, I want the compiler to require `val` or `var` on every property declaration, so that property mutability is always explicit.

#### Acceptance Criteria

1. WHEN a property declaration has the `val` keyword, THE Parser SHALL parse the property as immutable with `is_mutable = false` and `has_explicit_val = true`
2. WHEN a property declaration has the `var` keyword, THE Parser SHALL parse the property as mutable with `is_mutable = true` and `has_explicit_val = false`
3. IF a property declaration has neither `val` nor `var` keyword, THEN THE Parser SHALL report an error with the message "Missing mutability annotation: use 'val' for immutable or 'var' for mutable"
4. WHEN the Parser reports a missing mutability annotation error on a property, THE Parser SHALL include the source line number and column of the property name in the error message

### Requirement 73: Mandatory Mutability Annotation on Function Parameters

**User Story:** As a Meld developer, I want the compiler to require `val` or `var` on every function parameter, so that parameter mutability is always stated explicitly in function signatures.

#### Acceptance Criteria

1. WHEN a function parameter has the `val` keyword, THE Parser SHALL parse the parameter as immutable with `has_explicit_val = true`
2. WHEN a function parameter has the `var` keyword, THE Parser SHALL parse the parameter as mutable with `is_mutable = true`
3. IF a function parameter has neither `val` nor `var` keyword and no `@const` or `@mut` decorator, THEN THE Parser SHALL report an error with the message "Missing mutability annotation: use 'val' for immutable or 'var' for mutable"
4. WHEN a function parameter has a `@const` decorator without `val`, THE Parser SHALL accept the parameter without error (the decorator serves as the mutability annotation)
5. WHEN a function parameter has a `@mut` decorator without `var`, THE Parser SHALL accept the parameter without error (the decorator serves as the mutability annotation)
6. WHEN the Parser reports a missing mutability annotation error on a parameter, THE Parser SHALL include the source line number and column of the parameter name in the error message

### Requirement 74: Mandatory Mutability Annotation on Enum Variant Fields

**User Story:** As a Meld developer, I want the compiler to require `val` or `var` on enum variant associated data fields, so that mutability is explicit across all declaration contexts.

#### Acceptance Criteria

1. WHEN an enum variant associated data field has the `val` keyword, THE Parser SHALL parse the field as immutable with `is_mutable = false` and `has_explicit_val = true`
2. WHEN an enum variant associated data field has the `var` keyword, THE Parser SHALL parse the field as mutable with `is_mutable = true`
3. IF an enum variant associated data field has neither `val` nor `var` keyword, THEN THE Parser SHALL report an error with the message "Missing mutability annotation: use 'val' for immutable or 'var' for mutable"

### Requirement 75: Error Recovery After Missing Mutability Annotation

**User Story:** As a Meld developer, I want the parser to continue parsing after a missing mutability annotation error, so that I can see all errors in a single compilation pass.

#### Acceptance Criteria

1. WHEN the Parser encounters a bare declaration (no `val` or `var`), THE Parser SHALL report the error and continue parsing the declaration as immutable (treating the missing annotation as `val` for recovery purposes)
2. THE Parser SHALL report all missing mutability annotation errors in a single parse pass rather than stopping at the first error

### Requirement 76: Compiler Parser Mandatory Annotation Enforcement

**User Story:** As a Meld developer, I want the compiler-level parser to also enforce mandatory `val`/`var` annotations, so that enforcement is consistent across both parser implementations.

#### Acceptance Criteria

1. WHEN the Compiler_Parser encounters a struct or class field without `val` or `var`, THE Compiler_Parser SHALL report an error with the message "Missing mutability annotation: use 'val' for immutable or 'var' for mutable"
2. WHEN the Compiler_Parser encounters a function parameter without `val`, `var`, or a mutability decorator, THE Compiler_Parser SHALL report an error with the same message
3. THE Compiler_Parser SHALL produce identical kernel S-expressions for `val`-annotated declarations and recovered bare declarations (both map to immutable)

### Requirement 77: Pretty Printer Mandatory Annotation Output

**User Story:** As a Meld developer, I want the pretty printer to always emit `val` or `var` on every declaration, so that formatted output conforms to the mandatory annotation rule.

#### Acceptance Criteria

1. THE Pretty_Printer SHALL emit the `val` keyword for every immutable field, property, and parameter declaration
2. THE Pretty_Printer SHALL emit the `var` keyword for every mutable field, property, and parameter declaration
3. THE Pretty_Printer SHALL produce output where no bare declaration exists (every declaration has an explicit mutability annotation)
4. FOR ALL valid Meld source files, parsing then pretty-printing then parsing SHALL produce an equivalent AST (round-trip property)

---

> **Note:** Requirements 78–87 below were merged from the retired `.kiro/specs/stdlib-lowercase-naming/` spec. They cover migrating all standard library components from PascalCase to lowercase naming.

### Requirement 78: Lowercase Built-In Primitive Types

**User Story:** As a Meld developer, I want built-in primitive types to use lowercase names, so that the naming convention matches C++ standard library style.

#### Acceptance Criteria

1. WHEN the Type_Registry is initialized, THE Type_Registry SHALL register the following types with lowercase names: `int` (was `Int`), `bool` (was `Bool`), `string` (was `String`), `unit` (was `Unit`), `symbol` (was `Symbol`), `function` (was `Function`), `null` (was `Null`)
2. WHEN the Type_Registry accessor methods `get_int_type()`, `get_bool_type()`, `get_string_type()`, `get_unit_type()`, and `get_null_type()` are called, THE Type_Registry SHALL return types whose `name()` method returns the lowercase form
3. WHEN the Compiler encounters a lowercase primitive type name in Meld source code, THE Compiler SHALL resolve the type correctly

### Requirement 79: Lowercase Copyable Trait

**User Story:** As a Meld developer, I want the built-in `Copyable` trait to be named `copyable`, so that trait naming is consistent with the lowercase convention.

#### Acceptance Criteria

1. WHEN the Type_Registry is initialized, THE Type_Registry SHALL register the `Copyable` trait under the name `copyable`
2. WHEN the Compiler encounters `copyable` as a trait bound in Meld source code, THE Compiler SHALL resolve the trait correctly

### Requirement 80: Lowercase Type Projection Utilities

**User Story:** As a Meld developer, I want type projection utilities to use lowercase names, so that utility type usage follows the same convention as primitive types.

#### Acceptance Criteria

1. THE Compiler SHALL recognize the following lowercase type projection names in Meld source code: `omit` (was `Omit`), `pick` (was `Pick`), `partial` (was `Partial`), `required` (was `Required`), `readonly` (was `Readonly`)
2. WHEN a Meld developer uses a lowercase type projection name in a type expression, THE Compiler SHALL apply the corresponding type transformation correctly

### Requirement 81: Lowercase Effect Traits

**User Story:** As a Meld developer, I want effect traits to use lowercase names, so that effect annotations are consistent with the lowercase convention.

#### Acceptance Criteria

1. THE Stdlib_Module files SHALL define effect traits with lowercase names: `console` (was `Console`), `file_system` (was `FileSystem`), `network` (was `Network`), `random` (was `Random`), `time` (was `Time`)
2. WHEN a Meld developer uses `@uses(console)` or similar lowercase effect annotations, THE Compiler SHALL resolve the effect trait correctly
3. THE Stdlib_Module file names SHALL use lowercase naming: `console.meld` (was `Console.meld`), `file_system.meld` (was `FileSystem.meld`), `network.meld` (was `Network.meld`), `random.meld` (was `Random.meld`), `time.meld` (was `Time.meld`)

### Requirement 82: Lowercase Generic Container Types

**User Story:** As a Meld developer, I want generic container types to use lowercase names, so that collection usage follows C++ standard library conventions.

#### Acceptance Criteria

1. THE Compiler SHALL recognize the following lowercase collection type names in Meld source code: `optional` (was `Option`), `result` (was `Result`), `mutable_list` (was `MutableList`), `mutable_map` (was `MutableMap`), `mutable_set` (was `MutableSet`), `persistent_list` (was `PersistentList`), `persistent_map` (was `PersistentMap`), `persistent_set` (was `PersistentSet`), `lazy_sequence` (was `LazySequence`), `collection` (was `Collection`)
2. WHEN a Meld developer declares a variable with a lowercase container type name, THE Compiler SHALL resolve the generic type correctly
3. THE Stdlib_Module files `optional.meld` (was `Option.meld`) and `result.meld` (was `Result.meld`) SHALL define their types using lowercase names

### Requirement 83: Lowercase Refinement Types

**User Story:** As a Meld developer, I want refinement types to use lowercase names, so that all type names in the standard library follow a uniform convention.

#### Acceptance Criteria

1. WHEN `register_builtin_types()` is called, THE Type_Registry SHALL register refinement types with lowercase names: `positive_int` (was `PositiveInt`), `non_empty_string` (was `NonEmptyString`), `email` (was `Email`), `valid_char` (was `ValidChar`), `negative_int` (was `NegativeInt`), `non_zero_int` (was `NonZeroInt`), `even_int` (was `EvenInt`), `odd_int` (was `OddInt`), `percentage` (was `Percentage`), `normalized_float` (was `NormalizedFloat`), `positive_float` (was `PositiveFloat`), `url` (was `URL`), `phone_number` (was `PhoneNumber`), `alphanumeric_string` (was `AlphanumericString`), `uppercase_string` (was `UppercaseString`), `lowercase_string` (was `LowercaseString`), `trimmed_string` (was `TrimmedString`), `hex_string` (was `HexString`), `base64_string` (was `Base64String`), `positive_even_int` (was `PositiveEvenInt`), `valid_email_or_phone` (was `ValidEmailOrPhone`), `short_non_empty_string` (was `ShortNonEmptyString`)
2. WHEN a Meld developer uses a lowercase refinement type name in source code, THE Compiler SHALL resolve the refinement type and apply its validation constraints correctly

### Requirement 84: Lowercase Numeric Wrapper Type

**User Story:** As a Meld developer, I want the `Buffer` numeric wrapper type to use a lowercase name, so that it is consistent with the already-lowercase `u8`, `u16`, `u32`, `u64`, `i8`, `i16`, `char` types.

#### Acceptance Criteria

1. WHEN `register_all_types()` is called for numeric wrappers, THE Type_Registry SHALL register the buffer type under the name `buffer` (was `Buffer`)

### Requirement 85: Lowercase Stdlib Module Files

**User Story:** As a Meld developer, I want stdlib `.meld` files to use lowercase file names, so that file naming matches the lowercase type convention.

#### Acceptance Criteria

1. THE Stdlib_Module directory SHALL contain files with lowercase names: `error_trait.meld` (was `ErrorTrait.meld`), `error_propagation.meld` (was `ErrorPropagation.meld`), `system_out.meld` (was `System.out.meld`)
2. WHEN the Compiler or module resolver loads a stdlib module by name, THE Compiler SHALL locate the module using the new lowercase file name

### Requirement 86: Lowercase Constructor Functions for Optional and Result

**User Story:** As a Meld developer, I want the `Some`, `None`, `Ok`, and `Err` constructor functions to use lowercase names, so that constructors follow the same lowercase convention as all other standard library components.

#### Acceptance Criteria

1. THE Compiler SHALL recognize `some` (was `Some`), `none` (was `None`), `ok` (was `Ok`), and `err` (was `Err`) as constructor functions with lowercase names
2. WHEN a Meld developer writes `some(42)`, THE Compiler SHALL return a value of type `optional[int]`
3. WHEN a Meld developer writes `ok(value)`, THE Compiler SHALL return a value of type `result[T, E]`

### Requirement 87: Lowercase Naming in Examples and Documentation

**User Story:** As a Meld developer reading examples, I want all example code to use the new lowercase type names, so that examples serve as correct references for the current naming convention.

#### Acceptance Criteria

1. THE example `.meld` files in `meld-core/examples/` SHALL use lowercase type names for all standard library types
2. THE example `.cpp` files in `meld-core/examples/` SHALL use lowercase type name strings when referencing Meld types in string literals or parser calls
3. WHEN an example file references a standard library type, THE example file SHALL use the new lowercase name rather than the deprecated PascalCase name

---

> **Note:** Requirements 88–98 below were merged from the retired `.kiro/specs/generic-syntax-migration/` spec. They cover migrating Meld's generic syntax from angle brackets (`<>`) to unified square brackets (`[]`).

### Requirement 88: Parse Generic Type Arguments with Square Brackets

**User Story:** As a Meld developer, I want to write generic types using square brackets (e.g., `Result[int, string]`), so that the syntax is consistent with array indexing and avoids angle bracket ambiguity.

#### Acceptance Criteria

1. WHEN a type annotation contains a type name followed by `[` in a Type_Position, THE Type_Annotation_Parser SHALL parse the contents between `[` and `]` as a comma-separated list of Generic_Type_Arguments and produce a `type_annotation` AST node with `has_type_arguments = true`
2. WHEN a type annotation contains nested generic types (e.g., `result[optional[int], string]`), THE Type_Annotation_Parser SHALL correctly parse all nesting levels and produce a correctly nested AST
3. WHEN a type annotation contains zero type arguments (empty brackets `[]`), THE Type_Annotation_Parser SHALL report a parse error indicating that generic type arguments are required
4. IF the closing `]` is missing in a generic type annotation, THEN THE Parser SHALL report a descriptive error message indicating the expected `]`

### Requirement 89: Parse Generic Type Parameter Declarations with Square Brackets

**User Story:** As a Meld developer, I want to declare generic type parameters using square brackets (e.g., `struct optional[T]`, `enum result[T, E]`), so that declarations match the instantiation syntax.

#### Acceptance Criteria

1. WHEN a struct, class, enum, or function definition includes `[` after the name, THE Generic_Parameter_Parser SHALL parse the contents as a comma-separated list of Generic_Type_Parameters with optional variance annotations and bounds
2. WHEN a generic type parameter includes a bound (e.g., `T: Comparable`), THE Generic_Parameter_Parser SHALL parse the bound as a type annotation and store it in the `generic_type_parameter` AST node
3. WHEN a generic type parameter includes a variance annotation (e.g., `out T`, `in T`), THE Generic_Parameter_Parser SHALL parse the variance and store it in the `generic_type_parameter` AST node
4. IF the closing `]` is missing in a generic type parameter list, THEN THE Parser SHALL report a descriptive error message indicating the expected `]`

### Requirement 90: Disambiguate Square Brackets Between Generics and Indexing

**User Story:** As a Meld developer, I want the parser to correctly distinguish `foo[0]` (array indexing) from `Result[T, E]` (generic type), so that both features work with the same bracket syntax.

#### Acceptance Criteria

1. WHEN `[` follows an identifier in a Type_Position (parameter type, return type, type alias target, field type), THE Parser SHALL interpret the brackets as generic type arguments
2. WHEN `[` follows an expression in an Expression_Position (right side of assignment, function call argument, standalone statement), THE Parser SHALL interpret the brackets as array indexing
3. THE Parser SHALL use syntactic context (Type_Position vs Expression_Position) as the sole mechanism for Disambiguation, requiring no lookahead beyond the current parsing context
4. WHEN a type name is used in an Expression_Position with brackets (e.g., `Result[int, string]` as a constructor call), THE Parser SHALL treat the brackets as a generic type instantiation when the identifier refers to a known type name

### Requirement 91: Migration Error Messages for Angle Bracket Generics

**User Story:** As a Meld developer migrating existing code, I want clear error messages when I accidentally use `<>` for generics, so that I can quickly update my code to the new syntax.

#### Acceptance Criteria

1. WHEN the Parser encounters `<` immediately after a type name in a Type_Position, THE Parser SHALL emit an error message that suggests using `[]` instead of `<>` for generic type arguments
2. WHEN the Generic_Parameter_Parser encounters `<` after a struct, class, enum, or function name where generic parameters are expected, THE Parser SHALL emit an error message that suggests using `[]` instead of `<>` for generic type parameter declarations
3. THE Parser SHALL include the specific location (line and column) in migration error messages

### Requirement 92: AST Representation for Square Bracket Generics

**User Story:** As a compiler developer, I want the AST to correctly represent square bracket generic syntax, so that downstream compiler passes can process generics consistently.

#### Acceptance Criteria

1. THE AST `type_annotation` struct SHALL store generic type arguments parsed from `[]` syntax in the existing `type_arguments` vector with `has_type_arguments` set to true
2. THE AST `generic_type_parameter` struct SHALL represent type parameters parsed from `[]` syntax with no changes to the struct fields (name, variance, has_bound, bound)
3. WHEN the AST_Printer formats a type annotation with generic type arguments, THE AST_Printer SHALL output the type using `[]` syntax (e.g., `Result[int, string]`)
4. WHEN the AST_Printer formats a generic type parameter declaration, THE AST_Printer SHALL output the parameters using `[]` syntax (e.g., `[T, E]`)

### Requirement 93: Type Checker for Square Bracket Generics

**User Story:** As a compiler developer, I want the type checker to resolve generic types parsed from `[]` syntax, so that type checking works correctly with the new syntax.

#### Acceptance Criteria

1. WHEN the Type_Checker encounters a `type_annotation` with `has_type_arguments = true`, THE Type_Checker SHALL resolve the generic type arguments and perform bounds checking using the same logic as before the syntax migration
2. WHEN the Type_Checker substitutes generic type parameters during type instantiation, THE Type_Checker SHALL produce correct substitutions regardless of the source syntax used
3. THE Type_Checker SHALL report type errors for generic types using the `[]` notation in error messages (e.g., "Expected result[int, string] but found optional[int]")

### Requirement 94: Backend Code Generators for Square Bracket Generics

**User Story:** As a compiler developer, I want all backend code generators to correctly map `[]` generic syntax to the target language's generic syntax, so that generated code compiles in the target language.

#### Acceptance Criteria

1. WHEN the Cpp_Backend generates C++ code for a Meld generic type, THE Cpp_Backend SHALL output C++ angle bracket syntax (e.g., Meld `Result[int, string]` becomes C++ `Result<int, string>`)
2. WHEN the Jvm_Backend generates JVM bytecode or Java source for a Meld generic type, THE Jvm_Backend SHALL output Java angle bracket syntax (e.g., Meld `optional[int]` becomes Java `Optional<Integer>`)
3. WHEN the Wasm_Backend generates WASM code for a Meld generic type, THE Wasm_Backend SHALL apply the appropriate monomorphization or type erasure strategy
4. WHEN any Backend encounters a generic type in IR, THE Backend SHALL produce valid target language output regardless of the original Meld source syntax

### Requirement 95: Standard Library Square Bracket Syntax

**User Story:** As a Meld developer, I want the standard library types (`optional`, `result`, etc.) to use `[]` syntax in their definitions and documentation, so that the stdlib is consistent with the new syntax.

#### Acceptance Criteria

1. THE stdlib file `optional.meld` SHALL use `[]` syntax for all generic type parameters and arguments (e.g., `struct optional[T]`, `fnc map[U](f: (T) -> U) -> optional[U]`)
2. THE stdlib file `error_propagation.meld` SHALL use `[]` syntax for all generic type parameters and arguments (e.g., `result[int, string]`, `optional[string]`)
3. THE stdlib file `error_trait.meld` SHALL use `[]` syntax for all generic type parameters and arguments
4. WHEN a stdlib function signature uses generic types, THE stdlib definition SHALL use `[]` syntax consistently in parameter types, return types, and bounds

### Requirement 96: Examples with Square Bracket Syntax

**User Story:** As a Meld developer learning the language, I want all examples to use the `[]` generic syntax, so that I learn the correct, current syntax.

#### Acceptance Criteria

1. THE `.meld` example files in `meld-core/examples/` SHALL use `[]` syntax for all generic type parameters and arguments
2. THE `.cpp` example files in `meld-core/examples/` SHALL use `[]` syntax in any embedded Meld code strings passed to the parser
3. WHEN an example demonstrates generic types (e.g., `result-demo.meld`, `error-handling-demo.meld`), THE example SHALL use `[]` syntax exclusively
4. THE example file pairs SHALL maintain the required structure: separate `.cpp` (C++ test harness) and `.meld` (pure Meld code) files for each example

### Requirement 97: Tests for Square Bracket Syntax

**User Story:** As a compiler developer, I want all tests to validate the `[]` generic syntax, so that the test suite verifies the new syntax works correctly.

#### Acceptance Criteria

1. THE generics integration tests in `meld-core/tests/compiler/generics_integration_test.cpp` SHALL construct `generic_type_parameter` AST nodes and validate type checking using the same struct fields
2. THE generics type tests in `meld-core/tests/types/generics_test.cpp` SHALL validate generic type resolution with the `[]` syntax
3. WHEN a test constructs Meld source code as a string for parsing, THE test SHALL use `[]` syntax for generics
4. THE error handling property tests SHALL use `[]` syntax for `result[T, E]` and `optional[T]` types in any Meld source strings

### Requirement 98: Round-Trip Property for Generic Syntax

**User Story:** As a compiler developer, I want to verify that parsing and pretty-printing generic types with `[]` syntax is lossless, so that tooling (formatters, refactoring tools) preserves code correctly.

#### Acceptance Criteria

1. FOR ALL valid Meld type annotations containing generic type arguments with `[]` syntax, parsing the type annotation and then pretty-printing it with the AST_Printer SHALL produce a string that, when parsed again, yields an equivalent AST (round-trip property)
2. FOR ALL valid Meld generic type parameter declarations with `[]` syntax, parsing the declaration and then pretty-printing it SHALL produce a string that, when parsed again, yields an equivalent AST
3. THE round-trip property SHALL hold for nested generics (e.g., `result[optional[int], string]`), multiple type parameters, variance annotations, and bounded type parameters

---

> **Note:** Requirements 99–118 below were merged from the former `effects-annotations` spec. They cover the transformation of Meld's algebraic effects system from keyword-based to annotation-based syntax, the effect query API, resource resolution, implicit effect calls, and migration tooling.

### Requirement 99: Effect Definition via Annotation

**User Story:** As a developer, I want to define effects using annotations instead of keywords, so that the language remains consistent with its no-keyword philosophy.

#### Acceptance Criteria

1. THE Language Runtime SHALL support @effect annotation for defining abstract effect interfaces
2. THE @effect annotation SHALL be applied to trait definitions
3. WHEN @effect is applied to a trait, THE MMS SHALL generate the necessary effect infrastructure
4. THE @effect annotation SHALL NOT introduce new keywords into the language
5. THE effect keyword SHALL be removed from the language
6. THE Language Runtime SHALL support effect operations as trait method declarations

### Requirement 100: Effect Declaration via @uses Annotation

**User Story:** As a developer, I want to declare which effects a function performs using annotations, so that effect tracking remains explicit without keyword clutter.

#### Acceptance Criteria

1. THE Language Runtime SHALL support @uses annotation for declaring function effects
2. THE @uses annotation SHALL accept a list of effect types as parameters
3. THE @uses annotation SHALL be applied to function declarations
4. WHEN @uses is applied, THE MMS SHALL validate that the function only performs declared effects
5. THE imposes keyword SHALL be removed from the language
6. THE @uses annotation SHALL support multiple effects: @uses(file_system, network)
7. THE @uses annotation SHALL support single effects: @uses(file_system)

### Requirement 101: Effect Performance via Library Function

**User Story:** As a developer, I want to invoke effects using a library function instead of a keyword, so that effect invocation follows standard function call syntax.

#### Acceptance Criteria

1. THE Standard Library SHALL provide perform() function for invoking effect operations
2. THE perform() function SHALL accept an effect operation as a lambda or method reference
3. THE perform keyword SHALL be removed from the language
4. WHEN perform() is called, THE Language Runtime SHALL search the call stack for an appropriate handler
5. THE perform() function SHALL support syntax: perform { EffectName.operation(args) }
6. THE perform() function SHALL support syntax: perform(EffectName::operation, args)
7. THE perform() function SHALL return the value provided by the effect handler

### Requirement 102: Effect Handling via Library Function

**User Story:** As a developer, I want to handle effects using library functions instead of keywords, so that effect handling follows standard functional patterns.

#### Acceptance Criteria

1. THE Standard Library SHALL provide handle() function for intercepting effect operations
2. THE handle() function SHALL accept a computation block and handler configuration
3. THE handle keyword SHALL be removed from the language
4. THE with keyword SHALL be removed from the language
5. THE handle() function SHALL support syntax: handle({ computation }, EffectName { fnc op() { ... } })
6. THE handle() function SHALL support multiple handlers in a single call
7. WHEN handle() is called, THE Language Runtime SHALL intercept effect operations in the computation block

### Requirement 103: Effect Resumption via Library Function

**User Story:** As a developer, I want to resume execution from effect handlers using a library function, so that continuation control follows standard function call syntax.

#### Acceptance Criteria

1. THE Standard Library SHALL provide resume() function for continuing execution after handling
2. THE resume() function SHALL accept an optional return value
3. THE resume keyword SHALL be removed from the language
4. THE resume() function SHALL support syntax: resume() for void operations
5. THE resume() function SHALL support syntax: resume(value) for operations with return values
6. WHEN resume() is called, THE Language Runtime SHALL continue execution with the provided value

### Requirement 104: Effects Backward Compatibility and Migration

**User Story:** As a developer with existing effect-based code, I want clear migration guidance, so that I can update my code to use the new annotation-based syntax.

#### Acceptance Criteria

1. THE Compiler SHALL provide deprecation warnings for keyword-based effect syntax
2. THE Compiler SHALL suggest annotation-based replacements in deprecation warnings
3. THE Documentation SHALL provide a migration guide from keywords to annotations
4. THE Migration Guide SHALL include before/after examples for all effect patterns
5. THE Compiler SHALL support a --strict-no-keywords flag that rejects keyword-based syntax
6. THE Compiler SHALL provide an automated migration tool for converting keyword syntax to annotations

### Requirement 105: System.out Integration

**User Story:** As a developer, I want System.out to work seamlessly with the annotation-based effects system, so that console output remains consistent with the new syntax.

#### Acceptance Criteria

1. THE System.out.println function SHALL use @uses(console) annotation
2. THE System.out.print function SHALL use @uses(console) annotation
3. THE System.out functions SHALL use perform() function internally
4. THE Console effect SHALL be defined using @effect annotation
5. WHEN System.out functions are called, THE Language Runtime SHALL use the annotation-based effect system

### Requirement 106: Built-in Effects Migration

**User Story:** As a developer, I want all built-in effects to use the annotation-based syntax, so that the standard library is consistent with the language philosophy.

#### Acceptance Criteria

1. THE file_system effect SHALL be defined using @effect annotation
2. THE network effect SHALL be defined using @effect annotation
3. THE time effect SHALL be defined using @effect annotation
4. THE random effect SHALL be defined using @effect annotation
5. THE console effect SHALL be defined using @effect annotation
6. ALL built-in effect operations SHALL use perform() function
7. ALL built-in effect handlers SHALL use handle() function

### Requirement 107: Effects Documentation and Examples

**User Story:** As a developer learning MELD, I want comprehensive documentation of the annotation-based effects system, so that I can understand and use effects effectively.

#### Acceptance Criteria

1. THE Documentation SHALL provide a complete guide to annotation-based effects
2. THE Documentation SHALL include examples of effect definition with @effect
3. THE Documentation SHALL include examples of effect declaration with @uses
4. THE Documentation SHALL include examples of effect performance with perform()
5. THE Documentation SHALL include examples of effect handling with handle()
6. THE Documentation SHALL include examples of effect resumption with resume()
7. THE Documentation SHALL explain the rationale for annotation-based syntax
8. THE Documentation SHALL provide AI safety examples using the new syntax

### Requirement 108: Effects Parser and Compiler Updates

**User Story:** As a language implementer, I want the parser and compiler to support annotation-based effects, so that the new syntax is fully integrated into the language toolchain.

#### Acceptance Criteria

1. THE Parser SHALL recognize @effect annotation on trait definitions
2. THE Parser SHALL recognize @uses annotation on function declarations
3. THE Parser SHALL parse perform() as a standard library function call
4. THE Parser SHALL parse handle() as a standard library function call
5. THE Parser SHALL parse resume() as a standard library function call
6. THE Compiler SHALL validate @uses annotations against actual effect usage
7. THE Compiler SHALL generate appropriate error messages for effect-related violations
8. THE Compiler SHALL support the annotation-based syntax in all compilation modes

### Requirement 109: Effect Query API (`query_required_effects`)

**User Story:** As a tool developer (daemon, MCP server, or CLI), I want a semantic API that computes the complete set of effects required by an entry point via transitive call-graph traversal, so that I can generate accurate sandbox policies and provide effect diagnostics without reimplementing the traversal logic.

#### Acceptance Criteria

1. THE `libmeld_sema` library SHALL expose a `query_required_effects(entry_point) -> EffectQueryResult` API that accepts an AST node reference (function, method, or module entry point) and returns the transitive closure of all effects reachable from that entry point
2. THE traversal SHALL follow function calls through the call graph, mapping intrinsic functions (`io.print`, `file.open`, `net.connect`, etc.) to `core::Effect` enum values via the Intrinsic_Effect_Map
3. THE traversal SHALL use a "visited" set to handle cycles in recursive or mutually recursive call graphs, ensuring termination
4. WHEN the traversal encounters a function with a locked `@uses(...)` annotation, THE API SHALL use the declared effects as the function's contribution (trust the contract) rather than traversing into the function body
5. WHEN the traversal encounters a function without `@uses(...)` that calls an effectful function, THE API SHALL report an "Effect Leak" diagnostic identifying the call site and the missing effect
6. THE `EffectQueryResult` SHALL include: the set of `core::Effect` values, the call chain for each effect (for diagnostic tracing), and any Effect Leak errors encountered during traversal

> **Cross-reference:** The compiler's effect inference pass (`.kiro/specs/meld-compiler/requirements.md` Req 5) performs a similar traversal at compile time. `query_required_effects` is the on-demand query interface to the same analysis, used by the daemon and MCP tools at runtime.

### Requirement 110: EffectRequirement Structure and Resource Resolution

**User Story:** As a daemon or sandbox subsystem, I want the effect query to produce a resolved `EffectRequirement` that combines code-level effects with `meld.toml` resource constraints, so that I can generate a precise sandbox policy with specific domains and paths rather than just abstract effect categories.

#### Acceptance Criteria

1. THE `libmeld_sema` library SHALL define an `EffectRequirement` structure containing: `required_capabilities` (set of `core::Effect` values), `allowed_domains` (list of permitted network domains), `allowed_read_paths` (list of filesystem paths for read access), and `allowed_write_paths` (list of filesystem paths for write access)
2. THE `libmeld_sema` library SHALL expose a `resolve_requirements(entry_point, meld_toml) -> EffectRequirement` API that combines the static effects from `query_required_effects` with the dynamic resource constraints declared in `meld.toml`
3. WHEN `meld.toml` declares `allowed_domains` for a dependency with `Effect::Network`, THE resolver SHALL populate `EffectRequirement.allowed_domains` with those domains
4. WHEN `meld.toml` declares `allowed_paths` for a dependency with `Effect::FileSystemRead` or `Effect::FileSystemWrite`, THE resolver SHALL populate the corresponding path lists in the `EffectRequirement`
5. WHEN a dependency's `meld.toml` constraints are more restrictive than the code's declared effects, THE resolver SHALL use the more restrictive (intersection) policy
6. THE `EffectRequirement` SHALL be the input consumed by the `SandboxProvider` (see `.kiro/specs/meld-manifest/requirements.md` Req 10) to generate OS-level sandbox configurations

> **Cross-reference:** The `SandboxProvider` interface in `.kiro/specs/meld-manifest/requirements.md` consumes `EffectRequirement` to generate `SandboxConfig`. The daemon (`.kiro/specs/meld-daemon/requirements.md` Req 5) calls `resolve_requirements` before each sandboxed execution.

### Requirement 111: Implicit Effect Call Recognition

**User Story:** As a Meld developer, I want to write `FileSystem.read(path)` instead of `perform { FileSystem.read(path) }`, so that effect operations look like regular function calls and reduce syntactic noise.

#### Acceptance Criteria

1. WHEN the Parser encounters an expression of the form `Identifier.Identifier(args)` where the first identifier matches a registered Effect_Definition name, THE Parser SHALL produce an Implicit_Effect_Call AST node
2. WHEN the Parser encounters an expression of the form `Identifier.Identifier(args)` where the first identifier does not match any registered Effect_Definition name, THE Parser SHALL produce a standard member access or method call AST node
3. THE Parser SHALL resolve ambiguity between Implicit_Effect_Calls and regular method calls by consulting the set of known Effect_Definition names available in the current compilation unit
4. WHEN an Implicit_Effect_Call references an operation not defined on the matched Effect_Definition, THE Effect_Checker SHALL report a compile-time error identifying the unknown operation and the Effect_Definition

### Requirement 112: Effect Checker Validation of Implicit Calls

**User Story:** As a Meld developer, I want the compiler to validate that implicit effect calls match my `@uses` declarations, so that I get clear errors when I use an effect without declaring it.

#### Acceptance Criteria

1. WHEN a function body contains an Implicit_Effect_Call for an effect listed in the function's Uses_Annotation, THE Effect_Checker SHALL accept the call as valid
2. WHEN a function body contains an Implicit_Effect_Call for an effect not listed in the function's Uses_Annotation, THE Effect_Checker SHALL report a compile-time error stating the missing effect and suggesting the correct Uses_Annotation
3. WHEN a function has no Uses_Annotation and its body contains an Implicit_Effect_Call, THE Effect_Checker SHALL report a compile-time error indicating that a Uses_Annotation is required
4. THE Effect_Checker SHALL propagate Implicit_Effect_Call effects through the call graph using the same rules applied to Explicit_Effect_Calls

### Requirement 113: Code Generation for Implicit Effect Calls

**User Story:** As a Meld developer, I want implicit effect calls to produce the same runtime behavior as explicit `perform` blocks, so that removing `perform` is purely a syntactic change with no semantic difference.

#### Acceptance Criteria

1. WHEN the Code_Generator encounters an Implicit_Effect_Call, THE Code_Generator SHALL emit the same `primitive_suspend` and continuation machinery as for an equivalent Explicit_Effect_Call
2. FOR ALL valid Meld programs, replacing every `perform { Effect.op(args) }` with `Effect.op(args)` SHALL produce a program with identical runtime behavior
3. WHEN an Implicit_Effect_Call appears in a value binding (e.g., `val x = Effect.op(args)`), THE Code_Generator SHALL correctly thread the continuation so that `x` receives the resumed value

### Requirement 114: Backward Compatibility with Explicit Perform Syntax

**User Story:** As a Meld developer with existing code, I want `perform { ... }` blocks to continue working during a migration period, so that I can adopt the new syntax incrementally.

#### Acceptance Criteria

1. THE Parser SHALL continue to accept `perform { Effect.op(args) }` as a valid Explicit_Effect_Call
2. WHEN a function body mixes Implicit_Effect_Calls and Explicit_Effect_Calls for the same effect, THE Effect_Checker SHALL accept both forms without error
3. WHEN the compiler encounters an Explicit_Effect_Call, THE Effect_Checker SHALL emit a deprecation warning recommending the implicit syntax
4. THE Code_Generator SHALL produce identical output for semantically equivalent Implicit_Effect_Calls and Explicit_Effect_Calls

### Requirement 115: Effect Inference Integration for Implicit Calls

**User Story:** As a Meld developer, I want the automatic effect inference system to detect implicit effect calls, so that IDE ghost text and annotation suggestions remain accurate.

#### Acceptance Criteria

1. WHEN the Effect_Checker infers effects from a function body containing Implicit_Effect_Calls, THE Effect_Checker SHALL include those effects in the inferred set
2. WHEN the Effect_Checker generates a Uses_Annotation suggestion for a function with Implicit_Effect_Calls, THE Effect_Checker SHALL list all effects referenced by those calls
3. WHEN the Effect_Checker propagates effects through the call graph, THE Effect_Checker SHALL treat Implicit_Effect_Calls identically to Explicit_Effect_Calls
4. WHEN the LSP_Server provides ghost text for a function with Implicit_Effect_Calls, THE LSP_Server SHALL display the inferred Uses_Annotation reflecting those effects

### Requirement 116: Handler Block Compatibility with Implicit Calls

**User Story:** As a Meld developer, I want handler blocks to intercept implicit effect calls the same way they intercept explicit perform blocks, so that effect handling semantics are unchanged.

#### Acceptance Criteria

1. WHEN a Handler_Block wraps a computation containing Implicit_Effect_Calls, THE Handler_Block SHALL intercept those calls using the same mechanism as for Explicit_Effect_Calls
2. WHEN a handler provides a `resume` value for an intercepted Implicit_Effect_Call, THE Code_Generator SHALL deliver that value to the call site as the return value of the Implicit_Effect_Call
3. WHEN nested Handler_Blocks handle different effects, THE Code_Generator SHALL route each Implicit_Effect_Call to the correct handler based on the effect name

### Requirement 117: Error Reporting for Implicit Effect Calls

**User Story:** As a Meld developer, I want clear, actionable error messages when I misuse implicit effect calls, so that I can fix problems quickly.

#### Acceptance Criteria

1. WHEN an Implicit_Effect_Call references an effect not defined in any visible Effect_Definition, THE Effect_Checker SHALL report an error including the unrecognized effect name and a list of available Effect_Definitions
2. WHEN an Implicit_Effect_Call passes arguments that do not match the operation's parameter types, THE Effect_Checker SHALL report a type mismatch error with expected and actual types
3. WHEN an Implicit_Effect_Call is used outside any function (e.g., at module top level), THE Effect_Checker SHALL report an error stating that effect calls require a function context with a Uses_Annotation
4. IF a function's Uses_Annotation lists an effect that is never used in the function body (neither implicitly nor explicitly), THEN THE Effect_Checker SHALL emit a warning about the unused effect declaration

### Requirement 118: Example Migration to Implicit Effect Syntax

**User Story:** As a Meld developer reading examples, I want all `.meld` example files to use the new implicit syntax, so that documentation reflects the current recommended style.

#### Acceptance Criteria

1. THE example `.meld` files SHALL use Implicit_Effect_Call syntax (`Effect.op(args)`) instead of Explicit_Effect_Call syntax (`perform { Effect.op(args) }`)
2. THE example `.meld` files SHALL retain all existing `@uses(...)` annotations
3. THE example `.cpp` test harness files SHALL remain unchanged in structure, with updates only to expected output strings if the output changes
4. FOR ALL migrated example files, compiling and running the example SHALL produce the same observable output as before migration

---

> **Note:** Requirements 119–129 below were merged from the former `own-link-memory-model` spec (updated to Hold/View tenancy terminology). They cover the language-level `Hold[T]`/`View[T]` library types, the `Storable` trait, compiler diagnostics for memory safety, `std.mem.move()`, collection iteration, effect handler ARC interaction, and C++20 concept backend enforcement.

### Requirement 119: Hold[T] Library Type

**User Story:** As a Meld developer, I want a strong ownership reference type in the standard library, so that I can express owning relationships in type signatures and the compiler can guarantee object liveness.

#### Acceptance Criteria

1. THE `std.mem` module SHALL export a generic type `Hold[T]` that represents a strong owning reference to a heap-allocated object of type `T`.
2. WHEN a `Hold[T]` reference is created, THE `Hold[T]` SHALL increment the `strong_count` of the referenced object.
3. WHEN a `Hold[T]` reference goes out of scope, THE `Hold[T]` SHALL decrement the `strong_count` of the referenced object.
4. THE `Hold[T]` SHALL provide direct member access to the referenced object without requiring a nil check.
5. WHEN a class instance is assigned to a variable without an explicit wrapper (Creator Rule), THE Semantic_Analyzer SHALL infer the type as `Hold[T]`.
6. THE `Hold[T]` SHALL implement the Storable trait with `is_owning()` returning `true`.
7. IF a developer attempts to use the former name `Own[T]`, THE Semantic_Analyzer SHALL emit a migration hint: "Did you mean Hold[T]? Own[T] has been renamed to Hold[T]."

### Requirement 120: View[T] Library Type

**User Story:** As a Meld developer, I want a non-owning observation reference type in the standard library, so that I can express back-references, caches, and observer patterns without preventing deallocation.

#### Acceptance Criteria

1. THE `std.mem` module SHALL export a generic type `View[T]` that represents a non-owning observation reference to a heap-allocated object of type `T`.
2. WHEN a `View[T]` reference is created via `std.mem.view(holder)`, THE `View[T]` SHALL increment the `weak_count` of the referenced object without incrementing `strong_count`.
3. WHEN a `View[T]` reference goes out of scope, THE `View[T]` SHALL decrement the `weak_count` of the referenced object.
4. WHEN the last `Hold[T]` to an object is released, THE object SHALL be destroyed and all `View[T]` references to that object SHALL become invalidated.
5. THE `View[T]` SHALL implement the Storable trait with `is_owning()` returning `false`.
6. THE Meld grammar SHALL NOT include a `weak` keyword; non-owning reference semantics SHALL be expressed exclusively through `std.mem.View[T]`.
7. WHEN a function parameter is declared with a bare type (no explicit wrapper), THE Semantic_Analyzer SHALL infer the parameter type as `View[T]` (Guest Rule).
8. IF a developer attempts to use the former name `Link[T]`, THE Semantic_Analyzer SHALL emit a migration hint: "Did you mean View[T]? Link[T] has been renamed to View[T]."

### Requirement 121: View[T] Access Patterns

**User Story:** As a Meld developer, I want safe access patterns for View[T] references, so that I cannot accidentally dereference a dead object.

#### Acceptance Criteria

1. WHEN a developer attempts to directly access a member of a `View[T]` value without using `?.`, `if val`, or `match`, THE Semantic_Analyzer SHALL emit a compile-time error `VIEW_UNCERTAINTY`: "View[T] must be accessed via `?.` (safe navigation), `if val` (upgrade), or `match`."
2. THE `View[T]` SHALL support upgrading via `if val` syntax, where a successful upgrade binds a strong reference in the `if` body and the `else` branch handles the dead-object case.
3. THE `View[T]` SHALL support upgrading via `match` syntax with `on<some>` and `on<none>` branches.
4. WHEN a `View[T]` is successfully upgraded, THE upgrade operation SHALL temporarily increment the `strong_count` of the referenced object for the duration of the binding scope.
5. WHEN the upgraded binding goes out of scope, THE upgrade operation SHALL decrement the `strong_count` that was temporarily incremented.
6. IF the referenced object has already been destroyed (strong_count reached zero), THEN THE upgrade operation SHALL yield the `else` branch of `if val` or the `on<none>` branch of `match`.
7. THE `View[T]` SHALL support safe navigation via the `?.` operator, which compiles to a null-check guard that skips the operation (and its associated `@effect`) if the object has been deallocated.

### Requirement 122: std.mem.move() Ownership Transfer

**User Story:** As a Meld developer, I want a zero-cost ownership transfer operation, so that I can move references between bindings without unnecessary reference count increments and decrements.

#### Acceptance Criteria

1. THE `std.mem` module SHALL export a function `move(source: Hold[T]) -> Hold[T]` annotated with `@intrinsic(memory_move)` that transfers ownership of a reference without incrementing or decrementing the reference count.
2. WHEN `std.mem.move(source)` is called, THE Semantic_Analyzer SHALL statically invalidate the `source` binding so that subsequent reads or writes to `source` are compile-time errors.
3. WHEN `std.mem.move(source)` is called, THE resulting reference SHALL have the same `strong_count` as the source had before the move.
4. THE `std.mem.move()` SHALL lower to a pointer transfer (move constructor) in the C++ backend with no `retain()` or `release()` calls.
5. IF a developer reads or writes a variable after it has been moved, THEN THE Semantic_Analyzer SHALL emit a compile-time error: use of moved value.

### Requirement 123: Storable Trait and Managed Container Constraint

**User Story:** As a Meld developer, I want the compiler to enforce that only properly managed reference types can be stored in collections, so that raw class types cannot accidentally bypass memory management.

#### Acceptance Criteria

1. THE `std.mem` module SHALL define a trait `Storable` annotated with `@intrinsic(memory_strategy)` that declares `access() -> optional[ptr]` and `is_owning() -> bool` methods.
2. THE Semantic_Analyzer SHALL recognize the `@intrinsic(memory_strategy)` annotation and treat the annotated trait as the memory management strategy marker.
3. THE Semantic_Analyzer SHALL recognize the `@intrinsic(managed_container)` annotation on collection types and enforce that element type parameters implement the Storable trait.
4. WHEN a developer declares a collection with a raw class type (e.g., `List[User]`), THE Semantic_Analyzer SHALL emit a compile-time error indicating that the type does not implement Storable.
5. THE compile-time error for raw types in managed containers SHALL suggest using `Hold[T]` or `View[T]` as alternatives.
6. THE `Hold[T]` and `View[T]` SHALL be the only standard library types that implement the Storable trait.

### Requirement 124: Collection Iteration Behavior

**User Story:** As a Meld developer, I want collection iteration to behave differently based on whether elements are owned or observed, so that I get direct access for owned elements and safe handling for observed elements.

#### Acceptance Criteria

1. WHEN iterating over a `List[Hold[T]]`, THE default iterator SHALL yield `T` directly, with each element guaranteed to be non-null and alive.
2. WHEN iterating over a `List[View[T]]`, THE default iterator SHALL skip entries whose referenced objects have been deallocated, yielding only live `T` values.
3. THE `List[View[T]]` SHALL provide an `.entries()` method that returns an iterator yielding `optional[T]` for every slot, preserving the original element count including dead entries.
4. WHEN the default iterator of `List[View[T]]` skips a dead entry, THE iterator SHALL proceed to the next entry without error.
5. WHEN the `.entries()` iterator of `List[View[T]]` encounters a dead entry, THE iterator SHALL yield `none` for that slot.
6. WHEN iterating over a `Map[K, V]`, THE default iterator SHALL yield `(View[K], View[V])` tuples, enabling destructuring without unnecessary ARC increments.
7. THE `Map` and `Set` iteration order SHALL be deterministic by default in Agent-Mode to prevent flaky tests in AI-generated code.

### Requirement 125: Compiler Diagnostics for Memory Safety

**User Story:** As a Meld developer, I want clear compiler diagnostics for memory management errors and suggestions, so that I can write correct code and resolve issues quickly.

#### Acceptance Criteria

1. WHEN a developer accesses a `View[T]` value without using `?.`, `if val`, or `match`, THE Semantic_Analyzer SHALL emit an error diagnostic `VIEW_UNCERTAINTY`: "View[T] must be accessed via `?.` (safe navigation), `if val` (upgrade), or `match`."
2. WHEN a developer reads or writes a variable after `std.mem.move()` has invalidated it, THE Semantic_Analyzer SHALL emit an error diagnostic: "use of moved value".
3. WHEN a developer uses a raw class type as an element of a Managed_Container, THE Semantic_Analyzer SHALL emit an error diagnostic: "raw type in managed container — use Hold[T] or View[T]".
4. WHEN the Semantic_Analyzer detects two classes with mutual `Hold[T]` references, THE Semantic_Analyzer SHALL emit a warning diagnostic: "potential reference cycle detected".
5. WHEN the Semantic_Analyzer detects an assignment where `std.mem.move()` would eliminate an unnecessary retain/release pair, THE Semantic_Analyzer SHALL emit a warning diagnostic: "unnecessary retain/release — consider std.mem.move()".
6. WHEN the Semantic_Analyzer detects a back-reference pattern where `View[T]` would be more appropriate than `Hold[T]`, THE Semantic_Analyzer SHALL emit an info diagnostic suggesting `View[T]`.

### Requirement 126: Effect Handler ARC Interaction

**User Story:** As a Meld developer, I want effect handlers to correctly manage captured references, so that objects remain alive while handlers are active and View[T] references are still safely upgraded inside handlers.

#### Acceptance Criteria

1. WHEN an Effect_Handler captures a `Hold[T]` reference from its enclosing scope, THE runtime SHALL increment the `strong_count` of the referenced object when the handler is installed.
2. WHEN an Effect_Handler scope exits, THE runtime SHALL decrement the `strong_count` of all captured `Hold[T]` references.
3. WHEN an Effect_Handler captures a `View[T]` reference from its enclosing scope, THE `View[T]` SHALL still require accessing via `?.`, `if val`, or `match` inside the handler body.
4. WHEN a multi-shot continuation clones the stack, THE runtime SHALL clone all captured `Hold[T]` references, incrementing `strong_count` for each clone.
5. WHEN a cloned continuation is discarded, THE runtime SHALL decrement the `strong_count` of all `Hold[T]` references captured by that clone.

### Requirement 127: C++20 Concept Backend Enforcement

**User Story:** As a compiler backend developer, I want the Storable trait constraint mirrored as a C++20 Concept in the generated code, so that raw unmanaged pointers cannot enter collections at the C++ level.

#### Acceptance Criteria

1. THE C++ backend SHALL generate a `MeldStorable` concept that requires `get_ref_count() -> uint64_t` and `is_owning() -> bool` on the element type.
2. THE C++ backend SHALL generate collection class templates (e.g., `Vector`) constrained by the `MeldStorable` concept.
3. WHEN a C++ type that does not satisfy the `MeldStorable` concept is used as a collection element, THE C++ compiler SHALL emit a concept constraint violation error.
4. THE generated `MeldStorable` concept SHALL mirror the Storable trait's method signatures.

### Requirement 128: Cycle Resolution Pattern

**User Story:** As a Meld developer, I want a clear pattern for resolving reference cycles using Hold[T] and View[T], so that parent-child and observer relationships do not leak memory.

#### Acceptance Criteria

1. THE `std.mem` module SHALL support declaring `Hold[T]` for the strong (parent-to-child) direction and `View[T]` for the back-reference (child-to-parent) direction in cyclic data structures.
2. WHEN the last external `Hold[T]` to a parent object is released, THE parent object SHALL be destroyed, releasing its `Hold[T]` children, which in turn destroys the children.
3. WHEN a child object holding a `View[T]` to its destroyed parent is accessed, THE `View[T]` access SHALL yield `none` (via `?.` returning nil, or `if val`/`match` taking the else/none branch).
4. THE compiler cycle warning (Requirement 125, criterion 4) SHALL detect mutual `Hold[T]` references and suggest converting one direction to `View[T]`.

### Requirement 129: Example Demonstrating Hold[T], View[T], and std.mem.move()

**User Story:** As a developer learning Meld, I want an example that demonstrates the Hold[T]/View[T] memory model, so that I can understand ownership, observation, upgrade patterns, and move semantics.

#### Acceptance Criteria

1. THE example SHALL consist of a separate `hold-view-demo.cpp` file and a `hold-view-demo.meld` file following the meld-examples-structure guidelines.
2. THE `hold-view-demo.meld` file SHALL demonstrate creating `Hold[T]` references, creating `View[T]` references via `std.mem.view()`, accessing `View[T]` via `?.` (safe navigation) and `if val` (upgrade) and `match`, and transferring ownership via `std.mem.move()`.
3. THE `hold-view-demo.meld` file SHALL demonstrate the parent-child cycle resolution pattern using `Hold[T]` for the parent-to-child direction and `View[T]` for the child-to-parent direction.
4. THE `hold-view-demo.meld` file SHALL demonstrate collection declarations with `List[Hold[T]]` and `List[View[T]]`, including iteration behavior differences.
5. THE `hold-view-demo.cpp` file SHALL parse the `hold-view-demo.meld` file, run semantic analysis, and verify that the expected compiler diagnostics are emitted for intentional errors (direct View access without `?.`, use-after-move, raw type in container).

---

> **Note:** Requirements 130–139 below were merged from the former `meldobject-non-atomic-arc` spec. They cover the `MeldObject` unified base type with embedded non-atomic reference counting, the `MeldRef<T>` intrusive smart pointer, kernel type migration, Value type replacement, metadata migration, cycle detection, continuation migration, and backward compatibility.

### Requirement 130: MeldObject Base Class

**User Story:** As a kernel developer, I want a unified base class for all Meld kernel types, so that reference counting, type identification, and metadata are consistently managed in a single object header.

#### Acceptance Criteria

1. THE MeldObject SHALL provide a non-atomic 64-bit reference counter (`strong_count`) initialized to zero upon construction.
2. THE MeldObject SHALL provide a Type_Tag field that identifies the runtime type of the object.
3. THE MeldObject SHALL provide a virtual destructor for proper polymorphic cleanup of derived types.
4. THE MeldObject SHALL provide an inline `retain()` method that increments the reference counter by one.
5. THE MeldObject SHALL provide an inline `release()` method that decrements the reference counter by one; WHEN the counter reaches zero, `release()` SHALL first invoke any user-defined lifecycle destructor (see Requirement 156), then invoke the C++ virtual destructor, then deallocate the object (or retain the header as a tombstone if `weak_count > 0`).
6. THE MeldObject SHALL store the reference counter, Type_Tag, and metadata pointer within a contiguous object header to minimize cache misses.
7. WHEN a MeldObject is constructed, THE MeldObject SHALL set the Type_Tag to the value corresponding to the derived type.
8. THE MeldObject object header SHALL occupy a contiguous 40-byte layout: vtable pointer (8 bytes), strong reference counter (8 bytes), weak reference counter (8 bytes), TypeTag (1 byte), padding (7 bytes), metadata pointer (8 bytes).

### Requirement 131: MeldRef Intrusive Smart Pointer

**User Story:** As a kernel developer, I want an intrusive smart pointer that manages MeldObject lifetimes through the embedded counter, so that I can replace `std::shared_ptr` with a non-atomic, lower-overhead alternative.

#### Acceptance Criteria

1. WHEN a MeldRef is constructed from a raw MeldObject pointer, THE MeldRef SHALL call `retain()` on the pointed-to object.
2. WHEN a MeldRef is destroyed, THE MeldRef SHALL call `release()` on the pointed-to object.
3. WHEN a MeldRef is copy-constructed, THE MeldRef SHALL call `retain()` on the pointed-to object.
4. WHEN a MeldRef is move-constructed, THE MeldRef SHALL transfer ownership without calling `retain()` or `release()`.
5. WHEN a MeldRef is copy-assigned, THE MeldRef SHALL call `release()` on the previously held object and `retain()` on the newly assigned object.
6. WHEN a MeldRef is move-assigned, THE MeldRef SHALL call `release()` on the previously held object and transfer ownership of the new object without calling `retain()`.
7. THE MeldRef SHALL provide `operator->`, `operator*`, and `get()` for accessing the underlying MeldObject pointer.
8. THE MeldRef SHALL provide an `operator bool` that returns true when the pointer is non-null.
9. THE MeldRef SHALL be a template parameterized on a type `T` that derives from MeldObject, enabling type-safe access to derived types.
10. THE MeldRef SHALL support implicit conversion from `MeldRef<Derived>` to `MeldRef<MeldObject>` when `Derived` inherits from MeldObject.

### Requirement 132: Non-Atomic Reference Counting Safety

**User Story:** As a language designer, I want the non-atomic reference counter to be safe within the shared-nothing concurrency model, so that single-threaded performance improves without sacrificing correctness.

#### Acceptance Criteria

1. THE Non_Atomic_Reference_Counter SHALL use a plain `uint64_t` without atomic operations for increment and decrement.
2. WHILE an object is owned by a single Actor, THE Non_Atomic_Reference_Counter SHALL correctly track the number of live MeldRef instances pointing to that object.
3. IF a MeldRef is passed across Actor boundaries, THEN THE MeldRef SHALL perform a deep copy of the referenced object rather than sharing the reference.
4. THE Non_Atomic_Reference_Counter SHALL provide a debug-mode assertion that detects concurrent access from multiple threads.
5. WHEN the reference counter reaches zero, THE MeldObject SHALL invoke the destructor and deallocate the object.

### Requirement 133: Kernel Type Migration

**User Story:** As a kernel developer, I want all 14 kernel types to inherit from MeldObject, so that the Value type uses a unified object model instead of a variant of shared pointers.

#### Acceptance Criteria

1. THE Symbol class SHALL inherit from MeldObject and set its Type_Tag to `TypeTag::Symbol`.
2. THE Cell class SHALL inherit from MeldObject and set its Type_Tag to `TypeTag::Cell`.
3. THE Vec class SHALL inherit from MeldObject and set its Type_Tag to `TypeTag::Vec`.
4. THE Nil class SHALL inherit from MeldObject and set its Type_Tag to `TypeTag::Nil`.
5. THE Function class SHALL inherit from MeldObject and set its Type_Tag to `TypeTag::Function`.
6. THE Integer class SHALL inherit from MeldObject and set its Type_Tag to `TypeTag::Integer`.
7. THE Float class SHALL inherit from MeldObject and set its Type_Tag to `TypeTag::Float`.
8. THE Boolean class SHALL inherit from MeldObject and set its Type_Tag to `TypeTag::Boolean`.
9. THE String class SHALL inherit from MeldObject and set its Type_Tag to `TypeTag::String`.
10. THE Placeholder class SHALL inherit from MeldObject and set its Type_Tag to `TypeTag::Placeholder`.
11. THE Optional class SHALL inherit from MeldObject and set its Type_Tag to `TypeTag::Optional`.
12. THE Continuation class SHALL inherit from MeldObject and set its Type_Tag to `TypeTag::Continuation`.
13. THE NativeHandle class SHALL inherit from MeldObject and set its Type_Tag to `TypeTag::NativeHandle`.
14. THE NativeFunction class SHALL inherit from MeldObject and set its Type_Tag to `TypeTag::NativeFunction`.
15. WHEN any Kernel_Type is constructed, THE Kernel_Type SHALL pass its Type_Tag to the MeldObject base constructor.

### Requirement 134: Value Type Replacement

**User Story:** As a kernel developer, I want the Value type to use MeldRef instead of `std::variant<std::shared_ptr<T>...>`, so that runtime type dispatch uses the embedded Type_Tag rather than variant index.

#### Acceptance Criteria

1. THE Value class SHALL hold a single `MeldRef<MeldObject>` as its internal representation.
2. THE Value class SHALL provide a templated `is<T>()` method that checks the Type_Tag against the expected tag for type `T`.
3. THE Value class SHALL provide a templated `as<T>()` method that returns a typed pointer to the derived type after verifying the Type_Tag.
4. THE Value class SHALL provide a templated `try_as<T>()` method that returns `std::expected<T*, std::string>` for safe type access.
5. THE Value class SHALL provide factory methods `from_int()`, `from_string()`, `from_bool()`, `from_unit()`, and `from_symbol()` that create MeldRef-wrapped objects.
6. THE Value class SHALL provide `to_string()` and `is_truthy()` methods with behavior identical to the current implementation.
7. WHEN a Value is default-constructed, THE Value SHALL hold a null MeldRef.

### Requirement 135: Metadata Migration

**User Story:** As a kernel developer, I want metadata to be stored per-object in the MeldObject header instead of in a global singleton, so that metadata access is local and the global MetadataStore is eliminated.

#### Acceptance Criteria

1. THE MeldObject SHALL provide an optional metadata map accessible via `meta_set(key, value)` and `meta_get(key)` methods.
2. WHEN `meta_set` is called on a MeldObject that has no metadata, THE MeldObject SHALL lazily allocate the metadata map.
3. THE `meta_get` kernel primitive SHALL read metadata directly from the MeldObject instead of querying the global MetadataStore.
4. THE `meta_set` kernel primitive SHALL write metadata directly to the MeldObject instead of writing to the global MetadataStore.
5. THE `meta_has` kernel primitive SHALL check metadata directly on the MeldObject instead of querying the global MetadataStore.
6. WHEN a MeldObject is destroyed, THE MeldObject SHALL deallocate its metadata map.

### Requirement 136: Cycle Detection and Deallocation Sequence

**User Story:** As a kernel developer, I want a strategy for detecting and breaking reference cycles, and a well-defined deallocation sequence, so that non-atomic reference counting does not leak memory when cyclic data structures are created.

#### Acceptance Criteria

1. THE MeldRef SHALL provide a `make_weak()` method that creates a Weak_Reference from an existing MeldRef.
2. THE Weak_Reference SHALL NOT increment the reference counter of the referenced MeldObject.
3. THE Weak_Reference SHALL provide a `lock()` method that returns a MeldRef if the object is still alive, or a null MeldRef if the object has been destroyed.
4. THE MeldObject SHALL maintain a separate weak reference counter (`weak_count`) to track the number of live Weak_References.
5. WHEN the strong reference counter reaches zero and the weak reference counter is also zero, THE MeldObject SHALL deallocate the object immediately.
6. WHEN the strong reference counter reaches zero and the weak reference counter is greater than zero, THE MeldObject SHALL invoke the user-defined lifecycle destructor (if any), then invoke the C++ virtual destructor, then retain the object header as a tombstone until the weak reference counter also reaches zero.
7. THE deallocation sequence WHEN `strong_count` reaches zero SHALL be: (a) invoke the user-defined `@intrinsic(lifecycle_destructor)` method if present, (b) run field destructors in reverse declaration order, (c) if `weak_count == 0` free the entire object immediately, (d) if `weak_count > 0` destroy the payload but retain the header as a tombstone so `View[T]` references can safely detect the object is dead, (e) when `weak_count` later reaches zero free the tombstone header.
8. DURING destructor execution (step a), THE object SHALL still be considered alive — `View[T]` access (via `?.`, `if val`, or `match`) to the dying object SHALL succeed until the destructor returns.

### Requirement 137: Continuation and Control Flow Migration

**User Story:** As a kernel developer, I want continuation and control flow code to use MeldRef instead of `std::shared_ptr`, so that the entire kernel consistently uses the non-atomic reference counting model.

#### Acceptance Criteria

1. THE `continuation.hpp` library functions SHALL accept and return `MeldRef<Continuation>` instead of `std::shared_ptr<Continuation>`.
2. THE `mark_stack` function SHALL create continuations using `MeldRef<Continuation>` instead of `std::make_shared<Continuation>`.
3. THE `suspend` function SHALL pass `MeldRef<Continuation>` to callback functions.
4. THE `resume` function SHALL accept `MeldRef<Continuation>` as its first parameter.
5. THE `control_flow.cpp` extension functions SHALL use `MeldRef<Function>` instead of `std::shared_ptr<Function>` for block parameters.
6. THE `operators.cpp` operator functions SHALL operate on MeldRef-based Value objects without behavioral changes.

### Requirement 138: MeldObject Backward Compatibility

**User Story:** As a kernel developer, I want all existing kernel primitive semantics to be preserved after the migration, so that no existing functionality is broken.

#### Acceptance Criteria

1. THE 20 kernel primitives SHALL produce identical results for identical inputs after the migration.
2. THE `to_string()` method on Value SHALL produce identical output for all kernel types after the migration.
3. THE `is_truthy()` method on Value SHALL produce identical results for all kernel types after the migration.
4. WHEN existing tests are run against the migrated kernel, THE tests SHALL pass without modification to test assertions.
5. THE factory methods `Value::from_int()`, `Value::from_string()`, `Value::from_bool()`, `Value::from_unit()`, and `Value::from_symbol()` SHALL produce semantically equivalent Value objects after the migration.

### Requirement 139: Example Demonstrating MeldObject and MeldRef

**User Story:** As a developer learning Meld internals, I want an example that demonstrates the MeldObject base type and MeldRef smart pointer, so that I can understand the non-atomic reference counting model.

#### Acceptance Criteria

1. THE example SHALL consist of a separate `meldobject-demo.cpp` file and a `meldobject-demo.meld` file following the meld-examples-structure guidelines.
2. THE `meldobject-demo.cpp` file SHALL demonstrate creating MeldObject-derived types, MeldRef copy/move semantics, and reference count verification.
3. THE `meldobject-demo.meld` file SHALL demonstrate Meld language usage that exercises the underlying MeldObject types (creating values, passing them to functions, metadata operations).
4. THE example SHALL verify that reference counts are correct after copy, move, and destruction operations.
5. THE example SHALL demonstrate weak reference creation and cycle-breaking behavior.

---

> **Note:** Requirements 140–143 below were added to address the AI Developer Experience (AI_DX.md) gaps: executable specifications, seedable entropy, virtual time, and Agent-Test mode.

### Requirement 140: Executable Spec Blocks in @blueprint

**User Story:** As a module author, I want to include compiler-verified usage examples in my `@blueprint` annotations, so that consumers and AI agents can trust the examples as ground truth rather than stale documentation.

#### Acceptance Criteria

1. THE `@blueprint` macro SHALL support a `spec` field containing one or more Action-Result pairs, where each pair specifies an input expression and its expected output value
2. THE `spec` field SHALL use the syntax `spec { action: <expression>, result: <expected_value> }` for each pair, where `<expression>` is valid Meld code that invokes the annotated symbol and `<expected_value>` is the expected return value
3. THE `spec` pairs SHALL have access to the module's public API — they execute in a scope where the module's exports are available
4. THE `spec` pairs SHALL support effect declarations: when the action performs effects, the spec SHALL declare them via `@uses(...)` so the compiler can verify effect correctness
5. THE `spec` field SHALL coexist with the existing `@blueprint` fields (`summary`, `rules`, `examples`); the `examples` field remains as unverified documentation while `spec` is the verified counterpart
6. WHEN a `spec` action produces a value that does not match the declared `result`, THE Compiler SHALL emit a compile-time error identifying the spec pair, the expected value, and the actual value
7. THE `spec` blocks SHALL be serializable to a structured JSON format containing: the action source text, the expected result, the declared effects, and the verification status (pass/fail)
8. THE `spec` blocks SHALL support `@blueprint` inheritance — when a child blueprint extends a parent, the child inherits the parent's spec pairs and may add additional ones

> **Cross-reference:** Spec block verification timing is defined in `.kiro/specs/meld-compiler/requirements.md` Req 30. The MCP tool for streaming specs to agents is defined in `.kiro/specs/meld-mcp-server/requirements.md` Req 11.

### Requirement 141: Seedable Random Effect

**User Story:** As a developer testing AI-generated code, I want the `random` effect to accept a seed parameter, so that randomized behavior is reproducible when needed.

#### Acceptance Criteria

1. THE `random` effect declaration SHALL include a `seed` configuration parameter that, when provided, forces the random number generator to produce a deterministic sequence from that seed
2. WHEN a `random` effect handler is installed with a seed, ALL calls to `random.next_int()`, `random.next_float()`, `random.shuffle()`, and other random operations within that handler's scope SHALL produce the same sequence on every execution with the same seed
3. WHEN no seed is provided, THE `random` effect SHALL use a non-deterministic entropy source (system random) as the default behavior
4. THE seed parameter SHALL be a 64-bit integer value
5. THE `random` effect handler SHALL support re-seeding mid-execution via `random.reseed(new_seed)` for scenarios requiring multiple deterministic sequences
6. THE seedable random behavior SHALL work identically across all three execution tiers (AST Interpreter, ORC JIT, AOT)

### Requirement 142: Virtual Time Effect

**User Story:** As a developer testing time-dependent code, I want the `time` effect to support a virtual clock mode, so that tests and AI agents can control the passage of time deterministically.

#### Acceptance Criteria

1. THE `time` effect declaration SHALL support a `virtual` mode where the clock does not advance automatically — it only advances when explicitly told to via `time.advance(duration)`
2. WHEN a `time` effect handler is installed in virtual mode, ALL calls to `time.now()` within that handler's scope SHALL return the virtual clock's current value rather than the system clock
3. WHEN `time.advance(duration)` is called in virtual mode, THE virtual clock SHALL advance by the specified duration and any pending timers or delays that fall within the advanced window SHALL fire in deterministic order
4. THE virtual time handler SHALL support setting an initial time via `time.set_origin(timestamp)` so that tests can start from a known epoch
5. WHEN virtual time mode is active, THE `delay` and `timeout` operations in the async system (meld-async) SHALL use the virtual clock rather than the system clock
6. THE virtual time behavior SHALL work identically across all three execution tiers (AST Interpreter, ORC JIT, AOT)

> **Cross-reference:** Integration with async delay/timeout is specified in `.kiro/specs/meld-async/requirements.md` Req 23.

### Requirement 143: Agent-Test Mode

**User Story:** As an AI agent running tests, I want a single mode toggle that forces all non-deterministic effects to use deterministic defaults, so that test results are perfectly reproducible without manually wiring up effect handlers.

#### Acceptance Criteria

1. THE Language Runtime SHALL support an "Agent-Test" mode that, when activated, automatically installs deterministic effect handlers for all non-deterministic effects before user code executes
2. IN Agent-Test mode, THE `time` effect SHALL automatically use virtual time mode with a fixed origin of Unix epoch (0) and no automatic advancement
3. IN Agent-Test mode, THE `random` effect SHALL automatically use a fixed seed of 0, producing the same random sequence on every execution
4. IN Agent-Test mode, THE runtime SHALL log all effect invocations (time reads, random calls, I/O operations) to a structured trace that can be inspected after execution for debugging
5. THE Agent-Test mode SHALL be activated via the `--agent-test` CLI flag (see `.kiro/specs/meld-cli/requirements.md` Req 19) or programmatically via `Runtime.enable_agent_test_mode()`
6. THE Agent-Test mode SHALL compose with user-installed effect handlers — if the user explicitly installs a custom `time` or `random` handler, the user's handler takes precedence over the Agent-Test defaults
7. THE Agent-Test mode SHALL produce identical results across all three execution tiers (AST Interpreter, ORC JIT, AOT) for the same input program
8. THE structured effect trace produced in Agent-Test mode SHALL be serializable to JSON and compatible with the Flight Recorder snapshot format (Req 44)

> **Cross-reference:** The `--agent-test` CLI flag is specified in `.kiro/specs/meld-cli/requirements.md` Req 19. Deterministic actor scheduling in Agent-Test mode is specified in `.kiro/specs/meld-async/requirements.md` Req 24.

### Requirement 144: Empty Tuple Type Declaration

**User Story:** As a Meld developer, I want `()` to be a formal type in the type system representing "no meaningful value," so that void-returning functions fit naturally into Meld's existing tuple return syntax.

#### Acceptance Criteria

1. THE Type_Checker SHALL recognize `()` as a built-in type with exactly one inhabiting value
2. WHEN a developer writes `()` in a type position (e.g., `-> ()`, `Task[()]`), THE Parser SHALL parse `()` as the Empty_Tuple type
3. THE Type_Checker SHALL treat `()` as a distinct type that is not assignable to or from `nil`, `bool`, `int`, `string`, `float`, or any other built-in type without explicit conversion
4. THE Type_Checker SHALL treat `()` as the zero-element case of the tuple type continuum: `()` (zero), `(T)` (one, equivalent to `T`), `(T, U)` (two), etc.

### Requirement 145: Empty Tuple Value Literal

**User Story:** As a Meld developer, I want `()` in expression position to produce the empty tuple value, so that I can explicitly return it when needed.

#### Acceptance Criteria

1. WHEN a developer writes `()` in an expression position, THE Parser SHALL parse it as the canonical Empty_Tuple_Value (not as `nil`)
2. THE Type_Checker SHALL infer the type of the `()` literal as Empty_Tuple
3. WHEN `()` is compared to `()` using `==`, THE Type_Checker SHALL permit the comparison and the runtime SHALL evaluate the expression to `true`

### Requirement 146: Mandatory Explicit Return Type Annotation

**User Story:** As a Meld developer, I want every function to require an explicit `-> ReturnType` annotation (including `-> ()` for side-effect-only functions), so that return types are always visible and unambiguous.

> **Note:** This requirement supersedes Requirement 5 AC8 (which previously allowed omitting return type annotations for type inference). See updated Requirement 5 AC4.

#### Acceptance Criteria

1. WHEN a function declaration omits the `-> ReturnType` annotation, THE Parser SHALL report a compile-time error requiring an explicit return type
2. WHEN a developer writes `fnc name(params) -> () { body }`, THE Parser SHALL accept the declaration as valid
3. THE Type_Checker SHALL treat `-> ()` as returning the Empty_Tuple type
4. WHEN a function is annotated with `-> ()` and the body contains `rtn value` where `value` is not of Empty_Tuple type, THE Type_Checker SHALL report a type mismatch error

### Requirement 147: Implicit Empty Tuple Value from Block Expressions

**User Story:** As a Meld developer, I want function bodies that end with a side-effecting statement to implicitly evaluate to `()`, so that I do not need to write `rtn ()` at the end of every procedure.

#### Acceptance Criteria

1. WHEN a function body's last expression is a statement that does not produce a value (e.g., an assignment, or a call to a function returning Empty_Tuple), THE Type_Checker SHALL treat the Implicit_Return value as Empty_Tuple_Value
2. WHEN a function with return type `()` contains an explicit `rtn ()` statement, THE Type_Checker SHALL accept the statement as valid
3. WHEN a function with return type `()` contains an explicit `rtn` with no argument, THE Parser SHALL desugar the bare `rtn` into `rtn ()`

### Requirement 148: Task with Empty Tuple Type

**User Story:** As a Meld developer, I want to use `Task[()]` for async operations that complete without producing a value, so that I can compose side-effecting async work with Task combinators.

#### Acceptance Criteria

1. WHEN a developer writes `Task[()]` as a return type, THE Type_Checker SHALL accept the parameterized type as valid
2. WHEN an `async` block's body implicitly returns Empty_Tuple_Value, THE Type_Checker SHALL infer the block's type as `Task[()]`
3. WHEN a `Task[()]` is awaited, THE Type_Checker SHALL infer the result type of the `await` expression as Empty_Tuple
4. THE Type_Checker SHALL permit `Task[()]` to participate in all Task combinators (`.map[U]`, `.flatMap[U]`, `.thenAccept`, `.whenComplete`, `.withTimeout()`, `Task.all()`, `Task.race()`)

### Requirement 149: Empty Tuple Type in Generic Contexts

**User Story:** As a Meld developer, I want `()` to be a valid type argument for any generic parameter, so that generic abstractions work uniformly with side-effecting operations.

#### Acceptance Criteria

1. WHEN `()` is used as a type argument (e.g., `list[()]`, `option[()]`, `Result[(), E]`), THE Type_Checker SHALL accept the parameterization as valid
2. WHEN a generic function is instantiated with Empty_Tuple as a type parameter, THE Type_Checker SHALL produce a valid specialization
3. THE Type_Checker SHALL permit `Result[(), E]` to represent operations that succeed without a meaningful value or fail with error type `E`

### Requirement 150: Empty Tuple Distinction from Nil

**User Story:** As a Meld developer, I want `()` and `nil` to be clearly distinct concepts, so that I do not accidentally conflate "completed successfully" with "value is absent."

#### Acceptance Criteria

1. THE Type_Checker SHALL reject assignment of `nil` to a variable of type `()`
2. THE Type_Checker SHALL reject assignment of `()` to a variable of type `optional[T]` where `T` is not `()`
3. WHEN a developer applies a Control_Flow_Quintet operator (`?.`, `?`, `?:`, `?!`, `!!`) to a value of type `()`, THE Type_Checker SHALL report a type error because Empty_Tuple is neither `optional[T]` nor `Result[T, E]`
4. THE Type_Checker SHALL accept `optional[()]` as a valid type, representing a value that is either present-but-meaningless or absent

### Requirement 151: Effect Trait Methods with Explicit Return Type

**User Story:** As a Meld developer, I want effect trait methods to require an explicit return type annotation (including `-> ()` for side-effect-only methods), so that the effect system is consistent with the mandatory return type rule.

#### Acceptance Criteria

1. WHEN an `@effect trait` method declaration omits the `-> ReturnType` annotation, THE Parser SHALL report a compile-time error requiring an explicit return type
2. WHEN an effect handler implements a method with return type `()`, THE Type_Checker SHALL verify that the handler body returns Empty_Tuple_Value or has an implicit empty tuple return
3. WHEN a function annotated with `@uses` calls an effect method returning `()`, THE Type_Checker SHALL accept the call as a valid statement expression

### Requirement 152: Function Type Expressions with Empty Tuple

**User Story:** As a Meld developer, I want to use `()` in function type expressions (e.g., `() -> ()` as a parameter type), so that higher-order functions accepting side-effecting callbacks are well-typed.

#### Acceptance Criteria

1. WHEN a developer writes a Function_Type with `()` as the return type (e.g., `() -> ()`, `(int) -> ()`), THE Parser SHALL accept the type expression as valid
2. WHEN a higher-order function parameter is typed as `() -> ()`, THE Type_Checker SHALL accept a lambda or function reference whose return type is Empty_Tuple as a valid argument
3. Function_Type expressions SHALL always require an explicit return type — omitting the return type in a type annotation position SHALL be a parse error

### Requirement 153: Lambda and Closure Implicit Empty Tuple Return

**User Story:** As a Meld developer, I want lambdas and closures that perform only side effects to implicitly return `()`, so that callback-heavy code does not require explicit `rtn ()` statements.

#### Acceptance Criteria

1. WHEN a lambda body's last expression is a statement that does not produce a value, THE Type_Checker SHALL infer the lambda's return type as Empty_Tuple
2. WHEN a lambda is passed to a parameter expecting Function_Type returning `()`, and the lambda body implicitly returns Empty_Tuple_Value, THE Type_Checker SHALL accept the lambda without requiring an explicit `rtn ()`
3. WHEN a lambda with an inferred Empty_Tuple return contains `rtn value` where `value` is not of Empty_Tuple type, THE Type_Checker SHALL report a type mismatch error

### Requirement 154: Kernel Representation of Empty Tuple

**User Story:** As a compiler developer, I want a well-defined kernel S-expression representation for the empty tuple type and value, so that the compiler pipeline handles it consistently from parsing through code generation.

#### Acceptance Criteria

1. WHEN the Parser encounters `()` in expression position, THE Parser SHALL emit a kernel S-expression representing the Empty_Tuple_Value (distinct from Nil)
2. WHEN the Parser encounters `()` in type position, THE Parser SHALL emit a kernel type S-expression representing the Empty_Tuple type
3. THE Parser SHALL distinguish `()` (Empty_Tuple) from `nil` (Nil) in the kernel representation — these SHALL be different S-expression forms

### Requirement 155: MeldValue Runtime Representation

**User Story:** As a compiler developer, I want the runtime tagged union (`MeldValue`) to represent the empty tuple value efficiently, so that returning `()` incurs zero overhead compared to void functions in other languages.

#### Acceptance Criteria

1. THE MeldValue tagged union SHALL represent the Empty_Tuple_Value using an existing tag or a new dedicated tag that requires no heap allocation
2. WHEN a function returning Empty_Tuple completes execution, the runtime SHALL produce the canonical Empty_Tuple_Value without allocating memory
3. THE Empty_Tuple_Value runtime representation SHALL be equality-comparable, and two Empty_Tuple_Values SHALL compare as equal
4. THE Empty_Tuple_Value SHALL be a distinct runtime value from Nil — `MeldValue::make_nil()` and the Empty_Tuple_Value SHALL NOT be equal

---

> **Note:** Requirements 156–164 below add lifecycle methods (`@intrinsic(lifecycle_destructor)` and `@intrinsic(lifecycle_constructor)`), ergonomic `@constructor`/`@destructor` alias macros in `std.mem`, the ARC model semantics, cross-Actor transfer, the LLVM ARC injection pipeline, polyglot backend ARC strategy, and the lifecycle demo example. These formalize content previously documented only in MEMORY_MANAGEMENT.md and introduce the annotation-based lifecycle discovery mechanism.

### Requirement 156: Lifecycle Destructor via @intrinsic Annotation

**User Story:** As a Meld developer, I want to define cleanup logic that runs when my object is destroyed, so that I can release native resources, unsubscribe from events, and perform deterministic cleanup without hardcoding a destructor method name.

#### Acceptance Criteria

1. THE `@intrinsic(lifecycle_destructor)` annotation SHALL mark a method as the lifecycle destructor for its enclosing class.
2. THE `@intrinsic(lifecycle_destructor)` annotation SHALL expand (via the decorator macro system) to `meta_set(method, :intrinsic, :lifecycle_destructor)`, attaching metadata to the method's AST node.
3. THE Compiler SHALL discover lifecycle destructors by checking `meta_get(method, :intrinsic) == :lifecycle_destructor` on each method of a class — the method name is irrelevant.
4. WHEN the last `Hold[T]` reference to an object is released (`strong_count` reaches zero), THE runtime SHALL invoke the lifecycle destructor method before running field destructors and before deallocation.
5. A class SHALL have at most one method annotated with `@intrinsic(lifecycle_destructor)`; THE Semantic_Analyzer SHALL emit a compile-time error if more than one lifecycle destructor is declared on the same class.
6. THE lifecycle destructor method SHALL accept no parameters (other than implicit `self`) and SHALL have return type `-> ()`.
7. THE lifecycle destructor SHALL NOT perform effects — THE Semantic_Analyzer SHALL emit a compile-time error if a lifecycle destructor method has an `@uses` annotation or calls an effectful function. Destructors must be infallible and side-effect-free (relative to the effect system).
8. DURING lifecycle destructor execution, THE object SHALL still be considered alive — `View[T]` access (via `?.`, `if val`, or `match`) to the object SHALL succeed until the destructor returns.
9. THE lifecycle destructor SHALL be inherited: if a subclass does not declare its own `@intrinsic(lifecycle_destructor)` method, THE runtime SHALL invoke the parent class's lifecycle destructor. If a subclass declares its own, THE runtime SHALL invoke the subclass destructor first, then the parent class destructor (most-derived-first order).

### Requirement 157: Lifecycle Constructor via @intrinsic Annotation

**User Story:** As a Meld developer, I want to define initialization logic that runs after object allocation and field initialization, so that I can perform setup work without hardcoding a constructor method name.

#### Acceptance Criteria

1. THE `@intrinsic(lifecycle_constructor)` annotation SHALL mark a method as the lifecycle constructor for its enclosing class.
2. THE `@intrinsic(lifecycle_constructor)` annotation SHALL expand (via the decorator macro system) to `meta_set(method, :intrinsic, :lifecycle_constructor)`, attaching metadata to the method's AST node.
3. THE Compiler SHALL discover lifecycle constructors by checking `meta_get(method, :intrinsic) == :lifecycle_constructor` on each method of a class — the method name is irrelevant.
4. WHEN an object is allocated and its fields are initialized, THE runtime SHALL invoke the lifecycle constructor method before the object reference is returned to the caller.
5. A class SHALL have at most one method annotated with `@intrinsic(lifecycle_constructor)`; THE Semantic_Analyzer SHALL emit a compile-time error if more than one lifecycle constructor is declared on the same class.
6. THE lifecycle constructor method SHALL accept parameters matching the class's construction arguments and SHALL have return type `-> ()`.
7. THE lifecycle constructor SHALL be inherited: THE runtime SHALL invoke the parent class's lifecycle constructor first, then the subclass's lifecycle constructor (base-first order — the inverse of destructor order).

### Requirement 158: Lifecycle Alias Macros (@constructor / @destructor)

**User Story:** As a Meld developer, I want ergonomic `@constructor` and `@destructor` decorator aliases, so that I can annotate lifecycle methods concisely without writing the verbose `@intrinsic(lifecycle_constructor)` / `@intrinsic(lifecycle_destructor)` form.

#### Acceptance Criteria

1. THE `std.mem` module SHALL export a decorator macro `constructor` that expands to `meta_set(node, :intrinsic, :lifecycle_constructor)` when applied to a `MethodNode`.
2. THE `std.mem` module SHALL export a decorator macro `destructor` that expands to `meta_set(node, :intrinsic, :lifecycle_destructor)` when applied to a `MethodNode`.
3. WHEN `@constructor` is applied to a method, THE resulting metadata SHALL be identical to applying `@intrinsic(lifecycle_constructor)` — the compiler SHALL treat both forms equivalently.
4. WHEN `@destructor` is applied to a method, THE resulting metadata SHALL be identical to applying `@intrinsic(lifecycle_destructor)` — the compiler SHALL treat both forms equivalently.
5. THE `@constructor` and `@destructor` macros SHALL be available when `std.mem` is imported — no additional import required.
6. ALL validation rules from Requirements 156 and 157 (at-most-one per class, return type, effect prohibition for destructors, parameter matching for constructors) SHALL apply equally to methods annotated via `@constructor` / `@destructor` as to methods annotated via `@intrinsic(lifecycle_constructor)` / `@intrinsic(lifecycle_destructor)`.

### Requirement 159: ARC Model Semantics

**User Story:** As a Meld developer, I want well-defined reference counting semantics for class and actor instances, so that I understand exactly when objects are created, shared, and destroyed.

#### Acceptance Criteria

1. EVERY `class` and `actor` instance SHALL carry an embedded `strong_count` and `weak_count` in its object header (intrusive counting — no separate control block).
2. WHEN a reference to a `class` or `actor` instance is assigned (`=`), THE runtime SHALL perform a shallow copy of the reference and increment `strong_count` (non-atomic `++`).
3. WHEN a reference to a `class` or `actor` instance goes out of scope, THE runtime SHALL decrement `strong_count` (non-atomic `--`).
4. WHEN `strong_count` reaches zero, THE runtime SHALL execute the deallocation sequence defined in Requirement 136 criterion 7.
5. THE object header SHALL be freed only when both `strong_count == 0` and `weak_count == 0`.
6. ALL reference counting operations SHALL be non-atomic, justified by Meld's shared-nothing Actor concurrency model where objects never cross thread boundaries.
7. IN debug builds, THE runtime SHALL provide a `MELD_DEBUG_ASSERT_SAME_THREAD()` assertion that detects accidental cross-thread reference count access.

### Requirement 160: Cross-Actor Transfer and Send Trait

**User Story:** As a Meld developer, I want safe cross-Actor value transfer, so that non-atomic reference counting remains correct when values are sent between Actors.

#### Acceptance Criteria

1. WHEN a value is sent across Actor boundaries via `send()` or `ask()`, THE runtime SHALL deep-copy the value into the receiving Actor's thread-local heap — never share a raw pointer.
2. THE `Message_Envelope` used for cross-Actor communication SHALL contain a serialized representation of the value, never a raw `MeldRef`.
3. THE Standard Library SHALL define a `Send` trait that types must implement to be transferable across Actor boundaries.
4. WHEN a type that does not implement `Send` is passed to `send()` or `ask()`, THE Semantic_Analyzer SHALL emit a compile-time error.
5. Types with non-serializable resources (native handles, closures with captured mutable state) SHALL NOT implement `Send`.
6. WHEN a value crosses Isolate (OS process) boundaries, THE runtime SHALL serialize the value over IPC channels using length-prefixed binary framing.
7. THE cross-boundary transfer cost model SHALL be: Fiber→Fiber (same Actor) = zero cost (direct `MeldRef` sharing), Actor→Actor = proportional to value size (deep copy), Isolate→Isolate = serialization + IPC overhead.

### Requirement 161: LLVM ARC Injection Pipeline

**User Story:** As a compiler developer, I want a well-defined pipeline of compiler passes for injecting and optimizing reference counting operations, so that ARC overhead is minimized in generated code.

#### Acceptance Criteria

1. THE Compiler SHALL implement an ARC Injection Pass that performs lexical scope analysis to insert `intrinsic_retain` at reference creation and `intrinsic_release` at scope exit, handling control flow (phi nodes, branches, loops).
2. THE Compiler SHALL implement an ARC Optimization Pass that performs: (a) retain/release elision (cancel-out adjacent pairs), (b) retain sinking / release hoisting, (c) loop hoisting for loop-invariant references, (d) copy-on-write deferral for read-only values.
3. THE Compiler SHALL implement an ARC Verification Pass that detects use-after-free, double-free, and memory leak patterns in the IR.
4. THE ARC passes SHALL execute in order: Injection → Optimization → Verification → Backend-Specific Lowering.
5. THE IR SHALL use `intrinsic_retain <operand>` to increment reference count and `intrinsic_release <operand>` to decrement reference count (triggering deallocation at zero).

### Requirement 162: Polyglot Backend ARC Strategy

**User Story:** As a compiler developer, I want the ARC pipeline to adapt to different compilation targets, so that the same Meld source runs with deterministic ARC on native targets and defers to GC on managed targets.

#### Acceptance Criteria

1. THE C++ backend SHALL lower `intrinsic_retain` / `intrinsic_release` to `MeldRef::retain()` / `MeldRef::release()` calls.
2. THE WASM backend SHALL lower `intrinsic_retain` / `intrinsic_release` to `$meld_retain` / `$meld_release` functions from the `meld_rt` runtime.
3. THE JVM backend SHALL strip all ARC instructions (retain/release) from the IR, deferring memory management to the JVM garbage collector.
4. THE Go backend SHALL strip all ARC instructions from the IR, deferring memory management to the Go garbage collector.
5. THE same Meld source code SHALL compile and run correctly on all supported backends without source-level changes to memory management.

### Requirement 163: ARC Performance Characteristics

**User Story:** As a Meld developer, I want to understand the cost model of memory management operations, so that I can write performance-sensitive code with predictable overhead.

#### Acceptance Criteria

1. `Hold[T]` assignment SHALL cost exactly 1 non-atomic increment operation.
2. `Hold[T]` scope exit SHALL cost exactly 1 non-atomic decrement operation.
3. `std.mem.move()` SHALL cost zero reference count operations (pointer swap only).
4. `View[T]` assignment SHALL cost exactly 1 non-atomic increment on `weak_count`.
5. `View[T]` upgrade SHALL cost exactly 1 check + 1 non-atomic increment (if alive).
6. Object destruction SHALL be deterministic: destructor + free, with no GC pauses or unpredictable latency.

### Requirement 164: Lifecycle Methods Example

**User Story:** As a developer learning Meld, I want an example that demonstrates lifecycle constructors and destructors using `@intrinsic` annotations, so that I can understand how deterministic cleanup works with the ARC model.

#### Acceptance Criteria

1. THE example SHALL consist of a `lifecycle-demo.meld` file demonstrating lifecycle annotations in pure Meld code.
2. THE `lifecycle-demo.meld` file SHALL demonstrate a class with `@intrinsic(lifecycle_constructor)` and `@intrinsic(lifecycle_destructor)` methods.
3. THE `lifecycle-demo.meld` file SHALL demonstrate destructor ordering in a class hierarchy (subclass destructor runs before parent destructor).
4. THE `lifecycle-demo.meld` file SHALL demonstrate interaction between lifecycle destructors and `Hold[T]`/`View[T]` (field destruction cascade, `View[T]` access during destructor).
5. THE `lifecycle-demo.meld` file SHALL demonstrate a resource-managing class (e.g., file handle wrapper) that uses `@intrinsic(lifecycle_destructor)` for deterministic cleanup.

### Requirement 176: Unified Handle Function Syntax

Requirements 176.1–176.9 were merged from the former `handle-as-function` spec, which unifies Meld's three competing `handle` syntax forms into a single function-call style: `handle({ computation }, effect_name { handlers })`.

**User Story:** As a Meld developer, I want `handle` to use regular function-call syntax with comma-separated handler arguments, so that it looks and behaves like any other library function rather than requiring special grammar rules.

#### Acceptance Criteria

##### 176.1: Unified Handle Call Syntax

1. THE handle function SHALL accept a computation closure `() -> T` as its first positional argument
2. THE handle function SHALL accept one or more handler closures as subsequent comma-separated positional arguments
3. THE Parser SHALL parse `handle(...)` invocations using the same grammar rules as any other function call with closure arguments
4. THE handle function SHALL return the value produced by the computation closure (single return-value model — no void/non-void flavors)

##### 176.2: Handler Closure Syntax

1. THE handler closure SHALL be written as an effect name followed by a closure body: `effect_name { fnc op(...) { ... } }`
2. THE handler closure SHALL contain one or more effect operation interceptor definitions using standard `fnc` syntax
3. THE effect operation interceptors SHALL have access to the `resume` continuation to return values to the computation closure

##### 176.3: Multiple Effect Handlers

1. THE handle function SHALL accept multiple handler closures as comma-separated arguments after the computation closure
2. WHEN multiple handler closures are provided, THE handle function SHALL route each effect call to the matching handler based on the effect name
3. WHEN an effect call has no matching handler, THE handle function SHALL propagate the effect call to the next enclosing handler in the call stack

##### 176.4: Removal of Legacy Syntax Forms

1. THE Parser SHALL reject the `computation:` named-parameter syntax for handle calls
2. THE Parser SHALL reject the `with` keyword syntax for chaining handler closures
3. THE Parser SHALL reject bare `handle { ... }` blocks that omit explicit handler closures
4. WHEN the Parser encounters a legacy handle syntax form, THE Parser SHALL emit a diagnostic message identifying the deprecated form and suggesting the unified syntax

##### 176.5: Nested Handle Calls

1. WHEN a handle call appears inside the computation closure of another handle call, THE inner handle call SHALL take precedence for effects it handles
2. WHEN the inner handle call does not handle a given effect, THE effect call SHALL propagate to the outer handle call

##### 176.6: Handle as Expression

1. THE handle call SHALL be a valid expression that can appear on the right-hand side of a `val` binding
2. THE handle call SHALL be a valid expression that can appear as an argument to another function
3. THE handle call SHALL be a valid expression that can appear as the last expression in a block (implicit return)

##### 176.7: Parser Round-Trip for Handle Syntax

1. THE Parser SHALL parse unified handle call syntax into a handle_expression AST node
2. THE Pretty_Printer SHALL format handle_expression AST nodes back into valid unified handle call syntax
3. FOR ALL valid handle expressions, parsing then pretty-printing then parsing SHALL produce an equivalent AST (round-trip property)

##### 176.8: Migration Diagnostics

1. WHEN the Parser encounters `handle(computation: { ... })` syntax, THE Parser SHALL emit a diagnostic with severity "error" and a fix suggestion showing the equivalent unified syntax
2. WHEN the Parser encounters `handle { ... } with effect_name { ... }` syntax, THE Parser SHALL emit a diagnostic with severity "error" and a fix suggestion showing the equivalent unified syntax
3. WHEN the Parser encounters `perform { ... }` block syntax in code that uses implicit effect calls, THE Parser SHALL emit a diagnostic with severity "warning" suggesting removal of the `perform` wrapper

##### 176.9: Example File Updates

1. ALL `.meld` example files in `meld-core/examples/` SHALL use unified handle call syntax `handle({ ... }, effect { ... })` for all handle expressions
2. Migration example files SHALL show old syntax only in commented-out BEFORE sections, with AFTER sections using unified syntax

### Requirement 177: Anonymous Implementation Blocks

**User Story:** As a Meld developer, I want a single unified syntax `Name { ... }` for creating anonymous values that implement traits, extend types, or define inline objects with methods, so that I can use lightweight context-specific implementations without defining named types and without learning additional keywords.

#### Acceptance Criteria

##### 177.1: Unified Anonymous Block Syntax

1. THE syntax `Name { fnc method(...) -> T { ... } }` SHALL create an anonymous value that satisfies the type `Name`
2. WHEN `Name` refers to a trait, THE block SHALL implement the trait's required methods
3. WHEN `Name` refers to a class or struct, THE block SHALL create an anonymous subtype with the provided methods (overriding or extending)
4. THE block MAY contain `fnc` definitions, `val` declarations, `var` declarations, or any combination
5. THE anonymous block SHALL be a valid expression usable in val bindings, function arguments, and any expression position
6. THE methods in the block SHALL be closures that capture the enclosing scope
7. THE `self` keyword inside the block SHALL refer to the anonymous value being created

##### 177.2: Parser Disambiguation

1. THE Parser SHALL disambiguate `Name { fnc ... }` (anonymous implementation) from `Name { field = value }` (initialization block) by checking for the `fnc`/`func`/`val`/`var` keyword after `{`
2. WHEN the first token after `{` is `fnc`, `func`, `val`, or `var`, THE Parser SHALL parse as an anonymous implementation block
3. WHEN the first token after `{` is an identifier followed by `=`, THE Parser SHALL parse as an initialization block (existing behavior)

##### 177.3: Type Compatibility

1. AN anonymous implementation of trait `T` SHALL be assignable to variables of type `T`
2. AN anonymous subtype of class/struct `C` SHALL be assignable to variables of type `C`
3. THE type checker SHALL verify that anonymous implementations satisfy all required methods of the target trait
4. THE compiler SHALL emit an error if the block does not implement all required methods of a trait
5. THE compiler SHALL emit an error if the block is empty (no definitions)

##### 177.4: Multiple Trait Implementation

1. THE syntax `(Trait1 & Trait2) { fnc ... }` SHALL create an anonymous value implementing multiple traits via intersection type
2. THE block SHALL contain method definitions satisfying all required methods from all listed traits
3. THE compiler SHALL emit an error if any required method from any listed trait is missing

##### 177.5: Effect Handler Integration

1. THE `handle()` function SHALL accept anonymous implementation blocks as effect handler arguments
2. THE syntax `handle({ body }, EffectName { fnc op() { ... } })` SHALL use the same anonymous block syntax as standalone trait implementations
3. THERE SHALL be no special grammar for effect handlers — they are anonymous implementation blocks passed as function arguments

---

> **Note:** Requirements 165–175 below add the Hold/View tenancy updates, generic mutability qualifiers (`val`/`var` on type parameters), the standard collection rename (Dict→Map, Deque→Queue), tenancy-aware collection APIs, the State vs. Identity effect clarification, ARC-aware destructuring, and safe navigation `@effect` skipping. These formalize content from UPDATES.md.

### Requirement 165: Generic Mutability Qualifiers (val/var on Type Parameters)

**User Story:** As a Meld developer, I want to annotate generic type parameters with `val` or `var` to control whether the contained value can be mutated through that generic wrapper, so that I can express fine-grained mutability contracts at the type level.

#### Acceptance Criteria

1. THE Parser SHALL support `val` and `var` keywords before any type parameter in any generic type: e.g., `Hold[val T]`, `Hold[var T]`, `View[val T]`, `View[var T]`, `List[val T]`, `Map[val K, var V]`, `Result[val T, val E]`.
2. WHEN a generic type parameter has no explicit `val`/`var` qualifier, THE Semantic_Analyzer SHALL default to `val` (immutable), consistent with Meld's "immutable by default" philosophy.
3. WHEN a type parameter is qualified with `val`, THE Semantic_Analyzer SHALL reject calls to any method on the contained value that is annotated with `@effect(state)` or that mutates internal state.
4. WHEN a type parameter is qualified with `var`, THE Semantic_Analyzer SHALL permit calls to mutating methods on the contained value, provided the enclosing function has the `@effect(state)` permission.
5. THE Semantic_Analyzer SHALL support automatic downgrading: a `Foo[var T]` value MAY be passed where `Foo[val T]` is expected, but a `Foo[val T]` value SHALL NOT be passed where `Foo[var T]` is expected without an explicit checked cast.
6. THE checked cast from `val` to `var` SHALL require elevated permissions and SHALL be auditable by the Effect Firewall.
7. THE `meldd` daemon SHALL automatically infer and attach the `@effect(state)` tag to any function whose signature contains a `var`-qualified type parameter.

### Requirement 166: Standard Collection Types (List, Map, Set, Queue)

**User Story:** As a Meld developer, I want a consistent, short-named set of standard collection types, so that the standard library feels balanced and architectural.

#### Acceptance Criteria

1. THE Standard Library SHALL provide four core collection types: `List[T]`, `Map[K, V]`, `Set[T]`, and `Queue[T]`.
2. THE Parser SHALL recognize `List`, `Map`, `Set`, and `Queue` as reserved collection keywords to prevent name collisions with user-defined types.
3. IF a developer uses the former name `Dict`, THE Semantic_Analyzer SHALL emit a migration hint: "Did you mean Map? Dict has been renamed to Map."
4. IF a developer uses the former name `Deque` or `deque`, THE Semantic_Analyzer SHALL emit a migration hint: "Did you mean Queue? Deque has been renamed to Queue."
5. ALL collection types SHALL be annotated with `@intrinsic(managed_container)` and SHALL enforce the Storable trait constraint on element type parameters.

### Requirement 167: Tenancy-Aware Collection APIs

**User Story:** As a Meld developer, I want collection APIs that respect the Hold/View tenancy model, so that ownership transfer and observation semantics are explicit in collection operations.

#### Acceptance Criteria

1. `List[Hold[T]]` SHALL own the lifecycle of its elements: removing or dropping the list SHALL decrement the `strong_count` of every element.
2. `List[View[T]]` SHALL never increment the `strong_count` of its elements; it is an observer-only collection.
3. `Map[K, V].keys()` SHALL return `List[View[K]]` and `Map[K, V].values()` SHALL return `List[View[V]]` to prevent unnecessary reference counting during inspection.
4. `Queue[T].front()` and `Queue[T].back()` SHALL return `View[T]?` (safe navigation optional), allowing observation without ownership transfer.
5. `Queue[T].pop-front()` and `Queue[T].pop-back()` SHALL return `Hold[T]`, transferring ownership of the element to the caller.
6. WHEN accessing an element of a `List[View[T]]` by index (e.g., `list[n]`), THE return type SHALL be `optional[View[T]]`, forcing a safe check.
7. ALL destructive collection methods (`add`, `remove`, `clear`, `sort-in-place`) SHALL require the enclosing function to have `@effect(state)` permission.

### Requirement 168: Effect System — State vs. Identity Clarification

**User Story:** As a Meld developer, I want a clear distinction between variable binding mutability (`val`/`var`) and internal state mutation (`@effect(state)`), so that I understand exactly what each controls and the compiler enforces both dimensions.

#### Acceptance Criteria

1. THE `val`/`var` declaration keyword SHALL control the local binder (identity): `val` prevents reassignment of the variable name; `var` permits reassignment.
2. THE `@effect(state)` annotation SHALL control internal mutation (integrity): calling mutating methods on a collection or object's internals requires `@effect(state)` permission on the enclosing function.
3. WHEN a variable is declared as `val` and the developer attempts to call a mutating method (e.g., `list.add(x)`), THE Semantic_Analyzer SHALL reject the call because `val` implies deep immutability — the collection is `val`-qualified and therefore `val` at the type parameter level.
4. WHEN a variable is declared as `var` and the developer calls a mutating method, THE Semantic_Analyzer SHALL permit the call only if the enclosing function has `@effect(state)` permission.
5. THE following "State vs. Identity" matrix SHALL be enforced:
   - `val` + reassignment → compile error (binder is immutable)
   - `val` + `.add(x)` → compile error (lacks `@effect(state)` and type parameter is `val`)
   - `var` + reassignment → permitted (binder is mutable)
   - `var` + `.add(x)` → permitted only if function has `@effect(state)`

### Requirement 169: ARC-Aware Destructuring

**User Story:** As a Meld developer, I want destructuring to correctly handle Hold/View tenancy semantics with per-binding mutability qualifiers, so that ownership and observation are preserved when unpacking data structures and each binding can independently be `val` or `var`.

#### Acceptance Criteria

1. WHEN destructuring a struct or tuple containing `Hold[T]` fields, THE Semantic_Analyzer SHALL perform a partial move: ownership of each `Hold[T]` field transfers to the corresponding destructured variable, and the parent struct/tuple becomes a tombstone (logically dead).
2. WHEN destructuring a struct or tuple containing `View[T]` fields, THE resulting destructured variables SHALL also be `View[T]` — no ownership transfer occurs.
3. AFTER a partial move via destructuring, THE Semantic_Analyzer SHALL reject any subsequent access to the parent struct/tuple with a compile-time error: "use of partially moved value".
4. THE Semantic_Analyzer SHALL NOT allow reassembling a partially moved struct — once destructured, the parent's lifecycle is over.
5. WHEN destructuring a `Map` entry in a `for` loop (e.g., `for (val key, val value) in map`), THE destructured variables SHALL be `View[K]` and `View[V]` respectively, consistent with the Map iterator protocol (Requirement 124, criterion 6).
6. FOR Copy types (e.g., `int`, `float`, `bool`), destructuring SHALL produce independent copies regardless of the parent's tenancy state.
7. THE destructuring syntax SHALL require a `val` or `var` qualifier on each individual binding: `(val x, var y) = expr`. The shorthand `val (x, y) = expr` SHALL NOT be supported — there is exactly one syntax form for destructuring.
8. WHEN a destructuring binding omits the `val`/`var` qualifier, THE Parser SHALL emit a compile-time error requiring an explicit qualifier on each binding.
9. THE Parser SHALL support mixed qualifiers within a single destructuring: e.g., `(val key, var value) = map-entry` where the key is immutable and the value is mutable.

### Requirement 170: Safe Navigation @effect Skipping

**User Story:** As a Meld developer, I want the `?.` safe navigation operator to skip not only the method call but also its associated `@effect` when the View[T] target is deallocated, so that effect tracking remains accurate.

#### Acceptance Criteria

1. WHEN the `?.` operator is applied to a `View[T]` and the referenced object has been deallocated, THE runtime SHALL skip the entire operation including any `@effect` that would have been triggered.
2. THE Effect Firewall SHALL NOT record an `@effect(state)` event for an operation that was skipped due to `?.` short-circuiting on a dead `View[T]`.
3. WHEN `?.` short-circuits, THE expression SHALL evaluate to `nil` (consistent with optional chaining semantics in Requirement 14B).
4. THE `?.` operator on `View[T]` SHALL compile to a null-check guard that tests the `View[T]` validity before executing the operation and its associated effect.

### Requirement 175: IDE Ghost Text for Tenancy State

**User Story:** As a Meld developer, I want my IDE to show inferred Hold/View tenancy state as ghost text, so that I can see the ARC state without cluttering my source code.

#### Acceptance Criteria

1. THE `meldd` daemon SHALL provide an LSP `meld/inlayHints` command that streams inferred `Hold`/`View` states to the IDE.
2. WHEN a variable is initialized with a constructor and the developer did not write an explicit `Hold[T]` annotation, THE IDE SHALL display ghost text showing the inferred `Hold[T]` type.
3. WHEN a function parameter uses a bare type (no explicit wrapper), THE IDE SHALL display ghost text showing the inferred `View[T]` wrapper.
4. THE ghost text SHALL be visually distinct (e.g., grayed out) and SHALL NOT be part of the source file.

---

## Design Principles

### 1. AI-First Design
- Every language feature should enhance AI-human collaboration
- Semantic information should be first-class and searchable
- Code should be self-documenting for AI understanding

### 2. Minimal Core, Maximal Extension
- Keep the kernel as small as possible
- Build complexity through the macro system
- Enable user-defined language extensions

### 3. Safety by Default
- No null pointer exceptions
- No unhandled exceptions
- No data races
- No undefined behavior

### 4. Explicit Over Implicit
- Make intentions clear in code
- Prefer explicit type annotations where helpful
- Make side effects visible

### 5. Polyglot Interoperability
- Seamless integration with existing ecosystems
- Multiple compilation targets
- Foreign function interface for all major languages

---

## Success Criteria

1. **AI Integration:** AI agents can understand, generate, and modify Meld code with 95% accuracy
2. **Performance:** Compiled Meld code performs within 10% of equivalent C++ code
3. **Productivity:** Developers report 50% faster development with AI assistance
4. **Safety:** Zero null pointer exceptions or data races in production code
5. **Adoption:** Successfully transpiles to at least 3 major target platforms

---

## References

- CONSOLIDATED_SPEC_V1.7.md (Authoritative)
- C++ Implementation (meld-cpp)
- [Lisp Homoiconicity](https://en.wikipedia.org/wiki/Homoiconicity)
- [Smalltalk Control Flow](https://wiki.c2.com/?SmalltalkLanguage)
- [Design by Contract](https://en.wikipedia.org/wiki/Design_by_contract)
- [Structured Concurrency](https://vorpus.org/blog/notes-on-structured-concurrency/)
- [Model Context Protocol](https://modelcontextprotocol.io/)
