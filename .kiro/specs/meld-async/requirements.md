# Requirements Document

## Introduction

This feature implements the 3-tier concurrency runtime for Meld, providing Fibers (green threads), Actors (OS threads), and Isolates (OS processes) as the execution substrate beneath the existing user-facing structured concurrency API (`Task[T]`, `coroutineScope`, `launch`). The runtime upgrades `primitive_suspend` from logical frame-based continuation capture to hardware stack-switching via `boost::context::callcc`, adds a per-Actor event loop/reactor for I/O multiplexing, an Actor system with thread-local heaps and message passing, and an Isolate system with OS process isolation and IPC. This combines decisions C.2 (Reactor/Event Loop), C.3 (Fiber Scheduler/Green Threads), C.4 (Isolate = OS Process), C.5 (Actor = OS Thread), and C.6 (callcc/Hardware Stack-Switching) from SPEC_ALIGNMENT_ANALYSIS.md, implementing the hybrid approach established in decision B.10.

Requirements 14–23 were merged from the former `async-enhancements` spec, which enhances Meld's library-based async system with additional combinators, error handling, manual task completion, heterogeneous task combining, state inspection, delay/timeout improvements, execution control, and effect system integration — inspired by Java's CompletableFuture API. This spec was renamed from `three-tier-concurrency-runtime` to `meld-async`.

## Glossary

- **Fiber**: A lightweight green thread scheduled cooperatively within a single Actor. Each Fiber has its own hardware stack allocated via `boost::context::callcc`. Millions of Fibers can run within a single Actor.
- **Actor**: An OS-level thread with a thread-local heap, a dedicated event_loop, and a scheduler. Actors communicate exclusively via message passing. Objects within an Actor use non-atomic reference counting (MeldRef).
- **Isolate**: An OS-level process providing fault isolation and a separate address space. Isolates communicate via IPC (pipes, shared memory, or sockets). A crash in one Isolate does not affect others.
- **scheduler**: A per-Actor cooperative scheduler that manages a run queue of ready Fibers, performs context switches via `boost::context::callcc`, and integrates with the event loop for I/O-driven wake-ups.
- **event_loop**: A per-Actor reactor that multiplexes I/O readiness notifications (epoll on Linux, kqueue on macOS, IOCP on Windows) and dispatches ready callbacks to the scheduler.
- **Hardware_Stack**: A contiguous memory region allocated for a Fiber's execution stack, managed by `boost::context::callcc` for zero-copy context switching by saving and restoring CPU registers.
- **Message_Envelope**: A serialized, deep-copied value sent between Actors or Isolates. The envelope contains the payload, sender address, and a correlation ID for request-response patterns.
- **Actor_Address**: A unique identifier for an Actor, used to route messages. Actor_Addresses are valid within a single Isolate.
- **Isolate_Handle**: A handle to a child Isolate process, providing methods to send messages via IPC, check liveness, and request graceful shutdown.
- **Run_Queue**: A per-Actor FIFO queue of Fibers that are ready to execute. The scheduler dequeues Fibers from the Run_Queue and resumes them via `callcc`.
- **Continuation_Stack**: The hardware stack snapshot captured by `boost::context::callcc` when a Fiber suspends, containing saved CPU registers and stack pointer.
- **IPC_Channel**: A bidirectional communication channel between Isolates, implemented via OS pipes or Unix domain sockets, with length-prefixed serialized messages.
- **Mailbox**: A per-Actor message queue where incoming Message_Envelopes are buffered until the Actor's event_loop processes them.
- **primitive_suspend**: The single kernel control flow primitive, upgraded to use hardware stack-switching via `boost::context::callcc` instead of logical frame capture.
- **Completer\[T\]**: Handle for manually completing a deferred task from external code (callbacks, event systems).
- **Executor**: Thread pool or execution context for running async operations (I/O-optimized, CPU-optimized, or custom).
- **Combinator**: Function that combines or transforms Tasks (e.g., `thenCombine`, `applyToEither`).
- **Deferred_Task**: A Task created without immediate execution, completed manually via a Completer.
- **Terminal_Operation**: Operation that triggers execution and blocks until completion (e.g., `await`).
- **Intermediate_Operation**: Operation that transforms a Task without executing it (e.g., `map`, `flatMap`).

