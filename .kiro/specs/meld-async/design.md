# Design Document: Three-Tier Concurrency Runtime

## Overview

The meld-async library implements a 3-tier concurrency runtime entirely in pure Meld, built on kernel primitives. No C++ runtime code is required — the kernel provides `kernel.suspend`, `kernel.resume`, and `kernel.call` as the only bridge to the host machine.

**Three tiers:**
- **Fibers** — green threads within an Actor, cooperative scheduling via `kernel.suspend`
- **Actors** — OS threads with private heaps, communicate via deep-copied messages
- **Isolates** — OS processes with fault isolation, communicate via IPC

The runtime sits beneath the user-facing structured concurrency API (`Task[T]`, `coroutine-scope`, `launch`).

## Key Design Decisions

- **Pure Meld, not C++**: Unlike Tokio (Rust) which is written in Rust, meld-async is written in Meld. The kernel's `kernel.suspend` already provides hardware stack-switching (backed by `boost::context::callcc` in the C++ kernel). The scheduler, channels, tasks, and actors are all Meld code.
- **`kernel.suspend` is the universal primitive**: Fibers yield by calling `kernel.suspend("scheduler", callback)`. The callback receives a `Continuation` which the scheduler stores and later resumes via `kernel.resume`.
- **Per-Actor Fiber Scheduler**: Each Actor runs its own cooperative FIFO scheduler. No work-stealing across Actors — preserves the shared-nothing guarantee.
- **Effects gate all I/O**: Functions that use OS resources declare `@requires(Thread)`, `@requires(IO)`, `@requires(Random)`, etc. The compiler enforces this statically.
- **Deep copy for message passing**: Values crossing Actor boundaries are deep-copied. No shared mutable state between threads.
- **IPC for Isolates**: Isolate communication uses `kernel.call` to OS-level socketpair/pipes. Serialization is handled in Meld.

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│  User Code                                              │
│    coroutine-scope { launch { ... } }                   │
├─────────────────────────────────────────────────────────┤
│  std/async/scope.meld    — structured concurrency       │
│  std/async/task.meld     — Task[T], await, map, race    │
├─────────────────────────────────────────────────────────┤
│  std/async/scheduler.meld — FIFO run queue              │
│  std/async/fiber.meld     — green threads               │
│  std/async/channel.meld   — MPSC message passing        │
├─────────────────────────────────────────────────────────┤
│  std/async/actor.meld    — OS thread per actor          │
│  std/async/isolate.meld  — OS process isolation         │
├─────────────────────────────────────────────────────────┤
│  std/os/thread.meld      — effect Thread                │
│  std/os/io.meld          — effect IO                    │
│  std/os/atomic.meld      — effect Atomic                │
│  std/os/timer.meld       — effect Timer                 │
│  std/os/memory.meld      — effect Memory                │
│  std/os/random.meld      — effect Random                │
├─────────────────────────────────────────────────────────┤
│  std/kernel.meld         — kernel.suspend/resume/call   │
├─────────────────────────────────────────────────────────┤
│  C++ Kernel (primitives.hpp)                            │
│    primitive_suspend → boost::context::callcc           │
│    native_call → dlsym / OS syscalls                    │
└─────────────────────────────────────────────────────────┘
```

## Module Breakdown

### std/async/fiber.meld

A Fiber is a lightweight execution context. It wraps a function and a `Continuation` (captured via `kernel.suspend`). The scheduler resumes fibers by calling `kernel.resume` on their stored continuation.

```meld
struct Fiber {
    val id: int
    var state: FiberState
    var continuation: Continuation?
    var entry: fnc() -> ()
}
```

State machine: `Created → Running → Suspended → Running → ... → Completed | Failed`

### std/async/scheduler.meld

A cooperative FIFO scheduler. One per Actor. Maintains a run queue and a suspended map.

- `spawn(f)` — create fiber, enqueue
- `yield()` — current fiber suspends via `kernel.suspend`, re-enqueued at back
- `park()` — current fiber suspends, moved to suspended map (awaiting wake)
- `wake(id)` — move fiber from suspended to run queue
- `run-all()` — loop: dequeue front, resume, repeat until empty

### std/async/channel.meld

MPSC channel for inter-fiber communication. `send` is non-blocking (appends to buffer, wakes parked receiver). `recv` parks the fiber if buffer is empty.

### std/async/task.meld

`Task[T]` wraps a fiber that produces a value. `await` parks the calling fiber and registers it as a waiter. When the task's fiber completes, all waiters are woken.

Combinators: `map`, `flat-map`, `all`, `race`, `handle`, `with-timeout`, `delay`.

### std/async/scope.meld

Structured concurrency: `coroutine-scope` ensures all launched tasks complete before the scope exits. If any task fails, siblings are cancelled.

### std/async/actor.meld

An Actor is an OS thread (spawned via `@requires(Thread)`) with its own scheduler. Actors communicate by sending messages to each other's channels. Messages are deep-copied across the thread boundary.

### std/async/isolate.meld

An Isolate is an OS process (spawned via `kernel.call("isolate_spawn", ...)`). Communication is via IPC file descriptors. A crash in a child isolate cannot affect the parent.

## Effect Requirements

| Module | Effects Required |
|--------|-----------------|
| fiber.meld | none (pure kernel.suspend) |
| scheduler.meld | none (pure kernel.suspend) |
| channel.meld | none |
| task.meld | none |
| scope.meld | none |
| actor.meld | `@requires(Thread)` |
| isolate.meld | `@requires(Thread, IO)` |

## Deterministic Testing

In Agent-Test mode, the `Random` effect handler is replaced with a seeded PRNG using `.with()`:

```meld
{
    val result = my-concurrent-algorithm()
    assert(result == expected)
}.with(
    Random { fnc int() { rtn seeded-next() } },
    Timer { fnc now-ms() { rtn virtual-clock.tick() } }
)
```

This enables reproducible property-based testing of concurrent code.

## Kernel Primitives Used

| Primitive | Used By | Purpose |
|-----------|---------|---------|
| `kernel.suspend(delimiter, callback)` | fiber, scheduler | Capture continuation |
| `kernel.resume(k, value)` | scheduler | Resume a parked fiber |
| `kernel.call("isolate_spawn", ...)` | isolate | Fork OS process |
| `kernel.call("isolate_kill", ...)` | isolate | Kill child process |

## Relationship to C++ Kernel

The C++ kernel (`primitives.hpp`) provides the hardware stack-switching via `boost::context::callcc` inside `primitive_suspend`. From Meld's perspective, `kernel.suspend` is an opaque operation that freezes the current execution state into a `Continuation` value. The Meld-side scheduler never touches hardware registers directly — it just stores and resumes `Continuation` values.

This means:
- The scheduler logic is testable in pure Meld (mock `kernel.suspend` in tests)
- The fiber/task/channel code has zero platform-specific behavior
- Only `std/os/*.meld` and `std/async/isolate.meld` touch OS-level operations
