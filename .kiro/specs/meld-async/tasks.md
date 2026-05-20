# Implementation Plan: Three-Tier Concurrency Runtime (Pure Meld)

## Overview

Implement the 3-tier concurrency runtime as a pure Meld library built on kernel primitives (`kernel.suspend`, `kernel.resume`, `kernel.call`). No C++ runtime code. The implementation proceeds bottom-up: fiber → scheduler → channel → task → scope → actor → isolate.

All code lives in `meld-core/std/async/` and `meld-core/std/os/`.

## Tasks

- [x] 1. Implement Fiber (`std/async/fiber.meld`)
  - [x] 1.1 Define FiberState enum and Fiber struct
  - [x] 1.2 Implement `create-fiber(f)` using `kernel.suspend` to capture initial continuation
  - [x] 1.3 Implement `resume(k)` to resume a fiber's continuation
  - [x] 1.4 Implement monotonic fiber ID generation

- [x] 2. Implement FiberScheduler (`std/async/scheduler.meld`)
  - [x] 2.1 Define FiberScheduler struct with run-queue and suspended map
  - [x] 2.2 Implement `spawn(sched, f)` — create fiber and enqueue
  - [x] 2.3 Implement `yield(sched)` — suspend current fiber via `kernel.suspend`, re-enqueue at back
  - [x] 2.4 Implement `park(sched)` — suspend current fiber, move to suspended map
  - [x] 2.5 Implement `wake(sched, id)` — move fiber from suspended to run queue
  - [x] 2.6 Implement `run-all(sched)` — FIFO loop: dequeue, resume, repeat
  - [x] 2.7 Implement `tick(sched)` — single scheduling step

- [x] 3. Implement Channel (`std/async/channel.meld`)
  - [x] 3.1 Define Channel[T] struct with buffer, closed flag, waiting-receiver
  - [x] 3.2 Implement `send(ch, msg)` — append to buffer, wake parked receiver
  - [x] 3.3 Implement `recv(ch)` — pop from buffer or park fiber until message arrives
  - [x] 3.4 Implement `close(ch)` — mark closed, wake parked receiver

- [x] 4. Implement Task[T] (`std/async/task.meld`)
  - [x] 4.1 Define Task[T] struct with state, result, error, waiters list
  - [x] 4.2 Implement `create(f)` — spawn fiber that runs f, stores result, wakes waiters
  - [x] 4.3 Implement `await(task)` — park caller until task completes
  - [x] 4.4 Implement `map`, `flat-map` combinators
  - [x] 4.5 Implement `all(tasks)` — parallel execution, collect results
  - [x] 4.6 Implement `race(tasks)` — return first completed
  - [x] 4.7 Implement `handle(task, handler)` — error recovery
  - [x] 4.8 Implement `with-timeout(task, ms)` — timeout via race with sleep
  - [x] 4.9 Implement `delay(ms, value)` — delayed value
  - [x] 4.10 Implement `completed(value)` and `failed(error)` — immediate tasks
  - [x] 4.11 Implement `Completer[T]` — manual completion (deferred/complete/complete-error)
  - [x] 4.12 Implement state inspection: `is-done`, `is-failed`, `is-cancelled`

- [x] 5. Implement Structured Concurrency (`std/async/scope.meld`)
  - [x] 5.1 Define Scope struct with task list and cancelled flag
  - [x] 5.2 Implement `coroutine-scope(block)` — run block, await all tasks, cancel on failure
  - [x] 5.3 Implement `launch(scope, f)` — spawn task within scope
  - [x] 5.4 Implement `cancel-all(scope)` — cancel all tasks in scope

- [x] 6. Implement Actor (`std/async/actor.meld`)
  - [x] 6.1 Define ActorRef, Message, ActorConfig, SupervisionStrategy
  - [x] 6.2 Implement `spawn-actor(main, config)` — OS thread with private scheduler
  - [x] 6.3 Implement `send(target, payload, sender)` — deep-copy message to mailbox
  - [x] 6.4 Implement `receive(self)` — suspend until message arrives
  - [x] 6.5 Implement `ask(target, payload, sender)` — send and await response
  - [x] 6.6 Implement `stop(actor)` — close mailbox

- [x] 7. Implement Isolate (`std/async/isolate.meld`)
  - [x] 7.1 Define IsolateRef struct
  - [x] 7.2 Implement `spawn-isolate(module, args)` via `kernel.call`
  - [x] 7.3 Implement `send-to-isolate` / `recv-from-isolate` via IO effect
  - [x] 7.4 Implement `shutdown(iso, timeout)` — graceful + force kill
  - [x] 7.5 Implement `is-alive(iso)` and `kill(iso)`

- [x] 8. Implement OS Effect Layers (`std/os/*.meld`)
  - [x] 8.1 `effect Thread` — spawn, join, sleep-ms, current-id, yield
  - [x] 8.2 `effect IO` — poll, read, write, close, listen, accept
  - [x] 8.3 `effect Atomic` — load, store, compare-and-swap, fetch-add, fence
  - [x] 8.4 `effect Timer` — now-ms, sleep-until
  - [x] 8.5 `effect Memory` — alloc, free, alloc-stack, free-stack
  - [x] 8.6 `effect Random` — int, float, int-range

- [x] 9. Property Tests
  - [x] 9.1 Fiber suspend/resume preserves local state
  - [x] 9.2 FIFO scheduling order
  - [x] 9.3 Channel send/recv linearizability
  - [x] 9.4 Task await returns correct result
  - [x] 9.5 Scope cancels siblings on failure
  - [x] 9.6 Actor message delivery (no loss/duplication)
  - [x] 9.7 Deterministic scheduling reproducibility

- [x] 10. Examples
  - [x] 10.1 `meld-examples/examples/28-concurrency.meld` — demonstrate all three tiers
  - [x] 10.2 `meld-examples/examples/30-async-patterns.meld` — producer/consumer, fan-out, pipeline

## Notes

- All implementation is in Meld (`.meld` files), not C++
- The only C++ dependency is the kernel (`kernel.suspend`, `kernel.resume`, `kernel.call`)
- Effects (`@requires(Thread)`, `@requires(IO)`) are declared as annotations above function signatures
- Property tests use `std/test/property.meld` with `@requires(Random)` for generation
- The scheduler is testable in isolation by mocking `kernel.suspend`