## Requirements

### Requirement 1: Fiber with Hardware Stack-Switching

**User Story:** As a language runtime developer, I want Fibers backed by hardware stack-switching, so that millions of lightweight concurrent tasks can run within a single Actor with minimal context-switch overhead.

#### Acceptance Criteria

1. THE Fiber SHALL allocate a Hardware_Stack via `boost::context::callcc` upon creation.
2. THE Fiber SHALL support a configurable stack size with a default of 64 KB and a minimum of 8 KB.
3. WHEN a Fiber yields or suspends, THE Fiber SHALL save its CPU register state and stack pointer via `boost::context::callcc`.
4. WHEN a Fiber is resumed, THE Fiber SHALL restore its CPU register state and stack pointer via `boost::context::callcc`.
5. WHEN a Fiber's entry function returns, THE Fiber SHALL transition to a completed state and release its Hardware_Stack.
6. IF a Fiber's entry function throws an unhandled exception, THEN THE Fiber SHALL capture the exception, transition to a failed state, and release its Hardware_Stack.
7. THE Fiber SHALL provide a unique identifier within its owning Actor.
8. THE Fiber SHALL track its state as one of: Created, Ready, Running, Suspended, Completed, Failed.

### Requirement 2: Fiber Scheduler

**User Story:** As a language runtime developer, I want a per-Actor cooperative Fiber scheduler, so that Fibers are fairly scheduled and I/O-blocked Fibers do not starve other Fibers.

#### Acceptance Criteria

1. THE scheduler SHALL maintain a Run_Queue of Fibers in the Ready state.
2. WHEN the scheduler is polled, THE scheduler SHALL dequeue the next Fiber from the Run_Queue and resume it via `callcc`.
3. WHEN a running Fiber yields, THE scheduler SHALL place the Fiber at the back of the Run_Queue.
4. WHEN a running Fiber suspends on I/O, THE scheduler SHALL register the I/O interest with the event_loop and remove the Fiber from the Run_Queue.
5. WHEN the event_loop signals that an I/O operation is ready, THE scheduler SHALL move the corresponding Fiber to the back of the Run_Queue.
6. WHEN the Run_Queue is empty and no I/O operations are pending, THE scheduler SHALL block on the event_loop until an event arrives.
7. THE scheduler SHALL run on the Actor's OS thread without spawning additional threads.
8. THE scheduler SHALL provide a `spawn` method that creates a new Fiber and adds it to the Run_Queue.

### Requirement 3: Per-Actor Event Loop / Reactor

**User Story:** As a language runtime developer, I want a per-Actor event loop that multiplexes I/O, so that Fibers can perform non-blocking I/O without dedicated threads per connection.

#### Acceptance Criteria

1. THE event_loop SHALL use epoll on Linux, kqueue on macOS, and IOCP on Windows for I/O readiness notification.
2. THE event_loop SHALL support registering file descriptors for read-readiness, write-readiness, and error conditions.
3. WHEN a registered file descriptor becomes ready, THE event_loop SHALL invoke the associated callback on the Actor's thread.
4. THE event_loop SHALL support timer events with millisecond resolution.
5. WHEN a timer expires, THE event_loop SHALL invoke the associated callback on the Actor's thread.
6. THE event_loop SHALL support a `poll` method that processes all ready events without blocking.
7. THE event_loop SHALL support a `run_one` method that blocks until at least one event is ready and processes it.
8. THE event_loop SHALL integrate with the scheduler so that I/O readiness wakes suspended Fibers.
9. THE event_loop SHALL process incoming messages from the Actor's Mailbox as a source of events.

