# Meld Standard Library Plan

> Comprehensive standard library design for an AI-native, security-first language.
> Informed by: C++23, Rust, Python 3.13, Java 26, Go 1.24

## Design Principles

1. **Ironclad / Zero-Trust** — All I/O gated by `@requires(Effect)` annotations; the compiler enforces sandbox boundaries
2. **Hold/View Tenancy** — Ownership (`Hold[T]`) and borrowing (`View[T]`) are the universal memory model; all std APIs express lifetimes through this system
3. **AI-Native** — First-class tensors, inference, tokenization, agent interop in std
4. **Batteries Included** — Production-grade HTTP, crypto, DB on Day 1 (Go philosophy)
5. **Zero-Cost Abstractions** — C++-level performance via contiguous memory, lazy pipelines, compile-time DI
6. **Structured Concurrency** — All async work is lifetime-bound; no orphan tasks, no leaked sockets
7. **Pure Meld Runtime** — The async runtime (fibers, scheduler, channels, tasks, actors) is written in Meld, not C++. Only the kernel bridge (`kernel.suspend`, `kernel.call`) touches the host.

## Effect System Integration

Every std module that performs I/O declares its required effects via `@requires(Effect)` annotations above function signatures. The compiler refuses to compile code that uses `std.net` without `@requires(Net)` in scope. This enables:
- **Zerobox sandboxing** — binaries declare their capability surface in `meld.toml`
- **Graceful degradation** — libraries query `std.sandbox.has_effect("net")` at runtime
- **Audit trail** — the effect graph is statically extractable for security review
- **Deterministic testing** — effects can be replaced with mock handlers (e.g., seeded PRNG for `Random`, virtual clock for `Time`)

## Kernel Module (`kernel.*`)

The kernel is the only bridge between Meld code and the host machine. All std/ modules are built on these primitives:

| Primitive | Purpose |
|-----------|---------|
| `kernel.suspend(delimiter, callback)` | Capture continuation (powers fibers, effects, generators) |
| `kernel.resume(continuation, value)` | Resume a captured continuation |
| `kernel.call(name, ...args)` | FFI to C/OS functions |
| `kernel.load(path)` | Load native shared library |
| `kernel.eval(expr)` | Evaluate Meld expression |
| `kernel.meta-set(obj, key, value)` | Attach hidden metadata |
| `kernel.meta-get(obj, key)` | Retrieve metadata |

See `std/kernel.meld` for the full API specification.

## Standard Library Layering

```
User Code
  │  @requires(Net, Fs, Random, ...)
  ▼
std/*.meld          — User-facing effects + handlers (console, fs, net, time, random)
  │  delegates to kernel.call
  ▼
std/os/*.meld       — Low-level OS primitives (thread, io, atomic, timer, memory)
  │  delegates to kernel.call
  ▼
std/async/*.meld    — Pure Meld concurrency runtime (fiber, scheduler, channel, task, scope, actor, isolate)
  │  uses kernel.suspend / kernel.resume
  ▼
std/kernel.meld     — Kernel API contract (documentation)
  │
  ▼
C++ Kernel (primitives.hpp) — boost::context::callcc, dlsym, OS syscalls
```

---

## Tier 1 — Core `std` (ships with every Meld installation)

These modules have zero external dependencies and form the language foundation.

### 1.1 `std.core` — Fundamental Types & Error States
| What | Inspired By |
|------|-------------|
| Primitives: `bool`, `i8`–`i128`, `u8`–`u128`, `f16`/`f32`/`f64`/`f128`, `char`, `str`, `byte` | Rust, C++ |
| Unit `()`, Never `!` | Rust |
| `Result[T, E]` — forced explicit failure handling via pattern match | Rust `std::result` |
| `Option[T]` — nullable states without null pointers | Rust `std::option` |
| `Range`, `RangeInclusive` | Rust `std::ops` |
| Tuples (heterogeneous, up to N) | Rust, Python |
| `Any` (type-erased, runtime reflection) | C++ `std::any`, Rust `std::any` |

**Meld difference:** No `try/catch` exceptions. Functions that can fail *must* return `Result[T, E]`. The compiler (and AI Agent) enforces exhaustive handling before code compiles.

### 1.2 `std.collections` — Data Structures
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `std.collections.vec` | `Vec[T]` growable array | Rust `Vec`, C++ `vector` |
| `std.collections.array` | Fixed-size `Array[T, N]` | C++ `std::array`, Rust |
| `std.collections.deque` | Double-ended queue | Python `collections.deque`, C++ `deque` |
| `std.collections.list` | Linked list (singly + doubly) | C++ `list`/`forward_list` |
| `std.collections.map` | `HashMap[K,V]`, `BTreeMap[K,V]` | Rust, Java `HashMap`/`TreeMap` |
| `std.collections.flat_map` | `FlatMap[K,V]`, `FlatSet[T]` — contiguous memory, cache-friendly | C++23 `flat_map`/`flat_set` |
| `std.collections.set` | `HashSet[T]`, `BTreeSet[T]` | Rust, Java |
| `std.collections.heap` | Binary heap / priority queue | Go `container/heap`, C++ `priority_queue` |
| `std.collections.ring` | Circular buffer | Go `container/ring` |
| `std.collections.bitset` | Fixed + dynamic bitsets | C++ `bitset`, Rust `bitflags` |
| `std.collections.concurrent` | Lock-free queue, concurrent map | Java `ConcurrentHashMap`, Rust `dashmap` |
| `std.collections.small` | Small-buffer-optimized vec/string | Rust `smallvec`, C++ SSO |

