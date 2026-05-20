# Implementation Plan (v2.0 - AI Safety & Trust)

## Overview
This implementation plan is based on the unified requirements.md and design.md (v2.0), focusing on the 20-primitive kernel, AI-native features, safety, and trust tracking.

**Key Principles:**
- **Minimal Kernel**: Implement exactly 20 primitives organized into 7 categories (The "Meld 20")
- **Control Flow Primitives**: Use primitive_suspend as the SINGLE kernel control flow primitive (mark_stack, suspend, resume are LIBRARY functions)
- **Library-First**: Build all features (effects, exceptions, async, generators) as library code on top of primitives
- Focus on C++ implementation (meld-cpp) as primary target
- Implement features incrementally with testing
- Prioritize AI-native features and AI safety mechanisms
- Maintain strict library-based control flow (NO keywords — if/else, match/case, try/catch, async/await, flow are all library macros)
- Enforce strict Result[T,E] error handling (exceptions are library effects, not keywords)
- Enable AI sandboxing through algebraic effects
- Track code provenance and trust levels using meta_set/meta_get primitives

**v2.0 Focus Areas:**
- **AI Safety**: Algebraic effects for sandboxing, property-based testing for correctness
- **Trust & Transparency**: Code provenance tracking, shadow history storage
- **Debugging**: Flight recorder for deterministic crash replay
- **Clarity**: Visual logic with flow macro for state machines
- **Performance**: Binary context format (MELD-B) for efficient AI context loading

---

## Completed Foundation (C++ Implementation)

- [x] 1. Implement Meld Kernel primitives and core data structures (The "Meld 20")
  - [x] 1.1 Implement Data Structure Primitives (The Matter)
    - Implement cell primitive as pair (head, tail) for building AST, linked lists, S-expressions
    - Implement vec primitive as contiguous memory block for arrays, strings, buffers
    - Implement symbol primitive as interned unique identifier for variable names, keys, AST nodes
    - Implement type primitive as root metadata for type system enabling runtime type checking
    - Implement scope primitive as dictionary binding symbols to values for environments, modules, closures
    - _Requirements: 1.1, 1.2, 48.2_
    - **Status:** Fully implemented in C++ with modern C++23 features
  
  - [x] 1.2 Implement Scalar Primitives (The Values)
    - Implement int primitive (64-bit signed) as the single integer type
    - Implement float primitive (64-bit IEEE) as the single floating-point type
    - Implement bool primitive (true/false atoms)
    - Implement nil primitive (empty unit/null)
    - _Requirements: 1.6, 1.7, 48.3_
    - **Status:** Fully implemented
  
  - [x] 1.3 Implement Execution Primitives (The Energy)
    - Implement lambda primitive for closure creation capturing current scope
    - Implement apply primitive for function invocation pushing frame to call stack
    - Implement eval primitive as interpreter core turning data (cell) into result
    - Implement quote primitive to prevent evaluation (essential for macros)
    - _Requirements: 48.4_
    - **Status:** Fully implemented
  
  - [x] 1.4 Implement Control Flow Primitives (The Physics)





    - Implement primitive_suspend as the SINGLE kernel control flow primitive
    - Implement delimited continuation capture and restoration
    - Enable primitive_suspend to support all control flow (exceptions, async, generators)
    - _Requirements: 1.8, 48.5, 48.9, 48.10_
    - **Note:** mark_stack, suspend, and resume are LIBRARY functions built on primitive_suspend
  
  - [x] 1.5 Implement Memory & Binding Primitives (The Context)
    - Implement def primitive to define variable in current scope
    - Implement set primitive to mutate variable in nearest defining scope
    - Implement lookup primitive to traverse scope chain to find value
    - _Requirements: 48.6_
    - **Status:** Fully implemented
  -

  - [x] 1.6 Implement AI & Metadata Primitives (The Provenance)





    - Implement meta_set primitive to attach hidden metadata without changing value
    - Implement meta_get primitive to retrieve hidden metadata (provenance, docs, types)
    - Enable metadata attachment to any object or AST node
    - _Requirements: 48.7_
  


  - [x] 1.7 Implement Interop Primitives (The Bridge)










    - Implement native_call primitive to call functions in host environment (C, JVM, JS)
    - Implement native_load primitive to dynamically load shared libraries (.dll, .so)
    - _Requirements: 48.8_
  
  - [x] 1.8 Implement kernel operations
    - Implement cons cell operations (car, cdr, cons) for building data structures
    - Implement kernel operations (apply, gensym, symbol-name, typeof, eq, equal)
    - Implement the AST representation using cons cells to achieve homoiconicity
    - _Requirements: 1.2, 1.4, 1.5_
    - **Status:** Fully implemented

- [x] 2. Implement type system and type hierarchy
  - [x] 2.1 Create type as the root type with self-referential typeof
    - Implement type class with methods for type introspection (name, size, isValueType)
    - Implement typeof operation that returns type instances
    - Ensure type is an instance of itself
    - _Requirements: 2.1, 2.2_
    - **Status:** Fully implemented with comprehensive test cases
  
  - [x] 2.2 Implement specialized type subclasses
    - Create PrimitiveType for kernel primitives
    - Create StructType for value types
    - Create ClassType for reference types
    - Create TraitType for trait definitions
    - Create UnionType and IntersectionType for composite types
    - Create GenericType with variance annotations (in/out/invariant)
    - Implement TypeRegistry for managing all types
    - _Requirements: 2.2, 14.1, 14.5, 14.6_
    - **Status:** Fully implemented with inheritance, generics, and type compatibility checking

- [x] 3. Implement macro system infrastructure
  - [x] 3.1 Create macro definition and registration system
    - Implement macro definition syntax and parser
    - Create macro registry for storing and looking up macros
    - Implement macro expansion engine that transforms AST nodes
    - _Requirements: 2.3, 2.4, 2.5_
  
  - [x] 3.2 Implement hygienic macro expansion
    - Implement automatic gensym for generated symbols
    - Create scope isolation for macro expansion
    - Implement unhygienic annotation for intentional capture
    - _Requirements: 2.3_
  
  - [x] 3.3 Bootstrap core language constructs as macros
    - Implement class macro that generates ClassType instances
    - Implement struct macro that generates StructType instances
    - Implement trait macro that generates TraitType instances
    - _Requirements: 2.4, 2.6, 14.1, 16.4_

- [x] 4. Implement lexer and parser
  - Create lexer for tokenizing Meld source code (val, var, fnc, operators, literals)
  - Implement parser that builds AST from tokens
  - Add support for backtick template strings with ${} interpolation
  - Add support for regular expression literals (/pattern/flags)
  - Implement kebab-case identifier support with context-sensitive parsing
  - Implement parser error reporting with line/column information
  - _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 4.1, 4.2, 4.3, 4.4, 4.5_



- [x] 5. Implement type system core








  - [x] 5.1 Implement struct and class definitions
    - Create struct type with copy-by-value semantics
    - Create class type with reference semantics
    - Implement field storage and access for both types
    - _Requirements: 14.1, 16.1_
    - **Status:** Fully implemented with comprehensive tests
  
  - [x] 5.2 Implement nullability system
    - Create nullable type wrapper (Type? / optional[T] alias for T | nil)
    - Implement safe-navigation operator (?.)
    - Implement elvis operator (?:)
    - Implement smart casting after null checks (flow-sensitive type narrowing)
    - _Requirements: 14A.2, 14D.15, 14D.16, 14D.17_
    - **Note:** Basic nullability infrastructure exists. Compile-time nil rejection, optional[T] structural distinction enforcement, and full flow-sensitive narrowing are covered in task 48.
  
  - [x] 5.3 Implement union and intersection types






    - Create UnionType for Type | Type
    - Create IntersectionType for Type & Type
    - Implement type checking for union/intersection types
    - _Requirements: 14.5_
    - **Note:** UnionMetaType and IntersectionMetaType exist in type system, but runtime support incomplete
  

  - [x] 5.4 Implement generics system





    - Create generic type parameters with variance annotations (in/out)
    - Implement generic type instantiation and substitution
    - Implement type parameter bounds checking
    - Implement support for union types within generic parameters (e.g., List[int | string])
    - Implement support for intersection types within generic parameters (e.g., Array[Foo & Bar])
    - _Requirements: 14.6, 14.7_
    - **Note:** GenericMetaType exists in type system, but runtime support incomplete
  

  - [x] 5.5 Implement type aliases





    - Add type -> syntax support
    - Implement type alias resolution in type checker
    - _Requirements: 14.8_

- [x] 6. Implement memory management
  - [x] 6.1 Implement managed memory for classes
    - Create heap allocator for class instances
    - Implement non-atomic ARC with `Hold[T]`/`View[T]` library types for cycle-safe memory management (see Req 119–139)
    - _Requirements: 16.1, 16.6_
  
  - [x] 6.2 Implement Copyable trait and copy semantics
    - Define Copyable trait interface
    - Implement automatic Copyable for structs
    - Implement .copy() method generation
    - _Requirements: 16.2, 16.3_