### Requirement 4: Actor System

**User Story:** As a language runtime developer, I want an Actor system where each Actor runs on its own OS thread with a thread-local heap, so that CPU parallelism is achieved without shared mutable state.

#### Acceptance Criteria

1. THE Actor SHALL run on a dedicated OS thread.
2. THE Actor SHALL own a thread-local heap where all MeldObject allocations for that Actor reside.
3. THE Actor SHALL own a scheduler and an event_loop.
4. THE Actor SHALL own a Mailbox for receiving Message_Envelopes from other Actors.
5. WHEN an Actor is spawned, THE Actor system SHALL create a new OS thread, initialize the thread-local heap, scheduler, and event_loop, and begin processing.
6. THE Actor SHALL be identified by a unique Actor_Address within its Isolate.
7. WHEN an Actor's main Fiber completes, THE Actor SHALL drain its Mailbox, complete pending Fibers, and shut down its event_loop and OS thread.
8. IF an Actor's main Fiber fails with an unhandled exception, THEN THE Actor SHALL notify its supervisor (parent Actor or Isolate) and shut down.

### Requirement 5: Shared-Nothing Enforcement Between Actors

**User Story:** As a language designer, I want the runtime to enforce shared-nothing semantics between Actors, so that non-atomic reference counting within an Actor is safe and data races are impossible.

#### Acceptance Criteria

1. WHEN a value is sent from one Actor to another, THE Actor system SHALL perform a deep copy of the value into the receiving Actor's heap.
2. THE Actor system SHALL reject attempts to share MeldRef pointers across Actor boundaries at compile time via the Send trait.
3. WHILE an Actor is running, THE Actor SHALL access only objects allocated on its own thread-local heap.
4. THE Message_Envelope SHALL contain a serialized representation of the payload, not a pointer to the original object.
5. IF a type does not implement the Send trait, THEN THE type checker SHALL reject attempts to include that type in a Message_Envelope.

### Requirement 6: Actor Message Passing

**User Story:** As a language runtime developer, I want Actors to communicate via typed message passing, so that inter-Actor coordination is safe and structured.

#### Acceptance Criteria

1. THE Actor system SHALL provide a `send(target: Actor_Address, message: T)` function that enqueues a Message_Envelope in the target Actor's Mailbox.
2. THE `send` function SHALL be non-blocking and return immediately after enqueuing.
3. THE Actor system SHALL provide an `ask(target: Actor_Address, message: T) -> Task[R]` function that sends a message and returns a Task that completes when the response arrives.
4. THE `ask` function SHALL suspend the calling Fiber until the response Message_Envelope arrives or a timeout expires.
5. IF the target Actor_Address is invalid or the target Actor has shut down, THEN THE `send` function SHALL return an error result.
6. THE Mailbox SHALL be a lock-free multi-producer single-consumer queue.
7. WHEN the event_loop detects messages in the Mailbox, THE event_loop SHALL dispatch each message to the Actor's message handler Fiber.

### Requirement 7: Isolate System

**User Story:** As a language runtime developer, I want Isolates backed by OS processes, so that untrusted or crash-prone code runs in a separate address space without affecting the host.

#### Acceptance Criteria

1. WHEN an Isolate is spawned, THE Isolate system SHALL create a new OS process via `fork`/`exec` on Unix or `CreateProcess` on Windows.
2. THE Isolate SHALL have its own address space, heap, and set of Actors.
3. THE Isolate system SHALL establish an IPC_Channel between the parent and child Isolate upon creation.
4. THE Isolate_Handle SHALL provide a `send(message: T)` method that serializes the message and writes it to the IPC_Channel.
5. THE Isolate_Handle SHALL provide a `recv() -> Task[T]` method that reads and deserializes the next message from the IPC_Channel.
6. IF a child Isolate process crashes, THEN THE parent Isolate SHALL receive a notification via the Isolate_Handle without crashing itself.
7. THE Isolate_Handle SHALL provide a `shutdown()` method that sends a graceful shutdown signal and waits for the child process to exit.
8. THE Isolate system SHALL support a configurable timeout for graceful shutdown, after which the child process is forcefully terminated.