**Meld difference:** `FlatMap`/`FlatSet` store keys and values in contiguous arrays for cache-optimal iteration. Iterating over a `Hold[FlatMap[K, V]]` provides zero-cost contiguous access without iterator invalidation risk. This is the default recommendation for hot-path lookups (500k+ TPS scenarios).

### 1.3 `std.iter` — Iterators & Lazy View Pipelines
- `Iterator` trait with `map`, `filter`, `fold`, `zip`, `chain`, `enumerate`, `take`, `skip`, `flatten`, `collect`
- `DoubleEndedIterator`, `ExactSizeIterator`
- **Lazy evaluation by default** — chaining `users |> filter |> map |> take(5)` allocates zero intermediate heap memory
- `std.iter.views` — `View[T]` inherently supports lazy evaluation pipelines (C++20 Ranges model)
- Parallel iterator support (like Rust `rayon`)
- Inspired by: Rust `std::iter`, Java Streams, C++20 Ranges, Python `itertools`

**Meld difference:** The `View[T]` type is the native lazy pipeline handle. The compiler proves that no intermediate allocation occurs and that the view cannot outlive its source data.

### 1.4 `std.string` — String & Text Processing
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `std.string` | `String` (owned UTF-8), `str` (borrowed) | Rust |
| `std.string.fmt` | Format strings, `Display`, `Debug` traits | Rust `std::fmt`, C++20 `std::format`, Python f-strings |
| `std.string.regex` | Regular expressions (RE2-class) | Go `regexp`, Rust `regex` crate |
| `std.string.unicode` | Unicode normalization, categories, case folding | Python `unicodedata`, Go `unicode` |
| `std.string.encoding` | UTF-8/16/32, ASCII, Latin-1 conversion | Go `encoding`, Python `codecs` |
| `std.string.template` | String interpolation templates | Python `string.Template`, Go `text/template` |

### 1.5 `std.math` — Numerics & Mathematics
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `std.math` | Trig, log, exp, sqrt, floor/ceil, constants (π, e, τ) | C++ `<cmath>`, Rust, Go `math` |
| `std.math.big` | Arbitrary-precision `BigInt`, `BigDecimal` | Java `BigInteger`/`BigDecimal`, Go `math/big` |
| `std.math.complex` | Complex number arithmetic | C++ `<complex>`, Python `cmath` |
| `std.math.random` | CSPRNG by default, distributions, seedable | Rust `rand`, Python `random`+`secrets` |
| `std.math.stats` | Mean, median, stdev, variance, percentiles | Python `statistics` |
| `std.math.bits` | Bit manipulation, popcount, leading/trailing zeros | Go `math/bits`, C++20 `<bit>` |
| `std.math.simd` | Portable SIMD vector types | Rust `std::simd`, C++ intrinsics |

### 1.6 `std.mem` — Memory Management
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `std.mem` | `size_of`, `align_of`, `swap`, `replace`, `zeroed` | Rust `std::mem` |
| `std.mem.alloc` | Allocator trait, global/arena/pool allocators | Rust `std::alloc`, C++ `<memory_resource>` |
| `std.mem.rc` | Reference-counted `Rc<T>`, `Arc<T>` | Rust `std::rc`, `std::sync::Arc` |
| `std.mem.pin` | Pinned memory for self-referential types | Rust `std::pin` |
| `std.mem.cell` | Interior mutability (`Cell`, `RefCell`) | Rust `std::cell` |
| `std.mem.pool` | Object pool, slab allocator | Java pool patterns, C++ pmr |

### 1.7 `std.io` — Input/Output
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `std.io` | `Read`, `Write`, `Seek`, `BufRead` traits | Rust `std::io`, Go `io` |
| `std.io.buf` | `BufReader`, `BufWriter`, `Cursor` | Rust, Go `bufio` |
| `std.io.stdio` | `stdin`, `stdout`, `stderr` | All languages |
| `std.io.pipe` | In-memory pipe (reader/writer pair) | Go `io.Pipe` |

### 1.8 `std.fs` — Filesystem (`@requires(fs_read)` / `@requires(fs_write)`)
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `std.fs` | `File`, `read`, `write`, `create`, `remove`, `rename`, `copy` | Rust `std::fs`, Go `os` |
| `std.fs.path` | `Path` object with operator overloading (`path / "config.json"`) | Python `pathlib`, Rust `std::path` |
| `std.fs.dir` | `read_dir`, `create_dir`, `walk` (recursive) | Rust, Go `filepath.Walk`, Python `os.walk` |
| `std.fs.metadata` | Permissions, timestamps, file type, size | All languages |
| `std.fs.temp` | Temporary files and directories | Python `tempfile`, Rust `tempfile` crate |
| `std.fs.watch` | Filesystem event notifications | Rust `notify` crate, Go `fsnotify` |