- [x] 7. Implement properties system (REWORK: C#-style → @Property macro)

  - [x] 7.1 ~~Implement basic properties with backing fields~~ → REWORK: Implement @Property field-level macro
    - ~~Create property definition syntax~~ REMOVED: C#-style property syntax rejected (Requirement 17.6)
    - ~~Generate implicit backing fields for auto-properties~~ REMOVED
    - ~~Implement property access and assignment~~ REMOVED
    - NEW: Implement @Property as a composite field-level macro
    - NEW: @Property renames field `name` to `_name` (underscore prefix)
    - NEW: @Property changes backing field visibility to package-private
    - NEW: @Property generates public getter `fnc name() -> T { rtn this._name }`
    - NEW: @Property generates public setter `fnc set_name(v: T) { this._name = v }` for mutable fields
    - NEW: Call site uses explicit function syntax: `user.name()` and `user.set_name("Alice")`
    - _Requirements: 17.1, 17.2, 17.3, 17.4, 17.5, 25B.11_

  - [x] 7.2 ~~Implement custom get/set blocks~~ → REMOVED: C#-style get/set blocks rejected
    - ~~Add support for custom getter blocks~~ REMOVED
    - ~~Add support for custom setter blocks with value parameter~~ REMOVED
    - ~~Implement computed properties without backing fields~~ REMOVED
    - NEW: Ensure parser rejects C#-style `var x: T { get { ... } set { ... } }` syntax with clear error message
    - _Requirements: 17.6, 17.7_

  - [x] 7.3 ~~Implement property access modifiers~~ → REWORK: Implement field-level @Getter/@Setter decorators
    - ~~Add support for private set on properties~~ REMOVED
    - ~~Implement access control checking~~ REMOVED
    - NEW: Implement @Getter as field-level decorator (generates single getter, injects via .parent())
    - NEW: Implement @Setter as field-level decorator (generates single setter, injects via .parent())
    - NEW: Implement dual-mode dispatch for @Getter/@Setter (class-level iterates all fields, field-level targets one field)
    - _Requirements: 25B.8, 25B.9, 25B.10_

  - [x] 7.4 Implement property delegation with by macro
    - Create property delegation protocol
    - Implement by as a Standard Library macro that generates delegate field and accessor wiring
    - Create standard delegates (lazy, observable)
    - _Requirements: 17.9_

  - [x] 7.5 Write property tests for @Property macro expansion
    - **Property 70: @Property Macro Expansion Correctness** — For any field `name: T` annotated with @Property inside a class, the macro should rename the field to `_name`, set visibility to pkg-private, generate getter `name() -> T`, and generate setter `set_name(v: T)` for mutable fields
    - **Validates: Requirements 17.1, 17.2, 17.3, 17.4, 17.5, 25B.11**

  - [x] 7.6 Write property tests for C#-style syntax rejection
    - **Property 71: C#-Style Property Syntax Rejection** — For any field declaration using C#-style implicit `get { ... }` or `set { ... }` blocks, the parser should reject the syntax with a clear error message
    - **Validates: Requirements 17.6**

- [x] 7A. Implement AST Parent Pointer Infrastructure
  - [x] 7A.1 Implement .parent() weak reference on AST nodes
    - Add weak_ptr<ASTNode> parent field to base ASTNode class
    - Set parent pointer when child nodes are added to a parent
    - Implement .parent() accessor that returns optional (nil if detached)
    - Ensure parent pointer is a weak reference (not strong) to prevent ARC cycles
    - _Requirements: 2.7, 2.8_

  - [x] 7A.2 Implement parent pointer maintenance during AST mutations
    - Update parent pointers when nodes are moved between parents
    - Clear parent pointer when a node is detached from the tree
    - Ensure macro expansion correctly sets parent pointers on generated nodes
    - _Requirements: 2.7_

  - [x] 7A.3 Write property tests for AST parent pointer invariant
    - **Property 67: AST Parent Pointer Invariant** — For any AST node that is a child of another node, .parent() should return the enclosing parent, and the weak reference should not prevent parent deallocation
    - **Validates: Requirements 2.7, 2.8**

- [x] 7B. Implement Macro Authoring APIs (ast.quote, ast.abort)
  - [x] 7B.1 Implement ast.quote quasiquoting
    - Implement `ast.quote { ... }` syntax for generating AST fragments from template code
    - Implement `${}` interpolation within ast.quote blocks for splicing dynamic values
    - Validate that interpolated expressions produce valid AST node types at compile time
    - Ensure ast.quote produces structurally equivalent AST to manual node construction
    - _Requirements: 2.9_

  - [x] 7B.2 Implement ast.abort structured error reporting
    - Implement `ast.abort(message)` API that halts macro expansion
    - Produce structured compiler error with the provided message and source location
    - Integrate ast.abort errors with the Compiler-Agent Protocol (CAP) for structured JSON output
    - _Requirements: 2.10_

  - [x] 7B.3 Implement node.parent() ?: ast.abort(...) pattern support
    - Ensure the `?:` (elvis) operator works with ast.abort as the fallback
    - Validate that field-level macros can safely access parent via this pattern
    - _Requirements: 2.11_

  - [x] 7B.4 Write property tests for ast.quote round trip
    - **Property 68: ast.quote Round Trip** — For any valid AST fragment, generating it via ast.quote with interpolated values should produce an AST structurally equivalent to manual construction
    - **Validates: Requirements 2.9**

  - [x] 7B.5 Write property tests for ast.abort structured error
    - **Property 69: ast.abort Structured Error** — For any message passed to ast.abort, macro expansion should halt and produce a structured compiler error containing the message and source location
    - **Validates: Requirements 2.10**

- [x] 7C. Implement Field-Level Decorator Infrastructure
  - [x] 7C.1 Implement field-level decorator dispatch
    - Extend decorator application system to detect whether a decorator is applied to a ClassNode or FieldNode
    - Route to class-level or field-level macro implementation based on target node kind
    - _Requirements: 25B.8_

  - [x] 7C.2 Implement parent_class.add_method() API for field-level macros
    - Implement add_method(node) on ClassNode that inserts a generated method into the class body
    - Ensure added methods are properly registered in the class's method table
    - Ensure added methods have correct parent pointers set
    - _Requirements: 25B.13_

  - [x] 7C.3 Implement field-level decorator error handling
    - When a field-level decorator's .parent() returns nil, ensure ast.abort is called with descriptive error
    - Test that applying @Getter/@Setter/@Property to a top-level variable produces a clear error
    - _Requirements: 25B.12_


- [x] 8. Implement tree initialization
  - Implement block-style initialization syntax parsing
  - Create initialization block evaluation logic
  - Implement named parameter assignment within blocks
  - Generate appropriate constructor calls from initialization blocks
  - Support nested initialization blocks
  - _Requirements: 18.1, 18.2, 18.3, 18.4, 18.5_



- [x] 9. Implement tuple system



  - [x] 9.1 Implement tuple type and values


    - Create tuple type representation for heterogeneous ordered collections
    - Implement tuple literal syntax parsing: (a, b, c)
    - Implement named tuple syntax: (x = 1, y = 2)
    - Implement tuple indexing: tuple.0, tuple.1
    - Implement named field access: tuple.x, tuple.y
    - _Requirements: 5.7_
    - **Note:** Cons-based tuples exist in kernel, but parser support incomplete
  


  - [x] 9.2 Implement tuple destructuring





    - Implement destructuring assignment: (val a, val b) = tuple



    - Implement partial destructuring with underscore: (val a, _, val c) = tuple


    - Implement destructuring in function parameters
    - _Requirements: 5.7, 6.5_


- [x] 10. Implement advanced function signatures










  - [x] 10.1 Implement fnc keyword and named parameters



    - Add fnc keyword to parser for function declarations
    - Implement named parameter syntax: name: type
    - Implement parameter default values: name: type = value
    - Implement named parameter calling at call sites
    - _Requirements: 5.1, 5.2, 5.3, 5.10_
  
  - [x] 10.2 Implement named rtn values




    - Implement named rtn value syntax: -> (out1: type, out2: type)
    - Implement rtn value default values: -> (out: type = default)
    - Implement named rtn value assignment in function body
    - Implement tuple rtn value construction
    - _Requirements: 5.5, 5.6, 5.7, 5.9_

- [x] 11. Implement function system









  - [x] 11.1 Implement first-class functions and lambdas


    - Create function type representation
    - Implement lambda syntax parsing (=> and {...})
    - Implement function values and closures
    - Support function types with named returns
    - _Requirements: 19.1, 19.5_
    - **Note:** Lambda primitive exists in kernel, but parser support incomplete
  

  - [x] 11.2 Implement extension methods

    - Implement extend as a Standard Library macro (not a keyword)
    - Implement macro desugaring to method registration via meta_set
    - Implement extension method lookup and dispatch
    - _Requirements: 19.2_
  
  - [x] 11.3 Implement pipeline operator


    - Add |> operator parsing
    - Implement pipeline operator evaluation (right-to-left function application)
    - _Requirements: 19.3_

- [x] 12. Implement Smalltalk-style control flow




  - [x] 12.1 Implement Boolean control flow methods

    - Implement Boolean.ifTrue:ifFalse: methods
    - Create IfTrueBuilder helper class for chaining
    - _Requirements: 20.1, 20.2, 20.7_
  
  - [x] 12.2 Implement Int loop methods

    - Implement Int.times: method for counted loops
    - Implement Int.to: method returning Range
    - Implement Range.do: method for iteration
    - _Requirements: 20.1, 20.3, 20.4_
  
  - [x] 12.3 Implement Block.whileTrue: method

    - Create Block type for closures
    - Implement whileTrue: method for conditional loops
    - _Requirements: 20.1, 20.5_
  
  - [x] 12.4 Implement Collection.forEach: method

    - Implement forEach: method on collection types
    - _Requirements: 20.1, 20.6_

- [x] 13. Implement library-based pattern matching



  - [x] 13.1 Implement PatternMatcher core class


    - Create PatternMatcher[T, R] class with fluent API
    - Implement PatternCase storage and evaluation
    - Create PatternBuilder for fluent chaining
    - _Requirements: 21.1, 21.2_
  
  - [x] 13.2 Implement pattern types


    - Implement literal pattern matching (on(value))
    - Implement type pattern matching (on[Type])
    - Implement multiple value patterns (on(v1, v2, v3))
    - Implement otherwise clause for default case
    - _Requirements: 21.3, 21.4_
  
  - [x] 13.3 Implement pattern guards and destructuring



    - Add when clause for conditional guards
    - Implement destructure[Type] for struct/class destructuring
    - Support guard evaluation in pattern matching
    - _Requirements: 21.5_
  

  - [x] 13.4 Implement extension method and exhaustiveness checking


    - Create .match extension method on all types
    - Implement compile-time exhaustiveness checking macro
    - Generate warnings for non-exhaustive matches
    - Provide helpful suggestions for missing patterns
    - _Requirements: 21.6, 21.7_

- [x] 14. Implement collection processing (LINQ-style)




  - [x] 14.1 Implement fluent collection API

    - Create Collection trait with fluent methods (map, filter, reduce, groupBy, join, sorted, take)
    - Implement method chaining support
    - _Requirements: 22.1_
  

  - [x] 14.2 Implement lazy evaluation







    - Create lazy sequence type for deferred execution
    - Implement lazy intermediate operations
    - Implement terminal operations that trigger evaluation (toList, toSet, forEach)
    - _Requirements: 22.2, 22.5_

  

  - [x] 14.3 Implement parallel execution




    - Add .parallel modifier to collections
    - Implement parallel execution engine for collection operations

    - _Requirements: 22.3_
  
  - [x] 14.4 Implement query macro
    - Implement query as a Standard Library macro (not parser syntax)
    - Implement query macro desugaring to fluent API calls (map, filter, groupBy, join, sorted)
    - Support from, where, join, group, select clauses in macro syntax
    - _Requirements: 22.4_

- [x] 15. Implement multiple dispatch
  - [x] 15.1 Implement dispatch resolution algorithm
    - Create function signature registry
    - Implement type specificity ranking
    - Implement most-specific-match selection
    - _Requirements: 23.1, 23.3, 23.4_
  
  - [x] 15.2 Implement ambiguity detection
    - Create ambiguity checker for multiple matching signatures
    - Generate compile-time errors for ambiguous calls
    - _Requirements: 23.3, 23.5_

- [x] 16. Implement operator system


  - [x] 16.1 Implement opr keyword and operator overloading
    - Add opr keyword as macro wrapping fnc
    - Implement operator method registration with mangled names (e.g., __op_add__)
    - Implement operator dispatch based on type signatures
    - _Requirements: 24A.1, 24A.8, 24A.9_
  
  - [x] 16.2 Implement @infix annotation for infix operators
    - Add @infix annotation with precedence and assoc parameters
    - Implement precedence configuration (default: 5)
    - Implement associativity configuration (left or right)
    - Update parser to handle custom infix operators with configured precedence
    - _Requirements: 24A.2, 24A.5, 24A.6_
  
  - [x] 16.3 Implement @prefix annotation for prefix operators
    - Add @prefix annotation for unary prefix operators
    - Update parser to handle custom prefix operators
    - Implement prefix operator dispatch
    - _Requirements: 24A.3_
  
  - [x] 16.4 Implement @postfix annotation for postfix operators
    - Add @postfix annotation for unary postfix operators
    - Update parser to handle custom postfix operators
    - Implement postfix operator dispatch
    - _Requirements: 24A.4_
  
  - [x] 16.5 Implement ellipsis operator
    - Add ... syntax for rest parameters
    - Add ... syntax for spread in collections
    - Implement rest/spread semantics
    - _Requirements: 5.13, 24C.18_

- [x] 17. Implement compile-time decorators (class-level — see task 7C for field-level)




  - [x] 17.1 Implement decorator infrastructure

    - Create decorator registration system
    - Implement decorator application during compilation
    - _Requirements: 25A.7_
  

  - [x] 17.2 Implement property decorators (class-level only)
    - Implement @Getter decorator (class-level: iterates all fields)
    - Implement @Setter decorator (class-level: iterates all fields)
    - _Requirements: 25A.1_
    - **Note:** Field-level @Getter/@Setter and @Property are in tasks 7.3 and 7.1 (reworked)

  
  - [x] 17.3 Implement method generation decorators
    - Implement @ToString decorator
    - Implement @EqualsAndHashCode decorator

    - _Requirements: 25.2_
  
  - [x] 17.4 Implement constructor decorators
    - Implement @NoArgsConstructor decorator
    - Implement @RequiredArgsConstructor decorator

    - Implement @AllArgsConstructor decorator
    - _Requirements: 25.3_
  
  - [x] 17.5 Implement composite decorators

    - Implement @Data decorator (combines multiple decorators for mutable classes)
    - Implement @Value decorator (combines multiple decorators for immutable classes)
    - _Requirements: 25.4, 25.5_
  
  - [x] 17.6 Implement @Builder decorator
    - Generate builder class with fluent API
    - Generate build() method
    - _Requirements: 25.6_


- [x] 18. Implement runtime metaprogramming



  - [x] 18.1 Implement reflection API

    - Create reflect.typeOf[T]() function
    - Implement type introspection methods (fields, methods)
    - Implement field and method access via reflection
    - _Requirements: 26.1, 26.3, 26.4_
  
  - [x] 18.2 Implement dynamic type creation

    - Implement Meta.createClass(...) API
    - Support dynamic field and method definitions
    - Register dynamically created types with type system
    - _Requirements: 26.2, 26.5_

- [x] 19. Implement standard library collections







  - [x] 19.1 Implement persistent immutable collections
    - Implement List with structural sharing
    - Implement Map with structural sharing
    - Implement Set with structural sharing
    - _Requirements: 19.4, 6.9_

  
  - [x] 19.2 Implement mutable collection variants
    - Implement MutableList



    - Implement MutableMap
    - Implement MutableSet
    - _Requirements: 6.10_



- [x] 20. Implement Result type for error handling



  - [x] 20.1 Implement Result[T, E] type



    - Define Result[T, E] as union type: Success[T] | Error[E]
    - Implement Success and Error variant types
    - Implement isSuccess() and isError() methods
    - Implement value() and error() accessors
    - The E type parameter SHOULD be an anonymous union type (e.g., NetworkError | Timeout)
    - _Requirements: 28.1, 28.2, 28.13_
  
  - [x] 20.2 Implement Result combinators


    - Implement map[U](f: (T) -> U) -> Result[U, E]
    - Implement flatMap[U](f: (T) -> Result[U, E]) -> Result[U, E]
    - Implement mapError[F](f: (E) -> F) -> Result[T, F]
    - Implement library-based pattern matching support for Result
    - Implement unwrap() and unwrapOr(default) methods
    - _Requirements: 28.3, 28.4_


  
  - [x] 20.3 Implement Attempt.run blocks



    - Implement Attempt.run { ... } syntax for error recovery
    - Implement onFailure { error => ... } chaining


    - Implement onSuccess { result => ... } chaining
    - Support automatic Result propagation in Attempt blocks
    - _Requirements: 28.5, 28.6_
  
  - [x] 20.4 Write property tests for Result combinators
    - **Property: Result Functor Laws**
    - **Validates: Requirements 28.3**
  


  - [x] 20.5 Write property tests for Result monad laws
    - **Property: Result Monad Laws**
    - **Validates: Requirements 28.4**

- [x] 21. Implement async library (similar to Boost.ASIO)




  - [x] 21.1 Implement Task[T] type





    - Create Task[T] type for representing asynchronous computations
    - Implement Task.create { ... } factory function
    - Implement .await() method for waiting on task completion
    - Implement task state management (pending, running, completed, cancelled)
    - Store task result or error
    - _Requirements: 27.1, 27.8_
  

  - [x] 21.2 Implement Task combinators

    - Implement .map[U](f: (T) -> U) -> Task[U]
    - Implement .flatMap[U](f: (T) -> Task[U]) -> Task[U]
    - Implement .withTimeout(ms: int) -> Task[T]
    - Implement .onComplete(f: (Result[T, Error]) -> Unit)
    - Implement Task.all(tasks: list[Task[T]]) -> Task[list[T]]
    - Implement Task.race(tasks: list[Task[T]]) -> Task[T]
    - _Requirements: 27.1_
  
  - [x] 21.3 Implement structured concurrency primitives


    - Implement coroutineScope { } function for task scoping
    - Implement launch { } function for creating child tasks
    - Implement task lifetime management (all tasks complete before scope exit)
    - Prevent "fire and forget" operations
    - Track parent-child task relationships
    - _Requirements: 27.2, 27.4, 27.5, 27.6, 27.9, 27.10_
  


  - [x] 21.4 Implement cancellation support

    - Implement task cancellation mechanism
    - Implement CancellationException type
    - Implement checkCancellation() function
    - Implement automatic cancellation propagation through task hierarchies
    - Ensure parent cancellation cancels all children


    - _Requirements: 27.3, 27.4, 27.6_
  
  - [x] 21.5 Integrate with algebraic effects


    - Use algebraic effects for async I/O operations
    - Implement EffectAsync effect type


    - Support perform operations within Task blocks
    - Enable effect handlers to work with async code
    - _Requirements: 27.7_
  
  - [x] 21.6 Implement Runtime.runAsync



    - Create Runtime.runAsync { } function for executing async code
    - Implement event loop for task scheduling


    - Implement task executor with thread pool
    - Support blocking on task completion


    - _Requirements: 27.1, 27.2_
  
  - [x] 21.7 Write property tests for Task combinators
    - **Property 45: Task Functor Laws**
    - **Validates: Requirements 27.1**
  
  - [x] 21.8 Write property tests for structured concurrency
    - **Property 46: Structured Concurrency Lifetime**
    - **Validates: Requirements 27.2, 27.4, 27.5**
  
  - [x] 21.9 Write property tests for cancellation propagation
    - **Property 47: Cancellation Propagation**
    - **Validates: Requirements 27.3, 27.4**

- [x] 22. Implement compiler and code generation
  - [x] 22.1 Implement type checker
    - Implement type inference algorithm
    - Implement type compatibility checking
    - Implement generic type instantiation
    - Generate type errors with helpful messages
    - _Requirements: 14.1, 14.2, 14.6_
  
  - [x] 22.2 Implement intermediate representation (IR)
    - Design IR format for Meld
    - Implement AST to IR transformation
    - Implement IR optimization passes
    - _Requirements: 1.2, 1.4_
  
  - [x] 22.3 Implement code generator
    - Implement IR to target code generation (bytecode or native)
    - Implement runtime library linking
    - _Requirements: 1.1, 1.2_

- [x] 22.5 Implement unsigned types and numeric wrappers (Library-Based)
  - [x] 22.5.1 Implement refinement types for unsigned integers
    - Define uint as refinement type: type uint -> int where { it >= 0 }
    - Implement compile-time validation for statically determinable values
    - Implement runtime validation for dynamic values
    - Generate appropriate error messages for constraint violations
    - _Requirements: 47.2, 47.9_
  
  - [x] 22.5.2 Implement bit-width integer wrappers
    - Implement u8 struct wrapping int with @transpile_as annotations
    - Implement u16 struct wrapping int with @transpile_as annotations
    - Implement u32 struct wrapping int with @transpile_as annotations
    - Implement u64 struct wrapping int with @transpile_as annotations
    - Implement i8 (byte) struct wrapping int with @transpile_as annotations
    - Implement i16 (short) struct wrapping int with @transpile_as annotations
    - Mark all wrappers with @value for copy-by-value semantics
    - _Requirements: 47.3, 47.4, 47.8_
  
  - [x] 22.5.3 Implement bitwise operators for unsigned types
    - Implement logical right shift (>>>) operator for unsigned types
    - Implement bitwise AND, OR, XOR operators
    - Use native_call to invoke host language bitwise operations
    - Ensure correct unsigned semantics for all operations
    - _Requirements: 47.5_
  
  - [x] 22.5.4 Implement literal suffix macros
    - Implement macro for u8 suffix (e.g., 255u8 expands to u8(255))
    - Implement macro for u16 suffix (e.g., 1000u16 expands to u16(1000))
    - Implement macro for u32 suffix (e.g., 0xDEAD_BEEF_u32)
    - Implement macro for u64 suffix
    - Implement macro for i8 suffix (e.g., 100i8 expands to i8(100))
    - Implement macro for i16 suffix (e.g., 30000i16 expands to i16(30000))
    - _Requirements: 47.6_
  
  - [x] 22.5.5 Implement char as Unicode scalar value
    - Implement char struct wrapping int for code point storage
    - Add @transpile_as annotations for target languages (C++ char32_t, Rust char, Java int)
    - Implement ValidChar refinement type with constraint: it.code_point <= 0x10FFFF
    - Implement char literal syntax with single quotes ('A', '🚀')
    - Implement char methods (isDigit, toUpper, toLower, etc.)
    - _Requirements: 47.7_
  
  - [x] 22.5.6 Implement Buffer type for packed arrays
    - Implement Buffer struct with @transpile_as annotations
    - Use vec primitive with packing mode for efficient storage
    - Map to native byte arrays in target languages (C++ std::vector<uint8_t>, Java byte[], Rust Vec<u8>)
    - Ensure efficient memory layout (no 64-bit overhead per element)
    - _Requirements: 47.10_
  
  - [x] 22.5.7 Implement transpiler mappings
    - Configure transpiler to map u8 to native uint8_t (C++), byte (Java), u8 (Rust)
    - Configure transpiler to map i8 to native int8_t (C++), byte (Java), i8 (Rust)
    - Configure transpiler to map i16 to native int16_t (C++), short (Java), i16 (Rust)
    - Configure transpiler to map char to native char32_t (C++), int (Java), char (Rust)
    - Ensure zero-cost abstractions through proper native type mapping
    - _Requirements: 47.4_
  
  - [x] 22.5.8 Write property tests for unsigned type safety
    - **Property 48: Unsigned Type Safety**
    - **Validates: Requirements 47.2, 47.9**
  
  - [x] 22.5.9 Write property tests for numeric wrapper transpilation
    - **Property 49: Numeric Wrapper Transpilation**
    - **Validates: Requirements 47.4**
  
  - [x] 22.5.10 Write property tests for char Unicode validity
    - **Property 51: Char Unicode Validity**
    - **Validates: Requirements 47.7**

- [x] 22.6 Checkpoint - Ensure all core language tests pass
  - Ensure all tests pass for completed tasks 1-22
  - Ask the user if questions arise



- [x] 23. Implement refinement types

  - [x] 23.1 Implement refinement type syntax


    - Add type -> BaseType where { predicate } syntax
    - Parse refinement type definitions
    - _Requirements: 15.1_
  
  - [x] 23.2 Implement compile-time validation


    - Implement static analysis for refinement predicates
    - Generate compile-time errors for statically determinable violations
    - _Requirements: 15.2_
  
  - [x] 23.3 Implement runtime validation





    - Generate runtime checks for dynamic refinement validation
    - Implement .from() method for validated construction
    - Rtn Result[RefinementType, ValidationError] for dynamic validation
    - _Requirements: 15.3_
  
  - [x] 23.4 Implement refinement type composition
    - Support refinement type inheritance
    - Support refinement type composition

    - _Requirements: 15.4_
  

  - [x] 23.5 Implement built-in refinement types
    - Provide common refinement types (PositiveInt, NonEmptyString, Email, etc.)
    - _Requirements: 15.5_

- [x] 24. Implement type projections

  - Implement Omit[T, K] utility type



  - Implement Pick[T, K] utility type

  - Implement Partial[T] utility type
  - Implement Required[T] utility type
  - Implement Readonly[T] utility type
  - _Requirements: 29.1, 29.2, 29.3, 29.4, 29.5_

- [x] 25. Implement namespace macro
  - [x] 25.1 Implement namespace macro
    - Implement namespace as a Standard Library macro (not a keyword)
    - Implement macro desugaring to scope/def/lookup kernel primitives
    - Implement nested namespace support via nested scope creation
    - _Requirements: 31G.22, 31G.23, 31G.25_

- [x] 25A. Implement `imp`-based module and import system
  - [x] 25A.1 Implement `imp` keyword in lexer and parser
    - Add `imp` as a reserved keyword in the lexer
    - Ban `import`, `from`, and `as` keywords from the grammar
    - Parse `imp` as a top-level statement that must appear before any declarations or expressions
    - Emit compile-time error if `imp` appears after non-import declarations
    - _Requirements: 31A.1, 31A.2, 31A.3_

  - [x] 25A.2 Implement basic import form
    - Parse `imp std.math` syntax (dot-separated module path, no assignment)
    - Bind the module to its terminal path segment name (e.g., `math`)
    - Enable dot-notation access on the bound module name (e.g., `math.sin(1.0)`)
    - _Requirements: 31B.4_

  - [x] 25A.3 Implement module aliasing form
    - Parse `imp m = std.math` syntax (identifier `=` module path)
    - Bind the entire module to the custom local name
    - Reuse existing assignment (`=`) semantics
    - _Requirements: 31B.5_

  - [x] 25A.4 Implement named import (destructuring) form
    - Parse `imp { sin, cos } = std.math` syntax (destructuring `=` module path)
    - Extract only the listed symbols into local scope
    - Do not bind the module itself to any name
    - _Requirements: 31B.6_

  - [x] 25A.5 Implement named import with aliasing form
    - Parse `imp { sin -> s, cos -> c } = std.math` syntax (destructuring with `->` mapping)
    - Rename symbols during destructuring using the mapping operator
    - Allow mixing aliased and non-aliased members in the same block (e.g., `{ sin -> s, PI }`)
    - Support multi-line formatting for destructuring blocks
    - _Requirements: 31B.7, 31B.8, 31B.9_

  - [x] 25A.6 Implement module definition and visibility (private-by-default)
    - Treat each `.meld` file as an implicit module with name derived from file path
    - All top-level declarations are private by default (no annotation needed)
    - Implement `@visibility(pub)` annotation to make declarations public to external consumers
    - Implement `@visibility(pkg)` annotation to make declarations visible within the same package but hidden externally
    - _Requirements: 31C.10, 31C.11, 31C.12, 31C.13_

  - [x] 25A.7 Implement module resolution
    - Resolve dot-separated module paths to file system locations
    - Implement resolution order: `std.` prefix → standard library, project-relative paths → `path/to/module.meld`, registered package prefix → package manager or Git URL dependencies defined in `meld.toml`
    - Map dots to directory separators (e.g., `app.services.auth` → `app/services/auth.meld`)
    - Implement `~/` path modifier for importing relative to the package root (e.g., `imp db = "~/src/db/conn"`)
    - _Requirements: 31D.13, 31D.14, 31D.15, 31D.16, 31D.17_

  - [x] 25A.8 Implement circular import prevention
    - Build module dependency graph during compilation
    - Enforce DAG constraint — reject any import cycle at compile time
    - Emit structured error identifying the full cycle path (compatible with CAP)
    - _Requirements: 31E.17, 31E.18, 31E.19_

  - [x] 25A.9 Implement re-exports and facade pattern
    - Allow modules to re-export symbols by importing them and annotating the local alias with `@visibility(pub)`
    - Ensure re-exported symbols appear in the re-exporting module's public API
    - Importers of the re-exporting module gain access without knowing origin modules
    - Implement `module.meld` facade file at the package root (e.g., `src/module.meld`) as the single public API surface
    - The facade imports internal symbols and selectively re-exports them using `@visibility(pub)`
    - Support `entry` field in `meld.toml` specifying the facade file path (defaults to `src/module.meld`)
    - Consumers of a package only see symbols exported through the facade
    - _Requirements: 31F.20, 31F.21, 31F.22, 31F.23, 31F.24, 31F.25_

  - [x] 25A.10 Migrate existing `import` keyword usages to `imp`
    - Update all existing `.meld` example files to use `imp` syntax
    - Update all C++ test files that reference `import` keyword parsing
    - Update any internal compiler/runtime code that uses the old `import` keyword
    - Ensure old `import` keyword produces a helpful error message directing users to `imp`
    - _Requirements: 31A.1, 31A.2_

  - [x] 25A.11 Write property tests for module isolation
    - **Property: Module Isolation** — For any two symbols with the same name in different modules, they should be distinct and not conflict unless explicitly imported into the same scope via `imp`
    - **Validates: Requirements 31A.1, 31D.13, 31E.17, 31G.24**

  - [x] 25A.12 Write property tests for import form correctness
    - **Property: Import Form Equivalence** — For any module M with exported symbol S, all four import forms (basic, aliased, destructured, destructured-with-alias) should provide equivalent access to S
    - **Property: Circular Import Detection** — For any set of modules with a cyclic dependency, the compiler should reject the cycle at compile time
    - **Validates: Requirements 31B.4, 31B.5, 31B.6, 31B.7, 31E.17**

- [x] 26. Implement partial application and currying


  - [x] 26.1 Implement partial application



    - Add placeholder syntax (_ or ?) for unbound arguments
    - Implement partial application that returns new function
    - Support multiple placeholders

    - Support named argument binding
    - _Requirements: 32.1, 32.3, 32.4_
  

  - [x] 26.2 Implement currying




    - Implement .curry() method on functions
    - Add currying syntax: fnc foo(a: int)(b: int) -> result
    - Automatically curry functions when partially applied
    - _Requirements: 32.2, 32.5_

- [x] 27. Implement anonymous types and collection literals
  - [x] 27.1 Implement anonymous object syntax
    - Add curly brace syntax for anonymous objects: {field: value, ...}
    - Implement structural typing for anonymous objects
    - Support nested anonymous objects
    - _Requirements: 33.1_
  
  - [x] 27.2 Implement anonymous map syntax
    - Implement curly brace syntax for maps with homogeneous values
    - Implement map operations (get, keys, values, etc.)
    - _Requirements: 33.2_
  
  - [x] 27.3 Implement anonymous set syntax
    - Implement curly brace syntax for sets with homogeneous elements
    - Implement set operations (contains, add, remove, etc.)
    - _Requirements: 33.3_
  
  - [x] 27.4 Implement anonymous array syntax





    - Implement square bracket syntax for arrays with homogeneous elements
    - Implement array indexing and operations






    - Support multi-dimensional arrays
    - _Requirements: 33.4_
  
  - [x] 27.5 Implement anonymous tuple syntax






    - Implement square bracket syntax for tuples with heterogeneous elements
    - Support named tuple fields: [x: 10, y: 20]
    - Implement tuple indexing and field access
    - _Requirements: 33.5_
  

  - [x] 27.6 Implement type inference for collection literals



    - Implement inference rules for curly braces (object vs map vs set)
    - Implement inference rules for square brackets (array vs tuple)
    - Detect type homogeneity for inference
    - Support explicit type annotations to override inference
    - _Requirements: 33.6, 33.7, 33.8_

- [x] 28. Implement AI-native features

  - [x] 28.1 Implement Structural Search API



    - Implement Code.parse() for converting source to searchable AST
    - Implement ast`` literals for pattern matching with variable capture
    - Implement findAll() method for pattern-based search
    - Implement replace() method for AST-based transformations
    - Implement semantic search capabilities
    - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5_
  
  - [x] 28.2 Implement Holographic View




    - Implement toHologram() method on modules and classes
    - Strip function bodies while preserving signatures
    - Preserve type definitions and contracts
    - Maintain import/export relationships
    - Achieve ~95% token reduction
    - _Requirements: 9.1, 9.2, 9.3, 9.4, 9.5_
  


  - [x] 28.3 Implement Design by Contract (DbC) — require/ensure Macros




    - Implement require as a Standard Library macro for preconditions
    - Implement ensure as a Standard Library macro for postconditions
    - Implement old() function for referencing pre-state values
    - Support contract inheritance in class hierarchies
    - Implement compile-time contract verification where possible
    - _Requirements: 10.1, 10.2, 10.3, 10.4, 10.5_
  

  - [x] 28.4 Implement Compiler-Agent Protocol (CAP)





    - Output structured JSON for all errors and warnings
    - Include fix suggestions in error messages
    - Provide confidence scores for suggested fixes
    - Support batch compilation with structured results
    - Enable incremental compilation with change tracking
    - _Requirements: 11.1, 11.2, 11.3, 11.4, 11.5_
  
  - [x] 28.5 Implement Inline Micro-Tests (test Macro)
    - Implement test as a Standard Library macro for inline micro-tests within function definitions
    - Execute micro-tests during compilation via macro expansion
    - Report micro-test failures as compilation errors
    - Support property-based testing via forall macro within test macro blocks
    - Enable micro-test inheritance in class hierarchies
    - _Requirements: 12.1, 12.2, 12.3, 12.4, 12.5_
  
  - [x] 28.6 Implement @blueprint Macro
    - Provide @blueprint macro for semantic documentation
    - Support summary, intent, cost, and examples fields
    - Generate vector embeddings for semantic search
    - Integrate with IDE tooling for enhanced autocomplete
    - Support inheritance and composition of blueprints
    - _Requirements: 13.1, 13.2, 13.3, 13.4, 13.5_
  
  - [x] 28.7 Implement Auto-MCP Generation
    - Support --target=mcp flag for MCP generation
    - Generate JSON Schema for all public functions
    - Extract tool definitions from @blueprint metadata
    - Create MCP server configuration automatically
    - Support incremental MCP updates
    - _Requirements: 36.1, 36.2, 36.3, 36.4, 36.5_

- [-] 29. Implement polyglot architecture
  - [x] 29.1 Implement JVM backend
    - Implement transpilation to Java source code
    - Generate .java files with classes, interfaces, records
    - Map Meld types to Java types
    - _Requirements: 34.1_
  
  - [ ] 29.2 Implement Rust backend
    - Implement transpilation to Rust source code
    - Generate .rs files with structs, traits, enums
    - Map Result[T,E] to Rust's Result<T, E> type
    - Map ownership and borrowing semantics
    - _Requirements: 34.2_
  
  - [x] 29.3 Implement C++ backend
    - Implement transpilation to C++17 source code
    - Generate C++17 files with smart pointers
    - Map Meld types to C++ types
    - _Requirements: 34.3_
  
  - [x] 29.4 Implement WebAssembly backend
    - Implement WebAssembly compilation
    - Generate .wasm binary output
    - Generate TypeScript definition files (.d.ts) for JS interop
    - _Requirements: 34.4_
  
  - [x] 29.5 Implement meld.toml configuration
    - Parse meld.toml (TOML format) for multi-target project configuration
    - Support `[project]` section with `name`, `version`, `entry`, and `targets` fields
    - Support `[dependencies]` section with Git URL or registry name, version, and `allow` effect permissions
    - Support `[build]` section for build-system-specific configuration
    - Support multiple target definitions and target-specific settings
    - _Requirements: 34.5, 59.1, 59.2, 59.3, 59.4_

- [x] 30. Implement @extern macro for FFI
  - [x] 30.1 Implement @extern macro infrastructure
    - Create @extern macro for foreign function declarations
    - Support multiple target languages (java, rust, cpp, c)
    - _Requirements: 35.1, 35.2_
  
  - [x] 30.2 Implement FFI bindings generation
    - Generate appropriate bindings for each target language
    - Implement type mapping between Meld and target languages
    - Enable seamless interop with existing ecosystems
    - _Requirements: 35.3, 35.4, 35.5_

- [x] 31. Implement Abstract Syntax Graph (ASG)
  - [x] 31.1 Design ASG data structures
    - Define SemanticGraph class with nodes and edges
    - Define edge types: DataFlowEdge, ControlFlowEdge, ScopeEdge, TypeEdge, CallEdge
    - Create index structures for fast symbol and type lookup
    - _Requirements: 38.1, 38.2, 38.3, 38.4_
  
  - [x] 31.2 Implement ASG construction
    - Build ASG during semantic analysis phase
    - Construct data flow edges by tracking variable definitions and uses
    - Construct control flow edges by analyzing statement sequences
    - Construct scope edges by tracking lexical scopes
    - Construct type edges linking values to their types
    - Represent ASG nodes as Meld scopes (kernel primitive) with symbol-tagged metadata
    - Represent ASG edges as Meld cells (kernel primitive) with scope-based metadata
    - Ensure ASG data is queryable using standard Meld pattern matching and structural search
    - Construct call edges for function invocations
    - _Requirements: 38.10_
  
  - [x] 31.3 Implement SemanticGraph API
    - Implement graph.findUsage(symbol) method
    - Implement graph.dataFlow(variable) method with transitive closure
    - Implement graph.controlFlow(node) method
    - Implement graph.scopeAt(location) method
    - Enable bidirectional edge traversal
    - _Requirements: 38.5, 38.6, 38.7, 38.8, 38.9_
  
  - [x] 31.4 Write property tests for ASG
    - **Property 21: ASG Completeness**
    - **Validates: Requirements 38.6**
  
  - [x] 31.5 Write property tests for data flow
    - **Property 22: Data Flow Transitivity**
    - **Validates: Requirements 38.7**

- [x] 32. Implement Explicit Effect Tracking
  - [x] 32.1 Define effect type system
    - Create Effect trait as base type
    - Implement built-in effects: EffectPure, EffectIO, EffectNetwork, EffectState, EffectTime
    - Implement EffectSet for effect composition
    - Support custom user-defined effect types
    - _Requirements: 39.2, 39.6, 39.7_
  
  - [x] 32.2 Implement effects clause parsing
    - Add effects { ... } syntax to function signatures
    - Parse effect declarations
    - Default to EffectPure when no effects clause present
    - _Requirements: 39.1, 39.3_
  
  - [x] 32.3 Implement effect checking
    - Analyze function bodies to detect operations requiring effects
    - Check that all required effects are declared
    - Enforce that calling a function with effects requires caller to declare those effects
    - Generate compile-time errors for undeclared effects
    - _Requirements: 39.4, 39.5, 39.9_
  
  - [x] 32.4 Implement effect inference
    - Infer effects for lambda expressions based on their body
    - Propagate effects through function call chains
    - _Requirements: 39.8_
  
  - [x] 32.5 Implement effect polymorphism
    - Support generic functions parameterized by effects
    - Enable effect composition in generic contexts
    - _Requirements: 39.10_
  
  - [x] 32.6 Write property tests for effect system
    - **Property 23: Effect Soundness**
    - **Validates: Requirements 39.4, 39.5**
  
  - [x] 32.7 Write property tests for effect purity
    - **Property 24: Effect Purity**
    - **Validates: Requirements 39.3, 39.4**

- [x] 33. Implement Binary Context Format (MELD-B)
  - [x] 33.1 Design MELD-B file format
    - Define binary format specification with header, indices, and data sections
    - Design symbol table, type table, cross-reference table, effect table
    - Design ASG serialization format
    - Choose compression algorithm (zstd recommended)
    - _Requirements: 40.2, 40.3, 40.4, 40.5_
  
  - [x] 33.2 Implement MELD-B serialization
    - Implement ASG to binary serialization
    - Build symbol, type, cross-reference, and effect indices
    - Apply compression
    - Write .mldb file with proper format
    - _Requirements: 40.1, 40.2, 40.3, 40.4_
  
  - [x] 33.3 Implement MELD-B deserialization
    - Implement MeldBinary.load(path) API
    - Read and validate header
    - Decompress data
    - Load indices into memory
    - Lazy-load ASG nodes on demand
    - _Requirements: 40.6_
  
  - [x] 33.4 Implement MELD-B query API
    - Implement MeldBinary.query(pattern) for querying loaded contexts
    - Support queries by type, visibility, effects, etc.
    - Provide access to pre-computed indices
    - Enable conversion to SemanticGraph
    - _Requirements: 40.7_
  
  - [x] 33.5 Integrate MELD-B with compiler
    - Add --target=mldb flag to compiler
    - Generate .mldb files during compilation
    - Integrate with Compiler-Agent Protocol (CAP)
    - _Requirements: 40.8, 40.10_
  
  - [x] 33.6 Write property tests for MELD-B
    - **Property 25: MELD-B Round Trip**
    - **Validates: Requirements 40.2, 40.6**
  
  - [x] 33.7 Write property tests for MELD-B compression
    - **Property 26: MELD-B Compression**
    - **Validates: Requirements 40.9**

- [x] 34. Integrate v1.8 features with existing AI-native features
  - [x] 34.1 Update Structural Search API to use ASG
    - Enhance Code.parse() to build ASG
    - Add graph-based query methods
    - Integrate ASG with pattern matching
    - _Requirements: 8.1, 8.2, 8.3, 38.5_
  
  - [x] 34.2 Update Holographic View to export MELD-B
    - Add toMeldB() method to holographic views
    - Optimize hologram storage in binary format
    - _Requirements: 9.1, 40.1_
  
  - [x] 34.3 Update Compiler-Agent Protocol to support MELD-B
    - Add MELD-B as standard exchange format
    - Update CAP to include effect information
    - Support ASG queries in CAP
    - _Requirements: 11.1, 40.10_

- [x] 35. Implement Algebraic Effects System (Library-First with Kernel Primitive)

  - [x] 35.1 Implement primitive_suspend kernel primitive
    - Implement primitive_suspend as the ONLY kernel control flow primitive
    - Implement delimited continuation capture
    - Capture call stack, environment, and program counter
    - Enable jumping to delimiter (handler)
    - Pass captured continuation to callback
    - _Requirements: 1.8, 41.1, 48.5, 48.10_
    - **Note:** This is the foundation for ALL control flow in Meld
  
  - [x] 35.2 Implement library wrapper functions
    - Implement mark_stack as library function using primitive_suspend
    - Implement suspend as library function using primitive_suspend
    - Implement resume as library function using primitive_suspend
    - Create handler stack management in standard library
    - _Requirements: 41.2, 41.3, 41.4, 41.5_
    - **Note:** These are NOT kernel primitives, they are library code

  - [x] 35.3 Define effect definition syntax (Library Macro)
    - Implement effect macro (NOT keyword) for defining abstract effect interfaces
    - Parse effect operation signatures
    - Create EffectDefinition AST nodes
    - Validate effect definitions (operations must be abstract)
    - Generate dispatch logic for effect operations
    - _Requirements: 41.2, 41.25_
  
  - [x] 35.4 Implement perform as library function
    - Implement perform[T](eff: Effect[T]) -> T as standard library function
    - Use primitive_suspend internally to capture continuation
    - Search handler stack for nearest matching handler
    - Pass continuation to handler
    - _Requirements: 41.3, 41.13, 41.14_
    - **Note:** perform is a FUNCTION, not a keyword
  
  - [x] 35.5 Implement handle as library macro
    - Implement handle macro (NOT keyword) that expands to scope management
    - Generate pushScope(handlers) and popScope() calls
    - Expand to try/finally blocks for proper cleanup
    - _Requirements: 41.4_
    - **Note:** handle is a MACRO, not a keyword
  
  - [x] 35.6 Implement effect handler execution
    - Implement handler.handle(eff, continuation) method
    - Support resume() to continue execution from suspension point
    - Support resume(value) to continue with specific rtn value
    - Enable handlers to inspect effect parameters
    - Enable handlers to modify rtn values
    - _Requirements: 41.5, 41.14, 41.15, 41.16_
  
  - [x] 35.7 Implement effect runtime system
    - Create EffectRuntime with handler stack
    - Implement handler registration/deregistration (pushScope/popScope)
    - Implement handler search (innermost first)
    - Support nested handlers with proper precedence
    - _Requirements: 41.13, 41.17_
  
  - [x] 35.8 Implement automatic effect inference
    - Analyze function bodies to detect performed effects
    - Propagate effects through call graph automatically
    - Generate @uses(...) annotations based on inference
    - _Requirements: 41.6, 41.20, 41.21_
  
  - [x] 35.9 Implement IDE integration for ghost annotations
    - Send inferred effects to IDE as inlay hints (ghost text)
    - Display @uses(...) annotations before they're written to disk
    - Update ghost text as code changes
    - _Requirements: 41.8_
  
  - [x] 35.10 Implement annotation persistence
    - Write inferred @uses(...) annotations to source file on save/format
    - Update annotations automatically when implementation changes
    - _Requirements: 41.9, 41.10_
  
  - [x] 35.11 Implement manual annotation constraints
    - Treat manually-written @uses annotations as contracts
    - Validate implementation matches manual annotation
    - Generate compile-time errors for mismatches
    - _Requirements: 41.11, 41.12_
  
  - [x] 35.12 Implement built-in effects
    - Implement FileSystem effect (read, write, delete, exists)
    - Implement Network effect (get, post)
    - Implement Console effect (print, println, readLine)
    - Implement Random effect (nextInt, nextFloat)
    - Implement Time effect (now, sleep)
    - Implement Exception effect (raise)
    - Implement Async effect (wait)
    - Implement Generator effect (yield)
    - _Requirements: 41.19_
  
  - [x] 35.13 Implement System.out namespace
    - Implement System.out.println as library function performing Console.println
    - Implement System.out.print as library function performing Console.print
    - Ensure compiler auto-infers @uses(Console) for System.out calls
    - _Requirements: 41A.1, 41A.2, 41A.3, 41A.4, 41A.5, 41A.6, 41A.7_
  
  - [x] 35.14 Implement exceptions as effects
    - Implement throw as library function performing Exception.raise
    - Implement try/catch as library macros expanding to handle blocks
    - Ensure handlers discard continuations to unwind stack
    - Support custom exception types as effect data payloads
    - _Requirements: 41.22, 28.7, 28.8, 28.9, 28.10_
  
  - [x] 35.15 Implement async/await as effects
    - Implement await as library function performing Async.wait
    - Implement async scheduler as effect handler
    - Store continuations for later resumption
    - _Requirements: 41.23_
  
  - [x] 35.16 Implement generators as effects
    - Implement yield as library function performing Generator.yield
    - Implement generator iterator as effect handler
    - Resume continuations multiple times to produce sequence
    - _Requirements: 41.24_
  
  - [x] 35.17 Write property tests for single control flow primitive
    - **Property 50: Single Control Flow Primitive**
    - **Validates: Requirements 1.8, 48.9, 48.10**
  
  - [x] 35.18 Write property tests for effect handler interception
    - **Property 27: Effect Handler Interception**
    - **Validates: Requirements 41.13, 41.14**
  
  - [x] 35.19 Write property tests for resume continuation
    - **Property 28: Effect Handler Resume Continuation**
    - **Validates: Requirements 41.5, 41.14, 41.15**
  
  - [x] 35.20 Write property tests for handler nesting
    - **Property 29: Effect Handler Nesting**
    - **Validates: Requirements 41.17**
  
  - [x] 35.21 Write property tests for effect inference and annotation
    - **Property 30: Effect Inference and Annotation**
    - **Validates: Requirements 41.6, 41.7, 41.8, 41.9, 41.20, 41.21**
  
  - [x] 35.22 Write property tests for library-based effect implementation
    - **Property 30A: Library-Based Effect Implementation**
    - **Validates: Requirements 41.2, 41.3, 41.4, 41.5, 41.25**
  
  - [x] 35.23 Write property tests for exception-as-effect
    - **Property 30B: Exception-as-Effect Implementation**
    - **Validates: Requirements 28.7, 28.8, 28.9, 28.10, 41.22**
  
  - [x] 35.24 Write property tests for async-as-effect
    - **Property 30C: Async-as-Effect Implementation**
    - **Validates: Requirements 41.23**
  
  - [x] 35.25 Write property tests for generator-as-effect
    - **Property 30D: Generator-as-Effect Implementation**
    - **Validates: Requirements 41.24**
  
  - [x] 35.26 Write property tests for manual annotation constraints
    - **Property 31: Manual Annotation Constraint Enforcement**
    - **Validates: Requirements 41.11, 41.12**
  
  - [x] 35.27 Write property tests for System.out effect inference
    - **Property 45: System.out Effect Inference**
    - **Validates: Requirements 41A.4, 41A.5**
  
  - [x] 35.28 Write property tests for System.out effect interception
    - **Property 46: System.out Effect Interception**
    - **Validates: Requirements 41A.6, 41A.7**

- [x] 36. Implement Property-Based Testing with forall macro
  - [x] 36.1 Implement forall macro parsing
    - Add forall macro recognition within test macro blocks (both forall and test are library macros)
    - Parse quantified variable declarations
    - Parse property body (assertion block)
    - Create PropertyTest AST nodes
    - _Requirements: 42.1_
  
  - [x] 36.2 Implement Generator interface
    - Define Generator[T] interface with generate() and shrink()
    - Create GeneratorRegistry for type-based lookup
    - Support custom generator registration
    - _Requirements: 42.2, 42.8_
  
  - [x] 36.3 Implement built-in generators
    - Implement IntGenerator with edge cases (0, 1, -1, INT_MIN, INT_MAX)
    - Implement StringGenerator with edge cases (empty, whitespace, unicode, long)
    - Implement BoolGenerator
    - Implement FloatGenerator with edge cases (0.0, NaN, Infinity)
    - Implement ListGenerator with edge cases (empty, single, large)
    - Implement generators for other primitive types
    - _Requirements: 42.2, 42.5_
  
  - [x] 36.4 Implement property test execution
    - Create PropertyTest executor
    - Generate random inputs using generators
    - Execute property function with generated inputs
    - Catch assertion failures
    - Run configured number of iterations (default 100)
    - _Requirements: 42.3, 42.4_
  
  - [x] 36.5 Implement shrinking algorithm
    - Implement shrinkInputs() to find minimal failing case
    - Generate shrink candidates from current failing input
    - Test each candidate to see if it still fails
    - Iterate until no simpler failing input found
    - Report minimal counterexample
    - _Requirements: 42.6, 42.7_
  
  - [x] 36.6 Implement configuration support
    - Parse config { ... } blocks in forall tests
    - Support iterations configuration
    - Support timeout configuration
    - Support seed configuration for reproducibility
    - _Requirements: 42.4_
  
  - [x] 36.7 Integrate with test macro
    - Support forall macro within test macro blocks
    - Execute property tests during compilation via macro expansion
    - Report property test failures as compilation errors
    - _Requirements: 42.11_
  
  - [x] 36.8 Implement multi-parameter forall macro
    - Support multiple quantified variables
    - Generate independent random values for each parameter
    - Shrink each parameter independently
    - _Requirements: 42.9_
  
  - [x] 36.9 Integrate with effect handlers
    - Support forall macro tests that use effect handlers
    - Enable deterministic testing of effectful code
    - Ensure property tests work with handle blocks
    - _Requirements: 42.12_
  
  - [x] 36.10 Integrate with Compiler-Agent Protocol
    - Expose property test results in CAP
    - Include counterexamples in error messages
    - Provide property coverage metrics
    - _Requirements: 42.10_
  
  - [x] 36.11 Write property tests for test coverage
    - **Property 31: Property-Based Test Coverage**
    - **Validates: Requirements 42.3, 42.4, 42.5**
  
  - [x] 36.12 Write property tests for shrinking
    - **Property 32: Property-Based Test Shrinking**
    - **Validates: Requirements 42.6, 42.7**
  
  - [x] 36.13 Write property tests for determinism with effects
    - **Property 33: Property-Based Test Determinism with Effects**
    - **Validates: Requirements 42.12**

- [x] 37. Implement Visual Logic (flow Macro)
  - [x] 37.1 Implement flow macro parsing
    - Add flow macro to Standard Library MMS
    - Parse flow definition with initial state
    - Parse state blocks with transitions
    - Create FlowDefinition AST nodes
    - _Requirements: 43.1, 43.2, 43.3_
  
  - [x] 37.2 Implement transition parsing
    - Parse on(Event) => goto TargetState syntax
    - Parse guard conditions with when clause
    - Create Transition AST nodes
    - Validate transition syntax
    - _Requirements: 43.4, 43.5, 43.10_
  
  - [x] 37.3 Implement entry/exit action parsing
    - Parse entry { ... } blocks in states
    - Parse exit { ... } blocks in states
    - Create entry/exit action AST nodes
    - _Requirements: 43.11_
  
  - [x] 37.4 Implement flow macro validation
    - Validate that initial state exists
    - Validate that all goto targets exist
    - Check for unreachable states
    - Warn about missing terminal states
    - Generate compile-time errors for invalid flows
    - _Requirements: 43.6, 43.7_
  
  - [x] 37.5 Implement flow macro expansion and code generation
    - Generate FlowDefinition data structure
    - Generate FlowInstance class with trigger() method
    - Generate state transition logic
    - Generate entry/exit action execution
    - Generate guard condition evaluation
    - _Requirements: 43.8, 43.10, 43.11_
  
  - [x] 37.6 Implement flow macro runtime support
    - Create FlowInstance runtime class
    - Implement trigger(event) method
    - Implement state transition execution
    - Implement getCurrentState() method
    - Implement isTerminal() method
    - _Requirements: 43.12_
  
  - [x] 37.7 Implement IDE integration for flow macro visualization
    - Export flow macro definitions in visual format
    - Generate flowchart representation
    - Support IDE rendering of state machines
    - _Requirements: 43.9_
  
  - [x] 37.8 Write property tests for state transitions
    - **Property 35: Flow State Transition Validity**
    - **Validates: Requirements 43.4, 43.5, 43.6**
  
  - [x] 37.9 Write property tests for guard conditions
    - **Property 36: Flow Guard Condition Enforcement**
    - **Validates: Requirements 43.10**
  
  - [x] 37.10 Write property tests for entry/exit actions
    - **Property 37: Flow Entry/Exit Action Execution**
    - **Validates: Requirements 43.11**

- [x] 38. Implement Flight Recorder
  - [x] 38.1 Design snapshot data structures
    - Define ExecutionSnapshot structure
    - Define EffectOperation structure
    - Define StackFrame structure
    - Design serialization format
    - _Requirements: 44.2, 44.3, 44.4_
  
  - [x] 38.2 Implement snapshot capture
    - Implement Runtime.snapshot() API
    - Capture function inputs
    - Capture effect history
    - Capture call stack with local variables
    - Capture environment metadata
    - _Requirements: 44.1, 44.2, 44.3, 44.4, 44.11_
  
  - [x] 38.3 Implement automatic crash capture
    - Hook into error handling system
    - Automatically capture snapshot on unhandled errors
    - Save snapshot to disk
    - _Requirements: 44.6_
  
  - [x] 38.4 Implement snapshot serialization
    - Implement saveTo(path) method
    - Serialize all snapshot data
    - Support compression
    - _Requirements: 44.5_
  
  - [x] 38.5 Implement snapshot deserialization
    - Implement loadFrom(path) static method
    - Deserialize snapshot data
    - Validate snapshot format
    - _Requirements: 44.5_
  
  - [x] 38.6 Implement replay system
    - Implement Runtime.replay(snapshot) API
    - Restore function inputs
    - Install replay effect handlers
    - Execute code with recorded effects
    - _Requirements: 44.7, 44.8_
  
  - [x] 38.7 Implement effect recording
    - Record all effect operations during execution
    - Store effect parameters and results
    - Maintain effect history log
    - _Requirements: 44.3_
  
  - [x] 38.8 Implement replay effect handlers
    - Create handlers that rtn recorded results
    - Verify arguments match recording
    - Handle mismatches gracefully
    - _Requirements: 44.8_
  
  - [x] 38.9 Implement configuration support
    - Create FlightRecorderConfig structure
    - Support enabled/disabled flag
    - Support max snapshot size limit
    - Support snapshot directory configuration
    - _Requirements: 44.10_
  
  - [x] 38.10 Integrate with Compiler-Agent Protocol
    - Export snapshots in CAP-compatible format
    - Enable AI agents to load and analyze snapshots
    - _Requirements: 44.9_
  
  - [x] 38.11 Implement snapshot comparison
    - Support comparing snapshots for regression analysis
    - Highlight differences between snapshots
    - _Requirements: 44.12_
  
  - [x] 38.12 Write property tests for snapshot completeness
    - **Property 37: Flight Recorder Snapshot Completeness**
    - **Validates: Requirements 44.2, 44.3, 44.4, 44.11**
  
  - [x] 38.13 Write property tests for replay determinism
    - **Property 38: Flight Recorder Replay Determinism**
    - **Validates: Requirements 44.7, 44.8**

- [x] 38.5 Implement metadata primitives (meta_set and meta_get)
  - [x] 38.5.1 Implement meta_set kernel primitive
    - Implement meta_set(obj, key, val) -> obj primitive
    - Attach hidden metadata to any object or AST node
    - Ensure metadata doesn't affect object value or behavior
    - Support metadata for provenance, documentation, types
    - _Requirements: 48.7_
  
  - [x] 38.5.2 Implement meta_get kernel primitive
    - Implement meta_get(obj, key) -> val primitive
    - Retrieve hidden metadata from objects
    - Rtn nil if metadata key doesn't exist
    - _Requirements: 48.7_
  
  - [x] 38.5.3 Write property tests for metadata preservation
    - **Property 52: Metadata Preservation**
    - **Validates: Requirements 48.7**

- [-] 39. Implement Code Provenance and Trust Model
  - [x] 39.1 Design provenance metadata structures
    - Define OriginType enum (Human, Agent, Verified)
    - Define ProvenanceMetadata structure
    - Use meta_set to attach provenance to AST nodes
    - Use meta_get to retrieve provenance from AST nodes
    - _Requirements: 45.1, 45.2, 45.3, 45.4_
  
  - [x] 39.2 Implement provenance tracking during parsing
    - Mark user-typed code as Human origin using meta_set
    - Track creation timestamps
    - Store provenance metadata on AST nodes
    - _Requirements: 45.2_
  
  - [x] 39.3 Implement provenance tracking for AI generation
    - Mark AI-generated code as Agent origin
    - Store agent model name
    - Store confidence scores
    - Link to @blueprint
    - _Requirements: 45.3_
  
  - [x] 39.4 Implement provenance verification
    - Implement markAsVerified() method
    - Store reviewer information
    - Store verification timestamp
    - Update origin to Verified
    - _Requirements: 45.4_
  
  - [x] 39.5 Implement provenance mismatch detection
    - Detect when code changes but blueprint doesn't
    - Detect when blueprint changes but code doesn't
    - Generate compiler warnings for mismatches
    - Provide suggestions for resolution
    - _Requirements: 45.9, 45.10_
  
  - [x] 39.6 Implement trust score calculation
    - Calculate trust scores based on origin and confidence
    - Support trust level thresholds
    - _Requirements: 45.13_
  
  - [x] 39.7 Implement IDE integration for trust heatmap
    - Generate color tinting based on origin
    - Green for Human/Verified
    - Purple for high-confidence Agent
    - Red for low-confidence Agent
    - Implement gutter icons
    - Implement hover tooltips
    - _Requirements: 45.5, 45.6, 45.7, 45.8_
  
  - [x] 39.8 Implement provenance query API
    - Implement Provenance.query(node) API
    - Support filtering by origin type
    - Support filtering by confidence
    - _Requirements: 45.11, 45.14_
  
  - [x] 39.9 Integrate with MELD-B format
    - Preserve provenance in binary format
    - Support provenance queries on binary contexts
    - _Requirements: 45.12_
  
  - [x] 39.10 Implement compiler trust level enforcement
    - Add --trust-level command-line flag
    - Enforce minimum trust requirements
    - Generate errors for code below threshold
    - _Requirements: 45.13_
  
  - [ ] 39.11 Write property tests for provenance preservation
    - **Property 39: Provenance Metadata Preservation**
    - **Validates: Requirements 45.1, 45.12**
  
  - [ ] 39.12 Write property tests for mismatch detection
    - **Property 40: Provenance Mismatch Detection**
    - **Validates: Requirements 45.9, 45.10**
  
  - [ ] 39.13 Write property tests for trust filtering
    - **Property 41: Trust Level Filtering**
    - **Validates: Requirements 45.14**

- [-] 40. Implement Shadow Provenance
  - [x] 40.1 Design history storage structures
    - Define HistoryEntry structure
    - Define Message structure
    - Define CodeVersion structure
    - Design SQLite database schema
    - _Requirements: 46.1, 46.2_
  
  - [x] 40.2 Implement history database
    - Create .meld/history directory structure
    - Implement SQLite database for indexing
    - Create tables for history, versions, conversations
    - Implement indices for fast lookup
    - _Requirements: 46.1, 46.6_
  
  - [x] 40.3 Implement history recording
    - Record AI generation conversations
    - Record code refinements
    - Record version history
    - Link history to AST nodes via node ID
    - _Requirements: 46.2, 46.3_
  
  - [x] 40.4 Implement conversation storage
    - Save conversations to JSON files
    - Store in .meld/history/conversations/
    - Include timestamps and agent model
    - _Requirements: 46.2, 46.8_
  
  - [x] 40.5 Implement version storage
    - Save previous code versions
    - Store in .meld/history/versions/
    - Include change reasons
    - _Requirements: 46.2_
  
  - [x] 40.6 Implement History API
    - Implement History.query(functionName) method
    - Implement getConversation() method
    - Implement getVersions() method
    - Implement getBlueprintHistory() method
    - _Requirements: 46.7_
  
  - [x] 40.7 Implement IDE integration
    - Generate gutter heatmap based on version count
    - Implement hover tooltips with history summary
    - Implement history panel for viewing conversations
    - Implement version timeline visualization
    - _Requirements: 46.4, 46.5, 46.11_
  
  - [x] 40.8 Implement history export
    - Support --include-history flag
    - Export code with full history context
    - Generate portable history archives
    - _Requirements: 46.9_
  
  - [x] 40.9 Implement history cleanup
    - Support --clean flag for old history
    - Support --older-than parameter
    - Clean up old conversations and versions
    - _Requirements: 46.6_
  
  - [x] 40.10 Implement blueprint-implementation tracking
    - Track relationship between blueprints and implementations
    - Store original and current blueprints
    - Track blueprint evolution
    - _Requirements: 46.12_
  
  - [x] 40.11 Ensure source code separation
    - Validate that source files don't contain inline history
    - Enforce separation during compilation
    - _Requirements: 46.1, 46.4_
  
  - [x] 40.12 Write property tests for history linkage
    - **Property 42: Shadow History Linkage**
    - **Validates: Requirements 46.3, 46.7**
  
  - [x] 40.13 Write property tests for history separation
    - **Property 43: Shadow History Separation**
    - **Validates: Requirements 46.1, 46.4**
  
  - [-] 40.14 Write property tests for blueprint tracking
    - **Property 44: Blueprint-Implementation Relationship**
    - **Validates: Requirements 46.12**

- [x] 41. Integrate v2.0 features with existing systems
  - [x] 41.1 Update @blueprint macro for Doc-Driven Generation
    - Add rules field as array of generation constraints to @blueprint
    - Update examples field to support input-output pairs
    - Remove cost field (deprecated in v2.0)
    - Remove intent field (deprecated in v2.0)
    - Integrate with shadow provenance for tracking blueprint evolution
    - Link blueprints to implementations via AST node IDs
    - Support blueprint inheritance and composition
    - Enable runtime querying of blueprints
    - _Requirements: 13.1, 13.2, 13.3, 13.4, 13.7, 13.9, 13.10_
  
  - [x] 41.2 Update Compiler-Agent Protocol for v2.0
    - Include provenance information in CAP output
    - Include flight recorder snapshots in error reports
    - Support flow macro visualization in CAP
    - _Requirements: 11.1_
  
  - [x] 41.3 Update compilation pipeline
    - Add provenance tracking phase
    - Add flow macro validation phase
    - Add provenance mismatch detection phase
    - Add shadow history storage phase
    - Add flight recorder integration phase
    - _Requirements: 45.1, 43.6, 45.9, 46.1, 44.6_
  
  - [x] 41.4 Update MELD-B format for v2.0
    - Include provenance metadata in binary format
    - Include flow macro definitions in binary format
    - Support querying by provenance
    - _Requirements: 45.12, 40.2_

- [x] 42. Implement tooling and ecosystem support
  - [x] 42.1 Implement REPL
    - Implement read-eval-print loop
    - Add support for :type command for type inspection
    - Add support for :expand command for macro expansion
    - Implement incremental compilation for REPL
    - _Requirements: 37.1_
  
  - [x] 42.2 Implement LSP (Language Server Protocol) support
    - Implement LSP server for Meld
    - Implement autocomplete with type information
    - Implement go-to-definition (including macro-generated code)
    - Implement find references
    - Implement rename refactoring
    - Implement inline error diagnostics
    - Implement hover for documentation
    - _Requirements: 37.4_
  
  - [x] 42.3 Implement debugging support
    - Implement debug information generation
    - Implement breakpoint support
    - Implement step-through debugging
    - Implement variable inspection
    - Implement async task visualization
    - _Requirements: 37.5_
  
  - [x] 42.4 Implement hot reload
    - Support hot reload for development
    - Implement incremental recompilation
    - _Requirements: 37.6_
  
  - [x] 42.5 Implement AI coding assistant integration
    - Integrate with AI coding assistants
    - Provide structured code context
    - Support AI-driven code generation
    - _Requirements: 37.7_

- [x] 43. Final Checkpoint - Complete system integration
  - Ensure all tests pass across all modules
  - Verify integration between v2.0 features (effects, provenance, flight recorder, flow macro)
  - Validate AI-native features work with v2.0 safety features
  - Run end-to-end integration tests
  - Ask the user if questions arise

- [x] 44. Implement Mutating Methods (`var fnc`) and Mutable Parameters (`var`)
  - [x] 44.1 Implement `var fnc` syntax in lexer and parser
    - Add `var fnc` as a compound declaration form in the parser
    - Parse `var fnc method_name(params) -> ReturnType` syntax
    - Create MutatingMethodDeclaration AST node (or flag on existing FunctionDeclaration)
    - Ensure `var fnc` is only valid on methods (not free functions)
    - _Requirements: 57.1, 57.2_

  - [x] 44.2 Implement `var` parameter modifier
    - Parse `var` modifier before parameter names (e.g., `fnc update(u: var User)`)
    - Create MutableParameter AST node (or flag on existing ParameterDeclaration)
    - Default all parameters to immutable (`val`) when no modifier present
    - _Requirements: 57.4, 57.5_

  - [x] 44.3 Implement compiler enforcement for `var fnc`
    - Analyze method bodies to detect mutation of `this`/receiver
    - Reject methods that mutate `this` without `var fnc` declaration
    - Reject `var fnc` methods that don't actually mutate `this` (warning or error)
    - _Requirements: 57.2, 57.3_

  - [x] 44.4 Implement compiler enforcement for `var` parameters
    - Track parameter mutability through function bodies
    - Reject mutation of parameters not declared with `var`
    - Ensure `var` parameter mutation is visible at the call site
    - _Requirements: 57.5, 57.6, 57.7_

  - [x] 44.5 Implement trait compatibility for `var fnc`
    - Allow `var fnc` in trait method declarations
    - Enforce that implementations of `var fnc` trait methods also use `var fnc`
    - Reject implementations that add `var fnc` when trait doesn't declare it
    - _Requirements: 57.8_

  - [x] 44.6 Write property tests for mutating method enforcement
    - **Property 53: Mutating Method Declaration Enforcement** — For any method M that mutates `this`, the compiler accepts M if and only if M is declared with `var fnc`
    - **Validates: Requirements 57.1, 57.2, 57.3**

  - [x] 44.7 Write property tests for mutable parameter enforcement
    - **Property 54: Mutable Parameter Enforcement** — For any function parameter P, mutation of P is accepted if and only if P is declared with `var`
    - **Validates: Requirements 57.4, 57.5, 57.6**

  - [x] 44.8 Write property tests for trait `var fnc` compatibility
    - **Property 55: Trait var fnc Compatibility** — For any trait T with method M, an implementation of M must use `var fnc` if and only if T declares M with `var fnc`
    - **Validates: Requirements 57.8**

- [-] 45. Implement Effect Firewall — Dependency Sandboxing
  - **Note:** Core implementation (EffectChecker, EffectFirewall) completed in meld-unified-cli spec tasks 31-32. See `meld-core/src/effects/effect_checker.cpp` and `effect_firewall.cpp`. Remaining work is property tests only.

  - [x] 45.1 Implement dependency effect permission parsing _(completed in meld-unified-cli task 31)_
    - _Requirements: 58.2, 58.4_

  - [x] 45.2 Implement dependency effect verification _(completed in meld-unified-cli task 31)_
    - _Requirements: 58.1, 58.3_

  - [x] 45.3 Implement transitive effect enforcement _(completed in meld-unified-cli task 32)_
    - _Requirements: 58.5_

  - [x] 45.4 Integrate with `@uses` annotation system _(completed in meld-unified-cli task 31)_
    - _Requirements: 58.6_

  - [x] 45.5 Integrate with Compiler-Agent Protocol (CAP) _(completed in meld-unified-cli task 32)_
    - _Requirements: 58.7_

  - [x] 45.6 Exempt standard library from Effect Firewall _(completed in meld-unified-cli task 32)_
    - _Requirements: 58.8_

  - [ ] 45.7 Write property tests for effect firewall enforcement
    - **Property 56: Effect Firewall Enforcement** — For any dependency D performing effect E, the build succeeds if and only if E is listed in D's `allow` array (or D is a standard library dependency)
    - **Validates: Requirements 58.1, 58.2, 58.3, 58.4, 58.8**

  - [ ] 45.8 Write property tests for transitive effect permissions
    - **Property 57: Transitive Effect Permission Superset** — For any dependency chain A → B, A's `allow` must be a superset of B's required effects for the build to succeed
    - **Validates: Requirements 58.5**

  - [ ] 45.9 Write property tests for CAP integration
    - **Property 58: Effect Firewall CAP Error Output** — For any effect permission violation, the CAP output includes the undeclared effect, the offending dependency, and a fix suggestion
    - **Validates: Requirements 58.7**

- [ ] 46. Implement `meld.toml` as Single Source of Truth
  - [ ] 46.1 Implement compiler `meld.toml` reader
    - Read and parse `meld.toml` at project root during compilation startup
    - Extract `[project]` section: `name`, `version`, `entry`, `targets`
    - Extract `[dependencies]` section: Git URLs/registry names, versions, `allow` arrays
    - Extract `[build]` section: build-system-specific configuration
    - When `meld.toml` is absent, treat project as single-file script with defaults
    - _Requirements: 59.1, 59.2, 59.3, 59.4, 59.5, 59.8_

  - [ ] 46.2 Implement dependency resolution from `meld.toml`
    - Resolve Git URL dependencies to local checkouts
    - Resolve registry name dependencies to cached packages
    - Feed resolved dependency paths into the compiler's module resolution
    - _Requirements: 59.3, 59.5_

  - [ ] 46.3 Implement compilation target selection from `meld.toml`
    - Read `targets` from `[project]` section
    - Select appropriate backend(s) based on target list
    - Support multi-target compilation in a single build
    - _Requirements: 59.2, 59.5_

  - [ ] 46.4 Implement `rules_meld` Bazel integration
    - Parse `meld.toml` during Bazel workspace evaluation
    - Auto-generate `meld_library` targets from declared dependencies
    - Pipe `allow` effect arrays to compiler CLI flags (e.g., `--allow-effects=net,fs`)
    - _Requirements: 59.6, 59.7_

  - [ ] 46.5 Write property tests for `meld.toml` parsing
    - **Property 59: meld.toml Round-Trip Parsing** — For any valid `meld.toml` configuration, the compiler parses all sections and the extracted values match the original TOML content
    - **Validates: Requirements 59.1, 59.2, 59.3, 59.4**

  - [ ] 46.6 Write property tests for absent `meld.toml` defaults
    - **Property 60: Absent meld.toml Default Behavior** — When `meld.toml` is absent, the compiler treats the project as a single-file script with no dependencies and default settings
    - **Validates: Requirements 59.8**

- [-] 47. Implement Control Flow Quintet Operators
  - [x] 47.1 Implement safe-chaining operators (`?.`, `?[]`, `?()`)
    - Add `?.` (safe navigation) token to lexer
    - Add `?[]` (safe indexing) token to lexer
    - Add `?()` (safe invocation) token to lexer
    - Implement parser support for chained safe-navigation expressions (e.g., `user?.address?.zip`)
    - Implement parser support for safe-indexing expressions (e.g., `users?[0]?.name`)
    - Implement parser support for safe-invocation expressions (e.g., `callback?()`)
    - Implement type checker: operators target `optional[T]` only, reject `Result[T, E]`
    - Implement short-circuit semantics: if left side is `nil`, yield `nil` for the expression chain (do NOT return from function)
    - Implement compiler enforcement: these operators are non-overloadable (reject `opr ?.` definitions)
    - _Requirements: 14B.7, 14B.8, 14B.9, 14B.10, 14B.11, 24C.19_

  - [x] 47.2 Implement safe-return operator (`?`)
    - Add postfix `?` token to lexer (distinct from ternary or other `?` usage)
    - Implement parser support for postfix `?` on expressions (e.g., `val addr = user?.address?`)
    - Implement type checker: operator targets `optional[T]` only, reject `Result[T, E]`
    - Implement function-level return semantics: if value is `nil`, immediately return `nil` from enclosing function
    - Verify enclosing function return type is compatible with `optional[T]`
    - Implement compiler enforcement: operator is non-overloadable
    - _Requirements: 14C.12, 14C.13, 14C.14, 24C.20_

  - [x] 47.3 Implement error-propagation operator (`?!`)
    - Add postfix `?!` token to lexer
    - Implement parser support for postfix `?!` on expressions (e.g., `val content = read-file(path)?!`)
    - Implement type checker: operator targets `Result[T, E]` only, reject `optional[T]`
    - Implement unwrap semantics: if `Ok(v)`, unwrap to `v`; if `Err(e)`, immediately return `Err(e)` from enclosing function
    - Verify enclosing function return type is compatible with `Result[_, E]`
    - Implement compiler enforcement: operator is non-overloadable
    - _Requirements: 28.14, 28.15, 28.16, 24C.22_

  - [x] 47.4 Implement force-unwrap / panic operator (`!!`)
    - Add postfix `!!` token to lexer
    - Implement parser support for postfix `!!` on expressions
    - Implement type checker: operator targets BOTH `optional[T]` and `Result[T, E]`
    - Implement panic semantics: if `nil` or `Err`, crash the current Fiber/Actor
    - Implement compiler enforcement: operator is non-overloadable
    - _Requirements: 14E.18, 14E.19, 24C.23_

  - [-] 47.4a Harden elvis operator (`?:`) type enforcement
    - Verify existing `?:` implementation (from task 5.2) rejects `Result[T, E]` operands at the type-checker level
    - Add type-checker validation: if left operand is `Result[T, E]`, emit compile-time error directing developer to use `?!` instead
    - Ensure `?:` is included in the non-overloadable operator list (task 47.5)
    - Add test cases: `result ?: default` → compile error; `optional-val ?: default` → OK
    - _Requirements: 14D.16, 14D.17, 24C.21_

  - [ ] 47.5 Implement non-overloadable operator enforcement
    - Add forbidden operator list to compiler's operator registration system
    - Reject `opr` definitions for all structural tokens (`.`, `{}`, `[]` in type position, `:`, `;`, `...`)
    - Reject `opr` definitions for all quintet operators (`?.`, `?[]`, `?()`, `?`, `?:`, `?!`, `!!`)
    - Generate clear compile-time error messages explaining why these operators are reserved
    - _Requirements: 24C.13, 24C.14, 24C.15, 24C.16, 24C.17, 24C.18, 24C.19, 24C.20, 24C.21, 24C.22, 24C.23_

  - [ ] 47.6 Implement trait-based operator contracts
    - Define standard operator traits in `std.core`: `Addable`, `Subtractable`, `Comparable`, `Indexable`, etc.
    - Implement trait-based dispatch: types must implement the corresponding trait to overload a standard operator
    - Implement `Indexable[K, V]` trait with `opr [](key: K) -> V` and `opr []=(key: K, val: V)` for expression-position `[]`
    - Ensure custom operators (user-defined symbols) use standalone `opr` definitions without requiring a trait
    - _Requirements: 24B.10, 24B.11, 24B.12_

  - [ ] 47.7 Write property tests for safe-chaining operators
    - **Property 61: Safe Chaining Short-Circuit** — For any expression chain using `?.`, `?[]`, or `?()` on an `optional[T]` value, if any link in the chain is `nil`, the entire chain evaluates to `nil` without returning from the function
    - **Validates: Requirements 14B.7, 14B.8, 14B.9, 14B.10**

  - [ ] 47.8 Write property tests for safe-return operator
    - **Property 62: Safe Return Nil Propagation** — For any function using the `?` postfix operator on an `optional[T]` value, if the value is `nil`, the function immediately returns `nil`
    - **Validates: Requirements 14C.12, 14C.13**

  - [ ] 47.9 Write property tests for error-propagation operator
    - **Property 63: Error Propagation Correctness** — For any function using the `?!` postfix operator on a `Result[T, E]` value, `Ok(v)` unwraps to `v` and `Err(e)` immediately returns `Err(e)` from the enclosing function
    - **Validates: Requirements 28.14, 28.15**

  - [ ] 47.10 Write property tests for force-unwrap operator
    - **Property 64: Force Unwrap Panic Semantics** — For any `!!` operation on `optional[T]` or `Result[T, E]`, presence/success unwraps the value, while `nil`/`Err` panics the current Fiber/Actor
    - **Validates: Requirements 14E.18, 14E.19**

  - [ ] 47.11 Write property tests for non-overloadable enforcement
    - **Property 65: Non-Overloadable Operator Enforcement** — For any attempt to define an `opr` for a reserved operator (structural tokens or quintet), the compiler rejects the definition with a clear error
    - **Validates: Requirements 24C.13–24C.23**

  - [ ] 47.12 Write property tests for Absence vs Failure separation
    - **Property 66: Absence vs Failure Type Separation** — For any quintet operator, `?.`/`?[]`/`?()`/`?`/`?:` reject `Result[T, E]` operands, and `?!` rejects `optional[T]` operands, ensuring the two concerns are never conflated
    - **Validates: Requirements 14B.10, 14C.13, 14D.16, 28.15**

  - [ ] 47.13 Create Control Flow Quintet example files
    - Create `meld-core/examples/control-flow-quintet-demo.meld` with a comprehensive Meld example demonstrating all five quintet operators in a real-world scenario (database query, nested optional access, error propagation, defaults, force unwrap)
    - Create `meld-core/examples/control-flow-quintet-demo.cpp` with C++ test harness that parses and validates the `.meld` file
    - Include examples of structural token non-overloadability (compiler rejection of `opr .` etc.)
    - Include examples of quintet operator non-overloadability (compiler rejection of `opr ?.` etc.)
    - Follow workspace `.meld`/`.cpp` separation guidelines
    - _Requirements: 24C.13–24C.23, 14B.7–14B.11, 14C.12–14C.14, 14D.15–14D.17, 14E.18–14E.19, 28.14–28.16_

  - [ ] 47.14 Add reserved operator rationale to compiler error messages
    - Ensure all compile-time errors for forbidden operator overloading include a brief rationale (e.g., "`.` is a structural token and cannot be overloaded — it always means raw field access or method dispatch")
    - Ensure quintet operator errors explain the Absence vs Failure separation (e.g., "`?:` is reserved for `optional[T]` nil-coalescing and cannot be overloaded — use `?!` for Result error propagation")
    - Integrate error messages with CAP structured JSON output for AI agent consumption
    - _Requirements: 24C.13–24C.23, 11.1, 11.2_

- [-] 48. Implement Nil Handling and Compile-Time Null Safety
  - [x] 48.1 Implement compile-time nil rejection for non-optional types
    - Add semantic analysis pass that rejects `nil` assignment to non-optional types (e.g., `val x: string = nil` → compile error)
    - Add semantic analysis pass that rejects `nil` as argument where non-optional parameter is expected (e.g., `greet(nil)` where `greet(user: User)`)
    - Add semantic analysis pass that rejects `nil` return from functions with non-optional return types
    - Generate clear compile-time error messages via CAP (e.g., "cannot assign nil to non-nullable type string")
    - _Requirements: 14A-NIL.20, 14A-NIL.21_

  - [ ] 48.2 Implement optional[T] structural distinction enforcement
    - Ensure the type checker treats `optional[T]` (`T | nil`) as structurally distinct from `T`
    - Reject direct method calls on `optional[T]` values (e.g., `nickname.length` where `nickname: optional[string]` → compile error)
    - Reject passing `optional[T]` where `T` is expected (e.g., `print-name(nickname)` where `print-name(name: string)` → compile error)
    - Generate clear error messages guiding developers toward `?.`, `?:`, `!!`, or narrowing
    - _Requirements: 14A-NIL.22_

  - [ ] 48.3 Implement flow-sensitive type narrowing in semantic analyzer
    - Implement control flow tracking in the semantic analyzer to detect nil-exclusion branches
    - When `optional[T]` is checked against `nil` (e.g., `x != nil`), narrow the type to `T` within the true branch scope
    - When `optional[T]` is checked for equality with `nil` (e.g., `x == nil`), narrow the type to `T` within the false/else branch scope
    - Ensure narrowing applies within Smalltalk-style control flow (e.g., `(user != nil).ifTrue { ... }`)
    - Ensure narrowing does NOT persist outside the narrowed scope
    - Update AST type tags temporarily during narrowed scope analysis
    - _Requirements: 14A-NIL.23, 14A-NIL.24_

  - [ ] 48.4 Integrate narrowing with safe navigation and elvis operators
    - Ensure `?.` operator performs implicit narrowing: `user?.name()` narrows `user` to non-nil before calling `.name()`
    - Ensure `?:` operator performs implicit narrowing: the left operand is narrowed to `T` if non-nil before being returned
    - Ensure `?[]` and `?()` operators perform the same implicit narrowing
    - Document that these operators are syntactic sugar over flow-sensitive narrowing
    - _Requirements: 14A-NIL.25_

  - [ ] 48.5 Write property tests for compile-time nil rejection
    - **Property 72: Non-Optional Nil Rejection** — For any assignment, parameter passing, or return of `nil` to a non-optional type `T`, the semantic analyzer should emit a fatal compile-time error
    - **Validates: Requirements 14A-NIL.20, 14A-NIL.21**

  - [ ] 48.6 Write property tests for optional[T] structural distinction
    - **Property 73: Optional Structural Distinction** — For any type `T`, calling a method of `T` directly on a value of type `optional[T]` should be rejected at compile-time; the value must first be narrowed to `T`
    - **Validates: Requirements 14A-NIL.22**

  - [ ] 48.7 Write property tests for flow-sensitive type narrowing
    - **Property 74: Flow-Sensitive Narrowing Correctness** — For any value of type `optional[T]` checked against `nil` in a conditional, the type should be narrowed to `T` inside the nil-excluded branch and remain `optional[T]` outside that branch
    - **Validates: Requirements 14A-NIL.23, 14A-NIL.24**

  - [ ] 48.8 Write property tests for NPE impossibility invariant
    - **Property 75: Null Pointer Exception Impossibility** — For any well-typed pure Meld program that does not use the `!!` operator, no execution path should produce a null dereference at runtime
    - **Validates: Requirements 14A-NIL.26**

- [-] 49. Implement Executable Spec Blocks in @blueprint (Req 140)
  - [x] 49.1 Extend `@blueprint` macro to support `spec` field
    - Add `spec` field to `@blueprint` AST node, containing one or more Action-Result pairs
    - Parse `spec { action: <expression>, result: <expected_value> }` syntax within `@blueprint`
    - Spec pairs execute in a scope with the module's public API available
    - Support effect declarations on spec actions via `@uses(...)`
    - Coexist with existing `summary`, `rules`, `examples` fields
    - _Requirements: 140.1, 140.2, 140.3, 140.4, 140.5_

  - [x] 49.2 Implement spec block serialization
    - Serialize spec blocks to structured JSON: action source text, expected result, declared effects, verification status
    - Support `@blueprint` inheritance — child inherits parent's spec pairs and may add more
    - _Requirements: 140.7, 140.8_

  - [ ] 49.3 Write unit tests for spec block parsing
    - Test `spec` field parses correctly within `@blueprint`
    - Test multiple spec pairs in a single `@blueprint`
    - Test spec with effect declarations
    - Test JSON serialization round-trip
    - Test inheritance of spec pairs from parent blueprint
    - _Requirements: 140.1, 140.2, 140.4, 140.7, 140.8_

  - [x] 49.4 Create executable spec example file
    - Create `meld-core/examples/executable-spec-demo.meld` demonstrating `@blueprint` with `spec` blocks on exported functions
    - Include specs with and without effect declarations
    - Run via `meld-runner executable-spec-demo.meld`
    - _Requirements: 140.1, 140.2, 140.5_

- [-] 50. Implement Seedable Random Effect (Req 141)
  - [x] 50.1 Extend `random` effect with `seed` configuration
    - Add `seed` parameter to `random` effect handler installation
    - When seed is provided, force deterministic sequence from that seed (64-bit integer)
    - When no seed, use system entropy (default behavior)
    - Implement `random.reseed(new_seed)` for mid-execution re-seeding
    - _Requirements: 141.1, 141.2, 141.3, 141.4, 141.5_

  - [ ] 50.2 Ensure cross-tier consistency
    - Verify seedable random produces identical sequences across AST Interpreter, ORC JIT, and AOT
    - _Requirements: 141.6_

  - [ ] 50.3 Write property test for seedable random determinism
    - **Property 76: Seedable Random Determinism** — For any seed S, two executions of the same program with `random.seed(S)` SHALL produce identical random sequences
    - **Validates: Requirements 141.1, 141.2**

  - [ ] 50.4 Write unit tests for seedable random
    - Test seeded random produces deterministic sequence
    - Test unseeded random uses system entropy
    - Test `reseed` changes the sequence
    - _Requirements: 141.1, 141.2, 141.3, 141.5_

- [-] 51. Implement Virtual Time Effect (Req 142)
  - [x] 51.1 Extend `time` effect with `virtual` mode
    - Add `virtual` mode to `time` effect handler — clock does not advance automatically
    - `time.now()` returns virtual clock value instead of system clock
    - `time.advance(duration)` advances virtual clock and fires pending timers/delays in deterministic order
    - `time.set_origin(timestamp)` sets initial virtual time
    - _Requirements: 142.1, 142.2, 142.3, 142.4_

  - [ ] 51.2 Integrate with async delay/timeout
    - When virtual time is active, `delay` and `timeout` in meld-async use virtual clock
    - _Requirements: 142.5_

  - [ ] 51.3 Ensure cross-tier consistency
    - Verify virtual time produces identical behavior across AST Interpreter, ORC JIT, and AOT
    - _Requirements: 142.6_

  - [ ] 51.4 Write property test for virtual time determinism
    - **Property 77: Virtual Time Determinism** — For any program using virtual time with the same origin and advance sequence, all `time.now()` calls SHALL return identical values across executions
    - **Validates: Requirements 142.1, 142.2, 142.3**

  - [ ] 51.5 Write unit tests for virtual time
    - Test `time.now()` returns virtual value, not system clock
    - Test `time.advance()` fires pending timers in order
    - Test `time.set_origin()` sets initial time
    - Test integration with async `delay`
    - _Requirements: 142.1, 142.2, 142.3, 142.4, 142.5_

- [-] 52. Implement Agent-Test Mode (Req 143)
  - [x] 52.1 Implement Agent-Test mode runtime toggle
    - Support `Runtime.enable_agent_test_mode()` programmatic activation
    - When activated, automatically install deterministic handlers: `time` (virtual, epoch 0), `random` (seed 0)
    - Log all effect invocations to structured trace
    - _Requirements: 143.1, 143.2, 143.3, 143.4_

  - [x] 52.2 Implement handler precedence and composability
    - User-installed effect handlers take precedence over Agent-Test defaults
    - Agent-Test mode composes with explicit handler installation
    - _Requirements: 143.5, 143.6_

  - [x] 52.3 Implement structured effect trace
    - Serialize effect trace to JSON, compatible with Flight Recorder snapshot format (Req 44)
    - Ensure identical results across all three execution tiers
    - _Requirements: 143.4, 143.7, 143.8_

  - [ ] 52.4 Write property test for Agent-Test mode reproducibility
    - **Property 78: Agent-Test Mode Reproducibility** — For any program without external I/O, running twice in Agent-Test mode SHALL produce identical effect traces and return values
    - **Validates: Requirements 143.7**

  - [ ] 52.5 Write unit tests for Agent-Test mode
    - Test automatic handler installation (time=virtual epoch 0, random=seed 0)
    - Test user handler takes precedence over defaults
    - Test effect trace captures all invocations
    - Test trace JSON serialization
    - _Requirements: 143.1, 143.2, 143.3, 143.4, 143.6_

  - [x] 52.6 Create Agent-Test mode example file
    - Create `meld-core/examples/agent-test-demo.meld` demonstrating Agent-Test mode with deterministic random, virtual time, and effect tracing
    - Run via `meld-runner agent-test-demo.meld`
    - _Requirements: 143.1, 143.2, 143.3, 143.4_

- [x] 53. Checkpoint — AI_DX language features
  - Ensure all tests pass for executable specs, seedable random, virtual time, and Agent-Test mode
  - Ask the user if questions arise.

- [x] 54. Implement Unified Handle Function Syntax (Req 176)
  - [x] 54.1 Add `inline_trait_impl` AST node for anonymous inline trait implementations
    - Add `inline_trait_impl` struct with `identifier trait_name` and `vector<function_definition> methods`
    - This is a general-purpose AST node: `TypeName { fnc op1() { ... } fnc op2() { ... } }`
    - Used by handle() for effect handlers, but also available as a standalone expression
    - Add to the `expression` variant in ast.hpp
    - Add `BOOST_FUSION_ADAPT_STRUCT` macro
    - _Requirements: 176.1, 176.2_

  - [x] 54.2 Implement inline trait implementation parser
    - Recognize `Identifier { fnc ... }` pattern in `parse_primary()` or `parse_postfix()`
    - When an identifier is followed by `{` and the block contains `fnc` definitions, parse as `inline_trait_impl`
    - Parse each `fnc` definition inside the block using existing `parse_function_definition()`
    - Distinguish from object literals (`{ field: value }`) by checking for `fnc` keyword after `{`
    - _Requirements: 176.1, 176.2_

  - [x] 54.3 Update handle macro to accept inline trait implementations as handler arguments
    - `handle()` is still a regular function call — first arg is `{ computation }`, rest are `inline_trait_impl` nodes
    - Update `expand_handle_macro()` to extract trait name and methods from `inline_trait_impl` arguments
    - Support 1–N handler arguments (one per effect)
    - Generate one `EffectScope` per handler, nested from outermost to innermost
    - _Requirements: 176.1, 176.2, 176.3, 176.5_

  - [ ] 54.4 Implement legacy syntax detection and migration diagnostics
    - Detect `handle(computation: { ... })` named-parameter form → emit error diagnostic
    - Detect `handle { ... } with` keyword form → emit error diagnostic
    - Detect bare `handle { ... }` without handlers → emit error diagnostic
    - Include fix suggestions showing equivalent unified syntax
    - _Requirements: 176.4, 176.8_

  - [ ] 54.5 Update AST printer for inline trait implementations and handle syntax
    - Add `inline_trait_impl` visitor to `ast_printer.cpp` that emits `TypeName { fnc ... }`
    - Ensure handle calls print as `handle({ body }, Effect1 { fnc ... }, Effect2 { fnc ... })`
    - Update kernel pretty printer in `compiler/src/pretty_printer.cpp` for handle S-expressions
    - _Requirements: 176.7_

  - [ ] 54.6 Update effect checker, ASG builder, and AST parent setter
    - Update `EffectChecker` to iterate handler arguments from handle call
    - Update `visitHandleExpression` in `asg_builder.hpp` for multi-handler node
    - Update handle_expression traversal in `imposes_annotation.cpp`
    - Update `ast_parent_setter.hpp` for inline_trait_impl wiring
    - _Requirements: 176.1, 176.3_

  - [ ] 54.7 Verify all example files use unified handle syntax
    - Verify all `.meld` files in `meld-core/examples/` use `handle({ ... }, EffectName { fnc ... })` syntax
    - Verify migration examples show old syntax only in commented-out BEFORE sections
    - _Requirements: 176.9_

  - [x] 54.8 Write property-based tests for unified handle syntax
    - **Property: Inline trait impl parsing** — `TypeName { fnc ... }` parses as `inline_trait_impl` node
    - **Property: Handle with 1–5 handlers** — random handle calls parse correctly
    - **Property: Handler fnc syntax equivalence** — fnc in handler context produces same AST as top-level
    - **Property: Handle in expression positions** — handle calls accepted in val bindings, function args, block tails
    - **Property: Legacy syntax rejection** — old syntax forms emit error diagnostics
    - **Validates: Requirements 176.1–176.8**

  - [x] 54.9 Write unit tests for handle syntax edge cases
    - Parse minimal handle call: `handle({ 42 }, MyEffect { fnc op() -> () { resume() } })`
    - Parse handle with multiple handlers and multiple operations per handler
    - Parse standalone inline trait impl: `val x = Comparable { fnc compare(a, b) -> int { rtn 0 } }`
    - Verify each legacy form produces correct diagnostic message text
    - Verify empty handler body `handle({ x }, Eff { })` is rejected
    - Verify duplicate effect names are rejected
    - Verify nested handle calls parse correctly
    - _Requirements: 176.1–176.8_

- [x] 55. Implement Interpolation Modifiers (Library-Extensible Formatting)
  - [x] 55.1 Update lexer to recognize `:symbol` inside `${}`
    - Extend `read_string()` and `tokenize_multiline_string()` in `lexer.cpp` to detect `:symbol` after `${`
    - When `${` is followed by `:identifier`, emit a modifier token (e.g., store the symbol name alongside the interpolation)
    - Preserve backward compatibility: `${expr}` without `:` continues to work as before
    - Update `has_interpolation` flag handling to also track modifier presence
    - _Design ref: Interpolation Modifiers — Compiler Desugaring_

  - [x] 55.2 Update parser to desugar `${:symbol expr}` into function calls
    - When parsing a template string interpolation with a modifier symbol, desugar to `__interpolate-<symbol>__(expr)`
    - When parsing a template string interpolation without a modifier, desugar to `__interpolate-default__(expr)`
    - The compiler concatenates `__interpolate-` + symbol name + `__` to form the function name — no interpretation of the symbol
    - Ensure the desugared call is a standard function call AST node (no special node type)
    - _Design ref: Interpolation Modifiers — Compiler Desugaring_

  - [x] 55.3 Add `__interpolate-default__` and core formatter functions to `core.meld`
    - Add `fnc __interpolate-default__(value) { rtn value.to-string() }` to `compiler/core.meld`
    - Add `fnc __interpolate-debug__(value) { rtn value.debug() }` to `compiler/core.meld`
    - Add `fnc __interpolate-pretty__(value) { rtn value.pretty() }` to `compiler/core.meld`
    - These are plain functions, loaded automatically before user code like `print`/`println`
    - _Design ref: Interpolation Modifiers — Standard Library Formatter Functions_

  - [x] 55.4 Implement `@debug` decorator macro to generate `.debug()` and `.pretty()` methods
    - Implement `@debug` as a class/struct-level decorator macro
    - Generate `fnc debug() -> string` that produces structural representation (e.g., `"User { name: \"Alice\", age: 30 }"`)
    - Generated `debug()` should recursively use `${:debug field}` for nested fields
    - Generate `fnc pretty() -> string` that produces indented multi-line output
    - Inject generated methods into the type via `extend`
    - _Design ref: Interpolation Modifiers — @debug and @stringify Decorators_

  - [x] 55.5 Implement `@stringify` decorator macro to generate `.to-string()` method
    - Implement `@stringify` as a class/struct-level decorator macro
    - Generate `fnc to-string() -> string` that produces user-facing string representation
    - Ensure `@Data` composite decorator includes `@stringify` generation (already specified in existing design)
    - _Design ref: Interpolation Modifiers — @debug and @stringify Decorators_

  - [x] 55.6 Add `.debug()`, `.to-string()`, `.hex()` methods to primitive types
    - ~~Add `fnc debug() -> string` to `int`, `float`, `bool`, `string`, `nil` in standard library~~
    - **Interpreter**: Handled natively by `register_builtins()` — `__interpolate-debug__` calls `value_to_debug_string()` which covers all primitive types. `__interpolate-default__` handles `.to-string()` semantics. No dot-access dispatch needed.
    - **Compiled pipeline**: `core.meld` defines `__interpolate-debug__` → `native_call("meld_debug_string", value)`. The native C functions (`meld_debug_string`, `meld_pretty_string`, `meld_to_string`) are not yet implemented — this is a compiled-pipeline task, not an interpreter task.
    - For `string`, `debug()` produces escaped/quoted output (e.g., `string("hello")`) ✅
    - For `int`, `debug()` produces `int(42)` ✅
    - For `bool`, `debug()` returns `bool(true)` / `bool(false)` ✅
    - For `nil`, `debug()` returns `nil` ✅
    - `.hex()` method requires dot-access dispatch (future task)
    - These are the recursion base cases for `@debug` generated code
    - _Design ref: Interpolation Modifiers — Built-In Formatters_

  - [x] 55.7 Create `interpolation-modifier-demo.meld` example
    - Demonstrate `${:debug expr}`, `${:pretty expr}`, `${:hex expr}` usage
    - Show user-defined modifier via custom `__interpolate-xml__` function and `.xml()` method
    - Show `@debug` on a struct with nested fields
    - Show recursive debug output for nested types
    - Pure `.meld` file only (run via `meld-runner`)
    - _Design ref: Interpolation Modifiers — Extensibility Model_

  - [x] 55.8 Write property-based tests for interpolation modifier desugaring
    - **Property: Default desugaring** — `${expr}` always desugars to `__interpolate-default__(expr)` call
    - **Property: Modifier desugaring** — `${:sym expr}` always desugars to `__interpolate-sym__(expr)` call for any valid symbol
    - **Property: Naming convention** — the generated function name is always `__interpolate-` + symbol-name + `__`
    - **Property: No compiler knowledge** — the compiler produces identical AST structure regardless of which symbol is used (only the function name differs)
    - **Property: Backward compatibility** — existing template strings without modifiers produce identical AST to before this change
    - _Design ref: Interpolation Modifiers — Design Rationale_

  - [x] 55.9 Write unit tests for interpolation modifier edge cases
    - Verify `${:debug expr}` parses and desugars correctly
    - Verify `${expr}` without modifier still works (backward compat)
    - Verify undefined modifier function produces standard "undefined function" error
    - Verify nested modifiers work: `${:debug nested}` where `nested.debug()` itself uses `${:debug ...}`
    - Verify modifier with complex expressions: `${:debug user.get-name()}`
    - Verify modifier in multi-line template strings
    - Verify `:` without a following identifier inside `${}` produces a parse error
    - _Design ref: Interpolation Modifiers_

- [x] 56. Split String Literals: Static vs Evaluated (4 Variants)
  - [x] 56.1 Add backtick template string tokenization to lexer
    - Add single-backtick (`` ` ``) recognition in `Lexer::next_token()`
    - Implement `tokenize_template_string()` that scans for `${expr}` and `${:modifier expr}` (same logic currently in `tokenize_string()`)
    - Emit `TokenType::TEMPLATE_STRING` token (new token type)
    - Single-backtick strings are single-line (error on unescaped newline)
  - [x] 56.2 Add triple-backtick multi-line template string tokenization
    - Add triple-backtick (` ``` `) recognition in `Lexer::next_token()` (check before single-backtick, like `"""` before `"`)
    - Implement `tokenize_multiline_template_string()` with `${expr}` and `${:modifier expr}` support
    - Emit `TokenType::MULTILINE_TEMPLATE_STRING` token (new token type)
    - Triple-backtick strings preserve newlines and support interpolation
  - [x] 56.3 Remove interpolation support from double-quoted strings
    - Modify `tokenize_string()` to treat `$` as a literal character (no `${` scanning)
    - Remove `has_interpolation` flag setting from `tokenize_string()`
    - `"..."` strings become purely static — no expression evaluation
    - Add compiler warning when `${` is detected inside `"..."`: "did you mean to use a template string (`` ` ``)?
  - [x] 56.4 Remove interpolation support from triple-quoted strings
    - Modify `tokenize_multiline_string()` to treat `$` as a literal character
    - `"""..."""` strings become purely static multi-line — no expression evaluation
    - Add compiler warning when `${` is detected inside `"""..."""`
  - [x] 56.5 Update parser to handle new token types
    - Add `TEMPLATE_STRING` and `MULTILINE_TEMPLATE_STRING` cases to `TokenParser::parse_primary()`
    - Set `has_interpolation` and `has_modifier_interpolation` flags only for template string tokens
    - Ensure `string_literal` AST node distinguishes static vs template (add `is_template` field if needed)
  - [x] 56.6 Update interpreter `eval_string_literal` to respect string type
    - Static strings (`"..."`, `"""..."""`): return value directly, never process `${}`
    - Template strings (`` `...` ``, ` ```...``` `): process `${}` interpolation as before
    - The `has_interpolation` flag should only be true for template strings
  - [x] 56.7 Update existing tests to use correct string syntax
    - Update `interpolation_modifier_test.cpp` — tests that evaluate interpolation should use template strings
    - Update `interpolation_modifier_edge_test.cpp` — split tests: static string tests use `"..."`, interpolation tests use `` `...` ``
    - Update `interpolation_modifier_property_test.cpp` — property tests use template strings for interpolation
    - Update `parser_test.cpp` — existing interpolation test should use template string
    - Verify all existing tests still pass
  - [x] 56.8 Add tests for static vs template string distinction
    - Verify `"hello ${name}"` produces literal text `hello ${name}` (no evaluation)
    - Verify `` `hello ${name}` `` evaluates the interpolation
    - Verify `"""multi\n${x}\nline"""` produces literal text with `${x}` unevaluated
    - Verify ` ```multi\n${x}\nline``` ` evaluates the interpolation
    - Verify compiler warning when `${` appears in static strings
    - Verify `"\${name}"` (escaped `$`) suppresses the warning
  - [x] 56.9 Update design doc and examples
    - Update design.md Interpolation Modifiers section to show backtick syntax
    - Update `interpolation-modifier-demo.meld` to use backtick strings
    - Update `core.meld` string literals if any use interpolation (currently none do)

- [ ] 57. Implement Anonymous Implementation Blocks (Req 177)
  - [ ] 57.1 Promote `inline_trait_impl` to a standalone expression variant
    - Currently `inline_trait_impl` is only usable inside `handle_expression` (Boost.MPL variant limit = 50)
    - Either consolidate existing expression variant members to free a slot, or switch to a different variant implementation
    - Add `inline_trait_impl` to the `expression` variant so `Name { fnc ... }` works as a standalone expression
    - Update all visitors (EvalVisitor, effect_checker, ast_quote, imposes_annotation) to handle the new variant member
    - _Requirements: 177.1, 177.2_

  - [ ] 57.2 Update parser disambiguation for `val`/`var` in anonymous blocks
    - Currently parser checks for `fnc`/`func` after `{` to disambiguate from initialization blocks
    - Extend to also check for `val`/`var` keywords — anonymous blocks can contain field declarations
    - _Requirements: 177.2_

  - [ ] 57.3 Implement interpreter evaluation of anonymous implementation blocks
    - When evaluated, create an anonymous object that implements the trait's/type's methods
    - Methods are closures that capture the enclosing scope
    - Support `self` keyword inside the block referring to the anonymous value
    - The resulting value satisfies the target type for type checking purposes
    - _Requirements: 177.1, 177.3_

  - [ ] 57.4 Implement intersection type anonymous blocks
    - Parse `(Trait1 & Trait2) { fnc ... }` as an anonymous block implementing multiple traits
    - Verify all required methods from all listed traits are present
    - _Requirements: 177.4_

  - [ ] 57.5 Write tests for anonymous implementation blocks
    - `val x = Comparable { fnc compare(a, b) -> int { rtn 0 } }` parses and evaluates
    - Anonymous block works as function argument
    - Anonymous block works in val binding
    - Anonymous block with fields and methods
    - Empty block is rejected
    - Effect handlers in handle() use the same syntax
    - _Requirements: 177.1, 177.5_

  - [ ] 57.6 Write tests for type checking of anonymous blocks
    - Anonymous trait impl is assignable to trait type variable
    - Missing required method produces error
    - Intersection type anonymous block satisfies all listed traits
    - _Requirements: 177.3, 177.4_

- [x] 58. Rename Own[T]/Link[T] to Hold[T]/View[T] (Req 119-129 updated)
  - [x] 58.1 Rename `Own[T]` to `Hold[T]` in `std/mem.meld`
    - Rename `class Own[T]` to `class Hold[T]`
    - Rename `class Link[T]` to `class View[T]`
    - Rename `fnc link[T](owner: Own[T]) -> Link[T]` to `fnc view[T](holder: Hold[T]) -> View[T]`
    - Rename `fnc move[T](source: Own[T]) -> Own[T]` to `fnc move[T](source: Hold[T]) -> Hold[T]`
    - Update `impl[T] Storable for Own[T]` to `impl[T] Storable for Hold[T]`
    - Update `impl[T] Storable for Link[T]` to `impl[T] Storable for View[T]`
    - Update all comments and doc strings to use Hold/View terminology
    - _Requirements: 119, 120_

  - [x] 58.2 Rename compiler passes and internal references
    - Rename `OwnTypeInferencePass` to `HoldTypeInferencePass` (or update string literals to emit `Hold[T]`)
    - Rename `LinkAccessPass` to `ViewAccessPass` (or update string literals to emit `View[T]`)
    - Rename `LinkBindingTracker` to `ViewBindingTracker`
    - Update `OwnOp`/`LinkOp` enums in backend lowering tests to `HoldOp`/`ViewOp`
    - Update all comments in `compiler.hpp` from `Own[T]/Link[T]` to `Hold[T]/View[T]`
    - Update diagnostic string literals: E4001 message to reference `View[T]`, E4003 to suggest `Hold[T]` or `View[T]`
    - _Requirements: 119, 120, 125_

  - [x] 58.3 Add migration hints for old names
    - When parser encounters `Own[` as a type, emit migration hint: "Did you mean Hold[T]? Own[T] has been renamed to Hold[T]."
    - When parser encounters `Link[` as a type, emit migration hint: "Did you mean View[T]? Link[T] has been renamed to View[T]."
    - When parser encounters `std.mem.link(`, emit migration hint: "Did you mean std.mem.view()? link() has been renamed to view()."
    - _Requirements: 119.7, 120.8_

  - [x] 58.4 Rename example files
    - Rename `own-link-demo.meld` to `hold-view-demo.meld` and update all content to Hold/View terminology
    - Rename `own-link-demo.cpp` to `hold-view-demo.cpp` and update all content
    - Update BUILD file references for renamed example files
    - _Requirements: 129_

  - [x] 58.5 Update View[T] access enforcement to support `?.` operator
    - Extend `ViewAccessPass` (formerly LinkAccessPass) to accept `?.` as a valid access pattern in addition to `if val` and `match`
    - Update E4001 diagnostic message to: "View[T] must be accessed via `?.` (safe navigation), `if val` (upgrade), or `match`."
    - _Requirements: 121.1, 121.7, 125.1_

  - [x] 58.6 Implement Creator Rule and Guest Rule type inference
    - Update `HoldTypeInferencePass` (formerly OwnTypeInferencePass): when a variable is initialized with a constructor call and no explicit wrapper, infer `Hold[T]` (Creator Rule — already exists, just rename)
    - Add Guest Rule: when a function parameter uses a bare class type without explicit wrapper, infer `View[T]`
    - _Requirements: 119.5, 120.7_

  - [x] 58.7 Update all existing tests
    - Update `test_own_type_inference_pass.cpp` to use Hold terminology
    - Update `test_link_access_pass.cpp` to use View terminology
    - Update `test_backend_lowering_property.cpp` to use Hold/View enums
    - Update `test_ownership_ffi.cpp` to use Hold/View
    - Ensure all tests pass after rename
    - _Requirements: 119-129_

- [x] 59. Implement Generic Mutability Qualifiers — val/var on Type Parameters (Req 165)
  - [x] 59.1 Extend parser to support `val`/`var` before type parameters
    - Update Boost.Spirit X3 grammar to accept `val` or `var` keyword before any type parameter in generic types
    - Support: `Hold[val T]`, `Hold[var T]`, `View[val T]`, `View[var T]`, `List[val T]`, `Map[val K, var V]`, etc.
    - Store the mutability qualifier in the generic type parameter AST node
    - _Requirements: 165.1_

  - [x] 59.2 Implement default mutability inference
    - When a generic type parameter has no explicit `val`/`var` qualifier, default to `val` (immutable)
    - `View[T]` defaults to `View[val T]`, `Hold[T]` defaults to `Hold[val T]`
    - _Requirements: 165.2_

  - [x] 59.3 Implement mutability enforcement in semantic analyzer
    - When a type parameter is `val`, reject calls to methods annotated with `@effect(state)` on the contained value
    - When a type parameter is `var`, permit mutating method calls only if the enclosing function has `@effect(state)`
    - _Requirements: 165.3, 165.4_

  - [x] 59.4 Implement automatic downgrade logic
    - Allow `Foo[var T]` to be passed where `Foo[val T]` is expected (automatic downgrade)
    - Reject `Foo[val T]` where `Foo[var T]` is expected without explicit checked cast
    - Implement checked cast mechanism with elevated permissions
    - _Requirements: 165.5, 165.6_

  - [x] 59.5 Implement `meldd` auto-inference of `@effect(state)` from `var` type parameters
    - When a function signature contains any `var`-qualified type parameter, auto-attach `@effect(state)` tag
    - _Requirements: 165.7_

  - [x] 59.6 Write property tests for generic mutability qualifiers
    - **Property: val Type Parameter Immutability** — For any generic type `Foo[val T]`, calling a mutating method on the contained value should be rejected at compile time
    - **Property: var-to-val Downgrade** — For any `Foo[var T]` value, passing it where `Foo[val T]` is expected should succeed; the reverse should fail
    - **Validates: Requirements 165.1-165.6**

- [x] 60. Implement Standard Collection Rename — Dict→Map, Deque→Queue (Req 166)
  - [x] 60.1 Update parser reserved keywords
    - Add `List`, `Map`, `Set`, `Queue` as reserved collection keywords in the Boost.Spirit X3 lexer
    - Prevent user-defined types from using these names
    - _Requirements: 166.2_

  - [x] 60.2 Add migration hints for old collection names
    - When parser encounters `Dict[` as a type, emit migration hint: "Did you mean Map? Dict has been renamed to Map."
    - When parser encounters `Deque[` or `deque[` as a type, emit migration hint: "Did you mean Queue? Deque has been renamed to Queue."
    - _Requirements: 166.3, 166.4_

  - [x] 60.3 Update standard library collection definitions
    - Rename collection type definitions in std library to use `List`, `Map`, `Set`, `Queue`
    - Ensure all collection types are annotated with `@intrinsic(managed_container)`
    - _Requirements: 166.1, 166.5_

  - [x] 60.4 Update all existing examples and tests
    - Replace `vec[` with `List[` in all .meld example files
    - Replace `dict[` with `Map[` in all .meld example files
    - Update C++ test files that reference old collection names
    - _Requirements: 166.1_

- [x] 61. Implement Tenancy-Aware Collection APIs (Req 167)
  - [x] 61.1 Implement Hold/View lifecycle behavior for List
    - `List[Hold[T]]`: removing or dropping the list decrements `strong_count` of every element
    - `List[View[T]]`: never increments `strong_count` of elements (observer-only)
    - _Requirements: 167.1, 167.2_

  - [x] 61.2 Implement Map tenancy-aware accessors
    - `Map[K, V].keys()` returns `List[View[K]]`
    - `Map[K, V].values()` returns `List[View[V]]`
    - Map iterator yields `(View[K], View[V])` tuples
    - _Requirements: 167.3, 124.6_

  - [x] 61.3 Implement Queue ownership transfer semantics
    - `Queue[T].front()` and `Queue[T].back()` return `View[T]?`
    - `Queue[T].pop-front()` and `Queue[T].pop-back()` return `Hold[T]`
    - _Requirements: 167.4, 167.5_

  - [x] 61.4 Implement safe indexing for View collections
    - `list[n]` on `List[View[T]]` returns `optional[View[T]]`
    - _Requirements: 167.6_

  - [x] 61.5 Implement effect-gated mutation for collection methods
    - All destructive methods (`add`, `remove`, `clear`, `sort-in-place`) require `@effect(state)` on the enclosing function
    - Emit compile error if mutating method called without `@effect(state)` permission
    - _Requirements: 167.7_

  - [x] 61.6 Implement deterministic iteration for Map and Set in Agent-Mode
    - When Agent-Mode is active, `Map` and `Set` iteration order is deterministic
    - _Requirements: 124.7_

  - [x] 61.7 Write property tests for tenancy-aware collections
    - **Property: Hold Collection Lifecycle** — For any `List[Hold[T]]`, clearing the list decrements strong_count of every element
    - **Property: View Collection Observer** — For any `List[View[T]]`, the list never increments strong_count
    - **Property: Queue Ownership Transfer** — For any `Queue[Hold[T]]`, pop returns Hold (ownership transfer) and front returns View (observation)
    - **Validates: Requirements 167.1-167.6, 124.6-124.7**

- [x] 62. Implement State vs. Identity Effect Enforcement (Req 168)
  - [x] 62.1 Implement dual-dimension enforcement in semantic analyzer
    - `val` declaration + reassignment → compile error
    - `val` declaration + mutating method call → compile error (type parameter is `val`, lacks `@effect(state)`)
    - `var` declaration + reassignment → permitted
    - `var` declaration + mutating method call → permitted only if function has `@effect(state)`
    - _Requirements: 168.1-168.5_

  - [x] 62.2 Write property tests for State vs. Identity matrix
    - **Property: val Deep Immutability** — For any `val` collection, calling `.add()` or `.remove()` should be rejected at compile time
    - **Property: var Requires Effect** — For any `var` collection, calling `.add()` should succeed only if the enclosing function has `@effect(state)`
    - **Validates: Requirements 168.1-168.5**

- [x] 63. Implement Per-Binding Destructuring Qualifiers (Req 169)
  - [x] 63.1 Update parser for per-binding `val`/`var` in destructuring
    - Change destructuring grammar from `val (x, y) = expr` to `(val x, val y) = expr`
    - Require explicit `val` or `var` on each individual binding
    - Reject the old shorthand `val (x, y)` with a clear error message
    - Support mixed qualifiers: `(val key, var value) = expr`
    - _Requirements: 169.7, 169.8, 169.9_

  - [x] 63.2 Update for-loop destructuring syntax
    - Change `for (key, value) in map` to `for (val key, val value) in map`
    - Require per-binding qualifiers in for-loop destructuring
    - _Requirements: 169.5_

  - [x] 63.3 Implement ARC-aware partial move semantics
    - Destructuring `Hold[T]` fields performs partial move — parent becomes tombstone
    - Destructuring `View[T]` fields yields `View[T]` variables — no ownership transfer
    - Reject access to parent after partial move: "use of partially moved value"
    - Reject reassembly of partially moved structs
    - Copy types produce independent copies regardless of tenancy
    - _Requirements: 169.1-169.4, 169.6_

  - [x] 63.4 Update all existing examples and tests
    - Update all `val (x, y) = expr` patterns to `(val x, val y) = expr` in .meld files
    - Update all `var (a, b) = expr` patterns to `(var a, var b) = expr` in .meld files
    - Update for-loop destructuring in all examples
    - _Requirements: 169.7_

  - [x] 63.5 Write property tests for per-binding destructuring
    - **Property: Mixed Qualifier Destructuring** — For any destructuring `(val x, var y) = expr`, `x` should be immutable and `y` should be mutable
    - **Property: Partial Move Tombstone** — For any destructuring of a struct with `Hold[T]` fields, the parent struct should be inaccessible after destructuring
    - **Validates: Requirements 169.1-169.9**

- [x] 64. Implement Safe Navigation @effect Skipping (Req 170)
  - [x] 64.1 Implement `?.` effect skipping for dead View[T]
    - When `?.` is applied to a `View[T]` and the object is deallocated, skip the entire operation including any `@effect`
    - The Effect Firewall must not record an `@effect(state)` event for a skipped operation
    - Expression evaluates to `nil` on short-circuit
    - _Requirements: 170.1, 170.2, 170.3_

  - [x] 64.2 Implement codegen for View[T] null-check guard
    - Compile `?.` on `View[T]` to a null-check guard that tests validity before executing the operation and its effect
    - _Requirements: 170.4_

  - [x] 64.3 Write property tests for safe navigation effect skipping
    - **Property: View Safe Navigation Effect Skip** — For any `?.` on a dead `View[T]`, the effect firewall should record zero effects for that operation
    - **Validates: Requirements 170.1-170.4**

- [x] 65. Implement IDE Ghost Text for Tenancy State (Req 175)
  - [x] 65.1 Implement `meld/inlayHints` LSP command in `meldd`
    - Stream inferred `Hold`/`View` states to the IDE via LSP inlay hints protocol
    - _Requirements: 175.1_

  - [x] 65.2 Implement Creator Rule ghost text
    - When a variable is initialized with a constructor and no explicit `Hold[T]` annotation, display ghost text showing inferred `Hold[T]`
    - _Requirements: 175.2_

  - [x] 65.3 Implement Guest Rule ghost text
    - When a function parameter uses a bare type (no explicit wrapper), display ghost text showing inferred `View[T]`
    - _Requirements: 175.3_

  - [x] 65.4 Ensure ghost text is visually distinct
    - Ghost text must be grayed out and not part of the source file
    - _Requirements: 175.4_

---

## Implementation Notes

### Priority Order

**Phase 1: Core Language (Completed)**
- ✅ Kernel primitives
- ✅ Type system
- ✅ Macro system
- ✅ Lexer/Parser
- ✅ Memory management
- ✅ Properties
- ✅ Functions
- ✅ Control flow
- ✅ Collections
- ✅ Multiple dispatch
- ✅ Operators
- ✅ Decorators
- ✅ Metaprogramming
- ✅ Namespaces
- ✅ Modules & Imports (`imp` keyword) (Task 25A)
- ✅ Type projections

**Phase 2: Error Handling & Concurrency (Complete)**
- ✅ Result[T,E] type
- ✅ Attempt.run blocks
- ✅ Async/await
- ✅ Structured concurrency

**Phase 3: Advanced Features (Complete)**
- ✅ Refinement types
- ✅ Partial application/currying
- ✅ Anonymous types
- ✅ Pattern matching

**Phase 4: v1.8 Features - Synapse Integration (Complete)**
- ✅ Abstract Syntax Graph (ASG)
- ✅ Explicit Effect Tracking
- ✅ Binary Context Format (MELD-B)
- ✅ Algebraic Effects System
- ✅ Property-Based Testing with forall macro
- ✅ Integration with existing AI features

**Phase 4.5: v2.0 Features - AI Safety & Trust (Mostly Complete)**
- ✅ Visual Logic (flow macro)
- ✅ Flight Recorder for crash replay
- 🚧 Code Provenance and Trust Model (property tests pending: 39.11-39.13)
- 🚧 Shadow Provenance for history storage (property test pending: 40.14)
- ✅ Integration with v1.8 features

**Phase 5: AI-Native Features (Complete)**
- ✅ Structural Search API (enhanced with ASG)
- ✅ Holographic View (enhanced with MELD-B)
- ✅ Design by Contract (require/ensure macros)
- ✅ Compiler-Agent Protocol (enhanced with MELD-B)
- ✅ Inline Micro-Tests (test macro)
- ✅ @blueprint Macro
- ✅ Auto-MCP Generation

**Phase 6: Polyglot & Tooling (Partially Complete)**
- ✅ JVM backend
- 📋 Rust backend (Task 29.2)
- ✅ @extern macro
- ✅ Tooling and ecosystem (Task 42)

**Phase 7: Language Hardening (Not Started)**
- ✅ `var fnc` / `var` parameter enforcement (Task 44)
- 📋 Effect Firewall property tests (Tasks 45.7-45.9)
- 📋 `meld.toml` as Single Source of Truth (Task 46)
- 📋 Control Flow Quintet operators (Task 47)
- 📋 Nil Handling and Compile-Time Null Safety (Task 48)

**Phase 8: Hold/View Tenancy Model & Language Refinements (Not Started)**
- 📋 Own→Hold / Link→View rename (Task 58)
- 📋 Generic mutability qualifiers — val/var on type parameters (Task 59)
- 📋 Collection rename — Dict→Map, Deque→Queue (Task 60)
- 📋 Tenancy-aware collection APIs (Task 61)
- 📋 State vs. Identity effect enforcement (Task 62)
- 📋 Per-binding destructuring qualifiers (Task 63)
- 📋 Safe navigation @effect skipping (Task 64)
- 📋 IDE ghost text for tenancy state (Task 65)

### Testing Strategy

- **Unit Tests**: Test specific examples and edge cases
- **Property-Based Tests**: Verify universal properties across all inputs
- **Micro-Tests**: Co-located tests via test macro for immediate verification
- **Contract Tests**: Validate preconditions and postconditions
- **Integration Tests**: Test component interactions

### Key Principles

1. **Incremental Development**: Build features incrementally with testing
2. **Library-Based Control Flow**: NO keywords (if/else, match/case, flow are all library macros)
3. **Strict Error Handling**: Result[T,E] only, NO exceptions
4. **AI-First Design**: Every feature enhances AI collaboration
5. **Safety by Default**: Null safety, immutability, structured concurrency
6. **Polyglot Interoperability**: Multiple compilation targets

---

## Summary

**Kernel Architecture:** Exactly 20 primitives organized into 7 categories (The "Meld 20")
- ✅ Data Structure Primitives (4): cell, vec, symbol, scope
- ✅ Scalar Primitives (4): int, float, bool, nil
- ✅ Execution Primitives (4): lambda, apply, eval, quote
- ✅ Control Flow Primitives (1): primitive_suspend
- ✅ Memory & Binding Primitives (3): def, set, lookup
- ✅ AI & Metadata Primitives (2): meta_set, meta_get
- ✅ Interop Primitives (2): native_call, native_load

> **Note:** `type` is a library concept built on `meta_set`/`meta_get`, not a kernel primitive. `mark_stack`, `suspend`, and `resume` are library functions built on `primitive_suspend`.

**Total Tasks:** 55 major tasks with 270+ subtasks

**Completed (✅):** Tasks 1-28, 29.1, 30-38, 39.1-39.10, 40.1-40.13, 41-42, 44, 45.1-45.6
- Core language: kernel, types, macros, parser, memory, properties, functions, control flow, collections, dispatch, operators, decorators, metaprogramming, namespaces, modules, tuples, error handling, async
- v1.8 AI-native features: ASG, effects, MELD-B, algebraic effects, PBT, flow macro, flight recorder
- v2.0 AI safety: provenance tracking, shadow history, trust model (implementation done)
- Polyglot: JVM backend, @extern macro
- Effect Firewall: core implementation (via meld-unified-cli spec)

**In Progress (🚧):**
- Code Provenance property tests (Tasks 39.11-39.13)
- Shadow Provenance property test (Task 40.14)

**Not Started (📋):**
- Rust backend (Task 29.2)
- Final system integration checkpoint (Task 43)
- Effect Firewall property tests (Tasks 45.7-45.9)
- `meld.toml` as Single Source of Truth (Task 46)
- Control Flow Quintet operators (Task 47)
- Nil Handling and Compile-Time Null Safety (Task 48)
- Executable Spec Blocks in @blueprint (Task 49) — AI_DX
- Seedable Random Effect (Task 50) — AI_DX
- Virtual Time Effect (Task 51) — AI_DX
- Agent-Test Mode (Task 52) — AI_DX
- AI_DX language features checkpoint (Task 53) — AI_DX
- Unified Handle Function Syntax (Task 54) — Req 176
- Interpolation Modifiers (Task 55) — Library-Extensible Formatting
- Own→Hold / Link→View rename (Task 58) — Req 119-129 updated
- Generic mutability qualifiers (Task 59) — Req 165
- Collection rename Dict→Map, Deque→Queue (Task 60) — Req 166
- Tenancy-aware collection APIs (Task 61) — Req 167
- State vs. Identity effect enforcement (Task 62) — Req 168
- Per-binding destructuring qualifiers (Task 63) — Req 169
- Safe navigation @effect skipping (Task 64) — Req 170
- IDE ghost text for tenancy state (Task 65) — Req 175

**Implementation Progress:** ~75% complete
- Core language: ~95% complete
- v1.8 AI-native features: ~100% complete
- v2.0 AI safety features: ~85% complete (property tests pending)
- Polyglot & tooling: ~30% complete (JVM done; Rust/C++/WASM pending)
- Language hardening: ~30% complete (var fnc done; quintet operators + nil safety pending)
- AI Developer Experience: ~80% complete (Tasks 49-52 implemented; unit/property tests pending; Task 53 checkpoint pending)

**Next Priority Areas:**
1. Own→Hold / Link→View rename (Task 58) — foundational for all subsequent tenancy work
2. Generic mutability qualifiers (Task 59) — enables val/var on type parameters
3. Collection rename and tenancy-aware APIs (Tasks 60-61)
4. Per-binding destructuring qualifiers (Task 63)
5. State vs. Identity effect enforcement (Task 62)
6. Safe navigation @effect skipping (Task 64)
7. IDE ghost text for tenancy state (Task 65)
8. Implement Control Flow Quintet operators (Task 47)
9. Implement Nil Handling and Compile-Time Null Safety (Task 48)
10. Implement `meld.toml` as Single Source of Truth (Task 46)
11. Complete provenance property tests (Tasks 39.11-39.13)
12. Effect Firewall property tests (Tasks 45.7-45.9)
13. Implement Rust backend (Task 29.2)
14. Final system integration checkpoint (Task 43)