### Requirement 8: Isolate IPC Serialization

**User Story:** As a language runtime developer, I want Isolate IPC to use efficient serialization, so that cross-process communication has acceptable overhead.

#### Acceptance Criteria

1. THE IPC_Channel SHALL use length-prefixed binary framing for messages.
2. THE IPC_Channel SHALL serialize MeldObject values into a compact binary format.
3. THE IPC_Channel SHALL deserialize received bytes into MeldObject values on the receiving Isolate's heap.
4. FOR ALL serializable MeldObject values, serializing then deserializing SHALL produce a value equivalent to the original (round-trip property).
5. IF a received message fails to deserialize, THEN THE IPC_Channel SHALL return a descriptive error rather than crashing.
6. THE IPC_Channel serializer SHALL support all 14 Kernel_Types (Symbol, Cell, Vec, Nil, Function, Integer, Float, Boolean, String, Placeholder, Optional, Continuation, NativeHandle, NativeFunction).
7. IF a value contains a non-serializable type (NativeHandle, Function with captured closures), THEN THE serializer SHALL return an error indicating the type cannot cross Isolate boundaries.

### Requirement 9: primitive_suspend Upgrade to Hardware Stack-Switching

**User Story:** As a kernel developer, I want `primitive_suspend` to use hardware stack-switching instead of logical frame capture, so that all control flow (exceptions, async/await, generators, effects) benefits from zero-copy context switching.

#### Acceptance Criteria

1. THE `primitive_suspend` function SHALL save the current Fiber's CPU registers and stack pointer via `boost::context::callcc`.
2. THE `primitive_suspend` function SHALL pass a `Continuation` object to the callback that, when resumed, restores the saved registers and stack pointer.
3. THE `primitive_suspend` function SHALL maintain the same function signature: `primitive_suspend(delimiter_id, callback) -> Value`.
4. WHEN a Continuation is resumed with a value, THE Continuation SHALL restore the Fiber's execution context and return the value to the suspension point.
5. WHEN a Continuation is resumed multiple times (multi-shot), THE Continuation SHALL clone the saved stack before restoring.
6. THE upgraded `primitive_suspend` SHALL produce identical observable behavior to the current logical frame-based implementation for all existing callers.
7. THE `mark_stack`, `suspend`, and `resume` library functions SHALL continue to work without modification after the upgrade.

### Requirement 10: Integration with Task[T], coroutineScope, launch

**User Story:** As a Meld developer, I want the existing structured concurrency API to work transparently on top of the 3-tier runtime, so that I do not need to change my code.

#### Acceptance Criteria

1. WHEN `launch` is called, THE runtime SHALL spawn a new Fiber on the current Actor's scheduler.
2. WHEN `coroutineScope` is called, THE runtime SHALL create a scope that tracks all Fibers spawned within it and waits for their completion before returning.
3. IF any Fiber within a `coroutineScope` fails, THEN THE runtime SHALL cancel all sibling Fibers and propagate the error to the scope.
4. THE `Task[T]` type SHALL represent a Fiber's result, providing `await()` to suspend the calling Fiber until the result is available.
5. WHEN `Task.await()` is called, THE calling Fiber SHALL suspend via `primitive_suspend` and resume when the target Fiber completes.
6. THE `Runtime.runAsync` entry point SHALL create a default Actor on the calling thread and run the provided function as the Actor's main Fiber.
7. THE existing `TaskScope`, `CancellationToken`, and `CancellationException` types SHALL function identically after the migration.

### Requirement 11: Actor Supervision