**Meld difference:** Any read/write operation requires `@requires(fs_read)` or `@requires(fs_write)` in the function signature. Paths are objects (Python `pathlib` style) — not raw strings — preventing cross-platform slash bugs.

### 1.9 `std.net` — Networking (`@requires(net)` / `@requires(net_listen)`)
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `std.net` | `TcpStream`, `TcpListener`, `UdpSocket` | Rust `std::net`, Go `net` |
| `std.net.ip` | `IpAddr`, `Ipv4Addr`, `Ipv6Addr`, `SocketAddr` | Rust, Go `netip` |
| `std.net.dns` | DNS resolution | Go `net.Resolver` |
| `std.net.unix` | Unix domain sockets | Go, Rust |

**Meld difference:** Using these modules triggers `@requires(net)` (outbound) or `@requires(net_listen)` (server bind). The resulting binary is explicitly sandboxed by Zerobox/Firecracker based on declared effects in `meld.toml`.

### 1.10 `std.sync` — Concurrency & Synchronization
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `std.sync` | `Mutex`, `RwLock`, `Condvar`, `Barrier` | Rust `std::sync`, Go `sync` |
| `std.sync.atomic` | Atomic types, memory orderings | Rust `std::sync::atomic`, C++ `<atomic>` |
| `std.sync.channel` | MPSC + MPMC channels | Rust `std::sync::mpsc`, Go channels |
| `std.sync.once` | `Once`, `OnceLock`, `LazyLock` | Rust, Go `sync.Once` |
| `std.sync.semaphore` | Counting + binary semaphore | C++20 `<semaphore>`, Java |
| `std.sync.pool` | Object pool (like `sync.Pool`) | Go `sync.Pool` |
| `std.sync.rcu` | Read-Copy-Update + Hazard Pointers (lock-free reads) | C++26 RCU, Linux kernel |
| `std.sync.Task` | Virtual threads — user-space tasks (nanosecond spawn) | Java 26 Virtual Threads |
| `std.sync.Scope` | Structured concurrency — child tasks bound to parent lifetime | Java 26 Structured Concurrency |

**Meld difference — RCU:** Multiple reader threads access data with zero locking overhead. The compiler enforces that a `View[T]` obtained from an RCU domain cannot outlive the read-side critical section, mathematically guaranteeing thread-safe lock-free reads.

**Meld difference — Virtual Threads:** Powered by `kernel.suspend` (which uses `boost::context::callcc` internally in the C++ kernel), tasks spawn in nanoseconds. When a `Task` hits an `@requires(Net)` boundary, the scheduler automatically yields, parking the task and freeing the OS thread.

**Meld difference — Structured Concurrency:** A `Scope` holds execution of child tasks. If the scope exits or one task panics, all sibling I/O and computations are systematically cancelled. No orphan threads, no leaked sockets.

### 1.11 `std.thread` — OS Threading
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `std.thread` | `spawn`, `JoinHandle`, `sleep`, `park`, scoped threads | Rust `std::thread` |
| `std.thread.local` | Thread-local storage | Rust `thread_local!`, Java `ThreadLocal` |

Note: For most use cases, prefer `std.sync.Task` (virtual threads) over raw OS threads.

### 1.12 `std.async` — Async Runtime (pure Meld)
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `std.async.fiber` | Green threads via `kernel.suspend` | Go goroutines, Java virtual threads |
| `std.async.scheduler` | FIFO cooperative scheduler | Tokio, Go runtime |
| `std.async.channel` | MPSC channels with park/wake | Go channels, Rust `std::sync::mpsc` |
| `std.async.task` | `Task[T]`, `await`, `map`, `race`, `all` | Rust Tokio, Java `CompletableFuture` |
| `std.async.scope` | Structured concurrency (`coroutine-scope`, `launch`) | Java 26 Structured Concurrency, Kotlin |
| `std.async.actor` | OS-thread actors with message passing | Erlang/OTP, Akka |
| `std.async.isolate` | OS-process isolation with IPC | Dart Isolates, Erlang nodes |

**Meld difference:** The entire async runtime is written in pure Meld, built on `kernel.suspend`/`kernel.resume`. No C++ runtime code (unlike Tokio which is Rust, our runtime is Meld). This means the scheduler is testable, mockable, and subject to the effect system. In Agent-Test mode, a deterministic scheduler replaces the default for reproducible concurrent tests.

### 1.13 `std.time` — Date & Time (Java `java.time` gold standard)
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `std.time` | `Instant` (machine time), `Duration` | Rust `std::time`, Java `java.time.Instant` |
| `std.time.calendar` | `Date`, `Time`, `DateTime`, `ZonedDateTime` (human time) | Java `java.time`, Python `datetime` |
| `std.time.zone` | IANA timezone database, `TimeZone` | Java `java.time.zone`, Python `zoneinfo` |
| `std.time.format` | Parsing and formatting (ISO 8601, RFC 3339, custom) | Go `time.Format`, Java `DateTimeFormatter` |