**User Story:** As a language runtime developer, I want Actors to support supervision strategies, so that Actor failures are handled systematically rather than crashing the entire system.

#### Acceptance Criteria

1. WHEN an Actor is spawned, THE spawning Actor SHALL become the supervisor of the new Actor.
2. IF a supervised Actor fails, THEN THE supervisor SHALL receive a failure notification containing the Actor_Address and the error.
3. THE Actor system SHALL support a "restart" supervision strategy that restarts the failed Actor with a fresh heap.
4. THE Actor system SHALL support a "stop" supervision strategy that shuts down the failed Actor permanently.
5. THE Actor system SHALL support an "escalate" supervision strategy that propagates the failure to the supervisor's own supervisor.
6. THE supervision strategy SHALL be configurable per-Actor at spawn time.

### Requirement 12: Cross-Platform Compatibility

**User Story:** As a language runtime developer, I want the 3-tier runtime to work on Linux, macOS, and Windows, so that Meld programs are portable.

#### Acceptance Criteria

1. THE Fiber implementation SHALL use `boost::context::callcc` which supports Linux (x86_64, ARM64), macOS (x86_64, ARM64), and Windows (x86_64).
2. THE event_loop SHALL abstract over epoll (Linux), kqueue (macOS), and IOCP (Windows) behind a unified interface.
3. THE Isolate system SHALL abstract over `fork`/`exec` (Unix) and `CreateProcess` (Windows) behind a unified interface.
4. THE IPC_Channel SHALL use Unix domain sockets on Unix and named pipes on Windows.
5. THE Actor system SHALL use platform-native thread creation (`pthread_create` on Unix, `CreateThread` on Windows) via `std::thread`.

### Requirement 13: Concurrency Runtime Example

**User Story:** As a developer learning Meld, I want an example that demonstrates Fibers, Actors, and Isolates, so that I can understand the 3-tier concurrency model.

#### Acceptance Criteria

1. THE example SHALL consist of a separate `concurrency-runtime-demo.cpp` file and a `concurrency-runtime-demo.meld` file following the meld-examples-structure guidelines.
2. THE `concurrency-runtime-demo.cpp` file SHALL demonstrate spawning Fibers within an Actor, spawning multiple Actors with message passing, and spawning an Isolate with IPC.
3. THE `concurrency-runtime-demo.meld` file SHALL demonstrate the Meld language API for `launch`, `coroutineScope`, Actor creation, message sending, and Isolate spawning.
4. THE example SHALL demonstrate that a crash in an Isolate does not affect the parent process.
5. THE example SHALL demonstrate that Actors communicate only via message passing with no shared state.

### Requirement 14: Advanced Error Handling

**User Story:** As a developer, I want sophisticated error handling for async operations, so that I can recover from failures, provide fallbacks, and handle both success and error cases uniformly.

#### Acceptance Criteria

1. THE Task[T] type SHALL provide exceptionally method that accepts an error handler function and returns a new Task[T]
2. WHEN a task fails and exceptionally is called, THE error handler SHALL be invoked with the error and its return value SHALL become the task result
3. THE Task[T] type SHALL provide handle method that accepts a handler for both success and failure cases
4. THE handle method SHALL receive a Result[T, Error] and return a value of type U, producing Task[U]
5. THE Task[T] type SHALL provide whenComplete method that runs an action on completion regardless of success or failure
6. THE whenComplete method SHALL receive the Result[T, Error] but SHALL NOT transform the task value
7. WHEN multiple error handlers are chained, THE first matching handler SHALL process the error
8. THE error handling methods SHALL preserve structured concurrency semantics

### Requirement 15: Manual Task Completion

**User Story:** As a developer, I want to create tasks that can be completed manually, so that I can integrate async operations with callback-based APIs, event systems, and external libraries.

#### Acceptance Criteria

1. THE Task type SHALL provide static deferred method that returns a tuple of (Task[T], Completer[T])
2. THE Completer[T] type SHALL provide complete method that accepts a value of type T
3. THE Completer[T] type SHALL provide completeError method that accepts an Error
4. WHEN complete is called on a Completer, THE associated Task SHALL resolve with the provided value
5. WHEN completeError is called on a Completer, THE associated Task SHALL fail with the provided error
6. THE Task type SHALL provide static completed method that returns an immediately completed Task[T]
7. THE Task type SHALL provide static failed method that returns an immediately failed Task[T]
8. WHEN a Completer is used multiple times, THE first completion SHALL win and subsequent calls SHALL be ignored
9. THE deferred tasks SHALL integrate with structured concurrency scopes
10. THE Completer SHALL be thread-safe for concurrent completion attempts

### Requirement 16: Heterogeneous Task Combining

**User Story:** As a developer, I want to combine tasks of different types in a type-safe manner, so that I can coordinate multiple async operations without losing type information.

#### Acceptance Criteria

1. THE Task type SHALL provide static all2 method that accepts two tasks of different types and returns Task[(T1, T2)]
2. THE Task type SHALL provide static all3 method that accepts three tasks and returns Task[(T1, T2, T3)]
3. THE Task type SHALL provide static all4 through all10 methods for up to 10 heterogeneous tasks
4. WHEN all tasks in an allN call complete successfully, THE result SHALL be a tuple of all values in order
5. WHEN any task in an allN call fails, THE entire combined task SHALL fail with that error
6. THE Task type SHALL provide thenCombine method that combines two tasks with a combining function
7. THE thenCombine method SHALL accept Task[U] and function (T, U) -> V, returning Task[V]
8. WHEN both tasks complete successfully, THE combining function SHALL be called with both results
9. THE heterogeneous combining methods SHALL preserve structured concurrency semantics
10. THE type system SHALL enforce type safety for all heterogeneous combinations

### Requirement 17: State Inspection

**User Story:** As a developer, I want to inspect task state without blocking, so that I can make decisions based on completion status and retrieve results when available.

#### Acceptance Criteria

1. THE Task[T] type SHALL provide isDone method that returns bool indicating completion status
2. THE Task[T] type SHALL provide isCompletedExceptionally method that returns bool indicating error completion
3. THE Task[T] type SHALL provide isCancelled method that returns bool indicating cancellation status
4. THE Task[T] type SHALL provide getNow method that accepts a default value and returns T immediately
5. WHEN getNow is called on an incomplete task, THE default value SHALL be returned
6. WHEN getNow is called on a completed task, THE actual result SHALL be returned
7. THE Task[T] type SHALL provide resultNow method that returns Result[T, Error]? (nullable)
8. WHEN resultNow is called on an incomplete task, THE return value SHALL be null
9. WHEN resultNow is called on a completed task, THE return value SHALL be the Result
10. THE state inspection methods SHALL NOT block or trigger task execution

### Requirement 18: Delay and Timeout Enhancements

**User Story:** As a developer, I want better control over timing in async operations, so that I can delay execution, provide timeout fallbacks, and handle temporal constraints elegantly.

#### Acceptance Criteria

1. THE Task type SHALL provide static delay method that accepts milliseconds and returns Task[Unit]
2. THE Task[T] type SHALL provide delayedBy method that delays task execution by specified milliseconds
3. THE Task[T] type SHALL provide completeOnTimeout method that accepts milliseconds and default value
4. WHEN completeOnTimeout times out, THE task SHALL complete with the default value instead of failing
5. THE Task[T] type SHALL provide orTimeout method that fails the task on timeout (existing withTimeout behavior)
6. THE delay method SHALL integrate with structured concurrency scopes
7. WHEN a delayed task's scope is cancelled, THE delay SHALL be cancelled
8. THE timeout methods SHALL use the same time source as the effect system for deterministic testing
9. THE delayedBy method SHALL return a new Task that starts after the delay
10. THE delay and timeout methods SHALL be composable with other task operations