**Meld difference:** Strict separation between machine time (`Instant`) and human time (`ZonedDateTime`). You cannot accidentally mix them — the type system prevents it. This provides absolute clarity in high-precision distributed architectures.

### 1.14 `std.error` — Error Handling
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `std.error` | `Error` trait, `chain`, `source`, `backtrace` | Rust `std::error`, Go `errors` |
| `std.error.context` | Error context wrapping (like `anyhow`) | Rust `anyhow`/`eyre` |
| `std.error.panic` | Panic handling, `catch_unwind` | Rust `std::panic` |

### 1.15 `std.convert` — Type Conversion
- `From`, `Into`, `TryFrom`, `TryInto`, `AsRef`, `AsMut`
- Inspired by: Rust `std::convert`

### 1.16 `std.cmp` — Comparison & Ordering
- `PartialEq`, `Eq`, `PartialOrd`, `Ord`, `min`, `max`
- Three-way comparison (spaceship)
- Inspired by: Rust `std::cmp`, C++20 `<compare>`

### 1.17 `std.hash` — Hashing
- `Hash` trait, `Hasher`, `BuildHasher`
- Default: SipHash-1-3 (DoS-resistant)
- Inspired by: Rust `std::hash`, Go `hash`

### 1.18 `std.ops` — Operator Overloading
- Arithmetic, bitwise, index, deref, range, function call traits
- Inspired by: Rust `std::ops`

### 1.19 `std.env` — Environment & Process
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `std.env` | Environment variables, args, current_dir | Rust `std::env`, Go `os` |
| `std.process` | `Command`, `Child`, `exit`, `Stdio` | Rust `std::process`, Go `os/exec`, Python `subprocess` |

### 1.20 `std.context` — Context, Cancellation & Scoped Values
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `std.context` | `CancelToken`, deadline propagation, request-scoped context | Go `context` |
| `std.context.Scoped` | Immutable scoped values (Trace IDs, Tenant IDs) broadcast to deep call stacks | Java 26 Scoped Values |

**Meld difference — CancelToken:** Passed to long-running `@requires(net)` and `@requires(io)` calls. If the MicroVM nears its timeout or a structured concurrency scope fails, the token gracefully terminates internal state machines. No manual plumbing through 20 function layers.

**Meld difference — Scoped Values:** Strictly immutable and block-scoped. Maps to `View` semantics — the compiler guarantees they are never mutated and cannot outlive their execution block. Replaces error-prone `ThreadLocal` patterns.

### 1.21 `std.fmt` — Formatting & Display
- `Display`, `Debug`, `format!` macro, `print!`, `println!`, `eprint!`
- Inspired by: Rust `std::fmt`, C++20 `std::format`

---

## Tier 2 — Extended `std` (official, opt-in, maintained by Meld team)

These ship with the toolchain but are imported explicitly. They may have vendored dependencies.

### 2.1 `std.crypto` — Cryptography
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `std.crypto.hash` | SHA-2, SHA-3, BLAKE3, MD5 (legacy) | Go `crypto/sha*`, Rust `ring`/RustCrypto |
| `std.crypto.mac` | HMAC, KMAC, Poly1305 | Go `crypto/hmac`, Python `hmac` |
| `std.crypto.cipher` | AES-GCM, ChaCha20-Poly1305, XChaCha20 | Go `crypto/cipher`, Rust `ring` |
| `std.crypto.sign` | Ed25519, ECDSA (P-256/P-384), RSA | Go `crypto/ed25519`, Rust `ed25519-dalek` |
| `std.crypto.kex` | X25519, ECDH, ML-KEM (post-quantum) | Go `crypto/ecdh`, `crypto/mlkem` |
| `std.crypto.kdf` | HKDF, PBKDF2, Argon2, scrypt | Go `crypto/hkdf`, `x/crypto` |
| `std.crypto.rand` | CSPRNG (OS-backed) | Go `crypto/rand`, Rust `getrandom` |
| `std.crypto.cert` | X.509 parsing, verification, chain building | Go `crypto/x509`, Java `java.security.cert` |
| `std.crypto.pem` | PEM encoding/decoding | Go `encoding/pem` |
| `std.crypto.constant_time` | Constant-time comparison, select | Go `crypto/subtle` |

**Meld difference:** No legacy algorithms enabled by default. MD5/SHA-1 require explicit `#[allow(legacy_crypto)]`. Post-quantum algorithms (ML-KEM, ML-DSA) are first-class. **Zero-trust memory model:** Cryptographic keys are strictly typed as `Hold[SecretKey]`. When the exclusive owner goes out of scope, the deterministic destructor executes an immediate `explicit_bzero`, scrubbing the key from physical RAM. Memory zeroing is mathematically guaranteed by the Hold/View tenancy system.