### Requirement 19: Advanced Task Combinators

**User Story:** As a developer, I want advanced combinators for complex async workflows, so that I can express sophisticated coordination patterns concisely.

#### Acceptance Criteria

1. THE Task[T] type SHALL provide thenAccept method that accepts (T) -> Unit and returns Task[Unit]
2. THE Task[T] type SHALL provide thenRun method that accepts () -> Unit and returns Task[Unit]
3. THE Task[T] type SHALL provide applyToEither method that races two tasks and applies function to winner
4. THE Task[T] type SHALL provide acceptEither method that races two tasks and runs action on winner
5. THE Task[T] type SHALL provide runAfterBoth method that runs action after both tasks complete
6. THE Task[T] type SHALL provide runAfterEither method that runs action after either task completes
7. WHEN thenAccept is used, THE function SHALL be called with the result for side effects only
8. WHEN thenRun is used, THE action SHALL run after completion regardless of result value
9. THE racing combinators (applyToEither, acceptEither) SHALL cancel the losing task
10. THE coordination combinators (runAfterBoth, runAfterEither) SHALL handle errors from any task

### Requirement 20: Async Execution Control

**User Story:** As a developer, I want to control which thread pool executes async operations, so that I can optimize for I/O-bound vs CPU-bound work and prevent thread pool exhaustion.

> **Clarification:** An Executor is a managed pool of Actors. `Executor.io()` returns a pool optimized for I/O-heavy work (many Actors with lightweight fibers waiting on I/O). `Executor.cpu()` returns a pool sized to core count for compute-bound work. When `mapAsync` or `flatMapAsync` is called with an Executor, the work is dispatched as a message to an Actor in that pool. Within each Actor in the pool, fibers are still cooperatively scheduled by the scheduler (Req 2). The default executor (criterion 8) is the current Actor — work stays local unless explicitly dispatched.

#### Acceptance Criteria

1. THE Executor type SHALL provide static io method that returns an I/O-optimized executor
2. THE Executor type SHALL provide static cpu method that returns a CPU-optimized executor
3. THE Executor type SHALL provide static custom method that accepts thread pool configuration
4. THE Task[T] type SHALL provide mapAsync method that accepts Executor and mapping function
5. THE Task[T] type SHALL provide flatMapAsync method that accepts Executor and async mapping function
6. WHEN mapAsync is used, THE mapping function SHALL execute on the specified executor
7. WHEN flatMapAsync is used, THE async operation SHALL execute on the specified executor
8. THE default executor SHALL be used when no executor is specified
9. THE executor system SHALL integrate with structured concurrency for proper cleanup
10. THE Executor type SHALL support custom executors for specialized use cases

### Requirement 21: Task Copying and Independence

**User Story:** As a developer, I want to create independent copies of tasks, so that I can share task results across multiple consumers without interference.

#### Acceptance Criteria

1. THE Task[T] type SHALL provide copy method that returns a new independent Task[T]
2. WHEN copy is called, THE new task SHALL share the same underlying computation
3. THE copied task SHALL have independent completion handlers and transformations
4. WHEN the original task completes, THE copied task SHALL also complete with the same result
5. THE copy method SHALL preserve the task's completion state if already completed
6. THE copied tasks SHALL maintain their own cancellation state
7. WHEN a copied task is cancelled, THE original task SHALL NOT be affected
8. THE copy method SHALL work with both pending and completed tasks
9. THE copied tasks SHALL integrate properly with structured concurrency scopes
10. THE copy operation SHALL be efficient and not duplicate the underlying computation

### Requirement 22: Backward Compatibility

**User Story:** As a developer, I want new async features to integrate seamlessly with existing code, so that I can adopt enhancements incrementally without breaking changes.

#### Acceptance Criteria

1. THE enhanced Task[T] type SHALL maintain all existing methods (create, await, map, flatMap, withTimeout, etc.)
2. THE existing Task.all and Task.race methods SHALL continue to work unchanged
3. THE new methods SHALL follow the same naming conventions as existing methods
4. THE new methods SHALL integrate with algebraic effects system
5. THE new methods SHALL work within structured concurrency scopes (coroutineScope, launch)
6. THE existing async demo examples SHALL continue to work without modification
7. THE new features SHALL be documented as additive enhancements
8. THE type signatures SHALL remain compatible with existing code
9. THE error handling behavior SHALL be consistent with existing Result[T, E] patterns
10. THE new features SHALL support the same testing and mocking patterns as existing async code

### Requirement 23: Integration with Effect System

**User Story:** As a developer, I want async enhancements to work seamlessly with algebraic effects, so that I can sandbox, test, and control side effects in advanced async workflows.

#### Acceptance Criteria

1. THE delay and timeout features SHALL use EffectTime for time operations
2. WHEN delay is used within an effect handler, THE handler SHALL be able to intercept and control timing
3. THE manual completion features SHALL work within effect handlers
4. THE executor system SHALL integrate with effect handlers for I/O and network operations
5. THE error handling methods SHALL work with effect-based error recovery
6. WHEN testing async code with effects, THE new combinators SHALL support deterministic behavior
7. THE state inspection methods SHALL work correctly with effect-sandboxed tasks
8. THE heterogeneous combining SHALL preserve effect declarations
9. THE advanced combinators SHALL propagate effect requirements correctly
10. THE effect system SHALL enable full control over async behavior for testing and sandboxing


---

> **Note:** Requirement 24 below was added to address the AI Developer Experience (AI_DX.md) gap: deterministic cross-actor scheduling for reproducible test execution.

### Requirement 24: Deterministic Actor Scheduling

**User Story:** As an AI agent running tests, I want cross-actor message ordering to be deterministic when Agent-Test mode is active, so that concurrent programs produce reproducible traces and test failures can be replayed exactly.

#### Acceptance Criteria

1. WHEN Agent-Test mode is active (`.kiro/specs/meld-core/requirements.md` Req 143), THE Actor system SHALL use a deterministic scheduling policy for cross-actor message delivery
2. THE deterministic scheduling policy SHALL order message delivery by: (1) virtual timestamp of the send operation, (2) sender Actor_Address as tiebreaker, (3) message sequence number within the sender as final tiebreaker — producing a total order on all messages
3. WHEN multiple Actors have messages ready for delivery at the same virtual timestamp, THE scheduler SHALL deliver messages in ascending Actor_Address order (round-robin by ID)
4. THE deterministic scheduler SHALL integrate with the virtual time effect (`.kiro/specs/meld-core/requirements.md` Req 142) — message delivery timestamps are derived from the virtual clock, not the system clock
5. WHEN Agent-Test mode is NOT active, THE Actor system SHALL use its default non-deterministic scheduling (Req 2, Req 4) with no performance overhead from the deterministic scheduling infrastructure
6. THE deterministic scheduler SHALL produce a structured execution trace containing: each message delivery (sender, receiver, virtual timestamp, message type), each fiber context switch, and each effect invocation — serializable to JSON for post-mortem analysis
7. THE execution trace format SHALL be compatible with the Flight Recorder snapshot format (`.kiro/specs/meld-core/requirements.md` Req 44) so that traces can be replayed via `Runtime.replay(snapshot)`
8. FOR ALL programs that do not use external I/O effects, running the same program twice in Agent-Test mode with the same inputs SHALL produce byte-identical execution traces

> **Cross-reference:** Agent-Test mode activation is defined in `.kiro/specs/meld-core/requirements.md` Req 143. The `--agent-test` CLI flag is defined in `.kiro/specs/meld-cli/requirements.md` Req 19.