### 2.2 `std.tls` — TLS/SSL
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `std.tls` | TLS 1.3 client/server, certificate management | Go `crypto/tls`, Rust `rustls` |
| `std.tls.config` | Cipher suite selection, ALPN, SNI, client auth | Go, Java `javax.net.ssl` |

**Meld difference:** TLS 1.3 only by default. TLS 1.2 requires explicit opt-in. Pure-Meld implementation (no OpenSSL dependency).

### 2.3 `std.http` — HTTP Client & Server (`@requires(net)` / `@requires(net_listen)`)
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `std.http` | HTTP/1.1, HTTP/2, HTTP/3 (QUIC) | Go `net/http`, Java `java.net.http` |
| `std.http.client` | `Client`, request builder, connection pooling | Rust `reqwest`, Go `http.Client` |
| `std.http.server` | `Server`, router, middleware, handler trait | Go `net/http`, Rust `axum` |
| `std.http.header` | Typed headers, header map | Go, Rust `hyper` |
| `std.http.cookie` | Cookie jar, parsing, SameSite | Go `net/http/cookiejar` |
| `std.http.websocket` | WebSocket client/server | Java `java.net.http.WebSocket` |

**Meld difference:** Using `std.http` triggers the compiler to demand `@requires(net)` (client) or `@requires(net_listen)` (server) tags. The resulting binary is explicitly sandboxed — Zerobox (Tier 2) or Firecracker (Tier 3) enforces the declared capability surface. Production-grade HTTP server ships natively (Go's killer feature, now in Meld).

### 2.4 `std.encoding` — Serialization & Data Formats
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `std.json` | Top-level JSON module (alias for `std.encoding.json`) | Go `encoding/json` convenience |
| `std.encoding.json` | JSON parse/serialize with derive macros | Rust `serde_json`, Go `encoding/json` |
| `std.encoding.toon` | TOON: Token-Optimized Object Notation (AI-native, 30-50% fewer tokens than JSON) | Meld-original |
| `std.encoding.toml` | TOML | Python `tomllib`, Rust `toml` |
| `std.encoding.yaml` | YAML | Rust `serde_yaml` |
| `std.encoding.csv` | CSV reader/writer | Python `csv`, Go `encoding/csv` |
| `std.encoding.xml` | XML (SAX + DOM + streaming) | Go `encoding/xml`, Java `javax.xml` |
| `std.encoding.binary` | Binary pack/unpack, endianness | Go `encoding/binary`, Python `struct` |
| `std.encoding.base64` | Base64, Base32, Hex | Go `encoding/base64`, Python `base64` |
| `std.encoding.protobuf` | Protocol Buffers (code-gen + runtime) | gRPC ecosystem |
| `std.encoding.msgpack` | MessagePack | Rust `rmp-serde` |

**Meld difference:** Unified `Serialize`/`Deserialize` trait system (like Rust's `serde`) built into the language with derive macros. TOON format is AI-native — optimized for minimal token count in LLM context windows.

### 2.4a `std.serde` — Unified Serialization Framework
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `std.serde` | `Serialize`/`Deserialize` traits, format-agnostic | Rust `serde` |
| `std.serde.derive` | `#[derive(Serialize, Deserialize)]` macro | Rust `serde_derive` |

**Meld difference:** Tenancy-aware serialization. Serializes a `View[T]` without cloning data; deserializes directly into a `Hold[T]`. Provides the fastest possible serialization for Host-to-MicroVM communication over VSOCK.

### 2.5 `std.compress` — Compression
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `std.compress.gzip` | Gzip compress/decompress | Go `compress/gzip`, Python `gzip` |
| `std.compress.zlib` | Zlib/DEFLATE | Go `compress/zlib` |
| `std.compress.zstd` | Zstandard (modern default) | Rust `zstd` crate |
| `std.compress.brotli` | Brotli (web-optimized) | — |
| `std.compress.lz4` | LZ4 (speed-optimized) | — |
| `std.compress.bzip2` | Bzip2 | Go `compress/bzip2`, Python `bz2` |

### 2.6 `std.archive` — Archive Formats
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `std.archive.tar` | Tar read/write | Go `archive/tar`, Python `tarfile` |
| `std.archive.zip` | ZIP read/write | Go `archive/zip`, Python `zipfile` |

### 2.7 `std.db` — Database Access
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `std.db` | Generic DB interface (`Connection`, `Statement`, `Row`) | Go `database/sql`, Java JDBC, Python DB-API 2.0 |
| `std.db.sql` | SQL query builder, parameterized queries | Rust `sqlx` |
| `std.db.sqlite` | Embedded SQLite | Python `sqlite3`, Rust `rusqlite` |
| `std.db.pool` | Connection pooling | Java HikariCP, Go `sql.DB` |
| `std.db.migrate` | Schema migration engine | Java Flyway, Rust `sqlx-migrate` |

**Meld difference:** Compile-time SQL validation (like Rust `sqlx`). All queries parameterized by default — string concatenation into queries is a compile error.

### 2.8 `std.log` — Logging & Diagnostics
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `std.log` | Structured logging (levels, fields, context) | Go `log/slog`, Rust `tracing` |
| `std.log.trace` | Distributed tracing spans, context propagation | Rust `tracing`, OpenTelemetry |
| `std.log.metrics` | Counters, gauges, histograms | Go Prometheus client, Java Micrometer |

### 2.9 `std.test` — Testing Framework
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `std.test` | `#[test]`, assertions, test runner | Rust built-in, Go `testing` |
| `std.test.bench` | Benchmarking, `#[bench]` | Rust `criterion`, Go `testing.B` |
| `std.test.fuzz` | Fuzz testing | Go `testing.F`, Rust `cargo-fuzz` |
| `std.test.mock` | Mocking framework (trait-based) | Rust `mockall`, Java Mockito |
| `std.test.prop` | Property-based testing | Rust `proptest`, Python `hypothesis` |
| `std.test.snapshot` | Snapshot/golden-file testing | Rust `insta` |

### 2.10 `std.cli` — Command-Line Interface
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `std.cli` | Argument parsing with derive macros | Rust `clap`, Go `flag`+Cobra |
| `std.cli.prompt` | Interactive prompts, password input | Rust `dialoguer`, Python `getpass` |
| `std.cli.progress` | Progress bars, spinners | Rust `indicatif` |
| `std.cli.color` | Terminal colors and styling | Rust `console`, Go `fatih/color` |

### 2.11 `std.html` — HTML Processing
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `std.html` | HTML parsing (safe, no XSS) | Go `html`, Python `html.parser` |
| `std.html.template` | Auto-escaping HTML templates | Go `html/template` |

### 2.12 `std.uuid` — Identifiers
- UUID v4, v7 (time-sortable), ULID generation
- Inspired by: Java `java.util.UUID`, Python `uuid`, Go `google/uuid`

### 2.13 `std.url` — URL Handling
- URL parsing, encoding, query string manipulation
- Inspired by: Go `net/url`, Rust `url` crate, Python `urllib.parse`

### 2.14 `std.image` — Image Processing
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `std.image` | Image types, pixel formats, basic ops | Go `image`, Python PIL |
| `std.image.png` | PNG encode/decode | Go `image/png` |
| `std.image.jpeg` | JPEG encode/decode | Go `image/jpeg` |
| `std.image.webp` | WebP encode/decode | — |

### 2.15 `std.reflect` — Runtime Reflection
- Type introspection, field enumeration, method dispatch
- Inspired by: Go `reflect`, Java `java.lang.reflect`
- **Meld difference:** Opt-in per type with `#[reflect]`. No reflection by default (security + binary size).

### 2.16 `std.ffi` — Foreign Function Interface
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `std.ffi` | C ABI interop, `CStr`, `CString` | Rust `std::ffi` |
| `std.ffi.wasm` | WASM component model interop | Rust `wasm-bindgen` |
| `std.ffi.python` | Python interop (embed/extend) | Rust `pyo3` |

### 2.17 `std.cache` — High-Throughput Caching
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `std.cache` | Lock-free in-memory cache, TinyLFU admission | Java Caffeine, Go `sync.Map` |
| `std.cache.lru` | LRU eviction policy | — |
| `std.cache.lfu` | LFU eviction policy | — |

**Meld difference:** Built on RCU (Read-Copy-Update) and Hazard Pointers for zero-contention reads at 500k+ TPS. Cached items are stored as `Hold[T]` and queried as `View[T]`, eliminating thread contention and lock bottlenecks entirely. Designed for massive concurrency within the Actor model.

### 2.18 `meld-flatbuffers` — Zero-Copy Serialization
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `meld-flatbuffers` | Zero-copy buffer access, schema compiler | Google FlatBuffers |
| `meld-flatbuffers.schema` | Schema definition and code generation | `.fbs` schema language |

**Meld difference:** Maps directly to Meld's memory model. Reading a buffer from the network yields a `Hold[Buffer]`. The library provides a `View[Message]` allowing direct memory access via pointer offsets — zero-copy, zero-allocation data access. Mathematically safe access guaranteed by the Hold/View tenancy system. Critical for maintaining microsecond latency in microservice architectures.

---

## Tier 3 — Framework Layer (official, separate packages)

These are maintained by the Meld team but versioned independently. They represent opinionated, higher-level abstractions.

### 3.1 `meld.web` — Web Framework
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `meld.web` | Router, middleware stack, request/response | Rust `axum`, Go `net/http`, Java Spring MVC |
| `meld.web.rest` | REST resource macros, OpenAPI generation | Java JAX-RS, Spring `@RestController` |
| `meld.web.graphql` | GraphQL server | Java Spring GraphQL |
| `meld.web.grpc` | gRPC server/client | Go `grpc-go`, Rust `tonic` |
| `meld.web.sse` | Server-Sent Events | — |
| `meld.web.static` | Static file serving, embedded assets | Go `embed`, Rust `include_bytes!` |

### 3.2 `meld.di` — Dependency Injection
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `meld.di` | Compile-time DI container, `#[inject]` | Java Spring IoC, Go `wire` |
| `meld.di.scope` | Request/singleton/prototype scopes | Spring scopes |
| `meld.di.config` | Configuration binding, profiles, env overlay | Spring Boot `@ConfigurationProperties` |

**Meld difference:** DI is resolved at compile time (like Go Wire), not runtime. Zero overhead.

### 3.3 `meld.orm` — Object-Relational Mapping
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `meld.orm` | Entity mapping, `#[entity]`, relations | Java JPA/Hibernate, Rust `sea-orm` |
| `meld.orm.query` | Type-safe query builder, JPQL-like DSL | Java Criteria API, Rust `diesel` |
| `meld.orm.migrate` | Auto-migration from entity diffs | Java Flyway, Django migrations |
| `meld.orm.pool` | Connection pool management | Java HikariCP |

### 3.4 `meld.auth` — Authentication & Authorization
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `meld.auth` | Auth middleware, session management | Spring Security |
| `meld.auth.oauth2` | OAuth2 client/server, OIDC | Spring Security OAuth2, Go `x/oauth2` |
| `meld.auth.jwt` | JWT creation, validation, key rotation | Java JJWT |
| `meld.auth.rbac` | Role-based access control | Spring Security roles |
| `meld.auth.passkey` | WebAuthn/FIDO2/Passkey support | — |

### 3.5 `meld.msg` — Messaging & Events
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `meld.msg` | Message broker abstraction | Java Spring Integration, JMS |
| `meld.msg.queue` | Work queue patterns (pub/sub, fan-out) | Java Spring AMQP |
| `meld.msg.event` | Event sourcing primitives | — |
| `meld.msg.stream` | Stream processing (windowing, aggregation) | Java Spring Cloud Stream, Kafka Streams |

### 3.6 `meld.observe` — Observability
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `meld.observe.trace` | Distributed tracing (OpenTelemetry-compatible) | Java Micrometer Tracing, Go OTel |
| `meld.observe.metrics` | Metrics export (Prometheus, OTLP) | Java Micrometer, Go Prometheus |
| `meld.observe.health` | Health checks, readiness/liveness probes | Spring Boot Actuator |
| `meld.observe.profile` | Runtime profiling, flame graphs | Go `pprof`, Java JFR |

### 3.6a `meld-otel` — Distributed Observability (OpenTelemetry)
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `meld-otel` | Full OpenTelemetry SDK (traces, metrics, logs) | Go OTel SDK, Java OTel SDK |
| `meld-otel.propagation` | Context propagation (W3C TraceContext, B3) | OTel propagators |
| `meld-otel.export` | OTLP, Jaeger, Zipkin exporters | OTel exporters |

**Meld difference:** Integrates natively with `std.context.Scoped`. Trace IDs are immutably bound to a structured execution scope and automatically injected into outbound `@requires(Net)` requests, ensuring complete distributed visibility with zero boilerplate. The effect system guarantees no trace context is ever lost across async boundaries.

### 3.7 `meld.validate` — Validation
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `meld.validate` | `#[validate]` derive, constraint annotations | Java Bean Validation (`@NotNull`, `@Size`) |
| `meld.validate.schema` | JSON Schema validation | — |

### 3.8 `meld.schedule` — Task Scheduling
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `meld.schedule` | Cron-like scheduling, periodic tasks | Java Spring `@Scheduled`, Go `cron` |
| `meld.schedule.retry` | Retry policies, exponential backoff, circuit breaker | Spring Retry, Resilience4j |

### 3.9 `meld.cache` — Caching
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `meld.cache` | In-memory LRU/LFU cache, `#[cached]` | Java Caffeine, Python `functools.lru_cache` |
| `meld.cache.distributed` | Redis/Memcached abstraction | Spring Cache, Java Redisson |

---

## Tier 4 — AI-Native & Meld Exclusives (no other language has these)

These modules are Meld's core differentiator — integrating AI, sandboxing, and agent interop directly into the standard library.

### 4.1 `std.sandbox` — Runtime Capability Introspection
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `std.sandbox` | Query authorized capabilities at runtime | Meld-original |
| `std.sandbox.has_effect` | `sandbox.has_effect("net")` → bool; graceful degradation | Meld-original |
| `std.sandbox.manifest` | Read `meld.toml` capability declarations | Meld-original |

**How it works:** Allows a Meld application to dynamically query its own authorized capabilities. A library can gracefully degrade to local caching if the developer denied network egress in `meld.toml`. This bridges compile-time effect checking with runtime flexibility.

### 4.2 `std.agent` — Agentic Tier 0 Interop
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `std.agent` | Safe RPC bindings to local LLM infrastructure | Meld-original |
| `std.agent.extract` | `agent.extract_json(raw_text)` — structured data from unstructured input | LangChain, OpenAI function calling |
| `std.agent.tool` | Tool-use protocol, function registration for agent invocation | OpenAI tools API |
| `std.agent.memory` | Conversation/context memory for agent loops | LangChain memory |

**How it works:** Instead of writing brittle regex to parse poorly formatted data, developers invoke `agent.extract_json(raw_text)` which offloads unstructured data parsing to the transient Tier 0 Wasm sandbox. The LLM call is sandboxed — it cannot access the host filesystem or network beyond its declared effects.

### 4.3 `std.tensor` — Tensor Computation
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `std.tensor` | N-dimensional array, dtype system, broadcasting | NumPy, PyTorch, Rust `ndarray` |
| `std.tensor.ops` | Matmul, conv, attention, element-wise ops | PyTorch, JAX |
| `std.tensor.device` | CPU/GPU/NPU device abstraction, data transfer | PyTorch `.to(device)` |
| `std.tensor.autograd` | Automatic differentiation | PyTorch autograd, Rust `std::autodiff` |
| `std.tensor.quantize` | INT8/INT4 quantization, mixed precision | PyTorch quantization |

### 4.4 `std.ai` — AI/ML Primitives
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `std.ai.tokenizer` | BPE, SentencePiece, tiktoken-compatible | HuggingFace tokenizers |
| `std.ai.embedding` | Embedding tables, similarity search | — |
| `std.ai.inference` | Model loading (ONNX, SafeTensors), inference engine | ONNX Runtime, llama.cpp |
| `std.ai.prompt` | Prompt templating, structured output parsing | LangChain, Guidance |
| `std.ai.safety` | Output filtering, guardrails, content classification | — |

### 4.5 `std.ai.serve` — Model Serving
| Module | Contents | Inspired By |
|--------|----------|-------------|
| `std.ai.serve` | Model server, batching, KV-cache management | vLLM, TGI |
| `std.ai.serve.stream` | Streaming token generation | OpenAI streaming API |

---

## Summary: Module Count by Tier

| Tier | Modules | Philosophy |
|------|---------|-----------|
| **Tier 1 — Core** | ~21 top-level, ~65 sub-modules | Always available, zero-cost, safe, `Hold/View` enforced |
| **Tier 2 — Extended** | ~16 top-level, ~50 sub-modules | Opt-in, official, `@effect`-gated where applicable |
| **Tier 3 — Framework** | ~9 top-level, ~35 sub-modules | Opinionated, versioned independently |
| **Tier 4 — AI/Exclusive** | ~5 top-level, ~20 sub-modules | Meld's differentiator — no other language has these |

---

## Key Differentiators vs Other Languages

| Feature | C++ | Rust | Python | Java | Go | **Meld** |
|---------|-----|------|--------|------|-----|----------|
| Async in std | ❌ (coroutines only) | ❌ (trait only) | ✅ | ✅ (virtual threads) | ✅ (goroutines) | **✅ full runtime + virtual threads** |
| Structured concurrency | ❌ | ❌ | ❌ | ✅ (Java 26) | ❌ | **✅ lifetime-bound scopes** |
| RCU / lock-free reads | ❌ (C++26 proposal) | ❌ | ❌ | ❌ | ❌ | **✅ compiler-verified** |
| Scoped values | ❌ | ❌ | ❌ | ✅ (Java 26) | ❌ | **✅ View-semantics enforced** |
| Effect system / capabilities | ❌ | ❌ | ❌ | ❌ | ❌ | **✅ @effect firewall** |
| Crypto in std | ❌ | ❌ | partial | ✅ | ✅ | **✅ post-quantum** |
| HTTP in std | ❌ | ❌ | partial | ✅ | ✅ | **✅ HTTP/3 + sandbox-aware** |
| DB in std | ❌ | ❌ | SQLite only | ✅ (JDBC) | ✅ (interface) | **✅ + compile-time SQL** |
| Testing in std | ❌ | ✅ basic | ✅ | ❌ (JUnit separate) | ✅ | **✅ fuzz+prop+snapshot** |
| AI/Tensor in std | ❌ | ❌ | ❌ | ❌ | ❌ | **✅ first-class** |
| Agent interop in std | ❌ | ❌ | ❌ | ❌ | ❌ | **✅ std.agent** |
| Sandbox introspection | ❌ | ❌ | ❌ | ❌ | ❌ | **✅ std.sandbox** |
| DI framework | ❌ | ❌ | ❌ | Spring (separate) | Wire (separate) | **✅ compile-time** |
| Structured logging | ❌ | ❌ (tracing crate) | ✅ | ❌ (SLF4J separate) | ✅ (slog) | **✅ + tracing** |
| Cache-friendly flat containers | ✅ (C++23) | ❌ | ❌ | ❌ | ❌ | **✅ FlatMap/FlatSet** |

---

## What We Deliberately Exclude from std

| Excluded | Reason |
|----------|--------|
| GUI/Desktop toolkit | Too platform-specific, evolves too fast |
| Email (SMTP/IMAP) | Niche, better as community package |
| FTP, Telnet, legacy protocols | Security liability |
| Audio/Video codecs | Large, patent-encumbered |
| PDF generation | Complex, better as community package |
| Blockchain/Web3 | Too opinionated, rapidly changing |
| Game engine primitives | Separate ecosystem |
